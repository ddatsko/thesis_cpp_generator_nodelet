#ifndef MAP_TO_GRAPH_MAPPOLYGON_HPP
#define MAP_TO_GRAPH_MAPPOLYGON_HPP

#include <vector>
#include <string>
#include <array>
#include <stdexcept>
#include <numeric>
#include <set>
#include <map>
#include "custom_types.hpp"



struct kml_file_parse_error: public std::runtime_error {
  using runtime_error::runtime_error;
};

struct non_existing_point_error: public std::runtime_error {
    using runtime_error::runtime_error;
};

struct wrong_polygon_format_error: public std::runtime_error {
    using runtime_error::runtime_error;
};


/*!
 * Structure for representation of a polygon with holes
 */
struct MapPolygon {
    using polygon_t = std::vector<point_t>;

    polygon_t fly_zone_polygon_points;
    std::vector<polygon_t> no_fly_zone_polygons;

    MapPolygon() = default;
    MapPolygon(const MapPolygon &p);

    MapPolygon& operator=(const MapPolygon &rhs) = default;

    /*!
     * Parse a KML file and read points from it.
     * Only points from polygons called with names "fly-zone" and "no-fly-zone" will be taken into account
     * @param filename Path to the KML file
     */
    void load_polygon_from_file(const std::string &filename);

    // TODO: check if get_all_points method should be a method or a separate function
    /*!
     * Get the list of all the polygons point (both fly-zone and non-fly zone)
     * @return Set of all the points
     */
    [[nodiscard]] std::set<point_t> get_all_points() const;

    /*!
     * Get the vector of all segments (of both fly and no-fly zones of the map polygon)
     * @return Vector of all the present segments
     */
    [[nodiscard]] std::vector<segment_t> get_all_segments() const;

    /*!
     * Get two neighbouring points in some polygon for the specified point
     * @param point point that belongs to one of polygons
     * @return pair of two points - neighbors of the points
     */
    [[nodiscard]] std::pair<point_t, point_t> point_neighbors(point_t point) const;

    /*!
     * Get the map polygon, rotated by angle radians counter clockwise around the coordinates origin
     * @param angle Angle of the rotation [rad]
     * @return New MapPolygon with all the points rotated by the angle
     */
    [[nodiscard]] MapPolygon rotated(double angle) const;

    /*!
     * Find the edge starting from the first rightmost point
     * @return Edge starting from the first rightmost point
     */
    [[nodiscard]] std::pair<point_t, point_t> rightmost_edge() const;

    /*!
     * Get n (or less if n >= number_of_edges) angles of rotation of n longest edges
     * @param n Number of longest edges to encounter
     * @return vector of min(n, number_of_edges) rotation angles
     */
    [[nodiscard]] std::vector<double> get_n_longest_edges_rotation_angles(size_t n) const;

    /*!
     * Split fly-zone by a vertical line. If there are non-fly zones, they will be just deleted
     * @param x X coordinate of the vertical line
     * @return Two polygons, left one is first, right one is second
     */
    std::pair<MapPolygon, MapPolygon> split_by_vertical_line(double x);

    /*!
     * Split the convex polygon with no no-fly zones into the smallest number of
     * equal area pieces
     * @warning Works properly only for convex polygons with no no-fly zones
     * @param max_piece_area Max area of one piece
     * @return vector of the result of decomposition
     */
    std::vector<MapPolygon> split_into_pieces(double max_piece_area);

    /*!
     * Make the fly-zone purely convex by replacing it with its convex hull
     */
    void make_pure_convex();

    /*!
     * @return area of the fly-zone
     */
    [[nodiscard]] double area() const;
};



#endif //MAP_TO_GRAPH_MAPPOLYGON_HPP
