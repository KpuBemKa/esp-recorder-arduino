#include "pcf8563.hpp"

#include "esp_log.h"

#include <Arduino.h>
#include <Wire.h>

#define I2C_Addr 0x51
#define INT_PIN 2

#define CTRL_BUF1 0x00
#define CTRL_BUF2 0x01

#define SECOND_DATA_BUF 0x02
#define MINUTE_DATA_BUF 0x03
#define HOUR_DATA_BUF 0x04

#define DAY_DATA_BUF 0x05
#define WEEK_DATA_BUF 0x06
#define MONTH_DATA_BUF 0x07
#define YEAR_DATA_BUF 0x08

#define MINUTE_AE_BUF 0x09
#define HOUR_AE_BUF 0x0A
#define DAY_AE_BUF 0x0B
#define WEEK_AE_BUF 0x0C

#define CLK_FRQ_BUF 0x0D
#define TIMER_CTRL_BUF 0x0E
#define COUNT_VAL_BUF 0x0F

#define changeIntToHex(dec) ((((dec) / 10) << 4) + ((dec) % 10))
#define converseIntToHex(dec) ((((dec) >> 4) * 10) + ((dec) % 16))

#define changeHexToInt(hex) ((((hex) >> 4) * 10) + ((hex) % 16))
#define converseHexToInt(hex) ((((hex) / 10) << 4) + ((hex) % 10))

#define TIMER_FREQUENCY_4096 0 // 4096HZ
#define TIMER_FREQUENCY_64 1   // 64HZ
#define TIMER_FREQUENCY_1 2    // 1HZ
#define TIMER_FREQUENCY_1_60 3 // 1/60Hz

const char TAG[] = "PCF8563";

/******************************************************************************
function: Init
parameter:
Info:
******************************************************************************/
void
PCF8563::Init()
{
  // pinMode(INT_PIN, INPUT_PULLUP); // INPT
  Wire.begin();
  Serial.begin(115200);
  IIC_Write(0x00, 0x00); // basic setting
  IIC_Write(0x01, 0x00); // Disable INT
  Timer_Disable();
  Alarm_Disable();
}

/******************************************************************************
function: Set  Time
parameter:
    hour:0~23
    minute:0~60
    second:0~60
Info:
******************************************************************************/
void
PCF8563::Set_Time(int hour, int minute, int second)
{
  if (hour >= 0) {
    hour = changeIntToHex(hour % 60);
    IIC_Write(HOUR_DATA_BUF, hour);
  }
  if (minute >= 0) {
    minute = changeIntToHex(minute % 60);
    IIC_Write(MINUTE_DATA_BUF, minute);
  }
  if (second >= 0) {
    second = changeIntToHex(second % 60);
    IIC_Write(SECOND_DATA_BUF, second);
  }
}

/******************************************************************************
function: Get Time
parameter:
    buf: Data buffer
Info:
******************************************************************************/
void
PCF8563::Get_Time(uint8_t* buf)
{
  buf[0] = IIC_Read(SECOND_DATA_BUF) & 0x7f; // get second data
  buf[1] = IIC_Read(MINUTE_DATA_BUF) & 0x7f; // get minute data
  buf[2] = IIC_Read(HOUR_DATA_BUF) & 0x3f;   // get hour data

  buf[0] = changeHexToInt(buf[0]);
  buf[1] = changeHexToInt(buf[1]);
  buf[2] = changeHexToInt(buf[2]);
}

