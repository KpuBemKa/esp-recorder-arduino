#include <cstdio>
#include <ctime>

#include <Arduino.h>

#include "i2s_sampler.hpp"
#include "sd_card.hpp"
#include "settings.hpp"
#include "wav_writer.hpp"
#include "screen_driver.hpp"

constexpr i2s_std_config_t I2S_MIC_CONFIG = { 
  .clk_cfg = {
    .sample_rate_hz = MIC_SAMPLE_RATE,
    .clk_src = I2S_CLK_SRC_DEFAULT,
    // .ext_clk_freq_hz = 0,
    .mclk_multiple = I2S_MCLK_MULTIPLE_256,
  },
  .slot_cfg = {
    .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT, 
    .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO, 
    .slot_mode = I2S_SLOT_MODE_STEREO, 
    .slot_mask = I2S_STD_SLOT_LEFT,
    .ws_width = I2S_DATA_BIT_WIDTH_32BIT, 
    .ws_pol = false, 
    .bit_shift = true, 
    .left_align = true, 
    .big_endian = false, 
    .bit_order_lsb = false 
  },
    
  .gpio_cfg = {
    .mclk = I2S_GPIO_UNUSED,  
    .bclk = static_cast<gpio_num_t>(I2S_MIC_SERIAL_CLOCK),
    .ws   = static_cast<gpio_num_t>(I2S_MIC_WORD_SELECT),
    .dout = gpio_num_t::GPIO_NUM_NC,
    .din  = static_cast<gpio_num_t>(I2S_MIC_SERIAL_DATA),
    .invert_flags = {
      .mclk_inv = false,
      .bclk_inv = false,
      .ws_inv   = false,
    },
  }
};

bool
StartRecordingProcess();

bool
RecordMicro();

bool
IsRecButtonPressed();

esp_err_t
RenameFile(const std::string_view temp_file_path);

void
setup()
{
    Serial.begin(115200);

    pinMode(BUTTON_PIN, INPUT_PULLUP); // set the pullup
    digitalWrite(BUTTON_PIN, HIGH); // activate the pullup

    ScreenDriver<GxEPD2_290_T94> screen_driver;
    screen_driver.Init();

    Serial.printf("Ready.\n");
}

void
loop()
{
    if (!IsRecButtonPressed()) {
        delay(1'000);
        return;
    }

    StartRecordingProcess();
}

bool
StartRecordingProcess()
{
    Serial.printf("Preparing to record...\n");

    // s_screen_driver.Init();

    // s_screen_driver.Clear();
    // s_screen_driver.Clear();
    // s_screen_driver.DisplayTextOnRow(0, "Preparing");
    // s_screen_driver.DisplayTextOnRow(1, "to record...");

    // initialize the SD card & mount the partition
    sd::SDCard sd_card;
    if (!sd_card.Init()) {
        Serial.printf("Error initializing the SD card.\n");
        // s_screen_driver.Clear();
        // s_screen_driver.DisplayTextOnRow(0, "SD card");
        // s_screen_driver.DisplayTextOnRow(1, "error.");
        return false;
    }

    if (RecordMicro()) {
        // s_screen_driver.Clear();
        // s_screen_driver.DisplayTextOnRow(0, "Uploading the");
        // s_screen_driver.DisplayTextOnRow(1, "recording...");

        // esp_result = SendStoredFilesToServer();

        // s_screen_driver.Clear();
        // s_screen_driver.DisplayTextOnRow(0, "Recording");
        // s_screen_driver.DisplayTextOnRow(1, "transmission");

        // if (esp_result == ESP_OK) {
        //     // s_screen_driver.DisplayTextOnRow(2, "success.");
        // } else {
        //     // s_screen_driver.DisplayTextOnRow(2, "error.");
        //     Serial.printf("Error transmitting recorded files: %s", esp_err_to_name(esp_result));
        // }
    }

    // de-init the sd card
    sd_card.DeInit();

    // de-init the screen
    // esp_result = s_screen_driver.DeInit();
    // if (esp_result != ESP_OK) {
    //     Serial.printf("%s:%d | Error de-initializing the screen: %s",
    //           __FILE__,
    //           __LINE__,
    //           esp_err_to_name(esp_result));
    //     return esp_result;
    // }

    Serial.printf("Released used resources.\n");

    return ESP_OK;
}

bool
RecordMicro()
{
    // create & initialize the I2S sampler which samples the microphone
    I2sSampler i2s_sampler;
    esp_err_t esp_result = i2s_sampler.Init(I2S_MIC_CONFIG);
    if (esp_result != ESP_OK) {
        Serial.printf("%s:%d | Error initializing the I2S sampler: %s\n",
              __FILE__,
              __LINE__,
              esp_err_to_name(esp_result));
        // s_screen_driver.Clear();
        // s_screen_driver.DisplayTextOnRow(0, "Microphone");
        // s_screen_driver.DisplayTextOnRow(1, "error.");
        return false;
    }

    const std::string temp_file_path = sd::SDCard::GetFilePath("temp.wav");

    // create a new wav file writer
    WavWriter writer;
    if (!writer.Open(temp_file_path)) {
        Serial.printf("Error opening a file for writing: %s\n");
        // s_screen_driver.Clear();
        // s_screen_driver.DisplayTextOnRow(0, "File error.");
        return false;
    }

    // First few samples are a bit rough, it's best to discard them
    i2s_sampler.DiscardSamples(128 * 60);

    Serial.printf("Recording...\n");
    // s_screen_driver.Clear();
    // s_screen_driver.DisplayTextOnRow(0, "Recording...");

    // keep writing until the user releases the button
    while (IsRecButtonPressed()) {
        std::vector<int16_t> samples = i2s_sampler.ReadSamples(1024);
        writer.WriteSamples(samples);
    }

    // s_screen_driver.Clear();
    // s_screen_driver.DisplayTextOnRow(0, "Recording");
    // s_screen_driver.DisplayTextOnRow(1, "finished.");
    Serial.printf("Finished recording.\n");

    // stop the sampler
    esp_result = i2s_sampler.DeInit();
    if (esp_result != ESP_OK) {
        Serial.printf("%s:%d | Error de-initializing the I2S sampler: %s\n",
              __FILE__,
              __LINE__,
              esp_err_to_name(esp_result));
        return esp_result;
    }

    // finish the writing
    writer.Close();

    esp_result = RenameFile(temp_file_path);
    if (esp_result != ESP_OK) {
        Serial.printf("%s:%d | Error renaming file: %s\n", __FILE__, __LINE__, esp_err_to_name(esp_result));
        return esp_result;
    }

    return ESP_OK;
}

bool
IsRecButtonPressed()
{
    return digitalRead(BUTTON_PIN) == 0;
    // Serial.println(digitalRead(BUTTON_PIN));
    // return false;
}

esp_err_t
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
            dt->tm_mon,
            dt->tm_mday,
            dt->tm_hour,
            dt->tm_min,
            dt->tm_sec);

    Serial.printf("New file name: %s\n", new_file_name.c_str());

    const std::string new_path = sd::SDCard::GetFilePath(new_file_name);

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
        return ESP_FAIL;
    }

    return ESP_OK;
}