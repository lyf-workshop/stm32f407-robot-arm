/**
 * @file    touch_ui.c
 * @brief   Unified Touch UI for robot arm (320×240 landscape)
 *
 * ┌─────────────────────────────────────┐  y=0
 * │  ROBOT ARM  [5°]  ZERO STOP  [IDL] │  Title bar (24px, navy bg)
 * ├─────────────────────────────────────┤  y=24
 * │ J1:+000.00  J2:+000.00  J3:+000.00 │  Joint row 1 (16px)
 * │ J4:+000.00  J5:+000.00  J6:+000.00 │  Joint row 2 (16px)
 * │ Sts: OK                            │  Status line (16px)
 * ├─────────────────────────────────────┤  y=72
 * │  [J1-][J1+]  [J2-][J2+]  [J3-][J3+]│  Jog row 1 (40px buttons)
 * │  [J4-][J4+]  [J5-][J5+]  [J6-][J6+]│  Jog row 2 (40px buttons)
 * ├─────────────────────────────────────┤  y=152
 * │[ZERO][STOP][SYNC]│ [1°][5°][10][30] │  Control + Step row (44px)
 * ├─────────────────────────────────────┤  y=196
 * │  ◄ X ►     ◄ Y ►     ◄ Z ►        │  XYZ row (44px, future use)
 * └─────────────────────────────────────┘  y=240
 */

#include "touch_ui.h"
#include "lcd.h"
#include "lcd_font.h"
#include <string.h>

/* ==========================================================================
 * Color palette
 * ========================================================================== */
#define COL_BG         RGB565(25,25,30)     /* Near-black background */
#define COL_TITLE_BG   RGB565(20,40,80)     /* Dark blue title bar */
#define COL_TITLE_FG   LCD_WHITE

#define COL_BTN_PLUS   RGB565(0,100,60)     /* Teal-green for + */
#define COL_BTN_MINUS  RGB565(140,30,30)    /* Dark red for - */
#define COL_BTN_CTRL   RGB565(30,60,120)    /* Steel blue for control */
#define COL_BTN_STEP   RGB565(50,50,55)     /* Neutral gray for step */
#define COL_BTN_SEL    RGB565(180,140,20)   /* Gold for selected step */
#define COL_BORDER     RGB565(80,80,90)     /* Subtle border */
#define COL_TEXT       LCD_WHITE
#define COL_ANGLE      RGB565(100,255,160)  /* Bright green for angles */
#define COL_STATUS_OK  RGB565(80,220,120)
#define COL_STATUS_ERR RGB565(255,80,80)

/* ==========================================================================
 * Layout constants (all in pixels)
 * ========================================================================== */
#define TITLE_Y   0
#define TITLE_H  24

#define INFO_Y   24   /* Info panel: joints + status */
#define INFO_H   48   /* 3 rows × 16px */

#define JOG_Y    74   /* Jog buttons start */
#define JOG_H    40   /* Height of each jog button */
#define JOG_GAP   2   /* Gap between buttons */

#define CTRL_Y  158   /* Control + step select row */
#define CTRL_H   38

#define XYZ_Y   198   /* Bottom XYZ control row */
#define XYZ_H   42

/* Button dimensions for 3-group jog layout (each group = [-][+]) */
#define JOG_GRP_W  104   /* Width of one joint group (J1-)(J1+) */
#define JOG_BTN_W   50   /* Width of single - or + button */
#define JOG_GRP_GAP  4   /* Gap between groups */

/* Control button dimensions */
#define CTRL_BTN_W  54
#define STEP_BTN_W  42

/* ==========================================================================
 * State
 * ========================================================================== */
static float s_step_size = 5.0f;
static uint8_t s_mode = 0;  /* 0=IDLE, 1=RUNNING, 2=ERROR */

/* ==========================================================================
 * Button table
 * ========================================================================== */
