// ============================================================
//  INA228.h - Simple I2C driver for INA228
//  Header-only for ESP32
// ============================================================
#pragma once
#include <Wire.h>

#define INA228_REG_CONFIG        0x00
#define INA228_REG_ADC_CONFIG    0x01
#define INA228_REG_SHUNT_CAL     0x02
#define INA228_REG_SHUNT_VOLTAGE 0x04
#define INA228_REG_BUS_VOLTAGE   0x05
#define INA228_REG_CURRENT       0x07
#define INA228_REG_POWER         0x08
#define INA228_REG_DEVICE_ID     0x3F

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
    _addr = addr; _wire = wire;
    writeReg16(INA228_REG_CONFIG, 0x8000); // soft reset
    delay(10);
    uint16_t id = readReg16(INA228_REG_DEVICE_ID);
    return !(id == 0 || id == 0xFFFF);
  }

  void configure(ADCRange range, ConvTime shuntCT, ConvTime busCT, AVGCount avg) {
    uint16_t cfg = readReg16(INA228_REG_CONFIG);
    if ((uint8_t)range) cfg |= (1<<4); else cfg &= ~(1<<4);
    writeReg16(INA228_REG_CONFIG, cfg);
    _range = range;

    // ADC_CONFIG register (0x01) — INA228 datasheet SBOS835 Table 7-7
    // [15:12] MODE    = 0xF  → continuous shunt + bus + temp
    // [11:9]  VBUSCT  = bus conversion time
    // [8:6]   VSHCT   = shunt conversion time
    // [5:3]   VTCT    = temp conv time (lasciamo default 0 = 50us)
    // [2:0]   AVG     = averaging count
    uint16_t adc = 0;
    adc |= ((uint16_t)(uint8_t)avg     << 0);   // [2:0]
    adc |= ((uint16_t)(uint8_t)shuntCT << 6);   // [8:6]
    adc |= ((uint16_t)(uint8_t)busCT   << 9);   // [11:9]
    adc |= ((uint16_t)0x0F             << 12);  // [15:12] continuous
    writeReg16(INA228_REG_ADC_CONFIG, adc);
  }

  void setShuntCal(float maxI, float rShunt) {
    _maxI = maxI; _rShunt = rShunt;
    _currentLSB = maxI / 524288.0f;
    float cal = 13107.2e6f * _currentLSB * rShunt;
    if (_range == ADCRange::RANGE_40MV) cal *= 4.0f;
    writeReg16(INA228_REG_SHUNT_CAL, (uint16_t)cal);
  }

  float getBusVoltage() {
    uint32_t raw = readReg24(INA228_REG_BUS_VOLTAGE);
    // BUS_VOLTAGE è unsigned 20-bit, i 4 bit bassi sono riservati
    uint32_t val = raw >> 4;
    return (float)val * 195.3125e-6f;
  }

  float getCurrent() {
    uint32_t raw = readReg24(INA228_REG_CURRENT);
    int32_t val = (int32_t)(raw >> 4);
    if (val & 0x80000) val |= 0xFFF00000;
    return val * _currentLSB;
  }

  float getPower() { return getBusVoltage() * getCurrent(); }

private:
  uint8_t _addr = 0x40;
  TwoWire* _wire = nullptr;
  float _rShunt = 0.00025f, _maxI = 200.0f;
  float _currentLSB = 200.0f / 524288.0f;
  ADCRange _range = ADCRange::RANGE_163MV;

  uint16_t readReg16(uint8_t reg) {
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0) return 0;
    _wire->requestFrom((int)_addr, 2);
    if (_wire->available() < 2) return 0;
    return ((uint16_t)_wire->read() << 8) | _wire->read();
  }
  uint32_t readReg24(uint8_t reg) {
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
  void writeReg16(uint8_t reg, uint16_t val) {
    _wire->beginTransmission(_addr);
    _wire->write(reg);
    _wire->write((val >> 8) & 0xFF);
    _wire->write(val & 0xFF);
    _wire->endTransmission();
  }
};