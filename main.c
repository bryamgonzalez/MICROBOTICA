#include <stdio.h>
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define Pulsador GPIO_NUM_14
#define Verde    GPIO_NUM_27
#define Amarillo GPIO_NUM_26
#define Rojo     GPIO_NUM_25

typedef enum {
    Emergencia,
    VERDE,
    AMARILLO,
    ROJO
} Semaforo;

static volatile Semaforo estado_actual  = VERDE;
static volatile bool emergencia_on      = false;
static volatile bool toggle_emergencia  = false;

static gptimer_handle_t tiempo_semaforo   = NULL;
static gptimer_handle_t tiempo_emergencia = NULL;

void apagar_semaforo(void) {
    gpio_set_level(Verde,    0);
    gpio_set_level(Amarillo, 0);
    gpio_set_level(Rojo,     0);
}

void actualizar_estado(void) {
    apagar_semaforo();
    switch (estado_actual) {
        case VERDE:      gpio_set_level(Verde,    1); break;
        case AMARILLO:   gpio_set_level(Amarillo, 1); break;
        case ROJO:       gpio_set_level(Rojo,     1); break;
        case Emergencia: break;
    }
}

static bool IRAM_ATTR Cb_semaforo(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata,
    void *user_data)
{
    if (emergencia_on) return false;

    switch (estado_actual) {
        case VERDE:
            estado_actual = AMARILLO;
            gptimer_alarm_config_t alarma_2 = {
                .alarm_count = 2000000,
                .reload_count = 0,
                .flags.auto_reload_on_alarm = true,
            };
            gptimer_set_alarm_action(timer, &alarma_2);
            break;

        case AMARILLO:
            estado_actual = ROJO;
            gptimer_alarm_config_t alarma_5 = {
                .alarm_count = 5000000,
                .reload_count = 0,
                .flags.auto_reload_on_alarm = true,
            };
            gptimer_set_alarm_action(timer, &alarma_5);
            break;

        case ROJO:
            estado_actual = VERDE;
            break;

        default: break;
    }
    actualizar_estado();
    return false;
}

static bool IRAM_ATTR cb_emergencia(
    gptimer_handle_t timer,
    const gptimer_alarm_event_data_t *edata,
    void *user_data)
{
    toggle_emergencia = !toggle_emergencia;
    gpio_set_level(Amarillo, toggle_emergencia ? 1 : 0);
    return false;
}

static void IRAM_ATTR p_emergencia(void *arg) {
    if (!emergencia_on) {
        emergencia_on = true;
        gptimer_stop(tiempo_semaforo);
        apagar_semaforo();
        gptimer_start(tiempo_emergencia);
    } else {
        emergencia_on = false;
        gptimer_stop(tiempo_emergencia);
        apagar_semaforo();
        estado_actual = VERDE;
        actualizar_estado();
        gptimer_start(tiempo_semaforo);
    }
}

void iniciar_temps(void) {
    gptimer_config_t config = {
        .clk_src       = GPTIMER_CLK_SRC_DEFAULT,
        .direction     = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };

    gptimer_new_timer(&config, &tiempo_semaforo);
    gptimer_event_callbacks_t cbs_sem = {
        .on_alarm = Cb_semaforo
    };
    gptimer_register_event_callbacks(tiempo_semaforo, &cbs_sem, NULL);
    gptimer_alarm_config_t alarma_verde = {
        .alarm_count = 5000000,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    gptimer_set_alarm_action(tiempo_semaforo, &alarma_verde);

    gptimer_new_timer(&config, &tiempo_emergencia);
    gptimer_event_callbacks_t cbs_emergencia = {
        .on_alarm = cb_emergencia
    };
    gptimer_register_event_callbacks(tiempo_emergencia, &cbs_emergencia, NULL);
    gptimer_alarm_config_t alarma_emergencia = {
        .alarm_count = 500000,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    gptimer_set_alarm_action(tiempo_emergencia, &alarma_emergencia);

    gptimer_enable(tiempo_semaforo);
    gptimer_enable(tiempo_emergencia);
}

void app_main(void) {
    gpio_config_t out = {
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << Verde) | (1ULL << Amarillo) | (1ULL << Rojo),
    };
    gpio_config(&out);

    gpio_config_t in = {
        .mode         = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << Pulsador),
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&in);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(Pulsador, p_emergencia, NULL);

    iniciar_temps();
    actualizar_estado();
    gptimer_start(tiempo_semaforo);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}