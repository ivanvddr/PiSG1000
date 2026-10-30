#pragma once
/*
 * Questo file fa parte del progetto SMS Emulator per Raspberry Pi Pico.
 *
 * Copyright (C) 2026 Ivan Vettori
 *
 * Rilasciato sotto GNU General Public License v3.0.
 *
 * Basato su FunduinoJoystick dal progetto Pi64.
 */

#include <Arduino.h>
#include "FunduinoJoystick.h"

// ─────────────────────────────────────────────────
//  SMS Input — Porte joystick Sega Master System
//
//  Il SMS legge i joystick su due porte I/O Z80:
//
//  Port 0xDC (Joy 1 + Joy 2 parziale):
//  ┌─────┬─────┬─────┬─────┬─────┬─────┬─────┬─────┐
//  │  7  │  6  │  5  │  4  │  3  │  2  │  1  │  0  │
//  │J2-B2│J2-B1│J1-R │J1-L │J1-D │J1-U │(n/a)│(n/a)│
//  │ J2  │ J2  │ J1  │ J1  │ J1  │ J1  │     │     │
//  └─────┴─────┴─────┴─────┴─────┴─────┴─────┴─────┘
//
//  Correzione: il mapping reale della porta 0xDC è:
//  bit 0: Joy1 Up
//  bit 1: Joy1 Down
//  bit 2: Joy1 Left
//  bit 3: Joy1 Right
//  bit 4: Joy1 Button1
//  bit 5: Joy1 Button2  (pin TH joy1, usato come 2° tasto)
//  bit 6: Joy2 Up
//  bit 7: Joy2 Down
//
//  Port 0xDD (Joy 2 completo + misc):
//  bit 0: Joy2 Left
//  bit 1: Joy2 Right
//  bit 2: Joy2 Button1
//  bit 3: Joy2 Button2
//  bit 4: Reset button (attivo basso, non implementato)
//  bit 5: 1 (non usato)
//  bit 6: TH Joy1 (lightgun / non usato)
//  bit 7: TH Joy2 (lightgun / non usato)
//
//  LOGICA NEGATIVA: bit = 0 → tasto premuto, bit = 1 → rilasciato
//  (identica alla logica del FunduinoJoystick e della CIA C64)
//
//  Il joystick fisico ha 5 vie + 2 tasti → mappati su:
//  UP, DOWN, LEFT, RIGHT, BUTTON1, BUTTON2
//  Il FunduinoJoystick gestisce UP-DOWN-LEFT-RIGHT-FIRE(=B1)
//  Aggiungiamo un pin separato per B2.
// ─────────────────────────────────────────────────


// ── Bit mask porta 0xDC ───────────────────────────
// ── Bit mask porta 0xDC (Bit 0..7) ────────────────
static constexpr uint8_t DC_J1_UP    = 0x01;  // bit 0 (1 << 0)
static constexpr uint8_t DC_J1_DOWN  = 0x02;  // bit 1 (1 << 1)
static constexpr uint8_t DC_J1_LEFT  = 0x04;  // bit 2 (1 << 2)
static constexpr uint8_t DC_J1_RIGHT = 0x08;  // bit 3 (1 << 3)
static constexpr uint8_t DC_J1_BTN1  = 0x10;  // bit 4 (1 << 4) - Button 1 (A)
static constexpr uint8_t DC_J1_BTN2  = 0x20;  // bit 5 (1 << 5) - Button 2 (B)
static constexpr uint8_t DC_J2_UP    = 0x40;  // bit 6 (1 << 6)
static constexpr uint8_t DC_J2_DOWN  = 0x80;  // bit 7 (1 << 7)

// ── Bit mask porta 0xDD ───────────────────────────
static constexpr uint8_t DD_J2_LEFT  = 0x01;
static constexpr uint8_t DD_J2_RIGHT = 0x02;
static constexpr uint8_t DD_J2_BTN1  = 0x04;
static constexpr uint8_t DD_J2_BTN2  = 0x08;
static constexpr uint8_t DD_RESET    = 0x10;  // non implementato
static constexpr uint8_t DD_UNUSED   = 0xE0;  // bits 5-7 sempre 1

// ─────────────────────────────────────────────────
//  Classe SMSInput
// ─────────────────────────────────────────────────
class SMSInput {
public:
    SMSInput();

    // ── Init ──────────────────────────────────────
    // Joy1: up, down, left, right, btn1, btn2
    // Joy2: up, down, left, right, btn1, btn2
    // btn2 è un pin GPIO separato (il FunduinoJoystick gestisce solo 5 pin)
    void begin(
        // Joy 1
        int j1Up, int j1Down, int j1Left, int j1Right,
        int j1Btn1, int j1Btn2,
        // Joy 2
        int j2Up, int j2Down, int j2Left, int j2Right,
        int j2Btn1, int j2Btn2
    );

    // ── Lettura porte Z80 ─────────────────────────
    // Chiamato dalla CPU quando legge le porte 0xDC / 0xDD
    uint8_t readPort(uint8_t port);

    // ── Aggiornamento stato (polling) ─────────────
    // Chiamato periodicamente da Core0 nel loop principale
    // Legge i GPIO e aggiorna lo stato interno
    void update();

    // ── Accesso diretto per debug ─────────────────
    // Bit = 0 → premuto (logica negativa SMS)
    bool joy1Up()    const { return (_portDC & DC_J1_UP)    == 0; }
    bool joy1Down()  const { return (_portDC & DC_J1_DOWN)  == 0; }
    bool joy1Left()  const { return (_portDC & DC_J1_LEFT)  == 0; }
    bool joy1Right() const { return (_portDC & DC_J1_RIGHT) == 0; }
    bool joy1Btn1()  const { return (_portDC & DC_J1_BTN1)  == 0; }
    bool joy1Btn2()  const { return (_portDC & DC_J1_BTN2)  == 0; }
    bool joy2Up()    const { return (_portDC & DC_J2_UP)    == 0; }
    bool joy2Down()  const { return (_portDC & DC_J2_DOWN)  == 0; }
    bool joy2Left()  const { return (_portDD & DD_J2_LEFT)  == 0; }
    bool joy2Right() const { return (_portDD & DD_J2_RIGHT) == 0; }
    bool joy2Btn1()  const { return (_portDD & DD_J2_BTN1)  == 0; }
    bool joy2Btn2()  const { return (_portDD & DD_J2_BTN2)  == 0; }

private:
    // ── Oggetti joystick ──────────────────────────
    FunduinoJoystick _joy1;
    FunduinoJoystick _joy2;

    // ── Pin extra per Button 2 (TH pin) ──────────
    int _j1Btn2Pin;
    int _j2Btn2Pin;

    // ── Stato porte SMS (aggiornato da update()) ──
    // Già nel formato Z80 (logica negativa, bit=1 rilasciato)
    volatile uint8_t _portDC;  // bits Joy1 full + Joy2 Up/Down
    volatile uint8_t _portDD;  // bits Joy2 Left/Right/Btn + misc

    // ── Helpers ───────────────────────────────────
    // Converte lo stato FunduinoJoystick → bit SMS
    // Il FunduinoJoystick usa logica negativa (bit=0 premuto)
    // Il SMS usa la stessa logica → mappatura diretta dei bit
    inline bool pinPressed(int pin) const {
        if (pin < 0) return false;
        return digitalRead(pin) == LOW; // INPUT_PULLUP: LOW = premuto
    }
};
