#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "esp_err.h"

namespace sd {

struct FileInfo
{
  std::size_t timestamp;
  std::size_t size;

  static bool SortTimestamp(const FileInfo& a, const FileInfo& b)
  {
    return a.timestamp < b.timestamp;
  }
};

class SDCard
{
public:
  bool Init();
  void DeInit();

  uint64_t GetFreeSpace();
  void EnsureFreeSpace(const uint64_t& free_bytes);

  // ~SDCard();

  /// @brief Creates a full file path string for `file_name`, which includes SD card VFS mount
  /// point
  /// @param file_name file name
  /// @return file path
  static std::string GetFilePath(const std::string_view file_name);
  static std::size_t GetFileSize(const std::string_view file_path);

  static std::string_view GetMountPoint();
  static std::filesystem::path GetMountPointFs();

private:
  static bool HasExtension(const std::string_view file, const std::string_view extension);
  static std::size_t GetTimestampFromName(const std::string_view file_name);

  std::vector<FileInfo> GetAllWavInfo();

private:
  bool m_is_init = false;
};

} // namespace sd
