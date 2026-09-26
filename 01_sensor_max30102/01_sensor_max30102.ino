#include <Arduino.h>
#include <Wire.h>

// ============================================================================
// SCaDA Test 01: MAX30102 Heart Rate & SpO2 Optic Sensor
// Pinout: VIN -> 3V3 | GND -> GND | SDA -> D21 | SCL -> D22
// I2C Address: 0x57
// ============================================================================

#define MAX30102_ADDR 0x57
#define PIN_SDA 21
#define PIN_SCL 22

void writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MAX30102_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

void initMAX30102() {
  writeReg(0x09, 0x40); // Reset chip
  delay(100);
  writeReg(0x04, 0x00); // FIFO Write Pointer = 0
  writeReg(0x05, 0x00); // Overflow Counter = 0
  writeReg(0x06, 0x00); // FIFO Read Pointer = 0
  writeReg(0x08, 0x4F); // Sample averaging 4
  writeReg(0x09, 0x03); // SpO2 mode (Red + IR LED aktif)
  writeReg(0x0A, 0x27); // 4096 ADC, 100 Hz sample rate, 411us pulse width
  writeReg(0x0C, 0x24); // Red LED Current (~7.2 mA)
  writeReg(0x0D, 0x24); // IR LED Current (~7.2 mA)
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("[TEST 01] Uji Sensor MAX30102 (HR & SpO2)");
  Serial.println("==========================================");

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
}

void loop() {
  static bool connected = false;

  // 1. Cek koneksi sensor jika belum terhubung
  if (!connected) {
    Wire.beginTransmission(MAX30102_ADDR);
    if (Wire.endTransmission() == 0) {
      initMAX30102();
      connected = true;
      Serial.println("[OK] MAX30102 terdeteksi di alamat 0x57. Tempelkan jari ke sensor...");
    } else {
      Serial.println("[MENCARI...] MAX30102 belum terdeteksi. Cek: VIN (3V3), GND, SDA (D21), SCL (D22).");
      delay(1000);
      return;
    }
  }

  // 2. Baca data mentah 6 byte dari FIFO
  Wire.beginTransmission(MAX30102_ADDR);
  Wire.write(0x07);
  if (Wire.endTransmission(false) != 0) {
    Serial.println("[LOST] Koneksi sensor terputus!");
    connected = false;
    delay(500);
    return;
  }

  Wire.requestFrom((uint8_t)MAX30102_ADDR, (uint8_t)6);
  if (Wire.available() >= 6) {
    uint32_t red = ((uint32_t)Wire.read() << 16) | ((uint32_t)Wire.read() << 8) | Wire.read();
    uint32_t ir  = ((uint32_t)Wire.read() << 16) | ((uint32_t)Wire.read() << 8) | Wire.read();
    red &= 0x03FFFF;
    ir  &= 0x03FFFF;

    if (ir < 10000) {
      Serial.println("[STATUS] Tidak ada jari terdeteksi.");
    } else {
      Serial.printf("[JARI DETEKSI] IR Raw: %6lu | RED Raw: %6lu\n", ir, red);
    }
  }

  delay(150);
}
