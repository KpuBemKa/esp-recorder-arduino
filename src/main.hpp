#include <charconv>
#include <cstdio>
#include <ctime>
#include <dirent.h>
#include <expected>
#include <unistd.h>

#include "driver/gptimer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "Freenove_WS2812_Lib_for_ESP32.h"
#include <Arduino.h>

#include "connection.hpp"
#include "ftp_client.hpp"
#include "i2s_sampler.hpp"
#include "pcf8563.hpp"
#include "rotary_encoder.hpp"
#include "screen_driver.hpp"
#include "sd_card.hpp"
#include "settings.hpp"
#include "timeout.hpp"
#include "wav_writer.hpp"

/// @brief
/// Startup procedure after a reset.
/// Initializes pins, ePaper screen, Wi-Fi, and synchronizes system time via SNTP.
void
StartupSetupExecutor(void*);

void
StartSetupTask();

/// @brief
/// Stops the Wi-Fi, configures the recording button to be the wake up source,
/// and enters into the light sleep mode.
/// Automatically invokes `ResumeAfterSleep()` after waking up
bool
EnterSleep();

void
ResumeAfterSleep();

/// @brief
/// Initializes needed resources (SPI bus, SD card, screen driver, etc.),
/// records audio into a .wav file with timestamp in its name,
/// then tries to send all stored .wav files to the server,
/// after which they will be deleted if successfully delivered.
/// @return `true` in case of full success, `false` otherwise
bool
StartRecordingProcess();

/// @brief
/// Initializes the I2S audio sampler,
/// records data from it into a temporary .wav file while the recording button
/// is pushed, renames the temp file to a name which contains the device name
/// and a timestamp.
///
/// Note: Because `ResumeAfterSleep()` does not wait for system time to
/// synchronize via SNTP, file timestamp may be inaccurate if recording is short
/// enough.
/// @return `true` if recording was successful & file renamed, `false` otherwise
bool
RecordMicro();

/// @brief Sends all stored .wav files to the server, and deletes them if
/// transfer was a success
/// @return amount of files uploaded to the server
std::size_t
SendStoredFilesToServer();

bool
UploadFileAndDelete(FtpClient& ftp_client,
                    const std::string_view file_path,
                    const std::string_view remote_new_name);

/// @brief Check if the recording button is pressed
/// @return `true` if recording should be started, `false` otherwise
bool
IsRecButtonPressed();

// void
// SetLedState(const bool is_enabled);

/// @brief Renames the file at `temp_tile_path`
/// to contain date and time in its name
/// @param temp_file_path path to the temporary .wav
/// file into which the mic recording was stored
/// @return `true` if successful, `false` otherwise
bool
RenameFile(const std::string_view temp_file_path);

/// @brief Returns an array of file names in the root directory
/// which should be sent to the remote server
/// @param max_amount maximum amount if file names to return
/// @param offset ignore first `offset` files
/// @return An array of file names in the root directory
/// which should be sent to the remote server
std::vector<std::string>
GetWavFileNames(const std::size_t max_amount, const std::size_t offset);

/// @brief Checks if the file's extension at `file_path` is .wav
/// @param file_path path to the file to check
/// @return `true` if `file_path` is a .wav file, `false` otherwise
bool
IsWavFile(const std::string_view file_path);

/// @brief Just adds _(1) to the end of the file name
/// @param file_path file to the path of which name should be changed
/// @return new file path
std::string
AppendNumberToName(const std::string_view file_path);

void
UpdateLeds();

uint8_t
RotaryPositionToByte();

bool
DoesFileExist(const std::string_view file_path);

void
AsyncDisplayImageOnScreen2(const std::string_view file_path);