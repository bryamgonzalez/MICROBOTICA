#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MODULO2_ESP2";

static const gpio_num_t led_1_pin = GPIO_NUM_12;
static const gpio_num_t led_2_pin = GPIO_NUM_14;
static const gpio_num_t button_1_pin = GPIO_NUM_27;
static const gpio_num_t button_2_pin = GPIO_NUM_25;

static const uart_port_t uart_port = UART_NUM_2;
static const int uart_tx_pin = GPIO_NUM_17;
static const int uart_rx_pin = GPIO_NUM_16;

static const int64_t debounce_us = 180000;
static const int uart_buffer_size = 1024;

typedef struct {
    gpio_num_t pin;
    int last_reading;
    int stable_level;
    int64_t last_change_us;
} button_state_t;

static button_state_t button_1 = {
    .pin = GPIO_NUM_27,
    .last_reading = 1,
    .stable_level = 1,
    .last_change_us = 0,
};

static button_state_t button_2 = {
    .pin = GPIO_NUM_25,
    .last_reading = 1,
    .stable_level = 1,
    .last_change_us = 0,
};

static bool consume_button_press(button_state_t *button) {
    const int reading = gpio_get_level(button->pin);
    const int64_t now = esp_timer_get_time();

    if (reading != button->last_reading) {
        button->last_reading = reading;
        button->last_change_us = now;
    }

    if ((now - button->last_change_us) < debounce_us) {
        return false;
    }

    if (reading != button->stable_level) {
        button->stable_level = reading;
        return reading == 0;
    }

    return false;
}

static void set_leds(bool led_1_on, bool led_2_on) {
    gpio_set_level(led_1_pin, led_1_on ? 1 : 0);
    gpio_set_level(led_2_pin, led_2_on ? 1 : 0);
}

static void configure_leds(void) {
    gpio_config_t led_config = {
        .pin_bit_mask = (1ULL << led_1_pin) | (1ULL << led_2_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&led_config);
    set_leds(false, false);
}

static void configure_buttons(void) {
    gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << button_1_pin) | (1ULL << button_2_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&button_config);
}

static void configure_uart(void) {
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(uart_port, uart_buffer_size, uart_buffer_size, 0, NULL, 0);
    uart_param_config(uart_port, &uart_config);
    uart_set_pin(uart_port, uart_tx_pin, uart_rx_pin, UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);
}

static void print_pin_summary(void) {
    ESP_LOGI(TAG, "Modulo 2 - ESP2");
    ESP_LOGI(TAG, "LED 1 -> GPIO 12");
    ESP_LOGI(TAG, "LED 2 -> GPIO 14");
    ESP_LOGI(TAG, "Boton 1 -> GPIO 27 (envia 'A')");
    ESP_LOGI(TAG, "Boton 2 -> GPIO 25 (envia 'B')");
    ESP_LOGI(TAG, "UART TX -> GPIO 17");
    ESP_LOGI(TAG, "UART RX -> GPIO 16");
}

static void send_uart_message(char message) {
    uart_write_bytes(uart_port, &message, 1);
    ESP_LOGI(TAG, "Enviado por UART: '%c'", message);
}

static void handle_received_char(char received) {
    if (received == 'A') {
        set_leds(false, true);
        ESP_LOGI(TAG, "Recibido 'A' -> LED2 encendido");
    } else if (received == 'B') {
        set_leds(true, false);
        ESP_LOGI(TAG, "Recibido 'B' -> LED1 encendido");
    } else {
        ESP_LOGI(TAG, "Recibido caracter no esperado: '%c'", received);
    }
}

static void read_uart_messages(void) {
    uint8_t data[16];
    const int length = uart_read_bytes(uart_port, data, sizeof(data), pdMS_TO_TICKS(20));

    for (int i = 0; i < length; ++i) {
        handle_received_char((char)data[i]);
    }
}

void app_main(void) {
    configure_leds();
    configure_buttons();
    configure_uart();
    print_pin_summary();

    while (true) {
        if (consume_button_press(&button_1)) {
            send_uart_message('A');
        }

        if (consume_button_press(&button_2)) {
            send_uart_message('B');
        }

        read_uart_messages();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

