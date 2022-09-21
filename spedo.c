#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#include "hardware/i2c.h"
// from https://github.com/daschr/pico-ssd1306 (owner doesn't have a proper way to add project, instructs to just copy files in manually)
#include "extern/pico-ssd1306/src/ssd1306.h"

#define REED_GPIO 22
#define SEG_FIRST_GPIO 8

#define WHEEL_CIRCUMFERENCE 2.231

#define VELOCITY_CONSTANT (WHEEL_CIRCUMFERENCE*60*60)

#define ANIMATION_WELCOME_BACK_DELAY 40

#define DISPLAY_I2C i2c0
#define DISPLAY_I2C_SCL 5
#define DISPLAY_I2C_SDA 4

// define characters for each segment
const int bits_L[10] = {
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
const int bits_R[10] = {
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

void draw_oled(ssd1306_t disp, int dist, int mins_all, int mins_moving, int av_speed, int max_speed, int curr_speed_miles) {
    char str[20];
    ssd1306_clear(&disp);
    sprintf(str, "%d", dist);
    ssd1306_draw_string(&disp, 0, 0, 1, str);
    ssd1306_draw_string(&disp, 50, 0, 1, "metres");
    sprintf(str, "%d", mins_all);
    ssd1306_draw_string(&disp, 0, 10, 1, str);
    ssd1306_draw_string(&disp, 50, 10, 1, "minutes,");
    sprintf(str, "%d", mins_moving);
    ssd1306_draw_string(&disp, 0, 20, 1, str);
    ssd1306_draw_string(&disp, 50, 20, 1, "moving");
    sprintf(str, "%d", av_speed);
    ssd1306_draw_string(&disp, 0, 30, 1, str);
    ssd1306_draw_string(&disp, 50, 30, 1, "km/h avg.");
    sprintf(str, "%d", max_speed);
    ssd1306_draw_string(&disp, 0, 40, 1, str);
    ssd1306_draw_string(&disp, 50, 40, 1, "km/h max.");
    ssd1306_draw_string(&disp, 0, 50, 2, "km/h");
    sprintf(str, "%d", curr_speed_miles);
    ssd1306_draw_string(&disp, 80, 57, 1, str);
    ssd1306_draw_string(&disp, 105, 57, 1, "mph");
    ssd1306_show(&disp);
}

int main() {

    // INIT HARDWARE ---------------------------------------------------------------

    // init serial connection
    stdio_init_all();

    // init OLED
    i2c_init(DISPLAY_I2C, 400000);
    gpio_set_function(DISPLAY_I2C_SCL, GPIO_FUNC_I2C);
    gpio_set_function(DISPLAY_I2C_SDA, GPIO_FUNC_I2C);
    gpio_pull_up(DISPLAY_I2C_SCL);
    gpio_pull_up(DISPLAY_I2C_SDA);

    ssd1306_t disp;
    disp.external_vcc=false;
    ssd1306_init(&disp, 128, 64, 0x3C, DISPLAY_I2C);

    // Show test screen
    draw_oled(disp, 0, 0, 0, 0, 0, 0);

    // init rev indicator LED
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

    // set initial segments to single dash (normal resting state)
    int32_t mask;
    mask = 0b1000 << SEG_FIRST_GPIO;
    gpio_set_mask(mask);

    // INIT MAINLOOP ---------------------------------------------------------------

    int t = 0; // time in milliseconds since last complete revolution of the wheel
    int state = 3; // internal state used to determine if moving or not etc...
    float dist = 0; // distance in meters

    int max_v = 0; // highest speed reached since power on

    int time = 0; // current time tracker (like t, but never reset)
    int all_time = 0; // time in seconds since power on
    int moving_time = 1; // time in seconds that have been in motion (init to 1 to prevent /0 errors, still appears as 0mins on display)

    int mph = 0; // used only to store current speed for the conversion so can be used in optimised rendering times

    absolute_time_t prev_loop_iter_time_usec = get_absolute_time();

    while (1) {
        // account for if took over 1 millisecond for prev loop cycle
        while (absolute_time_diff_us(prev_loop_iter_time_usec, get_absolute_time()) > 1000) {
            t++;
            time++;
            prev_loop_iter_time_usec = delayed_by_ms(prev_loop_iter_time_usec,1);
        }
        // make sure exactly 1 millisecond has elapsed since timers were last incremented
        sleep_until(delayed_by_ms(prev_loop_iter_time_usec,1)); // sleep for up to 1ms
        prev_loop_iter_time_usec = get_absolute_time();  // start timer for next cycle of the mainloop
        // increment the timers
        t++;
        time++;
        // update time counters
        if (time > 1000) { 
            // then it must have been at least 1 second since last updated - so add 1 second to counters
            time -= 1000; // subtract in case it is like 1.05 seconds (for accuracy)
            all_time++;
            if (state < 3) {
                // only update if moving
                moving_time++;
            }
            // only update OLED every so often, since it takes a little while to update it it slows down the mainloop considerably if updated every 'millisecond'
            draw_oled(disp, (int)dist, (int)(all_time/60), (int)(moving_time/60), (int) (3.6*(dist / moving_time)), max_v, mph);
        }

        if (state == 0) { // reed-open state
            if (!gpio_get(REED_GPIO)){ // reed closed
                state = 1; 
                // this is kinda now a state 0.5 (only run when entering state 1 from 0)

                // set the speed based on params
                int v = VELOCITY_CONSTANT / t; // velocity in km/h
                dist += WHEEL_CIRCUMFERENCE; // add distance to the log
                printf("%d km/h = %d mph | %d m\n", v, (int) (v*0.62), (int)dist);
            
                // remove previous display, set new mask, and display
                gpio_clr_mask(mask);
                if (v >= 10) {
                    mask = (bits_L[v/10] | bits_R[v%10]) << SEG_FIRST_GPIO;
                } else {
                    mask = bits_R[v%10] << SEG_FIRST_GPIO;
                }
                gpio_set_mask(mask);

                if (v > max_v) {
                    max_v = v;
                }

                mph = (int) (v*0.62);
                draw_oled(disp, (int)dist, (int)(all_time/60), (int)(moving_time/60), (int) (3.6*(dist / moving_time)), max_v, mph);

                // reset time
                t = 0;
            } 
            // else stay in current state
            if (t == 5000) { // effectively stationary - turn display to dashes
                // remove previous display, set new mask, and display
                gpio_clr_mask(mask);
                mask = bits_R[0] << SEG_FIRST_GPIO; // 0b10001000 for dashes -- set to zero because it needs to be consuming enough power for power bank to not turn off!
                gpio_set_mask(mask);
                printf("0 km/h = 0 mph | %d m\n", (int)dist);
            }
            if (t > 10000) { // effectively stationary - turn display off
                // remove previous display, set new mask, and display
                gpio_clr_mask(mask);
                mask = 0b1000 << SEG_FIRST_GPIO;
                gpio_set_mask(mask);
                state = 3;
                mph = 0;
                printf("----- STOPPED -----\n");
            }
        }
        if (state == 1) { // in reed-closed state
            // flash the onboard led for 50ms, add that to the timer
            gpio_put(LED_PIN, 1);
            sleep_ms(50);
            t += 50;
            time += 50;
            gpio_put(LED_PIN, 0);
            state = 2;
        }
        if (state == 2) { // trying to leave reed-closed state
            // check constantly to see when the passing of the magnet is over
            if (!gpio_get(REED_GPIO)){ // reed closed
                state = 1;
            } else { // reed open
                state = 0;
            }
        }
        if (state == 3) { // stationary reed-open state
            // do nothing, unless starting up again:
            if (!gpio_get(REED_GPIO)){ // reed closed
                state = 1; 
                // show welcome back message
                printf("----- STARTING -----\n");
            
                // show animation!
                int segs[8] = { 
                    0b1,
                    0b10,
                    0b100000,
                    0b1000000,
                    0b100000000,
                    0b1000000000,
                    0b1000000000000,
                    0b10000000000000
                }; // circular animation
                for (int x = 0; x < 8; x++) {
                    gpio_clr_mask(mask);
                    mask = segs[x] << SEG_FIRST_GPIO;
                    gpio_set_mask(mask);
                    sleep_ms(ANIMATION_WELCOME_BACK_DELAY);
                }
                // set to an initial zero
                gpio_clr_mask(mask);
                mask = bits_R[0] << SEG_FIRST_GPIO;
                gpio_set_mask(mask);

                // reset time
                t = ANIMATION_WELCOME_BACK_DELAY*8;
                time += ANIMATION_WELCOME_BACK_DELAY*8; // don't reset this one!
            } 
        }
    }
}