typedef struct {
    uint16_t x0, y0, x1, y1;
    TouchUI_Event_t event;
    const char *label;
    uint16_t bg_color;
} Btn_t;

/*
 * Jog buttons: 3 groups per row, 2 rows = 12 buttons
 *
 * Row 1 (y=74..114): [J1-][J1+]  [J2-][J2+]  [J3-][J3+]
 * Row 2 (y=116..156):[J4-][J4+]  [J5-][J5+]  [J6-][J6+]
 *
 * Group X positions:
 *   Group 0: x= 2.. 106  (btn: 2..52, 54..104)
 *   Group 1: x=110.. 214  (btn: 110..160, 162..212)
 *   Group 2: x=218.. 318  (btn: 218..268, 270..318)
 */
#define G0X  2
#define G1X  110
#define G2X  218

#define JROW1  JOG_Y
#define JROW2  (JOG_Y + JOG_H + 2)

static const Btn_t s_jog_btns[] = {
    /* Row 1: J1, J2, J3 */
    {G0X,       JROW1, G0X+50,  JROW1+JOG_H, BTN_EVENT_J1_MINUS, "J1-", COL_BTN_MINUS},
    {G0X+52,    JROW1, G0X+104, JROW1+JOG_H, BTN_EVENT_J1_PLUS,  "J1+", COL_BTN_PLUS},
    {G1X,       JROW1, G1X+50,  JROW1+JOG_H, BTN_EVENT_J2_MINUS, "J2-", COL_BTN_MINUS},
    {G1X+52,    JROW1, G1X+104, JROW1+JOG_H, BTN_EVENT_J2_PLUS,  "J2+", COL_BTN_PLUS},
    {G2X,       JROW1, G2X+50,  JROW1+JOG_H, BTN_EVENT_J3_MINUS, "J3-", COL_BTN_MINUS},
    {G2X+52,    JROW1, G2X+100, JROW1+JOG_H, BTN_EVENT_J3_PLUS,  "J3+", COL_BTN_PLUS},
    /* Row 2: J4, J5, J6 */
    {G0X,       JROW2, G0X+50,  JROW2+JOG_H, BTN_EVENT_J4_MINUS, "J4-", COL_BTN_MINUS},
    {G0X+52,    JROW2, G0X+104, JROW2+JOG_H, BTN_EVENT_J4_PLUS,  "J4+", COL_BTN_PLUS},
    {G1X,       JROW2, G1X+50,  JROW2+JOG_H, BTN_EVENT_J5_MINUS, "J5-", COL_BTN_MINUS},
    {G1X+52,    JROW2, G1X+104, JROW2+JOG_H, BTN_EVENT_J5_PLUS,  "J5+", COL_BTN_PLUS},
    {G2X,       JROW2, G2X+50,  JROW2+JOG_H, BTN_EVENT_J6_MINUS, "J6-", COL_BTN_MINUS},
    {G2X+52,    JROW2, G2X+100, JROW2+JOG_H, BTN_EVENT_J6_PLUS,  "J6+", COL_BTN_PLUS},
};
#define NUM_JOG_BTNS (sizeof(s_jog_btns) / sizeof(Btn_t))

/* Control buttons: ZERO, STOP, SYNC (left half of CTRL row) */
static const Btn_t s_ctrl_btns[] = {
    {2,   CTRL_Y, 56,  CTRL_Y+CTRL_H, BTN_EVENT_ZERO,   "ZERO", COL_BTN_CTRL},
    {58,  CTRL_Y, 112, CTRL_Y+CTRL_H, BTN_EVENT_STOP,   "STOP", COL_BTN_MINUS},
    {114, CTRL_Y, 168, CTRL_Y+CTRL_H, BTN_EVENT_STATUS, "SYNC", COL_BTN_CTRL},
};
#define NUM_CTRL_BTNS (sizeof(s_ctrl_btns) / sizeof(Btn_t))

