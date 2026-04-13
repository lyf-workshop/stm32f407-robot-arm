/**
  ******************************************************************************
  * @file    robot_sequence.h
  * @brief   Pre-defined motion sequences for robot arm demo
  ******************************************************************************
  */

#ifndef __ROBOT_SEQUENCE_H__
#define __ROBOT_SEQUENCE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "robot_kinematics.h"
#include <stdint.h>

typedef struct {
    float x;
    float y;
    float z;
    float velocity_scale;  // 0.1 ~ 1.0
} WaypointCartesian_t;

typedef struct {
    float joints[6];       // Joint angles in degrees
    float velocity_scale;  // 0.1 ~ 1.0
} WaypointJoint_t;

/**
 * @brief Execute pre-defined pick-and-place sequence
 * @return 0 on success, -1 on error
 */
int RobotSeq_PickAndPlace(void);

/**
 * @brief Execute joint space demo sequence
 * @return 0 on success, -1 on error
 */
int RobotSeq_JointDemo(void);

/**
 * @brief Execute Cartesian space circular motion
 * @return 0 on success, -1 on error
 */
int RobotSeq_CircleDemo(void);

#ifdef __cplusplus
}
#endif

#endif /* __ROBOT_SEQUENCE_H__ */
