/**
 * @file    robot_display.h
 * @brief   Robot arm status UI for 2.8" ILI9341 LCD (320x240 landscape)
 *
 * Screen layout:
 *  y=  0-19  Title bar      "ROBOT ARM CONTROL"  + mode badge
 *  y= 20-21  Separator
 *  y= 22-85  Joint angles   J1-J6 (two columns, 3 rows)
 *  y= 86-87  Separator
 *  y= 88-103 End-effector   X / Y / Z
 *  y=104-105 Separator
 *  y=106-121 Last command   "Cmd: ..."
 *  y=122-137 Status line    "Sts: OK / Error"
 *  y=138-239 (reserved)
 */

#ifndef __ROBOT_DISPLAY_H
#define __ROBOT_DISPLAY_H

#include <stdint.h>

/* Robot operating modes --------------------------------------------------- */
#define RDISP_MODE_IDLE     0
#define RDISP_MODE_RUNNING  1
#define RDISP_MODE_ERROR    2

/* Public API -------------------------------------------------------------- */

/**
 * @brief  Initialise LCD hardware, draw static UI skeleton.
 *         Call once after all other peripheral inits.
 */
void RobotDisplay_Init(void);

/**
 * @brief  Refresh the six joint angle fields.
 * @param  angles  Array of 6 joint angles in degrees (J1 … J6).
 */
void RobotDisplay_UpdateJoints(const float angles[6]);

/**
 * @brief  Refresh end-effector Cartesian coordinates.
 */
void RobotDisplay_UpdatePose(float x, float y, float z);

/**
 * @brief  Show the most-recently received serial command (max 38 chars).
 */
void RobotDisplay_UpdateCmd(const char *cmd);

/**
 * @brief  Show execution status text.
 * @param  text      Null-terminated status string (e.g. "OK", "Error: IK").
 * @param  is_error  Non-zero → display in red; zero → display in green.
 */
void RobotDisplay_UpdateStatus(const char *text, uint8_t is_error);

/**
 * @brief  Update the running-mode badge in the title bar.
 * @param  mode  RDISP_MODE_IDLE / RDISP_MODE_RUNNING / RDISP_MODE_ERROR
 */
void RobotDisplay_SetMode(uint8_t mode);

#endif /* __ROBOT_DISPLAY_H */
