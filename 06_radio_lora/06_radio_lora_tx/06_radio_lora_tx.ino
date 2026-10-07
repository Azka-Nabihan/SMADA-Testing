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

long activeFrequency = 915000000L;
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

  // 2. Inisialisasi Frekuensi Radio (915 MHz atau fallback 433 MHz)
  if (!LoRa.begin(915000000L)) {
    Serial.println("[INFO] Gagal pada 915 MHz, mencoba frekuensi 433 MHz...");
    if (!LoRa.begin(433000000L)) {
      Serial.println("[GAGAL] Radio LoRa tidak dapat diaktifkan!");
      while (1);
    }
    activeFrequency = 433000000L;
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
