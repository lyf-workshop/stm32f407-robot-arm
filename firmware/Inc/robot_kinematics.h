/**
  ******************************************************************************
  * @file    robot_kinematics.h
  * @brief   Robot inverse kinematics algorithm (Pieper method)
  ******************************************************************************
  */

#ifndef __ROBOT_KINEMATICS_H__
#define __ROBOT_KINEMATICS_H__

#ifdef __cplusplus
extern "C" { 
#endif

#include "robot_config.h"
#include <stdint.h>
#include <stdbool.h>

#define ROBOT_KINEMATICS_RESULT_NUM (4U)
#define T_ROW_COL (4U)

typedef struct {
    float x;
    float y;
    float z;
} Position_t;

typedef struct {
    float T[T_ROW_COL][T_ROW_COL];
    float result[ROBOT_KINEMATICS_RESULT_NUM][ROBOT_MAX_JOINT_NUM];
    uint32_t result_invalid_mask;
} RobotKinematics_t;

/**
 * @brief Inverse kinematics solver
 * @param T_target Target transformation matrix (4x4, row-major)
 * @param result Output joint angles in degrees [6]
 * @param show Debug output flag (1=show, 0=silent)
 * @return 0 on success, -1 on failure
 */
int RobotKin_Inverse(float *T_target, float *result, int show);

/**
 * @brief Calculate new T matrix from current T and relative position
 * @param T_in Current transformation matrix
 * @param T_out Output transformation matrix
 * @param pos Relative position offset
 */
void RobotKin_CalcT(const float T_in[4][4], float T_out[4][4], Position_t *pos);

/**
 * @brief Update current joint angle for IK optimization
 * @param joint_id Joint index (0-5)
 * @param angle Current angle in degrees
 */
void RobotKin_UpdateJointAngle(uint32_t joint_id, float angle);

/**
 * @brief Update all joint angles at once
 * @param joint_angle Array of 6 joint angles in degrees
 */
void RobotKin_UpdateAllJointAngles(float *joint_angle);

/**
 * @brief Show all IK solution results (for debugging)
 */
void RobotKin_ShowResult(void);

/**
 * @brief Show transformation matrix (for debugging)
 */
void RobotKin_ShowT(float T[4][4]);

#ifdef __cplusplus
}
#endif

#endif /* __ROBOT_KINEMATICS_H__ */
