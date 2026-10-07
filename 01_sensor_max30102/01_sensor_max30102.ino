#include <Arduino.h>
#include <Wire.h>

// ============================================================================
// SCaDA Test 01: MAX30102 Heart Rate (BPM) & SpO2 Optic Sensor
// Algoritma: IIR AC-DC Isolator + Peak Apex Detection + 4-Sample Moving Avg
// Pinout: VIN -> 3V3 | GND -> GND | SDA -> D21 | SCL -> D22
// I2C Address: 0x57
// ============================================================================

#define MAX30102_ADDR 0x57
#define PIN_SDA 21
#define PIN_SCL 22

// --- Filter State & Buffer ---
float dc_ir = 0;
float dc_red = 0;
float prev_ac_ir = 0;

float max_ac_ir = -99999, min_ac_ir = 99999;
float max_ac_red = -99999, min_ac_red = 99999;

bool peak_registered = false;

// 12-Sample Circular Buffer & Double-Trimmed Mean untuk Stabilitas Medis
const byte RATE_SIZE = 12;
float rates[RATE_SIZE] = {0};
byte rate_spot = 0;
byte rate_count = 0;

unsigned long last_beat_time = 0;
float bpm_val = 0;
float spo2_val = 0;

unsigned long last_print_time = 0;

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

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("[SCaDA] MAX30102 Peak-Apex & Moving Average");
  Serial.println("==========================================");

  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(100000);
}

