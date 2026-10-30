/*
 * SMSLoader.cpp — Loader ROM per emulatore SG-1000 (RP2040)
 * Copyright (C) 2026 Ivan Vettori — GPLv3
 */

#include "SMSLoader.h"

// ─────────────────────────────────────────────────
SMSLoader::SMSLoader()
  : _loaded(false), _romSize(0), _numBanks(0), _hasHeader(false)
  , _currentFS(FSType::FS_NONE), _fsReady(false)
  , _sdCsPin(5), _fileOpen(false)
{
  memset(_mem,  0xFF, sizeof(_mem));
  memset(_ram,  0x00, sizeof(_ram));
  memset(_sink, 0x00, sizeof(_sink));
  _romName[0] = '\0';
  _romPath[0] = '\0';
  rebuildPages();
}

// ─────────────────────────────────────────────────
// Tabelle di pagina
// ─────────────────────────────────────────────────
void SMSLoader::rebuildPages() {
  const uint32_t romPages = (_romSize + 1023) >> 10;

#if SG_MIRROR_ROM
  // Replica la ROM (arrotondata a potenza di 2) su tutto $0000-$BFFF
  uint32_t pow2 = 1;
  while (pow2 < romPages) pow2 <<= 1;
  for (uint32_t p = 0; p < 48; p++) {
    if (romPages) {
      _rd[p] = _mem + (p & (pow2 - 1)) * 1024;
      _wr[p] = _sink;
    } else {
      _rd[p] = _mem + p * 1024;
      _wr[p] = _mem + p * 1024;
    }
  }
#else
  for (uint32_t p = 0; p < 48; p++) {
    uint8_t* b = _mem + p * 1024;
    _rd[p] = b;
    _wr[p] = (p < romPages) ? _sink : b;   // ROM: scritture ignorate; sopra: RAM
  }
#endif

  // $C000-$FFFF: RAM di sistema con mirror
  for (int p = 48; p < 64; p++) {
    uint8_t* r = _ram + (((p - 48) * 1024) % SG_RAM_SIZE);
    _rd[p] = r;
    _wr[p] = r;
  }
}

LOADER_RAMFUNC void SMSLoader::writeSlow(uint16_t addr, uint8_t value) {
  (void)addr; (void)value;   // nessun registro mapper sull'SG-1000
}

// ─────────────────────────────────────────────────
// Filesystem: SD (SPI1) con fallback su LittleFS
// ─────────────────────────────────────────────────
bool SMSLoader::initFilesystem(FSType preferred, int sdCsPin) {
  if (_fsReady) return true;
  _sdCsPin = sdCsPin;

  auto trySD = [&]() -> bool {
    Serial.println("[Loader] SD...");
    if (initSD()) {
      _currentFS = FSType::FS_SD;
      _fsReady = true;
      Serial.println("[Loader] SD OK");
      return true;
    }
    Serial.println("[Loader] SD not available");
    return false;
  };

  auto tryLFS = [&]() -> bool {
    Serial.println("[Loader] LittleFS...");
    if (LittleFS.begin()) {
      _currentFS = FSType::FS_LITTLEFS;
      _fsReady = true;
      Serial.println("[Loader] LittleFS OK");
      return true;
    }
    Serial.println("[Loader] LittleFS not available");
    return false;
  };

  if (preferred == FSType::FS_SD) {
    if (trySD())  return true;
    if (tryLFS()) return true;
  } else {
    if (tryLFS()) return true;
    if (trySD())  return true;
  }

  Serial.println("[Loader] ERROR: no filesystem found");
  return false;
}

void SMSLoader::closeFilesystem() {
  closeROMFile();
  if (_currentFS == FSType::FS_SD) {
    SD.end();
    Serial.println("[Loader] SD closed");
  }
  _fsReady = false;
  _currentFS = FSType::FS_NONE;
}

bool SMSLoader::initSD() {
  SPI1.setRX(SD_MISO);
  SPI1.setTX(SD_MOSI);
  SPI1.setSCK(SD_SCLK);
  SPI1.setCS(SD_CS);
  SPI1.begin();
  delay(100);
  return SD.begin(SD_CS, 10000000, SPI1);
}

// ─────────────────────────────────────────────────
// loadROM: l'intera ROM (max 48 KB) finisce in RAM, il file viene chiuso
// ─────────────────────────────────────────────────
bool SMSLoader::loadROM(const char* path) {
  if (!path || path[0] == '\0') return false;

  if (!_fsReady) {
    if (!initFilesystem()) {
      Serial.println("[Loader] ERROR: filesystem not ready");
      return false;
    }
  }

  if (_loaded) unloadROM();

  strncpy(_romPath, path, sizeof(_romPath) - 1);
  _romPath[sizeof(_romPath) - 1] = '\0';
  extractName(path);

  Serial.printf("\n[Loader] Loading: %s\n", path);

  if (!openROMFile()) return false;

  uint32_t fileSize = _romFile.size();
  Serial.printf("[Loader] File size: %lu byte\n", (unsigned long)fileSize);

  if (fileSize < 16) {
    Serial.println("[Loader] ERROR: file too small");
    closeROMFile(); return false;
  }

  // Alcuni dump hanno un header da 512 byte (copier)
  _hasHeader = ((fileSize & 0x3FF) == 0x200);
  uint32_t hdr = _hasHeader ? 512 : 0;
  _romSize = fileSize - hdr;

  if (_romSize == 0 || _romSize > SG_ROM_AREA) {
    Serial.printf("[Loader] ERROR: ROM size not supported (%lu byte, max %lu)\n",
                  (unsigned long)_romSize, (unsigned long)SG_ROM_AREA);
    _romSize = 0;
    closeROMFile(); return false;
  }
  _numBanks = (uint8_t)((_romSize + SMS_BANK_SIZE - 1) / SMS_BANK_SIZE);

  Serial.printf("[Loader] ROM: %-32s %lu KB%s\n", _romName,
                (unsigned long)((_romSize + 1023) / 1024),
                _hasHeader ? " (512B header skipped)" : "");

  memset(_mem, 0xFF, sizeof(_mem));
  if (hdr && !_romFile.seek(hdr)) {
    Serial.println("[Loader] ERROR: seek");
    _romSize = 0; closeROMFile(); return false;
  }
  uint32_t got = _romFile.read(_mem, _romSize);
  closeROMFile();

  if (got != _romSize) {
    Serial.printf("[Loader] ERROR: read %lu/%lu\n",
                  (unsigned long)got, (unsigned long)_romSize);
    _romSize = 0;
    rebuildPages();
    return false;
  }

  memset(_ram, 0x00, sizeof(_ram));
  _loaded = true;
  rebuildPages();
  Serial.println("[Loader] ROM all in RAM: file closed");
  Serial.println("[Loader] ROM ready!\n");
  return true;
}

