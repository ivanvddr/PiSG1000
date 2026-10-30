/*
 * PSG SN76489 — v3 (I2S MAX98357, sintesi su Core1)
 * Copyright (C) 2026 Ivan Vettori — GPLv3
 */

#include "PSG.h"

static constexpr uint32_t STEP      = 0x10000;   // 1 campione in 16.16
static constexpr uint32_t LFSR_INIT = 0x8000;

// Durata (unità STEP) di un ciclo PSG dopo il prescaler /16
static constexpr uint32_t PSG_UPDATE_STEP =
    (uint32_t)(((uint64_t)STEP * PSG_SAMPLE_RATE * 16) / PSG_CLOCK_HZ);

// Cicli Z80 per campione audio, Q16
static constexpr int64_t CYC_PER_SAMPLE_Q16 =
    (int64_t)(((uint64_t)PSG_CLOCK_HZ << 16) / PSG_SAMPLE_RATE);

// Tabella volumi in RAM (non in flash: evita traffico XIP da Core1)
static uint16_t kVol[16] = {
    32767, 26028, 20675, 16422,
    13045, 10362,  8231,  6538,
     5193,  4125,  3276,  2602,
     2067,  1642,  1304,     0
};

static inline uint32_t periodFromN(uint16_t N) {
    uint32_t p = PSG_UPDATE_STEP * (uint32_t)N;
    return p ? p : PSG_UPDATE_STEP;
}

// ─────────────────────────────────────────────────────
PSG::PSG()
    : _latchedChannel(0), _latchedIsVolume(false), _latchedFreqLo(0)
    , _gainQ8(AUDIO_GAIN_DEFAULT), _dcLP(0)
    , _evHead(0), _evTail(0), _evDropped(0), _writeCount(0)
    , _cycleSrc(nullptr), _last32(0), _core0Time(0), _synthQ(0)
    , _starved(0), _starving(false), _underflows(0), _lastPumpUs(0), _maxGapUs(0)
    , _i2s(OUTPUT), _pinBclk(26), _pinData(28), _running(false)
{
    reset();
}

void PSG::reset() {
    for (int i = 0; i < 3; i++) {
        _tone[i].toneReg = 0;
        _tone[i].volReg  = 15;
        _tone[i].period  = PSG_UPDATE_STEP;
        _tone[i].count   = (int32_t)PSG_UPDATE_STEP;
        _tone[i].output  = 0;
    }
    _noise.noiseReg = 0;
    _noise.volReg   = 15;
    _noise.lfsr     = LFSR_INIT;
    _noise.period   = PSG_UPDATE_STEP << 5;
    _noise.count    = (int32_t)_noise.period;
    _noise.output   = (uint8_t)(_noise.lfsr & 1);

    // Dopo il reset il registro selezionato è il n.3 (volume del canale 1), quindi un byte "dati" isolato agisce lì
    _latchedChannel = 1; _latchedIsVolume = true; _latchedFreqLo = 0;
#if PSG_TRACE
    _trCount = 0;
#endif
    _dcLP = 0;
    _evHead = 0; _evTail = 0;
    _evDropped = 0; _writeCount = 0;
    _starved = 0; _starving = false;
    _underflows = 0; _lastPumpUs = 0; _maxGapUs = 0;
    _synthQ = 0; _core0Time = 0; _last32 = 0;
    _playedCycles = 0;
}

void PSG::configure(uint8_t bclkPin, uint8_t dataPin,
                    const volatile uint32_t* cycleSource) {
    _pinBclk  = bclkPin;
    _pinData  = dataPin;
    _cycleSrc = cycleSource;
}

