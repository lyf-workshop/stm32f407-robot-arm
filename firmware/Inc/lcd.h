/**
 * @file    lcd.h
 * @brief   ILI9341 2.8" LCD Driver (FSMC 16-bit parallel interface)
 *
 * Hardware mapping (per 8-1 pin table):
 *   CS   = PD7  (FSMC_NE1)
 *   WE   = PD5  (FSMC_NWE)
 *   RD   = PD4  (FSMC_NOE)
 *   RS   = PD11 (FSMC_A16, 0=cmd, 1=data)
 *   BL   = PA15 (GPIO output, high=on)
 *   D0-D15: PD14,PD15,PD0,PD1,PE7-PE15,PD8-PD10
 *
 * FSMC address:
 *   CMD  base: 0x60000000  (FSMC_A16 = 0)
 *   DATA base: 0x60020000  (FSMC_A16 = 1, byte-addr bit17 = A16 in 16-bit mode)
 */

#ifndef __LCD_H
#define __LCD_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* --------------------------------------------------------------------------
 * Screen dimensions (landscape mode, MADCTL = 0x28)
 * -------------------------------------------------------------------------- */
#define LCD_WIDTH    320
#define LCD_HEIGHT   240

/* --------------------------------------------------------------------------
 * RGB565 colour constants
 * -------------------------------------------------------------------------- */
#define LCD_BLACK      0x0000u
#define LCD_WHITE      0xFFFFu
#define LCD_RED        0xF800u
#define LCD_GREEN      0x07E0u
#define LCD_BLUE       0x001Fu
#define LCD_YELLOW     0xFFE0u
#define LCD_CYAN       0x07FFu
#define LCD_MAGENTA    0xF81Fu
#define LCD_ORANGE     0xFD20u
#define LCD_GRAY       0x8410u
#define LCD_DARKGRAY   0x4208u
#define LCD_LIGHTGRAY  0xC618u
#define LCD_NAVY       0x000Fu
#define LCD_DARKGREEN  0x03E0u
#define LCD_MAROON     0x7800u
#define LCD_TEAL       0x0410u

/* Pack 8-bit R, G, B values into RGB565 */
#define RGB565(r, g, b) \
    ((uint16_t)(((uint16_t)((r) & 0xF8u) << 8) | \
                ((uint16_t)((g) & 0xFCu) << 3) | \
                ((uint16_t)((b) & 0xFFu) >> 3)))

/* --------------------------------------------------------------------------
 * FSMC memory-mapped access macros
 * Bank1 NE1 base = 0x60000000; RS via FSMC_A16 (byte-addr offset 1<<17)
 * -------------------------------------------------------------------------- */
#define LCD_CMD_ADDR   (*(__IO uint16_t *)((uint32_t)0x60000000UL))
#define LCD_DATA_ADDR  (*(__IO uint16_t *)((uint32_t)0x60020000UL))

/* Backlight pin */
#define LCD_BL_PORT    GPIOA
#define LCD_BL_PIN     GPIO_PIN_15

/* --------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------- */
void LCD_Init(void);
void LCD_BacklightOn(void);
void LCD_BacklightOff(void);

void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void LCD_Clear(uint16_t color);
void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void LCD_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void LCD_DrawChar(uint16_t x, uint16_t y, char ch, uint16_t fg, uint16_t bg);
void LCD_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg);

#endif /* __LCD_H */
