/**
 * @file    lcd_font.h
 * @brief   8x16 ASCII bitmap font (characters 0x20 - 0x7E)
 *
 * Each character occupies 16 bytes, one byte per scan-line (row 0 = top).
 * Bit 7 of each byte is the leftmost pixel.  Active-high (1 = foreground).
 */

#ifndef __LCD_FONT_H
#define __LCD_FONT_H

#include <stdint.h>

/* Font dimensions */
#define FONT_W   8    /* glyph width  in pixels */
#define FONT_H  16    /* glyph height in pixels */

/**
 * LCD_Font8x16[c - 0x20][row] gives the bitmap row for printable ASCII c.
 * Valid indices: c in [0x20, 0x7E]  (space through tilde, 95 glyphs).
 */
extern const uint8_t LCD_Font8x16[95][16];

#endif /* __LCD_FONT_H */