/******************************************************************************
function: Set  date
parameter:
    year:1900~2099
    months:1~12
    days:0~31
Info:
******************************************************************************/
void
PCF8563::Set_Days(int year, int months, int days)
{

  if (days >= 0 && days <= 31) {
    days = changeIntToHex(days);
    IIC_Write(DAY_DATA_BUF, days);
  }
  if (months >= 0 && months <= 12) {
    months = changeIntToHex(months);
    IIC_Write(MONTH_DATA_BUF, (IIC_Read(MONTH_DATA_BUF) & 0x80) | months);
  }

  if (year >= 1900 && year < 2000) {

    IIC_Write(MONTH_DATA_BUF, IIC_Read(MONTH_DATA_BUF) | 0x80);
    /*7  century; this bit is toggled when the years register
        overflows from 99 to 00
        0 indicates the century is 20xx
        1 indicates the century is 19xx
    */
    IIC_Write(YEAR_DATA_BUF, year % 100);
  } else if (year >= 2000 && year < 3000) {
    IIC_Write(MONTH_DATA_BUF, IIC_Read(MONTH_DATA_BUF) & 0x7F);
    IIC_Write(YEAR_DATA_BUF, year % 100);
  }
}

/******************************************************************************
function: Get date
parameter:
    buf: Data buffer
Info:
******************************************************************************/
void
PCF8563::Get_Days(uint8_t* buf)
{
  buf[0] = IIC_Read(DAY_DATA_BUF) & 0x3f;
  buf[1] = IIC_Read(MONTH_DATA_BUF) & 0x1f;
  buf[2] = IIC_Read(YEAR_DATA_BUF) & 0xff;

  buf[0] = changeHexToInt(buf[0]);
  buf[1] = changeHexToInt(buf[1]);
  // buf[2] = changeHexToInt(buf[2]);

  if (IIC_Read(MONTH_DATA_BUF) & 0x80) {
    buf[3] = 19;
  } else {
    buf[3] = 20;
  }
}

bool
PCF8563::SetExternalTime(const tm& p_dt)
{
  // Serial.printf("p_dt time: %04d.%02d.%02d %02d:%02d:%02d\n",
  //               p_dt.tm_year + 1900,
  //               p_dt.tm_mon + 1,
  //               p_dt.tm_mday,
  //               p_dt.tm_hour,
  //               p_dt.tm_min,
  //               p_dt.tm_sec);

  Set_Time(p_dt.tm_hour, p_dt.tm_min, p_dt.tm_sec);
  Set_Days(p_dt.tm_year + 1900, p_dt.tm_mon + 1, p_dt.tm_mday);

  return true;
}

bool
PCF8563::SetInternalTimeFromExternal()
{
  uint8_t buf[7] = { 0 };
  Get_Time(buf);
  Get_Days(buf + 3);

  // Serial.printf(
  //   "Buffer: %d %d %d %d %d %d %d\n", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);

  // buf[6] is the `20` part of `2024`, while buf[5] is the `24` part
  tm temp_time{
    // seconds are stored same way as in POSIX: [0, 60]
    .tm_sec = buf[0],
    // minutes are stored same way as in POSIX: [0, 60]
    .tm_min = buf[1],
    // hours are stored same way as in POSIX: [0, 23]
    .tm_hour = buf[2],
    // days are stored same way as in POSIX: [1, 31|30|29|28]
    .tm_mday = buf[3],
    // months are stored as [1, 12], in POSIX as [0, 11]
    .tm_mon = buf[4] - 1,
    // years are stored as [0, 99] in POSIX years since 1900
    .tm_year = buf[5] + 100,
  };

  timeval timestamp;
  gettimeofday(&timestamp, nullptr); // get microseconds too
  timestamp.tv_sec = mktime(&temp_time);
  if (timestamp.tv_sec == -1) {
    ESP_LOGE(TAG, "%s:%d | Couldn't convert time struct to timestamp.", __FILE__, __LINE__);
    return false;
  }

  if (settimeofday(&timestamp, nullptr) != 0) {
    ESP_LOGE(TAG, "%s:%d | Couldn't set internal time.", __FILE__, __LINE__);
    return false;
  }

  return true;
}

/******************************************************************************
function: Set Week Data
parameter:
    WeekData:0~7
Info:
******************************************************************************/
void
PCF8563::Set_WeekData(int WeekData)
{
  if (WeekData <= 7) {
    IIC_Write(WEEK_DATA_BUF, WeekData);
  }
}

