/*
 * Questo file fa parte del progetto PiSMS.
 *
 * Copyright (C) 2026 Ivan Vettori
 *
 * Questo file è opera originale dell'autore e viene rilasciato
 * sotto licenza GNU General Public License v3.0.
 * Puoi ridistribuirlo e/o modificarlo secondo i termini della GPLv3.
 *
 * Questo programma è distribuito nella speranza che sia utile,
 * ma SENZA ALCUNA GARANZIA; senza neppure la garanzia implicita
 * di COMMERCIABILITÀ o IDONEITÀ PER UN PARTICOLARE SCOPO.
 * Vedi la licenza GPLv3 per maggiori dettagli.
 */

#ifndef Z80_H
#define Z80_H

#define RAMFUNCZ80 __attribute__((section(".time_critical.")))

#include <Arduino.h>

typedef uint8_t (*Z80_ReadCallback)(uint16_t address);
typedef void    (*Z80_WriteCallback)(uint16_t address, uint8_t value);
typedef uint8_t (*Z80_IOReadCallback)(uint8_t port);
typedef void    (*Z80_IOWriteCallback)(uint8_t port, uint8_t value);

// Flag register bits
#define FLAG_C  0x01
#define FLAG_N  0x02
#define FLAG_PV 0x04
#define FLAG_X  0x08
#define FLAG_H  0x10
#define FLAG_Y  0x20
#define FLAG_Z  0x40
#define FLAG_S  0x80

class Z80 {
public:
    Z80();

    void begin(Z80_ReadCallback readMem, Z80_WriteCallback writeMem,
               Z80_IOReadCallback readIO, Z80_IOWriteCallback writeIO);
    void reset();

    int  execute(int cycles);
    void executeOne();
    int  step();

    void triggerNMI();
    void triggerINT(uint8_t vector = 0xFF);
    void setInterruptMode(uint8_t mode);

    inline uint16_t getPC()  const { return PC; }
    inline uint16_t getSP()  const { return SP; }
    inline uint8_t  getA()   const { return A; }
    inline uint8_t  getF()   const { return F; }
    inline uint16_t getAF()  const { return (A << 8) | F; }
    inline uint16_t getBC()  const { return (B << 8) | C; }
    inline uint16_t getDE()  const { return (D << 8) | E; }
    inline uint16_t getHL()  const { return (H << 8) | L; }
    inline uint16_t getIX()  const { return IX; }
    inline uint16_t getIY()  const { return IY; }
    inline uint8_t  getIM()  const { return IM; }

    void setPC(uint16_t v)  { PC = v; }
    void setSP(uint16_t v)  { SP = v; }
    void setAF(uint16_t v)  { A = v >> 8; F = v & 0xFF; }
    void setBC(uint16_t v)  { B = v >> 8; C = v & 0xFF; }
    void setDE(uint16_t v)  { D = v >> 8; E = v & 0xFF; }
    void setHL(uint16_t v)  { H = v >> 8; L = v & 0xFF; }

    bool IFF1, IFF2;
    bool halted;
    uint32_t totalCycles;

    uint8_t  IM;
    bool     pendingNMI;
    bool     pendingINT;
    bool     pendingEI;
    uint8_t  intVector;

    void setPages(const uint8_t* const* rd, uint8_t* const* wr) { _rd = rd; _wr = wr; }

private:
    uint8_t  A, F;
    uint8_t  B, C, D, E, H, L;
    uint16_t PC, SP;
    uint16_t IX, IY;
    uint8_t  I, R;

    uint8_t A_, F_, B_, C_, D_, E_, H_, L_;

