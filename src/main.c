/**
 * @file main.c
 * @brief Main file for the project
 * @author Jakub Kratochvil (xkrato67)
*/

#include "../lib/header.h"

// Change this constant to ENABLE/DISABLE debug printing
#define DEBUG 0

void init_leds() {
    ESP_ERROR_CHECK(gpio_reset_pin(LED_GREEN));
    ESP_ERROR_CHECK(gpio_set_direction(LED_GREEN, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_reset_pin(LED_RED));
    ESP_ERROR_CHECK(gpio_set_direction(LED_RED, GPIO_MODE_OUTPUT));
}

void set_led(int gpio_pin, bool status) {
    ESP_ERROR_CHECK(gpio_set_level(gpio_pin, status));
}

void blink_led(int gpio_pin, int delay, bool begin_state) {
    set_led(gpio_pin, begin_state);
    vTaskDelay(delay / portTICK_PERIOD_MS);
    set_led(gpio_pin, !begin_state);
}

void blink_swap_leds(int delay) {
    set_led(LED_GREEN, true);
    set_led(LED_RED, false);
    vTaskDelay(delay / portTICK_PERIOD_MS);
    set_led(LED_GREEN, false);
    set_led(LED_RED, true);
}

void init_keyboard(struct kb keyboard) {
    for (int i = 0; i < ROWS; i++) {
        ESP_ERROR_CHECK(gpio_reset_pin(keyboard.row_pins[i]));
        ESP_ERROR_CHECK(gpio_set_direction(keyboard.row_pins[i], GPIO_MODE_INPUT));
        ESP_ERROR_CHECK(gpio_set_pull_mode(keyboard.row_pins[i], GPIO_PULLUP_ONLY));
    }
    for (int i = 0; i < COLS; i++) {
        ESP_ERROR_CHECK(gpio_reset_pin(keyboard.col_pins[i]));
        ESP_ERROR_CHECK(gpio_set_direction(keyboard.col_pins[i], GPIO_MODE_OUTPUT));
        ESP_ERROR_CHECK(gpio_set_level(keyboard.col_pins[i], 1));
    }
}

bool button_pressed(int row_gpio) {
    if (gpio_get_level(row_gpio) == 0) {
        vTaskDelay(30 / portTICK_PERIOD_MS);
        if (gpio_get_level(row_gpio) == 0) {
            vTaskDelay(120 / portTICK_PERIOD_MS);
            return true;
        }
    }
    return false;
}

char button_handler(int row, int col, int row_gpio) {
    if (!button_pressed(row_gpio))
        return '\0';

    const char kb_layout[ROWS][COLS] = {
        {'1', '2', '3'},
        {'4', '5', '6'},
        {'7', '8', '9'},
        {'*', '0', '#'}
    };

    return kb_layout[row][col];
}

void handle_asterisk(char *buf, char *pw, nvs_handle my_handle, enum state_t* state) {
    if (*state == SET_1) {
        if (strcmp (buf, pw) == 0) {
            memset(buf, 0, PW_LEN);
            if (DEBUG) printf("Current password is correct\n");
            blink_swap_leds(500);
            *state = SET_2;
        } else {
            memset(buf, 0, PW_LEN);
            if (DEBUG) printf("Current password is wrong\n");
            blink_led(LED_RED, 500, false);
            *state = NORMAL;
        }
        return;
    }
    memset(buf, 0, PW_LEN);
    if (DEBUG) printf("Password cleared\n");
    return;
}

void handle_hashtag(char *buf, char *pw, nvs_handle my_handle, enum state_t* state) {
    if (*state == NORMAL && strlen(buf) == 0) {
        if (DEBUG) printf("Enter current password\n");
        *state = SET_1;
    } else if (*state == SET_2) {
        strcpy(pw, buf);
        nvs_set_str(my_handle, "password", pw);
        nvs_commit(my_handle);
        if (DEBUG) printf("Password changed to: %s\n", pw);
        memset(buf, 0, PW_LEN);
        blink_swap_leds(500);
        vTaskDelay(250 / portTICK_PERIOD_MS);
        blink_swap_leds(500);
        *state = NORMAL;
    } else {
        if (DEBUG) printf("%s\n", buf);
        if (strcmp(buf, pw) == 0) {
            if (DEBUG) printf("Correct password\n");
            blink_swap_leds(3500);
        } else {
            if (DEBUG) printf("Wrong password\n");
            blink_led(LED_RED, 500, false);
        }
        memset(buf, 0, PW_LEN);
    }
}

void check_char(char c, char *pw, char *buf, nvs_handle my_handle, enum state_t* state) {
    if (c == '\0')
        return;

    if (c == '*') {
        handle_asterisk(buf, pw, my_handle, state);
        return;
    } 

    if (c == '#') {
        handle_hashtag(buf, pw, my_handle, state);
        return;
    }

    if (strlen(buf) == PW_LEN - 1) {
        if (DEBUG) {
            printf("Password too long, press (*) to clear\n");
            printf("%s\n", buf);
        }
        return;
    }

    buf[strlen(buf)] = c;
    if (DEBUG) printf("%s\n", buf);
}

void terminal_task(void *pvParameter) {
    // setup nvs
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    char pw[PW_LEN];
    size_t len = PW_LEN;
    esp_err_t err = nvs_get_str(my_handle, "password", pw, &len);
    if (err == ESP_OK) {
        if (DEBUG) printf("Password found in memory: %s\n", pw);
    } else {
        if (DEBUG) printf("Password not found\n");
        char default_pw[] = "1234";
        nvs_set_str(my_handle, "password", default_pw);
        nvs_commit(my_handle);
    }
    // setup leds
    init_leds();
    set_led(LED_RED, true);
    // setup keyboard struct
    struct kb keyboard = {
        .row_pins = {ROW_1, ROW_2, ROW_3, ROW_4},
        .col_pins = {COL_1, COL_2, COL_3}
    };
    init_keyboard(keyboard);
    // setup variables
    char c;
    char buf[PW_LEN] = "";
    enum state_t state = NORMAL;
    // main loop
    while (1) {
        for (int col = 0; col < COLS; col++) {
            gpio_set_level(keyboard.col_pins[col], 0);
            for (int row = 0; row < ROWS; row++) {
                c = button_handler(row, col, keyboard.row_pins[row]);
                check_char(c, pw, buf, my_handle, &state);
            }
            gpio_set_level(keyboard.col_pins[col], 1);
        }
    }
    nvs_close(my_handle);
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    xTaskCreate(&terminal_task, "terminal_task", 4096, NULL, 5, NULL);
}