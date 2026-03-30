/**
 * @file    touch_ui.h
 * @brief   Touch UI components for robot arm control
 *
 * Provides virtual buttons for jog control:
 *   - 6 joints: J1~J6, each with + / - buttons
 *   - Common controls: Zero, Stop, Status
 *   - Step size selector: 1°, 5°, 10°, 30°
 */

#ifndef __TOUCH_UI_H
#define __TOUCH_UI_H

#include <stdint.h>
#include <stdbool.h>

/* --------------------------------------------------------------------------
 * Button event types
 * -------------------------------------------------------------------------- */
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
    BTN_EVENT_STEP_1,    /* Select 1° step */
    BTN_EVENT_STEP_5,    /* Select 5° step */
    BTN_EVENT_STEP_10,   /* Select 10° step */
    BTN_EVENT_STEP_30    /* Select 30° step */
} TouchUI_Event_t;

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

/**
 * @brief Initialize touch UI (draw buttons on screen)
 */
void TouchUI_Init(void);

/**
 * @brief Handle touch input and return button event
 * @param x LCD X coordinate (0~319)
 * @param y LCD Y coordinate (0~239)
 * @return Button event (BTN_EVENT_NONE if no button hit)
 */
TouchUI_Event_t TouchUI_HandleTouch(uint16_t x, uint16_t y);

/**
 * @brief Get current step size (degrees)
 * @return Current step size (1, 5, 10, or 30)
 */
float TouchUI_GetStepSize(void);

/**
 * @brief Redraw the UI (call after screen updates)
 */
void TouchUI_Redraw(void);

#endif /* __TOUCH_UI_H */
