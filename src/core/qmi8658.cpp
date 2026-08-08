#include "qmi8658.h"

namespace {
constexpr uint8_t kAddrLow = 0x6B;
constexpr uint8_t kAddrHigh = 0x6A;
constexpr uint8_t kWhoAmI = 0x00;
constexpr uint8_t kCtrl1 = 0x02;
constexpr uint8_t kCtrl2 = 0x03;
constexpr uint8_t kCtrl3 = 0x04;
constexpr uint8_t kCtrl5 = 0x06;
constexpr uint8_t kCtrl7 = 0x08;
constexpr uint8_t kAxL = 0x35;
}

bool Qmi8658::writeReg(uint8_t reg, uint8_t value) {
  wire_->beginTransmission(addr_);
  wire_->write(reg);
  wire_->write(value);
  return wire_->endTransmission() == 0;
}

bool Qmi8658::readRegs(uint8_t reg, uint8_t* buf, size_t len) {
  wire_->beginTransmission(addr_);
  wire_->write(reg);
  if (wire_->endTransmission(false) != 0) {
    return false;
  }
  const size_t got = wire_->requestFrom(static_cast<uint16_t>(addr_), len);
  if (got != len) {
    return false;
  }
  for (size_t i = 0; i < len; ++i) {
    buf[i] = wire_->read();
  }
  return true;
}

bool Qmi8658::probe(uint8_t addr) {
  addr_ = addr;
  wire_->beginTransmission(addr_);
  if (wire_->endTransmission() != 0) {
    return false;
  }

  uint8_t who = 0;
  if (!readRegs(kWhoAmI, &who, 1)) {
    return false;
  }

  Serial.printf("QMI8658 probe 0x%02X WHO_AM_I=0x%02X\n", addr_, who);
  // Datasheet: 0x05. Algunos clones responden distinto pero != 0 / 0xFF.
  return who == 0x05 || (who != 0x00 && who != 0xFF);
}

bool Qmi8658::begin(TwoWire& wire, int sda, int scl) {
  wire_ = &wire;
  if (sda >= 0 && scl >= 0) {
    wire_->begin(sda, scl);
  } else {
    wire_->begin();
  }
  wire_->setClock(400000);
  delay(20);

  Serial.printf("I2C scan SDA=%d SCL=%d\n", sda, scl);
  for (uint8_t a = 1; a < 127; ++a) {
    wire_->beginTransmission(a);
    if (wire_->endTransmission() == 0) {
      Serial.printf("  found 0x%02X\n", a);
    }
  }

  ok_ = false;
  if (probe(kAddrLow) || probe(kAddrHigh)) {
    // addr_ ya quedó seteado por probe()
  } else {
    Serial.println("QMI8658 not found");
    return false;
  }

  // Secuencia alineada al demo de Waveshare
  writeReg(kCtrl1, 0x40);   // auto increment
  writeReg(kCtrl7, 0x43);   // hs clock + accel + gyro
  writeReg(kCtrl2, 0x95);   // ±8G, ODR ~125 Hz (aScale=2 << 4 | odr)
  writeReg(kCtrl3, 0x95);   // gyro scale/odr
  writeReg(kCtrl5, 0x11);   // LPF on

  // ±8G / ±512 dps approx scales matching CTRL bits above
  // CTRL2: scale bits [6:4], 0x95 => scale=1 (±4G) if we use 0x15...
  // Usamos ±4G / ±64 dps como el demo:
  writeReg(kCtrl2, 0x15);  // ±4G, ODR 250
  writeReg(kCtrl3, 0x15);  // ±64 dps, ODR 250
  writeReg(kCtrl7, 0x43);

  accel_scale_ = 4.0f / 32768.0f;
  gyro_scale_ = 64.0f / 32768.0f;
  delay(80);
  ok_ = true;
  Serial.printf("QMI8658 ready @ 0x%02X\n", addr_);
  return true;
}

bool Qmi8658::read(Vec3& accel_g, Vec3& gyro_dps) {
  if (!ok_) {
    return false;
  }

  uint8_t buf[12];
  if (!readRegs(kAxL, buf, 12)) {
    return false;
  }

  auto to_i16 = [](uint8_t lo, uint8_t hi) -> int16_t {
    return static_cast<int16_t>((static_cast<uint16_t>(hi) << 8) | lo);
  };

  accel_g.x = to_i16(buf[0], buf[1]) * accel_scale_;
  accel_g.y = to_i16(buf[2], buf[3]) * accel_scale_;
  accel_g.z = to_i16(buf[4], buf[5]) * accel_scale_;
  gyro_dps.x = to_i16(buf[6], buf[7]) * gyro_scale_;
  gyro_dps.y = to_i16(buf[8], buf[9]) * gyro_scale_;
  gyro_dps.z = to_i16(buf[10], buf[11]) * gyro_scale_;
  return true;
}
