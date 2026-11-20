#pragma once
#include <Arduino.h>

// ============================================================================
// 1. CẤU HÌNH PHẦN CỨNG THEO SƠ ĐỒ NGUYÊN LÝ (ESP32 DEVKIT - 2 KHAY)
// ============================================================================
#define PIN_I2C_SDA         21  // G21 -> SDA (O1, O2, U5, U8)
#define PIN_I2C_SCL         22  // G22 -> SCL (O1, O2, U5, U8)[cite: 19]
#define PIN_ONEWIRE         4   // G4  -> TEMP (I1, I2 DS18B20)[cite: 19]

#define PIN_RELAY_1         19  // G19 -> R1 (Relay kênh 1)[cite: 19]
#define PIN_RELAY_2         18  // G18 -> R2 (Relay kênh 2)[cite: 19]

#define PIN_RELAY_SLOT1     PIN_RELAY_1
#define PIN_RELAY_SLOT2     PIN_RELAY_2

// Địa chỉ I2C trên bus dùng chung (G21, G22)[cite: 19]
#define INA226_ADDR_SLOT1   0x41  // U5 (Mặc định)[cite: 18, 19]
#define INA226_ADDR_SLOT2   0x40  // U8 (Hàn chập pad A0 trên module)[cite: 18, 19]

#define OLED_ADDR_SLOT1     0x3C  // O1 (Mặc định)[cite: 19]
#define OLED_ADDR_SLOT2     0x3D  // O2 (Chuyển điện trở địa chỉ sang 0x7A/0x3D)[cite: 19]

#define NUM_SLOTS           2     // Hệ thống 2 khay sạc U2, U4[cite: 19]

// Giữ đúng mức kích CAO (1) theo phần cứng module Relay của bạn[cite: 18]
#define RELAY_ACTIVE_HIGH   1     //[cite: 18]

#if RELAY_ACTIVE_HIGH
  const int RELAY_ON  = HIGH;     //[cite: 18]
  const int RELAY_OFF = LOW;      //[cite: 18]
#else
  const int RELAY_ON  = LOW;      //[cite: 18]
  const int RELAY_OFF = HIGH;     //[cite: 18]
#endif

// ============================================================================
// 2. THAM SỐ NGƯỠNG AN TOÀN & TỰ ĐỘNG NHẬN DIỆN PIN
// ============================================================================
const float HARD_MAX_VOLTAGE          = 4.32f;    // Ngắt khẩn cấp nếu quá áp[cite: 18]
const float SAFE_RECOVER_V            = 4.22f;    // Mức áp an toàn để xóa lỗi[cite: 18]
const float HARD_MAX_TEMP             = 50.0f;    // Ngắt khẩn cấp nếu quá nhiệt[cite: 18]
const float SAFE_RECOVER_TEMP         = 46.0f;    // Mức nhiệt an toàn để sạc lại[cite: 18]
const float BATTERY_PRESENT_V         = 1.50f;    // Ngưỡng nhận diện có pin trong khay[cite: 18]
const float FULL_CHARGE_V             = 4.15f;    // Điện áp ngưỡng đầy pin
const float FULL_CHARGE_I             = 0.08f;    // Dòng kết thúc sạc (80mA)
const float MIN_CHARGE_LOAD_I         = 0.015f;   // Dòng tải tối thiểu (15mA)
const unsigned long FULL_TIME_MS      = 3000UL;   // Duy trì dòng thấp 3s ở áp cao -> Chốt DAY PIN
const unsigned long AUTO_CHARGE_MS    = 1500UL;   // Chờ 1.5s ổn định rồi đóng sạc
const unsigned long WAKE_INTERVAL_MS  = 2500UL;   // Chu kỳ nháy xung đánh thức pin bị khóa bảo vệ
const unsigned long WAKE_DURATION_MS  = 350UL;    // Độ rộng xung đánh thức (350ms)

enum SlotStatus : uint8_t {
  STATUS_EMPTY    = 0, //[cite: 18]
  STATUS_STANDBY  = 1, //[cite: 18]
  STATUS_CHARGING = 2, //[cite: 18]
  STATUS_FULL     = 3, //[cite: 18]
  STATUS_FAULT    = 4  //[cite: 18]
};

enum FaultCode : uint8_t {
  FAULT_NONE         = 0, //[cite: 18]
  FAULT_OVER_VOLTAGE = 1, //[cite: 18]
  FAULT_OVER_TEMP    = 2, //[cite: 18]
  FAULT_SENSOR_ERROR = 3  //[cite: 18]
};

#define LOG_PRINTLN(x)  Serial.println(x)          //[cite: 18]
#define LOG_PRINTF(...) Serial.printf(__VA_ARGS__) //[cite: 18]