/******************************************************************************
function: Set alarm parameters
parameter:
    hour: 0~23
    minute: 0~59
Info:
******************************************************************************/
void
PCF8563::Set_Alarm(int hour, int minute)
{
  if (minute >= 0) {
    minute = changeIntToHex(minute);
    IIC_Write(MINUTE_AE_BUF, minute);
  }

  if (hour >= 0) {
    hour = changeIntToHex(hour);
    IIC_Write(HOUR_AE_BUF, hour);
  }
}
/******************************************************************************
function: Enable Alarm
parameter:
Info:
******************************************************************************/
void
PCF8563::Alarm_Enable()
{

  IIC_Write(0x01, (IIC_Read(0x01) | 0x02));
  IIC_Write(MINUTE_AE_BUF, IIC_Read(MINUTE_AE_BUF) & 0x7f);
  IIC_Write(HOUR_AE_BUF, IIC_Read(HOUR_AE_BUF) & 0x7f);
  IIC_Write(DAY_AE_BUF, 0x80);  // 关闭
  IIC_Write(WEEK_AE_BUF, 0x80); // 关闭
}

/******************************************************************************
function: Disable Alarm
parameter:
Info:
******************************************************************************/
void
PCF8563::Alarm_Disable()
{
  IIC_Write(0x01, (IIC_Read(0x01) & 0xfd));
  IIC_Write(MINUTE_AE_BUF, IIC_Read(MINUTE_AE_BUF) | 0x80);
  IIC_Write(HOUR_AE_BUF, IIC_Read(HOUR_AE_BUF) | 0x80);
  IIC_Write(DAY_AE_BUF, 0x80);  // 关闭
  IIC_Write(WEEK_AE_BUF, 0x80); // 关闭
}

/******************************************************************************
function: Set timer register
parameter:
    Timer_Frequency : Choose the corresponding frequency
                    4096    :TIMER_FREQUENCY_4096
                    64      :TIMER_FREQUENCY_64
                    1       :TIMER_FREQUENCY_1
                    0       :TIMER_FREQUENCY_1_60
    Value           : Value
                    Total cycle = Value/TIMER_FREQUENCY
Info:
    TIMER_CTRL_BUF//0x0E
    TIMER_FREQUENCY_4096    0 // 4096HZ      MAX  0.062 second
    TIMER_FREQUENCY_64      1 // 64HZ        MAX  3.98 second
    TIMER_FREQUENCY_1       2 // 1HZ         MAX  255 second
    TIMER_FREQUENCY_1_60    3 // 1/60Hz      MAX  255 minute
******************************************************************************/
void
PCF8563::Set_Timer(int Timer_Frequency, uint8_t Value)
{
  // IIC_Write(TIMER_CTRL_BUF, IIC_Read(TIMER_CTRL_BUF)&0x7f);
  IIC_Write(COUNT_VAL_BUF, Value);
  if (Timer_Frequency == 4096) {
    IIC_Write(TIMER_CTRL_BUF, ((IIC_Read(TIMER_CTRL_BUF)) & 0xfc) | TIMER_FREQUENCY_4096);
  } else if (Timer_Frequency == 64) {
    IIC_Write(TIMER_CTRL_BUF, ((IIC_Read(TIMER_CTRL_BUF)) & 0xfc) | TIMER_FREQUENCY_64);
  } else if (Timer_Frequency == 1) {
    IIC_Write(TIMER_CTRL_BUF, ((IIC_Read(TIMER_CTRL_BUF)) & 0xfc) | TIMER_FREQUENCY_1);
  } else if (Timer_Frequency == 0) { // 1/60
    IIC_Write(TIMER_CTRL_BUF, ((IIC_Read(TIMER_CTRL_BUF)) & 0xfc) | TIMER_FREQUENCY_1_60);
  } else {
    printf("Set Timer Error\r\n");
  }
}
/******************************************************************************
function: Enable timer
parameter:
Info:
******************************************************************************/
void
PCF8563::Timer_Enable()
{
  IIC_Write(0x01, (IIC_Read(0x01) | 0x01));
  IIC_Write(TIMER_CTRL_BUF, IIC_Read(TIMER_CTRL_BUF) | 0x80);
}

