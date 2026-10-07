#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

// ============================================================================
// SCaDA Test 06: LoRa RF Raw Energy Meter & Packet Sniffer
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
int rssiOffset = -157; // -157 untuk HF (915 MHz), -164 untuk LF (433 MHz)

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("[TEST 06] LoRa Real-Time RF Energy Sniffer");
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
  Serial.println("[SPI CHECK] Modul SX1276 terdeteksi normal (0x12). Melanjutkan inisialisasi RF...");

  // 3. Inisialisasi Frekuensi Radio (Coba 915 MHz, fallback ke 433 MHz)
  if (!LoRa.begin(915000000L)) {
    Serial.println("[INFO] Gagal pada 915 MHz, mencoba frekuensi 433 MHz...");
    if (!LoRa.begin(433000000L)) {
      Serial.println("[GAGAL] Radio LoRa tidak merespons!");
      while (1);
    }
    activeFrequency = 433000000L;
    rssiOffset = -164;
  }

  // 3. Konfigurasi Radio Promiscuous (Penerimaan Terbuka)
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);

  // Invariant SCaDA: Matikan pengecekan CRC agar paket tanpa CRC tetap lolos
  LoRa.disableCrc();

  // Masuk ke mode continuous receive
  LoRa.receive();

  Serial.printf("[BERHASIL] Radio Aktif pada %.1f MHz!\n", activeFrequency / 1E6);
  Serial.println("Petunjuk Uji Fisik:");
  Serial.println("-> Coba sentuh kawat antena dengan jari atau dekatkan HP.");
  Serial.println("-> Bar grafik di bawah akan langsung melompat jika antena menangkap energi!\n");
}

void loop() {
  // 1. Cek jika ada paket data valid yang tertangkap
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.println("\n******************************************");
    Serial.printf("[PAKET TERTANGKAP!] Panjang: %d byte | RSSI: %d dBm | SNR: %.1f dB\n",
                  packetSize, LoRa.packetRssi(), LoRa.packetSnr());
    
    String payloadStr = "";
    Serial.print("Data HEX: ");
    while (LoRa.available()) {
      byte b = LoRa.read();
      payloadStr += (isPrintable(b) ? (char)b : '.');
      if (b < 0x10) Serial.print("0");
      Serial.print(b, HEX);
      Serial.print(" ");
    }
    Serial.println();
    Serial.printf("Data Teks: %s\n", payloadStr.c_str());
    Serial.println("******************************************\n");
  }

  // 2. Baca kekuatan gelombang elektromagnetik mentah di udara (Register 0x1B)
  digitalWrite(LORA_SS, LOW);
  SPI.transfer(0x1B & 0x7F);        // Baca Register REG_RSSI_VALUE
  byte rawVal = SPI.transfer(0x00);
  digitalWrite(LORA_SS, HIGH);

  int currentRssi = rssiOffset + rawVal;

  // 3. Tampilkan grafik bar kekuatan energi radio di udara
  Serial.printf("RF Energy: %4d dBm [", currentRssi);
  
  // Normalisasi rentang -120 dBm (sangat hening) s/d -50 dBm (sangat kuat)
  int barLength = map(constrain(currentRssi, -120, -50), -120, -50, 0, 30);
  for (int i = 0; i < 30; i++) {
    if (i < barLength) Serial.print("=");
    else Serial.print(" ");
  }
  Serial.println("]");

  delay(200);
}