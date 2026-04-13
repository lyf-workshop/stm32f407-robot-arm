/**
 * @file    lcd.c
 * @brief   ILI9341 LCD driver via FSMC 16-bit parallel interface.
 *
 * FSMC configuration (direct register access, no HAL_SRAM required):
 *   Bank1 NE1 (PD7), SRAM mode, 16-bit data bus
 *   BCR1 = 0x00001011  (MBKEN | MWID=16bit | WREN)
 *   BTR1 = 0x00000901  (ADDSET=1, DATAST=9)
 *   Write cycle = (1+1+9+1)*5.95ns = 71.4ns ≥ 66ns (ILI9341 tWC min)
 *
 * ILI9341 orientation: MADCTL=0x28 → landscape 320x240, BGR colour order.
 */

#include "lcd.h"
#include "lcd_font.h"

/* -------------------------------------------------------------------------
 * Low-level inline helpers
 * ------------------------------------------------------------------------- */
static inline void LCD_WrCmd(uint16_t cmd)  { LCD_CMD_ADDR  = cmd;  }
static inline void LCD_WrDat(uint16_t dat)  { LCD_DATA_ADDR = dat;  }
static inline void LCD_WrDat8(uint8_t  dat) { LCD_DATA_ADDR = (uint16_t)dat; }

/* -------------------------------------------------------------------------
 * GPIO + FSMC hardware initialisation
 * ------------------------------------------------------------------------- */
static void LCD_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* Enable port clocks (GPIOA already enabled by USART init, but safe) */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* ---- Port D: FSMC data + control signals (AF12) ----
     * PD0 =D2, PD1=D3, PD4=NOE, PD5=NWE, PD7=NE1,
     * PD8=D13, PD9=D14, PD10=D15, PD11=A16(RS), PD14=D0, PD15=D1
     */
    gpio.Pin  = GPIO_PIN_0  | GPIO_PIN_1  | GPIO_PIN_4  | GPIO_PIN_5  |
                GPIO_PIN_7  | GPIO_PIN_8  | GPIO_PIN_9  | GPIO_PIN_10 |
                GPIO_PIN_11 | GPIO_PIN_14 | GPIO_PIN_15;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF12_FSMC;
    HAL_GPIO_Init(GPIOD, &gpio);

    /* ---- Port E: FSMC data bus D4-D12 (AF12) ----
     * PE7=D4, PE8=D5, PE9=D6, PE10=D7,
     * PE11=D8, PE12=D9, PE13=D10, PE14=D11, PE15=D12
     */
    gpio.Pin  = GPIO_PIN_7  | GPIO_PIN_8  | GPIO_PIN_9  | GPIO_PIN_10 |
                GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |
                GPIO_PIN_15;
    gpio.Alternate = GPIO_AF12_FSMC;
    HAL_GPIO_Init(GPIOE, &gpio);

    /* ---- PA15: Backlight control, GPIO push-pull output ----
     * PA15 defaults to JTDI; safe to use as GPIO in SWD-only debug mode.
     */
    gpio.Pin       = LCD_BL_PIN;
    gpio.Mode      = GPIO_MODE_OUTPUT_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = 0;
    HAL_GPIO_Init(LCD_BL_PORT, &gpio);
}

static void LCD_FSMC_Init(void)
{
    __HAL_RCC_FSMC_CLK_ENABLE();

    /*
     * BCR1: Bank1 Control Register
     *   MBKEN  (bit 0)  = 1  – enable bank
     *   MWID   (bit 4)  = 1  – 16-bit data bus  (bits [5:4] = 01)
     *   WREN   (bit 12) = 1  – write enable
     * All other fields 0: SRAM type, non-multiplexed, no burst/wait.
     */
    FSMC_Bank1->BTCR[0] = 0x00001011UL;

    /*
     * BTR1: Bank1 Read (and write, since EXTMOD=0) Timing Register
     *   ADDSET  [3:0]  = 1  → 1 HCLK address-setup cycle   (~6 ns)
     *   DATAST  [15:8] = 9  → 9 HCLK data-setup cycles     (~54 ns)
     *   Write cycle total = (1+1+9+1)*5.95 = 71.4 ns ≥ 66 ns (ILI9341)
     */
    FSMC_Bank1->BTCR[1] = 0x00000901UL;
}

/* -------------------------------------------------------------------------
 * ILI9341 initialisation sequence
 * Each entry: {command, {data bytes…}, data_count, post_delay_ms}
 * ------------------------------------------------------------------------- */
typedef struct { uint8_t cmd; uint8_t d[16]; uint8_t n; uint8_t dly; } LcdCmd_t;