void SMSLoader::unloadROM() {
  closeROMFile();
  memset(_mem, 0xFF, sizeof(_mem));
  memset(_ram, 0x00, sizeof(_ram));
  _loaded    = false;
  _romSize   = 0;
  _numBanks  = 0;
  _hasHeader = false;
  _romName[0] = '\0';
  _romPath[0] = '\0';
  rebuildPages();
}

// ─────────────────────────────────────────────────
// Helpers filesystem
// ─────────────────────────────────────────────────
bool SMSLoader::openROMFile() {
  if (_fileOpen) return true;
  if (!_fsReady) return false;

  if (_currentFS == FSType::FS_SD) _romFile = SD.open(_romPath, FILE_READ);
  else                             _romFile = LittleFS.open(_romPath, "r");

  _fileOpen = (bool)_romFile;
  if (!_fileOpen) Serial.printf("[Loader] ERROR: cannot open %s\n", _romPath);
  return _fileOpen;
}

void SMSLoader::closeROMFile() {
  if (_fileOpen) {
    _romFile.close();
    _fileOpen = false;
  }
}

File SMSLoader::openFile(const char* path, const char* mode) {
  if (_currentFS == FSType::FS_SD)       return SD.open(path, mode);
  if (_currentFS == FSType::FS_LITTLEFS) return LittleFS.open(path, mode);
  return File();
}

bool SMSLoader::fileExists(const char* path) {
  if (_currentFS == FSType::FS_SD)       return SD.exists(path);
  if (_currentFS == FSType::FS_LITTLEFS) return LittleFS.exists(path);
  return false;
}

void SMSLoader::extractName(const char* path) {
  if (!path) { _romName[0] = '\0'; return; }
  const char* slash = strrchr(path, '/');
  if (!slash) slash = strrchr(path, '\\');
  const char* base = slash ? slash + 1 : path;
  strncpy(_romName, base, sizeof(_romName) - 1);
  _romName[sizeof(_romName) - 1] = '\0';
  char* dot = strrchr(_romName, '.');
  if (dot) *dot = '\0';
}

void SMSLoader::setSelectedROM(const char* path) {
  if (!path) return;
  strncpy(_romPath, path, sizeof(_romPath) - 1);
  _romPath[sizeof(_romPath) - 1] = '\0';
  Serial.printf("SMSLoader: ROM selected > %s\n", _romPath);
}

void SMSLoader::debugStatus() const {
  if (!_loaded) { Serial.println("[Loader] No ROM loaded"); return; }
  Serial.println("\n=== SG-1000 Loader Status ===");
  Serial.printf("ROM    : %s (%lu bytes)\n", _romName, (unsigned long)_romSize);
  Serial.printf("Header : %s\n", _hasHeader ? "yes" : "no");
  Serial.printf("File   : %s\n", _fileOpen ? "open" : "closed (ROM all in RAM)");
  Serial.printf("Cart   : $0000-$%04lX ROM%s\n",
                (unsigned long)(((_romSize + 1023) & ~1023UL) - 1),
                SG_MIRROR_ROM ? ", mirrored up to $BFFF" : ", free area = expansion RAM");
  Serial.printf("RAM    : $C000-$FFFF, %u KB mirrored\n", (unsigned)(SG_RAM_SIZE / 1024));
  Serial.println("=============================\n");
}

void SMSLoader::listROMs(const char* folder) {
  if (!_fsReady) { Serial.println("[Loader] Filesystem not ready"); return; }

  Serial.printf("ROM in %s:\n", folder);
  Serial.println("==========================================");

  auto isROM = [](const char* name) -> bool {
    const char* dot = strrchr(name, '.');
    if (!dot) return false;
    return strcasecmp(dot, ".sg")  == 0 ||
           strcasecmp(dot, ".sc")  == 0 ||
           strcasecmp(dot, ".bin") == 0;
  };

  File root = openFile(folder, "r");
  if (!root || !root.isDirectory()) { Serial.println(" ERROR: folder not found"); return; }

  File entry;
  while ((entry = root.openNextFile())) {
    if (!entry.isDirectory() && isROM(entry.name())) {
      Serial.printf(" %-40s %6lu KB\n", entry.name(),
                    (unsigned long)(entry.size() / 1024));
    }
    entry.close();
  }
  root.close();
  Serial.println("==========================================");
}
