#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <nvs_flash.h>
#include <string.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>

void app_task(void * args);
void pro_task(void * args);

static esp_err_t event_handler(void * ctx, system_event_t * event)
{
    return ESP_OK;
}

void app_main(void)
{
    esp_err_t nvs_status = nvs_flash_init();

    if (nvs_status == ESP_ERR_NVS_NO_FREE_PAGES)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    assert(xTaskCreatePinnedToCore(app_task, "testing task", 10000, NULL, 20, NULL, 1) == pdTRUE);
    assert(xTaskCreatePinnedToCore(pro_task, "wifi task", 4096, NULL, 2, NULL, 0) == pdTRUE);
}

void app_task(void * args)
{
    uint32_t start, end;

    while (true)
    {
        asm volatile ("rsr.ccount %0" : "=r"(start));

        for (uint32_t i = 0; i < 10000; i++)
                asm ("nop");

        asm volatile ("rsr.ccount %0" : "=r"(end));

        if (end > start && (end - start) / 240 > 500)
        {
            printf("%d us\n", (end - start) / 240);
        }

        for (uint32_t i = 0; i < 10000; i++)
                asm ("nop");
    }
}

void pro_task(void *args)
{
    const char * ssid = "test_ssid";
    const char * password = "test1234";

    tcpip_adapter_init();

    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&config));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t wifi_config;
    wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    wifi_config.ap.beacon_interval = 100;
    wifi_config.ap.channel = 7;
    wifi_config.ap.max_connection = 1;
    strcpy((char *)wifi_config.ap.ssid, ssid);
    strcpy((char *)wifi_config.ap.password, password);
    wifi_config.ap.ssid_hidden = 0;
    wifi_config.ap.ssid_len = 0;

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    while (true)
        vTaskDelay(100);

}