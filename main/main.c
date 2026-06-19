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

SemaphoreHandle_t sem;

const uint8_t max_retries = 5;
uint8_t retries = 0;

void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
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


static void log_error_if_nonzero(const char *message, int error_code)
{
  if (error_code != 0) {
    printf("Last error %s: 0x%x", message, error_code);
  }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
  printf("Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
  esp_mqtt_event_handle_t event = event_data;
  esp_mqtt_client_handle_t client = event->client;
  int msg_id;

  switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
      printf("MQTT_EVENT_CONNECTED");
      msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
      printf("sent publish successful, msg_id=%d", msg_id);

      msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
      printf("sent subscribe successful, msg_id=%d", msg_id);

      msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
      printf("sent subscribe successful, msg_id=%d", msg_id);

      msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
      printf("sent unsubscribe successful, msg_id=%d", msg_id);
      break;
    case MQTT_EVENT_DISCONNECTED:
      printf("MQTT_EVENT_DISCONNECTED");
      break;

    case MQTT_EVENT_SUBSCRIBED:
      printf("MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
      msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
      printf("sent publish successful, msg_id=%d", msg_id);
      break;
    case MQTT_EVENT_UNSUBSCRIBED:
      printf("MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_PUBLISHED:
      printf("MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_DATA:
      printf("MQTT_EVENT_DATA");
      printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
      printf("DATA=%.*s\r\n", event->data_len, event->data);
      break;
    case MQTT_EVENT_ERROR:
      printf("MQTT_EVENT_ERROR");
      if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
        log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
        log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
        printf("Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

      }
      break;
    default:
      printf("Other event id:%d", event->event_id);
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

  esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &wifi_any_evh);

  esp_event_handler_instance_t got_ip_evh;
  esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &got_ip_evh);

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
