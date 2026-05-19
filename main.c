// Librerías estándar de C
#include <stdio.h>      // Permite usar funciones como printf y snprintf
#include <string.h>     // Permite usar funciones para manejo de cadenas como strlen

// Librerías de FreeRTOS
#include "freertos/FreeRTOS.h"   // Núcleo de FreeRTOS
#include "freertos/task.h"       // Manejo de tareas

// Librerías del ESP-IDF para red y eventos
#include "esp_event.h"           // Manejo de eventos del sistema
#include "esp_http_server.h"     // Servidor HTTP integrado del ESP32
#include "esp_log.h"             // Mensajes de depuración por consola
#include "esp_netif.h"           // Interfaces de red
#include "esp_wifi.h"            // Control WiFi
#include "nvs_flash.h"           // Memoria NVS (almacenamiento interno)

// Librería para manejo de GPIO
#include "driver/gpio.h"

// ---------------- CONFIGURACIÓN DEL ACCESS POINT ----------------

// Nombre de la red WiFi que creará el ESP32
#define AP_SSID      "ESP32-AP"

// Contraseña de la red WiFi
#define AP_PASSWORD  "12345678"

// Canal WiFi utilizado
#define AP_CHANNEL   1

// Máximo número de dispositivos conectados
#define AP_MAX_CONN  4

// ---------------- DEFINICIÓN DE PINES ----------------

// GPIO donde está conectado el LED 1
#define LED1_GPIO GPIO_NUM_26

// GPIO donde está conectado el LED 2
#define LED2_GPIO GPIO_NUM_27

// Etiqueta usada para mostrar mensajes en consola
static const char *TAG = "MODULO6_AP";

// -----------------------------------------------------------------------------
// FUNCIÓN: init_leds
// CONFIGURA LOS PINES DE LOS LEDS COMO SALIDAS
// -----------------------------------------------------------------------------
static void init_leds(void)
{
    // Reinicia configuración del pin del LED 1
    gpio_reset_pin(LED1_GPIO);

    // Configura el pin como salida
    gpio_set_direction(LED1_GPIO, GPIO_MODE_OUTPUT);

    // Inicialmente el LED queda apagado
    gpio_set_level(LED1_GPIO, 0);

    // Reinicia configuración del pin del LED 2
    gpio_reset_pin(LED2_GPIO);

    // Configura el pin como salida
    gpio_set_direction(LED2_GPIO, GPIO_MODE_OUTPUT);

    // Inicialmente el LED queda apagado
    gpio_set_level(LED2_GPIO, 0);
}

// -----------------------------------------------------------------------------
// FUNCIÓN: redirect_to_root
// REDIRECCIONA AL USUARIO A LA PÁGINA PRINCIPAL "/"
// -----------------------------------------------------------------------------
static esp_err_t redirect_to_root(httpd_req_t *req)
{
    // Código HTTP 303 = redirección
    httpd_resp_set_status(req, "303 See Other");

    // Redirecciona hacia la raíz "/"
    httpd_resp_set_hdr(req, "Location", "/");

    // Envía respuesta vacía
    return httpd_resp_send(req, NULL, 0);
}

