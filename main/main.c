#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <freertos/idf_additions.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_system.h>
#include <esp_random.h>
#include <esp_task.h>

#include <nvs_flash.h>
#include <mqtt_client.h>

#include "credentials.h"

#define BROKER_IP ""
#define BROKER_PORT "1883"
#define MQTT_TOPIC "data/temperature"

SemaphoreHandle_t sem;

const uint8_t max_retries = 5;
uint8_t retries = 0;

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  if (event_base == WIFI_EVENT) {
    if (event_id == WIFI_EVENT_STA_START) {
      esp_wifi_connect();

    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
      printf("Error al conectar, intento %d/%d\n", retries, max_retries);

      if (retries < max_retries) {
        esp_wifi_connect();
        retries++;

      } else {
        xSemaphoreGive(sem);
      }
    }

  } else if (event_base == IP_EVENT) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
      ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
      printf("IP obtenida: " IPSTR "\n", IP2STR(&event->ip_info.ip));

      xSemaphoreGive(sem);
    }
  }
}

static void mqtt_event_handler(void *handler_args, 
    esp_event_base_t base, int32_t event_id, void *event_data) {

  esp_mqtt_event_t *event = event_data;

  switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
      esp_mqtt_client_subscribe(event->client, MQTT_TOPIC, 0);
      break;

    case MQTT_EVENT_DISCONNECTED:
      printf("desconectado del broker, reconectando...\n");
      esp_mqtt_client_reconnect(event->client);
      break;

    case MQTT_EVENT_DATA: {
      uint32_t *temp = (uint32_t*)event->data;
      printf("Recived data %.*s: %lu\r\n", event->topic_len, event->topic, *temp);
      break;
    }

    default:
      break;
  }
}

void app_main() {
  sem = xSemaphoreCreateBinary();

  nvs_flash_init();
  esp_netif_init();

  esp_event_loop_create_default();
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&cfg);

  esp_event_handler_instance_t wifi_any_evh;

  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &wifi_any_evh);

  esp_event_handler_instance_t got_ip_evh;
  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &got_ip_evh);

  esp_wifi_set_mode(WIFI_MODE_STA);
  wifi_config_t wifi_config = {
    .sta = {
      .ssid = WIFI_AP_SSID,
      .password = WIFI_AP_PASS,
      .threshold.authmode = WIFI_AUTH_WPA2_PSK,
      .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
      .sae_h2e_identifier = "",
    },
  };
  esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

  esp_wifi_start();

  xSemaphoreTake(sem, portMAX_DELAY);

  if (retries >= max_retries) {
    printf("Error al conectarse al AP %s\n, reiniciando...\n", WIFI_AP_SSID);
    sleep(5);
    esp_restart();
  }
  printf("Conectado con exito al AP %s\n", WIFI_AP_SSID);

  // Publisher MQTT
  esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = "mqtt://"BROKER_IP":"BROKER_PORT,
  };

  esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);

  esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
  esp_mqtt_client_start(client);
}
