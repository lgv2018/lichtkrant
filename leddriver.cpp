#include <string.h>

#include <Arduino.h>

#include "framebuffer.h"
#include "leddriver.h"

#define PIN_MUX_2   D1          // J1.15
#define PIN_MUX_1   D2          // J1.13
#define PIN_MUX_0   D3          // J1.11
#define PIN_ENABLE  D4          // J1.9
#define PIN_DATA_R  D5          // J1.7
#define PIN_DATA_G  D6          // J1.5
#define PIN_LATCH   D7          // J1.3
#define PIN_SHIFT   D8          // J1.1

typedef struct {
    uint8_t r;
    uint8_t g;
} led_pixel_t;

static const vsync_fn_t *vsync_fn;
static pixel_t framebuffer[LED_HEIGHT][LED_WIDTH];
static led_pixel_t pwmstate[LED_HEIGHT][LED_WIDTH];
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
        led_pixel_t *pwmrow = pwmstate[row];
        pixel_t *fb_row = framebuffer[row];
        for (int col = 0; col < LED_WIDTH; col++) {
            // dither
            led_pixel_t c1 = pwmrow[col];
            pixel_t c2 = fb_row[col];
            int r = c1.r + c2.r;
            int g = c1.g + c2.g;

            digitalWrite(PIN_SHIFT, 0);
            digitalWrite(PIN_DATA_R, r < 256);
            digitalWrite(PIN_DATA_G, g < 256);

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

void led_init(const vsync_fn_t * vsync)
{
    // copy
    vsync_fn = vsync;

    // set pins
    pinMode(PIN_ENABLE, OUTPUT);
    digitalWrite(PIN_ENABLE, 0);
    pinMode(PIN_LATCH, OUTPUT);
    pinMode(PIN_SHIFT, OUTPUT);
    pinMode(PIN_DATA_R, OUTPUT);
    pinMode(PIN_DATA_G, OUTPUT);
    pinMode(PIN_MUX_0, OUTPUT);
    pinMode(PIN_MUX_1, OUTPUT);
    pinMode(PIN_MUX_2, OUTPUT);

    // clear the frame buffer and initialise pwm state
    memset(framebuffer, 0, sizeof(framebuffer));
    for (int row = 0; row < LED_HEIGHT; row++) {
        for (int col = 0; col < LED_WIDTH; col++) {
            pwmstate[row][col].r = random(256);
            pwmstate[row][col].g = random(256);
        }
    }
    row = 0;

    // initialise timer
    timer1_isr_init();
}

void led_enable(void)
{
    // set up timer interrupt
    timer1_disable();
    timer1_write(1250); // fps = 625000/number
    timer1_attachInterrupt(led_tick);
    timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP);

    // enable pin
    digitalWrite(PIN_ENABLE, 1);
}

void led_disable(void)
{
    // enable pin
    digitalWrite(PIN_ENABLE, 0);

    // detach the interrupt routine
    timer1_detachInterrupt();
    timer1_disable();

    // flush shift register
    row = 0;
    memset(framebuffer, 0, sizeof(framebuffer));
    led_tick();
}


