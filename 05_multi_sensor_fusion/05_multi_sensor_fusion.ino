#include <Arduino.h>
#include <Wire.h>

// ============================================================================
// SCaDA Test 05: Dual-Sensor I2C Fusion (MAX30102 & MCP9808)
// Algoritma: IIR DC Filter + Peak Apex Responsif + Double-Trimmed Mean
//
// Pinout ESP32 (Hanya 4 kabel ke breadboard):
//   3V3 -> VCC/VDD MAX30102 & MCP9808
//   GND -> GND kedua sensor
//   D21 -> SDA kedua sensor
//   D22 -> SCL kedua sensor
// ============================================================================

#define MAX30102_ADDR 0x57
#define PIN_SDA 21
#define PIN_SCL 22

// --- State & Buffer MAX30102 ---
bool maxConnected = false;
float dc_ir = 0;
float dc_red = 0;
float prev_ac_ir = 0;

float max_ac_ir = -99999, min_ac_ir = 99999;
float max_ac_red = -99999, min_ac_red = 99999;
bool peak_registered = false;

const byte RATE_SIZE = 10;
float rates[RATE_SIZE] = {0};
byte rate_spot = 0;
byte rate_count = 0;

unsigned long last_beat_time = 0;
float bpm_val = 0;
float spo2_val = 0;

// --- State & Buffer MCP9808 ---
uint8_t mcpAddr = 0;
bool mcpConnected = false;
float latest_temp = 0.0f;
unsigned long last_temp_time = 0;

// Timer Serial Print
unsigned long last_print_time = 0;

// ============================================================================
// FUNGSI MAX30102
// ============================================================================
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
  writeReg(0x0A, 0x27); // 4096 ADC range, 100 Hz sample rate, 411us pulse width
  writeReg(0x0C, 0x24); // Red LED Current (~7.2 mA)
  writeReg(0x0D, 0x24); // IR LED Current (~7.2 mA)
}

// ============================================================================
// FUNGSI MCP9808
// ============================================================================
uint8_t scanMCP9808() {
  for (uint8_t addr = 0x18; addr <= 0x1F; addr++) {
    Wire.beginTransmission(addr);
    Wire.write(0x06); // Register Manufacturer ID (0x0054)
    if (Wire.endTransmission() == 0) {
      Wire.requestFrom(addr, (uint8_t)2);
      if (Wire.available() >= 2) {
        uint16_t mfgID = (Wire.read() << 8) | Wire.read();
        if (mfgID == 0x0054) return addr;
      }
    }
  }
  return 0;
}

float readTemperature(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write(0x05); // Register Ambient Temperature
  if (Wire.endTransmission() != 0) return -999.0f;

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

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================================");
  Serial.println("[SCaDA Test 05] Dual I2C Fusion: MAX30102 + MCP9808");
  Serial.println("========================================================");

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000); // 100 kHz I2C Standard

  // 1. Inisialisasi MAX30102
  Wire.beginTransmission(MAX30102_ADDR);
  if (Wire.endTransmission() == 0) {
    initMAX30102();
    maxConnected = true;
    Serial.println("[OK] MAX30102 terdeteksi di alamat 0x57");
  } else {
    Serial.println("[GAGAL] MAX30102 tidak merespons di 0x57!");
  }

  // 2. Inisialisasi MCP9808 via Auto-Scan
  mcpAddr = scanMCP9808();
  if (mcpAddr != 0) {
    mcpConnected = true;
    latest_temp = readTemperature(mcpAddr);
    Serial.printf("[OK] MCP9808 terdeteksi di alamat 0x%02X | Suhu awal: %.2f °C\n", mcpAddr, latest_temp);
  } else {
    Serial.println("[GAGAL] MCP9808 tidak ditemukan di alamat 0x18-0x1F!");
  }

  Serial.println("--------------------------------------------------------");
  Serial.println("Tempelkan jari telunjuk Anda dengan santai ke sensor...");
  Serial.println("========================================================\n");
}

