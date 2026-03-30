/**
 * @file    touch_ui.c
 * @brief   Touch UI implementation for robot arm jog control
 *
 * Layout (320×240, landscape):
 *   Top 30px: Title bar (from robot_display)
 *   30~210px: 6 rows × 4 columns of buttons (30px height each)
 *   210~240px: Bottom bar with step size selector
 */

#include "touch_ui.h"
#include "lcd.h"
#include "lcd_font.h"
#include <string.h>

/* --------------------------------------------------------------------------
 * Button layout constants
 * -------------------------------------------------------------------------- */
#define BTN_HEIGHT      30
#define BTN_WIDTH       80
#define BTN_MARGIN      0

#define BTN_START_Y     30    /* Below title bar */
#define BTN_ROWS        6
#define BTN_COLS        4

#define STEP_BAR_Y      210   /* Bottom bar for step size */

/* Button colors */
#define BTN_COLOR_BG         0x2945   /* Dark blue-gray */
#define BTN_COLOR_PRESSED    LCD_NAVY
#define BTN_COLOR_TEXT       LCD_WHITE
#define BTN_COLOR_PLUS       0x07E0   /* Green for + */
#define BTN_COLOR_MINUS      0xF800   /* Red for - */
#define BTN_COLOR_CTRL       0xFD20   /* Orange for control buttons */
#define BTN_COLOR_STEP       0x4208   /* Gray for step selector */
#define BTN_COLOR_STEP_SEL   LCD_YELLOW /* Yellow for selected step */

/* Current step size */
static float s_step_size = 5.0f;

/* --------------------------------------------------------------------------
 * Button structure
 * -------------------------------------------------------------------------- */
typedef struct {
    uint16_t x0, y0, x1, y1;
    TouchUI_Event_t event;
    const char *label;
    uint16_t bg_color;
} Button_t;

/* Button definitions (4 columns × 6 rows) */
static const Button_t s_buttons[] = {
    /* Row 0: J1 controls */
    {0,   30,  80,  60,  BTN_EVENT_J1_MINUS, "J1-", BTN_COLOR_MINUS},
    {80,  30, 160,  60,  BTN_EVENT_J1_PLUS,  "J1+", BTN_COLOR_PLUS},
    {160, 30, 240,  60,  BTN_EVENT_J2_MINUS, "J2-", BTN_COLOR_MINUS},
    {240, 30, 320,  60,  BTN_EVENT_J2_PLUS,  "J2+", BTN_COLOR_PLUS},
    
    /* Row 1: J3 controls */
    {0,   60,  80,  90,  BTN_EVENT_J3_MINUS, "J3-", BTN_COLOR_MINUS},
    {80,  60, 160,  90,  BTN_EVENT_J3_PLUS,  "J3+", BTN_COLOR_PLUS},
    {160, 60, 240,  90,  BTN_EVENT_J4_MINUS, "J4-", BTN_COLOR_MINUS},
    {240, 60, 320,  90,  BTN_EVENT_J4_PLUS,  "J4+", BTN_COLOR_PLUS},
    
    /* Row 2: J5 controls */
    {0,   90,  80, 120,  BTN_EVENT_J5_MINUS, "J5-", BTN_COLOR_MINUS},
    {80,  90, 160, 120,  BTN_EVENT_J5_PLUS,  "J5+", BTN_COLOR_PLUS},
    {160, 90, 240, 120,  BTN_EVENT_J6_MINUS, "J6-", BTN_COLOR_MINUS},
    {240, 90, 320, 120,  BTN_EVENT_J6_PLUS,  "J6+", BTN_COLOR_PLUS},
    
    /* Row 3: Control buttons */
    {0,  120,  80, 150,  BTN_EVENT_ZERO,   "ZERO",   BTN_COLOR_CTRL},
    {80, 120, 160, 150,  BTN_EVENT_STOP,   "STOP",   BTN_COLOR_CTRL},
    {160,120, 240, 150,  BTN_EVENT_STATUS, "STATUS", BTN_COLOR_CTRL},
    {240,120, 320, 150,  BTN_EVENT_NONE,   "---",    BTN_COLOR_BG},
    
    /* Rows 4-5: Reserved / Info display (no buttons, just background) */
};

#define NUM_BUTTONS (sizeof(s_buttons) / sizeof(Button_t))

/* Step size buttons (bottom bar) */
static const Button_t s_step_buttons[] = {
    {0,   210,  80, 240, BTN_EVENT_STEP_1,  "1deg",  BTN_COLOR_STEP},
    {80,  210, 160, 240, BTN_EVENT_STEP_5,  "5deg",  BTN_COLOR_STEP},
    {160, 210, 240, 240, BTN_EVENT_STEP_10, "10deg", BTN_COLOR_STEP},
    {240, 210, 320, 240, BTN_EVENT_STEP_30, "30deg", BTN_COLOR_STEP}
};

