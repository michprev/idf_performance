#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>

#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>

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

    const uint16_t UDP_PORT = 4789;
    const uint16_t TCP_PORT = 4821;

    int udp_server_fd;
    int tcp_server_fd;
    int tcp_client_fd;

    /*-------------------------------------------------------------------------------------*/

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

    /*-------------------------------------------------------------------------------------*/

    tcp_server_fd = socket(AF_INET, SOCK_STREAM, 0);

    assert(tcp_server_fd >= 0);

    struct sockaddr_in tcp_server_address;
    tcp_server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    tcp_server_address.sin_port = htons(TCP_PORT);
    tcp_server_address.sin_family = AF_INET;

    assert(fcntl(tcp_server_fd, F_SETFL, O_NONBLOCK) == 0);

    assert(bind(tcp_server_fd, (struct sockaddr *) &tcp_server_address, sizeof(tcp_server_address)) >= 0);

    assert(listen(tcp_server_fd, 1) >= 0);

    /*-------------------------------------------------------------------------------------*/

    udp_server_fd = socket(AF_INET, SOCK_DGRAM, 0);

    assert(udp_server_fd >= 0);

    struct sockaddr_in udp_server_address;
    udp_server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    udp_server_address.sin_port = htons(UDP_PORT);
    udp_server_address.sin_family = AF_INET;

    assert(fcntl(udp_server_fd, F_SETFL, O_NONBLOCK) == 0);

    assert(bind(udp_server_fd, (struct sockaddr *) &udp_server_address, sizeof(udp_server_address)) >= 0);

    /*-------------------------------------------------------------------------------------*/

    while (true)
        vTaskDelay(100);

}