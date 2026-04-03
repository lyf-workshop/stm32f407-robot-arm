/**
 * @file    touch_ui.h
 * @brief   Unified Touch UI for robot arm control (320x240 landscape)
 *
 * This module provides:
 *   - Virtual jog buttons for J1~J6 (+/- per joint)
 *   - Control buttons: ZERO, STOP, SYNC
 *   - Step size selector: 1, 5, 10, 30 degrees
 *   - Integrated info panel: joint angles + status line
 *   - Title bar with step badge and mode indicator
 */

#ifndef __TOUCH_UI_H
#define __TOUCH_UI_H

#include <stdint.h>
#include <stdbool.h>

/* ---- Mode constants (compatible with old robot_display.h) --------------- */
#define RDISP_MODE_IDLE     0
#define RDISP_MODE_RUNNING  1
#define RDISP_MODE_ERROR    2

/* ---- Button event types ------------------------------------------------- */
typedef enum {
    BTN_EVENT_NONE = 0,
    BTN_EVENT_J1_PLUS,
    BTN_EVENT_J1_MINUS,
    BTN_EVENT_J2_PLUS,
    BTN_EVENT_J2_MINUS,
    BTN_EVENT_J3_PLUS,
    BTN_EVENT_J3_MINUS,
    BTN_EVENT_J4_PLUS,
    BTN_EVENT_J4_MINUS,
    BTN_EVENT_J5_PLUS,
    BTN_EVENT_J5_MINUS,
    BTN_EVENT_J6_PLUS,
    BTN_EVENT_J6_MINUS,
    BTN_EVENT_ZERO,
    BTN_EVENT_STOP,
    BTN_EVENT_STATUS,
    BTN_EVENT_STEP_1,
    BTN_EVENT_STEP_5,
    BTN_EVENT_STEP_10,
    BTN_EVENT_STEP_30
} TouchUI_Event_t;

/* ---- Core UI ------------------------------------------------------------ */

/** Initialize LCD, draw full UI (title + info panel + buttons). */
void TouchUI_Init(void);

/** Process a touch at (x,y) and return the button event. */
TouchUI_Event_t TouchUI_HandleTouch(uint16_t x, uint16_t y);

/** Get current step size in degrees. */
float TouchUI_GetStepSize(void);

/** Full UI redraw (e.g. after calibration or screen clear). */
void TouchUI_Redraw(void);

/* ---- Info panel updates ------------------------------------------------- */

/** Update the six joint angle fields in the info panel. */
void TouchUI_UpdateJoints(const float angles[6]);

/** Update the status line (green or red text). */
void TouchUI_UpdateStatus(const char *text, uint8_t is_error);

/** Update the mode badge [IDL] / [RUN] / [ERR] in the title bar. */
void TouchUI_SetMode(uint8_t mode);

#endif /* __TOUCH_UI_H */
