/**
 * @file    touch.c
 * @brief   XPT2046 Resistive Touch Controller Driver (software SPI)
 */

#include "touch.h"
#include "lcd.h"
#include <string.h>

/* --------------------------------------------------------------------------
 * Pin definitions (software SPI), confirmed from official bsp_XPT2046.h:
 *   CS   = PD13  (Chip Select, active low)
 *   CLK  = PE0   (SPI clock)
 *   MOSI = PE2   (Master Out Slave In)
 *   MISO = PE3   (Master In Slave Out)
 *   IRQ  = PE4   (PENIRQ#, active low when touched)
 * -------------------------------------------------------------------------- */
#define TOUCH_CS_PORT    GPIOD
#define TOUCH_CS_PIN     GPIO_PIN_13

#define TOUCH_CLK_PORT   GPIOE
#define TOUCH_CLK_PIN    GPIO_PIN_0

#define TOUCH_MOSI_PORT  GPIOE
#define TOUCH_MOSI_PIN   GPIO_PIN_2

#define TOUCH_MISO_PORT  GPIOE
#define TOUCH_MISO_PIN   GPIO_PIN_3

#define TOUCH_IRQ_PORT   GPIOE
#define TOUCH_IRQ_PIN    GPIO_PIN_4

#define TOUCH_CS_LOW()   HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, GPIO_PIN_RESET)
#define TOUCH_CS_HIGH()  HAL_GPIO_WritePin(TOUCH_CS_PORT, TOUCH_CS_PIN, GPIO_PIN_SET)

#define TOUCH_CLK_LOW()  HAL_GPIO_WritePin(TOUCH_CLK_PORT, TOUCH_CLK_PIN, GPIO_PIN_RESET)
#define TOUCH_CLK_HIGH() HAL_GPIO_WritePin(TOUCH_CLK_PORT, TOUCH_CLK_PIN, GPIO_PIN_SET)

#define TOUCH_MOSI_LOW() HAL_GPIO_WritePin(TOUCH_MOSI_PORT, TOUCH_MOSI_PIN, GPIO_PIN_RESET)
#define TOUCH_MOSI_HIGH() HAL_GPIO_WritePin(TOUCH_MOSI_PORT, TOUCH_MOSI_PIN, GPIO_PIN_SET)

#define TOUCH_MISO_READ() HAL_GPIO_ReadPin(TOUCH_MISO_PORT, TOUCH_MISO_PIN)

/* XPT2046 control byte commands */
#define XPT2046_CMD_X    0xD0  /* Measure X position (12-bit) */
#define XPT2046_CMD_Y    0x90  /* Measure Y position (12-bit) */

/* Calibration storage */
static Touch_Calib_t g_touch_cal = {0};

/* Default calibration for typical 2.8" XPT2046+ILI9341 (landscape 320x240) */
static const int32_t s_default_calib[6] = {
    /* These are empirical values, user may need to run calibration */
    /* Transform: LCD_x = (a[0]*raw_x + a[1]*raw_y + a[2]) / 65536 */
    /*            LCD_y = (a[3]*raw_x + a[4]*raw_y + a[5]) / 65536 */
    /* Typical XPT2046: raw X ~200-3900, raw Y ~200-3900 */
    5734,   /* a[0] = 320 / (3900-200) * 65536 ≈ 5734 */
    0,      /* a[1] = 0 (no skew) */
    -1146880,  /* a[2] = -200 * a[0] ≈ -1146880 */
    0,      /* a[3] = 0 (no skew) */
    3502,   /* a[4] = 240 / (3900-200) * 65536 ≈ 4267, inverted = -4267, but Y is typically inverted */
    -700960   /* a[5] = -200 * a[4] */
};

/* --------------------------------------------------------------------------
 * Low-level software SPI
 * -------------------------------------------------------------------------- */
static void spi_delay(void)
{
    /* ~1us delay for ~500kHz SPI clock */
    for (volatile int i = 0; i < 10; i++);
}