/* Step size buttons: right half of CTRL row */
static const Btn_t s_step_btns[] = {
    {176, CTRL_Y, 210, CTRL_Y+CTRL_H, BTN_EVENT_STEP_1,  "1",  COL_BTN_STEP},
    {212, CTRL_Y, 246, CTRL_Y+CTRL_H, BTN_EVENT_STEP_5,  "5",  COL_BTN_STEP},
    {248, CTRL_Y, 282, CTRL_Y+CTRL_H, BTN_EVENT_STEP_10, "10", COL_BTN_STEP},
    {284, CTRL_Y, 318, CTRL_Y+CTRL_H, BTN_EVENT_STEP_30, "30", COL_BTN_STEP},
};
#define NUM_STEP_BTNS (sizeof(s_step_btns) / sizeof(Btn_t))

/* ==========================================================================
 * Drawing helpers
 * ========================================================================== */

static void draw_rounded_btn(const Btn_t *b, bool selected)
{
    uint16_t bg = selected ? COL_BTN_SEL : b->bg_color;
    uint16_t w = b->x1 - b->x0;
    uint16_t h = b->y1 - b->y0;

    /* Fill with 1px inset to leave room for border */
    LCD_FillRect(b->x0 + 1, b->y0 + 1, w - 2, h - 2, bg);

    /* Border */
    LCD_DrawLine(b->x0 + 1, b->y0,     b->x1 - 2, b->y0,     COL_BORDER);
    LCD_DrawLine(b->x0 + 1, b->y1 - 1, b->x1 - 2, b->y1 - 1, COL_BORDER);
    LCD_DrawLine(b->x0,     b->y0 + 1, b->x0,     b->y1 - 2, COL_BORDER);
    LCD_DrawLine(b->x1 - 1, b->y0 + 1, b->x1 - 1, b->y1 - 2, COL_BORDER);

    /* Label (centered) */
    uint16_t lw = (uint16_t)strlen(b->label) * 8;
    uint16_t tx = b->x0 + (w - lw) / 2;
    uint16_t ty = b->y0 + (h - 16) / 2;
    LCD_DrawString(tx, ty, b->label, COL_TEXT, bg);
}

/* Draw a small inline badge like "[5°]" in the title bar */
static void draw_step_badge(void)
{
    char badge[6];
    if (s_step_size < 1.5f)       { badge[0]='1'; badge[1]='\xF8'; badge[2]='\0'; }
    else if (s_step_size < 7.5f)  { badge[0]='5'; badge[1]='\xF8'; badge[2]='\0'; }
    else if (s_step_size < 20.0f) { badge[0]='1'; badge[1]='0'; badge[2]='\xF8'; badge[3]='\0'; }
    else                          { badge[0]='3'; badge[1]='0'; badge[2]='\xF8'; badge[3]='\0'; }

    /* Draw at a fixed position in title bar */
    LCD_FillRect(90, 0, 40, TITLE_H, COL_TITLE_BG);
    LCD_DrawChar(90, 4, '[', COL_BTN_SEL, COL_TITLE_BG);
    LCD_DrawString(98, 4, badge, COL_BTN_SEL, COL_TITLE_BG);
    uint16_t ex = 98 + (uint16_t)strlen(badge) * 8;
    LCD_DrawChar(ex, 4, ']', COL_BTN_SEL, COL_TITLE_BG);
}

/* Draw mode badge [IDL] / [RUN] / [ERR] in title bar right side */
static void draw_mode_badge(void)
{
    const char *txt;
    uint16_t fg;
    switch (s_mode) {
        case 1:  txt = "[RUN]"; fg = COL_STATUS_OK;  break;
        case 2:  txt = "[ERR]"; fg = COL_STATUS_ERR; break;
        default: txt = "[IDL]"; fg = LCD_YELLOW;     break;
    }
    LCD_FillRect(280, 0, 40, TITLE_H, COL_TITLE_BG);
    LCD_DrawString(280, 4, txt, fg, COL_TITLE_BG);
}

