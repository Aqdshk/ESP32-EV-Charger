#include "EvseController.h"

void EvseController::begin() {
    pinMode(PIN_CONTACTOR, OUTPUT);
    pinMode(PIN_RCD_STATUS, INPUT_PULLUP);
    pinMode(PIN_EMERGENCY_STOP, INPUT_PULLUP);
    pinMode(PIN_CP_SENSE, INPUT);

    // Setup PWM untuk Control Pilot
    ledcSetup(CP_PWM_CHANNEL, CP_PWM_FREQ_HZ, CP_PWM_RES_BITS);
    ledcAttachPin(PIN_CP_PWM, CP_PWM_CHANNEL);

    setContactor(false);
    setEnabled(true); // Enable by default - can be changed via configuration

    lastEnergyUpdateMs = millis();
}

void EvseController::setEnabled(bool en) {
    state = en ? EvseState::Idle : EvseState::Disabled;
    chargeRequested = false;
    stopRequested = false;

    if (!en) {
        setContactor(false);
        ledcWrite(CP_PWM_CHANNEL, 0); // matikan CP PWM
    } else {
        updateCpPwm();
    }
}

void EvseController::setCurrentLimit(int amps) {
    offeredCurrentA = constrain(amps, MIN_CURRENT_AMP, MAX_CURRENT_AMP);
    Serial.print(F("[EVSE] Current limit set to: "));
    Serial.print(offeredCurrentA);
    Serial.println(F("A"));
    updateCpPwm();
}

void EvseController::startChargingRequest() {
    // Manual button press - uses 2-press safety feature
    // Press 1: Idle -> WaitingForAuth
    // Press 2: WaitingForAuth -> Charging
    if (state == EvseState::Charging) {
        Serial.println(F("[EVSE] Already charging, ignore start request"));
        return;
    }
    
    if (state == EvseState::Disabled || state == EvseState::Fault) {
        Serial.print(F("[EVSE] Cannot start charging from state="));
        Serial.println((int)state);
        return;
    }
    
    // Set charge request flag - state machine akan handle transition dengan safety feature
    chargeRequested = true;
    Serial.print(F("[EVSE] Manual start requested, current state="));
    Serial.println((int)state);
}

void EvseController::startChargingRemote() {
    // Remote start dari OCPP - bypass safety feature, direct start
    if (state == EvseState::Charging) {
        Serial.println(F("[EVSE] Already charging, ignore remote start"));
        return;
    }
    
    if (state == EvseState::Disabled || state == EvseState::Fault) {
        Serial.print(F("[EVSE] Cannot start charging from state="));
        Serial.println((int)state);
        return;
    }
    
    if (!isSafetyOk() || !isCarConnected()) {
        Serial.println(F("[EVSE] Cannot start: safety check failed or car not connected"));
        return;
    }
    
    // Direct transition to Charging (bypass WaitingForAuth safety step)
    Serial.print(F("[EVSE] Remote start - direct transition to Charging from state="));
    Serial.println((int)state);

    // Reset meter untuk sesi baru
    lastSample.energy = 0.0f;
    lastEnergyUpdateMs = millis();

    state = EvseState::Charging;
    setContactor(true);
    chargeRequested = false; // Clear flag
}

void EvseController::stopChargingRequest() {
    stopRequested = true;
}

bool EvseController::isCarConnected() {
    // TODO: Implement real CP voltage detection (IEC 61851 state A/B/C/D)
    // For now: always return true (bypass for testing)
    // Real implementation should read PIN_CP_SENSE and check voltage level
    return true;
}

bool EvseController::isSafetyOk() {
    // TODO: Implement real safety checks (RCD status, emergency stop)
    // For now: always return true (bypass for testing)
    // Real implementation:
    //   bool rcdOk = digitalRead(PIN_RCD_STATUS) == HIGH;
    //   bool estopNotPressed = digitalRead(PIN_EMERGENCY_STOP) == HIGH;
    //   return rcdOk && estopNotPressed;
    return true;
}

void EvseController::setContactor(bool close) {
    digitalWrite(PIN_CONTACTOR, close ? HIGH : LOW);
}

void EvseController::updateCpPwm() {
    // IEC 61851: duty cycle Control Pilot dikaitkan dengan arus maks.
    // Di sini kita guna mapping hampir-linear: 1A ≈ 1% duty (untuk contoh).
    float dutyPercent = (float)offeredCurrentA;
    dutyPercent = constrain(dutyPercent, 10.0f, 80.0f); // elak 0%/100% tepi spec

    int maxDuty = (1 << CP_PWM_RES_BITS) - 1;
    int duty = (int)(maxDuty * (dutyPercent / 100.0f));
    ledcWrite(CP_PWM_CHANNEL, duty);
}

