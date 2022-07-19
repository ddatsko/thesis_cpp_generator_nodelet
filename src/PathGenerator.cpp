#include <PathGenerator.h>

/* every nodelet must include macros which export the class as a nodelet plugin */
#include <pluginlib/class_list_macros.h>
#include <mrs_msgs/PathSrv.h>
#include <std_msgs/String.h>
#include "MapPolygon.hpp"
#include "EnergyCalculator.h"
#include "algorithms.hpp"
#include <boost/shared_ptr.hpp>
#include <vector>
#include "utils.hpp"
#include "ShortestPathCalculator.hpp"
#include "mstsp_solver/SolverConfig.h"
#include "mstsp_solver/MstspSolver.h"

#include <thesis_path_generator/GeneratePaths.h>

namespace path_generation {

/* onInit() method //{ */
    void PathGenerator::onInit() {

        // | ---------------- set my booleans to false ---------------- |
        // but remember, always set them to their default value in the header file
        // because, when you add new one later, you might forget to come back here

        /* obtain node handle */
        ros::NodeHandle nh = nodelet::Nodelet::getMTPrivateNodeHandle();

        /* waits for the ROS to publish clock */
        ros::Time::waitForValid();

        // | ------------------- load ros parameters ------------------ |<<6
        /* (mrs_lib implementation checks whether the parameter was loaded or not) */
        mrs_lib::ParamLoader pl(nh, "PathGenerator");

        pl.loadParam("battery_model/cell_capacity", m_energy_config.battery_model.cell_capacity);
        pl.loadParam("battery_model/number_of_cells", m_energy_config.battery_model.number_of_cells);
        pl.loadParam("battery_model/d0", m_energy_config.battery_model.d0);
        pl.loadParam("battery_model/d1", m_energy_config.battery_model.d1);
        pl.loadParam("battery_model/d2", m_energy_config.battery_model.d2);
        pl.loadParam("battery_model/d3", m_energy_config.battery_model.d3);

        pl.loadParam("best_speed_model/c0", m_energy_config.best_speed_model.c0);
        pl.loadParam("best_speed_model/c1", m_energy_config.best_speed_model.c1);
        pl.loadParam("best_speed_model/c2", m_energy_config.best_speed_model.c2);
        pl.loadParam("drone_mass", m_energy_config.drone_mass);
        pl.loadParam("drone_area", m_energy_config.drone_area);
        pl.loadParam("average_acceleration", m_energy_config.average_acceleration);
        pl.loadParam("propeller_radius", m_energy_config.propeller_radius);
        pl.loadParam("number_of_propellers", m_energy_config.number_of_propellers);
        pl.loadParam("allowed_path_deviation", m_energy_config.allowed_path_deviation);

        if (!pl.loadedSuccessfully()) {
            ROS_ERROR("[PathGenerator]: failed to load non-optional parameters!");
            ros::shutdown();
            return;
        } else {
            ROS_INFO_ONCE("[PathGenerator]: loaded parameters");
        }

        m_generate_paths_service_server = nh.advertiseService("/generate_paths",
                                                              &PathGenerator::callback_generate_paths, this);

        ROS_INFO_ONCE("[PathGenerator]: initialized");

        m_is_initialized = true;
    }



