#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

// ============================================================================
// SCaDA Test 06: LoRa Clean Point-to-Point Receiver
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

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("[TEST 06] Uji Penerima LoRa (Mode Clean Receiver)");
  Serial.println("==========================================");

  // 1. Inisialisasi SPI Hardware
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setSPI(SPI);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  // 2. Validasi Hardware Register SX1276 (0x42 == 0x12)
  pinMode(LORA_SS, OUTPUT);
  digitalWrite(LORA_SS, LOW);
  SPI.transfer(0x42 & 0x7F);
  uint8_t chipVersion = SPI.transfer(0x00);
  digitalWrite(LORA_SS, HIGH);

  Serial.printf("[SPI CHECK] Silicon Version Register (0x42): 0x%02X\n", chipVersion);
  if (chipVersion != 0x12) {
    Serial.println("\n[ERROR] Koneksi SPI ke modul LoRa Ra-02 GAGAL!");
    Serial.printf("        Nilai terbaca: 0x%02X (Ekspektasi: 0x12)\n", chipVersion);
    Serial.println("        Periksa kabel SPI (SCK:18, MISO:19, MOSI:23, CS:5, 3.3V, GND).\n");
    while (1) { delay(1000); }
  }
  Serial.println("[SPI CHECK] Chip SX1276 terdeteksi normal. Melanjutkan inisialisasi RF...");

  // 3. Inisialisasi Frekuensi Radio (Sama persis dengan TX)
  if (!LoRa.begin(activeFrequency)) {
    Serial.println("[INFO] Gagal pada 915 MHz, mencoba fallback ke 433 MHz...");
    activeFrequency = 433000000L;
    if (!LoRa.begin(activeFrequency)) {
      Serial.println("[GAGAL] Radio LoRa tidak dapat diaktifkan!");
      while (1);
    }
  }

  // 4. Parameter Radio (Identik 1:1 dengan Transmitter)
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc(); // Samakan dengan TX (keduanya CRC aktif)

  Serial.printf("[BERHASIL] Receiver SIAP mendengarkan pada frekuensi %.1f MHz!\n", activeFrequency / 1E6);
  Serial.println("Menunggu paket data masuk dari Transmitter...\n");
}

void loop() {
  // Polling paket yang masuk secara non-blocking dan murni tanpa delay
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.println("------------------------------------------");
    Serial.printf("[PAKET MASUK!] Panjang: %d byte | RSSI: %d dBm | SNR: %.1f dB\n",
                  packetSize, LoRa.packetRssi(), LoRa.packetSnr());

    String incomingText = "";
    while (LoRa.available()) {
      incomingText += (char)LoRa.read();
    }

    Serial.printf("Data Diterima : %s\n", incomingText.c_str());
    Serial.println("------------------------------------------\n");
  }
}