// ─────────────────────────────────────────────────────
//  Core1: avvio I2S. LRCLK = BCLK + 1 (GP27) è imposto dal programma PIO.
// ─────────────────────────────────────────────────────
void PSG::beginAudio() {
    _i2s.setBCLK(_pinBclk);
    _i2s.setDATA(_pinData);
    _i2s.setBitsPerSample(16);
    _i2s.setBuffers(I2S_BUFFERS, I2S_BUF_FRAMES);

    _running = _i2s.begin(PSG_SAMPLE_RATE);
    _i2s.getOverUnderflow();                       // azzera il flag di avvio

    uint32_t now = _cycleSrc ? *_cycleSrc : 0;
    _last32    = now;
    _core0Time = now;
    _synthQ    = (int64_t)now * 65536;

    Serial.printf("[PSG] I2S %s: BCLK=GP%u LRC=GP%u DIN=GP%u  %lu Hz 16bit  "
                  "buffer %dx%d  lag=%lu cycles\n",
                  _running ? "OK" : "ERROR",
                  _pinBclk, _pinBclk + 1, _pinData,
                  (unsigned long)PSG_SAMPLE_RATE, I2S_BUFFERS, I2S_BUF_FRAMES,
                  (unsigned long)AUDIO_LAG_CYCLES);
}

// ─────────────────────────────────────────────────────
//  Core0: accoda la scrittura con timestamp (cicli Z80). Mai bloccante.
// ─────────────────────────────────────────────────────
PSG_RAMFUNC void PSG::write(uint8_t value) {
    _writeCount++;
#if PSG_TRACE
    if (_writeCount > PSG_TRACE_SKIP && _trCount < PSG_TRACE_N) {
        _trStamp[_trCount] = _cycleSrc ? *_cycleSrc : 0;
        _trVal[_trCount]   = value;
        _trCount++;
    }
#endif
    uint16_t h = _evHead;
    uint16_t n = (h + 1) & EV_MASK;
    if (n == _evTail) { _evDropped++; return; }
    _evStamp[h] = _cycleSrc ? *_cycleSrc : 0;
    _evVal[h]   = value;
    __dmb();
    _evHead = n;
}

// ─────────────────────────────────────────────────────
//  Core1: protocollo SN76489 (spostato qui da write())
// ─────────────────────────────────────────────────────
PSG_RAMFUNC void PSG::applyWrite(uint8_t value) {
    if (value & 0x80) {
        _latchedChannel  = (value >> 5) & 0x03;
        _latchedIsVolume = (value >> 4) & 0x01;
        _latchedFreqLo   = value & 0x0F;

        if (_latchedIsVolume) {
            uint8_t vol = value & 0x0F;
            if (_latchedChannel < 3) _tone[_latchedChannel].volReg = vol;
            else                     _noise.volReg = vol;
        } else if (_latchedChannel == 3) {
            _noise.noiseReg = value & 0x0F;
            _noise.lfsr     = LFSR_INIT;
            recalcNoiseInc();
        } else {
            uint16_t& reg = _tone[_latchedChannel].toneReg;
            reg = (reg & 0x3F0) | _latchedFreqLo;
            recalcToneInc(_latchedChannel);
        }
    } else {
        // Byte "dati": agisce sul registro selezionato dall'ultimo latch
        // (vale anche per volume e rumore, non solo per i toni)
        if (_latchedIsVolume) {
            uint8_t vol = value & 0x0F;
            if (_latchedChannel < 3) _tone[_latchedChannel].volReg = vol;
            else                     _noise.volReg = vol;
        } else if (_latchedChannel == 3) {
            _noise.noiseReg = value & 0x0F;
            _noise.lfsr     = LFSR_INIT;
            recalcNoiseInc();
        } else {
            uint16_t& reg = _tone[_latchedChannel].toneReg;
            reg = (reg & 0x00F) | ((uint16_t)(value & 0x3F) << 4);
            recalcToneInc(_latchedChannel);
        }
    }
}

PSG_RAMFUNC void PSG::recalcToneInc(int ch) {
    _tone[ch].period = periodFromN(_tone[ch].toneReg);
    if (ch == 2 && (_noise.noiseReg & 0x03) == 3) recalcNoiseInc();
}

