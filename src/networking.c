#include <esp_system.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_log.h>
#include "mqtt_client.h"
#include "networking.h"
#include "event_log.h"
#include "mpu6050.h"
#include "wifi_config.h"

EventGroupHandle_t network_event_group;
char network_event[64];
#define TAG "Networking"
int bg_col = 0;
esp_netif_t *network_interface = NULL;
esp_netif_t *network_interface_ap = NULL;

static mqtt_callback_type mqtt_callback = NULL;
static bool mqtt_is_connected = false;

bool mqtt_connected(void);

esp_mqtt_client_handle_t mqtt_client = NULL;

bool mqtt_connected(void)
{
    return mqtt_is_connected;
}

static void set_event_message(const char *s)
{
  snprintf(network_event, sizeof(network_event), "%s\n", s);
}

void set_mqtt_callback(mqtt_callback_type callback)
{
  mqtt_callback = callback;
}

void event_handler(void *arg, esp_event_base_t event_base,
    int32_t event_id, void *event_data)
{
  if (event_base == WIFI_EVENT) {
    switch (event_id) {
      case WIFI_EVENT_STA_START:
        set_event_message("WiFi started");
        xEventGroupClearBits(network_event_group,
            AUTH_FAIL | CONNECTED_BIT);
        if (wifi_mode == STATION)
          esp_wifi_connect();
        break;

      case WIFI_EVENT_STA_CONNECTED:
        set_event_message("WiFi connected");
        break;

      case WIFI_EVENT_STA_DISCONNECTED: {
                                          wifi_event_sta_disconnected_t *event =
                                            (wifi_event_sta_disconnected_t *)event_data;

                                          ESP_LOGW(TAG,
                                              "WiFi disconnected: reason=%d, SSID=%s",
                                              event->reason, WIFI_SSID);
                                          set_event_message("WiFi disconnected");
                                          xEventGroupClearBits(network_event_group, CONNECTED_BIT);

                                          if (event->reason == WIFI_REASON_AUTH_FAIL ||
                                              event->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT) {
                                            set_event_message("Authentication failed");
                                            xEventGroupSetBits(network_event_group, AUTH_FAIL);
                                          }
                                          break;
                                        }

      case WIFI_EVENT_SCAN_DONE:
                                        set_event_message("WiFi scan complete");
                                        break;

      case WIFI_EVENT_AP_START:
                                        set_event_message("Access point started");
                                        xEventGroupSetBits(network_event_group, AP_STARTED);
                                        break;

      case WIFI_EVENT_AP_STOP:
                                        set_event_message("Access point stopped");
                                        xEventGroupClearBits(network_event_group, AP_STARTED);
                                        break;

      default:
                                        break;
    }
  } else if (event_base == IP_EVENT) {
    switch (event_id) {
      case IP_EVENT_STA_GOT_IP:
        set_event_message("Got station IP");
        xEventGroupSetBits(network_event_group, CONNECTED_BIT);
        break;

      case IP_EVENT_AP_STAIPASSIGNED:
        set_event_message("Access point assigned IP");
        xEventGroupSetBits(network_event_group, CONNECTED_BIT);
        break;

      case IP_EVENT_ETH_GOT_IP:
        set_event_message("Ethernet got IP");
        xEventGroupSetBits(network_event_group, CONNECTED_BIT);
        break;

      default:
        break;
    }

  }
}


static void mqtt_event_handler(void *arg, esp_event_base_t event_base,
    int32_t event_id, void *event_data)
{
  esp_mqtt_event_handle_t event = event_data;

  switch (event_id) {
    case MQTT_EVENT_CONNECTED:
      mqtt_is_connected = true;
      event_log_add("MQTT CONNECTED");
      set_event_message("MQTT connected");
      break;

    case MQTT_EVENT_DISCONNECTED:
      mqtt_is_connected = false;
      event_log_add("MQTT DISCONNECTED");
      set_event_message("MQTT disconnected");
      break;
    case MQTT_EVENT_SUBSCRIBED:
      set_event_message("MQTT subscribed");
      break;
    case MQTT_EVENT_PUBLISHED:
      set_event_message("MQTT published");
      break;
    case MQTT_EVENT_DATA:
      set_event_message("MQTT data received");
      break;
    case MQTT_EVENT_ERROR:
      set_event_message("MQTT error");
      break;
    default:
      break;
  }

  if (mqtt_callback != NULL)
    mqtt_callback(event_id, event);
}

void mqtt_connect(mqtt_callback_type callback)
{
  char client_name[32];

  if (mqtt_client != NULL)
    mqtt_disconnect();

  srand((unsigned int)esp_timer_get_time());
  snprintf(client_name, sizeof(client_name), "esp32_%d", rand() % 1000);

  esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = "mqtt://mqtt.webhop.org",
    .credentials.client_id = client_name
  };

  wifi_connect(1);

  if (network_event_group != NULL &&
      (xEventGroupGetBits(network_event_group) & CONNECTED_BIT)) {
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(
        mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    set_mqtt_callback(callback);
    esp_mqtt_client_start(mqtt_client);
  }
}


int mqtt_publish_telemetry(const mpu6050_reading_t *reading)
{
  if (reading == NULL || mqtt_client == NULL || !mqtt_is_connected) {
    return -1;
  }

  char payload[192];

  int len = snprintf(
      payload,
      sizeof(payload),
      "{\"accel_x\":%.2f,\"accel_y\":%.2f,\"accel_z\":%.2f,"
      "\"gyro_x\":%.1f,\"gyro_y\":%.1f,\"gyro_z\":%.1f,"
      "\"temperature_c\":%.1f}",
      reading->accel_x,
      reading->accel_y,
      reading->accel_z,
      reading->gyro_x,
      reading->gyro_y,
      reading->gyro_z,
      reading->temperature_c
      );

  if (len < 0 || len >= sizeof(payload)) {
    return -1;
  }

  return esp_mqtt_client_publish(
      mqtt_client,
      "launcher/telemetry/neo-s3-01",
      payload,
      len,
      0,
      0
      );
}

void mqtt_disconnect(void)
{
  if (mqtt_client != NULL) {
    esp_mqtt_client_stop(mqtt_client);
    esp_mqtt_client_destroy(mqtt_client);
    mqtt_client = NULL;
    mqtt_callback = NULL;
  }
}
