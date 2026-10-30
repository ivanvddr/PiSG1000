/*
 * Questo file fa parte del progetto SMS Emulator per Raspberry Pi Pico.
 *
 * Copyright (C) 2026 Ivan Vettori
 *
 * Rilasciato sotto GNU General Public License v3.0.
 *
 * Basato su FunduinoJoystick dal progetto Pi64.
 */
#include "SMSInput.h"

// ─────────────────────────────────────────────────
//  Costruttore
// ─────────────────────────────────────────────────
SMSInput::SMSInput()
    : _joy1(JOYSTICK_PORT_1)
    , _joy2(JOYSTICK_PORT_2)
    , _j1Btn2Pin(-1)
    , _j2Btn2Pin(-1)
{
    // Stato iniziale: tutti i bit a 1 = nessun tasto premuto
    _portDC = 0xFF;
    _portDD = 0xFF;
}

// ─────────────────────────────────────────────────
//  begin
// ─────────────────────────────────────────────────
void SMSInput::begin(
    int j1Up, int j1Down, int j1Left, int j1Right, int j1Btn1, int j1Btn2,
    int j2Up, int j2Down, int j2Left, int j2Right, int j2Btn1, int j2Btn2)
{
    // ── Joy 1: delega i 5 pin standard al FunduinoJoystick ──
    // fire del Funduino = Button 1 SMS
    _joy1.begin(j1Up, j1Down, j1Left, j1Right, j1Btn1);

    // ── Joy 2 ────────────────────────────────────
    _joy2.begin(j2Up, j2Down, j2Left, j2Right, j2Btn1);

    // ── Button 2 (TH pin): pin GPIO separato ─────
    _j1Btn2Pin = j1Btn2;
    _j2Btn2Pin = j2Btn2;

    if (_j1Btn2Pin >= 0) {
        pinMode(_j1Btn2Pin, INPUT_PULLUP);
        Serial.printf("[Input] Joy1 BTN2: GP%d\n", _j1Btn2Pin);
    }
    if (_j2Btn2Pin >= 0) {
        pinMode(_j2Btn2Pin, INPUT_PULLUP);
        Serial.printf("[Input] Joy2 BTN2: GP%d\n", _j2Btn2Pin);
    }

    // Lettura iniziale
    update();

    Serial.println("[Input] SMSInput ready");
}

// ─────────────────────────────────────────────────
//  update — polling GPIO → porte SMS
//
//  Chiamato da Core0 ogni N cicli Z80 (es. ogni scanline).
//  Legge i GPIO fisici e compone i byte delle porte
//  in formato SMS (logica negativa: 0 = premuto).
//
//  Il FunduinoJoystick.read() è già logica negativa:
//  bit = 0 → premuto, bit = 1 → rilasciato
//  Mappa diretta sui bit SMS.
// ─────────────────────────────────────────────────
void SMSInput::update() {
    uint8_t j1 = _joy1.readDebounced();
    uint8_t j2 = _joy2.readDebounced();

    // Formato FunduinoJoystick:
    // bit0=Up, bit1=Down, bit2=Left, bit3=Right, bit4=Fire(=Btn1)
    // Valore 1 = NON premuto, 0 = premuto  (logica negativa)

    // ── Porta 0xDC ────────────────────────────────
    // bit 0: Joy1 Up
    // bit 1: Joy1 Down
    // bit 2: Joy1 Left
    // bit 3: Joy1 Right
    // bit 4: Joy1 Btn1
    // bit 5: Joy1 Btn2  (pin separato)
    // bit 6: Joy2 Up
    // bit 7: Joy2 Down

    uint8_t dc = 0xFF; // default: tutto rilasciato

    // Joy1: i bit Funduino 0-4 corrispondono esattamente ai bit SMS 0-4
    // Maschera solo i 5 bit rilevanti e copia in DC
    dc &= ~0x1F;                     // azzera bits 0-4
    dc |= (j1 & 0x1F);               // copia Up/Down/Left/Right/Btn1

    // Joy1 Btn2: pin separato, logica negativa come Funduino
    if (!pinPressed(_j1Btn2Pin)) {
        dc |= DC_J1_BTN2;            // bit5 = 1 → rilasciato
    } else {
        dc &= ~DC_J1_BTN2;           // bit5 = 0 → premuto
    }

    // Joy2 Up (bit6) e Down (bit7) nel byte DC
    if (j2 & 0x01) dc |= DC_J2_UP;   else dc &= ~DC_J2_UP;
    if (j2 & 0x02) dc |= DC_J2_DOWN; else dc &= ~DC_J2_DOWN;

    _portDC = dc;

    // ── Porta 0xDD ────────────────────────────────
    // bit 0: Joy2 Left
    // bit 1: Joy2 Right
    // bit 2: Joy2 Btn1
    // bit 3: Joy2 Btn2
    // bit 4: Reset (non implementato → sempre 1)
    // bit 5-7: sempre 1

    uint8_t dd = 0xFF; // default: tutto rilasciato

    // Joy2 Left (bit2 Funduino → bit0 DD)
    if (j2 & 0x04) dd |= DD_J2_LEFT;  else dd &= ~DD_J2_LEFT;
    // Joy2 Right (bit3 Funduino → bit1 DD)
    if (j2 & 0x08) dd |= DD_J2_RIGHT; else dd &= ~DD_J2_RIGHT;
    // Joy2 Btn1  (bit4 Funduino → bit2 DD)
    if (j2 & 0x10) dd |= DD_J2_BTN1;  else dd &= ~DD_J2_BTN1;
    // Joy2 Btn2  (pin separato → bit3 DD)
    if (!pinPressed(_j2Btn2Pin)) {
        dd |=  DD_J2_BTN2;
    } else {
        dd &= ~DD_J2_BTN2;
    }

    // bit4 = Reset: sempre 1 (non cablato)
    // bit5-7: sempre 1 (già 0xFF di default)

    _portDD = dd;
}

// ─────────────────────────────────────────────────
//  readPort — lettura porta Z80
//
//  Chiamato dal mapper I/O quando la CPU legge 0xDC o 0xDD.
//  Restituisce lo stato aggiornato dall'ultimo update().
//
//  Nota: update() viene chiamato da Core0 nel loop principale,
//  readPort() viene chiamato dalla CPU (sempre Core0) →
//  nessun problema di concorrenza, stessa CPU.
// ─────────────────────────────────────────────────
uint8_t SMSInput::readPort(uint8_t port) {
    switch (port) {
        case 0xDC:
        case 0xC0:  // mirror (alcuni giochi usano 0xC0)
            return _portDC;

        case 0xDD:
        case 0xC1:  // mirror
            return _portDD;

        default:
            return 0xFF;
    }
}
