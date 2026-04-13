/**
  ******************************************************************************
  * @file    robot_examples.h
  * @brief   Robot arm usage examples
  ******************************************************************************
  */

#ifndef __ROBOT_EXAMPLES_H__
#define __ROBOT_EXAMPLES_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Example 1: Simple joint rotation test
 */
void Example_SingleJointTest(void);

/**
 * @brief Example 2: Multi-axis synchronized motion
 */
void Example_SyncMotion(void);

/**
 * @brief Example 3: Cartesian space control (IK)
 */
void Example_CartesianControl(void);

/**
 * @brief Example 4: Pre-defined sequence execution
 */
void Example_SequenceExecution(void);

/**
 * @brief Example 5: RL action execution simulation
 */
void Example_RLActionSimulation(void);

#ifdef __cplusplus
}
#endif

#endif /* __ROBOT_EXAMPLES_H__ */
