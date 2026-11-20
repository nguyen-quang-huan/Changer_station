#include "OledDisplay.h"

OledDisplay::OledDisplay()
  : _oled1(128, 64, &Wire, -1),
    _oled2(128, 64, &Wire, -1),
    _oled1Online(false),
    _oled2Online(false) {}

void OledDisplay::begin() {
  if (_oled1.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_SLOT1)) {
    _oled1Online = true;
    _oled1.clearDisplay();
    _oled1.setTextColor(SSD1306_WHITE);
    _oled1.display();
    LOG_PRINTF("[OLED] Man hinh O1 (0x%02X): KHOI TAO THANH CONG\n", OLED_ADDR_SLOT1);
  } else {
    LOG_PRINTF("[OLED] Man hinh O1 (0x%02X): KHONG TIM THAY!\n", OLED_ADDR_SLOT1);
  }

  if (_oled2.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR_SLOT2)) {
    _oled2Online = true;
    _oled2.clearDisplay();
    _oled2.setTextColor(SSD1306_WHITE);
    _oled2.display();
    LOG_PRINTF("[OLED] Man hinh O2 (0x%02X): KHOI TAO THANH CONG\n", OLED_ADDR_SLOT2);
  } else {
    LOG_PRINTF("[OLED] Man hinh O2 (0x%02X): KHONG TIM THAY!\n", OLED_ADDR_SLOT2);
  }
}

void OledDisplay::renderSlot(Adafruit_SSD1306& oled, const BatterySlot& slot) {
  oled.clearDisplay();
  oled.setTextSize(1);

  // Dòng tiêu đề: Kênh + Trạng thái + Trạng thái Relay [ON]/[OFF]
  oled.setCursor(0, 0);
  oled.printf("K%d:%s", slot.getId(), slot.getStatusText());
  oled.setCursor(98, 0);
  oled.print(slot.isRelayActive() ? "[ON]" : "[OFF]");
  oled.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Thông số đo đạc từ INA226 & DS18B20
  oled.setCursor(0, 16);
  oled.printf("Dien ap : %.2f V", slot.getVoltage());

  oled.setCursor(0, 31);
  oled.printf("Dong sac: %.0f mA", slot.getCurrent() * 1000.0f);

  oled.setCursor(0, 46);
  oled.printf("Nhiet do: %.1f C", slot.getTemperature());

  oled.display();
}

void OledDisplay::update(const BatterySlot slots[NUM_SLOTS]) {
  if (_oled1Online) {
    renderSlot(_oled1, slots[0]);
  }
  if (_oled2Online && NUM_SLOTS >= 2) {
    renderSlot(_oled2, slots[1]);
  }
}