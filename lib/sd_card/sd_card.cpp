#include "sd_card.hpp"

#include <cstdio>

// #if DEBUG_SD
// #define LOG_LOCAL_LEVEL esp_log_level_t::ESP_LOG_DEBUG
// #else
// #define LOG_LOCAL_LEVEL esp_log_level_t::ESP_LOG_WARN
// #endif
// #include "esp_log.h"

#include "SD.h"

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

    const bool b_result = SD.begin(SD_PIN_CS, SPI, 4'000'000UL, VFS_MOUNT_POINT.data(), 5, false);
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

    if (!m_is_init) {
        LOG("SD card is not initialized. Skipping the de-initialization...\n");
    }

    SD.end();

    m_is_init = false;
}

SDCard::~SDCard()
{
    DeInit();
}

std::string
SDCard::GetFilePath(const std::string_view file_name)
{
    std::string file_path;
    file_path.reserve(VFS_MOUNT_POINT.size() + sizeof('/') + file_path.size() + sizeof('\0'));
    file_path.append(VFS_MOUNT_POINT).append(1, '/').append(file_name);
    return file_path;
}

std::string_view
SDCard::GetMountPoint()
{
    return VFS_MOUNT_POINT;
}

} // namespace sd