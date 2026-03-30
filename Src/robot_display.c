/**
 * @file    robot_display.c
 * @brief   Robot arm status UI for 2.8" ILI9341 LCD (320×240 landscape).
 *
 * Screen layout (all Y values are top-of-text, using 8×16 font):
 *
 *   y=  0  ┌──────────────────────────────────┐
 *            │ ROBOT ARM CONTROL         [IDL] │  (navy bg, white/yellow text)
 *   y= 20  ├──────────────────────────────────┤  (separator line)
 *   y= 22  │ J1:+123.45   J4: -45.00         │
 *   y= 38  │ J2: +90.00   J5: +90.00         │  (joint angles, green text)
 *   y= 54  │ J3: -90.00   J6:  +0.00         │
 *   y= 70  ├──────────────────────────────────┤
 *   y= 72  │ X:+0000.0  Y:-0184.5  Z:+0215.5 │  (end-effector, cyan text)
 *   y= 88  ├──────────────────────────────────┤
 *   y= 90  │ Cmd:sync 90 90 0 0 90 0          │  (last command, white text)
 *   y=106  │ Sts:OK                           │  (status, green/red text)
 *   y=122  └──────────────────────────────────┘
 *
 * Floating-point formatting avoids printf to stay compatible with MicroLib.
 */

#include "robot_display.h"
#include "lcd.h"
#include "lcd_font.h"
#include <string.h>

/* ── Layout constants ───────────────────────────────────────────────────── */
#define Y_TITLE_BG    0      /* title bar top                               */
#define Y_TITLE_H    20      /* title bar height (px)                       */
#define Y_TITLE_TXT   2      /* text row inside title bar                   */
#define X_TITLE_TXT   4
#define X_MODE_BADGE  272    /* "[IDL]" starts here (5 chars × 8 = 40 px)  */

#define Y_SEP1       20
#define Y_JOINT1     22
#define Y_JOINT2     38
#define Y_JOINT3     54
#define Y_SEP2       70
#define Y_EFF        72
#define Y_SEP3       88
#define Y_CMD        90
#define Y_STS       106
#define Y_SEP4      122

/* Background colour for data rows */
#define BG_DATA      RGB565(20,20,20)

/* ── Static helpers ─────────────────────────────────────────────────────── */

/** Draw a full-width 1-pixel horizontal separator */
static void draw_sep(uint16_t y)
{
    LCD_FillRect(0, y, LCD_WIDTH, 1, LCD_GRAY);
}

/**
 * Format a signed angle to "+XXX.XX" (7 chars + NUL).
 * Range: ±999.99°.  No printf dependency.
 */
static void fmt_angle(char *buf, float angle)
{
    buf[0] = (angle < 0.0f) ? '-' : '+';
    if (angle < 0.0f) angle = -angle;

    int32_t i = (int32_t)angle;
    int32_t d = (int32_t)((angle - (float)i) * 100.0f + 0.5f);
    if (d >= 100) { d -= 100; i += 1; }
    if (i > 999)  { i = 999; d = 99; }

    buf[1] = (char)('0' + (i / 100) % 10);
    buf[2] = (char)('0' + (i /  10) % 10);
    buf[3] = (char)('0' + i % 10);
    buf[4] = '.';
    buf[5] = (char)('0' + d / 10);
    buf[6] = (char)('0' + d % 10);
    buf[7] = '\0';
}

/**
 * Format a signed coordinate to "+XXXX.X" (7 chars + NUL).
 * Range: ±9999.9 mm.
 */
static void fmt_coord(char *buf, float v)
{
    buf[0] = (v < 0.0f) ? '-' : '+';
    if (v < 0.0f) v = -v;

    int32_t i = (int32_t)v;
    int32_t d = (int32_t)((v - (float)i) * 10.0f + 0.5f);
    if (d >= 10) { d -= 10; i += 1; }
    if (i > 9999) { i = 9999; d = 9; }

    buf[1] = (char)('0' + (i / 1000) % 10);
    buf[2] = (char)('0' + (i /  100) % 10);
    buf[3] = (char)('0' + (i /   10) % 10);
    buf[4] = (char)('0' + i % 10);
    buf[5] = '.';
    buf[6] = (char)('0' + d);
    buf[7] = '\0';
}

/**
 * Draw a string into a fixed-width field, padding with spaces on the right.
 * Clears leftover pixels from previous renders.
 */
