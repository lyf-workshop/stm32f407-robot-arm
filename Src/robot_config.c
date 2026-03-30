/**
  ******************************************************************************
  * @file    robot_config.c
  * @brief   Robot arm physical parameters implementation
  ******************************************************************************
  */

#include "robot_config.h"

/**
 * @brief DH parameters table [a, alpha, d, theta_offset]
 * From zero-robotic-arm project (verified with MATLAB)
 * Units: a and d in mm, alpha and theta_offset in radians
 */
const float D_H[6][4] = {
    {0,      0,       0,      M_PI/2},   // J1
    {0,      M_PI/2,  0,      M_PI/2},   // J2
    {200,    M_PI,    0,     -M_PI/2},   // J3: a2=200mm
    {47.63, -M_PI/2, -184.5,  0},        // J4: a3=47.63, d4=-184.5mm
    {0,      M_PI/2,  0,      M_PI/2},   // J5
    {0,      M_PI/2,  0,      0},        // J6
};

/**
 * @brief Gear reduction ratios
 * From zero-robotic-arm BOM (planetary gearboxes)
 */
const float GEAR_RATIO[6] = {50.0f, 50.89f, 50.89f, 51.0f, 26.85f, 51.0f};

/**
 * @brief Joint angle limits [min, max] in degrees
 */
const float JOINT_LIMITS[6][2] = {
    {0,   360},   // J1: base rotation
    {90,  180},   // J2: shoulder
    {-90, 90},    // J3: elbow
    {-90, 90},    // J4: wrist pitch
    {0,   90},    // J5: wrist roll
    {0,   360},   // J6: end effector rotation
};

/**
 * @brief Joint weight for optimal solution selection
 * Higher weight = prefer smaller angle change for this joint
 */
const float JOINT_WEIGHT[6] = {5.0f, 3.0f, 3.0f, 1.0f, 1.0f, 1.0f};

/**
 * @brief Initial joint configuration
 */
const JointConfig_t JOINT_CONFIG[6] = {
    {90.0f,  MOTOR_DIR_CCW, 50.0f,  0,   360, DIR_NEGATIVE},  // J1
    {90.0f,  MOTOR_DIR_CW,  50.89f, 90,  180, DIR_NEGATIVE},  // J2
    {-90.0f, MOTOR_DIR_CW,  50.89f, -90, 90,  DIR_NEGATIVE},  // J3
    {0.0f,   MOTOR_DIR_CW,  51.0f,  -90, 90,  DIR_NEGATIVE},  // J4
    {90.0f,  MOTOR_DIR_CCW, 26.85f, 0,   90,  DIR_POSITIVE},  // J5
    {0.0f,   MOTOR_DIR_CW,  51.0f,  0,   360, DIR_NEGATIVE},  // J6
};

/**
 * @brief Reset pose T0_6 transformation matrix
 * Robot arm in reset position (from zero-robotic-arm)
 */
const float T_0_6_RESET[4][4] = {
    {0, -1,  0,  0},
    {0,  0, -1, -47.63f},
    {1,  0,  0,  15.5f},
    {0,  0,  0,  1},
};
