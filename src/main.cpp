#include <Arduino.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "Config.h"
#include "BatterySlot.h"
#include "OledDisplay.h"

// Khởi tạo 2 khay sạc tương ứng U2 (0x40) và U4 (0x41)
BatterySlot slots[NUM_SLOTS] = {
  BatterySlot(1, PIN_RELAY_SLOT1, INA226_ADDR_SLOT1),
  BatterySlot(2, PIN_RELAY_SLOT2, INA226_ADDR_SLOT2)
};

OledDisplay displays;
OneWire oneWire(PIN_ONEWIRE);
DallasTemperature tempSensors(&oneWire);

unsigned long lastInaTime = 0;
unsigned long lastTempReqTime = 0;
unsigned long lastLogTime = 0;
unsigned long lastOledTime = 0;
bool isWaitingTemp = false;
int ds18b20Count = 0;

void setup() {
  Serial.begin(115200);
  delay(500);
  LOG_PRINTLN("\n========================================================");
  LOG_PRINTLN("   TRAM SAC PIN 18650 2 KENH - ESP32 STANDALONE         ");
  LOG_PRINTLN("========================================================");

  // 1. Khởi tạo Bus I2C chung (G21 = SDA, G22 = SCL)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(100000);
  Wire.setTimeOut(30);

  for (int i = 0; i < NUM_SLOTS; i++) {
    slots[i].begin();
    LOG_PRINTF(">> Slot %d INA226: %s | Relay Pin: G%d\n",
               i + 1,
               slots[i].isOnline() ? "ONLINE" : "CHUA KET NOI",
               (i == 0 ? PIN_RELAY_SLOT1 : PIN_RELAY_SLOT2));
  }

  // 2. Khởi tạo 2 màn hình OLED O1 (0x3C) & O2 (0x3D)
  displays.begin();

  // 3. Quét cảm biến nhiệt độ DS18B20 trên chân G4
  tempSensors.begin();
  tempSensors.setWaitForConversion(false);
  ds18b20Count = tempSensors.getDeviceCount();

  LOG_PRINTF(">> DS18B20 (G%d): Phat hien %d cam bien tren bus.\n", PIN_ONEWIRE, ds18b20Count);
  if (ds18b20Count == 0) {
    LOG_PRINTLN("   [!] CANH BAO: Khong tim thay DS18B20! Kiem tra tro keo 4.7k.");
  } else {
    DeviceAddress da;
    for (int i = 0; i < ds18b20Count; i++) {
      if (tempSensors.getAddress(da, i)) {
        tempSensors.setResolution(da, 10);
      }
    }
  }

  LOG_PRINTLN(">> He thong san sang! Lap pin vao khay de tu dong sac.");
  LOG_PRINTLN(">> (Nhan '1' hoac '2' tren Serial de tam dung/tiep tuc thu cong)\n");
}

void loop() {
  unsigned long now = millis();

  // Task 1: Đọc nguồn INA226 & cập nhật trạng thái tự động (Mỗi 200ms)
  if (now - lastInaTime >= 200) {
    lastInaTime = now;
    for (int i = 0; i < NUM_SLOTS; i++) {
      slots[i].readPower();
      slots[i].updateStateMachine();
    }
  }

  // Task 2: Đọc nhiệt độ DS18B20 bất đồng bộ (Mỗi 1000ms)
  if (!isWaitingTemp && (now - lastTempReqTime >= 1000)) {
    tempSensors.requestTemperatures();
    lastTempReqTime = now;
    isWaitingTemp = true;
  }

  if (isWaitingTemp && (now - lastTempReqTime >= 200)) {
    ds18b20Count = tempSensors.getDeviceCount();

    if (ds18b20Count == 1) {
      float t = tempSensors.getTempCByIndex(0);
      if (t > -10.0f && t < 85.0f) {
        for (int i = 0; i < NUM_SLOTS; i++) slots[i].setTemperature(t);
      }
    } else if (ds18b20Count >= 2) {
      for (int i = 0; i < NUM_SLOTS; i++) {
        if (i < ds18b20Count) {
          float t = tempSensors.getTempCByIndex(i);
          if (t > -10.0f && t < 85.0f) {
            slots[i].setTemperature(t);
          }
        }
      }
    }
    isWaitingTemp = false;
  }

  // Task 3: Điều khiển bật/tắt thủ công qua Serial Monitor ('1' hoặc '2')
  int key = -1;
  if (Serial.available()) key = Serial.read();
  if (key >= '1' && key <= '2') {
    int idx = key - '1';
    bool targetState = !slots[idx].isRelayActive();
    bool ok = slots[idx].setRelay(targetState, true);
    if (targetState && !ok) {
      LOG_PRINTF("\n>>> [THU CONG] TU CHOI BAT Slot %d: Khong co pin hoac dang loi (V=%.3fV)! <<<\n",
                 idx + 1, slots[idx].getVoltage());
    } else {
      LOG_PRINTF("\n>>> [THU CONG] Da chuyen Relay Slot %d -> %s (V=%.3fV) <<<\n",
                 idx + 1, slots[idx].isRelayActive() ? "BAT" : "TAM DUNG", slots[idx].getVoltage());
    }
  }

  // Task 4: Cập nhật hiển thị lên 2 màn hình OLED (Mỗi 500ms)
  if (now - lastOledTime >= 500) {
    lastOledTime = now;
    displays.update(slots);
  }

  // Task 5: In thông số giám sát lên Serial Monitor (Mỗi 1.5 giây)
  if (now - lastLogTime >= 1500) {
    lastLogTime = now;
    LOG_PRINTF("[DATA] S1: %5.3fV | %5.3fA | %4.1f°C (%s) || S2: %5.3fV | %5.3fA | %4.1f°C (%s)\n",
               slots[0].getVoltage(), slots[0].getCurrent(), slots[0].getTemperature(), slots[0].getStatusText(),
               slots[1].getVoltage(), slots[1].getCurrent(), slots[1].getTemperature(), slots[1].getStatusText());
  }
}