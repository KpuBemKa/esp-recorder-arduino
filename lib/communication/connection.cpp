#include "connection.hpp"

#include <algorithm>
#include <ctime>

#include <WiFi.h>

#include "settings.hpp"

#if DEBUG_COM
#define LOG(...) Serial.printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

bool
Connection::InitWifi(const std::string_view ssid, const std::string_view password)
{
  if (m_is_wifi_started) {
    LOG("Wi-Fi is already started.\n");
    return true;
  }
  LOG("Initializing Wi-Fi...\n");

  WiFi.begin(String(ssid.data(), ssid.length()), String(password.data(), password.length()));

  // wait for the wifi to connect
  const std::size_t wait_start = millis();
  const std::size_t wait_end = wait_start + 5'000;
  while (WiFi.status() != WL_CONNECTED && millis() <= wait_end) {
    delay(100);
  }

  if (WiFi.status() != WL_CONNECTED) {
    LOG("Failed to connect to the WiFi.\n");
    return false;
  } else {
    LOG("WiFi connected. IP address: ");
    Serial.println(WiFi.localIP());
  }

  // if (!SntpTimeSync()) {
  //   LOG("%s:%d | Error starting SNTP service\n", __FILE__, __LINE__);
  //   return false;
  // }

  m_is_wifi_started = true;

  return true;
}

bool
Connection::DeInitWifi()
{
  if (!m_is_wifi_started) {
    LOG("Wi-Fi is already stopped.\n");
    return true;
  }

  if (WiFi.disconnect(true)) {
    m_is_wifi_started = false;
    LOG("Wi-Fi has been stopped.\n");
    return true;
  } else {
    LOG("Failed to stop Wi-Fi.\n");
    return false;
  }
}

bool
Connection::IsWifiConnected()
{
  if (!m_is_wifi_started) {
    return false;
  }

  return WiFi.isConnected();
}

std::expected<tm, bool>
Connection::SntpTimeSync()
{
  LOG("Syncronizing system time...\n");

  if (!IsWifiConnected()) {
    LOG("Wi-Fi is not connected. Aborting...\n");
    return std::unexpected(false);
  }

  configTime(0, 0, "pool.ntp.org");

  tm time_info;
  if (!getLocalTime(&time_info)) {
    
    LOG("Failed to obtain time.\n");
    return std::unexpected(false);
  }

  m_time_sync_attempted = true;

  return time_info;

// #if DEBUG_COM
#if 0
  time_info.tm_mon += 1;

  Serial.print("Local time: ");
  Serial.println(&time_info, "%d %b %y %H:%M:%S");
#endif
}
