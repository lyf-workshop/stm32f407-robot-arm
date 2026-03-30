/**
  ******************************************************************************
  * @file    robot_control.h
  * @brief   Robot arm control logic (bare-metal version)
  ******************************************************************************
  */

#ifndef __ROBOT_CONTROL_H__
#define __ROBOT_CONTROL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "robot_config.h"
#include "robot_kinematics.h"
#include <stdint.h>
#include <stdbool.h>

#define ROBOT_CAN_DELAY 10
#define ROBOT_CAN_TIMEOUT 100

typedef struct {
    float current_angle;           // Current angle in degrees
    enum motor_dir pos_direction;  // Motor direction for positive joint rotation
    float reduction_ratio;         // Gear reduction ratio
    float min_angle;              // Min angle limit (degrees)
    float max_angle;              // Max angle limit (degrees)
    enum dir reset_dir;           // Reset direction
    float velocity;               // Current velocity (deg/s)
} Joint_t;

typedef struct {
    Joint_t joints[ROBOT_MAX_JOINT_NUM];
    float T_current[4][4];        // Current end-effector pose
    bool initialized;
} RobotArm_t;

extern RobotArm_t g_robot_arm;

/**
 * @brief Initialize robot arm control system
 */
void RobotCtrl_Init(void);

/**
 * @brief Move to target joint angles (absolute positioning)
 * @param target_angles Array of 6 target angles in degrees
 * @param velocity Speed in deg/s
 * @param acceleration Acceleration (0-255)
 * @param sync Use multi-motor synchronization
 * @return 0 on success, -1 on error
 */
int RobotCtrl_MoveToJointAngles(const float target_angles[6], float velocity, uint8_t acceleration, bool sync);

/**
 * @brief Move single joint by relative angle
 * @param joint_id Joint index (0-5)
 * @param rel_angle Relative angle in degrees (+ or -)
 * @param velocity Speed in deg/s
 * @param acceleration Acceleration (0-255)
 * @return 0 on success, -1 on error
 */
int RobotCtrl_MoveJointRelative(uint8_t joint_id, float rel_angle, float velocity, uint8_t acceleration);

/**
 * @brief Move end-effector to Cartesian position (IK + motion)
 * @param x X coordinate in mm
 * @param y Y coordinate in mm
 * @param z Z coordinate in mm
 * @param velocity Speed scale (0.0-1.0)
 * @return 0 on success, -1 on IK failure
 */
int RobotCtrl_MoveToPose(float x, float y, float z, float velocity);

/**
 * @brief Reset current position as zero point
 */
void RobotCtrl_SetCurrentAsZero(void);

/**
 * @brief Emergency stop all motors
 */
void RobotCtrl_StopAll(void);

/**
 * @brief Read actual pulse count from one motor via CAN (blocking, 100ms timeout).
 * @param joint_id  Joint index 0-5
 * @param out_pulses  Returned signed pulse count from motor encoder
 * @return 0 on success, -1 on CAN timeout
 */
int RobotCtrl_ReadMotorPulses(uint8_t joint_id, int32_t *out_pulses);

/**
 * @brief Sync all six joint angles from motor encoders.
 *        Overwrites software-tracked current_angle with encoder-derived values.
 * @return Number of joints successfully read (0-6)
 */
int RobotCtrl_SyncAnglesFromMotors(void);

/**
 * @brief Convert angle to motor pulse count
 * @param angle Angle in degrees
 * @param joint_id Joint index (0-5)
 * @return Absolute pulse count
 */
int32_t RobotCtrl_AngleToPulse(float angle, uint8_t joint_id);

/**
 * @brief Get current joint angle
 * @param joint_id Joint index (0-5)
 * @return Current angle in degrees
 */
float RobotCtrl_GetJointAngle(uint8_t joint_id);

#ifdef __cplusplus
}
#endif

#endif /* __ROBOT_CONTROL_H__ */
