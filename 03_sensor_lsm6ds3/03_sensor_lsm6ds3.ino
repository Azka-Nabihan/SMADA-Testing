#include <Arduino.h>
#include <Wire.h>
#include "SparkFunLSM6DS3.h"

// ============================================================================
// SCaDA Test 03: SparkFun LSM6DS3 Official Library
// Pinout: VCC -> 3V3 | GND -> GND | CS -> 3V3 (Wajib I2C!) | SDA -> D21 | SCL -> D22
// ============================================================================

#define PIN_SDA 21
#define PIN_SCL 22

// Coba inisialisasi pada alamat default 0x6A
LSM6DS3 myIMU(I2C_MODE, 0x6A);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("[TEST 03] Uji Sensor LSM6DS3 (Official Library)");
  Serial.println("==========================================");

  // 1. Kunci pin I2C ESP32 secara eksplisit
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);

  // 2. Coba koneksi ke alamat 0x6A
  Serial.println("[INIT] Menghubungkan ke alamat 0x6A...");
  status_t status = myIMU.begin();

  if (status != IMU_SUCCESS) {
    Serial.printf("[INFO] Alamat 0x6A gagal (Code: %d). Mencoba alamat alternatif 0x6B...\n", status);
    
    // Alihkan ke alamat 0x6B
    LSM6DS3 myIMU_Alt(I2C_MODE, 0x6B);
    status = myIMU_Alt.begin();

    if (status != IMU_SUCCESS) {
      Serial.printf("[GAGAL] Kedua alamat (0x6A & 0x6B) gagal kontak (Code: %d)!\n", status);
      Serial.println("Periksa hardware: Pin CS wajib ke 3V3, VCC 3.3V, GND, SDA D21, SCL D22.");
      while (1);
    } else {
      myIMU = myIMU_Alt; // Salin konfigurasi yang sukses
      Serial.println("[OK] BERHASIL terkoneksi di alamat 0x6B!");
    }
  } else {
    Serial.println("[OK] BERHASIL terkoneksi di alamat 0x6A!");
  }

  Serial.println("Mulai membaca data sensor IMU...\n");
}

void loop() {
  // Baca data akselerasi (g)
  float ax = myIMU.readFloatAccelX();
  float ay = myIMU.readFloatAccelY();
  float az = myIMU.readFloatAccelZ();

  // Baca data rotasi gyro (derajat per detik / dps)
  float gx = myIMU.readFloatGyroX();
  float gy = myIMU.readFloatGyroY();
  float gz = myIMU.readFloatGyroZ();

  // Cetak hasil pembacaan
  Serial.printf("[ACCEL g] X:%+5.2f Y:%+5.2f Z:%+5.2f | [GYRO dps] X:%+6.1f Y:%+6.1f Z:%+6.1f °/s\n",
                ax, ay, az, gx, gy, gz);

  delay(400);
}
