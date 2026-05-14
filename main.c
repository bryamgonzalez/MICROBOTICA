#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MODULO_1";

static const gpio_num_t led_pins[] = {
    GPIO_NUM_15,
    GPIO_NUM_4,
    GPIO_NUM_16,
    GPIO_NUM_17,
    GPIO_NUM_5,
};

static const size_t led_count = sizeof(led_pins) / sizeof(led_pins[0]);

static const gpio_num_t direction_button_pin = GPIO_NUM_22;
static const gpio_num_t interrupt_button_pin = GPIO_NUM_23;

static const int64_t debounce_us = 180000;
static const int64_t sequence_step_us = 800000;
static const uint32_t all_leds_on_time_ms = 5000;

typedef struct {
    gpio_num_t pin;
    int last_reading;
    int stable_level;
    int64_t last_change_us;
} button_state_t;

static volatile bool interrupt_event = false;

static button_state_t direction_button = {
    .pin = GPIO_NUM_22,
    .last_reading = 1,
    .stable_level = 1,
    .last_change_us = 0,
};

static int current_led_index = 0;
static int sequence_direction = 1;
static int64_t last_sequence_step_us = 0;

static void IRAM_ATTR interrupt_button_isr_handler(void *arg) {
    (void)arg;
    interrupt_event = true;
}

static void apply_single_led_state(void) {
    for (size_t i = 0; i < led_count; ++i) {
        gpio_set_level(led_pins[i], (int)i == current_led_index ? 1 : 0);
    }
}

static void set_all_leds(bool enabled) {
    for (size_t i = 0; i < led_count; ++i) {
        gpio_set_level(led_pins[i], enabled ? 1 : 0);
    }
}

static void advance_sequence(void) {
    current_led_index += sequence_direction;

    if (current_led_index >= (int)led_count) {
        current_led_index = (int)led_count - 2;
        sequence_direction = -1;
    } else if (current_led_index < 0) {
        current_led_index = 1;
        sequence_direction = 1;
    }
}

static bool consume_direction_button_press(button_state_t *button) {
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

static void handle_interrupt_request(void) {
    if (!interrupt_event) {
        return;
    }

    interrupt_event = false;
    ESP_LOGI(TAG, "Interrupcion detectada: todos los LEDs encendidos 5 segundos");
    set_all_leds(true);
    vTaskDelay(pdMS_TO_TICKS(all_leds_on_time_ms));
    apply_single_led_state();
    last_sequence_step_us = esp_timer_get_time();
}

static void configure_leds(void) {
    gpio_config_t led_config = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    for (size_t i = 0; i < led_count; ++i) {
        led_config.pin_bit_mask |= (1ULL << led_pins[i]);
    }

    gpio_config(&led_config);
    set_all_leds(false);
}

static void configure_buttons(void) {
    gpio_config_t direction_button_config = {
        .pin_bit_mask = (1ULL << direction_button_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&direction_button_config);

    gpio_config_t interrupt_button_config = {
        .pin_bit_mask = (1ULL << interrupt_button_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };

    gpio_config(&interrupt_button_config);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(interrupt_button_pin, interrupt_button_isr_handler, NULL);
}

static void print_pin_summary(void) {
    ESP_LOGI(TAG, "Configuracion del modulo 1");
    ESP_LOGI(TAG, "LED 1 -> GPIO 15");
    ESP_LOGI(TAG, "LED 2 -> GPIO 4");
    ESP_LOGI(TAG, "LED 3 -> GPIO 16");
    ESP_LOGI(TAG, "LED 4 -> GPIO 17");
    ESP_LOGI(TAG, "LED 5 -> GPIO 5");
    ESP_LOGI(TAG, "Boton direccion -> GPIO 22");
    ESP_LOGI(TAG, "Boton interrupcion -> GPIO 23");
}

void app_main(void) {
    configure_leds();
    configure_buttons();
    print_pin_summary();
    apply_single_led_state();
    last_sequence_step_us = esp_timer_get_time();

    while (true) {
        handle_interrupt_request();

        if (consume_direction_button_press(&direction_button)) {
            sequence_direction *= -1;
            ESP_LOGI(TAG, "Cambio de direccion: %s",
                     sequence_direction > 0 ? "izquierda a derecha"
                                            : "derecha a izquierda");
        }

        const int64_t now = esp_timer_get_time();
        if ((now - last_sequence_step_us) >= sequence_step_us) {
            last_sequence_step_us = now;
            apply_single_led_state();
            advance_sequence();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
