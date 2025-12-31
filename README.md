# ESP32 EV Charger Controller (OCPP-ready)

Projek ini jadikan `ESP32` sebagai **"otak" untuk EV charger**, dengan:

- **EVSE logic** (state machine, Control Pilot PWM, kawal contactor)
- **OCPP 1.6J integration** dengan SteVe backend (remote start/stop, transaction management)
- **I2C LCD Display** untuk status monitoring (16x2 LCD dengan PCF8574T adapter)
- **Manual buttons** untuk start/stop charging (2-press safety feature)
- **OTA (Over-The-Air) Updates** via ArduinoOTA (local network)
- **OCPP Firmware Update** via SteVe backend (remote firmware management)
- **Meter simulation** untuk testing (boleh diganti dengan Modbus/CT sensor sebenar)

> **PENTING (keselamatan)**  
> Kod ini hanya rangka (prototype). Semua sambungan AC / contactor / RCD untuk EV charger **mesti** direka & diperiksa oleh jurutera berkelayakan dan mengikut standard (contoh IEC 61851, IEC 60364). Jangan guna projek ini terus untuk pemasangan grid sebenar tanpa reka bentuk dan ujian yang betul.

---

## Cara build (PlatformIO)

1. Pastikan ada **VS Code + PlatformIO** atau gunakan **CLI**:

   ```bash
   cd ESP-Charger-RND
   pio run
   pio run --target upload
   pio device monitor
   ```

2. Edit `platformio.ini` jika perlu (board lain, library lain, dsb).

---

## Fail utama

- `src/HardwareConfig.h`  
  Pin mapping untuk contactor, Control Pilot, RCD, emergency stop, buttons, LCD.

- `src/EvseController.*`  
  EVSE state machine (Idle / WaitingForCar / WaitingForAuth / Charging / Fault), kawal contactor, hasilkan PWM CP, dan simulasi bacaan meter (boleh diganti dengan Modbus / CT sebenar).

- `src/OcppClient.*`  
  OCPP 1.6J client menggunakan MicroOcpp library:
  - WiFi connection management
  - OCPP WebSocket connection ke SteVe backend
  - Remote start/stop transaction handling
  - Transaction management dan meter value reporting

- `src/LcdDisplay.*`  
  I2C LCD display untuk status monitoring (state, current, energy, transaction info).

- `src/OtaManager.*`  
  ArduinoOTA untuk firmware update via local network.

- `src/OcppFirmwareUpdate.*`  
  OCPP Firmware Management untuk remote firmware update dari SteVe backend.

- `src/main.cpp`  
  Main loop: inisialisasi semua modul, handle button inputs, update LCD, manage OTA updates.

---

## Konfigurasi

### SteVe OCPP Setup

Di pelayan SteVe (contoh: `http://34.143.146.176:8180/steve/manager/signin`):

1. Tambah **Charge Point** baru
2. Set **ChargeBoxId / Charge Point ID** = `ESP32-CP-01` (atau apa-apa, tapi **mesti sama** dengan `CHARGE_POINT_ID` dalam `OcppClient.cpp`)
3. Pilih **OCPP 1.6J** dan set bilangan connector (contoh 1)

### WiFi & OCPP Client Setup

1. Edit `src/OcppClient.cpp`:
   - Masukkan `WIFI_SSID` dan `WIFI_PASSWORD`
   - Pastikan `OCPP_WS_URL` betul:
     - Contoh: `ws://34.143.146.176:8180/steve/websocket/CentralSystemService`
   - Pastikan `CHARGE_POINT_ID` sama dengan yang di SteVe (default: `ESP32-CP-01`)

2. Edit `src/HardwareConfig.h` jika perlu:
   - Pin mapping untuk hardware kamu
   - LCD I2C address (default: 0x27)
   - Simulated meter values untuk testing

