#pragma once

#include <Arduino.h>
#include <Wire.h>

struct Vec3 {
  float x = 0;
  float y = 0;
  float z = 0;
};

class Qmi8658 {
 public:
  bool begin(TwoWire& wire = Wire, int sda = -1, int scl = -1);
  bool read(Vec3& accel_g, Vec3& gyro_dps);

  uint8_t address() const { return addr_; }
  bool ok() const { return ok_; }

 private:
  bool probe(uint8_t addr);
  bool writeReg(uint8_t reg, uint8_t value);
  bool readRegs(uint8_t reg, uint8_t* buf, size_t len);

  TwoWire* wire_ = nullptr;
  uint8_t addr_ = 0;
  bool ok_ = false;
  float accel_scale_ = 4.0f / 32768.0f;
  float gyro_scale_ = 64.0f / 32768.0f;
};
