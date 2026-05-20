#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "driver/dac_continuous.h"
#include "driver/dac_oneshot.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DAC_VREF                3.3f
#define DAC_OUTPUT_CHAN         DAC_CHAN_0          // GPIO25 en ESP32
#define DAC_OUTPUT_MASK         DAC_CHANNEL_MASK_CH0

#define BUTTON_GPIO             GPIO_NUM_0          // Boton BOOT en muchas placas
#define BUTTON_DEBOUNCE_MS      200

#define VOLTAGE_HOLD_MS         3000

#define WAVE_SAMPLES            128
#define WAVE_FREQUENCY_HZ       1000
#define DAC_SAMPLE_FREQ_HZ      (WAVE_SAMPLES * WAVE_FREQUENCY_HZ)
#define DMA_DESC_NUM            4
#define DMA_BUF_SIZE            256

#define PI_F                    3.14159265f

typedef enum {
    WAVE_SINE = 0,
    WAVE_TRIANGLE,
    WAVE_SAWTOOTH,
    WAVE_MAX
} wave_type_t;

static dac_continuous_handle_t s_cont_handle;
static volatile wave_type_t s_current_wave = WAVE_SINE;

static uint8_t s_sine_wave[WAVE_SAMPLES];
static uint8_t s_triangle_wave[WAVE_SAMPLES];
static uint8_t s_sawtooth_wave[WAVE_SAMPLES];

static uint8_t clamp_to_u8(float value)
{
    if (value < 0.0f) {
        return 0;
    }
    if (value > 255.0f) {
        return 255;
    }
    return (uint8_t)(value + 0.5f);
}

static float dac_code_to_voltage(uint8_t dac_code)
{
    return (DAC_VREF * dac_code) / 255.0f;
}

static uint8_t voltage_to_dac_code(float voltage)
{
    if (voltage < 0.0f) {
        voltage = 0.0f;
    }
    if (voltage > DAC_VREF) {
        voltage = DAC_VREF;
    }
    return clamp_to_u8((voltage / DAC_VREF) * 255.0f);
}

static const char *wave_name(wave_type_t wave)
{
    switch (wave) {
    case WAVE_SINE:
        return "SENO";
    case WAVE_TRIANGLE:
        return "TRIANGULAR";
    case WAVE_SAWTOOTH:
        return "DIENTE_DE_SIERRA";
    default:
        return "DESCONOCIDA";
    }
}

static uint8_t *wave_buffer(wave_type_t wave)
{
    switch (wave) {
    case WAVE_SINE:
        return s_sine_wave;
    case WAVE_TRIANGLE:
        return s_triangle_wave;
    case WAVE_SAWTOOTH:
        return s_sawtooth_wave;
    default:
        return s_sine_wave;
    }
}

static void generate_wave_tables(void)
{
    for (int i = 0; i < WAVE_SAMPLES; i++) {
        float t = (float)i / (float)WAVE_SAMPLES;
        float phase = 2.0f * PI_F * t;

        s_sine_wave[i] = clamp_to_u8((sinf(phase) + 1.0f) * 127.5f);

        if (t < 0.5f) {
            s_triangle_wave[i] = clamp_to_u8(t * 2.0f * 255.0f);
        } else {
            s_triangle_wave[i] = clamp_to_u8((2.0f - (t * 2.0f)) * 255.0f);
        }

        s_sawtooth_wave[i] = clamp_to_u8(t * 255.0f);
    }
}

static void configure_button(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));
}

static bool button_is_pressed(void)
{
    return gpio_get_level(BUTTON_GPIO) == 0;
}

static void run_exercise_1(void)
{
    static const float target_voltages[] = {0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f};

    dac_oneshot_handle_t dac_handle;
    dac_oneshot_config_t dac_cfg = {
        .chan_id = DAC_OUTPUT_CHAN,
    };

    printf("\n===== EJERCICIO 1: GENERADOR DE VOLTAJES =====\n");
    printf("Salida por GPIO25 usando DAC\n");

    ESP_ERROR_CHECK(dac_oneshot_new_channel(&dac_cfg, &dac_handle));

    for (size_t i = 0; i < sizeof(target_voltages) / sizeof(target_voltages[0]); i++) {
        float target_voltage = target_voltages[i];
        uint8_t dac_code = voltage_to_dac_code(target_voltage);
        float theoretical_voltage = dac_code_to_voltage(dac_code);

        ESP_ERROR_CHECK(dac_oneshot_output_voltage(dac_handle, dac_code));

        printf("Valor DAC: %u | Voltaje aproximado: %.3f V | Objetivo: %.1f V\n",
               dac_code, theoretical_voltage, target_voltage);

        vTaskDelay(pdMS_TO_TICKS(VOLTAGE_HOLD_MS));
    }

    ESP_ERROR_CHECK(dac_oneshot_del_channel(dac_handle));
    printf("===== FIN EJERCICIO 1 =====\n\n");
}

static void start_exercise_2(void)
{
    dac_continuous_config_t cont_cfg = {
        .chan_mask = DAC_OUTPUT_MASK,
        .desc_num = DMA_DESC_NUM,
        .buf_size = DMA_BUF_SIZE,
        .freq_hz = DAC_SAMPLE_FREQ_HZ,
        .offset = 0,
        .clk_src = DAC_DIGI_CLK_SRC_DEFAULT,
        .chan_mode = DAC_CHANNEL_MODE_SIMUL,
    };

    generate_wave_tables();
    configure_button();

    ESP_ERROR_CHECK(dac_continuous_new_channels(&cont_cfg, &s_cont_handle));
    ESP_ERROR_CHECK(dac_continuous_enable(s_cont_handle));
    ESP_ERROR_CHECK(dac_continuous_write_cyclically(
        s_cont_handle, wave_buffer(s_current_wave), WAVE_SAMPLES, NULL));

    printf("===== EJERCICIO 2: GENERADOR DE SENALES =====\n");
    printf("Salida por GPIO25 usando DAC + DMA continuo\n");
    printf("Boton en GPIO0 para cambiar la onda\n");
    printf("Tipo de senal: %s | Frecuencia: %d Hz\n",
           wave_name(s_current_wave), WAVE_FREQUENCY_HZ);
    printf("Resolucion DAC: 8 bits | Muestras por ciclo: %d | Frecuencia de muestreo: %d Hz\n\n",
           WAVE_SAMPLES, DAC_SAMPLE_FREQ_HZ);
}

static void waveform_button_task(void *arg)
{
    int64_t last_press_time_us = 0;

    while (1) {
        if (button_is_pressed()) {
            int64_t now_us = esp_timer_get_time();

            if ((now_us - last_press_time_us) >= (BUTTON_DEBOUNCE_MS * 1000LL)) {
                last_press_time_us = now_us;
                s_current_wave = (wave_type_t)((s_current_wave + 1) % WAVE_MAX);

                ESP_ERROR_CHECK(dac_continuous_write_cyclically(
                    s_cont_handle, wave_buffer(s_current_wave), WAVE_SAMPLES, NULL));

                printf("Tipo de senal: %s | Frecuencia: %d Hz\n",
                       wave_name(s_current_wave), WAVE_FREQUENCY_HZ);
            }

            while (button_is_pressed()) {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void app_main(void)
{
    run_exercise_1();
    vTaskDelay(pdMS_TO_TICKS(1000));

    start_exercise_2();

    xTaskCreate(waveform_button_task, "waveform_button_task", 4096, NULL, 5, NULL);
}