3. Flash ESP32 dan buka Serial Monitor (`115200 baud`):
   - Kau patut nampak log WiFi connect
   - Bila OCPP sudah connected, `BootNotification` akan muncul di UI SteVe dan charge point akan nampak **online**

### OTA (Over-The-Air) Update

Untuk update firmware tanpa USB cable:

1. First upload via USB: `pio run -t upload`
2. Get IP address dari Serial Monitor (contoh: `192.168.1.100`)
3. Subsequent uploads via OTA:
   ```bash
   pio run -t upload -e esp32dev --upload-port 192.168.1.100
   ```

**Dokumentasi lengkap:** Lihat `OTA_UPDATE.md`

### OCPP Firmware Update

Untuk update firmware remotely dari SteVe backend:

1. Host firmware `.bin` file di HTTP server (GitHub Releases atau web server)
2. Trigger `UpdateFirmware` command dari SteVe dengan firmware URL
3. ESP32 akan download dan install firmware automatically

**Dokumentasi lengkap:** Lihat `OCPP_FIRMWARE_UPDATE.md`

---

## Hardware Setup

Lihat `WIRING_DIAGRAM.md` untuk wiring diagram lengkap.

### Pin Mapping (Default)

- **Control Pilot (CP)**:
  - `PIN_CP_PWM` (GPIO 25) → CP PWM output
  - `PIN_CP_SENSE` (GPIO 34) → CP voltage sensing (ADC)

- **Power Control**:
  - `PIN_CONTACTOR` (GPIO 23) → Contactor/relay control

- **Safety Inputs**:
  - `PIN_RCD_STATUS` (GPIO 35) → RCD/RCBO status
  - `PIN_EMERGENCY_STOP` (GPIO 32) → Emergency stop button

- **Manual Buttons**:
  - `PIN_BTN_ON` (GPIO 18) → Start charging button
  - `PIN_BTN_OFF` (GPIO 19) → Stop charging button

- **LCD Display (I2C)**:
  - `PIN_I2C_SDA` (GPIO 21) → I2C Data
  - `PIN_I2C_SCL` (GPIO 22) → I2C Clock
  - Address: `0x27` (default, boleh tukar ikut hardware)

> **PENTING (keselamatan):** Rekaan fizikal & keselamatan perlukan kerja hardware yang serius. Kod di sini fokus kepada logik ESP32 dan integrasi OCPP. Pastikan semua sambungan AC / contactor / RCD direka & diperiksa oleh jurutera berkelayakan mengikut standard (IEC 61851, IEC 60364).

---

## Features

- ✅ **EVSE State Machine** - IEC 61851 compliant state management (Idle, WaitingForCar, WaitingForAuth, Charging, Fault)
- ✅ **OCPP 1.6J Integration** - Full OCPP client dengan SteVe backend support
- ✅ **Remote Start/Stop** - Control charging dari SteVe backend
- ✅ **Manual Buttons** - Local start/stop dengan 2-press safety feature
- ✅ **LCD Display** - Real-time status monitoring (state, current, energy, transaction info)
- ✅ **OTA Updates** - Over-the-air firmware updates via ArduinoOTA (local network)
- ✅ **OCPP Firmware Management** - Remote firmware updates dari SteVe backend
- ✅ **Meter Simulation** - Testing meter readings (boleh replace dengan real Modbus/CT sensor)

---

## Dokumentasi

- `WIRING_DIAGRAM.md` - Detailed hardware wiring instructions
- `OTA_UPDATE.md` - ArduinoOTA setup dan usage guide
- `OCPP_FIRMWARE_UPDATE.md` - OCPP firmware update setup guide
- `HYBRID_ARCHITECTURE.md` - Future hybrid (RPi + ESP32) architecture design
- `HYBRID_IMPLEMENTATION_ROADMAP.md` - Implementation roadmap untuk hybrid architecture
- `RASPBERRY_PI_CHARGER_COMPARISON.md` - ESP32 vs Raspberry Pi comparison

---

## License

See license file for details.