static void draw_field(uint16_t x, uint16_t y, const char *str,
                        uint8_t field_chars, uint16_t fg, uint16_t bg)
{
    uint8_t drawn = 0;
    while (*str && drawn < field_chars) {
        LCD_DrawChar(x, y, *str++, fg, bg);
        x += FONT_W;
        drawn++;
    }
    /* Pad remainder with spaces (background colour) */
    while (drawn < field_chars) {
        LCD_DrawChar(x, y, ' ', fg, bg);
        x += FONT_W;
        drawn++;
    }
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void RobotDisplay_Init(void)
{
    LCD_Init();

    /* ---- Title bar ---- */
    LCD_FillRect(0, Y_TITLE_BG, LCD_WIDTH, Y_TITLE_H, LCD_NAVY);
    LCD_DrawString(X_TITLE_TXT, Y_TITLE_TXT,
                   "ROBOT ARM CONTROL", LCD_WHITE, LCD_NAVY);
    LCD_DrawString(X_MODE_BADGE, Y_TITLE_TXT,
                   "[IDL]", LCD_YELLOW, LCD_NAVY);

    /* ---- Background fill for data rows ---- */
    LCD_FillRect(0, Y_SEP1 + 1, LCD_WIDTH, Y_SEP4 - Y_SEP1 - 1, BG_DATA);

    /* ---- Static separator lines ---- */
    draw_sep(Y_SEP1);
    draw_sep(Y_SEP2);
    draw_sep(Y_SEP3);
    draw_sep(Y_SEP4);

    /* ---- Static row labels ---- */
    /* Joint rows – labels are part of the dynamic update */
    LCD_DrawString(0,   Y_JOINT1, "J1:-------  J4:-------", LCD_GREEN, BG_DATA);
    LCD_DrawString(0,   Y_JOINT2, "J2:-------  J5:-------", LCD_GREEN, BG_DATA);
    LCD_DrawString(0,   Y_JOINT3, "J3:-------  J6:-------", LCD_GREEN, BG_DATA);

    /* End-effector row */
    LCD_DrawString(0,   Y_EFF,    "X:-------  Y:-------  Z:-------",
                   LCD_CYAN, BG_DATA);

    /* Command / status labels */
    LCD_DrawString(0,   Y_CMD, "Cmd:---", LCD_WHITE, BG_DATA);
    LCD_DrawString(0,   Y_STS, "Sts:---", LCD_GREEN, BG_DATA);
}

void RobotDisplay_UpdateJoints(const float angles[6])
{
    char a[7][8];   /* a[0]=J1 … a[5]=J6, a[6] scratch */
    for (int j = 0; j < 6; j++) fmt_angle(a[j], angles[j]);

    /* Row 1: J1 + J4 */
    char row[41];
    /* "J1:+123.45  J4:-045.00" */
    row[0]='J'; row[1]='1'; row[2]=':';
    memcpy(&row[3],  a[0], 7);
    row[10]=' '; row[11]=' ';
    row[12]='J'; row[13]='4'; row[14]=':';
    memcpy(&row[15], a[3], 7);
    row[22]='\0';
    draw_field(0, Y_JOINT1, row, 22, LCD_GREEN, BG_DATA);

    /* Row 2: J2 + J5 */
    row[1]='2'; memcpy(&row[3],a[1],7); row[13]='5'; memcpy(&row[15],a[4],7);
    draw_field(0, Y_JOINT2, row, 22, LCD_GREEN, BG_DATA);

    /* Row 3: J3 + J6 */
    row[1]='3'; memcpy(&row[3],a[2],7); row[13]='6'; memcpy(&row[15],a[5],7);
    draw_field(0, Y_JOINT3, row, 22, LCD_GREEN, BG_DATA);
}

void RobotDisplay_UpdatePose(float x, float y, float z)
{
    char cx[8], cy[8], cz[8];
    fmt_coord(cx, x);
    fmt_coord(cy, y);
    fmt_coord(cz, z);

    /* "X:+0000.0  Y:-0184.5  Z:+0215.5" = 32 chars */
    char row[33];
    row[0]='X'; row[1]=':'; memcpy(&row[2],  cx, 7);
    row[9]=' '; row[10]=' ';
    row[11]='Y'; row[12]=':'; memcpy(&row[13], cy, 7);
    row[20]=' '; row[21]=' ';
    row[22]='Z'; row[23]=':'; memcpy(&row[24], cz, 7);
    row[31]='\0';
    draw_field(0, Y_EFF, row, 32, LCD_CYAN, BG_DATA);
}

void RobotDisplay_UpdateCmd(const char *cmd)
{
    /* Fixed-width "Cmd:" prefix + 35 chars of command */
    char row[41];
    row[0]='C'; row[1]='m'; row[2]='d'; row[3]=':';
    uint8_t i = 4;
    while (*cmd && i < 39) { row[i++] = *cmd++; }
    row[i] = '\0';
    draw_field(0, Y_CMD, row, 40, LCD_WHITE, BG_DATA);
}

void RobotDisplay_UpdateStatus(const char *text, uint8_t is_error)
{
    /* "Sts:" prefix + status text, max 36 chars */
    char row[41];
    row[0]='S'; row[1]='t'; row[2]='s'; row[3]=':';
    uint8_t i = 4;
    while (*text && i < 39) { row[i++] = (char)*text++; }
    row[i] = '\0';
    uint16_t fg = is_error ? LCD_RED : LCD_GREEN;
    draw_field(0, Y_STS, row, 40, fg, BG_DATA);
}

void RobotDisplay_SetMode(uint8_t mode)
{
    const char *badge;
    uint16_t    fg;
    switch (mode) {
        case RDISP_MODE_RUNNING: badge = "[RUN]"; fg = LCD_GREEN;  break;
        case RDISP_MODE_ERROR:   badge = "[ERR]"; fg = LCD_RED;    break;
        default:                 badge = "[IDL]"; fg = LCD_YELLOW; break;
    }
    LCD_DrawString(X_MODE_BADGE, Y_TITLE_TXT, badge, fg, LCD_NAVY);
}