    bool PathGenerator::callback_generate_paths(thesis_path_generator::GeneratePaths::Request &req,
                                                thesis_path_generator::GeneratePaths::Response &res) {

        if (!m_is_initialized) return false;
        res.success = false;
        if (req.fly_zone.points.size() <= 2) {
            res.message = "Fly zone of less than 3 points";
            return true;
        }
        m_drones_altitude = req.drones_altitude;
        m_unique_altitude_step = req.unique_altitude_step;

        point_t gps_transform_origin{req.fly_zone.points.front().x, req.fly_zone.points.front().y};
        // Convert the area from message to custom MapPolygon type
        MapPolygon polygon;
        std::for_each(req.fly_zone.points.begin(), req.fly_zone.points.end(), [&](const auto &p){polygon.fly_zone_polygon_points.push_back(gps_coordinates_to_meters({p.x, p.y}, gps_transform_origin));});
        make_polygon_clockwise(polygon.fly_zone_polygon_points);
        for (auto &no_fly_zone: req.no_fly_zones) {
            polygon.no_fly_zone_polygons.emplace_back();
            std::for_each(no_fly_zone.points.begin(), no_fly_zone.points.end(), [&](const auto &p) {polygon.no_fly_zone_polygons.back().push_back(gps_coordinates_to_meters({p.x, p.y}, gps_transform_origin));});
            make_polygon_clockwise(polygon.no_fly_zone_polygons[polygon.no_fly_zone_polygons.size() - 1]);
        }


        energy_calculator_config_t energy_config = m_energy_config;
        if (req.override_battery_model) {
            energy_config.battery_model.cell_capacity = req.battery_cell_capacity;
            energy_config.battery_model.number_of_cells = req.battery_number_of_cells;
        }

        if (req.override_drone_parameters) {
            ROS_INFO("[PathGenerator] Overriding UAV parameters");
            energy_config.drone_area = req.drone_area;
            energy_config.average_acceleration = req.average_acceleration;
            energy_config.drone_mass = req.drone_mass;
            energy_config.number_of_propellers = req.number_of_propellers;
            energy_config.propeller_radius = req.propeller_radius;
        }


        // Decompose the polygon
        ShortestPathCalculator shortest_path_calculator(polygon);

        polygon = polygon.rotated(req.decomposition_rotation);
        std::vector<MapPolygon> polygons_decomposed;
        try {
            polygons_decomposed = trapezoidal_decomposition(polygon, BOUSTROPHEDON_WITH_CONVEX_POLYGONS);
        } catch (const polygon_decomposition_error &e) {
            ROS_ERROR("[PathGenerator]: Error while decomposing the polygon");
            res.success = false;
            res.message = "Error while decomposing the polygon";
            return true;
        }

        ROS_INFO_STREAM("[PathGenerator]: Polygon decomposed. Decomposed polygons: ");
        for (const auto &p: polygons_decomposed) {
            ROS_INFO_STREAM("[PathGenerator] Polygon points: ");
            for (const auto &point: p.fly_zone_polygon_points) {
                ROS_INFO_STREAM("(" << point.first << ", " << point.second << "), ");
            }

            ROS_INFO_STREAM("\n[PathGenerator] Decomposed sub polygon area: " << p.area());
        }

        std::for_each(polygons_decomposed.begin(), polygons_decomposed.end(), [&](MapPolygon &p){p = p.rotated(-req.decomposition_rotation);});
        polygon = polygon.rotated(-req.decomposition_rotation);

        std::vector<MapPolygon> polygons_divided;
        for (auto &pol: polygons_decomposed) {

            auto splitted = pol.split_into_pieces(req.max_polygon_area != 0 ? req.max_polygon_area : std::numeric_limits<double>::max());
            polygons_divided.insert(polygons_divided.end(), splitted.begin(), splitted.end());
        }
        polygons_decomposed = polygons_divided;
        ROS_INFO_STREAM("[PathGenerator]: Divided large polygons into smaller ones");

        EnergyCalculator energy_calculator{energy_config};
        auto starting_point = gps_coordinates_to_meters({req.start_lat, req.start_lon}, gps_transform_origin);

        mstsp_solver::SolverConfig solver_config{req.rotations_per_cell, req.sweeping_step, starting_point, req.number_of_drones, m_drones_altitude, m_unique_altitude_step};
        mstsp_solver::MstspSolver solver(solver_config, polygons_decomposed, energy_calculator,
                                         shortest_path_calculator);

        ROS_INFO_STREAM("[PathGenerator]: Optimal speed: " << energy_calculator.get_optimal_speed());

        auto solver_res = solver.solve();


        // Modify the finishing coordinate to prevent drones from collision at the end
        for (size_t i = 0; i < solver_res.size(); ++i) {
            solver_res[i].back().x += static_cast<double>(i) * 3;
        }

        res.success = true;
        res.paths_gps.resize(solver_res.size());
        res.energy_consumptions.resize(solver_res.size());
        for (size_t i = 0; i < solver_res.size(); ++i) {
            solver_res[i].erase(solver_res[i].begin(), solver_res[i].begin() + 2);

            res.paths_gps[i].header.frame_id = "latlon_origin";
            res.energy_consumptions[i] = energy_calculator.calculate_path_energy_consumption(remove_path_heading(solver_res[i]));

            auto generated_path =  _generate_path_for_simulation_one_drone(solver_res[i], gps_transform_origin, req.distance_for_turning, req.max_number_of_extra_points, energy_calculator.get_optimal_speed(), energy_config.average_acceleration);

            res.paths_gps[i] = generated_path;
        }
        return true;
    }


    mrs_msgs::Path PathGenerator::_generate_path_for_simulation_one_drone(
            const std::vector<point_heading_t<double>> &points_to_visit,
            point_t gps_transform_origin,
            double distance_for_turning,
            int max_number_of_extra_points,
            double optimal_speed,
            double horizontal_acceleration) {
        mrs_msgs::Path path;

        // Set the parameters for trajectory generation
        path.header.stamp = ros::Time::now();
        path.header.seq = sequence_counter++;
        path.header.frame_id = "latlon_origin";

        path.fly_now = true;
        path.use_heading = true;
        path.stop_at_waypoints = false;
        path.loop = false;
        path.override_constraints = true;

        path.override_max_velocity_horizontal = optimal_speed;
        path.override_max_acceleration_horizontal = horizontal_acceleration;
        path.override_max_jerk_horizontal = 200;
        path.override_max_jerk_vertical = 200;
        path.override_max_acceleration_vertical = 1;
        path.override_max_velocity_vertical = optimal_speed;

        // TODO: find out what this parameter means
        path.relax_heading = false;

        std::vector<mrs_msgs::Reference> points;

        for (auto p: points_to_visit) {
            mrs_msgs::ReferenceStamped point_3d;
            point_3d.header.frame_id = "latlon_origin";

            auto gps_coordinates = meters_to_gps_coordinates({p.x, p.y}, gps_transform_origin);
            point_3d.reference.position.x = gps_coordinates.first;
            point_3d.reference.position.y = gps_coordinates.second;
            point_3d.reference.position.z = p.z;
            point_3d.reference.heading = -p.heading + M_PI_2;


            points.push_back(point_3d.reference);
        }
        path.points = points;
        return path;
    }

}  // namespace trajectory_generatiion

/* every nodelet must export its class as nodelet plugin */
PLUGINLIB_EXPORT_CLASS(path_generation::PathGenerator, nodelet::Nodelet)
