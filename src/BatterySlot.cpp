#include "BatterySlot.h"

BatterySlot::BatterySlot(uint8_t id, uint8_t relayPin, uint8_t inaAddr)
  : _id(id),
    _relayPin(relayPin),
    _inaAddr(inaAddr),
    _voltage(0.0f),
    _current(0.0f),
    _temperature(25.0f),
    _relayActive(false),
    _inaOnline(false),
    _manualPaused(false),
    _isWakePulsing(false),
    _status(STATUS_EMPTY),
    _fault(FAULT_NONE),
    _fullTimer(0),
    _standbyTimer(0),
    _relayStartTime(0),
    _lastWakeTime(0),
    _overVoltCount(0),
    _overTempCount(0),
    _safeRecoverCount(0),
    _noLoadCount(0) {}

void BatterySlot::begin() {
  pinMode(_relayPin, OUTPUT);
  digitalWrite(_relayPin, RELAY_OFF);
  _relayActive = false;
  _isWakePulsing = false;

  Wire.beginTransmission(_inaAddr);
  if (Wire.endTransmission() == 0) {
    _inaOnline = true;
    Wire.beginTransmission(_inaAddr);
    Wire.write(0x00);
    Wire.write(0x45); // AVG = 16 mẫu trung bình, VBUSCT = 1.1ms[cite: 13]
    Wire.write(0x27);
    Wire.endTransmission();
    LOG_PRINTF("[INA226] Khe %d (0x%02X): KHOI TAO THANH CONG\n", _id, _inaAddr);
  } else {
    _inaOnline = false;
    LOG_PRINTF("[INA226] Khe %d (0x%02X): KHONG TIM THAY!\n", _id, _inaAddr);
  }
}

bool BatterySlot::readPower() {
  if (!_inaOnline) {
    _voltage = 0.0f;
    _current = 0.0f;
    return false;
  }

  // 1. Đọc Bus Voltage (Thanh ghi 0x02, 1 LSB = 1.25 mV)[cite: 13]
  Wire.beginTransmission(_inaAddr);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)_inaAddr, 2) != 2) return false;
  uint16_t rawBus = (Wire.read() << 8) | Wire.read();
  float vMeas = rawBus * 0.00125f;

  // 2. Đọc Shunt Voltage (Thanh ghi 0x01, 1 LSB = 2.5 uV, R_shunt = 0.1 Ohm)[cite: 13]
  Wire.beginTransmission(_inaAddr);
  Wire.write(0x01);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)_inaAddr, 2) != 2) return false;
  int16_t rawShunt = (int16_t)((Wire.read() << 8) | Wire.read());
  float vShunt_mV = rawShunt * 0.0025f;
  float iMeas = vShunt_mV / 100.0f;

  if (iMeas < 0.0f) iMeas = -iMeas;
  if (iMeas < 0.008f) iMeas = 0.0f; // Cắt nhiễu dòng tĩnh < 8mA[cite: 13]
  if (vMeas < 0.05f)  vMeas = 0.0f; // Cắt nhiễu áp thả nổi < 50mV khi khay trống

  _voltage = vMeas;
  _current = iMeas;
  return true;
}

void BatterySlot::setTemperature(float t) {
  if (t > -10.0f && t < 85.0f) {
    _temperature = t;
  }
}

