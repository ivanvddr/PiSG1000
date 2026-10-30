#pragma once
/*
 * SMSLoader.h — Loader ROM per emulatore SG-1000 (RP2040)
 * Copyright (C) 2026 Ivan Vettori — GPLv3
 *
 * Stessa classe e stessa API pubblica del loader di PiSMS, cosi' menu e
 * game browser restano invariati. Internamente e' molto piu' semplice:
 * l'SG-1000 non ha mapper.
 *
 * Mappa di memoria SG-1000
 * ------------------------
 *   $0000-$BFFF  cartuccia, fino a 48 KB. La ROM viene caricata INTERAMENTE
 *                in RAM all'avvio e il file viene chiuso: nessun accesso
 *                SD/LittleFS durante il gioco.
 *   (area libera) i KB sopra la fine della ROM sono RAM di espansione
 *                 (letta/scritta): serve ad es. a "The Castle" e agli
 *                 Othello Multivision (8 KB a $2000). Inizializzata a $FF.
 *   $C000-$FFFF  RAM di sistema (SG_RAM_SIZE) con mirror
 *
 * Le tabelle di pagina (64 x 1 KB) sono usate direttamente dallo Z80
 * (Z80::setPages): nessuna chiamata a funzione per accesso in memoria.
 */

#include <Arduino.h>
#include <SD.h>
#include <LittleFS.h>

#define LOADER_RAMFUNC __attribute__((section(".time_critical.loader")))

enum class FSType {
  FS_NONE,
  FS_SD,
  FS_LITTLEFS
};

#ifndef SD_CS
  #define SD_CS    13
  #define SD_MISO  12
  #define SD_MOSI  11
  #define SD_SCLK  10
#endif

// Mantenuto per compatibilita' con il codice PiSMS: l'SG-1000 non ha mapper
enum class SMSMapper : uint8_t {
  NONE,
  SEGA,
  CODEMASTERS,
  KOREAN
};

static constexpr uint32_t SMS_BANK_SIZE = 0x4000;
static constexpr uint32_t SG_ROM_AREA   = 0xC000;   // $0000-$BFFF = 48 KB

// RAM di sistema. 8 KB e' il valore piu' tollerante (SG-1000 reale: 1 KB,
// SC-3000: 2 KB, entrambi con mirror). Valori ammessi: 0x0400 .. 0x2000.
#ifndef SG_RAM_SIZE
  #define SG_RAM_SIZE 0x2000
#endif

// 0 = l'area sopra la ROM e' RAM di espansione (default)
// 1 = la ROM viene replicata (mirror) in tutta l'area $0000-$BFFF
#ifndef SG_MIRROR_ROM
  #define SG_MIRROR_ROM 0
#endif

class SMSLoader {
public:
  SMSLoader();
  ~SMSLoader() { unloadROM(); }

  // ── Filesystem ───────────────────────────────
  bool   initFilesystem(FSType preferred = FSType::FS_SD, int sdCsPin = 5);
  FSType currentFS() const { return _currentFS; }
  bool   fsReady()   const { return _fsReady; }
  void   closeFilesystem();

  // ── ROM ───────────────────────────────────────
  bool loadROM(const char* path);
  void unloadROM();
  bool isLoaded() const { return _loaded; }

  // ── Accesso memoria ───────────────────────────
  inline uint8_t read(uint16_t addr) const {
    return _rd[addr >> 10][addr & 0x3FF];
  }
  inline void write(uint16_t addr, uint8_t value) {
    uint8_t* p = _wr[addr >> 10];
    if (p) p[addr & 0x3FF] = value;
    else   writeSlow(addr, value);
  }
  // Percorso lento: con questa mappa non viene mai usato (tutte le pagine
  // sono scrivibili o puntano al "sink"), resta per compatibilita'
  void writeSlow(uint16_t addr, uint8_t value);

  // Tabelle di pagina per Z80::setPages()
  const uint8_t* const* readPages()  const { return _rd; }
  uint8_t* const*       writePages()       { return _wr; }

  // ── Info ──────────────────────────────────────
  uint32_t    romSize()    const { return _romSize; }
  uint8_t     numBanks()   const { return _numBanks; }
  SMSMapper   mapperType() const { return SMSMapper::NONE; }
  const char* romName()    const { return _romName; }
  uint8_t     slotBank(int s) const { return (uint8_t)s; }

  // ── Debug / utilita' ──────────────────────────
  void debugStatus() const;
  void listROMs(const char* folder = "/PiSMS/games");
  bool fileExists(const char* path);
  void setSelectedROM(const char* path);
  File openFile(const char* path, const char* mode);

  FSType      getCurrentFilesystem() const { return _currentFS; }
  const char* getSelectedROM()       const { return _romPath; }

private:
  // ── Memoria ───────────────────────────────────
  uint8_t _mem[SG_ROM_AREA];       // ROM (in basso) + RAM di espansione (sopra)
  uint8_t _ram[SG_RAM_SIZE];       // RAM di sistema
  uint8_t _sink[1024];             // destinazione delle scritture su ROM

  // ── Tabelle di pagina (64 x 1 KB) ─────────────
  const uint8_t* _rd[64];
  uint8_t*       _wr[64];

  // ── Stato ROM ─────────────────────────────────
  bool     _loaded;
  uint32_t _romSize;
  uint8_t  _numBanks;
  char     _romName[64];
  char     _romPath[128];
  bool     _hasHeader;

  // ── Filesystem ────────────────────────────────
  FSType _currentFS = FSType::FS_NONE;
  bool   _fsReady;
  int    _sdCsPin;
  File   _romFile;
  bool   _fileOpen;

  bool initSD();
  bool openROMFile();
  void closeROMFile();
  void rebuildPages();
  void extractName(const char* path);
};