// -----------------------------------------------------------------------------
// FUNCIÓN: root_get_handler
// GENERA Y ENVÍA LA PÁGINA WEB PRINCIPAL
// -----------------------------------------------------------------------------
static esp_err_t root_get_handler(httpd_req_t *req)
{
    // Buffer donde se almacenará el HTML
    char html[2500];

    // Verifica estado del LED 1
    // Si el pin está en HIGH -> "ENCENDIDO"
    // Si está en LOW -> "APAGADO"
    const char *estado_led1 =
        gpio_get_level(LED1_GPIO) ? "ENCENDIDO" : "APAGADO";

    // Verifica estado del LED 2
    const char *estado_led2 =
        gpio_get_level(LED2_GPIO) ? "ENCENDIDO" : "APAGADO";

    // Construcción completa del código HTML
    int len = snprintf(
        html,
        sizeof(html),

        // ---------------- HTML ----------------
        "<!DOCTYPE html>"
        "<html>"

        // CABECERA
        "<head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"

        // Título de la pestaña del navegador
        "<title>Modulo 6 - WiFi</title>"

        // ---------------- CSS ----------------
        "<style>"

        // Estilo del cuerpo
        "body{"
        "font-family:Arial,sans-serif;"
        "text-align:center;"
        "background:#f2f2f2;"
        "margin:0;"
        "padding:30px;"
        "}"

        // Caja contenedora
        ".contenedor{"
        "max-width:420px;"
        "margin:auto;"
        "background:#fff;"
        "padding:25px;"
        "border-radius:12px;"
        "box-shadow:0 4px 12px rgba(0,0,0,0.15);"
        "}"

        // Título
        "h1{margin-bottom:20px;}"

        // Texto
        "p{font-size:18px;margin:10px 0;}"

        // Estilo general de botones
        ".btn{"
        "display:block;"
        "width:100%%;"
        "padding:15px;"
        "margin:12px 0;"
        "font-size:18px;"
        "color:white;"
        "border:none;"
        "border-radius:8px;"
        "cursor:pointer;"
        "}"

        // Colores de botones
        ".verde{background:#28a745;}"
        ".azul{background:#007bff;}"
        ".rojo{background:#dc3545;}"

        "</style>"
        "</head>"

        // ---------------- CUERPO ----------------
        "<body>"

        // Caja principal
        "<div class='contenedor'>"

        // Título principal
        "<h1>ESP32 Web Server</h1>"

        // Estado del LED 1
        "<p>LED 1 (GPIO %d): <b>%s</b></p>"

        // Estado del LED 2
        "<p>LED 2 (GPIO %d): <b>%s</b></p>"

        // Botón para encender LED 1
        "<a href='/led1/on'>"
        "<button class='btn verde'>Encender LED 1</button>"
        "</a>"

        // Botón para encender LED 2
        "<a href='/led2/on'>"
        "<button class='btn azul'>Encender LED 2</button>"
        "</a>"

        // Botón para apagar LEDs
        "<a href='/led/off'>"
        "<button class='btn rojo'>Apagar LED</button>"
        "</a>"

        "</div>"
        "</body>"
        "</html>",

        // Variables insertadas en el HTML
        LED1_GPIO, estado_led1,
        LED2_GPIO, estado_led2
    );

    // Verifica errores o desbordamiento del buffer
    if (len < 0 || len >= sizeof(html)) {
        return ESP_FAIL;
    }

    // Define tipo de contenido HTML
    httpd_resp_set_type(req, "text/html");

    // Envía página al navegador
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

// -----------------------------------------------------------------------------
// FUNCIÓN: led1_on_handler
// ENCIENDE EL LED 1 Y APAGA EL LED 2
// -----------------------------------------------------------------------------
static esp_err_t led1_on_handler(httpd_req_t *req)
{
    // LED 1 encendido
    gpio_set_level(LED1_GPIO, 1);

    // LED 2 apagado
    gpio_set_level(LED2_GPIO, 0);

    // Mensaje en consola
    ESP_LOGI(TAG, "LED 1 ENCENDIDO");

    // Redirección a página principal
    return redirect_to_root(req);
}

// -----------------------------------------------------------------------------
// FUNCIÓN: led2_on_handler
// ENCIENDE EL LED 2 Y APAGA EL LED 1
// -----------------------------------------------------------------------------
static esp_err_t led2_on_handler(httpd_req_t *req)
{
    // LED 1 apagado
    gpio_set_level(LED1_GPIO, 0);

    // LED 2 encendido
    gpio_set_level(LED2_GPIO, 1);

    // Mensaje en consola
    ESP_LOGI(TAG, "LED 2 ENCENDIDO");

    // Redirección a página principal
    return redirect_to_root(req);
}

// -----------------------------------------------------------------------------
// FUNCIÓN: led_off_handler
// APAGA LOS DOS LEDS
// -----------------------------------------------------------------------------
static esp_err_t led_off_handler(httpd_req_t *req)
{
    // Apaga LED 1
    gpio_set_level(LED1_GPIO, 0);

    // Apaga LED 2
    gpio_set_level(LED2_GPIO, 0);

    // Mensaje en consola
    ESP_LOGI(TAG, "LEDS APAGADOS");

    // Regresa a la página principal
    return redirect_to_root(req);
}

// -----------------------------------------------------------------------------
// FUNCIÓN: start_webserver
// INICIA EL SERVIDOR WEB Y REGISTRA LAS RUTAS
// -----------------------------------------------------------------------------
static httpd_handle_t start_webserver(void)
{
    // Configuración por defecto del servidor
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Variable manejadora del servidor
    httpd_handle_t server = NULL;

    // Intenta iniciar servidor
    if (httpd_start(&server, &config) == ESP_OK) {

        // ---------------- RUTA PRINCIPAL "/" ----------------
        httpd_uri_t root = {
            .uri = "/",                      // Dirección
            .method = HTTP_GET,             // Método GET
            .handler = root_get_handler,    // Función asociada
            .user_ctx = NULL
        };

        // ---------------- RUTA LED 1 ----------------
        httpd_uri_t led1_on = {
            .uri = "/led1/on",
            .method = HTTP_GET,
            .handler = led1_on_handler,
            .user_ctx = NULL
        };

        // ---------------- RUTA LED 2 ----------------
        httpd_uri_t led2_on = {
            .uri = "/led2/on",
            .method = HTTP_GET,
            .handler = led2_on_handler,
            .user_ctx = NULL
        };

        // ---------------- RUTA APAGAR LEDS ----------------
        httpd_uri_t led_off = {
            .uri = "/led/off",
            .method = HTTP_GET,
            .handler = led_off_handler,
            .user_ctx = NULL
        };

        // Registra todas las rutas
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &led1_on);
        httpd_register_uri_handler(server, &led2_on);
        httpd_register_uri_handler(server, &led_off);

        // Mensaje de depuración
        ESP_LOGI(TAG, "Servidor web iniciado");
    }

    // Devuelve manejador del servidor
    return server;
}

