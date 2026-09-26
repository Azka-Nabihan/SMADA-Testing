#include <Arduino.h>
#include <Wire.h>

// ============================================================================
// SCaDA Test 03: GY-LSM6DS3 6-DOF IMU (Accelerometer & Gyroscope)
// Pinout: VCC -> 3V3 | GND -> GND | CS -> 3V3 (Wajib I2C Mode!) | SDA -> D21 | SCL -> D22
// I2C Address: 0x6A (SA0 -> GND/Float) atau 0x6B (SA0 -> 3V3)
// ============================================================================

#define PIN_SDA 21
#define PIN_SCL 22

uint8_t sensorAddr = 0;
bool isReady = false;

uint8_t readChipID(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write(0x0F); // Register WHO_AM_I
  if (Wire.endTransmission(false) != 0) return 0x00;
  Wire.requestFrom(addr, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0x00;
}

void writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("[TEST 03] Uji Sensor IMU GY-LSM6DS3 (6-DOF)");
  Serial.println("==========================================");

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
}

void loop() {
  // 1. Auto-Scan jika belum terhubung
  if (!isReady) {
    uint8_t id6A = readChipID(0x6A);
    uint8_t id6B = readChipID(0x6B);

    if (id6A == 0x69) {
      sensorAddr = 0x6A;
      isReady = true;
    } else if (id6B == 0x69) {
      sensorAddr = 0x6B;
      isReady = true;
    }

    if (isReady) {
      Serial.printf("[OK] GY-LSM6DS3 KETEMU di 0x%02X! (Chip ID: 0x69)\n", sensorAddr);
      writeReg(sensorAddr, 0x10, 0x40); // Aktifkan Accel: 104 Hz, ±2g
      writeReg(sensorAddr, 0x11, 0x40); // Aktifkan Gyro: 104 Hz, 250 dps
    } else {
      Serial.println("[MENCARI...] LSM6DS3 belum kontak. Pastikan: CS ke 3V3, VCC 3.3V, GND, SDA D21, SCL D22.");
      delay(1000);
      return;
    }
  }

  // 2. Baca 12 Byte data mentah (Gyro: 0x22-0x27, Accel: 0x28-0x2D)
  Wire.beginTransmission(sensorAddr);
  Wire.write(0x22);
  if (Wire.endTransmission(false) != 0) {
    Serial.println("[LOST] Koneksi sensor terputus!");
    isReady = false;
    delay(500);
    return;
  }

  Wire.requestFrom(sensorAddr, (uint8_t)12);
  if (Wire.available() >= 12) {
    int16_t rawGX = (int16_t)(Wire.read() | (Wire.read() << 8));
    int16_t rawGY = (int16_t)(Wire.read() | (Wire.read() << 8));
    int16_t rawGZ = (int16_t)(Wire.read() | (Wire.read() << 8));
    int16_t rawAX = (int16_t)(Wire.read() | (Wire.read() << 8));
    int16_t rawAY = (int16_t)(Wire.read() | (Wire.read() << 8));
    int16_t rawAZ = (int16_t)(Wire.read() | (Wire.read() << 8));

    float gyroX = rawGX * 0.00875f; // Skala 250 dps
    float gyroY = rawGY * 0.00875f;
    float gyroZ = rawGZ * 0.00875f;
    float accX  = (rawAX * 0.061f) / 1000.0f; // Skala ±2g
    float accY  = (rawAY * 0.061f) / 1000.0f;
    float accZ  = (rawAZ * 0.061f) / 1000.0f;

    Serial.printf("[ACCEL g] X:%+5.2f Y:%+5.2f Z:%+5.2f | [GYRO dps] X:%+6.1f Y:%+6.1f Z:%+6.1f °/s\n",
                  accX, accY, accZ, gyroX, gyroY, gyroZ);
  }

  delay(200);
}