    // ── OTTIMIZZAZIONE: tabella puntatori registri condivisa ──────
    // Sostituisce gli array locali "uint8_t* reg[] = {&B,&C,...}"
    // che venivano ricostruiti sullo stack ad ogni LD r,r' / INC r /
    // DEC r / ALU A,r / CB non-(HL) / IN-OUT (C),r — pattern fra i
    // più frequenti nel codice Z80 reale. Inizializzata una sola
    // volta in initRegPtr() (chiamata dal costruttore): gli indirizzi
    // dei campi membro non cambiano per tutta la vita dell'oggetto.
    // Indice 6 (nullptr) corrisponde a (HL), gestito separatamente
    // nei punti di decodifica che lo richiedono.
    uint8_t* _regPtr[8];
    inline void initRegPtr() {
        _regPtr[0]=&B; _regPtr[1]=&C; _regPtr[2]=&D; _regPtr[3]=&E;
        _regPtr[4]=&H; _regPtr[5]=&L; _regPtr[6]=nullptr; _regPtr[7]=&A;
    }

    Z80_ReadCallback    readMem;
    Z80_WriteCallback   writeMem;
    Z80_IOReadCallback  readIO;
    Z80_IOWriteCallback writeIO;

    static uint8_t parityTable[256];
    static uint8_t sz53Table[256];

    const uint8_t* const* _rd = nullptr;   // 64 pagine da 1 KB, fornite da SMSLoader
    uint8_t* const*       _wr = nullptr;   // nullptr = percorso lento (callback)

    inline uint8_t readByte(uint16_t a) {
        return _rd[a >> 10][a & 0x3FF];
    }
    inline void writeByte(uint16_t a, uint8_t v) {
        uint8_t* p = _wr[a >> 10];
        if (__builtin_expect(p != nullptr, 1)) p[a & 0x3FF] = v;
        else                                   writeMem(a, v);
    }
    // Lettura operando a 16 bit con ordine di valutazione GARANTITO
    inline uint16_t fetch16() {
        uint8_t lo = readByte(PC++);
        uint8_t hi = readByte(PC++);
        return lo | (hi << 8);
    }

    inline uint16_t readWord(uint16_t addr) {
        return readByte(addr) | (readByte(addr + 1) << 8);
    }
    inline void writeWord(uint16_t addr, uint16_t value) {
        writeByte(addr,     value & 0xFF);
        writeByte(addr + 1, value >> 8);
    }

    inline void     push(uint16_t v) { SP -= 2; writeWord(SP, v); }
    inline uint16_t pop()            { uint16_t v = readWord(SP); SP += 2; return v; }

    inline uint8_t inPort(uint8_t port)               { return readIO  ? readIO(port)        : 0xFF; }
    inline void    outPort(uint8_t port, uint8_t val)  { if (writeIO) writeIO(port, val); }

    inline void setFlag(uint8_t f)        { F |=  f; }
    inline void clearFlag(uint8_t f)      { F &= ~f; }
    inline bool getFlag(uint8_t f) const  { return (F & f) != 0; }

    // ── OTTIMIZZAZIONE: ALU 8-bit ora inline nell'header ──────────
    // Prima erano dichiarate qui e definite in Z80.cpp: senza LTO
    // il compilatore non poteva inlinearle attraverso il confine delle translation
    // unit. Sono fra le funzioni più chiamate dell'intero emulatore
    // (ogni istruzione ALU A,r / A,n) — spostarle qui rende
    // l'inlining sempre possibile, eliminando l'overhead di
    // call/return su funzioni di poche istruzioni ciascuna.
    inline void add8(uint8_t value) {
        uint16_t result = A + value;
        F = ((A ^ value ^ result) & FLAG_H) |
            ((result >> 8) & FLAG_C) |
            sz53Table[result & 0xFF] |
            (((A ^ value ^ 0x80) & (value ^ result) & 0x80) >> 5);
        A = result & 0xFF;
    }

    inline void adc8(uint8_t value) {
        uint16_t result = A + value + (F & FLAG_C);
        F = ((A ^ value ^ result) & FLAG_H) |
            ((result >> 8) & FLAG_C) |
            sz53Table[result & 0xFF] |
            (((A ^ value ^ 0x80) & (value ^ result) & 0x80) >> 5);
        A = result & 0xFF;
    }