// -----------------------------------------------------------------------------
// FUNCIÓN: wifi_init_softap
// CONFIGURA EL ESP32 COMO ACCESS POINT (PUNTO DE ACCESO)
// -----------------------------------------------------------------------------
static void wifi_init_softap(void)
{
    // Inicializa interfaz de red
    ESP_ERROR_CHECK(esp_netif_init());

    // Crea bucle de eventos por defecto
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Crea interfaz WiFi en modo AP
    esp_netif_create_default_wifi_ap();

    // Configuración WiFi por defecto
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    // Inicializa WiFi
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Configuración del Access Point
    wifi_config_t wifi_config = {

        .ap = {

            // Nombre de red
            .ssid = AP_SSID,

            // Longitud del SSID
            .ssid_len = strlen(AP_SSID),

            // Canal WiFi
            .channel = AP_CHANNEL,

            // Contraseña
            .password = AP_PASSWORD,

            // Máximo de conexiones
            .max_connection = AP_MAX_CONN,

            // Seguridad WPA/WPA2
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    // Si no hay contraseña -> red abierta
    if (strlen(AP_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    // Configura modo AP
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    // Aplica configuración
    ESP_ERROR_CHECK(
        esp_wifi_set_config(WIFI_IF_AP, &wifi_config)
    );

    // Inicia WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    // Mensajes en consola
    ESP_LOGI(TAG, "Modo AP iniciado");
    ESP_LOGI(TAG, "SSID: %s", AP_SSID);
    ESP_LOGI(TAG, "Password: %s", AP_PASSWORD);

    // Dirección IP del servidor
    ESP_LOGI(TAG, "Abre en el navegador: http://192.168.4.1");
}

// -----------------------------------------------------------------------------
// FUNCIÓN PRINCIPAL app_main
// -----------------------------------------------------------------------------
void app_main(void)
{
    // Inicializa memoria NVS
    esp_err_t ret = nvs_flash_init();

    // Si la memoria tiene errores:
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        // Borra memoria NVS
        ESP_ERROR_CHECK(nvs_flash_erase());

        // Reinicializa NVS
        ret = nvs_flash_init();
    }

    // Verifica errores
    ESP_ERROR_CHECK(ret);

    // Inicializa LEDs
    init_leds();

    // Configura WiFi en modo Access Point
    wifi_init_softap();

    // Inicia servidor web
    start_webserver();
}