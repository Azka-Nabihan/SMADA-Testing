#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

// ============================================================================
// SCaDA Test 06: LoRa Radio Transmitter (Mode Pengirim untuk 1 ESP32)
// Pinout SPI Hardware ESP32:
//   VCC   -> 3V3
//   GND   -> GND
//   NSS   -> GPIO 5  (CS)
//   RST   -> GPIO 14
//   DIO0  -> GPIO 2
//   SCK   -> GPIO 18
//   MISO  -> GPIO 19
//   MOSI  -> GPIO 23
// ============================================================================

#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_SS    5
#define LORA_RST   14
#define LORA_DIO0  2

long activeFrequency = 433000000L; // 433 MHz (Sesuai spesifikasi modul Ra-02: 410 - 525 MHz)
int packetCounter = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("[TEST 06] Uji Pemancar LoRa (Mode Transmitter)");
  Serial.println("==========================================");

  // 1. Inisialisasi SPI Hardware
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setSPI(SPI);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  // 2. Validasi Koneksi Fisik SPI ke Chip SX1276
  pinMode(LORA_SS, OUTPUT);
  digitalWrite(LORA_SS, LOW);
  SPI.transfer(0x42 & 0x7F);              // Baca RegVersion (0x42)
  uint8_t chipVersion = SPI.transfer(0x00);
  digitalWrite(LORA_SS, HIGH);

  Serial.printf("[SPI CHECK] Silicon Version Register (0x42): 0x%02X\n", chipVersion);
  if (chipVersion != 0x12) {
    Serial.println("\n[ERROR] Koneksi SPI ke modul LoRa Ra-02 GAGAL!");
    Serial.printf("        Nilai terbaca: 0x%02X (Ekspektasi: 0x12)\n", chipVersion);
    Serial.println("        Panduan Cepat:");
    Serial.println("        - 0x00: Periksa kabel daya 3.3V, GND, atau jalur SCK/MOSI.");
    Serial.println("        - 0xFF: Periksa kabel MISO (GPIO 19) atau NSS (GPIO 5) yang mungkin lepas/kendor.");
    Serial.println("        Program dihentikan untuk mencegah kerusakan.\n");
    while (1) { delay(1000); }
  }
  Serial.println("[SPI CHECK] Modul SX1276/SX1278 terdeteksi normal (0x12). Melanjutkan inisialisasi RF...");

  // 3. Inisialisasi Frekuensi Radio (433 MHz untuk modul Ra-02 410-525 MHz)
  if (!LoRa.begin(activeFrequency)) {
    Serial.println("[GAGAL] Radio LoRa tidak dapat diaktifkan pada 433 MHz!");
    while (1);
  }

  // 3. Konfigurasi Parameter RF Pengirim
  LoRa.setTxPower(17);              // Daya pancar 17 dBm (~50 mW, kuat & aman)
  LoRa.setSpreadingFactor(7);       // SF7 (cepat & jangkauan baik)
  LoRa.setSignalBandwidth(125E3);   // 125 kHz
  LoRa.setCodingRate4(5);           // 4/5
  LoRa.setSyncWord(0x12);           // Private Sync Word default

  // Invariant SCaDA: Opsi kontrol CRC
  LoRa.enableCrc();

  Serial.printf("[BERHASIL] Pemancar LoRa SIAP pada frekuensi %.1f MHz!\n", activeFrequency / 1E6);
  Serial.println("Mulai memancarkan paket data secara berkala...\n");
}

void loop() {
  packetCounter++;

  // Siapkan isi paket data yang akan dipancarkan
  String payload = "SCaDA Cuff #1 | Detak: 78 BPM | SpO2: 98% | Pkt: " + String(packetCounter);

  Serial.printf("[TX] Memancarkan Paket #%d (%d byte)... ", packetCounter, payload.length());

  // Proses modulasi dan transmisi radio
  LoRa.beginPacket();
  LoRa.print(payload);
  int success = LoRa.endPacket(); // endPacket(false) = blocking sampai transmisi selesai fisik

  if (success == 1) {
    Serial.println("BERHASIL! (Gelombang radio terpancar)");
  } else {
    Serial.println("GAGAL memancarkan paket!");
  }

  // Jeda 2 detik sebelum memancarkan paket berikutnya
  delay(2000);
}