static const LcdCmd_t s_ili_seq[] = {
    /* Power and timing */
    {0xCB, {0x39,0x2C,0x00,0x34,0x02}, 5, 0},   /* Power Control A          */
    {0xCF, {0x00,0xC1,0x30},           3, 0},   /* Power Control B          */
    {0xE8, {0x85,0x00,0x78},           3, 0},   /* Driver Timing Ctrl A     */
    {0xEA, {0x00,0x00},                2, 0},   /* Driver Timing Ctrl B     */
    {0xED, {0x64,0x03,0x12,0x81},      4, 0},   /* Power-on Sequence Ctrl   */
    {0xF7, {0x20},                     1, 0},   /* Pump Ratio Control       */
    /* VCOM / Power */
    {0xC0, {0x23},                     1, 0},   /* VRH = 4.60 V             */
    {0xC1, {0x10},                     1, 0},   /* Power Ctrl 2             */
    {0xC5, {0x3E,0x28},                2, 0},   /* VCOM Ctrl 1              */
    {0xC7, {0x86},                     1, 0},   /* VCOM Ctrl 2              */
    /* Display geometry and format */
    {0x36, {0x28},                     1, 0},   /* MADCTL: landscape, BGR   */
    {0x3A, {0x55},                     1, 0},   /* Pixel Format: 16 bpp     */
    {0xB1, {0x00,0x18},                2, 0},   /* Frame Rate: ~79 Hz       */
    {0xB6, {0x08,0x82,0x27},           3, 0},   /* Display Function Control */
    {0xF2, {0x00},                     1, 0},   /* Gamma Function Disable   */
    {0x26, {0x01},                     1, 0},   /* Gamma Curve 1 selected   */
    /* Positive gamma */
    {0xE0, {0x0F,0x31,0x2B,0x0C,0x0E,0x08,
            0x4E,0xF1,0x37,0x07,0x10,0x03,
            0x0E,0x09,0x00},          15, 0},
    /* Negative gamma */
    {0xE1, {0x00,0x0E,0x14,0x03,0x11,0x07,
            0x31,0xC1,0x48,0x08,0x0F,0x0C,
            0x31,0x36,0x0F},          15, 0},
    /* Wake up */
    {0x11, {0}, 0, 120},                         /* Sleep Out (wait 120 ms)  */
    {0x29, {0}, 0,  20},                         /* Display ON               */
};

/* -------------------------------------------------------------------------
 * Public functions
 * ------------------------------------------------------------------------- */

void LCD_Init(void)
{
    LCD_GPIO_Init();
    LCD_FSMC_Init();

    LCD_BacklightOff();
    HAL_Delay(50);

    for (uint32_t i = 0; i < sizeof(s_ili_seq)/sizeof(s_ili_seq[0]); i++) {
        LCD_WrCmd(s_ili_seq[i].cmd);
        for (uint8_t j = 0; j < s_ili_seq[i].n; j++) {
            LCD_WrDat8(s_ili_seq[i].d[j]);
        }
        if (s_ili_seq[i].dly) {
            HAL_Delay(s_ili_seq[i].dly);
        }
    }

    LCD_Clear(LCD_BLACK);
    LCD_BacklightOn();
}

void LCD_BacklightOn(void)  { HAL_GPIO_WritePin(LCD_BL_PORT, LCD_BL_PIN, GPIO_PIN_SET);   }
void LCD_BacklightOff(void) { HAL_GPIO_WritePin(LCD_BL_PORT, LCD_BL_PIN, GPIO_PIN_RESET); }

/* Set pixel-write window (Column Address Set + Page Address Set) */
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    LCD_WrCmd(0x2A);                        /* Column Address Set */
    LCD_WrDat8((uint8_t)(x0 >> 8));
    LCD_WrDat8((uint8_t)(x0 & 0xFF));
    LCD_WrDat8((uint8_t)(x1 >> 8));
    LCD_WrDat8((uint8_t)(x1 & 0xFF));

    LCD_WrCmd(0x2B);                        /* Page Address Set   */
    LCD_WrDat8((uint8_t)(y0 >> 8));
    LCD_WrDat8((uint8_t)(y0 & 0xFF));
    LCD_WrDat8((uint8_t)(y1 >> 8));
    LCD_WrDat8((uint8_t)(y1 & 0xFF));

    LCD_WrCmd(0x2C);                        /* Memory Write       */
}

void LCD_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    LCD_SetWindow(x, y, x, y);
    LCD_WrDat(color);
}

void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    if (x + w > LCD_WIDTH)  w = LCD_WIDTH  - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;

    LCD_SetWindow(x, y, x + w - 1, y + h - 1);
    uint32_t n = (uint32_t)w * h;
    while (n--) { LCD_WrDat(color); }
}

void LCD_Clear(uint16_t color)
{
    LCD_FillRect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

/* Bresenham line algorithm */
void LCD_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    int16_t dx = (int16_t)x1 - (int16_t)x0;
    int16_t dy = (int16_t)y1 - (int16_t)y0;
    int16_t sx = (dx >= 0) ? 1 : -1;
    int16_t sy = (dy >= 0) ? 1 : -1;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;

    /* Optimised horizontal line */
    if (dy == 0) {
        uint16_t lx = (x0 < x1) ? x0 : x1;
        LCD_FillRect(lx, y0, (uint16_t)(dx + 1), 1, color);
        return;
    }

    int16_t err = dx - dy;
    for (;;) {
        LCD_DrawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = err * 2;
        if (e2 > -dy) { err -= dy; x0 = (uint16_t)(x0 + sx); }
        if (e2 <  dx) { err += dx; y0 = (uint16_t)(y0 + sy); }
    }
}

/* Draw one 8×16 character at (x,y); fg = text colour, bg = background */
void LCD_DrawChar(uint16_t x, uint16_t y, char ch, uint16_t fg, uint16_t bg)
{
    if (x + FONT_W > LCD_WIDTH || y + FONT_H > LCD_HEIGHT) return;
    if ((uint8_t)ch < 0x20u || (uint8_t)ch > 0x7Eu) ch = ' ';

    const uint8_t *p = LCD_Font8x16[(uint8_t)ch - 0x20u];
    LCD_SetWindow(x, y, x + FONT_W - 1, y + FONT_H - 1);

    for (uint8_t row = 0; row < FONT_H; row++) {
        uint8_t b = p[row];
        for (int8_t col = 7; col >= 0; col--) {
            LCD_WrDat((b >> col) & 1u ? fg : bg);
        }
    }
}

/* Draw a null-terminated ASCII string; wraps at screen right edge */
void LCD_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg)
{
    while (*str) {
        if (x + FONT_W > LCD_WIDTH) break;
        LCD_DrawChar(x, y, *str++, fg, bg);
        x += FONT_W;
    }
}