PSG_RAMFUNC void PSG::recalcNoiseInc() {
    switch (_noise.noiseReg & 0x03) {
        case 0: _noise.period = PSG_UPDATE_STEP << 5; break;
        case 1: _noise.period = PSG_UPDATE_STEP << 6; break;
        case 2: _noise.period = PSG_UPDATE_STEP << 7; break;
        default: _noise.period = 2 * _tone[2].period; break;
    }
    if (_noise.period == 0) _noise.period = PSG_UPDATE_STEP;
}

PSG_RAMFUNC inline void PSG::stepLFSR() {
    bool white = (_noise.noiseReg & 0x04) != 0;
    uint32_t fb = white
        ? ((_noise.lfsr & 1) ^ ((_noise.lfsr >> 3) & 1))
        : (_noise.lfsr & 1);
    _noise.lfsr   = (_noise.lfsr >> 1) | (fb << 15);
    _noise.output = (uint8_t)(_noise.lfsr & 1);
}

// Edge-accurate: frazione di campione trascorsa a livello alto
PSG_RAMFUNC inline int32_t PSG::tickTone(int ch) {
    PSGToneChannel& t = _tone[ch];

    uint32_t hi        = 0;
    int32_t  remaining = (int32_t)STEP;
    while (remaining > 0) {
        int32_t segment = (t.count < remaining) ? t.count : remaining;
        if (segment > 0) {
            if (t.output) hi += (uint32_t)segment;
            t.count   -= segment;
            remaining -= segment;
        }
        if (t.count <= 0) {
            t.output ^= 1;
            t.count  += (int32_t)t.period;
        }
    }

    if (t.volReg == 15) return 0;
    uint32_t vol    = kVol[t.volReg] >> 1;                // max 16383
    uint32_t scaled = (hi * (vol << 1)) >> 16;
    return (int32_t)scaled - (int32_t)vol;
}

PSG_RAMFUNC inline int32_t PSG::tickNoise() {
    _noise.count -= (int32_t)STEP;
    while (_noise.count <= 0) {
        _noise.count += (int32_t)_noise.period;
        stepLFSR();
    }
    if (_noise.volReg == 15) return 0;
    int32_t vol = kVol[_noise.volReg] >> 1;
    return _noise.output ? vol : -vol;
}

// mix (±65532 max) → DC-block → gain → int16
PSG_RAMFUNC int32_t PSG::renderSample() {
    int32_t mix = tickTone(0) + tickTone(1) + tickTone(2) + tickNoise();

    // DC-blocker a 1 polo, 255/256 → taglio ≈ 27 Hz @44.1k
    _dcLP = (_dcLP * 255 + mix) >> 8;
    mix  -= _dcLP;

    // gain 256 → mix>>2 (fondo scala ±16383)
    mix = (mix * (int32_t)_gainQ8) >> 10;

    if (mix >  32767) mix =  32767;
    if (mix < -32768) mix = -32768;
    return mix;
}

