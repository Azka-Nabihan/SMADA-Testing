# SCaDA Modular Test Suite (Arduino IDE)
> **Smart Cuff and Distress Alarm (SCaDA) — Kelompok 23 DTE FTUI**

Kumpulan skrip pengujian terisolasi (*per sensor / per modul*) untuk memverifikasi hardware secara bertahap menggunakan **Arduino IDE**.

---

## 1. Daftar Sketsa Pengujian

| Folder Sketsa | Target Uji | Alamat / Pin | Keterangan |
|---|---|---|---|
| [`01_sensor_max30102/`](file:///c:/Grimoire/Despro/firmware/tests/01_sensor_max30102/01_sensor_max30102.ino) | **MAX30102** | I2C `0x57` (D21, D22) | Deteksi kontak kulit & nilai mentah IR / Red ADC |
| [`02_sensor_mcp9808/`](file:///c:/Grimoire/Despro/firmware/tests/02_sensor_mcp9808/02_sensor_mcp9808.ino) | **MCP9808** | I2C `0x18` (D21, D22) | Pembacaan suhu tubuh/kulit real-time |
| [`03_sensor_lsm6ds3/`](file:///c:/Grimoire/Despro/firmware/tests/03_sensor_lsm6ds3/03_sensor_lsm6ds3.ino) | **GY-LSM6DS3** | I2C `0x6A` (D21, D22, CS=3V3) | Akselerometer 3-Axis & Giroskop 3-Axis |
| [`04_actuators_sos/`](file:///c:/Grimoire/Despro/firmware/tests/04_actuators_sos/04_actuators_sos.ino) | **Buzzer, Strobo, SOS** | GPIO 25, 26, 27 | Pola nada Pre-Alarm, Full-Alarm, & tombol fisik |
| [`05_multi_sensor_fusion/`](file:///c:/Grimoire/Despro/firmware/tests/05_multi_sensor_fusion/05_multi_sensor_fusion.ino) | **Multi-Sensor I2C** | Bus I2C Paralel | Pembacaan simultan MAX30102 + MCP9808 + LSM6DS3 |

---

## 2. Diagram Pinout Referensi

```text
ESP32 (NodeMCU / DevKit V1)
---------------------------
3V3       ---> VCC / VDD Semua Sensor
GND       ---> GND Semua Sensor
GPIO 21   ---> SDA Semua Sensor (I2C Bus Bersama)
GPIO 22   ---> SCL Semua Sensor (I2C Bus Bersama)

Khusus GY-LSM6DS3:
Pin CS    ---> Wajib disambung ke 3V3 (Pengaktif mode I2C)

Khusus Aktuator:
GPIO 25   ---> Buzzer PWM
GPIO 26   ---> LED Strobo
GPIO 27   ---> Tombol SOS (Input Pull-Up ke GND)
```

---

## 3. Cara Penggunaan di Arduino IDE

1. Buka Arduino IDE.
2. Klik **File $\rightarrow$ Open...**
3. Arahkan ke file sketsa yang ingin diuji (misal: `02_sensor_mcp9808/02_sensor_mcp9808.ino`).
4. Pilih Board: **ESP32 Dev Module** dan pilih port COM yang sesuai.
5. Upload dan buka **Serial Monitor** pada kecepatan **`115200 baud`**.
