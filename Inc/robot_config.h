/**
  ******************************************************************************
  * @file    robot_config.h
  * @brief   Robot arm physical parameters and configuration
  ******************************************************************************
  */

#ifndef __ROBOT_CONFIG_H
#define __ROBOT_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#define ROBOT_MAX_JOINT_NUM 6
#define ROBOT_PULSES_PER_REV 3200  // 1.8° motor with 16 subdivision
#define ROBOT_MOTOR_BASE_ADDR 1    // Joint 1 address = 1, Joint 2 = 2, ...

#define ROBOT_ERROR_RANGE (1e-4f)
#define ROBOT_JOINT_ANGLE_ERROR_RANGE (1e-1f)

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

enum {
    ROBOT_JOINT_1 = 0,
    ROBOT_JOINT_2,
    ROBOT_JOINT_3,
    ROBOT_JOINT_4,
    ROBOT_JOINT_5,
    ROBOT_JOINT_6,
};

enum motor_dir {
    MOTOR_DIR_CW = 0,
    MOTOR_DIR_CCW = 1,
};

enum dir {
    DIR_POSITIVE = 1,
    DIR_NEGATIVE = -1,
};

/**
 * @brief DH parameters [a, alpha, d, theta_offset]
 * From zero-robotic-arm project (verified in MATLAB)
 * Units: a and d in mm, alpha and theta_offset in radians
 */
extern const float D_H[6][4];

/**
 * @brief Gear reduction ratios for each joint
 */
extern const float GEAR_RATIO[6];

/**
 * @brief Joint angle limits in degrees [min, max]
 */
extern const float JOINT_LIMITS[6][2];

/**
 * @brief Joint weight for optimal IK solution selection
 */
extern const float JOINT_WEIGHT[6];

/**
 * @brief Initial joint parameters
 */
typedef struct {
    float init_angle;           // Initial angle (degrees)
    enum motor_dir pos_direction; // Motor direction for positive joint rotation
    float reduction_ratio;      // Gear reduction ratio
    float min_angle;           // Minimum angle limit (degrees)
    float max_angle;           // Maximum angle limit (degrees)
    enum dir reset_dir;        // Reset direction
} JointConfig_t;

extern const JointConfig_t JOINT_CONFIG[6];

/**
 * @brief Reset pose transformation matrix T0_6
 * From zero-robotic-arm project
 */
extern const float T_0_6_RESET[4][4];

#ifdef __cplusplus
}
#endif

#endif /* __ROBOT_CONFIG_H */
