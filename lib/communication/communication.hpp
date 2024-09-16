#pragma once

#include <ctime>
#include <string_view>

#include "esp_wifi.h"

class Connection
{
public:
  esp_err_t InitWifi(const std::string_view ssid,
                     const std::string_view password);
  esp_err_t DeInitWifi();
  bool IsWifiConnected();

  esp_err_t StartupSntpSync();
  esp_err_t WaitForSntpSync();
  esp_err_t StopSntpSync();
  static void TimeSyncornizedCallback(timeval* tv);

private:
  static void WifiEventHandler(void* arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void* event_data);
  static void IpEventHandler(void* arg,
                             esp_event_base_t event_base,
                             int32_t event_id,
                             void* event_data);

private:
  inline static constexpr int MAX_CONNECT_RETRIES = 5;

  esp_netif_t* m_netif;
  esp_event_handler_instance_t m_wifi_event_handler;
  esp_event_handler_instance_t m_ip_event_handler;

  int m_connect_try_count = 0;

  bool m_is_wifi_connected = false;
};