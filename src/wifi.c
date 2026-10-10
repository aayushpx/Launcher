#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <stdio.h>
#include <string.h>

#include "networking.h"
#include "wifi_config.h"

wifi_mode_type wifi_mode = SCAN;

#define TAG "Wifi"

static bool wifi_initialised = false;

bool wifi_connected(void)
{
    return network_event_group != NULL &&
           (xEventGroupGetBits(network_event_group) & CONNECTED_BIT);
}

bool ap_started(void)
{
    return network_event_group != NULL &&
           (xEventGroupGetBits(network_event_group) & AP_STARTED);
}

void init_wifi(wifi_mode_type mode)
{
    if (network_event_group == NULL)
        network_event_group = xEventGroupCreate();

    if (network_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create network event group");
        return;
    }

    if (wifi_initialised && wifi_mode == mode && wifi_connected())
        return;

    xEventGroupClearBits(network_event_group,
                         AUTH_FAIL | CONNECTED_BIT);

    if (wifi_initialised)
        ESP_ERROR_CHECK(esp_wifi_stop());

    wifi_mode = mode;

    if (network_interface == NULL) {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        network_interface = esp_netif_create_default_wifi_sta();
        network_interface_ap = esp_netif_create_default_wifi_ap();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        ESP_ERROR_CHECK(esp_event_handler_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(
            IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    }

    if (mode == ACCESS_POINT) {
        wifi_config_t wifi_config = {
            .ap = {
                .ssid = "ESP32",
                .ssid_len = 5,
                .channel = 3,
                .password = "",
                .max_connection = 8,
                .authmode = WIFI_AUTH_OPEN
            }
        };

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(
            WIFI_IF_AP, &wifi_config));
    } else {
        wifi_config_t wifi_config = {0};

        snprintf((char *)wifi_config.sta.ssid,
                 sizeof(wifi_config.sta.ssid), "%s", WIFI_SSID);
        snprintf((char *)wifi_config.sta.password,
                 sizeof(wifi_config.sta.password), "%s", WIFI_PASSWORD);

        wifi_config.sta.channel = 0;

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(
            WIFI_IF_STA, &wifi_config));
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    wifi_initialised = true;
}

void wifi_disconnect(void)
{
    if (ap_started()) {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        while (ap_started())
            vTaskDelay(pdMS_TO_TICKS(100));
    } else if (wifi_connected()) {
        ESP_ERROR_CHECK(esp_wifi_disconnect());
        while (wifi_connected())
            vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/*
 * Connect to the configured Wi-Fi network.
 * The lecturer's version also draws its own Wi-Fi demo screen.
 * This version leaves the display to the Launcher application.
 */
void wifi_connect(int onlyconnect)
{
    (void)onlyconnect;

    init_wifi(STATION);

    const TickType_t timeout = pdMS_TO_TICKS(15000);
    TickType_t start = xTaskGetTickCount();

    while (!wifi_connected() &&
           !(xEventGroupGetBits(network_event_group) & AUTH_FAIL) &&
           (xTaskGetTickCount() - start) < timeout) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (!wifi_connected())
        ESP_LOGW(TAG, "Wi-Fi connection did not complete");
}