bool BatterySlot::setRelay(bool enable, bool isManual) {
  _isWakePulsing = false;

  if (!enable) {
    _relayActive = false;
    digitalWrite(_relayPin, RELAY_OFF);
    _noLoadCount = 0;
    _fullTimer = 0;
    _standbyTimer = 0;
    _relayStartTime = 0;
    if (isManual) _manualPaused = true;

    _status = (_voltage >= BATTERY_PRESENT_V) ? STATUS_STANDBY : STATUS_EMPTY;
    if (_voltage <= SAFE_RECOVER_V) {
      _fault = FAULT_NONE;
      _overVoltCount = 0;
    }
    return true;
  }

  if (!_inaOnline) {
    LOG_PRINTF("[KHE %d] TU CHOI BAT: Cam bien INA226 dang offline!\n", _id);
    return false;
  }

  if (_status == STATUS_FAULT || _voltage > HARD_MAX_VOLTAGE || _temperature > HARD_MAX_TEMP) {
    LOG_PRINTF("[KHE %d] TU CHOI BAT: Qua nguong an toan (V=%.3fV, T=%.1fC)!\n",
               _id, _voltage, _temperature);
    _relayActive = false;
    digitalWrite(_relayPin, RELAY_OFF);
    return false;
  }

  if (!_relayActive && _voltage < BATTERY_PRESENT_V) {
    LOG_PRINTF("[KHE %d] TU CHOI BAT: KHAY KHONG CO PIN (V=%.3fV < %.2fV)!\n",
               _id, _voltage, BATTERY_PRESENT_V);
    _relayActive = false;
    digitalWrite(_relayPin, RELAY_OFF);
    _status = STATUS_EMPTY;
    return false;
  }

  if (isManual) _manualPaused = false;
  _relayActive = true;
  _relayStartTime = millis();
  digitalWrite(_relayPin, RELAY_ON);
  _status = STATUS_CHARGING;
  _fullTimer = 0;
  _standbyTimer = 0;
  _noLoadCount = 0;
  LOG_PRINTF("[KHE %d] DONG RELAY SAC (Pin V=%.3fV, I=%.3fA)\n", _id, _voltage, _current);
  return true;
}

