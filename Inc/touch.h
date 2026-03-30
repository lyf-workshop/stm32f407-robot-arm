/**
 * @file    touch.h
 * @brief   XPT2046 Resistive Touch Controller Driver (SPI interface)
 *
 * Hardware mapping (per pin table):
 *   T_CLK  = PD13  (SPI_SCK, software SPI)
 *   T_CS   = PE0   (Chip Select, active low)
 *   T_MOSI = PE2   (Master Out Slave In)
 *   T_MISO = PE3   (Master In Slave Out)
 *   T_PEN  = Not used (optional IRQ, we use polling mode)
 *
 * Coordinate system:
 *   Raw ADC: 12-bit (0~4095) for X and Y
 *   LCD space: 320×240 (landscape)
 *   Calibration: 4-point calibration matrix stored in g_touch_cal
 */

#ifndef __TOUCH_H
#define __TOUCH_H

#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* --------------------------------------------------------------------------
 * Touch state structure
 * -------------------------------------------------------------------------- */
typedef struct {
    bool     pressed;     /* true if touch detected */
    uint16_t x;           /* LCD coordinate X (0~319) */
    uint16_t y;           /* LCD coordinate Y (0~239) */
    uint16_t raw_x;       /* Raw ADC X (0~4095) */
    uint16_t raw_y;       /* Raw ADC Y (0~4095) */
} Touch_State_t;

/* --------------------------------------------------------------------------
 * Calibration data (4-point affine transform)
 * -------------------------------------------------------------------------- */
typedef struct {
    int32_t  a[6];        /* Affine transform coefficients */
    bool     valid;       /* true if calibration data is valid */
} Touch_Calib_t;

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

/**
 * @brief Initialize XPT2046 touch controller (software SPI)
 */
void Touch_Init(void);

/**
 * @brief Read touch state (blocking, ~1ms)
 * @param state Output touch state structure
 * @return true if touch detected, false otherwise
 */
bool Touch_Read(Touch_State_t *state);

/**
 * @brief Check if screen is currently being touched (quick poll)
 * @return true if pressed
 */
bool Touch_IsPressed(void);

/**
 * @brief Load calibration data from internal FLASH or use defaults
 */
void Touch_LoadCalibration(void);

/**
 * @brief Save calibration data to internal FLASH
 */
void Touch_SaveCalibration(void);

/**
 * @brief Set calibration coefficients manually
 * @param a Array of 6 affine transform coefficients
 */
void Touch_SetCalibration(const int32_t a[6]);

/**
 * @brief Get default calibration (for typical XPT2046+ILI9341 combo)
 */
void Touch_GetDefaultCalibration(int32_t a[6]);

#endif /* __TOUCH_H */
