#include "utils.hpp"
#include <vector>
#include <cmath>
#include <iostream>


const double METERS_IN_DEGREE = 111319.5;

hom_t cross_product(const hom_t &a, const hom_t &b) {
    return {std::get<1>(a) * std::get<2>(b) - std::get<2>(a) * std::get<1>(b),
            std::get<2>(a) * std::get<0>(b) - std::get<0>(a) * std::get<2>(b),
            std::get<0>(a) * std::get<1>(b) - std::get<1>(a) * std::get<0>(b)};
}

std::pair<double, double> segment_line_intersection(const std::pair<double, double> &p1,
                                                    const std::pair<double, double> &p2,
                                                    const hom_t &line) {
    hom_t p1_h = {p1.first, p1.second, 1};
    hom_t p2_h = {p2.first, p2.second, 1};
    hom_t segment_line = cross_product(p1_h, p2_h);
    hom_t p_intersection_h = cross_product(segment_line, line);
    return {std::get<0>(p_intersection_h) / std::get<2>(p_intersection_h),
            std::get<1>(p_intersection_h) / std::get<2>(p_intersection_h)};
}

std::pair<double, double> rotate_point(std::pair<double, double> p, double angle) {
    return {p.first * std::cos(angle) - p.second * std::sin(angle),
            p.second * std::cos(angle) + p.first * std::sin(angle)};
}


std::pair<double, double> segment_segment_intersection(const segment_t &s1, const segment_t &s2) {
    hom_t p1 = {s2.first.first, s2.first.second, 1};
    hom_t p2 = {s2.second.first, s2.second.second, 1};
    hom_t line = cross_product(p1, p2);
    return segment_line_intersection(s1.first, s1.second, line);
}

double segment_length(const segment_t &segment) {
    return std::sqrt(std::pow(segment.second.first - segment.first.first, 2) +
                     std::pow(segment.second.second - segment.first.second, 2));
}

bool segments_intersect(const segment_t &s1, const segment_t &s2) {
    auto intersection = segment_segment_intersection(s1, s2);
    // Return true only of the intersection of lines is between the constraints of the first and the second segments
    return intersection.first >= std::min(s1.first.first, s1.second.first) &&
           intersection.first <= std::max(s1.first.first, s1.second.first) &&
           intersection.first >= std::min(s2.first.first, s2.second.first) &&
           intersection.first <= std::max(s2.first.first, s2.second.first) &&
           intersection.second >= std::min(s1.first.second, s1.second.second) &&
           intersection.second <= std::max(s1.first.second, s1.second.second) &&
           intersection.second >= std::min(s2.first.second, s2.second.second) &&
           intersection.second <= std::max(s2.first.second, s2.second.second);
}


/*!
 * Angle between vectors (p1, p2) and (p2, p3)
 * @param p1 First point of first vector
 * @param p2 Second point of first vector, first point of third vector
 * @param p3 Second point of the second vector
 * @return  Clockwise angle between two vectors in range (0, 2 * pi)
 */
double angle_between_vectors(point_t p1, point_t p2, point_t p3) {
    double x1 = p1.first - p2.first, y1 = p1.second - p2.second;
    double x2 = p3.first - p2.first, y2 = p3.second - p2.second;
    double dot = x1 * x2 + y1 * y2, det = x1 * y2 - y1 * x2;
    double angle = atan2(det, dot);
    if (angle < 0) {
        angle += 2 * M_PI;
    }
    return angle;
}

double distance_between_points(point_t p1, point_t p2) {
    return std::sqrt((p1.first - p2.first) * (p1.first - p2.first) +
                     (p1.second - p2.second) * (p1.second - p2.second));
}


// TODO: test this
point_t gps_coordinates_to_meters(point_t p) {
    point_t res;
    res.second = p.first * METERS_IN_DEGREE;
    res.first = std::cos(p.first / 180.0 * M_PI) * p.second * METERS_IN_DEGREE;
    return res;
}

point_t meters_to_gps_coordinates(point_t p) {
    point_t res;
    res.first = p.second / METERS_IN_DEGREE;
    res.second = p.first / (std::cos(res.first / 180.0 * M_PI) * METERS_IN_DEGREE);
    return res;
}