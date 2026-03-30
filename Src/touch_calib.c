/**
 * @file    touch_calib.c
 * @brief   4-point touch calibration implementation
 *
 * Calibration procedure:
 *   1. Display crosshair at (20, 20)    -> collect raw ADC (x1, y1)
 *   2. Display crosshair at (300, 20)   -> collect raw ADC (x2, y2)
 *   3. Display crosshair at (20, 220)   -> collect raw ADC (x3, y3)
 *   4. Display crosshair at (300, 220)  -> collect raw ADC (x4, y4)
 *   5. Compute affine transform coefficients
 *   6. Save to Touch_SetCalibration()
 */

#include "touch_calib.h"
#include "touch.h"
#include "lcd.h"
#include <string.h>

/* --------------------------------------------------------------------------
 * Calibration points (LCD coordinates)
 * -------------------------------------------------------------------------- */
static const uint16_t s_lcd_points[4][2] = {
    {20,  20},   /* Top-left */
    {300, 20},   /* Top-right */
    {20,  220},  /* Bottom-left */
    {300, 220}   /* Bottom-right */
};

/* Collected raw ADC values */
static uint16_t s_raw_points[4][2];

/* State machine */
static TouchCalib_State_t s_state = CALIB_STATE_IDLE;
static uint32_t s_last_touch_time = 0;

/* --------------------------------------------------------------------------
 * Helper: Draw crosshair target
 * -------------------------------------------------------------------------- */
static void draw_crosshair(uint16_t x, uint16_t y, uint16_t color)
{
    /* Draw horizontal line */
    LCD_DrawLine(x - 10, y, x + 10, y, color);
    /* Draw vertical line */
    LCD_DrawLine(x, y - 10, x, y + 10, color);
    /* Draw center dot */
    LCD_FillRect(x - 2, y - 2, 5, 5, color);
}

/* --------------------------------------------------------------------------
 * Helper: Compute affine transform from 4 point pairs
 * -------------------------------------------------------------------------- */
static void compute_calibration(int32_t calib[6])
{
    /* We have 4 pairs: (raw_x, raw_y) -> (lcd_x, lcd_y)
     * Affine transform: lcd_x = (a0*raw_x + a1*raw_y + a2) / 65536
     *                   lcd_y = (a3*raw_x + a4*raw_y + a5) / 65536
     *
     * For simplicity, we use a linear fit (assume minimal skew):
     *   a0 = (lcd_x_max - lcd_x_min) / (raw_x_max - raw_x_min) * 65536
     *   a1 = 0 (no skew)
     *   a2 = -raw_x_min * a0
     *   a4 = (lcd_y_max - lcd_y_min) / (raw_y_max - raw_y_min) * 65536
     *   a3 = 0
     *   a5 = -raw_y_min * a4
     */
    
    /* Find raw X/Y ranges */
    uint16_t raw_x_min = 4095, raw_x_max = 0;
    uint16_t raw_y_min = 4095, raw_y_max = 0;
    
    for (int i = 0; i < 4; i++) {
        if (s_raw_points[i][0] < raw_x_min) raw_x_min = s_raw_points[i][0];
        if (s_raw_points[i][0] > raw_x_max) raw_x_max = s_raw_points[i][0];
        if (s_raw_points[i][1] < raw_y_min) raw_y_min = s_raw_points[i][1];
        if (s_raw_points[i][1] > raw_y_max) raw_y_max = s_raw_points[i][1];
    }
    
    /* LCD ranges: X: 20~300 (280px), Y: 20~220 (200px) */
    int32_t lcd_x_span = 280;
    int32_t lcd_y_span = 200;
    int32_t raw_x_span = raw_x_max - raw_x_min;
    int32_t raw_y_span = raw_y_max - raw_y_min;
    
    /* Compute coefficients (fixed-point * 65536) */
    calib[0] = (lcd_x_span * 65536) / raw_x_span;     /* a0 */
    calib[1] = 0;                                      /* a1 */
    calib[2] = 20 * 65536 - (int32_t)raw_x_min * calib[0];  /* a2 */
    
    calib[3] = 0;                                      /* a3 */
    calib[4] = (lcd_y_span * 65536) / raw_y_span;     /* a4 */
    calib[5] = 20 * 65536 - (int32_t)raw_y_min * calib[4];  /* a5 */
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void TouchCalib_Start(void)
{
    /* Clear screen and show instructions */
    LCD_Clear(LCD_BLACK);
    LCD_DrawString(20, 10, "Touch Calibration", LCD_WHITE, LCD_BLACK);
    LCD_DrawString(20, 30, "Tap each crosshair", LCD_CYAN, LCD_BLACK);
    
    /* Draw first crosshair */
    draw_crosshair(s_lcd_points[0][0], s_lcd_points[0][1], LCD_RED);
    
    /* Reset state */
    s_state = CALIB_STATE_POINT1;
    s_last_touch_time = HAL_GetTick();
    memset(s_raw_points, 0, sizeof(s_raw_points));
}

bool TouchCalib_ProcessTouch(uint16_t raw_x, uint16_t raw_y)
{
    uint32_t now = HAL_GetTick();
    
    /* Debounce: ignore touches < 300ms apart */
    if (now - s_last_touch_time < 300) {
        return false;
    }
    s_last_touch_time = now;
    
    switch (s_state) {
        case CALIB_STATE_POINT1:
            s_raw_points[0][0] = raw_x;
            s_raw_points[0][1] = raw_y;
            s_state = CALIB_STATE_POINT2;
            LCD_Clear(LCD_BLACK);
            LCD_DrawString(20, 10, "Point 1/4 OK", LCD_GREEN, LCD_BLACK);
            draw_crosshair(s_lcd_points[1][0], s_lcd_points[1][1], LCD_RED);
            break;
            
        case CALIB_STATE_POINT2:
            s_raw_points[1][0] = raw_x;
            s_raw_points[1][1] = raw_y;
            s_state = CALIB_STATE_POINT3;
            LCD_Clear(LCD_BLACK);
            LCD_DrawString(20, 10, "Point 2/4 OK", LCD_GREEN, LCD_BLACK);
            draw_crosshair(s_lcd_points[2][0], s_lcd_points[2][1], LCD_RED);
            break;
            
        case CALIB_STATE_POINT3:
            s_raw_points[2][0] = raw_x;
            s_raw_points[2][1] = raw_y;
            s_state = CALIB_STATE_POINT4;
            LCD_Clear(LCD_BLACK);
            LCD_DrawString(20, 10, "Point 3/4 OK", LCD_GREEN, LCD_BLACK);
            draw_crosshair(s_lcd_points[3][0], s_lcd_points[3][1], LCD_RED);
            break;
            
        case CALIB_STATE_POINT4:
            s_raw_points[3][0] = raw_x;
            s_raw_points[3][1] = raw_y;
            s_state = CALIB_STATE_COMPUTING;
            
            /* Compute calibration */
            int32_t calib[6];
            compute_calibration(calib);
            Touch_SetCalibration(calib);
            
            /* Show success message */
            LCD_Clear(LCD_BLACK);
            LCD_DrawString(60, 100, "Calibration Complete!", LCD_GREEN, LCD_BLACK);
            LCD_DrawString(60, 120, "Touch to continue...", LCD_CYAN, LCD_BLACK);
            
            s_state = CALIB_STATE_DONE;
            return true;
            
        default:
            break;
    }
    
    return false;
}

TouchCalib_State_t TouchCalib_GetState(void)
{
    return s_state;
}

void TouchCalib_Cancel(void)
{
    s_state = CALIB_STATE_IDLE;
    LCD_Clear(LCD_BLACK);
}
