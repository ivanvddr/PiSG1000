#pragma once
/*
 * PSG SN76489 — v3: uscita I2S (MAX98357), sintesi interamente su Core1
 *
 * Copyright (C) 2026 Ivan Vettori — GPLv3
 *
 * ─────────────────────────────────────────────────────────────────
 *  Pipeline:
 *
 *   Core0 (Z80)                          Core1
 *   ───────────────────────────          ────────────────────────────────
 *   OUT (0x7E/0x7F) → psg.write()        psg.pump()  (chiamata di continuo)
 *     └→ accoda {timestamp, byte}          ├→ legge il tempo di Core0
 *        (nessun calcolo audio su Core0)    │   (cpu.totalCycles)
 *                                           ├→ applica gli eventi al
 *                                           │   timestamp esatto
 *                                           ├→ sintetizza i campioni
 *                                           └→ li scrive nel buffer I2S
 *
 *  - Core0 paga solo un push in coda per ogni scrittura PSG.
 *  - Tutto lo stato DSP appartiene a Core1: nessuna race.
 *  - I timestamp sono in cicli Z80 > le note cadono sempre al punto
 *    giusto, indipendentemente da quando gira Core1.
 *  - Il synth resta indietro di AUDIO_LAG_CYCLES rispetto a Core0 per
 *    assorbire la natura "a raffiche" dell'emulazione (Core0 corre un
 *    frame in ca 10 ms e poi attende): latenza totale ca lag + buffer I2S.
 *
 *  Collegamenti MAX98357:
 *     BCLK > GP26,  LRC > GP27 (= BCLK+1, imposto dal PIO),  DIN > GP28
 * ─────────────────────────────────────────────────────────────────
 */

#include <Arduino.h>
#include <I2S.h>
#include <stdint.h>
#include "hardware/sync.h"
#include "hardware/timer.h"

#define PSG_RAMFUNC __attribute__((section(".time_critical.psg")))

// ── Traccia diagnostica delle scritture PSG ───────────
// Dopo PSG_TRACE_SKIP scritture ne registra PSG_TRACE_N e le stampa UNA
// volta in seriale (decodificate, con distanza in cicli Z80 fra l'una e
// l'altra). Mettere PSG_TRACE a 0 per disattivarla.
#define PSG_TRACE       0
#define PSG_TRACE_SKIP  1500
#define PSG_TRACE_N     200

// ── Clock / sample rate ───────────────────────────────
static constexpr uint32_t PSG_CLOCK_HZ    = 3579545UL;
static constexpr uint32_t PSG_SAMPLE_RATE = 44100;

// ── Buffer I2S (arduino-pico): N buffer × FRAMES frame stereo ──
// 8 × 64 frame @44.1k ca 11.6 ms. Buffer piccoli = stima della posizione
// di riproduzione più precisa (granularità 64 frame ca 1.45 ms).
static constexpr int I2S_BUFFERS    = 8;
static constexpr int I2S_BUF_FRAMES = 64;

// ── SINCRONIA AUDIO > EMULAZIONE ──────────────────────
// Il clock audio (I2S) è il master del ritmo di emulazione:
//   Core0 a fine frame attende finché i cicli Z80 emulati ma non ancora
//   riprodotti (leadCycles) scendono sotto AUDIO_TARGET_LEAD_CYCLES.
//   Core1 sintetizza subito tutto ciò che Core0 ha emulato e lo versa
//   nell'I2S man mano che si libera posto.
// Risultato: la velocità di emulazione segue esattamente i 44100 Hz
// dell'I2S (= 3.579545 MHz Z80), nessuna deriva, nessun resync, e la
// latenza audio è pari al solo "lead" (circa il valore sotto).
//   45000 cicli ca 12.6 ms.
static constexpr uint32_t AUDIO_TARGET_LEAD_CYCLES = 45000;

// Il synth non supera mai il tempo emulato da Core0 (piccolo margine di sicurezza)
static constexpr uint32_t AUDIO_LAG_CYCLES        = 1000;
// Se il synth resta indietro oltre questo limite (es. stallo SD) salta avanti.
static constexpr uint32_t AUDIO_MAX_BEHIND_CYCLES = 200000;

// ── Coda eventi Core0 > Core1 ─────────────────────────
static constexpr int EV_BITS = 10;
static constexpr int EV_SIZE = 1 << EV_BITS;
static constexpr int EV_MASK = EV_SIZE - 1;

// ── Volume master (Q8: 256 = 1.0x) ────────────────────
// Con 256 il fondo scala teorico (4 canali al massimo) è +-16383 (-6 dB).
// Default 512 > +-32766: più volume, clip solo in casi estremi.
static constexpr uint16_t AUDIO_GAIN_UNITY   = 256;
static constexpr uint16_t AUDIO_GAIN_DEFAULT = 512;

