#include <Arduino.h>
#include <Wire.h>

// ============================================================================
// SCaDA Test 05: Multi-Sensor I2C Bus Fusion
// Membaca 3 Sensor secara simultan pada bus I2C paralel:
// 1. MAX30102   : 0x57 (Heart Rate & SpO2)
// 2. MCP9808    : 0x18 (Body Temperature)
// 3. GY-LSM6DS3 : 0x6A (6-DOF IMU Motion & Orientation)
//
// Wiring Bus Bersama:
// - Semua VCC/VDD -> 3V3
// - Semua GND     -> GND
// - Semua SDA     -> GPIO 21
// - Semua SCL     -> GPIO 22
// - Pin CS LSM6DS3 -> 3V3 (Wajib)
// ============================================================================

#define PIN_SDA 21
#define PIN_SCL 22

#define ADDR_MAX30102   0x57
#define ADDR_MCP9808    0x18
#define ADDR_LSM6DS3    0x6A

bool maxReady = false;
bool mcpReady = false;
bool lsmReady = false;

void writeI2C(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

void initAllSensors() {
  // 1. Init MAX30102
  Wire.beginTransmission(ADDR_MAX30102);
  if (Wire.endTransmission() == 0) {
    writeI2C(ADDR_MAX30102, 0x09, 0x40); // Reset
    delay(50);
    writeI2C(ADDR_MAX30102, 0x04, 0x00);
    writeI2C(ADDR_MAX30102, 0x05, 0x00);
    writeI2C(ADDR_MAX30102, 0x06, 0x00);
    writeI2C(ADDR_MAX30102, 0x08, 0x4F); // Sample avg 4
    writeI2C(ADDR_MAX30102, 0x09, 0x03); // SpO2 mode (Red + IR)
    writeI2C(ADDR_MAX30102, 0x0A, 0x27); // 4096 ADC, 100 Hz
    writeI2C(ADDR_MAX30102, 0x0C, 0x24); // Red LED ~7.2mA
    writeI2C(ADDR_MAX30102, 0x0D, 0x24); // IR LED ~7.2mA
    maxReady = true;
  } else {
    maxReady = false;
  }

  // 2. Init MCP9808
  Wire.beginTransmission(ADDR_MCP9808);
  if (Wire.endTransmission() == 0) {
    mcpReady = true;
  } else {
    mcpReady = false;
  }

  // 3. Init GY-LSM6DS3
  Wire.beginTransmission(ADDR_LSM6DS3);
  if (Wire.endTransmission() == 0) {
    writeI2C(ADDR_LSM6DS3, 0x10, 0x40); // Accel 104Hz, ±2g
    writeI2C(ADDR_LSM6DS3, 0x11, 0x40); // Gyro 104Hz, 250dps
    lsmReady = true;
  } else {
    lsmReady = false;
  }
}

float readMCP9808Temp() {
  if (!mcpReady) return -999.0f;
  Wire.beginTransmission(ADDR_MCP9808);
  Wire.write(0x05);
  if (Wire.endTransmission(false) != 0) return -999.0f;
  Wire.requestFrom((uint8_t)ADDR_MCP9808, (uint8_t)2);
  if (Wire.available() < 2) return -999.0f;
  uint8_t upper = Wire.read() & 0x1F;
  uint8_t lower = Wire.read();
  if ((upper & 0x10) == 0x10) {
    upper &= 0x0F;
    return 256.0f - ((upper * 16.0f) + (lower / 16.0f));
  }
  return (upper * 16.0f) + (lower / 16.0f);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================================");
  Serial.println("[TEST 05] Uji Multi-Sensor I2C Bus Fusion (SCaDA Core)");
  Serial.println("========================================================");

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);

  initAllSensors();
  Serial.printf("Status Deteksi: MAX30102: [%s] | MCP9808: [%s] | LSM6DS3: [%s]\n\n",
                maxReady ? "OK" : "FAIL",
                mcpReady ? "OK" : "FAIL",
                lsmReady ? "OK" : "FAIL");
}

void loop() {
  // Pastikan sensor terinisialisasi jika sempat lepas
  if (!maxReady || !mcpReady || !lsmReady) {
    initAllSensors();
  }

  // 1. Baca MAX30102
  uint32_t ir = 0, red = 0;
  if (maxReady) {
    Wire.beginTransmission(ADDR_MAX30102);
    Wire.write(0x07);
    if (Wire.endTransmission(false) == 0) {
      Wire.requestFrom((uint8_t)ADDR_MAX30102, (uint8_t)6);
      if (Wire.available() >= 6) {
        red = ((uint32_t)Wire.read() << 16) | ((uint32_t)Wire.read() << 8) | Wire.read();
        ir  = ((uint32_t)Wire.read() << 16) | ((uint32_t)Wire.read() << 8) | Wire.read();
        red &= 0x03FFFF;
        ir  &= 0x03FFFF;
      }
    }
  }

  // 2. Baca MCP9808
  float tempC = readMCP9808Temp();

  // 3. Baca GY-LSM6DS3
  float accZ = 0.0f, gyroZ = 0.0f;
  if (lsmReady) {
    Wire.beginTransmission(ADDR_LSM6DS3);
    Wire.write(0x22);
    if (Wire.endTransmission(false) == 0) {
      Wire.requestFrom((uint8_t)ADDR_LSM6DS3, (uint8_t)12);
      if (Wire.available() >= 12) {
        Wire.read(); Wire.read(); // GX
        Wire.read(); Wire.read(); // GY
        int16_t rawGZ = (int16_t)(Wire.read() | (Wire.read() << 8));
        Wire.read(); Wire.read(); // AX
        Wire.read(); Wire.read(); // AY
        int16_t rawAZ = (int16_t)(Wire.read() | (Wire.read() << 8));
        gyroZ = rawGZ * 0.00875f;
        accZ  = (rawAZ * 0.061f) / 1000.0f;
      }
    }
  }

  // Cetak baris data gabungan
  Serial.printf("[FUSION DATA] Suhu: %5.2f°C | PPG IR: %6lu | Accel Z: %+5.2fg | Gyro Z: %+5.1f°/s\n",
                tempC, ir, accZ, gyroZ);

  delay(200);
}