static uint8_t spi_transfer(uint8_t data)
{
    uint8_t rx = 0;
    
    for (int i = 0; i < 8; i++) {
        /* Write MOSI bit (MSB first) */
        if (data & 0x80) {
            TOUCH_MOSI_HIGH();
        } else {
            TOUCH_MOSI_LOW();
        }
        data <<= 1;
        
        spi_delay();
        TOUCH_CLK_HIGH();
        
        /* Read MISO bit */
        rx <<= 1;
        if (TOUCH_MISO_READ() == GPIO_PIN_SET) {
            rx |= 0x01;
        }
        
        spi_delay();
        TOUCH_CLK_LOW();
    }
    
    return rx;
}

/* --------------------------------------------------------------------------
 * XPT2046 read raw ADC (12-bit)
 *
 * XPT2046 MISO output format after command byte (16 bits total):
 *   Bit 15: null (0)
 *   Bits 14..3: D11..D0 (12-bit result, MSB first)
 *   Bits 2..0: null (0)
 *
 * Combined 16-bit: [null][D11..D0][0][0][0]
 * Correct 12-bit value: ((msb << 8) | lsb) >> 3
 * -------------------------------------------------------------------------- */
static uint16_t xpt2046_read_adc(uint8_t cmd)
{
    uint16_t val = 0;

    TOUCH_CS_LOW();
    spi_delay();

    /* Send command byte */
    spi_transfer(cmd);

    /* Wait for XPT2046 ADC conversion (~2.5us max) */
    spi_delay();
    spi_delay();
    spi_delay();

    /* Read 16-bit result: [null][D11..D0][0][0][0] */
    uint8_t msb = spi_transfer(0x00);
    uint8_t lsb = spi_transfer(0x00);

    /* Correct 12-bit extraction: discard null bit (bit15) and 3 trailing zeros */
    val = (((uint16_t)msb << 8) | (uint16_t)lsb) >> 3;
    if (val > 4095) val = 4095;  /* clamp to 12-bit */

    spi_delay();
    TOUCH_CS_HIGH();

    return val;
}

/* --------------------------------------------------------------------------
 * Median filter (3 samples)
 * -------------------------------------------------------------------------- */
static uint16_t median_filter(uint16_t a, uint16_t b, uint16_t c)
{
    if (a > b) {
        if (b > c) return b;        /* a > b > c */
        else if (a > c) return c;   /* a > c >= b */
        else return a;              /* c >= a > b */
    } else {
        if (a > c) return a;        /* b >= a > c */
        else if (b > c) return c;   /* b >= c >= a */
        else return b;              /* c > b >= a */
    }
}

/* --------------------------------------------------------------------------
 * Apply calibration transform
 * -------------------------------------------------------------------------- */
static void apply_calibration(uint16_t raw_x, uint16_t raw_y, uint16_t *lcd_x, uint16_t *lcd_y)
{
    if (!g_touch_cal.valid) {
        /* No calibration, use raw values scaled linearly */
        *lcd_x = (uint16_t)((raw_x * LCD_WIDTH) / 4096);
        *lcd_y = (uint16_t)((raw_y * LCD_HEIGHT) / 4096);
        return;
    }
    
    /* Apply affine transform */
    int32_t x = (g_touch_cal.a[0] * (int32_t)raw_x + 
                 g_touch_cal.a[1] * (int32_t)raw_y + 
                 g_touch_cal.a[2]) / 65536;
    int32_t y = (g_touch_cal.a[3] * (int32_t)raw_x + 
                 g_touch_cal.a[4] * (int32_t)raw_y + 
                 g_touch_cal.a[5]) / 65536;
    
    /* Clamp to LCD bounds */
    if (x < 0) x = 0;
    if (x >= LCD_WIDTH) x = LCD_WIDTH - 1;
    if (y < 0) y = 0;
    if (y >= LCD_HEIGHT) y = LCD_HEIGHT - 1;
    
    *lcd_x = (uint16_t)x;
    *lcd_y = (uint16_t)y;
}

/* --------------------------------------------------------------------------
 * Public API implementation
 * -------------------------------------------------------------------------- */

