/**
 * @file    robot_display.c
 * @brief   Compatibility shim — forwards all calls to the unified TouchUI.
 *
 * This file is kept so the Keil project compiles without removing source files.
 * All real drawing is done in touch_ui.c.
 */

#include "robot_display.h"
#include "touch_ui.h"

void RobotDisplay_Init(void)               { TouchUI_Init(); }
void RobotDisplay_UpdateJoints(const float angles[6]) { TouchUI_UpdateJoints(angles); }
void RobotDisplay_UpdatePose(float x, float y, float z) { (void)x; (void)y; (void)z; }
void RobotDisplay_UpdateCmd(const char *cmd) { (void)cmd; }
void RobotDisplay_UpdateStatus(const char *text, uint8_t is_error) { TouchUI_UpdateStatus(text, is_error); }
void RobotDisplay_SetMode(uint8_t mode)     { TouchUI_SetMode(mode); }
