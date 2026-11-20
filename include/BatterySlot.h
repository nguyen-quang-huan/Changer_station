#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "Config.h"

class BatterySlot {
private:
  uint8_t _id;          //[cite: 16]
  uint8_t _relayPin;    //[cite: 16]
  uint8_t _inaAddr;     //[cite: 16]

  float _voltage;       //[cite: 16]
  float _current;       //[cite: 16]
  float _temperature;   //[cite: 16]

  bool _relayActive;    //[cite: 16]
  bool _inaOnline;      //[cite: 16]
  bool _manualPaused;
  bool _isWakePulsing;

  SlotStatus _status;   //[cite: 16]
  FaultCode _fault;     //[cite: 16]

  unsigned long _fullTimer;       //[cite: 16]
  unsigned long _standbyTimer;
  unsigned long _relayStartTime;  //[cite: 16]
  unsigned long _lastWakeTime;
  uint8_t _overVoltCount;         //[cite: 16]
  uint8_t _overTempCount;         //[cite: 16]
  uint8_t _safeRecoverCount;      //[cite: 16]
  uint8_t _noLoadCount;           //[cite: 16]

public:
  BatterySlot(uint8_t id, uint8_t relayPin, uint8_t inaAddr); //[cite: 16]

  void begin();                                     //[cite: 16]
  bool readPower();                                 //[cite: 16]
  void setTemperature(float t);                     //[cite: 16]
  void updateStateMachine();                        //[cite: 16]
  bool setRelay(bool enable, bool isManual = false);
  void resetFault();                                //[cite: 16]

  uint8_t getId() const { return _id; }                       //[cite: 16]
  float getVoltage() const { return _voltage; }               //[cite: 16]
  float getCurrent() const { return _current; }               //[cite: 16]
  float getTemperature() const { return _temperature; }       //[cite: 16]
  bool isRelayActive() const { return _relayActive; }         //[cite: 16]
  bool isOnline() const { return _inaOnline; }                //[cite: 16]
  bool hasBattery() const { return (_voltage >= BATTERY_PRESENT_V); } //[cite: 16]
  SlotStatus getStatus() const { return _status; }            //[cite: 16]
  FaultCode getFault() const { return _fault; }               //[cite: 16]
  const char* getStatusText() const;
};