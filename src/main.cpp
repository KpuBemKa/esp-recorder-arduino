#include <charconv>
#include <cstdio>
#include <ctime>
#include <dirent.h>
#include <expected>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <Arduino.h>

#include "connection.hpp"
#include "ftp_client.hpp"
#include "i2s_sampler.hpp"
#include "screen_driver.hpp"
#include "sd_card.hpp"
#include "settings.hpp"
#include "wav_writer.hpp"

#define LOG(...) Serial.printf(__VA_ARGS__)

ScreenDriver<GxEPD2_290_T94> s_screen_driver;
Connection s_connection;

TaskHandle_t s_setup_task_handle = nullptr;

/// @brief
/// Startup procedure after a reset.
/// Initializes pins, ePaper screen, Wi-Fi, and synchronizes system time via SNTP.
void
StartupSetupExecutor(void*);

/// @brief
/// Initializes Wi-Fi and starts the SNTP synchronization,
/// but does no wait for the sync to complete.
/// @return `true` in case of full success, `false` otherwise
bool
ResumeAfterSleep();

/// @brief
/// Stops the Wi-Fi, configures the recording button to be the wake up source,
/// and enters into the light sleep mode.
/// Automatically invokes `ResumeAfterSleep()` after waking up
void
EnterDeepSleep();

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
setup()
{
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT); // set the pullup
  // pinMode(BUTTON_PIN, INPUT_PULLUP); // set the pullup
  // digitalWrite(BUTTON_PIN, HIGH);    // activate the pullup

  SPI.begin(SPI_PIN_CLK, SPI_PIN_MISO, SPI_PIN_MOSI);
  if (SPI.bus() == nullptr) {
    Serial.println("Failed to init SPI.\n");
  }

  // manage wi-fi startup, time sync & ePaper initialization in a separate thread
  // to start the recording process as quick as possible
  xTaskCreate(StartupSetupExecutor, "Setup_Executor", 1024 * 4, nullptr, 8, &s_setup_task_handle);
  // s_setup_thread = std::thread(StartupSetup);
  // StartupSetup();

  // if (!IsRecButtonPressed()) {
  //   delay(100);
  //   return;
  // }

  // StartRecordingProcess();
}

void
loop()
{
  if (!IsRecButtonPressed()) {
    delay(100);
    return;
  }

  StartRecordingProcess();
  // EnterDeepSleep();
}

void
StartupSetupExecutor(void*)
{
  s_connection.InitWifi(WIFI_SSID, WIFI_PASS);
  s_connection.SntpTimeSync();

  // ScreenDriver<GxEPD2_290_T94> screen_driver;
  // screen_driver.Init();

  // s_screen_driver.Init();
  // s_screen_driver.Clear();
  //   s_screen_driver.DisplayStandby();
  // s_screen_driver.DisplayAfterRecording();
  // s_screen_driver.DeInit();

  vTaskDelete(s_setup_task_handle);
}