/******************************************************************************
function: Disable timer
parameter:
Info:
******************************************************************************/
void
PCF8563::Timer_Disable()
{
  IIC_Write(0x01, (IIC_Read(0x01) & 0xfe));
  IIC_Write(TIMER_CTRL_BUF, IIC_Read(TIMER_CTRL_BUF) & 0x7f);
}

/******************************************************************************
function: Clear alarm interrupt flag
parameter:
Info:
******************************************************************************/
void
PCF8563::Cleare_AF_Flag()
{
  IIC_Write(CTRL_BUF2, IIC_Read(CTRL_BUF2) & 0xf7);
}

/******************************************************************************
function: Clear timer interrupt flag
parameter:
Info:
******************************************************************************/
void
PCF8563::Cleare_TF_Flag()
{
  IIC_Write(CTRL_BUF2, IIC_Read(CTRL_BUF2) & 0xfB);
}

/******************************************************************************
function: Get Flag
parameter:
Info:
    return: 1:AF alarm
            2:TF timer
            3:AF and TF
******************************************************************************/
uint8_t
PCF8563::Get_Flag()
{
  uint8_t temp = 0;
  if (IIC_Read(CTRL_BUF2) & 0x08) {
    temp = temp | 0x01;
  }
  if (IIC_Read(CTRL_BUF2) & 0x04) {
    temp = temp | 0x02;
  }
  return temp;
}
/******************************************************************************
function: Set timer register
parameter:
    Timer_Frequency : Choose the corresponding frequency
                    32768   :327.68KHz
                    1024    :1024Hz
                    32      :32Hz
                    1       :1Hz
    Value           : Value
                    Total cycle = Value/TIMER_FREQUENCY
Info:

******************************************************************************/
void
PCF8563::CLKOUT_FrequencyHZ(uint16_t Frequency)
{
  if (Frequency == 32768) {
    IIC_Write(CLK_FRQ_BUF, (IIC_Read(CLK_FRQ_BUF) & 0xfC) | 0x00);
  } else if (Frequency == 1024) {
    IIC_Write(CLK_FRQ_BUF, (IIC_Read(CLK_FRQ_BUF) & 0xfC) | 0x01);
  } else if (Frequency == 32) {
    IIC_Write(CLK_FRQ_BUF, (IIC_Read(CLK_FRQ_BUF) & 0xfC) | 0x02);
  } else if (Frequency == 1) {
    IIC_Write(CLK_FRQ_BUF, (IIC_Read(CLK_FRQ_BUF) & 0xfC) | 0x03);
  } else {
    printf("Set CLKOUT requency Selection Error\r\n");
  }
}
/******************************************************************************
function: Enable CLKOUT
parameter:
Info:
******************************************************************************/
void
PCF8563::CLKOUT_Enable()
{
  IIC_Write(CLK_FRQ_BUF, IIC_Read(CLK_FRQ_BUF) | 0x80);
}

/******************************************************************************
function: Disable CLKOUT
parameter:
Info:
******************************************************************************/
void
PCF8563::CLKOUT_Disable()
{
  IIC_Write(CLK_FRQ_BUF, IIC_Read(CLK_FRQ_BUF) & 0x7f);
}

/******************************************************************************
i2C underlying read and write
******************************************************************************/
void
PCF8563::IIC_Write(uint8_t reg, uint8_t data)
{
  Wire.beginTransmission(I2C_Addr);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

uint8_t
PCF8563::IIC_Read(uint8_t reg)
{
  Wire.beginTransmission(I2C_Addr);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom(I2C_Addr, 1);
  return Wire.read();
}