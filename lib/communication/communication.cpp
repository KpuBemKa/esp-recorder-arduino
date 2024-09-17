#include "communication.hpp"

#include <algorithm>
#include <ctime>

#include "Network.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "settings.hpp"

const char TAG[] = "TCP_COM";

#if DEBUG_COM
#define LOG_I(...) ESP_LOGI(TAG, __VA_ARGS__)
#else
#define LOG_I(...)
#endif

#define LOG_E(...) ESP_LOGE(TAG, __VA_ARGS__)
#define LOG_W(...) ESP_LOGW(TAG, __VA_ARGS__)

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

constexpr int CONNECT_WAIT_TIMEOUT_MS = 2'000;

esp_err_t
Connection::InitWifi(const std::string_view ssid,
                     const std::string_view password)
{
  LOG_I("Initializing Wi-Fi...");

  // EventGroupHandle_t wifi_event_group = xEventGroupCreate();

  esp_err_t result = esp_netif_init();
  if (result != ESP_OK) {
    LOG_E("%s:%d | Error initializing netif: %s",
          __FILE__,
          __LINE__,
          esp_err_to_name(result));
    return result;
  }

  // Create the event loop
  result = esp_event_loop_create_default();
  if (result != ESP_ERR_INVALID_STATE && result != ESP_OK) {
    LOG_E("%s:%d | Error creating event loop : %s",
          __FILE__,
          __LINE__,
          esp_err_to_name(result));
    return result;
  }

  // Create the netif
  m_netif = esp_netif_create_default_wifi_sta();

  // Initialize the Wi-Fi
  const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  result = esp_wifi_init(&cfg);
  if (result != ESP_OK) {
    LOG_E("%s:%d | Error initializing Wi-Fi : %s",
          __FILE__,
          __LINE__,
          esp_err_to_name(result));
    return result;
  }

  result = esp_event_handler_instance_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               WifiEventHandler,
                                               this,
                                               &m_wifi_event_handler);
  result |= esp_event_handler_instance_register(
    IP_EVENT, IP_EVENT_STA_GOT_IP, IpEventHandler, this, &m_ip_event_handler);

  wifi_config_t wifi_config = {
        .sta = {
            // .ssid = 0,
            // .password = 0,
            .ssid = "MARS",
            .password = "789456123",
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold = { .authmode = wifi_auth_mode_t::WIFI_AUTH_WPA2_PSK },
        },
    };
  // std::copy_n(ssid.data(), ssid.size(), wifi_config.sta.ssid);
  // std::copy_n(password.data(), password.size(), wifi_config.sta.password);

  // Set Wi-Fi in station mode
  result = esp_wifi_set_mode(wifi_mode_t::WIFI_MODE_STA);
  if (result != ESP_OK) {
    LOG_E("%s:%d | Error setting Wi-Fi in AP mode: %s",
          __FILE__,
          __LINE__,
          esp_err_to_name(result));
    return result;
  }

  // Apply the settings
  result = esp_wifi_set_config(wifi_interface_t::WIFI_IF_STA, &wifi_config);
  if (result != ESP_OK) {
    LOG_E("%s:%d | Error configuring Wi-Fi settings: %s",
          __FILE__,
          __LINE__,
          esp_err_to_name(result));
    return result;
  }

  // Start Wi-Fi
  result = esp_wifi_start();
  if (result != ESP_OK) {
    LOG_E("%s:%d | Error starting Wi-Fi: %s",
          __FILE__,
          __LINE__,
          esp_err_to_name(result));
    return result;
  }

  LOG_I("Wi-Fi has been started. Waiting for connection... %d",
        m_is_wifi_connected);

  // Wait `CONNECT_WAIT_TIMEOUT_MS` ms until Wi-Fi is connected
  const TickType_t wait_end =
    xTaskGetTickCount() + pdMS_TO_TICKS(CONNECT_WAIT_TIMEOUT_MS);
  while (!IsWifiConnected() && xTaskGetTickCount() <= wait_end) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }

  result = StartupSntpSync();
  if (result != ESP_OK) {
    LOG_E("%s:%d | Error starting SNTP service: %s",
          __FILE__,
          __LINE__,
          esp_err_to_name(result));
    return result;
  }

  return ESP_OK;
}