/* ==========================================================================
 * Info panel: joint angles + status (y=24..72)
 * These functions replace the old robot_display module for the info area.
 * ========================================================================== */

static void draw_info_bg(void)
{
    LCD_FillRect(0, INFO_Y, 320, INFO_H, COL_BG);
    /* Thin separator lines */
    LCD_DrawLine(0, INFO_Y, 319, INFO_Y, COL_BORDER);
    LCD_DrawLine(0, INFO_Y + INFO_H, 319, INFO_Y + INFO_H, COL_BORDER);
}

/* Format angle to "+XXX.X" (6 chars + NUL), compact version */
static void fmt_angle_s(char *buf, float angle)
{
    buf[0] = (angle < 0.0f) ? '-' : '+';
    if (angle < 0.0f) angle = -angle;
    int32_t i = (int32_t)angle;
    int32_t d = (int32_t)((angle - (float)i) * 10.0f + 0.5f);
    if (d >= 10) { d -= 10; i += 1; }
    if (i > 999)  { i = 999; d = 9; }
    buf[1] = (char)('0' + (i / 100) % 10);
    buf[2] = (char)('0' + (i / 10) % 10);
    buf[3] = (char)('0' + i % 10);
    buf[4] = '.';
    buf[5] = (char)('0' + d);
    buf[6] = '\0';
}

static char s_status_text[24] = "Ready";
static uint8_t s_status_err = 0;

/* ==========================================================================
 * Public API
 * ========================================================================== */

void TouchUI_Init(void)
{
    /* 0. Ensure LCD hardware is initialised (FSMC + ILI9341) */
    LCD_Init();

    /* 1. Full screen background */
    LCD_FillRect(0, 0, 320, 240, COL_BG);

    /* 2. Title bar */
    LCD_FillRect(0, TITLE_Y, 320, TITLE_H, COL_TITLE_BG);
    LCD_DrawString(4, 4, "ROBOT ARM", COL_TITLE_FG, COL_TITLE_BG);
    draw_step_badge();
    draw_mode_badge();

    /* 3. Info panel background + separator */
    draw_info_bg();

    /* 4. Jog buttons */
    for (uint16_t i = 0; i < NUM_JOG_BTNS; i++)
        draw_rounded_btn(&s_jog_btns[i], false);

    /* 5. Control buttons */
    for (uint16_t i = 0; i < NUM_CTRL_BTNS; i++)
        draw_rounded_btn(&s_ctrl_btns[i], false);

    /* 6. Step buttons (highlight current) */
    for (uint16_t i = 0; i < NUM_STEP_BTNS; i++) {
        bool sel = false;
        if (s_step_size < 1.5f  && s_step_btns[i].event == BTN_EVENT_STEP_1)  sel = true;
        if (s_step_size < 7.5f  && s_step_size >= 1.5f && s_step_btns[i].event == BTN_EVENT_STEP_5)  sel = true;
        if (s_step_size < 20.0f && s_step_size >= 7.5f && s_step_btns[i].event == BTN_EVENT_STEP_10) sel = true;
        if (s_step_size >= 20.0f && s_step_btns[i].event == BTN_EVENT_STEP_30) sel = true;
        draw_rounded_btn(&s_step_btns[i], sel);
    }

    /* 7. Step label "deg:" before step buttons */
    LCD_DrawString(144, CTRL_Y + 12, "deg", LCD_GRAY, COL_BG);
}