// ─────────────────────────────────────────────────────
//  Core1: produce campioni finché c'è posto nel buffer I2S e finché
//  Core0 ha "già emulato" quel tempo (meno il lag di sicurezza).
// ─────────────────────────────────────────────────────
PSG_RAMFUNC void PSG::pump() {
    if (!_running || !_cycleSrc) return;

    // Diagnostica: intervallo massimo fra pump() e underrun reali dell'I2S
    uint32_t nowUs = time_us_32();
    if (_lastPumpUs) {
        uint32_t gap = nowUs - _lastPumpUs;
        if (gap > _maxGapUs) _maxGapUs = gap;
    }
    _lastPumpUs = nowUs;
    if (_i2s.getOverUnderflow()) _underflows++;

    int avail = _i2s.availableForWrite() >> 2;     // byte → frame stereo 16 bit
    if (avail < 0) avail = 0;

    uint32_t now32 = *_cycleSrc;
    _core0Time += (uint32_t)(now32 - _last32);
    _last32 = now32;

    // Posizione di riproduzione = posizione del synth − campioni ancora nel
    // buffer I2S. Va pubblicata anche a buffer pieno: è ciò che Core0 usa
    // per regolare la velocità di emulazione (leadCycles()).
    int buffered = I2S_BUFFERS * I2S_BUF_FRAMES - avail;
    if (buffered < 0) buffered = 0;
    _playedCycles = (uint32_t)((_synthQ - (int64_t)buffered * CYC_PER_SAMPLE_Q16) >> 16);

    if (avail == 0) return;

    int64_t limitQ = ((int64_t)_core0Time - (int64_t)AUDIO_LAG_CYCLES) * 65536;

    // Resync dopo stalli lunghi (es. lettura SD): salta avanti
    if (limitQ - _synthQ > (int64_t)AUDIO_MAX_BEHIND_CYCLES * 65536) {
        _synthQ = limitQ - CYC_PER_SAMPLE_Q16;
    }

    uint32_t buf[64];
    int n = 0;

    while (avail > 0 && (_synthQ + CYC_PER_SAMPLE_Q16) <= limitQ) {
        _synthQ += CYC_PER_SAMPLE_Q16;
        int64_t posCyc = _synthQ >> 16;

        // applica gli eventi scaduti
        while (_evTail != _evHead) {
            int32_t d     = (int32_t)(now32 - _evStamp[_evTail]);
            int64_t stamp = (int64_t)_core0Time - d;
            if (stamp > posCyc) break;
            applyWrite(_evVal[_evTail]);
            _evTail = (_evTail + 1) & EV_MASK;
        }

        uint32_t u = (uint16_t)renderSample();
        buf[n++] = (u << 16) | u;                  // mono -> L=R
        avail--;
        if (n == 64) { _i2s.write((const uint8_t*)buf, n * 4); n = 0; }
    }
    if (n) _i2s.write((const uint8_t*)buf, n * 4);

    // Diagnostica: buffer quasi vuoto e nessun dato da produrre
    bool starving = (avail > (I2S_BUFFERS * I2S_BUF_FRAMES * 3) / 4);
    if (starving && !_starving) _starved++;
    _starving = starving;
}

// ─────────────────────────────────────────────────────
//  Core0: stampa la traccia delle scritture (una sola volta)
// ─────────────────────────────────────────────────────
void PSG::traceDump() {
#if PSG_TRACE
    if (_trCount != PSG_TRACE_N) return;
    _trCount = PSG_TRACE_N + 1;                    // non ripetere
    Serial.printf("[PSGTRACE] init: %d write (dt in cycles Z80, 1 frame = 59736)\n",
                  PSG_TRACE_N);
    uint32_t prev = _trStamp[0];
    uint8_t  lch = 1; bool lvol = true;
    for (int i = 0; i < PSG_TRACE_N; i++) {
        uint8_t v = _trVal[i];
        uint32_t dt = _trStamp[i] - prev;
        prev = _trStamp[i];
        if (v & 0x80) {
            lch = (v >> 5) & 3; lvol = (v >> 4) & 1;
            Serial.printf("[PSGTRACE] +%6lu  %02X  latch ch%u %s %X\n",
                          (unsigned long)dt, v, lch, lvol ? "VOL " : (lch == 3 ? "NOIS" : "TONE-lo"), v & 15);
        } else {
            Serial.printf("[PSGTRACE] +%6lu  %02X  data  -> ch%u %s %X\n",
                          (unsigned long)dt, v, lch, lvol ? "VOL " : (lch == 3 ? "NOIS" : "TONE-hi"),
                          lvol ? (v & 15) : (v & 0x3F));
        }
    }
    Serial.println("[PSGTRACE] end");
#endif
}