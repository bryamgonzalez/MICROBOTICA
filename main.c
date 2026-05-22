#include <stdio.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SERVO_PIN   GPIO_NUM_18
#define PULSADOR    GPIO_NUM_19

#define SERVO_FREQ_HZ    50
#define SERVO_RESOLUTION LEDC_TIMER_13_BIT

static const int angulos[] = {0, 45, 90, 135, 180};
static const int duties[]  = {205, 410, 614, 819, 1024};
#define NUM_ANGULOS 5

static volatile int indice_actual = 0;
static volatile int direccion     = 1;

// ─────────────────────────────────────────────
// Inicializar LEDC
// ─────────────────────────────────────────────
static void init_servo(void) {
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = SERVO_RESOLUTION,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = SERVO_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = SERVO_PIN,
        .duty       = duties[0],
        .hpoint     = 0,
    };
    ledc_channel_config(&channel);
}

// ─────────────────────────────────────────────
// Mover servo al ángulo del índice actual
// ─────────────────────────────────────────────
static void mover_servo(void) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duties[indice_actual]);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    printf("Angulo actual: %d°\n", angulos[indice_actual]);
}

// ─────────────────────────────────────────────
// ISR del pulsador
// ─────────────────────────────────────────────
static void IRAM_ATTR isr_pulsador(void *arg) {
    indice_actual += direccion;

    if (indice_actual >= NUM_ANGULOS - 1) {
        indice_actual = NUM_ANGULOS - 1;
        direccion = -1;
    } else if (indice_actual <= 0) {
        indice_actual = 0;
        direccion = 1;
    }
}

// ─────────────────────────────────────────────
// app_main
// ─────────────────────────────────────────────
void app_main(void) {
    // Configurar pulsador
    gpio_config_t in = {
        .mode         = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << PULSADOR),
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&in);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PULSADOR, isr_pulsador, NULL);

    // Inicializar servo
    init_servo();
    mover_servo();

    int ultimo_indice = indice_actual;

    while (1) {
        if (indice_actual != ultimo_indice) {
            mover_servo();
            ultimo_indice = indice_actual;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}