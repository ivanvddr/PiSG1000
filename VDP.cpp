/*
 * VDP.cpp — TMS9918A per emulatore SG-1000
 * Copyright (C) 2026 Ivan Vettori — GPLv3
 */

#include "VDP.h"
#include "hardware/sync.h"

// Tabelle in RAM (non flash): Core1 che legge dalla flash XIP sporcherebbe
// la cache condivisa e rallenterebbe Core0.
static uint16_t sPal[16];   // palette TMS9918A, RGB565 gia' nel byte-order d'uscita
static uint16_t sDbl[256];  // raddoppio orizzontale dei bit (sprite ingranditi)

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

static inline uint16_t outPx(uint16_t v) {
#if VDP_PRESWAP_565
  return (uint16_t)((v << 8) | (v >> 8));
#else
  return v;
#endif
}

// Una riga di uno sprite, gia' risolta (pixel allineati a sinistra nel bit 31)
struct SpriteLine {
  int16_t  x;
  uint8_t  color;
  uint8_t  width;
  uint32_t bits;
};

// ─────────────────────────────────────────────────
VDP::VDP() {
  // Palette TMS9918A (approssimazione standard). Indice 0 = trasparente.
  static const uint8_t rgb[16][3] = {
    {  0,   0,   0}, {  0,   0,   0}, { 33, 200,  66}, { 94, 220, 120},
    { 84,  85, 237}, {125, 118, 252}, {212,  82,  77}, { 66, 235, 245},
    {252,  85,  84}, {255, 121, 120}, {212, 193,  84}, {230, 206, 128},
    { 33, 176,  59}, {201,  91, 186}, {204, 204, 204}, {255, 255, 255}
  };
  for (int i = 0; i < 16; i++)
    sPal[i] = outPx(rgb565(rgb[i][0], rgb[i][1], rgb[i][2]));

  for (int i = 0; i < 256; i++) {
    uint16_t d = 0;
    for (int b = 0; b < 8; b++)
      if (i & (1 << b)) d |= (uint16_t)(3u << (b * 2));
    sDbl[i] = d;
  }
  reset();
}

void VDP::reset() {
  memset(_vram, 0, sizeof(_vram));
  memset(_reg,  0, sizeof(_reg));
  _status      = 0;
  _fifth       = 0;
  _addr        = 0;
  _latchData   = 0;
  _latch       = false;
  _readBuf     = 0;
  _currentLine = 0;
  _irq         = false;
  _snapState   = SNAP_FREE;
}

// ─────────────────────────────────────────────────
// Porte I/O (Core0, hot path > RAM)
// ─────────────────────────────────────────────────

VDP_RAMFUNC void VDP::writeControl(uint8_t value) {
  if (!_latch) {
    _latchData = value;
    _latch = true;
    return;
  }
  _latch = false;

  if (value & 0x80) {
    // Scrittura registro: dato = primo byte, numero = 3 bit bassi del secondo
    uint8_t r = value & 0x07;
    _reg[r] = _latchData;
    if (r == 1) {
      // INT = F & IE: abilitare IE con F gia' alzato genera subito l'interrupt
      if (_latchData & R1_IE) { if (_status & STATUS_F) _irq = true; }
      else                    { _irq = false; }
    }
  } else {
    _addr = (uint16_t)(((value & 0x3F) << 8) | _latchData);
    if (!(value & 0x40)) {
      // Setup lettura: prefetch
      _readBuf = _vram[_addr];
      _addr = (_addr + 1) & 0x3FFF;
    }
  }
}

VDP_RAMFUNC void VDP::writeData(uint8_t value) {
  _latch = false;
  _vram[_addr] = value;
  _readBuf = value;
  _addr = (_addr + 1) & 0x3FFF;
}

VDP_RAMFUNC uint8_t VDP::readData() {
  _latch = false;
  uint8_t ret = _readBuf;
  _readBuf = _vram[_addr];
  _addr = (_addr + 1) & 0x3FFF;
  return ret;
}

VDP_RAMFUNC uint8_t VDP::readStatus() {
  _latch = false;
  uint8_t s = (_status & 0xE0) | ((_status & STATUS_5S) ? _fifth : 0x1F);
  _status = 0;       // la lettura azzera F, 5S e C
  _irq = false;
  return s;
}

