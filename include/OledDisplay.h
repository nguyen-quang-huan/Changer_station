#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Config.h"
#include "BatterySlot.h"

class OledDisplay {
private:
  Adafruit_SSD1306 _oled1;
  Adafruit_SSD1306 _oled2;
  bool _oled1Online;
  bool _oled2Online;

  void renderSlot(Adafruit_SSD1306& oled, const BatterySlot& slot);

public:
  OledDisplay();
  void begin();
  void update(const BatterySlot slots[NUM_SLOTS]);
};