esp_err_t
Connection::DeInitWifi()
{
  StopSntpSync();

  esp_err_t result = esp_wifi_stop();
  if (result != ESP_OK) {
    LOG_E("%s:%d | Error stopping Wi-Fi: %s",
          __FILE__,
          __LINE__,
          esp_err_to_name(result));
    return result;
  }

  result = esp_event_handler_instance_unregister(
    WIFI_EVENT, ESP_EVENT_ANY_ID, m_wifi_event_handler);
  result |= esp_event_handler_instance_unregister(
    IP_EVENT, ESP_EVENT_ANY_ID, m_ip_event_handler);
  if (result != ESP_OK) {
    LOG_E("%s:%d | Error unregistering Wi-Fi event handler: %s",
          __FILE__,
          __LINE__,
          esp_err_to_name(result));
    return result;
  }

  esp_netif_destroy(m_netif);

  /* Note: Deinitialization is not supported yet */
  // result = esp_netif_deinit();
  // if (result != ESP_OK) {
  //   LOG_E("%s:%d | Error de-initializing netif: %s",
  //         __FILE__,
  //         __LINE__,
  //         esp_err_to_name(result));
  //   return result;
  // }

  return ESP_OK;
}

bool
Connection::IsWifiConnected()
{
  return m_is_wifi_connected;
}

esp_err_t
Connection::StartupSntpSync()
{
  LOG_I("Syncronizing system time...");

  const esp_sntp_config_t sntp_config{
    .smooth_sync = false,
    .server_from_dhcp = false,
    .wait_for_sync = true,
    .start = true,
    .sync_cb = TimeSyncornizedCallback,
    .renew_servers_after_new_IP = false,
    .ip_event_to_renew = IP_EVENT_STA_GOT_IP,
    .index_of_first_server = 0,
    .num_of_servers = 1,
    .servers = { "pool.ntp.org" },
  };

  esp_err_t esp_result = esp_netif_sntp_init(&sntp_config);
  if (esp_result != ESP_OK) {
    LOG_E("%s:%d | Error initializing SNTP: %s",
          __FILE__,
          __LINE__,
          esp_err_to_name(esp_result));
    return esp_result;
  }

  return esp_result;
}

esp_err_t Connection::WaitForSntpSync()
{
  return esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10'000));
}

esp_err_t
Connection::StopSntpSync()
{
  esp_netif_sntp_deinit();

  return ESP_OK;
}

void
Connection::TimeSyncornizedCallback(timeval* tv)
{
  LOG_I("System time has been syncronized with SNTP.");

  const std::tm* dt = localtime(&tv->tv_sec);
  LOG_I("Current system time: %4d.%02d.%02d %02d:%02d:%02d",
        dt->tm_year + 1900,
        dt->tm_mon,
        dt->tm_mday,
        dt->tm_hour,
        dt->tm_min,
        dt->tm_sec);
}

void
Connection::WifiEventHandler(void* arg,
                             esp_event_base_t event_base,
                             int32_t event_id,
                             void* event_data)
{
  Connection* connection_ptr = reinterpret_cast<Connection*>(arg);

  switch (event_id) {
    case wifi_event_t::WIFI_EVENT_STA_CONNECTED: {
      // wifi_event_sta_connected_t* event =
      // (wifi_event_sta_connected_t*)event_data;
      // connection_ptr->m_is_wifi_connected = true;
      LOG_I("Connected");
      break;
    }

    case wifi_event_t::WIFI_EVENT_STA_DISCONNECTED: {
      connection_ptr->m_is_wifi_connected = false;
      if (connection_ptr->m_connect_try_count <
          connection_ptr->MAX_CONNECT_RETRIES) {
        esp_wifi_connect();
        connection_ptr->m_connect_try_count++;
        LOG_W("Connection failed. Retrying...");
      } else {
        LOG_E("Failed to connect.");
      }
      break;
    }

    case wifi_event_t::WIFI_EVENT_STA_START: {
      LOG_I("Station has been started. Connecting...");
      esp_wifi_connect();
      break;
    }

    default:
      break;
  }
}

void
Connection::IpEventHandler(void* arg,
                           esp_event_base_t event_base,
                           int32_t event_id,
                           void* event_data)
{
  Connection* connection_ptr = reinterpret_cast<Connection*>(arg);

  switch (event_id) {
    case ip_event_t::IP_EVENT_STA_GOT_IP: {
      ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
      connection_ptr->m_is_wifi_connected = true;
      LOG_I("Connected. Acquired IP:" IPSTR, IP2STR(&event->ip_info.ip));
      break;
    }

    default:
      break;
  }
}
