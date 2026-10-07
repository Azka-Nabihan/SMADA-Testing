#include <Arduino.h>
#include <SPI.h>

// ============================================================================
// SCaDA Test 06: LoRa SX1276 Hardware & SPI Bus Sanity Check
// Tujuan: Memvalidasi koneksi fisik SPI antara ESP32 dan modul LoRa Ra-02
//         tanpa memancarkan sinyal radio apapun.
//
// Pinout SPI Hardware ESP32:
//   VCC   -> 3V3  (Wajib 3.3V, jangan 5V!)
//   GND   -> GND
//   NSS   -> GPIO 5  (Chip Select / CS)
//   RST   -> GPIO 14 (Hardware Reset)
//   DIO0  -> GPIO 2  (Interrupt)
//   SCK   -> GPIO 18 (SPI Clock)
//   MISO  -> GPIO 19 (SPI Data Out dari LoRa)
//   MOSI  -> GPIO 23 (SPI Data In ke LoRa)
// ============================================================================

#define PIN_SCK   18
#define PIN_MISO  19
#define PIN_MOSI  23
#define PIN_SS    5
#define PIN_RST   14
#define PIN_DIO0  2

#define REG_OP_MODE 0x01
#define REG_VERSION 0x42

uint8_t readRegister(uint8_t address) {
  digitalWrite(PIN_SS, LOW);
  SPI.transfer(address & 0x7F);        // Bit 7 = 0 untuk operasi Read
  uint8_t response = SPI.transfer(0x00); // Clock dummy untuk membaca respon
  digitalWrite(PIN_SS, HIGH);
  return response;
}

void resetLoRa() {
  pinMode(PIN_RST, OUTPUT);
  digitalWrite(PIN_RST, LOW);
  delay(10);
  digitalWrite(PIN_RST, HIGH);
  delay(10);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==========================================");
  Serial.println("  SCaDA: Uji Validasi Koneksi ESP32 - LoRa");
  Serial.println("==========================================");

  // 1. Inisialisasi Pin Kontrol
  pinMode(PIN_SS, OUTPUT);
  digitalWrite(PIN_SS, HIGH);
  pinMode(PIN_DIO0, INPUT);

  // 2. Reset fisik chip LoRa
  Serial.print("[1/3] Melakukan hardware reset pada chip Ra-02... ");
  resetLoRa();
  Serial.println("OK");

  // 3. Inisialisasi Bus SPI
  Serial.print("[2/3] Memulai komunikasi bus SPI (SCK:18, MISO:19, MOSI:23, CS:5)... ");
  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_SS);
  Serial.println("OK");

  // 4. Membaca Register Versi Silikon SX1276 (Register 0x42)
  Serial.println("[3/3] Membaca Silicon Version Register (0x42)...");
  uint8_t version = readRegister(REG_VERSION);
  uint8_t opMode  = readRegister(REG_OP_MODE);

  Serial.println("------------------------------------------");
  Serial.printf("Nilai Register Terbaca : 0x%02X\n", version);
  Serial.printf("Status OpMode Terbaca  : 0x%02X\n", opMode);
  Serial.printf("Status Pin DIO0        : %s\n", digitalRead(PIN_DIO0) == LOW ? "LOW (Normal Idle)" : "HIGH");
  Serial.println("------------------------------------------");

  // Evaluasi Hasil
  if (version == 0x12) {
    Serial.println("\n>>> HASIL: VALIDASI SUKSES! <<<");
    Serial.println("Chip Semtech SX1276 terdeteksi secara valid (Versi 0x12).");
    Serial.println("Jalur daya 3.3V, GND, dan semua pin SPI (SCK, MISO, MOSI, NSS) terhubung dengan baik.");
    Serial.println("Modul siap digunakan untuk transmisi TX / RX.\n");
  } else {
    Serial.println("\n>>> HASIL: VALIDASI GAGAL! <<<");
    Serial.printf("Register terbaca 0x%02X (Seharusnya 0x12).\n\n", version);
    Serial.println("Panduan Pengecekan:");
    if (version == 0x00) {
      Serial.println("- Nilai 0x00 umumnya menandakan modul tidak mendapat daya (periksa pin 3V3 dan GND)");
      Serial.println("  atau jalur SCK / MOSI belum tersambung ke ESP32.");
    } else if (version == 0xFF) {
      Serial.println("- Nilai 0xFF umumnya menandakan jalur MISO mengambang/putus (periksa kabel GPIO 19)");
      Serial.println("  atau pin NSS (GPIO 5) belum terhubung dengan kuat.");
    } else {
      Serial.println("- Terbaca data acak. Periksa kualitas kabel jumper dari derau (noise) atau kontak kendor.");
    }
    Serial.println("\nSilakan perbaiki sambungan fisik sebelum menjalankan program transmisi radio.");
  }
}

void loop() {
  // Loop kosong, uji validasi selesai pada setup
  delay(1000);
}