void Touch_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIO clocks */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    /* Configure CS (PD13), CLK (PE0), MOSI (PE2) as push-pull output */
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

    GPIO_InitStruct.Pin = TOUCH_CS_PIN;
    HAL_GPIO_Init(TOUCH_CS_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = TOUCH_CLK_PIN;
    HAL_GPIO_Init(TOUCH_CLK_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = TOUCH_MOSI_PIN;
    HAL_GPIO_Init(TOUCH_MOSI_PORT, &GPIO_InitStruct);

    /* Configure MISO (PE3) as input with pull-up */
    GPIO_InitStruct.Pin  = TOUCH_MISO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(TOUCH_MISO_PORT, &GPIO_InitStruct);

    /* Configure IRQ (PE4) as input with pull-up (PENIRQ# active low) */
    GPIO_InitStruct.Pin  = TOUCH_IRQ_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(TOUCH_IRQ_PORT, &GPIO_InitStruct);

    /* Idle state: CS high, CLK low */
    TOUCH_CS_HIGH();
    TOUCH_CLK_LOW();

    /* Load calibration from FLASH or use defaults */
    Touch_LoadCalibration();
}

bool Touch_IsPressed(void)
{
    /* Use PENIRQ# pin (PE4) for reliable touch detection.
     * XPT2046 pulls PE4 LOW when the screen is being touched. */
    return (HAL_GPIO_ReadPin(TOUCH_IRQ_PORT, TOUCH_IRQ_PIN) == GPIO_PIN_RESET);
}

bool Touch_Read(Touch_State_t *state)
{
    if (!state) return false;

    /* Use PENIRQ# (PE4) for reliable touch detection */
    if (!Touch_IsPressed()) {
        state->pressed = false;
        state->x = 0;
        state->y = 0;
        state->raw_x = 0;
        state->raw_y = 0;
        return false;
    }

    /* Read X and Y three times for median filtering */
    uint16_t x1 = xpt2046_read_adc(XPT2046_CMD_X);
    uint16_t y1 = xpt2046_read_adc(XPT2046_CMD_Y);

    uint16_t x2 = xpt2046_read_adc(XPT2046_CMD_X);
    uint16_t y2 = xpt2046_read_adc(XPT2046_CMD_Y);

    uint16_t x3 = xpt2046_read_adc(XPT2046_CMD_X);
    uint16_t y3 = xpt2046_read_adc(XPT2046_CMD_Y);

    /* Median filter to reduce noise */
    uint16_t raw_x = median_filter(x1, x2, x3);
    uint16_t raw_y = median_filter(y1, y2, y3);

    /* Double-check IRQ is still low (reject if released mid-read) */
    if (!Touch_IsPressed()) {
        state->pressed = false;
        state->x = 0; state->y = 0;
        state->raw_x = 0; state->raw_y = 0;
        return false;
    }
    
    /* Apply calibration */
    uint16_t lcd_x, lcd_y;
    apply_calibration(raw_x, raw_y, &lcd_x, &lcd_y);
    
    state->pressed = true;
    state->x = lcd_x;
    state->y = lcd_y;
    state->raw_x = raw_x;
    state->raw_y = raw_y;
    
    return true;
}

void Touch_LoadCalibration(void)
{
    /* For now, use default calibration */
    /* TODO: Load from FLASH sector if needed */
    memcpy(g_touch_cal.a, s_default_calib, sizeof(g_touch_cal.a));
    g_touch_cal.valid = true;
}

void Touch_SaveCalibration(void)
{
    /* TODO: Save to FLASH if needed */
    /* For bare-metal, we skip FLASH programming for simplicity */
}

void Touch_SetCalibration(const int32_t a[6])
{
    memcpy(g_touch_cal.a, a, sizeof(g_touch_cal.a));
    g_touch_cal.valid = true;
}

void Touch_GetDefaultCalibration(int32_t a[6])
{
    memcpy(a, s_default_calib, sizeof(s_default_calib));
}

void Touch_ReadRaw(uint16_t *raw_x, uint16_t *raw_y)
{
    if (!raw_x || !raw_y) return;
    /* Read single sample without filtering for diagnosis */
    *raw_x = xpt2046_read_adc(XPT2046_CMD_X);
    *raw_y = xpt2046_read_adc(XPT2046_CMD_Y);
}
