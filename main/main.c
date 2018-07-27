#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <nvs_flash.h>

void app_task(void * args);
void pro_task(void * args);

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
            assert(0);
        }

        for (uint32_t i = 0; i < 10000; i++)
                asm ("nop");
    }
}

void pro_task(void *args)
{
    while (true)
        vTaskDelay(100);

}