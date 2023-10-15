#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

enum kb_to_gpio {
    ROW_1 = 19,
    ROW_2 = 14,
    ROW_3 = 12,
    ROW_4 = 5,
    COL_1 = 23,
    COL_2 = 18,
    COL_3 = 13
};

enum state_t {
    NORMAL,
    SET_1,
    SET_2
};

struct kb {
    int row_pins[4];
    int col_pins[3];
};

void init_leds() {
    ESP_ERROR_CHECK(gpio_reset_pin(25));
    ESP_ERROR_CHECK(gpio_set_direction(25, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_reset_pin(26));
    ESP_ERROR_CHECK(gpio_set_direction(26, GPIO_MODE_OUTPUT));
}

void set_led(int gpio_pin, bool status) {
    ESP_ERROR_CHECK(gpio_set_level(gpio_pin, status));
}

void init_keyboard(struct kb keyboard) {
    for (int i = 0; i < 4; i++) {
        ESP_ERROR_CHECK(gpio_reset_pin(keyboard.row_pins[i]));
        ESP_ERROR_CHECK(gpio_set_direction(keyboard.row_pins[i], GPIO_MODE_INPUT));
        ESP_ERROR_CHECK(gpio_set_pull_mode(keyboard.row_pins[i], GPIO_PULLUP_ONLY));
    }
    for (int i = 0; i < 3; i++) {
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

    const char kb_layout[4][3] = {
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
            memset(buf, 0, 16);
            *state = SET_2;
        } else {
            memset(buf, 0, 16);
            printf("Current password is wrong\n");
            set_led(26, false); 
            *state = NORMAL;
        }
        return;
    }
    memset(buf, 0, 16);
    printf("Password cleared\n");
    return;
}

void handle_hashtag(char *buf, char *pw, nvs_handle my_handle, enum state_t* state) {
    if (*state == NORMAL && strlen(buf) == 0) {
        *state = SET_1;
    } else if (*state == SET_2) {
        printf("Password changed\n");
        strcpy(pw, buf);
        nvs_set_str(my_handle, "password", pw);
        nvs_commit(my_handle);
        printf("New password: %s\n", pw);
        memset(buf, 0, 16);
        *state = NORMAL;
    } else {
        printf("%s\n", buf);
        if (strcmp(buf, pw) == 0) {
            printf("Correct password\n");
            set_led(25, true); set_led(26, false);
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            set_led(25, false); set_led(26, true);
        } else {
            printf("Wrong password\n");
            set_led(26, false);
            vTaskDelay(500 / portTICK_PERIOD_MS);
            set_led(26, true);
        }
        memset(buf, 0, 16);
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

    if(strlen(buf) == 15) {
        printf("Password too long, press (*) to clear\n");
        printf("%s\n", buf);
        return;
    }

    buf[strlen(buf)] = c;
    printf("%s\n", buf);
}

void terminal_task(void *pvParameter) {
    // setup nvs
    nvs_handle my_handle;
    ESP_ERROR_CHECK(nvs_open("storage", NVS_READWRITE, &my_handle));
    char pw[16]; size_t len = 16;
    esp_err_t err = nvs_get_str(my_handle, "password", pw, &len);
    if (err == ESP_OK) {
        printf("Password found\n");
        printf("Password: %s\n", pw);
    } else {
        printf("Password not found\n");
        char default_pw[] = "1234";
        nvs_set_str(my_handle, "password", default_pw);
        nvs_commit(my_handle);
    }
    // setup leds
    init_leds();
    set_led(26, true);
    // setup keyboard
    struct kb keyboard = {
        .row_pins = {ROW_1, ROW_2, ROW_3, ROW_4},
        .col_pins = {COL_1, COL_2, COL_3}
    };
    init_keyboard(keyboard);
    // setup variables
    char c;
    char buf[16] = "";
    enum state_t state = NORMAL;
    // main loop
    while (1) {
        for (int col = 0; col < 3; col++) {
            gpio_set_level(keyboard.col_pins[col], 0);
            for (int row = 0; row < 4; row++) {
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