#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define REED_GPIO 22
#define SEG_FIRST_GPIO 8

#define WHEEL_CIRCUMFERENCE 2.231

#define VELOCITY_CONSTANT (WHEEL_CIRCUMFERENCE*60*60)

#define ANIMATION_WELCOME_BACK_DELAY 10

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

    // init vars for the loop
    int32_t mask;
    int t = 0;
    int state = 0;

    while (1) {
        // increment the timer
        sleep_ms(1); // TODO account for code execution time!
        t++;
        if (state == 0) { // reed-open state
            if (!gpio_get(REED_GPIO)){ // reed closed
                state = 1; 
                // this is kinda now a state 0.5 (only run when entering state 1 from 0)

                // set the speed based on params
                int v = VELOCITY_CONSTANT / t; // velocity in km/h
            
                // remove previous display, set new mask, and display
                gpio_clr_mask(mask);
                if (v >= 10) {
                    mask = (bits_L[v/10] | bits_R[v%10]) << SEG_FIRST_GPIO;
                } else {
                    mask = bits_R[v%10] << SEG_FIRST_GPIO;
                }
                gpio_set_mask(mask);

                // reset time
                t = 0;
            } 
            // else stay in current state
            if (t > 5000) { // effectively stationary - turn display to dashes
                // remove previous display, set new mask, and display
                gpio_clr_mask(mask);
                mask = 0b10001000 << SEG_FIRST_GPIO;
                gpio_set_mask(mask);
            }
            if (t > 10000) { // effectively stationary - turn display off
                // remove previous display, set new mask, and display
                gpio_clr_mask(mask);
                mask = 0;
                gpio_set_mask(mask);
                state = 3;
            }
        }
        if (state == 1) { // in reed-closed state
            gpio_put(LED_PIN, 1);
            sleep_ms(50);
            t += 50;
            gpio_put(LED_PIN, 0);
            state = 2;
        }
        if (state == 2) { // trying to leave reed-closed state
            if (!gpio_get(REED_GPIO)){ // reed closed
                state = 1;
            } else { // reed open
                state = 0;
            }
        }
        if (state == 3) { // stationary reed-open state
            if (!gpio_get(REED_GPIO)){ // reed closed
                state = 1; 
                // show welcome back message
            
                // show animation!
                int segs[8] = { 1,2,32,64,128,256,2048,4096 }; // circular animation
                for (int x = 0; x < 8; x++) {
                    gpio_clr_mask(mask);
                    mask = segs[x] << SEG_FIRST_GPIO;
                    gpio_set_mask(mask);
                    sleep_ms(ANIMATION_WELCOME_BACK_DELAY);
                }
                // set to an initial zero
                gpio_clr_mask(mask);
                mask = (bits_L[0] | bits_R[0]) << SEG_FIRST_GPIO;
                gpio_set_mask(mask);

                // reset time
                t = ANIMATION_WELCOME_BACK_DELAY*8;
            } 
        }
    }
}