void BatterySlot::updateStateMachine() {
  if (!_inaOnline) {
    setRelay(false);
    _status = STATUS_EMPTY;
    _fault = FAULT_NONE;
    return;
  }

  unsigned long now = millis();

  // 1. BẢO VỆ QUÁ ÁP[cite: 13]
  if (_voltage > HARD_MAX_VOLTAGE) {
    _overVoltCount++;
    if (_overVoltCount >= 3) {
      if (_relayActive) {
        setRelay(false);
        LOG_PRINTF("[KHE %d] CANH BAO QUA AP (V=%.3fV) -> NGAT RELAY!\n", _id, _voltage);
      }
      _status = STATUS_FAULT;
      _fault = FAULT_OVER_VOLTAGE;
      _safeRecoverCount = 0;
      return;
    }
  } else {
    _overVoltCount = 0;
  }

  // 2. BẢO VỆ QUÁ NHIỆT[cite: 13]
  if (_temperature > HARD_MAX_TEMP) {
    _overTempCount++;
    if (_overTempCount >= 4) {
      if (_relayActive) {
        setRelay(false);
        LOG_PRINTF("[KHE %d] CANH BAO QUA NHIET (T=%.1fC) -> NGAT RELAY!\n", _id, _temperature);
      }
      _status = STATUS_FAULT;
      _fault = FAULT_OVER_TEMP;
      _safeRecoverCount = 0;
      return;
    }
  } else {
    _overTempCount = 0;
  }

  // 3. TỰ ĐỘNG PHỤC HỒI TỪ FAULT[cite: 13]
  if (_status == STATUS_FAULT || _fault != FAULT_NONE) {
    if (!_relayActive && !_isWakePulsing && _voltage <= SAFE_RECOVER_V && _temperature <= SAFE_RECOVER_TEMP) {
      _safeRecoverCount++;
      if (_safeRecoverCount >= 2) {
        resetFault();
        LOG_PRINTF("[KHE %d] THONG SO AN TOAN (V=%.3fV, T=%.1fC) -> DA XOA LOI!\n",
                   _id, _voltage, _temperature);
      }
    } else {
      _safeRecoverCount = 0;
    }
    return;
  }

 // 4. PHÁT HIỆN RÚT PIN HOẶC LỖI HỞ MẠCH KHI ĐANG SẠC (Ân hạn 2.0s sau khi đóng Relay)
  if (_relayActive) {
    bool gracePeriodOver = (_relayStartTime > 0 && (now - _relayStartTime >= 2000UL));

    // Trường hợp A: Áp tụt hẳn xuống dưới 1.50V -> Chắc chắn đã rút pin
    if (gracePeriodOver && _voltage < BATTERY_PRESENT_V) {
      LOG_PRINTF("[KHE %d] DA RUT PIN (V=%.3fV) -> NGAT RELAY!\n", _id, _voltage);
      setRelay(false);
      _status = STATUS_EMPTY;
      return;
    }

    // Trường hợp B: Dòng về 0mA nhưng áp đang thấp (< 4.10V) -> Hở tiếp xúc khay pin, tạm dừng để không nhảy relay
    if (gracePeriodOver && _current < MIN_CHARGE_LOAD_I && _voltage < 4.10f) {
      _noLoadCount++;
      if (_noLoadCount >= 5) {
        LOG_PRINTF("[KHE %d] HO TIEP XUC / KHONG DONG TAI (V=%.3fV, I=%.3fA) -> TAM DUNG!\n",
                   _id, _voltage, _current);
        setRelay(false, true); // Đặt cờ tạm dừng để không tự đóng lại liên tục
        _noLoadCount = 0;
        return;
      }
    } else {
      _noLoadCount = 0;
    }
  } else {
    _noLoadCount = 0;
  }

  // 5. MÁY TRẠNG THÁI VẬN HÀNH (CHỐT ĐẦY PIN & TỰ ĐỘNG SẠC)
  if (_relayActive) {
    _status = STATUS_CHARGING;
    bool gracePeriodOver = (_relayStartTime > 0 && (now - _relayStartTime >= 2000UL));

    // Khi áp đạt >= 4.10V và dòng giảm <= 80mA (bao gồm cả 0mA khi TP4056 tự ngắt lúc đầy)
    if (gracePeriodOver && _voltage >= 4.10f && _current <= FULL_CHARGE_I) {
      if (_fullTimer == 0) {
        _fullTimer = now;
      } else if (now - _fullTimer >= FULL_TIME_MS) {
        setRelay(false);
        _status = STATUS_FULL; // Khóa cứng trạng thái ĐẦY PIN
        _fullTimer = 0;
        LOG_PRINTF("[KHE %d] PIN DA DAY (V=%.3fV, I=%.3fA) -> NGAT VA KHOA SAC!\n",
                   _id, _voltage, _current);
      }
    } else {
      _fullTimer = 0;
    }
  } else {
    _fullTimer = 0;
    _isWakePulsing = false;
    digitalWrite(_relayPin, RELAY_OFF);

    // Chỉ mở khóa trạng thái DAY PIN khi người dùng thực sự rút pin ra (áp < 1.50V)
    if (_voltage < BATTERY_PRESENT_V) {
      _status = STATUS_EMPTY;
      _manualPaused = false;
      _standbyTimer = 0;
    } else if (_status != STATUS_FULL) {
      _status = STATUS_STANDBY;
      if (!_manualPaused && _voltage < FULL_CHARGE_V && _temperature <= SAFE_RECOVER_TEMP) {
        if (_standbyTimer == 0) {
          _standbyTimer = now;
        } else if (now - _standbyTimer >= AUTO_CHARGE_MS) {
          setRelay(true);
        }
      } else {
        _standbyTimer = 0;
      }
    }
  }
}

void BatterySlot::resetFault() {
  _fault = FAULT_NONE;
  _overVoltCount = 0;
  _overTempCount = 0;
  _safeRecoverCount = 0;
  _noLoadCount = 0;
  _standbyTimer = 0;
  _isWakePulsing = false;
  _status = (_voltage >= BATTERY_PRESENT_V) ? STATUS_STANDBY : STATUS_EMPTY;
}

const char* BatterySlot::getStatusText() const {
  if (!_inaOnline) return "LOI CAM BIEN";
  if (_manualPaused && _status == STATUS_STANDBY) return "TAM DUNG";
  switch (_status) {
    case STATUS_EMPTY:    return "TRONG";
    case STATUS_STANDBY:  return "CHO SAC";
    case STATUS_CHARGING: return "DANG SAC";
    case STATUS_FULL:     return "DAY PIN";
    case STATUS_FAULT:
      if (_fault == FAULT_OVER_VOLTAGE) return "QUA AP!";
      if (_fault == FAULT_OVER_TEMP)    return "QUA NHIET!";
      return "LOI!";
    default:              return "CHO";
  }
}