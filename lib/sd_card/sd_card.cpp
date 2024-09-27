#include "sd_card.hpp"

#include <charconv>
#include <cstdio>
#include <dirent.h>
#include <filesystem>

// #if DEBUG_SD
// #define LOG_LOCAL_LEVEL esp_log_level_t::ESP_LOG_DEBUG
// #else
// #define LOG_LOCAL_LEVEL esp_log_level_t::ESP_LOG_WARN
// #endif
// #include "esp_log.h"

#include <SD.h>
#include <SPI.h>

#include "settings.hpp"

#if DEBUG_SD
#define LOG(...) Serial.printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

namespace sd {

bool
SDCard::Init()
{
  LOG("Initializing SD card...\n");

  if (m_is_init) {
    LOG("SD card is already initialized.\n");
    return ESP_OK;
  }

  const bool b_result = SD.begin(pins::SD_CS, SPI, 4'000'000UL, VFS_MOUNT_POINT.data(), 5, false);

  // const bool b_result = SD.begin(SD_PIN_CS);
  if (!b_result) {
    LOG("Failed to mount the SD card.\n");
    return false;
  }

  const sdcard_type_t card_type = SD.cardType();
  if (card_type == sdcard_type_t::CARD_NONE) {
    LOG("No SD card detected.\n");
    return false;
  }

#if DEBUG_SD
  LOG("SD card type: ");
  switch (card_type) {
    case sdcard_type_t::CARD_SD:
      LOG("SDSC\n");
      break;

    case sdcard_type_t::CARD_SDHC:
      LOG("SDHC\n");
      break;

    case sdcard_type_t::CARD_MMC:
      LOG("SDMMC\n");
      break;

    case sdcard_type_t::CARD_UNKNOWN:
      LOG("UNKNOWN\n");
      break;
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  LOG("SD Card Size: %lluMB\n", cardSize);
#endif

  m_is_init = true;

  return true;
}

void
SDCard::DeInit()
{
  LOG("De-initializing the SD card...\n");

  // if (!m_is_init) {
  //   LOG("SD card is not initialized. Skipping the de-initialization...\n");
  // }

  SD.end();

  m_is_init = false;
}

uint64_t
SDCard::GetFreeSpace()
{
  return SD.totalBytes() - SD.usedBytes();
}

void
SDCard::EnsureFreeSpace(const uint64_t& free_bytes)
{
  const uint64_t free_space = GetFreeSpace();
  if (free_space > free_bytes) {
    return;
  }

  std::vector<FileInfo> wav_infos = GetAllWavInfo();
  std::sort(wav_infos.begin(), wav_infos.end(), FileInfo::SortTimestamp);

  const std::filesystem::path base_path = GetMountPointFs();
  int64_t bytes_to_free = free_bytes - GetFreeSpace();
  std::size_t delete_counter = 0;

  while (bytes_to_free > 0) {
    std::filesystem::path file_path = base_path;
    file_path /= DEVICE_NAME;
    file_path += "_";
    file_path += std::to_string(wav_infos[delete_counter].timestamp);
    file_path += ".wav";

    if (!std::filesystem::remove(file_path)) {
      const auto s = file_path.string();
      LOG("Failed to delete %.*s", s.length(), s.c_str());
      ++delete_counter;
      continue;
    }

    bytes_to_free -= wav_infos[delete_counter].size;
    ++delete_counter;
  }

  LOG("%d files have been deleted.\n", delete_counter);

  return;
}

// SDCard::~SDCard()
// {
//   DeInit();
// }

std::string
SDCard::GetFilePath(const std::string_view file_name)
{
  std::string file_path;
  file_path.reserve(VFS_MOUNT_POINT.size() + sizeof('/') + file_path.size() + sizeof('\0'));

  file_path.append(VFS_MOUNT_POINT);
  if (file_name[0] != '/') {
    file_path.append(1, '/');
  }
  file_path.append(file_name);

  return file_path;
}

std::size_t
SDCard::GetFileSize(const std::string_view file_path)
{
  return std::filesystem::file_size(std::filesystem::path(file_path));

  // struct stat file_stats;
  // if (stat(file_path.data(), &file_stats) == -1) {
  //   perror(file_path.data());
  //   return 0;
  // }

  // return file_stats.st_size;
}

std::string_view
SDCard::GetMountPoint()
{
  return VFS_MOUNT_POINT;
}

std::filesystem::path
SDCard::GetMountPointFs()
{
  return std::filesystem::path(VFS_MOUNT_POINT);
}

bool
SDCard::HasExtension(const std::string_view file, const std::string_view extension)
{
  return file.substr(file.find_last_of(".")) == extension;
}

std::size_t
SDCard::GetTimestampFromName(const std::string_view file_name)
{
  // extract the timestamp from the file name
  const std::size_t start_index = file_name.find_first_of("_");
  const std::string_view string_timestamp =
    file_name.substr(start_index + 1, file_name.find_last_of(".") - start_index - 1);

  // convert it to int
  std::size_t value = 0;
  const std::from_chars_result result = std::from_chars(
    string_timestamp.data(), string_timestamp.data() + string_timestamp.size(), value);
  if (result.ec == std::errc::invalid_argument) {
    LOG("Failed to convert `%.*s` to int.", string_timestamp.length(), string_timestamp.data());
    return 0;
  }

  return value;
}

std::vector<FileInfo>
SDCard::GetAllWavInfo()
{
  std::vector<FileInfo> result = {};

  DIR* dir;
  dir = opendir(GetMountPoint().data());

  if (dir == nullptr) {
    LOG("Failed to open the mount directory");
    return result;
  }

  dirent* dir_entity;
  while ((dir_entity = readdir(dir)) != nullptr) {
    // skip the directory entity if it's not a file
    if (dir_entity->d_type != DT_REG) {
      continue;
    }

    const std::string_view file_name(dir_entity->d_name);
    if (!HasExtension(file_name, ".wav")) {
      // skip the file if it's not a .wav file
      continue;
    }

    const std::size_t timestamp = GetTimestampFromName(file_name);
    if (timestamp == 0) {
      // skip the file if there is something wrong with the timestamp
      continue;
    }

    result.push_back(
      FileInfo{ .timestamp = timestamp, .size = GetFileSize(GetFilePath(file_name)) });
  }

  closedir(dir);

  return result;
}

} // namespace sd