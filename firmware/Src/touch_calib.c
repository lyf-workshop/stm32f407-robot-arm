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
    /* Use known point correspondences for accurate affine transform.
     *
     * Calibration targets:
     *   P0: lcd=(20, 20)   raw=(s_raw_points[0][0..1])   top-left
     *   P1: lcd=(300, 20)  raw=(s_raw_points[1][0..1])   top-right
     *   P2: lcd=(20, 220)  raw=(s_raw_points[2][0..1])   bottom-left
     *   P3: lcd=(300, 220) raw=(s_raw_points[3][0..1])   bottom-right
     *
     * X axis: P0 and P2 share lcd_x=20,  P1 and P3 share lcd_x=300
     *         -> average their raw_x to get two reference raw values
     * Y axis: P0 and P1 share lcd_y=20,  P2 and P3 share lcd_y=220
     *         -> average their raw_y to get two reference raw values
     *
     * This is more accurate than global min/max because it directly uses
     * the known screen-position <-> ADC-value correspondence.
     */

    int32_t rx_at_20  = ((int32_t)s_raw_points[0][0] + s_raw_points[2][0]) / 2;
    int32_t rx_at_300 = ((int32_t)s_raw_points[1][0] + s_raw_points[3][0]) / 2;
    int32_t ry_at_20  = ((int32_t)s_raw_points[0][1] + s_raw_points[1][1]) / 2;
    int32_t ry_at_220 = ((int32_t)s_raw_points[2][1] + s_raw_points[3][1]) / 2;

    int32_t raw_x_span = rx_at_300 - rx_at_20;  /* signed: handles axis inversion */
    int32_t raw_y_span = ry_at_220 - ry_at_20;

    if (raw_x_span == 0 || raw_y_span == 0) {
        /* Bad touch data, keep existing calibration unchanged */
        return;
    }

    /* a[0]: X scale  (280 LCD pixels across raw_x_span ADC counts) */
    calib[0] = (int32_t)(((int32_t)280 * 65536) / raw_x_span);
    calib[1] = 0;  /* no skew */
    /* a[2]: X offset so that rx_at_20 maps to lcd_x = 20 */
    calib[2] = (int32_t)20 * 65536 - calib[0] * rx_at_20;

    calib[3] = 0;  /* no skew */
    /* a[4]: Y scale  (200 LCD pixels across raw_y_span ADC counts) */
    calib[4] = (int32_t)(((int32_t)200 * 65536) / raw_y_span);
    /* a[5]: Y offset so that ry_at_20 maps to lcd_y = 20 */
    calib[5] = (int32_t)20 * 65536 - calib[4] * ry_at_20;
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
            
            /* Compute calibration and persist to Flash */
            int32_t calib[6];
            compute_calibration(calib);
            Touch_SetCalibration(calib);
            Touch_SaveCalibration();
            
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
}
