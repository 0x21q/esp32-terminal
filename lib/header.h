#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

// GPIO mapping for the keyboard
#define ROW_1 19
#define ROW_2 14
#define ROW_3 12
#define ROW_4 5
#define COL_1 23
#define COL_2 18
#define COL_3 13

// GPIO mapping for the LEDs
#define LED_GREEN 25
#define LED_RED 26

// Keyboard dimensions
#define ROWS 4
#define COLS 3

// Max password length
#define PW_LEN 16

// Using state_t to track the state of program
enum state_t {
    NORMAL,
    SET_1,
    SET_2
};

// Keyboard struct allows iterating through the pins
struct kb {
    int row_pins[ROWS];
    int col_pins[COLS];
};

/**
 * @brief Initialize the LEDs
*/
void init_leds();

/**
 * @brief Initialize the keyboard
 * @param gpio_pin GPIO pin of led
 * @param status Status of the led (true = on, false = off)
*/
void set_led(int gpio_pin, bool status);

/**
 * @brief Blink the LED for a given delay and value
 * @param gpio_pin GPIO pin of led
 * @param delay Blink interval
 * @param begin_state Begin state of the blink (true = on -> off, false = off -> on)
*/
void blink_led(int gpio_pin, int delay, bool begin_state);

/**
 * @brief Alternate between LEDs for a given delay
 * @param delay Blink interval
*/
void blink_swap_leds(int delay);

/**
 * @brief Initialize the keyboard
 * @param keyboard Keyboard struct
*/
void init_keyboard(struct kb keyboard);

/**
 * @brief Check if a button is pressed
 * @param row_gpio GPIO pin of the row
 * @return True if button is pressed, false otherwise
*/
bool button_pressed(int row_gpio);

/**
 * @brief Handle the button press
 * @param row Row of the button
 * @param col Column of the button
 * @param row_gpio GPIO pin of the row
 * @return Character of the pressed button, or '\0' if no button is pressed
*/
char button_handler(int row, int col, int row_gpio);

/**
 * @brief Handle the asterisk button press
 * @param buf Buffer for user input
 * @param pw Current password
 * @param my_handle Handle for the NVS
 * @param state Current state of the program
*/
void handle_asterisk(char *buf, char *pw, nvs_handle my_handle, enum state_t* state);

/**
 * @brief Handle the hashtag button press
 * @param buf Buffer for user input
 * @param pw Current password
 * @param my_handle Handle for the NVS
 * @param state Current state of the program
*/
void handle_hashtag(char *buf, char *pw, nvs_handle my_handle, enum state_t* state);

/**
 * @brief Handle the character given by button_handler
 * @param c Character of the pressed button
 * @param pw Current password
 * @param buf Buffer for user input
 * @param my_handle Handle for the NVS
 * @param state Current state of the program
*/
void check_char(char c, char *pw, char *buf, nvs_handle my_handle, enum state_t* state);

/**
 * @brief Main task for the program
 * @param pvParameter Optional data parameter for the task
*/
void terminal_task(void *pvParameter);