// ─────────────────────────────────────────────────
// Selezione sprite per una linea (usata da Core0 per i flag e da Core1
// per il disegno: stessa funzione, nessuna divergenza possibile)
// ─────────────────────────────────────────────────
static VDP_RAMFUNC int collectSprites(const uint8_t* vram, uint8_t r1,
                                      uint8_t r5, uint8_t r6, int line,
                                      SpriteLine* out, int* fifth) {
  const bool big = (r1 & R1_SPR_16) != 0;
  const int  mag = (r1 & R1_SPR_MAG) ? 1 : 0;
  const int  h   = (big ? 16 : 8) << mag;

  const uint8_t* sat = vram + ((uint16_t)(r5 & 0x7F) << 7);
  const uint8_t* pg  = vram + ((uint16_t)(r6 & 0x07) << 11);

  int n = 0;
  *fifth = -1;

  for (int i = 0; i < 32; i++) {
    const uint8_t* e = sat + (i << 2);
    int y = e[0];
    if (y == 0xD0) break;               // fine lista
    if (y > 0xE0) y -= 256;             // Y "negativo": sprite che entra dall'alto
    int row = line - (y + 1);
    if ((unsigned)row >= (unsigned)h) continue;

    if (n == 4) { *fifth = i; break; }  // quinto sprite sulla linea

    row >>= mag;
    uint8_t name = e[2];
    uint32_t bits;
    uint8_t  w;

    if (!big) {
      uint8_t p = pg[(name << 3) + row];
      if (!mag) { bits = (uint32_t)p << 24;           w = 8;  }
      else      { bits = (uint32_t)sDbl[p] << 16;     w = 16; }
    } else {
      const uint8_t* q = pg + ((name & 0xFC) << 3);
      uint8_t hi = q[row];
      uint8_t lo = q[16 + row];
      if (!mag) { bits = ((uint32_t)hi << 24) | ((uint32_t)lo << 16);        w = 16; }
      else      { bits = ((uint32_t)sDbl[hi] << 16) | (uint32_t)sDbl[lo];    w = 32; }
    }

    int x = e[1];
    uint8_t at = e[3];
    if (at & 0x80) x -= 32;             // early clock

    out[n].x     = (int16_t)x;
    out[n].color = at & 0x0F;
    out[n].width = w;
    out[n].bits  = bits;
    n++;
  }
  return n;
}

// ─────────────────────────────────────────────────
// Timing (Core0)
// ─────────────────────────────────────────────────

// Flag 5S e collisione, valutati a ogni linea attiva (anche sui frame non
// catturati: i giochi li leggono). Costo: una scansione della SAT per linea.
VDP_RAMFUNC void VDP::endLine(int line) {
  const uint8_t r1 = _reg[1];
  if (!(r1 & R1_SCREEN_ON) || (r1 & R1_M1)) return;   // blank o Text: niente sprite

  SpriteLine sl[4];
  int fifth;
  int n = collectSprites(_vram, r1, _reg[5], _reg[6], line, sl, &fifth);

  if (fifth >= 0 && !(_status & STATUS_5S)) {
    _status |= STATUS_5S;
    _fifth = (uint8_t)fifth;
  }

  if (n >= 2 && !(_status & STATUS_C)) {
    // Maschera di 10 parole = 320 posizioni (x+32): le parole 0 e 9 sono
    // fuori schermo e vengono ignorate
    uint32_t mask[10] = {0};
    for (int k = 0; k < n; k++) {
      if (!sl[k].bits) continue;
      int pos = sl[k].x + 32;                         // 0..287
      int wi  = pos >> 5;                             // 0..8
      uint64_t v = ((uint64_t)sl[k].bits << 32) >> (pos & 31);
      uint32_t a = (wi >= 1)     ? (uint32_t)(v >> 32) : 0;
      uint32_t b = (wi + 1 <= 8) ? (uint32_t)v         : 0;
      if ((mask[wi] & a) | (mask[wi + 1] & b)) { _status |= STATUS_C; break; }
      mask[wi]     |= a;
      mask[wi + 1] |= b;
    }
  }
}

VDP_RAMFUNC void VDP::signalVBlank() {
  _currentLine = VDP_LINES_ACTIVE;
  _status |= STATUS_F;
  if (_reg[1] & R1_IE) _irq = true;
}

