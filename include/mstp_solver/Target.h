//
// Created by mrs on 28.03.22.
//

#ifndef THESIS_TRAJECTORY_GENERATOR_TARGET_H
#define THESIS_TRAJECTORY_GENERATOR_TARGET_H

#include "utils.hpp"


struct Target {
    bool first_line_up;
    double rotation_angle;
    double energy_consumption;
    point_t starting_point;
    point_t end_point;
    size_t target_set_index;
};


#endif //THESIS_TRAJECTORY_GENERATOR_TARGET_H