void EvseController::sampleMeter() {
    // TODO: Replace with real meter reading (Modbus / pulse / CT sensor)
    // Currently using simulated values for OCPP integration testing

    // Simulasi meter reading:
    //  - Bila Charging: arus = SIMULATED_CURRENT_AMP (untuk testing dengan powerbank/low voltage)
    //  - Bila bukan Charging: arus = 0
    // NOTE: Untuk real EV charging, guna CT sensor atau Modbus meter untuk actual reading
    //       Real charger biasanya actual current ≈ 85-95% dari offered current (efficiency losses)
    if (state == EvseState::Charging) {
        // Untuk testing dengan powerbank 5V, guna current yang lebih rendah (1-2A)
        // Untuk real EV charging, guna actual meter reading atau simulate dengan offeredCurrentA * 0.9
        lastSample.current = SIMULATED_CURRENT_AMP;
    } else {
        lastSample.current = 0.0f;
    }

    // Untuk testing dengan powerbank, voltage akan lebih rendah (5V)
    // Untuk real EV charging, guna GRID_VOLTAGE (230V)
    // NOTE: Untuk real implementation, baca voltage dari meter sebenar
    if (state == EvseState::Charging) {
        // Untuk testing, guna simulated voltage (5V untuk powerbank)
        // Untuk real EV, guna GRID_VOLTAGE atau actual meter reading
        lastSample.voltage = SIMULATED_VOLTAGE_V;
    } else {
        lastSample.voltage = GRID_VOLTAGE; // Idle state: show grid voltage
    }
    lastSample.power   = lastSample.voltage * lastSample.current; // W

    // Tenaga (kWh) anggaran berdasarkan power dan masa (berdasarkan lastEnergyUpdateMs)
    // P(W) * dt(s) -> Wh -> kWh
    unsigned long now = millis();
    float dt_s = (now - lastEnergyUpdateMs) / 1000.0f;
    lastEnergyUpdateMs = now;

    if (dt_s > 0 && lastSample.power > 0.0f) {
        float energy_Wh = lastSample.power * dt_s / 3600.0f; // W * s -> Wh
        lastSample.energy += energy_Wh / 1000.0f;            // kWh
    }
}

void EvseController::loop() {
    // State machine asas EVSE
    switch (state) {
        case EvseState::Disabled:
            setContactor(false);
            ledcWrite(CP_PWM_CHANNEL, 0);
            break;

        case EvseState::Idle:
            setContactor(false);

            if (!isSafetyOk()) {
                state = EvseState::Fault;
                break;
            }

            // Safety feature: Button press pertama -> go to WaitingForAuth (authorization step)
            // Button press kedua (dalam WaitingForAuth) -> go to Charging
            // NOTE: Jangan auto-transition ke WaitingForAuth tanpa explicit request
            // (elak auto-start selepas power cycle)
            if (chargeRequested) {
                chargeRequested = false; // Reset flag
                // Always go to WaitingForAuth first (safety/authorization step)
                Serial.println(F("[EVSE] State transition: Idle -> WaitingForAuth (button pressed - authorization requested)"));
                state = EvseState::WaitingForAuth;
            }
            // No auto-transition: User must press button to start charging (prevents auto-start after power cycle)
            break;

        case EvseState::WaitingForAuth:
            if (!isSafetyOk()) {
                state = EvseState::Fault;
                break;
            }

            if (!isCarConnected()) {
                state = EvseState::Idle;
                break;
            }

            if (chargeRequested) {
                // Button press kedua: Authorization granted -> Start charging
                Serial.println(F("[EVSE] State transition: WaitingForAuth -> Charging (authorization granted)"));

                // Reset meter untuk sesi baru
                lastSample.energy = 0.0f;
                lastEnergyUpdateMs = millis();

                state = EvseState::Charging;
                setContactor(true);
                chargeRequested = false; // reset flag setelah digunakan
                Serial.println(F("[EVSE] State sekarang = Charging, contactor ON"));
            }
            break;

        case EvseState::Charging:
            if (!isSafetyOk()) {
                Serial.println(F("[EVSE] Charging -> Fault: isSafetyOk() = false"));
                setContactor(false);
                state = EvseState::Fault;
                chargeRequested = false;
                stopRequested = false;
                break;
            }
            if (!isCarConnected()) {
                Serial.println(F("[EVSE] Charging -> Idle: isCarConnected() = false"));
                setContactor(false);
                state = EvseState::Idle;
                chargeRequested = false;
                stopRequested = false;
                break;
            }
            if (stopRequested) {
                Serial.println(F("[EVSE] Charging -> Idle: stopRequested = true"));
                setContactor(false);
                state = EvseState::Idle;
                chargeRequested = false;
                stopRequested = false;
                break;
            }
            // Charging OK - stay in Charging state
            break;

        case EvseState::Fault:
            setContactor(false);
            // Auto-reset mudah: bila semua OK dan kereta tak sambung, kembali ke Idle
            if (isSafetyOk() && !isCarConnected()) {
                state = EvseState::Idle;
            }
            break;
    }

    // Sampling meter berkala
    unsigned long now = millis();
    if (now - lastMeterSampleMs >= METER_SAMPLE_INTERVAL_MS) {
        lastMeterSampleMs = now;
        sampleMeter();
    }
}
