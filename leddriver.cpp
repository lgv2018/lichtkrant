#include <string.h>

#include <Arduino.h>
#include <Ticker.h>

#include "leddriver.h"


#define PIN_MUX_2   D1          // J1.15
#define PIN_MUX_1   D2          // J1.13
#define PIN_MUX_0   D3          // J1.11
#define PIN_ENABLE  D4          // J1.9
#define PIN_DATA_R  D5          // J1.7
#define PIN_DATA_G  D6          // J1.5
#define PIN_LATCH   D7          // J1.3
#define PIN_SHIFT   D8          // J1.1

static Ticker ticker;
static const vsync_fn_t *vsync_fn;
static pixel_t framebuffer[LED_NUM_ROWS][LED_NUM_COLS];
static pixel_t pwmstate[LED_NUM_ROWS][LED_NUM_COLS];
static int row = 0;
static int frame = 0;

static void ICACHE_RAM_ATTR led_tick(void)
{
    // latch the data
    digitalWrite(PIN_LATCH, 1);

    // set the row multiplexer
    digitalWrite(PIN_MUX_0, row & 1);
    digitalWrite(PIN_MUX_1, row & 2);
    digitalWrite(PIN_MUX_2, row & 4);

    // prepare to latch the columns
    digitalWrite(PIN_LATCH, 0);

    // write column data
    row = (row + 1) & 7;
    if (row == 7) {
        vsync_fn(frame++);
    } else {
        // write the column shift registers
        pixel_t *pwmrow = pwmstate[row];
        pixel_t *fb_row = framebuffer[row];
        for (int col = 0; col < LED_NUM_COLS; col++) {
            // dither
            pixel_t c1 = pwmrow[col];
            pixel_t c2 = fb_row[col];
            int r = c1.r + c2.r;
            int g = c1.g + c2.g;

            digitalWrite(PIN_SHIFT, 0);
#if 1
            digitalWrite(PIN_DATA_R, r < 256);
            digitalWrite(PIN_DATA_G, g < 256);
#else
            digitalWrite(PIN_DATA_R, c2.r < 128);
            digitalWrite(PIN_DATA_G, c2.g < 128);
#endif

            // write back
            pwmrow[col].r = r;
            pwmrow[col].g = g;

            // shift
            digitalWrite(PIN_SHIFT, 1);
        }
    }
}

void led_write_framebuffer(const void *data)
{
    memcpy(framebuffer, data, sizeof(framebuffer));
}

static uint8_t reverse(uint8_t b)
{
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

void led_init(const vsync_fn_t * vsync)
{
    // copy
    vsync_fn = vsync;

    pinMode(PIN_ENABLE, OUTPUT);
    digitalWrite(PIN_ENABLE, 0);
    pinMode(PIN_LATCH, OUTPUT);
    pinMode(PIN_SHIFT, OUTPUT);
    pinMode(PIN_DATA_R, OUTPUT);
    pinMode(PIN_DATA_G, OUTPUT);
    pinMode(PIN_MUX_0, OUTPUT);
    pinMode(PIN_MUX_1, OUTPUT);
    pinMode(PIN_MUX_2, OUTPUT);

    // clear the frame buffer
    memset(framebuffer, 0, sizeof(framebuffer));

    // initialise the pwm state with random values
    for (int row = 0; row < LED_NUM_ROWS; row++) {
        for (int col = 0; col < LED_NUM_COLS; col++) {
#if 0   // ordered dither
            pwmstate[row][col].r = reverse((row + col) << 3);
            pwmstate[row][col].g = reverse((row - col) << 3);
#else   // random dither
            pwmstate[row][col].r = random(256);
            pwmstate[row][col].g = random(256);
#endif
        }
    }

    row = 0;

}

void led_enable(void)
{
    // install the interrupt routine
    ticker.attach_ms(1, led_tick);

    // enable pin
    digitalWrite(PIN_ENABLE, 1);
}

void led_disable(void)
{
    // detach the interrupt routine
    ticker.detach();

    // enable pin
    digitalWrite(PIN_ENABLE, 0);
}


