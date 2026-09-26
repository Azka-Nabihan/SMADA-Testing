#include <Arduino.h>
#include <Wire.h>

// ============================================================================
// SCaDA Test 02: MCP9808 High-Accuracy Skin/Body Temperature Sensor
// Pinout: VDD -> 3V3 | GND -> GND | SDA -> D21 | SCL -> D22 | A0-A2 -> GND/Float
// I2C Address: 0x18 (Default)
// ============================================================================

#define PIN_SDA 21
#define PIN_SCL 22

uint8_t mcpAddr = 0;
bool isConnected = false;

// Cari sensor di rentang alamat MCP9808 (0x18 s.d 0x1F)
uint8_t scanMCP9808() {
  for (uint8_t addr = 0x18; addr <= 0x1F; addr++) {
    Wire.beginTransmission(addr);
    Wire.write(0x06); // Register Manufacturer ID (Harus 0x0054 untuk Microchip)
    if (Wire.endTransmission(false) == 0) {
      Wire.requestFrom(addr, (uint8_t)2);
      if (Wire.available() >= 2) {
        uint16_t mfgID = (Wire.read() << 8) | Wire.read();
        if (mfgID == 0x0054) return addr;
      }
    }
  }
  return 0;
}

// Baca suhu dari Register 0x05 (Ambient Temperature)
float readTemperature(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write(0x05);
  if (Wire.endTransmission(false) != 0) return -999.0f;

  Wire.requestFrom(addr, (uint8_t)2);
  if (Wire.available() < 2) return -999.0f;

  uint8_t upper = Wire.read() & 0x1F;
  uint8_t lower = Wire.read();

  if ((upper & 0x10) == 0x10) { // Suhu negatif (< 0 °C)
    upper = upper & 0x0F;
    return 256.0f - ((upper * 16.0f) + (lower / 16.0f));
  } else {
    return (upper * 16.0f) + (lower / 16.0f);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("[TEST 02] Uji Sensor Suhu MCP9808");
  Serial.println("==========================================");

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
}

void loop() {
  // 1. Auto-Scan jika belum terhubung
  if (!isConnected) {
    mcpAddr = scanMCP9808();
    if (mcpAddr != 0) {
      isConnected = true;
      Serial.printf("[OK] MCP9808 KETEMU di alamat 0x%02X! Membaca suhu...\n", mcpAddr);
    } else {
      Serial.println("[MENCARI...] MCP9808 belum terdeteksi. Cek VDD (3V3), GND, SDA (D21), SCL (D22).");
      delay(1000);
      return;
    }
  }

  // 2. Baca suhu real-time
  float tempC = readTemperature(mcpAddr);
  if (tempC < -900.0f) {
    Serial.println("[LOST] Koneksi sensor terputus!");
    isConnected = false;
    delay(500);
  } else {
    Serial.printf("[SUHU KULIT] %.2f °C\n", tempC);
    delay(500);
  }
}
