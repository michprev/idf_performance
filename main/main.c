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
    assert(xTaskCreatePinnedToCore(pro_task, "wifi task", 8192, NULL, 2, NULL, 0) == pdTRUE);
}

void app_task(void * args)
{
    uint32_t start, end;

    TaskStatus_t task_status[15];
    uint32_t times[15];

    while (true)
    {
        int tasks = uxTaskGetSystemState(task_status, 15, NULL);
        for (int i = 0; i < tasks; i++)
            times[i] = task_status[i].ulRunTimeCounter;

        asm volatile ("rsr.ccount %0" : "=r"(start));

        for (uint32_t i = 0; i < 10000; i++)
                asm ("nop");

        asm volatile ("rsr.ccount %0" : "=r"(end));

        if (end > start && (end - start) / 240 > 500)
        {
            int tasks = uxTaskGetSystemState(task_status, 15, NULL);
            for (int i = 0; i < tasks; i++)
            {
                uint32_t delta = task_status[i].ulRunTimeCounter >= times[i] ?
                                 task_status[i].ulRunTimeCounter - times[i] :
                                 (0xffffffffu - times[i]) + task_status[i].ulRunTimeCounter;

                printf("[%d][%s] %u %u %u us\n", task_status[i].xCoreID, task_status[i].pcTaskName,
                       times[i], task_status[i].ulRunTimeCounter, delta / 240);
            }

            printf("%d us\n\n", (end - start) / 240);
        }

        for (uint32_t i = 0; i < 10000; i++)
                asm ("nop");
    }
}

void pro_task(void *args)
{
    const char * SSID = "test_ssid";
    const char * PASSWORD = "test1234";

    const uint16_t UDP_PORT = 4789;
    const uint16_t TCP_PORT = 4821;

    int udp_server_fd;
    int tcp_server_fd;
    int tcp_client_fd;

    struct sockaddr_in tcp_client_address;
    struct sockaddr_in udp_client_socket;

    uint32_t tcp_client_address_length;
    uint32_t udp_client_socket_length;

    const uint16_t TCP_BUFFER_LENGTH = 1024;
    uint8_t tcp_buffer[TCP_BUFFER_LENGTH];

    const uint16_t UDP_BUFFER_LENGTH = 1024;
    uint8_t udp_buffer[UDP_BUFFER_LENGTH];

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
    strcpy((char *)wifi_config.ap.ssid, SSID);
    strcpy((char *)wifi_config.ap.password, PASSWORD);
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

    // wait for TCP client to connect

    do
    {
        tcp_client_fd = accept(tcp_server_fd, (struct sockaddr *) &tcp_client_address, &tcp_client_address_length);
        vTaskDelay(10 / portTICK_RATE_MS);
    } while (tcp_client_fd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK));

    assert(tcp_client_fd >= 0);

    printf("[TCP] client connected\n");

    /*-------------------------------------------------------------------------------------*/

    while (true)
    {
        // in case that TCP client reconnected
        int tmp_fd = accept(tcp_server_fd, (struct sockaddr *) &tcp_client_address, &tcp_client_address_length);
        if (tmp_fd >= 0)
        {
            tcp_client_fd = tmp_fd;
            printf("[TCP] client reconnected\n");
        }

        // receive TCP packets
        ssize_t tcp_length;
        if ((tcp_length = recv(tcp_client_fd, tcp_buffer, TCP_BUFFER_LENGTH, MSG_DONTWAIT)) > 0)
        {
            tcp_buffer[tcp_length] = '\0';
            printf("[TCP]: %s\n", tcp_buffer);
        }

        // receive UDP datagrams
        ssize_t udp_length;
        if ((udp_length = recvfrom(udp_server_fd, udp_buffer, UDP_BUFFER_LENGTH, 0,
                                   (struct sockaddr *) &udp_client_socket, &udp_client_socket_length)) > 0)
        {
            udp_buffer[udp_length] = '\0';
            //printf("[UDP]: %s\n", udp_buffer);
        }

        vTaskDelay(20 / portTICK_RATE_MS);
    }
}