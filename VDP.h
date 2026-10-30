/*
 * VDP.h — TMS9918A per emulatore SG-1000 (derivato dall'architettura PiSMS v3)
 *
 * Copyright (C) 2026 Ivan Vettori — GPLv3
 *
 * Modi supportati: Graphics I, Graphics II, Multicolor, Text (40x24).
 * Sprite: 32, 8x8 / 16x16, ingranditi x2, max 4 per linea, flag 5S e
 * collisione, early clock (bit 7 dell'attributo).
 *
 * Architettura "snapshot" (invariata rispetto a PiSMS):
 *   Core0 emula Z80 + stato VDP. Quando Core1 e' libera, a inizio frame
 *   copia la VRAM (16 KB) e per ogni linea registra gli 8 registri del VDP.
 *   Core1 disegna TUTTO il frame dallo snapshot, a blocchi di linee, e lo
 *   invia al display in DMA. Core0 non aspetta mai Core1: se e' ancora
 *   occupata col frame precedente, quel frame logico non viene catturato
 *   (frameskip adattivo).
 *
 *   Handshake: _snapState FREE > (Core0) READY > (Core1) BUSY > FREE
 *
 *   Il TMS9918A non ha CRAM: la palette e' fissa, quindi lo snapshot non
 *   contiene palette.
 */
#pragma once

#include <Arduino.h>
#include <stdint.h>

#define VDP_RAMFUNC __attribute__((section(".time_critical.vdp")))

// 1 = i pixel in uscita sono gia' nel byte-order del TFT
//     (usare tft.setSwapBytes(false)).
#define VDP_PRESWAP_565 1

// ── Dimensioni schermo (nomi mantenuti per compatibilita' col main PiSMS) ──
static constexpr int SMS_SCREEN_W   = 256;
static constexpr int SMS_SCREEN_H   = 192;
static constexpr int SMS_VRAM_SIZE  = 0x4000;

static constexpr int VDP_LINES_TOTAL  = 262;
static constexpr int VDP_LINES_ACTIVE = 192;
static constexpr int VDP_REG_COUNT    = 8;

// ── Bit dei registri ──────────────────────────────────────────────
static constexpr uint8_t R0_M3          = 0x02;   // Graphics II
static constexpr uint8_t R1_SCREEN_ON   = 0x40;   // BLANK (1 = schermo acceso)
static constexpr uint8_t R1_IE          = 0x20;   // interrupt frame
static constexpr uint8_t R1_M1          = 0x10;   // Text
static constexpr uint8_t R1_M2          = 0x08;   // Multicolor
static constexpr uint8_t R1_SPR_16      = 0x02;   // sprite 16x16
static constexpr uint8_t R1_SPR_MAG     = 0x01;   // sprite ingranditi

static constexpr uint8_t STATUS_F       = 0x80;   // frame (vblank)
static constexpr uint8_t STATUS_5S      = 0x40;   // quinto sprite sulla linea
static constexpr uint8_t STATUS_C       = 0x20;   // collisione sprite

// Stato dei registri per una linea (R0..R7), copiato nello snapshot
struct VDPLine {
  uint8_t r0, r1, r2, r3, r4, r5, r6, r7;
};
static_assert(sizeof(VDPLine) == VDP_REG_COUNT, "VDPLine deve coincidere con _reg[]");

class VDP {
public:
  VDP();
  void reset();

  // ── Accesso I/O dalla CPU (Core0) ─────────────
  void    writeControl(uint8_t value);   // porta 0xBF (scrittura)
  void    writeData(uint8_t value);      // porta 0xBE (scrittura)
  uint8_t readData();                    // porta 0xBE (lettura)
  uint8_t readStatus();                  // porta 0xBF (lettura)

  // ── Timing per linea (Core0) ──────────────────
  void beginLine(int line) { _currentLine = line; }
  void endLine(int line);                // dopo la CPU: flag 5S / collisione
  void signalVBlank();                   // fine area attiva

  // ── Cattura frame (Core0) ─────────────────────
  bool beginCapture();                   // false se Core1 ancora occupata
  void captureLine(int line);            // dopo l'esecuzione CPU della linea
  void endCapture();                     // frame completo > READY

  // ── Rendering (Core1) ─────────────────────────
  bool acquireSnapshot();                // true se c'e' un frame da disegnare
  void renderChunk(int firstLine, int count, uint16_t* out);
  void releaseSnapshot();                // dopo l'ultimo renderChunk

  // ── Interrupt (linea INT = F & IE) ────────────
  bool pendingVBlankIRQ() const { return _irq; }
  void clearVBlankIRQ()         { _irq = false; }

  int  currentLine() const { return _currentLine; }
  bool isVBlank()    const { return _currentLine >= VDP_LINES_ACTIVE; }

  uint8_t readVRAM(uint16_t addr)  const { return _vram[addr & 0x3FFF]; }
  void    writeVRAM(uint16_t addr, uint8_t v) { _vram[addr & 0x3FFF] = v; }

private:
  enum : uint8_t { SNAP_FREE = 0, SNAP_READY = 1, SNAP_BUSY = 2 };

  struct Snapshot {
    uint8_t  vram[SMS_VRAM_SIZE];
    VDPLine  line[VDP_LINES_ACTIVE];
  };

  // ── Stato "vivo" (Core0) ──────────────────────
  uint8_t  _vram[SMS_VRAM_SIZE];
  uint8_t  _reg[VDP_REG_COUNT];
  uint8_t  _status;
  uint8_t  _fifth;          // numero del quinto sprite (valido se 5S)
  uint16_t _addr;
  uint8_t  _latchData;
  bool     _latch;          // true = attesa del secondo byte di controllo
  uint8_t  _readBuf;
  int      _currentLine;
  bool     _irq;

  // ── Snapshot ──────────────────────────────────
  Snapshot _snap;
  volatile uint8_t _snapState;
};
