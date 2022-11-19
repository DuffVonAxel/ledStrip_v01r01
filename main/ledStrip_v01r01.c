
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "lwip/err.h"
#include "lwip/sys.h"

/* RTOS */
#define CONFIG_FREERTOS_HZ 100									// Definicao da Espressif. Escala de tempo base (vTaskDelay).

/* WiFi */
#define CONFIG_ESP_WIFI_SSID            "LedStrip"              // SSID do AP.
#define CONFIG_ESP_WIFI_PASSWORD        "abcdefgh"              // Senha do AP.
#define CONFIG_ESP_WIFI_CHANNEL         1                       // Canal a ser utilizado (BR).
#define CONFIG_ESP_MAX_STA_CONN         4                       // Numero maximo de conexoes.

/* Valor RGB */
unsigned char valorR=0, valorG=0, valorB=0;                     // Variavel Global com valor.

// Pelo SDKCONFIG
#define CONFIG_ESP32_WIFI_STATIC_RX_BUFFER_NUM       10         // Define quantas pilhas estaticas (min).
#define CONFIG_ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM      32         // Define quantas pilhas dinamicas (max).
#define CONFIG_ESP32_WIFI_TX_BUFFER_TYPE             1          // Define o tipo de Buffer da Tx do WiFi.

static const char *TAG = "LedStrip";                            // Nome da Task.

/* Aplicacao */
char __convVlr(char valor)                                     // Sub rotina de conversao de valores.
{
    char volta=0;                                               // Variavel Local criada e inicializada (obrigatorio).
    if(valor >= '0' && valor <= '9') volta=(valor-48);          // Calcula valores numericos.
    if(valor >= 'A' && valor <= 'F') volta=(valor-55);          // Calcula alfa maiusculas.
    if(valor >= 'a' && valor <= 'f') volta=(valor-87);          // Calcula alfa minusculas.
    return(volta);                                              // Retorna o valor calculado.
}

void hex2dec(char *vlrHex)                                      // Converte Hexadecimal(0x00~0xFF) em decimal(0~255).
{
    valorR = (__convVlr(vlrHex[3])*16) + __convVlr(vlrHex[4]);  // Calcula o valor de R.
    valorG = (__convVlr(vlrHex[5])*16) + __convVlr(vlrHex[6]);  // Calcula o valor de G.
    valorB = (__convVlr(vlrHex[7])*16) + __convVlr(vlrHex[8]);  // Calcula o valor de B.
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) 
    {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Estacao "MACSTR" entrou, AID=%d", MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) 
    {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Estacao "MACSTR" saiu, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .ssid_len = strlen(CONFIG_ESP_WIFI_SSID),
            .channel = CONFIG_ESP_WIFI_CHANNEL,
            .password = CONFIG_ESP_WIFI_PASSWORD,
            .max_connection = CONFIG_ESP_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            // .pmf_cfg = {
            //         .required = 0,
            // },
        },
    };

    // wifi_pmf_config_t wifi_pmf = {
    //     {.required = 0}
    // };

    if (strlen(CONFIG_ESP_WIFI_PASSWORD) == 0) 
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

	esp_wifi_connect();
    ESP_LOGI(TAG, "Soft AP Ok: SSID:%s Pass:%s Canal:%d", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD, CONFIG_ESP_WIFI_CHANNEL);
}

void nvs_init(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Modo: AP");
}

esp_err_t get_handler(httpd_req_t *req)
{
    const char resp[] = "<!DOCTYPE html><html><head><title>ESP-IDF Fita Led</title>\
<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head>\
<body><style>*{font-family: Verdana;text-align: center;}</style>\
<h2>Color Picker</h2><form action=\"\"><label for=\"rgb\"><h3>Cor:</h3></label>\
<input type=\"color\" id=\"rgb\" name=\"rgb\" value=\"#0000ff\"><br><br><input type=\"submit\">\
</form></body></html>";

    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t get_handler_rgb(httpd_req_t *req)
{
    /**/
    const char resp[] = "<!DOCTYPE html><html><head><title>ESP-IDF Fita Led</title>\
<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head>\
<body><style>*{font-family: Verdana;text-align: center;}</style>\
<h2>Color Picker</h2><form action=\"\" method=\"get\"><label for=\"rgb\"><h3>Cor:</h3></label>\
<input type=\"color\" id=\"rgb\" name=\"rgb\" value=\"#0000ff\"><br><br><input type=\"submit\">\
</form></body></html>";

    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    /**/

    // Le a linha da URI e pega o host.
    char *buf;
    size_t buf_len;
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1)
    {
        buf = malloc(buf_len);
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK)
        {
            ESP_LOGI(TAG, "Host: %s", buf);
        }
        free(buf);
    }

    // Le a linha da URI e pega o(s) parametro(s).
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Dado na URL: %s", buf);
            char param[32]; // era 32
            if (httpd_query_key_value(buf, "rgb", param, sizeof(param)) == ESP_OK) 
            {
                ESP_LOGI(TAG, "Valor RGB= %s", param);
                hex2dec(param);
                printf("Valor R= %d\n", valorR);
                printf("Valor G= %d\n", valorG);
                printf("Valor B= %d\n", valorB);
            }
        }
        free(buf);
    }
    get_handler(req);
    return ESP_OK;
}

/* Manipulador da estrutura da URI para metodo GET /uri */
httpd_uri_t uri_get = 
    {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_handler,
    .user_ctx = NULL
    };

httpd_uri_t uri_get_rgb = 
    {
    .uri = "/cor",
    .method = HTTP_GET,
    .handler = get_handler_rgb,
    .user_ctx = NULL
    };

httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &uri_get);
        // httpd_register_uri_handler(server, &uri_get_saidas);
        // httpd_register_uri_handler(server, &uri_get_entradas);
        httpd_register_uri_handler(server, &uri_get_rgb);
    }
    return server;
}

void stop_webserver(httpd_handle_t server)
{
    if (server)
    {
        httpd_stop(server);
    }
}

void wifi_init( void )
{
    nvs_init();
    wifi_init_softap();
    // esp32Ap();
    // server_initiation();
    start_webserver();
}

/* Definicoes para os LEDs */
typedef struct                                                  // Contem as variaveis do RGB (Tipo char)
{
    uint8_t r;                                                  // Variavel R (Red = Vermelho)
    uint8_t g;                                                  // Variavel G (Green = Verde)
    uint8_t b;                                                  // Variavel B (Blue = Azul)
} color_t;

#define LED_RED_PIN         33                                  // Numero do pino do Led R.
#define LED_GREEN_PIN       32                                  // Numero do pino do Led G.
#define LED_BLUE_PIN        25                                  // Numero do pino do Led B.

ledc_timer_config_t ledc_timer =                                // Estrutura de configuracao do Timer do Led (Unico).
{
    .speed_mode = LEDC_LOW_SPEED_MODE,                          // Baixa velocidade de atualizacao (1kHz). Sem 'Fade'.
    .timer_num  = LEDC_TIMER_0,                                 // Timer a ser utilizado.
    .duty_resolution = LEDC_TIMER_13_BIT,                       // Resolucao do contador.
    .freq_hz = 1000,                                            // Frequencia do PWM.
    .clk_cfg = LEDC_AUTO_CLK                                    // Configuracao interna do clock.
};
ledc_channel_config_t ledc_channel[3];                          // Vetor para criar 3 instancias da estrutura de configuracao.

static void ledRGBInit(void)                                    // Inicializa
{
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));            // Verifica possiveis erros na configuracao.
    ledc_channel[0].channel = LEDC_CHANNEL_0;                   // Nomeia o canal.
    ledc_channel[0].gpio_num = LED_RED_PIN;                     // Configura o pino do Led.
    ledc_channel[1].channel = LEDC_CHANNEL_1;                   // Nomeia o canal.
    ledc_channel[1].gpio_num = LED_GREEN_PIN;                   // Configura o pino do Led.
    ledc_channel[2].channel = LEDC_CHANNEL_2;                   // Nomeia o canal.
    ledc_channel[2].gpio_num = LED_BLUE_PIN;                    // Configura o pino do Led.
    for (int i = 0; i < 3; i++)                                 // Laco para copiar os dados nas instancias da estrutura de configuracao.
    {   
        ledc_channel[i].speed_mode = LEDC_LOW_SPEED_MODE;       // Baixa taxa de atualizacao (Adequado para usar na Task).
        ledc_channel[i].timer_sel = LEDC_TIMER_0;               // Seleciona o Timer 0 (Adequado para usar na Task).
        ledc_channel[i].intr_type = LEDC_INTR_DISABLE;          // Desliga interrupcao (Somente funcao 'Fade')(Adequado para usar na Task).
        ledc_channel[i].duty = 0;                               // Inicia o duty desligado.
        ledc_channel[i].hpoint = 0;                             // Valor para o ponto de alta velocidade (Nao vai ser usado).
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel[i])); // Testa e configura cada estrutura.
    }
}

static void setColor(color_t color)
{
    // uint32_t red_duty   = 8191 * color.r / 255; // cte=(8191/255)*cor ... cte=32,1215...*cor
    // uint32_t green_duty = 8191 * color.g / 255;
    // uint32_t blue_duty  = 8191 * color.b / 255; 

    uint32_t red_duty   = 32 * color.r;                         // Converte o valor em duty cicle do PWM.
    uint32_t green_duty = 32 * color.g;                         // Converte o valor em duty cicle do PWM.
    uint32_t blue_duty  = 32 * color.b;                         // Converte o valor em duty cicle do PWM.

    // Bloco de verificacao e configuracao da: velocidade, canal e duty.
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, red_duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, green_duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, blue_duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2));
}

void app_main(void)
{
    // const color_t red = { .r = 255, .g = 0, .b = 0 };
    // const color_t green = { .r = 0, .g = 255, .b = 0 };
    // const color_t blue = { .r = 0, .g = 0, .b = 255 };
    // const color_t yellow = { .r = 255, .g = 255, .b = 0 };
    // const color_t purple = { .r = 128, .g = 0, .b = 128 };
    // const color_t silver = setColor
    // color_t sample_colors[6] = { red, green, blue, yellow, purple, silver };

    ledRGBInit();
    
    wifi_init();

    while (1) 
    {
        // for (int i = 0; i < 6; i++)
        // {
        //     printf("Color %d\r\n", i);
        //     setColor(sample_colors[i]);
        //     vTaskDelay(pdMS_TO_TICKS(2000));
        // }
        color_t corAtual = { .r = valorR, .g = valorG, .b = valorB };
        setColor(corAtual);

        vTaskDelay(10);
    }
}

/* Link:
https://embeddedexplorer.com/esp32-pwm-using-ledc-peripheral/
https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html
*/

/*
_LED
VAAV
mczd

Vm=1.6V
Vd=1.9V(+-)
Az=?
*/

/* Erro:
 httpd_parse: parse_block: request URI/header too long
 httpd_txrx: httpd_resp_send_err: 431 Request Header Fields Too Large - Header fields are too long for server to interpr
et

SDKCONFIG:
CONFIG_HTTPD_MAX_REQ_HDR_LEN=512
CONFIG_HTTPD_MAX_URI_LEN=1024  //  era 512
*/

/* Aviso:
httpd_txrx: httpd_sock_err: error in recv : 104
Eh normal, o cliente enviou comando RST antes do processo Fin
*/