// ============================================================================
// LOOP UTAMA
// ============================================================================
void loop() {
  unsigned long now = millis();

  // --------------------------------------------------------------------------
  // 1. Auto-Reconnect jika kabel sempat goyang
  // --------------------------------------------------------------------------
  if (!maxConnected) {
    Wire.beginTransmission(MAX30102_ADDR);
    if (Wire.endTransmission() == 0) {
      initMAX30102();
      maxConnected = true;
      Serial.println("[PULIH] MAX30102 berhasil tersambung kembali.");
    }
  }

  if (!mcpConnected) {
    mcpAddr = scanMCP9808();
    if (mcpAddr != 0) {
      mcpConnected = true;
      Serial.printf("[PULIH] MCP9808 ditemukan di alamat 0x%02X.\n", mcpAddr);
    }
  }

  // --------------------------------------------------------------------------
  // 2. Baca 6 Byte dari MAX30102
  // --------------------------------------------------------------------------
  if (maxConnected) {
    Wire.beginTransmission(MAX30102_ADDR);
    Wire.write(0x07);
    if (Wire.endTransmission(false) != 0) {
      maxConnected = false;
    } else {
      Wire.requestFrom((uint8_t)MAX30102_ADDR, (uint8_t)6);
      if (Wire.available() >= 6) {
        uint32_t red = ((uint32_t)Wire.read() << 16) | ((uint32_t)Wire.read() << 8) | Wire.read();
        uint32_t ir  = ((uint32_t)Wire.read() << 16) | ((uint32_t)Wire.read() << 8) | Wire.read();
        red &= 0x03FFFF;
        ir  &= 0x03FFFF;

        // Ambang batas jari menempel (25000 aman untuk semua tekanan jari)
        if (ir < 25000) {
          dc_ir = 0;
          dc_red = 0;
          bpm_val = 0;
          spo2_val = 0;
          last_beat_time = 0;
          rate_spot = 0;
          rate_count = 0;
          memset(rates, 0, sizeof(rates));
          prev_ac_ir = 0;
          peak_registered = false;
          max_ac_ir = -99999; min_ac_ir = 99999;
          max_ac_red = -99999; min_ac_red = 99999;
        } else {
          // Filter IIR DC Isolator (alpha 0.95)
          const float alpha = 0.95f;
          if (dc_ir == 0) {
            dc_ir = ir;
            dc_red = red;
          } else {
            dc_ir = alpha * dc_ir + (1.0f - alpha) * ir;
            dc_red = alpha * dc_red + (1.0f - alpha) * red;
          }

          // Ekstraksi Fluktuasi AC (Denyut Nadi)
          float ac_ir  = ir - dc_ir;
          float ac_red = red - dc_red;

          if (ac_ir > max_ac_ir)   max_ac_ir = ac_ir;
          if (ac_ir < min_ac_ir)   min_ac_ir = ac_ir;
          if (ac_red > max_ac_red) max_ac_red = ac_red;
          if (ac_red < min_ac_red) min_ac_red = ac_red;

          // Deteksi Puncak Nadi (Apex): Saat gelombang berbalik turun & amplitudo > 25
          if (prev_ac_ir > 25.0f && ac_ir < prev_ac_ir && !peak_registered) {
            peak_registered = true;

            // Inisialisasi patokan awal (anchor) jika baru mulai atau jeda terlalu lama
            if (last_beat_time == 0 || (now - last_beat_time) >= 2000) {
              last_beat_time = now;
            } else {
              unsigned long dt = now - last_beat_time;
              last_beat_time = now; // Selalu perbarui anchor ke denyut terbaru

              // Rentang fisiologis manusia (450ms = 133 BPM s/d 1500ms = 40 BPM)
              if (dt >= 450 && dt <= 1500) {
                float instant_bpm = 60000.0f / (float)dt;

                rates[rate_spot++] = instant_bpm;
                rate_spot %= RATE_SIZE;
                if (rate_count < RATE_SIZE) rate_count++;

                // Perhitungan BPM dengan perataan bertingkat
                if (rate_count >= 6) {
                  // Double-Trimmed Mean (buang 1 terendah dan 1 tertinggi)
                  float sorted[RATE_SIZE];
                  for (byte i = 0; i < rate_count; i++) sorted[i] = rates[i];
                  for (byte i = 0; i < rate_count - 1; i++) {
                    for (byte j = 0; j < rate_count - i - 1; j++) {
                      if (sorted[j] > sorted[j + 1]) {
                        float tmp = sorted[j];
                        sorted[j] = sorted[j + 1];
                        sorted[j + 1] = tmp;
                      }
                    }
                  }
                  float sum = 0.0f;
                  for (byte i = 1; i < rate_count - 1; i++) sum += sorted[i];
                  bpm_val = sum / (float)(rate_count - 2);
                } else {
                  // Rata-rata langsung agar angka pertama cepat muncul (2-3 detik)
                  float sum = 0.0f;
                  for (byte i = 0; i < rate_count; i++) sum += rates[i];
                  bpm_val = sum / (float)rate_count;
                }

                // Kalkulasi SpO2 (Ratio-of-Ratios R)
                float vpp_ir  = max_ac_ir - min_ac_ir;
                float vpp_red = max_ac_red - min_ac_red;

                if (vpp_ir > 15.0f && dc_ir > 1000.0f && dc_red > 1000.0f) {
                  float r = (vpp_red / dc_red) / (vpp_ir / dc_ir);
                  float calc_spo2 = 110.0f - 25.0f * r;

                  if (calc_spo2 > 100.0f) calc_spo2 = 99.0f;
                  if (calc_spo2 < 80.0f)  calc_spo2 = 94.0f;

                  if (spo2_val == 0) {
                    spo2_val = calc_spo2;
                  } else {
                    spo2_val = 0.80f * spo2_val + 0.20f * calc_spo2;
                  }
                }

                // Reset window min/max untuk denyut berikutnya
                max_ac_ir = -99999; min_ac_ir = 99999;
                max_ac_red = -99999; min_ac_red = 99999;
              }
            }
          } else if (ac_ir < 10.0f) {
            // Reset trigger saat gelombang turun kembali ke baseline
            peak_registered = false;
          }

          prev_ac_ir = ac_ir;
        }
      }
    }
  }

  // --------------------------------------------------------------------------
  // 3. Baca Suhu MCP9808 Setiap 1 Detik (Non-Blocking)
  // --------------------------------------------------------------------------
  if (mcpConnected && (now - last_temp_time >= 1000)) {
    last_temp_time = now;
    float t = readTemperature(mcpAddr);
    if (t > -50.0f && t < 100.0f) {
      latest_temp = t;
    } else {
      mcpConnected = false;
    }
  }

  // --------------------------------------------------------------------------
  // 4. Cetak Baris Telemetri Setiap 1 Detik
  // --------------------------------------------------------------------------
  if (now - last_print_time >= 1000) {
    last_print_time = now;

    if (dc_ir == 0) {
      Serial.printf("[FUSION] Menunggu Jari... | Suhu Kulit: %5.2f °C\n", latest_temp);
    } else if (bpm_val == 0) {
      Serial.printf("[FUSION] Jari Terdeteksi. Mengunci Detak (Sampel %d/6)... | Suhu: %5.2f °C\n", 
                    rate_count, latest_temp);
    } else {
      Serial.printf("[FUSION] AKTIF | Detak: %3.0f BPM | SpO2: %3.0f %% | Suhu Kulit: %5.2f °C\n",
                    bpm_val, spo2_val, latest_temp);
    }
  }

  // Jeda sampling MAX30102 sinkron ~25 Hz
  delay(38);
}
