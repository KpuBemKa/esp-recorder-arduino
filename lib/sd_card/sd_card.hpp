#pragma once

#include <string>
#include <string_view>

#include "esp_err.h"

namespace sd {

class SDCard
{
  public:
    bool Init();
    void DeInit();

    ~SDCard();

    /// @brief Creates a full file path string for `file_name`, which includes SD card VFS mount
    /// point
    /// @param file_name file name
    /// @return file path
    static std::string GetFilePath(const std::string_view file_name);

    static std::string_view GetMountPoint();

  private:
    bool m_is_init = false;
};

} // namespace sd