#define NUM_STEP_BUTTONS (sizeof(s_step_buttons) / sizeof(Button_t))

/* --------------------------------------------------------------------------
 * Helper: Draw a button
 * -------------------------------------------------------------------------- */
static void draw_button(const Button_t *btn, bool selected)
{
    uint16_t bg = selected ? BTN_COLOR_STEP_SEL : btn->bg_color;
    
    /* Draw button background */
    LCD_FillRect(btn->x0, btn->y0, btn->x1 - btn->x0, btn->y1 - btn->y0, bg);
    
    /* Draw border (1px white outline) */
    LCD_DrawLine(btn->x0, btn->y0, btn->x1 - 1, btn->y0, LCD_WHITE);
    LCD_DrawLine(btn->x0, btn->y1 - 1, btn->x1 - 1, btn->y1 - 1, LCD_WHITE);
    LCD_DrawLine(btn->x0, btn->y0, btn->x0, btn->y1 - 1, LCD_WHITE);
    LCD_DrawLine(btn->x1 - 1, btn->y0, btn->x1 - 1, btn->y1 - 1, LCD_WHITE);
    
    /* Draw label centered */
    uint16_t label_len = strlen(btn->label);
    uint16_t label_w = label_len * 8;  /* 8px per char */
    uint16_t label_h = 16;
    uint16_t text_x = btn->x0 + ((btn->x1 - btn->x0) - label_w) / 2;
    uint16_t text_y = btn->y0 + ((btn->y1 - btn->y0) - label_h) / 2;
    
    LCD_DrawString(text_x, text_y, btn->label, BTN_COLOR_TEXT, bg);
}

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */

void TouchUI_Init(void)
{
    /* Draw all main buttons */
    for (uint16_t i = 0; i < NUM_BUTTONS; i++) {
        draw_button(&s_buttons[i], false);
    }
    
    /* Draw step size buttons (highlight default 5°) */
    for (uint16_t i = 0; i < NUM_STEP_BUTTONS; i++) {
        bool selected = (s_step_buttons[i].event == BTN_EVENT_STEP_5);
        draw_button(&s_step_buttons[i], selected);
    }
}

TouchUI_Event_t TouchUI_HandleTouch(uint16_t x, uint16_t y)
{
    /* Check main buttons */
    for (uint16_t i = 0; i < NUM_BUTTONS; i++) {
        if (x >= s_buttons[i].x0 && x < s_buttons[i].x1 &&
            y >= s_buttons[i].y0 && y < s_buttons[i].y1) {
            return s_buttons[i].event;
        }
    }
    
    /* Check step size buttons */
    for (uint16_t i = 0; i < NUM_STEP_BUTTONS; i++) {
        if (x >= s_step_buttons[i].x0 && x < s_step_buttons[i].x1 &&
            y >= s_step_buttons[i].y0 && y < s_step_buttons[i].y1) {
            
            /* Update step size */
            switch (s_step_buttons[i].event) {
                case BTN_EVENT_STEP_1:  s_step_size = 1.0f;  break;
                case BTN_EVENT_STEP_5:  s_step_size = 5.0f;  break;
                case BTN_EVENT_STEP_10: s_step_size = 10.0f; break;
                case BTN_EVENT_STEP_30: s_step_size = 30.0f; break;
                default: break;
            }
            
            /* Redraw step buttons to show selection */
            for (uint16_t j = 0; j < NUM_STEP_BUTTONS; j++) {
                bool selected = (j == i);
                draw_button(&s_step_buttons[j], selected);
            }
            
            return s_step_buttons[i].event;
        }
    }
    
    return BTN_EVENT_NONE;
}

float TouchUI_GetStepSize(void)
{
    return s_step_size;
}

void TouchUI_Redraw(void)
{
    /* Redraw all buttons (called after screen is cleared/changed) */
    for (uint16_t i = 0; i < NUM_BUTTONS; i++) {
        draw_button(&s_buttons[i], false);
    }
    
    for (uint16_t i = 0; i < NUM_STEP_BUTTONS; i++) {
        bool selected = false;
        if (s_step_size == 1.0f && s_step_buttons[i].event == BTN_EVENT_STEP_1) selected = true;
        if (s_step_size == 5.0f && s_step_buttons[i].event == BTN_EVENT_STEP_5) selected = true;
        if (s_step_size == 10.0f && s_step_buttons[i].event == BTN_EVENT_STEP_10) selected = true;
        if (s_step_size == 30.0f && s_step_buttons[i].event == BTN_EVENT_STEP_30) selected = true;
        draw_button(&s_step_buttons[i], selected);
    }
}
