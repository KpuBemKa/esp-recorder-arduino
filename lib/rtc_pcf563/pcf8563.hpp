#pragma once

#include <cstdint>
#include <ctime>

class PCF8563
{
public:
  void Init();

  bool SetExternalTime(const tm& p_dt);
  bool SetInternalTimeFromExternal();

  void Set_Time(int hour, int minute, int second);
  void Get_Time(uint8_t* buf);

  void Set_Days(int year, int months, int days);
  void Get_Days(uint8_t* buf);

  void Set_WeekData(int WeekData);

  void Set_Alarm(int hour, int minute);
  void Set_Timer(int Timer_frequency, uint8_t value);

  void Alarm_Enable();
  void Alarm_Disable();
  void Timer_Enable();
  void Timer_Disable();

  void Cleare_AF_Flag(); // Alarm
  void Cleare_TF_Flag(); // Timer

  uint8_t Get_Flag();

  void CLKOUT_FrequencyHZ(uint16_t Frequency);

  void CLKOUT_Disable();
  void CLKOUT_Enable();

private:
  void IIC_Write(uint8_t reg, uint8_t data);
  uint8_t IIC_Read(uint8_t reg);

private:
};
