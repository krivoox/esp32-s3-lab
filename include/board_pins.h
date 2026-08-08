#pragma once

#include <Arduino.h>
#include <Wire.h>

// Waveshare ESP32-S3-LCD-1.47B pinout
static constexpr int PIN_LCD_MOSI = 45;
static constexpr int PIN_LCD_SCLK = 40;
static constexpr int PIN_LCD_CS = 42;
static constexpr int PIN_LCD_DC = 41;
static constexpr int PIN_LCD_RST = 39;
static constexpr int PIN_LCD_BL = 46;

static constexpr int PIN_I2C_SDA = 48;
static constexpr int PIN_I2C_SCL = 47;

static constexpr int PIN_RGB = 38;
static constexpr int PIN_BOOT = 0;  // botón BOOT → siguiente slide

static constexpr int LCD_WIDTH = 172;
static constexpr int LCD_HEIGHT = 320;
static constexpr int LCD_COL_OFFSET = 34;