    inline void sub8(uint8_t value) {
        uint16_t result = A - value;
        F = FLAG_N |
            ((A ^ value ^ result) & FLAG_H) |
            ((result >> 8) & FLAG_C) |
            sz53Table[result & 0xFF] |
            (((A ^ value) & (A ^ result) & 0x80) >> 5);
        A = result & 0xFF;
    }

    inline void sbc8(uint8_t value) {
        uint16_t result = A - value - (F & FLAG_C);
        F = FLAG_N |
            ((A ^ value ^ result) & FLAG_H) |
            ((result >> 8) & FLAG_C) |
            sz53Table[result & 0xFF] |
            (((A ^ value) & (A ^ result) & 0x80) >> 5);
        A = result & 0xFF;
    }

    inline void and8(uint8_t value) {
        A &= value;
        F = FLAG_H | sz53Table[A] | parityTable[A];
    }

    inline void or8(uint8_t value) {
        A |= value;
        F = sz53Table[A] | parityTable[A];
    }

    inline void xor8(uint8_t value) {
        A ^= value;
        F = sz53Table[A] | parityTable[A];
    }

    inline void cp8(uint8_t value) {
        uint16_t result = A - value;
        F = FLAG_N |
            ((A ^ value ^ result) & FLAG_H) |
            ((result >> 8) & FLAG_C) |
            (sz53Table[result & 0xFF] & (FLAG_S | FLAG_Z)) |
            (value & (FLAG_Y | FLAG_X)) |
            (((A ^ value) & (A ^ result) & 0x80) >> 5);
    }

    // ── ALU 16-bit, ora inline ─────────────────────────────────
    inline void add16(uint16_t* hl, uint16_t val) {
        uint32_t res = (uint32_t)(*hl) + val;
        uint8_t  hflag = (((*hl & 0x0FFF) + (val & 0x0FFF)) & 0x1000) ? FLAG_H : 0;

        // Preserva S, Z e P/V inalterati!
        F = (F & (FLAG_S | FLAG_Z | FLAG_PV)) |
            ((res & 0x10000) ? FLAG_C : 0) |
            ((res >> 8) & (FLAG_Y | FLAG_X)) |
            hflag;

        *hl = (uint16_t)res;
    }

    inline void adc16(uint16_t val) {
        uint16_t hl = (H << 8) | L;
        uint8_t  c  = (F & FLAG_C);
        uint32_t res = (uint32_t)hl + val + c;

        // Half-carry: riporto dal bit 11 al bit 12
        uint8_t hflag = (((hl & 0x0FFF) + (val & 0x0FFF) + c) & 0x1000) ? FLAG_H : 0;

        // Overflow: (~(hl ^ val)) & (hl ^ res) & 0x8000, shiftato di 13 bit -> bit 2 (FLAG_PV)
        uint8_t vflag = (((~(hl ^ val)) & (hl ^ (uint16_t)res) & 0x8000) >> 13);

        F = ((res & 0x10000) ? FLAG_C : 0) |
            (((res & 0xFFFF) == 0) ? FLAG_Z : 0) |       // FIX 1: Zero sui 16 bit
            ((res >> 8) & (FLAG_S | FLAG_Y | FLAG_X)) |
            hflag |
            vflag;                                       // FIX 2: Overflow su bit 2 (>> 13)

        H = (uint8_t)(res >> 8);
        L = (uint8_t)(res & 0xFF);
    }

    inline void sbc16(uint16_t val) {
        uint16_t hl = (H << 8) | L;
        uint8_t  c  = (F & FLAG_C);
        uint32_t res = (uint32_t)hl - val - c;

        // Half-carry: prestito dal bit 11 al bit 12
        uint8_t hflag = (((hl & 0x0FFF) - (val & 0x0FFF) - c) & 0x1000) ? FLAG_H : 0;
        
        // Overflow: (hl ^ val) & (hl ^ res) & 0x8000, shiftato di 13 bit -> bit 2 (FLAG_PV)
        uint8_t vflag = (((hl ^ val) & (hl ^ (uint16_t)res) & 0x8000) >> 13);

        F = FLAG_N |
            ((res & 0x10000) ? FLAG_C : 0) |
            (((res & 0xFFFF) == 0) ? FLAG_Z : 0) |       // FIX 1: Zero sui 16 bit
            ((res >> 8) & (FLAG_S | FLAG_Y | FLAG_X)) |
            hflag |
            vflag;                                       // FIX 2: Overflow su bit 2 (>> 13)

        H = (uint8_t)(res >> 8);
        L = (uint8_t)(res & 0xFF);
    }

