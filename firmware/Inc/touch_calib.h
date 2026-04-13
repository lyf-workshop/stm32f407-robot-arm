/**
 * @file    touch_calib.h
 * @brief   4-point touch calibration for XPT2046 + ILI9341
 *
 * Usage:
 *   1. Call TouchCalib_Start() to begin calibration
 *   2. UI shows 4 crosshairs at corners
 *   3. User taps each crosshair in sequence
 *   4. Calibration computes affine transform
 *   5. Call Touch_SetCalibration() with result
 */

#ifndef __TOUCH_CALIB_H
#define __TOUCH_CALIB_H

#include <stdint.h>
#include <stdbool.h>

/* --------------------------------------------------------------------------
 * Calibration state machine
 * -------------------------------------------------------------------------- */
typedef enum {
    CALIB_STATE_IDLE = 0,
    CALIB_STATE_POINT1,   /* Top-left */
    CALIB_STATE_POINT2,   /* Top-right */
    CALIB_STATE_POINT3,   /* Bottom-left */
    CALIB_STATE_POINT4,   /* Bottom-right */
    CALIB_STATE_COMPUTING,
    CALIB_STATE_DONE
} TouchCalib_State_t;

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

/**
 * @brief Start calibration process (draws UI, resets state)
 */
void TouchCalib_Start(void);

/**
 * @brief Feed touch input to calibration state machine
 * @param raw_x Raw ADC X value
 * @param raw_y Raw ADC Y value
 * @return true if calibration is complete
 */
bool TouchCalib_ProcessTouch(uint16_t raw_x, uint16_t raw_y);

/**
 * @brief Get calibration state
 */
TouchCalib_State_t TouchCalib_GetState(void);

/**
 * @brief Cancel calibration and restore previous screen
 */
void TouchCalib_Cancel(void);

#endif /* __TOUCH_CALIB_H */
