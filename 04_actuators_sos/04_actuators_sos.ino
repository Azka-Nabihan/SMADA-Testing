#include <Arduino.h>

// ============================================================================
// SCaDA Test 04: PASS Actuators & SOS Manual Button
// Pinout:
// - PIN_BUZZER_PWM : GPIO 25 (PWM Resonant 2.7 kHz Piezo Buzzer)
// - PIN_STROBE_LED : GPIO 26 (Visual Distress Strobe LED via MOSFET)
// - PIN_SOS_BUTTON : GPIO 27 (Push Button with Internal Pull-Up, Active Low)
// ============================================================================

#define PIN_BUZZER_PWM   25
#define PIN_STROBE_LED   26
#define PIN_SOS_BUTTON   27

#define BUZZER_CHANNEL   0
#define BUZZER_FREQ_HZ   2700 // Frekuensi resonansi piezo ~2.7 kHz
#define BUZZER_RES_BITS  8    // Resolusi 8-bit (0-255)

void triggerPreAlarmSound() {
  // Pola Pre-Alarm: Beep lambat berulang
  digitalWrite(PIN_STROBE_LED, HIGH);
  ledcWrite(BUZZER_CHANNEL, 128); // 50% duty cycle
  delay(100);
  digitalWrite(PIN_STROBE_LED, LOW);
  ledcWrite(BUZZER_CHANNEL, 0);
  delay(200);
}

void triggerFullAlarmSound() {
  // Pola Full Alarm / SOS: Sirene nada tinggi cepat & Strobo aktif
  digitalWrite(PIN_STROBE_LED, HIGH);
  ledcWrite(BUZZER_CHANNEL, 200);
  delay(80);
  digitalWrite(PIN_STROBE_LED, LOW);
  ledcWrite(BUZZER_CHANNEL, 0);
  delay(80);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("[TEST 04] Uji Aktuator Buzzer, Strobo & SOS");
  Serial.println("==========================================");

  pinMode(PIN_STROBE_LED, OUTPUT);
  digitalWrite(PIN_STROBE_LED, LOW);

  pinMode(PIN_SOS_BUTTON, INPUT_PULLUP);

  // Setup ESP32 LEDC PWM untuk Buzzer
  ledcSetup(BUZZER_CHANNEL, BUZZER_FREQ_HZ, BUZZER_RES_BITS);
  ledcAttachPin(PIN_BUZZER_PWM, BUZZER_CHANNEL);
  ledcWrite(BUZZER_CHANNEL, 0); // Pastikan buzzer mati di awal

  Serial.println("[OK] Hardware siap.");
  Serial.println("- Tekan tombol di GPIO 27 untuk memicu ALARM MANUAL.");
  Serial.println("- Ketik '1' di Serial Monitor untuk Uji Pre-Alarm.");
  Serial.println("- Ketik '2' di Serial Monitor untuk Uji Full-Alarm.");
  Serial.println("- Ketik '0' di Serial Monitor untuk Matikan Alarm.");
}

void loop() {
  // 1. Cek tombol fisik SOS di GPIO 27 (Active Low)
  if (digitalRead(PIN_SOS_BUTTON) == LOW) {
    Serial.println("[ALERT] Tombol SOS Fisik DITEKAN! Memicu Sirene Darurat...");
    for (int i = 0; i < 10; i++) {
      triggerFullAlarmSound();
    }
  }

  // 2. Cek input Serial Monitor
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == '1') {
      Serial.println("[MODE] Menjalankan Bunyi Pre-Alarm (Pola 15 Detik Diam)...");
      for (int i = 0; i < 5; i++) triggerPreAlarmSound();
    } else if (cmd == '2') {
      Serial.println("[MODE] Menjalankan Bunyi Full-Alarm / Distress...");
      for (int i = 0; i < 10; i++) triggerFullAlarmSound();
    } else if (cmd == '0') {
      Serial.println("[MODE] Alarm Dimatikan.");
      ledcWrite(BUZZER_CHANNEL, 0);
      digitalWrite(PIN_STROBE_LED, LOW);
    }
  }

  delay(50);
}