// ─────────────────────────────────────────────────
// Cattura frame (Core0)
// ─────────────────────────────────────────────────

VDP_RAMFUNC bool VDP::beginCapture() {
  if (_snapState != SNAP_FREE) return false;
  memcpy(_snap.vram, _vram, SMS_VRAM_SIZE);
  return true;
}

VDP_RAMFUNC void VDP::captureLine(int line) {
  memcpy(&_snap.line[line], _reg, sizeof(VDPLine));
}

VDP_RAMFUNC void VDP::endCapture() {
  __dmb();
  _snapState = SNAP_READY;
}

// ─────────────────────────────────────────────────
// Handshake Core1
// ─────────────────────────────────────────────────

VDP_RAMFUNC bool VDP::acquireSnapshot() {
  if (_snapState != SNAP_READY) return false;
  __dmb();
  _snapState = SNAP_BUSY;
  return true;
}

VDP_RAMFUNC void VDP::releaseSnapshot() {
  __dmb();
  _snapState = SNAP_FREE;
}

// ─────────────────────────────────────────────────
// Rendering (Core1, tutto in RAM per non toccare la cache XIP di Core0)
// ─────────────────────────────────────────────────

static inline void put8(uint16_t* d, uint8_t b, uint16_t fg, uint16_t bg) {
  d[0] = (b & 0x80) ? fg : bg;
  d[1] = (b & 0x40) ? fg : bg;
  d[2] = (b & 0x20) ? fg : bg;
  d[3] = (b & 0x10) ? fg : bg;
  d[4] = (b & 0x08) ? fg : bg;
  d[5] = (b & 0x04) ? fg : bg;
  d[6] = (b & 0x02) ? fg : bg;
  d[7] = (b & 0x01) ? fg : bg;
}

static VDP_RAMFUNC void fillLine(uint16_t* dst, uint16_t c) {
  uint32_t c2 = c | ((uint32_t)c << 16);
  uint32_t* d32 = (uint32_t*)dst;
  for (int x = 0; x < SMS_SCREEN_W / 2; x++) d32[x] = c2;
}

// Graphics I: pattern per nome, colore per gruppi di 8 caratteri
static VDP_RAMFUNC void renderGfx1(const uint8_t* vram, const VDPLine& L,
                                   int line, uint16_t* dst, const uint16_t* lp) {
  const uint8_t* nt = vram + ((uint16_t)(L.r2 & 0x0F) << 10) + ((line >> 3) << 5);
  const uint8_t* pg = vram + ((uint16_t)(L.r4 & 0x07) << 11);
  const uint8_t* ct = vram + ((uint16_t)L.r3 << 6);
  const int py = line & 7;

  for (int col = 0; col < 32; col++) {
    uint8_t name = nt[col];
    uint8_t bits = pg[(name << 3) + py];
    uint8_t c    = ct[name >> 3];
    put8(dst + (col << 3), bits, lp[c >> 4], lp[c & 15]);
  }
}

// Graphics II: schermo diviso in terzi, pattern e colore per riga di ogni tile,
// R3/R4 fanno da maschera sull'indice (quirk hardware del TMS9918A)
static VDP_RAMFUNC void renderGfx2(const uint8_t* vram, const VDPLine& L,
                                   int line, uint16_t* dst, const uint16_t* lp) {
  const uint8_t* nt = vram + ((uint16_t)(L.r2 & 0x0F) << 10) + ((line >> 3) << 5);
  const uint8_t* pg = vram + ((uint16_t)(L.r4 & 0x04) << 11);
  const uint8_t* ct = vram + ((uint16_t)(L.r3 & 0x80) << 6);
  const int colMask = ((L.r3 & 0x7F) << 3) | 7;
  const int patMask = ((L.r4 & 0x03) << 8) | 0xFF;
  const int third   = (line >> 6) << 8;
  const int py      = line & 7;

  for (int col = 0; col < 32; col++) {
    int idx  = nt[col] | third;
    uint8_t bits = pg[((idx & patMask) << 3) + py];
    uint8_t c    = ct[((idx & colMask) << 3) + py];
    put8(dst + (col << 3), bits, lp[c >> 4], lp[c & 15]);
  }
}

