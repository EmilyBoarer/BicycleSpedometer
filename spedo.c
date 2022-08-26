#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"

#define REED_GPIO 22
#define SEG_FIRST_GPIO 8

#define WHEEL_CIRCUMFERENCE 2.23053078

#define VELOCITY_CONSTANT (WHEEL_CIRCUMFERENCE*60*60)

// define characters for each segment
int bits_L[10] = {
    0b00011101110000,
    0b00010000010000,
    0b00001110110000,
    0b00011010110000,
    0b00010011010000,
    0b00011011100000,
    0b00011111100000,
    0b00010000110000,
    0b00011111110000,
    0b00010011110000,
};
int bits_R[10] = {
    0b11100000000111,
    0b10000000000001,
    0b01100000001011,
    0b11000000001011,
    0b10000000001101,
    0b11000000001110,
    0b11100000001110,
    0b10000000000011,
    0b11100000001111,
    0b10000000001111,
};

const uint LED_PIN = 25;

int main() {
    bi_decl(bi_program_description("This is a test binary."));
    bi_decl(bi_1pin_with_name(LED_PIN, "On-board LED"));

    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    // init reed switch gpio as a button
    gpio_init(REED_GPIO);
    gpio_set_dir(REED_GPIO, GPIO_IN);
    gpio_pull_up(REED_GPIO);

    // init 7 seg gpios
    for (int gpio = SEG_FIRST_GPIO; gpio < SEG_FIRST_GPIO + 14; gpio++) {
        gpio_init(gpio);
        gpio_set_dir(gpio, GPIO_OUT);
    }

    // counter (temp)
    int i = 0;
    float m = 0;
    char buf[3]; // just the 2 digit rev counter and end of string
    int32_t mask;

    while (1) {
        if (!gpio_get(REED_GPIO)) {
            m += WHEEL_CIRCUMFERENCE;
            i++;
            if (i >= 100) {
                i = 0;
            }
            if (m >= 100) {
                m = 0;
            }

            gpio_put(LED_PIN, 1);
            
            // remove previous display
            gpio_clr_mask(mask);
            // output to 7 seg
            mask = (bits_L[((int)m)/10] | bits_R[((int)m)%10]) << SEG_FIRST_GPIO;
            gpio_set_mask(mask);

            sleep_ms(100); // TODO work out reasonable cooldown based on likely max speed
            gpio_put(LED_PIN, 0);

        }
    }
}