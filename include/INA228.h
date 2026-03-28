// ============================================================
//  INA228.h - Driver INA228 riscritto seguendo RobTillaart
//  Corretto ordine byte e sign extension registro CURRENT
// ============================================================
#pragma once
#include <Wire.h>

// Registri
#define INA228_REG_CONFIG        0x00
#define INA228_REG_ADC_CONFIG    0x01
#define INA228_REG_SHUNT_CAL     0x02
#define INA228_REG_SHUNT_VOLTAGE 0x04
#define INA228_REG_BUS_VOLTAGE   0x05
#define INA228_REG_CURRENT       0x07
#define INA228_REG_POWER         0x08
#define INA228_REG_DEVICE_ID     0x3F

// ADC Config
#define INA228_MODE_CONT_TEMP_BUS_SHUNT  0x0F

enum class ADCRange : uint8_t { RANGE_163MV = 0, RANGE_40MV = 1 };
enum class ConvTime : uint8_t {
  CT_50US=0, CT_84US=1, CT_150US=2, CT_280US=3,
  CT_540US=4, CT_1052US=5, CT_2074US=6, CT_4120US=7
};
enum class AVGCount : uint8_t {
  AVG_1=0, AVG_4=1, AVG_16=2, AVG_64=3,
  AVG_128=4, AVG_256=5, AVG_512=6, AVG_1024=7
};

class INA228Driver {
public:
  INA228Driver(uint8_t addr = 0x40) : _addr(addr) {}

  bool begin(uint8_t addr = 0x40, TwoWire* wire = &Wire) {
    _addr = addr;
    _wire = wire;
    // Soft reset
    _writeReg16(INA228_REG_CONFIG, 0x8000);
    delay(10);
    uint16_t id = _readReg16(INA228_REG_DEVICE_ID);
    return !(id == 0 || id == 0xFFFF);
  }

  void configure(ADCRange range, ConvTime shuntCT, ConvTime busCT, AVGCount avg) {
    // CONFIG register: bit 4 = ADCRANGE
    uint16_t cfg = _readReg16(INA228_REG_CONFIG);
    if ((uint8_t)range) cfg |= (1 << 4);
    else                cfg &= ~(1 << 4);
    _writeReg16(INA228_REG_CONFIG, cfg);
    _ADCRange = (range == ADCRange::RANGE_40MV);

    // ADC_CONFIG register layout (datasheet Table 7-7):
    // [15:12] MODE    = 0xF  continuous shunt+bus+temp
    // [11:9]  VBUSCT  = bus conversion time
    // [8:6]   VSHCT   = shunt conversion time
    // [5:3]   VTCT    = temp conversion time (default 0)
    // [2:0]   AVG     = averaging count
    uint16_t adc = 0;
    adc |= ((uint16_t)(uint8_t)avg     << 0);
    adc |= ((uint16_t)(uint8_t)shuntCT << 6);
    adc |= ((uint16_t)(uint8_t)busCT   << 9);
    adc |= ((uint16_t)INA228_MODE_CONT_TEMP_BUS_SHUNT << 12);
    _writeReg16(INA228_REG_ADC_CONFIG, adc);
  }

  // Calibrazione seguendo RobTillaart / datasheet INA228 pag 31
  void setShuntCal(float maxCurrent, float rShunt) {
    _maxCurrent = maxCurrent;
    _rShunt     = rShunt;
    // currentLSB = maxCurrent / 2^19
    _currentLSB = _maxCurrent * 1.9073486328125e-6f;
    // SHUNT_CAL = 13107.2e6 * currentLSB * Rshunt
    // RANGE_163MV: nessun fattore correttivo
    // RANGE_40MV:  CAL *= 4 (shunt voltage LSB 4x più piccolo)
    float cal = 13107.2e6f * _currentLSB * _rShunt;
    if (_ADCRange) cal *= 4.0f;
    uint16_t calVal = (uint16_t)constrain(cal, 0, 65535);
    Serial.printf("[CAL] maxI=%.1f rShunt=%.6f LSB=%.8f cal=%.2f reg=%d ADCRange=%d\n",
                  _maxCurrent, _rShunt, _currentLSB, cal, calVal, _ADCRange ? 1 : 0);
    _writeReg16(INA228_REG_SHUNT_CAL, calVal);
    delay(5);
    uint16_t check = _readReg16(INA228_REG_SHUNT_CAL);
    Serial.printf("[CAL] verifica registro=%d %s\n", check, check == calVal ? "OK" : "ERRORE!");
  }

  float getBusVoltage() {
    // Registro BUS_VOLTAGE: 24 bit, unsigned, 20 bit utili [23:4]
    // LSB = 195.3125 µV
    uint32_t raw = _readReg24(INA228_REG_BUS_VOLTAGE);
    uint32_t val = raw >> 4;
    return (float)val * 195.3125e-6f;
  }

float getCurrent() {
  uint32_t raw = _readReg24(INA228_REG_CURRENT);
  int32_t val = (int32_t)(raw >> 4);
  if (val & 0x00080000) val |= 0xFFF00000;
  
  // DEBUG
  static unsigned long lastDbgMs = 0;
  if (millis() - lastDbgMs > 5000UL) {
    lastDbgMs = millis();
    Serial.printf("[INA RAW] raw24bit=0x%06X | val20bit=%ld (0x%05X) | LSB=%.8f | I=%.4f A\n",
                  raw, val, val & 0xFFFFF, _currentLSB, (float)val * _currentLSB);
  }
  
  return (float)val * _currentLSB;
}

  float getPower() {
    return getBusVoltage() * getCurrent();
  }

private:
  uint8_t   _addr      = 0x40;
  TwoWire*  _wire      = nullptr;
  float     _rShunt    = 0.00025f;
  float     _maxCurrent= 300.0f;
  float     _currentLSB= 300.0f * 1.9073486328125e-6f;
  bool      _ADCRange  = false;  // false=163mV, true=40mV

  uint16_t _readReg16(uint8_t reg) {
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0) return 0;
    _wire->requestFrom((int)_addr, 2);
    if (_wire->available() < 2) return 0;
    uint16_t v = (uint16_t)_wire->read() << 8;
    v |= _wire->read();
    return v;
  }

  uint32_t _readReg24(uint8_t reg) {
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0) return 0;
    _wire->requestFrom((int)_addr, 3);
    if (_wire->available() < 3) return 0;
    uint32_t v = (uint32_t)_wire->read() << 16;
    v |= (uint32_t)_wire->read() << 8;
    v |= _wire->read();
    return v;
  }

  void _writeReg16(uint8_t reg, uint16_t val) {
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    _wire->write((val >> 8) & 0xFF);
    _wire->write(val & 0xFF);
    _wire->endTransmission();
  }
};