struct PSGToneChannel {
    uint16_t toneReg;
    uint8_t  volReg;
    uint32_t period;   // mezzo periodo in unità STEP (16.16 di campione)
    int32_t  count;
    uint8_t  output;
};

struct PSGNoiseChannel {
    uint8_t  noiseReg;
    uint8_t  volReg;
    uint32_t lfsr;
    uint32_t period;
    int32_t  count;
    uint8_t  output;
};

class PSG {
public:
    PSG();

    // Core0, prima di avviare Core1 (SMSsetup)
    void reset();
    void configure(uint8_t bclkPin, uint8_t dataPin,
                   const volatile uint32_t* cycleSource);

    // Core1: avvia I2S (l'IRQ DMA della libreria gira su questo core)
    void beginAudio();
    // Core1: da chiamare spesso (loop principale + attese DMA video)
    void pump();

    // Core0: sync col clock audio
    bool audioRunning() const { return _running; }
    // Cicli Z80 già emulati da Core0 ma non ancora riprodotti dall'I2S
    // (≈ latenza audio corrente). Aggiornato da Core1 a ogni pump().
    int32_t leadCycles() const {
        return _cycleSrc ? (int32_t)(*_cycleSrc - _playedCycles) : 0;
    }

    // Core0: porta 0x7E/0x7F
    void write(uint8_t value);
    void traceDump();                   // Core0: stampa la traccia (una volta)

    // Volume
    void setGainQ8(uint16_t g) { _gainQ8 = g; }
    uint16_t gainQ8() const    { return _gainQ8; }
    void setVolume(float v01) {
        if (v01 < 0.0f) v01 = 0.0f;
        if (v01 > 1.0f) v01 = 1.0f;
        _gainQ8 = (uint16_t)(v01 * v01 * (AUDIO_GAIN_UNITY * 2) + 0.5f);
    }

    // Diagnostica
    uint32_t starved()       const { return _starved; }
    // Underrun REALI dell'I2S (il DMA ha riprodotto silenzio perché non c'era
    // un buffer pieno pronto) dall'avvio
    uint32_t underflows()    const { return _underflows; }
    // Massimo intervallo (us) fra due pump() consecutive dall'ultima chiamata:
    // misura quanto a lungo Core1 è rimasta senza servire l'audio
    uint32_t takeMaxGapUs()  { uint32_t g = _maxGapUs; _maxGapUs = 0; return g; }
    uint32_t eventsDropped() const { return _evDropped; }
    uint32_t writeCount()    const { return _writeCount; }
    int      eventsPending() const { return (_evHead - _evTail) & EV_MASK; }
    uint8_t  volumeReg(int ch) const {
        return (ch < 3) ? _tone[ch].volReg : _noise.volReg;
    }

private:
    // ── DSP (di proprietà di Core1) ───────────────────
    PSGToneChannel  _tone[3];
    PSGNoiseChannel _noise;
    uint8_t  _latchedChannel;
    bool     _latchedIsVolume;
    uint8_t  _latchedFreqLo;
    uint16_t _gainQ8;
    int32_t  _dcLP;

    // ── Coda eventi (SPSC: Core0 produce, Core1 consuma) ──
    uint32_t          _evStamp[EV_SIZE];
    uint8_t           _evVal[EV_SIZE];
    volatile uint16_t _evHead;
    volatile uint16_t _evTail;
    uint32_t          _evDropped;
    uint32_t          _writeCount;

    // ── Temporizzazione ───────────────────────────────
    const volatile uint32_t* _cycleSrc;   // &cpu.totalCycles
    uint32_t _last32;
    uint64_t _core0Time;                  // tempo Core0 "srotolato" a 64 bit
    int64_t  _synthQ;                     // posizione synth, Q16 cicli Z80
    uint32_t _starved;
    bool     _starving;
    uint32_t _underflows;
    uint32_t _lastPumpUs;
    uint32_t _maxGapUs;

    // ── I2S ───────────────────────────────────────────
    I2S      _i2s;
    uint8_t  _pinBclk;
    uint8_t  _pinData;
#if PSG_TRACE
    uint32_t _trStamp[PSG_TRACE_N];
    uint8_t  _trVal[PSG_TRACE_N];
    uint16_t _trCount;
#endif
    volatile bool     _running;
    volatile uint32_t _playedCycles;      // posizione di riproduzione, dominio cicli Z80

    void    applyWrite(uint8_t value);
    void    recalcToneInc(int ch);
    void    recalcNoiseInc();
    int32_t renderSample();
    inline int32_t tickTone(int ch);
    inline int32_t tickNoise();
    inline void    stepLFSR();
};