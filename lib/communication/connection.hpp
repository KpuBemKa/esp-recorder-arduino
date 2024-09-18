#pragma once

#include <ctime>
#include <string_view>

#include "esp_wifi.h"

class Connection
{
public:
  bool InitWifi(const std::string_view ssid, const std::string_view password);
  bool DeInitWifi();
  bool IsWifiConnected();

  void SntpTimeSync();
  bool WasTimeSyncAttempted() { return m_time_sync_attempted; }

private:
  bool m_is_wifi_started = false;
  // a flag representing if a time sync has been atempted
  bool m_time_sync_attempted = false;
};