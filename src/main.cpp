#include "main.hpp"

#define LOG(...) Serial.printf(__VA_ARGS__)

RotaryEncoder s_rotary_encoder(16, -16);
ScreenDriver<GxEPD2_290_T94> s_screen_driver;
Connection s_connection;
Timeout s_sleep_timeout;
PCF8563 s_rtc_driver;

TaskHandle_t s_setup_task_handle = nullptr;

void
setup()
{
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(BUTTON_PIN, HIGH); // activate the pullup

  SPI.begin(SPI_PIN_CLK, SPI_PIN_MISO, SPI_PIN_MOSI);
  if (SPI.bus() == nullptr) {
    Serial.println("Failed to init SPI.\n");
  }
  Wire.setPins(I2C_PIN_SDA, I2C_PIN_SCL);

  s_rotary_encoder.Init(ROT_ENC_PIN_SIA, ROT_ENC_PIN_SIB, ROT_ENC_PIN_SW);
  s_rtc_driver.Init();

  // manage wi-fi startup, time sync, ePaper initialization, sleep timeout timer in a separate
  // thread to start the recording process as quick as possible
  StartSetupTask();
}

void
loop()
{
  if (IsRecButtonPressed()) {
    s_sleep_timeout.Stop();

    StartRecordingProcess();

    s_sleep_timeout.Reset();
    s_sleep_timeout.Start();
  }

  if (s_sleep_timeout.IsTimeoutReached()) {
    s_sleep_timeout.DeInit();

    EnterSleep();
  }

  static int rotary_pos = s_rotary_encoder.GetPosition();
  int new_rotary_pos = s_rotary_encoder.GetPosition();
  if (rotary_pos != new_rotary_pos) {
    rotary_pos = new_rotary_pos;
    s_sleep_timeout.Reset();
    Serial.println(rotary_pos);
  }

  vTaskDelay(pdMS_TO_TICKS(50));
}

void
StartupSetupExecutor(void*)
{
  s_sleep_timeout.Init(10'000);

  s_connection.InitWifi(WIFI_SSID, WIFI_PASS);

  const std::expected<tm, bool> result = s_connection.SntpTimeSync();
  result.has_value() ? s_rtc_driver.SetExternalTime(result.value())
                     : s_rtc_driver.SetInternalTimeFromExternal();

  // s_screen_driver.Init();
  // s_screen_driver.Clear();
  //   s_screen_driver.DisplayStandby();
  // s_screen_driver.DisplayAfterRecording();
  // s_screen_driver.DeInit();

  s_sleep_timeout.Start();

  vTaskDelete(s_setup_task_handle);
}

void
StartSetupTask()
{
  xTaskCreate(StartupSetupExecutor, "Setup_Executor", 4096, nullptr, 8, &s_setup_task_handle);
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
      vTaskDelay(pdMS_TO_TICKS(250));
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

bool
EnterSleep()
{
  LOG("Preparing to enter into sleep mode...\n");

  if (!s_connection.DeInitWifi()) {
    LOG("Failed to de-initialize Wi-Fi.\n");
  }

  s_screen_driver.DeInit();
  s_rotary_encoder.PrepareForSleep();
  gpio_wakeup_enable(static_cast<gpio_num_t>(BUTTON_PIN), gpio_int_type_t::GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();

  LOG("Entering sleep...\n");
  Serial.flush();

  const esp_err_t esp_result = esp_light_sleep_start();
  if (esp_result != ESP_OK) {
    LOG(
      "%s:%d | Failed to enter sleep mode: %s\n", __FILE__, __LINE__, esp_err_to_name(esp_result));
    return false;
  }

  LOG("Awakened from sleep.\n");

  StartSetupTask();

  return true;
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