// Multicolor: blocchi 4x4, ogni tile = 2x2 blocchi
static VDP_RAMFUNC void renderMulti(const uint8_t* vram, const VDPLine& L,
                                    int line, uint16_t* dst, const uint16_t* lp) {
  const int ty = line >> 3;
  const uint8_t* nt = vram + ((uint16_t)(L.r2 & 0x0F) << 10) + (ty << 5);
  const uint8_t* pg = vram + ((uint16_t)(L.r4 & 0x07) << 11);
  const int pr = ((ty & 3) << 1) | ((line & 7) >> 2);

  for (int col = 0; col < 32; col++) {
    uint8_t b = pg[(nt[col] << 3) + pr];
    uint16_t l = lp[b >> 4], r = lp[b & 15];
    uint16_t* d = dst + (col << 3);
    d[0] = l; d[1] = l; d[2] = l; d[3] = l;
    d[4] = r; d[5] = r; d[6] = r; d[7] = r;
  }
}

// Text: 40 colonne da 6 pixel, bordi da 8 pixel a destra e a sinistra
static VDP_RAMFUNC void renderText(const uint8_t* vram, const VDPLine& L,
                                   int line, uint16_t* dst, const uint16_t* lp) {
  const uint8_t* nt = vram + ((uint16_t)(L.r2 & 0x0F) << 10) + (line >> 3) * 40;
  const uint8_t* pg = vram + ((uint16_t)(L.r4 & 0x07) << 11);
  const uint16_t fg = lp[L.r7 >> 4];
  const uint16_t bg = lp[L.r7 & 15];
  const int py = line & 7;

  for (int x = 0; x < 8; x++)   dst[x]       = bg;
  for (int x = 248; x < 256; x++) dst[x]     = bg;

  uint16_t* d = dst + 8;
  for (int col = 0; col < 40; col++, d += 6) {
    uint8_t b = pg[(nt[col] << 3) + py];
    d[0] = (b & 0x80) ? fg : bg;
    d[1] = (b & 0x40) ? fg : bg;
    d[2] = (b & 0x20) ? fg : bg;
    d[3] = (b & 0x10) ? fg : bg;
    d[4] = (b & 0x08) ? fg : bg;
    d[5] = (b & 0x04) ? fg : bg;
  }
}

static VDP_RAMFUNC void renderSprites(const uint8_t* vram, const VDPLine& L,
                                      int line, uint16_t* dst) {
  SpriteLine sl[4];
  int fifth;
  int n = collectSprites(vram, L.r1, L.r5, L.r6, line, sl, &fifth);

  // Dal meno al piu' prioritario: lo sprite 0 ha priorita' massima e va
  // disegnato per ultimo. Colore 0 = trasparente (non disegna).
  for (int s = n - 1; s >= 0; s--) {
    if (!sl[s].color) continue;
    const uint16_t c    = sPal[sl[s].color];
    const uint32_t bits = sl[s].bits;
    const int w = sl[s].width;
    const int x = sl[s].x;
    int p0 = (x < 0) ? -x : 0;
    int p1 = (x + w > SMS_SCREEN_W) ? (SMS_SCREEN_W - x) : w;
    for (int px = p0; px < p1; px++)
      if (bits & (0x80000000u >> px)) dst[x + px] = c;
  }
}

VDP_RAMFUNC void VDP::renderChunk(int firstLine, int count, uint16_t* out) {
  uint16_t lp[16];                       // palette di linea: indice 0 = backdrop
  for (int k = 0; k < 16; k++) lp[k] = sPal[k];
  int bd = -1;

  for (int i = 0; i < count; i++) {
    const int line = firstLine + i;
    const VDPLine& L = _snap.line[line];
    uint16_t* dst = out + i * SMS_SCREEN_W;

    int b = L.r7 & 0x0F;
    if (b != bd) { bd = b; lp[0] = sPal[b]; }

    if (!(L.r1 & R1_SCREEN_ON)) {        // schermo spento: solo backdrop
      fillLine(dst, lp[0]);
      continue;
    }

    if      (L.r1 & R1_M1) { renderText (_snap.vram, L, line, dst, lp); continue; }
    else if (L.r1 & R1_M2)   renderMulti(_snap.vram, L, line, dst, lp);
    else if (L.r0 & R0_M3)   renderGfx2 (_snap.vram, L, line, dst, lp);
    else                     renderGfx1 (_snap.vram, L, line, dst, lp);

    renderSprites(_snap.vram, L, line, dst);
  }
}