bool
StartRecordingProcess()
{
  Serial.printf("Preparing to record...\n");

  // initialize the SD card & mount the partition
  sd::SDCard sd_card;
  if (!sd_card.Init()) {
    Serial.printf("Error initializing the SD card.\n");
    return false;
  }

  if (RecordMicro()) {
    const TickType_t wait_start = xTaskGetTickCount();
    const TickType_t wait_end = wait_start + pdMS_TO_TICKS(5'000);
    while (s_connection.IsWifiConnected() && xTaskGetTickCount() <= wait_end) {
      vTaskDelay(pdMS_TO_TICKS(500));
    }

    const std::size_t upload_count = SendStoredFilesToServer();
    if (upload_count > 0) {
      LOG("%d files have been successfuly stored on the remove server.\n", upload_count);
    } else {
      LOG("Failed to upload any files to the remote server.\n");
    }
  }

  // de-init the sd card
  sd_card.DeInit();

  return true;
}

bool
RecordMicro()
{
  // create & initialize the I2S sampler which samples the microphone
  I2sSampler i2s_sampler;
  if (!i2s_sampler.Init()) {
    Serial.printf("%s:%d | Error initializing the I2S sampler.\n", __FILE__, __LINE__);
    return false;
  }

  const std::string temp_file_path = sd::SDCard::GetFilePath("temp.wav");

  // create a new wav file writer
  WavWriter writer;
  if (!writer.Open(temp_file_path)) {
    Serial.printf("Error opening a file for writing.\n");
    return false;
  }

  // First few samples are a bit rough, it's best to discard them
  i2s_sampler.DiscardSamples(128 * 60);

  Serial.printf("Recording...\n");

  // keep writing until the user releases the button
  while (IsRecButtonPressed()) {
    std::vector<int16_t> samples = i2s_sampler.ReadSamples(1024);
    writer.WriteSamples(samples);
  }

  Serial.printf("Finished recording.\n");

  // stop the sampler
  if (!i2s_sampler.DeInit()) {
    Serial.printf("%s:%d | Error de-initializing the I2S sampler.\n", __FILE__, __LINE__);
    return false;
  }

  // finish the writing
  writer.Close();

  // wait for system time to synchronize before renaming the file
  const TickType_t wait_start = xTaskGetTickCount();
  const TickType_t wait_end = wait_start + pdMS_TO_TICKS(5'000);
  while (!s_connection.WasTimeSyncAttempted() && xTaskGetTickCount() <= wait_end) {
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  if (!RenameFile(temp_file_path)) {
    Serial.printf("%s:%d | Error renaming file.\n", __FILE__, __LINE__);
    return false;
  }

  return true;
}

bool
IsRecButtonPressed()
{
  return digitalRead(BUTTON_PIN) == 0;
}

bool
RenameFile(const std::string_view temp_file_path)
{
  constexpr std::size_t file_name_length =
    DEVICE_NAME.length() + sizeof("_0000-00-00_00-00-00.wav");

  // Get current time
  const std::time_t now_time = std::time(nullptr);
  const std::tm* dt = std::localtime(&now_time);

  // Create the buffer, and set it to the needed length
  std::string new_file_name(file_name_length, '\0');

  sprintf(new_file_name.data(),
          "%.*s_%04d-%02d-%02d_%02d-%02d-%02d.wav",
          DEVICE_NAME.length(),
          DEVICE_NAME.data(),
          dt->tm_year + 1900,
          dt->tm_mon + 1,
          dt->tm_mday,
          dt->tm_hour,
          dt->tm_min,
          dt->tm_sec);

  Serial.printf("New file name: %s\n", new_file_name.c_str());

  std::string new_path = sd::SDCard::GetFilePath(new_file_name);

  // make another name if file exists
  if (access(new_path.c_str(), F_OK) != 0) {
    new_path = AppendNumberToName(new_path);
  }

  if (rename(temp_file_path.data(), new_path.c_str()) != 0) {
    Serial.printf("%s:%d | Unable to rename '%.*s' to '%.*s'. errno: %d = %s\n",
                  __FILE__,
                  __LINE__,
                  temp_file_path.length(),
                  temp_file_path.data(),
                  new_path.length(),
                  new_path.c_str(),
                  errno,
                  strerror(errno));
    return false;
  }

  return true;
}

void
EnterDeepSleep()
{
  // if (!s_connection.DeInitWifi()) {
  //   LOG("%s:%d | Failed to de-initialize Wi-Fi.\n", __FILE__, __LINE__);
  // }

  // s_screen_driver.DeInit();

  // gpio_wakeup_enable(static_cast<gpio_num_t>(BUTTON_PIN), gpio_int_type_t::GPIO_INTR_LOW_LEVEL);
  // esp_sleep_enable_gpio_wakeup();

  // esp_result = esp_light_sleep_start();
  // if (esp_result != ESP_OK) {
  //   LOG("%s:%d | Failed to enter sleep mode: %s", __FILE__, __LINE__,
  //   esp_err_to_name(esp_result)); return false;
  // }

  // LOG("Awakened from sleep.");

  // esp_result = ResumeAfterSleep();
  // if (esp_result != ESP_OK) {
  //   LOG("%s:%d | Failed resume systems after waking up: %s",
  //       __FILE__,
  //       __LINE__,
  //       esp_err_to_name(esp_result));
  // }

  // // return esp_result;
  // return true;
}

std::size_t
SendStoredFilesToServer()
{
  if (!s_connection.IsWifiConnected()) {
    LOG("Failed to upload files. Wi-Fi is not connected.");
    return 0;
  }

  // Open FTP server
  LOG("ftp server: %s\n", CONFIG_FTP_SERVER.data());
  LOG("ftp user  : %s\n", CONFIG_FTP_USER.data());

  FtpClient ftp_client;

  int connect = ftp_client.ftpClientConnect(CONFIG_FTP_SERVER.data(), CONFIG_FTP_PORT);
  LOG("connect=%d", connect);
  if (connect == 0) {
    LOG("FTP server connect() failed.\n");
    return false;
  }

  // Login to the FTP server
  int login = ftp_client.ftpClientLogin(CONFIG_FTP_USER.data(), CONFIG_FTP_PASSWORD.data());
  LOG("login=%d\n", login);
  if (login == 0) {
    LOG("FTP server login failed.\n");
    return false;
  }

  std::size_t upload_count = 0;
  std::size_t failed_count = 0;
  std::vector<std::string> wav_names = GetWavFileNames(8, 0);

  // upload all stored files to the server by a batch of 8 at a time
  while (wav_names.size() != 0) {
    for (auto wav_file : wav_names) {
      UploadFileAndDelete(ftp_client, sd::SDCard::GetFilePath(wav_file), wav_file) ? ++upload_count
                                                                                   : ++failed_count;
    }

    wav_names = GetWavFileNames(8, failed_count);
  }

  ftp_client.ftpClientQuit();

  return upload_count;
}

bool
UploadFileAndDelete(FtpClient& ftp_client,
                    const std::string_view file_path,
                    const std::string_view remote_new_name)
{
  LOG("Uploading '%.*s'...\n", file_path.length(), file_path.data());

  int result = ftp_client.ftpClientPut(file_path.data(), remote_new_name.data(), FTP_CLIENT_BINARY);
  if (result != 1) {
    LOG("%s:%d | Error uploading '%.*s' to the server.\n",
        __FILE__,
        __LINE__,
        file_path.length(),
        file_path.data());
    return false;
  }

  result = std::remove(file_path.data());
  if (result != 0) {
    LOG("%s:%d | Error deleting '%.*s': %d = %s\n",
        __FILE__,
        __LINE__,
        file_path.length(),
        file_path.data(),
        errno,
        strerror(errno));
    return false;
  }

  return true;
}

std::vector<std::string>
GetWavFileNames(const std::size_t max_amount, const std::size_t offset)
{

  DIR* dir;
  dir = opendir(sd::SDCard::GetMountPoint().data());

  if (dir == nullptr) {
    return {};
  }

  std::size_t file_count = 0;
  dirent* dir_entity;
  std::vector<std::string> wav_names;
  std::size_t remaining_to_skip = offset;

  while ((dir_entity = readdir(dir)) != nullptr && file_count < max_amount) {
    // skip the directory entity if it's not a file
    if (dir_entity->d_type != DT_REG) {
      LOG("'%s' is not a file. Skipping...\n", dir_entity->d_name);
      continue;
    }

    const std::string_view file_path(dir_entity->d_name);

    // skip the file if it's not a .wav file
    if (!IsWavFile(file_path)) {
      LOG("'%s' is not a .wav file. Skipping...\n", dir_entity->d_name);
      continue;
    }

    // skip first `remaining_to_skip` files
    if (remaining_to_skip > 0) {
      --remaining_to_skip;
      continue;
    }

    wav_names.push_back(std::string(file_path));
    ++file_count;

    LOG("'%.*s' has been added to the list.\n", file_path.length(), file_path.data());
  }

  closedir(dir);

  return wav_names;
}

bool
IsWavFile(const std::string_view file_path)
{
  return file_path.substr(file_path.find_last_of(".")) == ".wav";
}

std::string
AppendNumberToName(const std::string_view file_path)
{
  const std::size_t dot_index = file_path.find_last_of(".");

  std::string result = "";
  result.append(file_path.data(), dot_index)
    .append("_(1)")
    .append(file_path.data() + dot_index, file_path.length() - dot_index);

  return result;
}