TouchUI_Event_t TouchUI_HandleTouch(uint16_t x, uint16_t y)
{
    /* Jog buttons */
    for (uint16_t i = 0; i < NUM_JOG_BTNS; i++) {
        if (x >= s_jog_btns[i].x0 && x < s_jog_btns[i].x1 &&
            y >= s_jog_btns[i].y0 && y < s_jog_btns[i].y1) {
            return s_jog_btns[i].event;
        }
    }

    /* Control buttons */
    for (uint16_t i = 0; i < NUM_CTRL_BTNS; i++) {
        if (x >= s_ctrl_btns[i].x0 && x < s_ctrl_btns[i].x1 &&
            y >= s_ctrl_btns[i].y0 && y < s_ctrl_btns[i].y1) {
            return s_ctrl_btns[i].event;
        }
    }

    /* Step size buttons */
    for (uint16_t i = 0; i < NUM_STEP_BTNS; i++) {
        if (x >= s_step_btns[i].x0 && x < s_step_btns[i].x1 &&
            y >= s_step_btns[i].y0 && y < s_step_btns[i].y1) {
            switch (s_step_btns[i].event) {
                case BTN_EVENT_STEP_1:  s_step_size = 1.0f;  break;
                case BTN_EVENT_STEP_5:  s_step_size = 5.0f;  break;
                case BTN_EVENT_STEP_10: s_step_size = 10.0f; break;
                case BTN_EVENT_STEP_30: s_step_size = 30.0f; break;
                default: break;
            }
            /* Redraw step buttons + title badge */
            for (uint16_t j = 0; j < NUM_STEP_BTNS; j++)
                draw_rounded_btn(&s_step_btns[j], (j == i));
            draw_step_badge();
            return s_step_btns[i].event;
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
    TouchUI_Init();
    /* Re-render dynamic info */
    float zeros[6] = {0};
    TouchUI_UpdateJoints(zeros);
    TouchUI_UpdateStatus(s_status_text, s_status_err);
}

/* ==========================================================================
 * Integrated info-panel updates (replace old robot_display functions)
 * ========================================================================== */

void TouchUI_UpdateJoints(const float angles[6])
{
    char a[7];
    /* Row 1: J1 J2 J3   at y = INFO_Y + 2 */
    uint16_t y1 = INFO_Y + 2;
    uint16_t y2 = INFO_Y + 18;

    uint16_t col_x[3] = {4, 112, 220};
    const char jlabel1[3] = {'1','2','3'};
    const char jlabel2[3] = {'4','5','6'};

    for (int c = 0; c < 3; c++) {
        uint16_t px = col_x[c];
        LCD_DrawChar(px, y1, 'J', LCD_GRAY, COL_BG);
        LCD_DrawChar(px + 8, y1, jlabel1[c], LCD_GRAY, COL_BG);
        LCD_DrawChar(px + 16, y1, ':', LCD_GRAY, COL_BG);
        fmt_angle_s(a, angles[c]);
        LCD_DrawString(px + 24, y1, a, COL_ANGLE, COL_BG);

        LCD_DrawChar(px, y2, 'J', LCD_GRAY, COL_BG);
        LCD_DrawChar(px + 8, y2, jlabel2[c], LCD_GRAY, COL_BG);
        LCD_DrawChar(px + 16, y2, ':', LCD_GRAY, COL_BG);
        fmt_angle_s(a, angles[c + 3]);
        LCD_DrawString(px + 24, y2, a, COL_ANGLE, COL_BG);
    }
}

void TouchUI_UpdateStatus(const char *text, uint8_t is_error)
{
    uint16_t y3 = INFO_Y + 34;
    uint16_t fg = is_error ? COL_STATUS_ERR : COL_STATUS_OK;

    /* Save for redraw */
    uint8_t i = 0;
    while (text[i] && i < 23) { s_status_text[i] = text[i]; i++; }
    s_status_text[i] = '\0';
    s_status_err = is_error;

    /* Clear line and draw */
    LCD_FillRect(0, y3, 320, 16, COL_BG);
    LCD_DrawString(4, y3, "Sts:", LCD_GRAY, COL_BG);
    LCD_DrawString(36, y3, s_status_text, fg, COL_BG);
}

void TouchUI_SetMode(uint8_t mode)
{
    s_mode = mode;
    draw_mode_badge();
}