    // ── INC/DEC 8-bit, ora inline ──────────────────────────────
    inline uint8_t inc8(uint8_t value) {
        value++;
        F = (F & FLAG_C) |
            sz53Table[value] |
            ((value == 0x80) ? FLAG_PV : 0) |
            ((value & 0x0F) == 0 ? FLAG_H : 0);
        return value;
    }

    inline uint8_t dec8(uint8_t value) {
        F = (F & FLAG_C) | FLAG_N | ((value & 0x0F) == 0 ? FLAG_H : 0);
        value--;
        F |= sz53Table[value] | ((value == 0x7F) ? FLAG_PV : 0);
        return value;
    }

    // ── Rotate/shift, ora inline ───────────────────────────────
    inline uint8_t rlc(uint8_t v) {
        uint8_t c = v >> 7;
        v = (v << 1) | c;
        F = sz53Table[v] | parityTable[v] | c;
        return v;
    }

    inline uint8_t rrc(uint8_t v) {
        uint8_t c = v & 1;
        v = (v >> 1) | (c << 7);
        F = sz53Table[v] | parityTable[v] | c;
        return v;
    }

    inline uint8_t rl(uint8_t v) {
        uint8_t oc = F & FLAG_C;
        uint8_t c  = v >> 7;
        v = (v << 1) | oc;
        F = sz53Table[v] | parityTable[v] | c;
        return v;
    }

    inline uint8_t rr(uint8_t v) {
        uint8_t oc = F & FLAG_C;
        uint8_t c  = v & 1;
        v = (v >> 1) | (oc << 7);
        F = sz53Table[v] | parityTable[v] | c;
        return v;
    }

    inline uint8_t sla(uint8_t v) {
        uint8_t c = v >> 7;
        v <<= 1;
        F = sz53Table[v] | parityTable[v] | c;
        return v;
    }

    inline uint8_t sra(uint8_t v) {
        uint8_t c = v & 1;
        v = (v >> 1) | (v & 0x80);
        F = sz53Table[v] | parityTable[v] | c;
        return v;
    }

    inline uint8_t sll(uint8_t v) {
        uint8_t c = v >> 7;
        v = (v << 1) | 1;
        F = sz53Table[v] | parityTable[v] | c;
        return v;
    }

    inline uint8_t srl(uint8_t v) {
        uint8_t c = v & 1;
        v >>= 1;
        F = sz53Table[v] | parityTable[v] | c;
        return v;
    }

    int exec(uint8_t opcode);
    int handleCB();
    int handleED();
    int handleDD();
    int handleFD();

    int executeCB(uint8_t op2);
    int executeED(uint8_t op2);
    int executeDDFD(bool isIX, uint8_t op2);

    int executeDD() { return handleDD(); }
    int executeFD() { return handleFD(); }

    int executeDDFDCB(bool isIX, int8_t offset, uint8_t op2);
    int executeInstruction();

    inline bool checkCondition(uint8_t cc) {
        switch (cc) {
            case 0: return !getFlag(FLAG_Z);
            case 1: return  getFlag(FLAG_Z);
            case 2: return !getFlag(FLAG_C);
            case 3: return  getFlag(FLAG_C);
            case 4: return !getFlag(FLAG_PV);
            case 5: return  getFlag(FLAG_PV);
            case 6: return !getFlag(FLAG_S);
            case 7: return  getFlag(FLAG_S);
            default: return false;
        }
    }
};

#endif // Z80_H