void loop() {
  static bool connected = false;

  // 1. Inisialisasi awal / pemulihan koneksi I2C
  if (!connected) {
    Wire.beginTransmission(MAX30102_ADDR);
    if (Wire.endTransmission() == 0) {
      initMAX30102();
      connected = true;
      Serial.println("[OK] MAX30102 siap. Tempelkan jari Anda ke sensor...");
    } else {
      Serial.println("[MENCARI...] Sensor belum terbaca. Periksa kabel D21/D22 & 3V3.");
      delay(1000);
      return;
    }
  }

  // 2. Baca 6 byte dari FIFO (3 byte Red + 3 byte IR)
  Wire.beginTransmission(MAX30102_ADDR);
  Wire.write(0x07);
  if (Wire.endTransmission(false) != 0) {
    Serial.println("[LOST] Sambungan sensor terputus!");
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

    // 3. Deteksi keberadaan jari
    if (ir < 50000) {
      // Jari diangkat: bersihkan seluruh state agar tidak menyimpan residu lama
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
      // 4. Estimasi Garis Dasar DC (IIR Filter Ringan)
      const float alpha = 0.95;
      if (dc_ir == 0) {
        dc_ir = ir;
        dc_red = red;
      } else {
        dc_ir = alpha * dc_ir + (1.0 - alpha) * ir;
        dc_red = alpha * dc_red + (1.0 - alpha) * red;
      }

      // 5. Ekstraksi Gelombang AC
      float ac_ir = ir - dc_ir;
      float ac_red = red - dc_red;

      // Rekam puncak dan lembah untuk kalkulasi SpO2
      if (ac_ir > max_ac_ir)   max_ac_ir = ac_ir;
      if (ac_ir < min_ac_ir)   min_ac_ir = ac_ir;
      if (ac_red > max_ac_red) max_ac_red = ac_red;
      if (ac_red < min_ac_red) min_ac_red = ac_red;

      // 6. Deteksi Puncak Sejati (Peak Apex: saat gelombang berbalik arah dari naik ke turun)
      // Opsi 1 Tuning: Ambang > 85 & jeda minimum 580ms (mengunci detak istirahat ~80 BPM)
      if (prev_ac_ir > 85 && ac_ir < prev_ac_ir && !peak_registered) {
        unsigned long now = millis();

        // Kunci waktu denyut pertama sebagai patokan awal (anchor)
        if (last_beat_time == 0 || (now - last_beat_time) >= 2200) {
          last_beat_time = now;
        } else {
          unsigned long dt = now - last_beat_time;

          // Jeda fisiologis istirahat valid: 650ms (92 BPM) s/d 1400ms (43 BPM)
          // Batas 650ms memblokir 100% pantulan gelombang dikrotik (~580-620ms pada detak 70-an BPM)
          if (dt >= 650 && dt <= 1400) {
            float instant_bpm = 60000.0 / (float)dt;

            // Masukkan ke Circular Buffer 12-Sample
            rates[rate_spot++] = instant_bpm;
            rate_spot %= RATE_SIZE;
            if (rate_count < RATE_SIZE) rate_count++;

            // Double-Trimmed Mean:
            // Jika sampel sudah >= 8: Buang 2 nilai tertinggi dan 2 terendah (buang 4 outlier!)
            // Jika 5-7 sampel: Buang 1 tertinggi dan 1 terendah
            // Jika < 5 sampel: Rata-rata langsung agar angka pertama cepat muncul
            if (rate_count >= 8) {
              float sorted[RATE_SIZE];
              for (byte i = 0; i < rate_count; i++) sorted[i] = rates[i];
              // Sortir kecil ke besar (bubble sort ringan 12 elemen)
              for (byte i = 0; i < rate_count - 1; i++) {
                for (byte j = 0; j < rate_count - i - 1; j++) {
                  if (sorted[j] > sorted[j + 1]) {
                    float tmp = sorted[j];
                    sorted[j] = sorted[j + 1];
                    sorted[j + 1] = tmp;
                  }
                }
              }
              // Buang 2 terendah (indeks 0, 1) dan 2 tertinggi (indeks count-2, count-1)
              float sum = 0.0;
              byte valid_count = rate_count - 4;
              for (byte i = 2; i < rate_count - 2; i++) {
                sum += sorted[i];
              }
              bpm_val = sum / (float)valid_count;
            } else if (rate_count >= 5) {
              float min_b = 999.0, max_b = 0.0, sum = 0.0;
              for (byte i = 0; i < rate_count; i++) {
                if (rates[i] < min_b) min_b = rates[i];
                if (rates[i] > max_b) max_b = rates[i];
                sum += rates[i];
              }
              bpm_val = (sum - min_b - max_b) / (float)(rate_count - 2);
            } else {
              float sum = 0.0;
              for (byte i = 0; i < rate_count; i++) sum += rates[i];
              bpm_val = sum / (float)rate_count;
            }

            last_beat_time = now;

            // 7. Estimasi SpO2 Berbasis Ratio-of-Ratios (R)
            float vpp_ir = max_ac_ir - min_ac_ir;
            float vpp_red = max_ac_red - min_ac_red;

            if (vpp_ir > 30 && dc_ir > 1000 && dc_red > 1000) {
              float r = (vpp_red / dc_red) / (vpp_ir / dc_ir);
              // Rumus kalibrasi empiris Maxim Oximetry: SpO2 = 110 - 25 * R
              float calc_spo2 = 110.0 - 25.0 * r;

              if (calc_spo2 > 100.0) calc_spo2 = 99.0;
              if (calc_spo2 < 75.0)  calc_spo2 = 75.0;

              if (spo2_val == 0) {
                spo2_val = calc_spo2;
              } else {
                spo2_val = 0.80 * spo2_val + 0.20 * calc_spo2;
              }
            }

            // Reset window min/max untuk siklus denyut berikutnya
            max_ac_ir = -99999; min_ac_ir = 99999;
            max_ac_red = -99999; min_ac_red = 99999;
          }
        }
        peak_registered = true;
      } else if (ac_ir < 15) {
        peak_registered = false; // Reset saat gelombang kembali ke garis dasar (baseline)
      }

      prev_ac_ir = ac_ir;
    }
  }

  // 8. Tampilkan data ke Serial Monitor tiap 1 detik
  if (millis() - last_print_time >= 1000) {
    last_print_time = millis();

    if (dc_ir == 0) {
      Serial.println("[STATUS] Tidak ada jari terdeteksi. Silakan tempelkan jari...");
    } else if (bpm_val == 0) {
      Serial.println("[STATUS] Jari terdeteksi. Mengunci detak awal...");
    } else {
      Serial.printf("[SCaDA] JARI AKTIF | Detak Jantung: %3.0f BPM | SpO2: %3.0f %%\n",
                    bpm_val, spo2_val);
    }
  }

  // Laju produksi data MAX30102: 100 Hz / Sample Averaging 4 = 25 Hz (1 sampel per 40 ms)
  // Jeda 38 ms + overhead pembacaan I2C ~2 ms = total siklus loop sinkron tepat 40 ms
  delay(38);
}
