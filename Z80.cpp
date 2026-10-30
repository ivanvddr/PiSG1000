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
 
#include "Z80.h"

// ============================================================================
// TABELLE LOOKUP
// ============================================================================
uint8_t Z80::parityTable[256] = {
    4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,
    0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,
    0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,
    4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,
    0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,
    4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,
    4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,
    0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,
    0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,
    4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,
    4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,
    0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,
    4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4,
    0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,
    0,4,4,0,4,0,0,4,4,0,0,4,0,4,4,0,
    4,0,0,4,0,4,4,0,0,4,4,0,4,0,0,4
};

uint8_t Z80::sz53Table[256] = {
    0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
    0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28,
    0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
    0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28,
    0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x28,0x28,0x28,0x28,0x28,0x28,0x28,0x28,
    0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88,
    0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88,
    0xA0,0xA0,0xA0,0xA0,0xA0,0xA0,0xA0,0xA0,0xA8,0xA8,0xA8,0xA8,0xA8,0xA8,0xA8,0xA8,
    0xA0,0xA0,0xA0,0xA0,0xA0,0xA0,0xA0,0xA0,0xA8,0xA8,0xA8,0xA8,0xA8,0xA8,0xA8,0xA8,
    0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88,
    0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x88,0x88,0x88,0x88,0x88,0x88,0x88,0x88,
    0xA0,0xA0,0xA0,0xA0,0xA0,0xA0,0xA0,0xA0,0xA8,0xA8,0xA8,0xA8,0xA8,0xA8,0xA8,0xA8,
    0xA0,0xA0,0xA0,0xA0,0xA0,0xA0,0xA0,0xA0,0xA8,0xA8,0xA8,0xA8,0xA8,0xA8,0xA8,0xA8
};

// ============================================================================
// COSTRUTTORE / INIT / RESET
// ============================================================================
Z80::Z80() {
    readMem  = nullptr;
    writeMem = nullptr;
    readIO   = nullptr;
    writeIO  = nullptr;
    initRegPtr();   // OTTIMIZZAZIONE: tabella puntatori inizializzata una
                     // sola volta qui, mai più ricostruita nel decoder
    reset();
}

void Z80::begin(Z80_ReadCallback readMemCb, Z80_WriteCallback writeMemCb,
                Z80_IOReadCallback readIOCb, Z80_IOWriteCallback writeIOCb) {
    readMem  = readMemCb;
    writeMem = writeMemCb;
    readIO   = readIOCb;
    writeIO  = writeIOCb;
}

void Z80::reset() {
    A = F = B = C = D = E = H = L = 0;
    A_= F_= B_= C_= D_= E_= H_= L_= 0;
    PC = 0;
    SP = 0xFFFF;
    IX = IY = 0;
    I  = R  = 0;
    IFF1 = IFF2 = false;
    IM          = 0;
    halted      = false;
    pendingNMI  = false;
    pendingINT  = false;
    pendingEI   = false;
    intVector   = 0xFF;
    totalCycles = 0;
}

// ============================================================================
// INTERRUPT
// ============================================================================

void Z80::triggerNMI() {
    pendingNMI = true;
    halted     = false;
}

void Z80::triggerINT(uint8_t vector) {
    pendingINT = true;
    intVector  = vector;
}

void Z80::setInterruptMode(uint8_t mode) {
    IM = mode & 0x03;
}

// ============================================================================
// EXECUTE (N cicli)
// ============================================================================
int Z80::execute(int cycles) {
    int executed = 0;
    while (executed < cycles) {
        if (pendingEI) {
            pendingEI = false;
            IFF1 = IFF2 = true;
            if (!halted) {
                int c = executeInstruction();
                executed    += c;
                totalCycles += c;
            } else {
                executed += 4;
                R = (R & 0x80) | ((R + 1) & 0x7F);
            }
            continue;
        }
        if (pendingNMI) {
            pendingNMI = false;
            IFF2   = IFF1;
            IFF1   = false;
            halted = false;
            push(PC);
            PC = 0x0066;
            executed += 11;
            R = (R & 0x80) | ((R + 1) & 0x7F);
            continue;
        }
        if (pendingINT && IFF1) {
            pendingINT = false;
            IFF2 = IFF1;
            IFF1       = false;
            halted     = false;
            push(PC);
            switch (IM) {
                case 0:
                case 1:
                    PC = 0x0038;
                    executed += 13;
                    break;
                case 2: {
                    uint16_t addr = ((uint16_t)I << 8) | intVector;
                    PC = readWord(addr);
                    executed += 19;
                    break;
                }
            }
            R = (R & 0x80) | ((R + 1) & 0x7F);
            continue;
        }
        if (halted) {
            executed += 4;
            R = (R & 0x80) | ((R + 1) & 0x7F);
            continue;
        }
        int c = executeInstruction();
        executed    += c;
        totalCycles += c;
    }
    return executed;
}

void Z80::executeOne() {
    int c = executeInstruction();
    totalCycles += c;
}

RAMFUNCZ80 int Z80::step() {
    if (__builtin_expect(!pendingEI & !pendingNMI & !pendingINT & !halted, 1)) {
        const int c = executeInstruction();
        totalCycles += c;
        return c;
    }
    if (pendingEI) {
        pendingEI = false;
        IFF1 = IFF2 = true;
        if (!halted) {
            int c = executeInstruction();
            totalCycles += c;
            return c;
        } else {
            totalCycles += 4;
            R = (R & 0x80) | ((R + 1) & 0x7F);
            return 4;
        }
    }
    if (pendingNMI) {
        pendingNMI = false;
        IFF2   = IFF1;
        IFF1   = false;
        halted = false;
        push(PC);
        PC = 0x0066;
        totalCycles += 11;
        R = (R & 0x80) | ((R + 1) & 0x7F);
        return 11;
    }
    if (pendingINT && IFF1) {
        pendingINT = false;
        IFF2 = IFF1;
        IFF1       = false;
        halted     = false;
        push(PC);
        int cost = 13;
        switch (IM) {
            case 0:
            case 1:
                PC = 0x0038;
                cost = 13;
                break;
            case 2: {
                uint16_t addr = ((uint16_t)I << 8) | intVector;
                PC   = readWord(addr);
                cost = 19;
                break;
            }
        }
        totalCycles += cost;
        R = (R & 0x80) | ((R + 1) & 0x7F);
        return cost;
    }
    if (halted) {
        totalCycles += 4;
        R = (R & 0x80) | ((R + 1) & 0x7F);
        return 4;
    }
    int c = executeInstruction();
    totalCycles += c;
    return c;
}



// ============================================================================
// DECODER PRINCIPALE - CON SEPARAZIONE FETCH/EXEC
// ============================================================================
RAMFUNCZ80 int Z80::executeInstruction() {
    uint8_t opcode = readByte(PC++);
    R = (R & 0x80) | ((R + 1) & 0x7F);
    return exec(opcode);
}

RAMFUNCZ80 int Z80::exec(uint8_t opcode) {
    uint8_t x = opcode >> 6;
    uint8_t y = (opcode >> 3) & 0x07;
    uint8_t z = opcode & 0x07;
    uint8_t p = y >> 1;
    uint8_t q = y & 1;

    switch (x) {
        case 0:
            switch (z) {
                case 0:
                    switch (y) {
                        case 0: return 4;   // NOP
                        case 1: {           // EX AF,AF'
                            uint8_t t; t=A; A=A_; A_=t; t=F; F=F_; F_=t;
                            return 4;
                        }
                        case 2: {           // DJNZ d
                            int8_t offset = (int8_t)readByte(PC++);
                            if (--B != 0) { PC += offset; return 13; }
                            return 8;
                        }
                        case 3: {           // JR d
                            int8_t offset = (int8_t)readByte(PC++);
                            PC += offset;
                            return 12;
                        }
                        default: {          // JR cc,d
                            int8_t offset = (int8_t)readByte(PC++);
                            if (checkCondition(y - 4)) { PC += offset; return 12; }
                            return 7;
                        }
                    }
                case 1:
                    if (q == 0) {           // LD rp,nn
                        uint16_t nn = fetch16();
                        switch (p) {
                            case 0: B=nn>>8; C=nn&0xFF; break;
                            case 1: D=nn>>8; E=nn&0xFF; break;
                            case 2: H=nn>>8; L=nn&0xFF; break;
                            case 3: SP=nn; break;
                        }
                        return 10;
                    } else {                // ADD HL,rp
                        uint16_t hl = (H<<8)|L, rp;
                        switch (p) {
                            case 0: rp=(B<<8)|C; break;
                            case 1: rp=(D<<8)|E; break;
                            case 2: rp=(H<<8)|L; break;
                            default: rp=SP; break;
                        }
                        add16(&hl, rp);
                        H=hl>>8; L=hl&0xFF;
                        return 11;
                    }
                case 2:
                    switch (y) {
                        case 0: writeByte((B<<8)|C, A); return 7;
                        case 1: A=readByte((B<<8)|C); return 7;
                        case 2: writeByte((D<<8)|E, A); return 7;
                        case 3: A=readByte((D<<8)|E); return 7;
                        case 4: { uint16_t a=fetch16(); writeWord(a,(H<<8)|L); return 16; }
                        case 5: { uint16_t a=fetch16(); uint16_t v=readWord(a); H=v>>8; L=v&0xFF; return 16; }
                        case 6: { uint16_t a=fetch16(); writeByte(a,A); return 13; }
                        case 7: { uint16_t a=fetch16(); A=readByte(a); return 13; }
                    }
                    break;
                case 3:
                    if (q == 0) {           // INC rp
                        switch (p) {
                            case 0: { uint16_t v=((B<<8)|C)+1; B=v>>8; C=v&0xFF; break; }
                            case 1: { uint16_t v=((D<<8)|E)+1; D=v>>8; E=v&0xFF; break; }
                            case 2: { uint16_t v=((H<<8)|L)+1; H=v>>8; L=v&0xFF; break; }
                            case 3: SP++; break;
                        }
                        return 6;
                    } else {                // DEC rp
                        switch (p) {
                            case 0: { uint16_t v=((B<<8)|C)-1; B=v>>8; C=v&0xFF; break; }
                            case 1: { uint16_t v=((D<<8)|E)-1; D=v>>8; E=v&0xFF; break; }
                            case 2: { uint16_t v=((H<<8)|L)-1; H=v>>8; L=v&0xFF; break; }
                            case 3: SP--; break;
                        }
                        return 6;
                    }
                case 4: {                   // INC r
                    // OTTIMIZZAZIONE: _regPtr condiviso, non più
                    // ricostruito ad ogni chiamata (era qui un array locale identico)
                    if (y == 6) { uint16_t a=(H<<8)|L; writeByte(a,inc8(readByte(a))); return 11; }
                    *_regPtr[y] = inc8(*_regPtr[y]); return 4;
                }
                case 5: {                   // DEC r
                    if (y == 6) { uint16_t a=(H<<8)|L; writeByte(a,dec8(readByte(a))); return 11; }
                    *_regPtr[y] = dec8(*_regPtr[y]); return 4;
                }
                case 6: {                   // LD r,n
                    uint8_t n = readByte(PC++);
                    if (y == 6) { writeByte((H<<8)|L, n); return 10; }
                    *_regPtr[y] = n; return 7;
                }
                case 7:
                    switch (y) {
                        case 0: { // RLCA
                            uint8_t c=A>>7; A=(A<<1)|c;
                            F=(F&(FLAG_S|FLAG_Z|FLAG_PV))|(A&(FLAG_Y|FLAG_X))|c;
                            return 4;
                        }
                        case 1: { // RRCA
                            uint8_t c=A&1; A=(A>>1)|(c<<7);
                            F=(F&(FLAG_S|FLAG_Z|FLAG_PV))|(A&(FLAG_Y|FLAG_X))|c;
                            return 4;
                        }
                        case 2: { // RLA
                            uint8_t oc=F&FLAG_C, c=A>>7; A=(A<<1)|oc;
                            F=(F&(FLAG_S|FLAG_Z|FLAG_PV))|(A&(FLAG_Y|FLAG_X))|c;
                            return 4;
                        }
                        case 3: { // RRA
                            uint8_t oc=F&FLAG_C, c=A&1; A=(A>>1)|(oc<<7);
                            F=(F&(FLAG_S|FLAG_Z|FLAG_PV))|(A&(FLAG_Y|FLAG_X))|c;
                            return 4;
                        }
                        case 4: { // DAA
                            uint8_t corr = 0;
                            uint8_t cflag = 0;
                            if (F & FLAG_N) {
                                if ((F & FLAG_H) || (A & 0x0F) > 9)  corr |= 0x06;
                                if ((F & FLAG_C) || A > 0x99)        { corr |= 0x60; cflag = FLAG_C; }
                                A -= corr;
                            } else {
                                if ((F & FLAG_H) || (A & 0x0F) > 9)  corr |= 0x06;
                                if ((F & FLAG_C) || A > 0x99)        { corr |= 0x60; cflag = FLAG_C; }
                                A += corr;
                            }
                            F = (F & FLAG_N) | cflag | sz53Table[A] | parityTable[A] |
                                ((corr & 0x06) ? FLAG_H : 0);
                            return 4;
                        }
                        case 5: { // CPL
                            A = ~A;
                            F = (F & (FLAG_C|FLAG_PV|FLAG_Z|FLAG_S)) | FLAG_H | FLAG_N | (A & (FLAG_Y|FLAG_X));
                            return 4;
                        }
                        case 6: { // SCF
                            F = (F & (FLAG_PV|FLAG_Z|FLAG_S)) | FLAG_C | (A & (FLAG_Y|FLAG_X));
                            return 4;
                        }
                        case 7: { // CCF
                            F = (F & (FLAG_PV|FLAG_Z|FLAG_S)) |
                                ((F & FLAG_C) ? FLAG_H : FLAG_C) |
                                (A & (FLAG_Y|FLAG_X));
                            return 4;
                        }
                    }
            }
            break;

        case 1:                             // LD r,r' / HALT
            if (y==6 && z==6) { halted=true; return 4; }
            // OTTIMIZZAZIONE: _regPtr condiviso invece di due array
            // locali "dst[]"/"src[]" ricostruiti ad ogni LD r,r'
            if      (y==6) { writeByte((H<<8)|L, *_regPtr[z]); return 7; }
            else if (z==6) { *_regPtr[y]=readByte((H<<8)|L); return 7; }
            else           { *_regPtr[y]=*_regPtr[z]; return 4; }

        case 2: {                           // ALU A,r / ALU A,(HL)
            uint8_t val; int cy=4;
            if (z==6) { val=readByte((H<<8)|L); cy=7; }
            else { val=*_regPtr[z]; }
            switch (y) {
                case 0: add8(val); break; case 1: adc8(val); break;
                case 2: sub8(val); break; case 3: sbc8(val); break;
                case 4: and8(val); break; case 5: xor8(val); break;
                case 6: or8(val);  break; case 7: cp8(val);  break;
            }
            return cy;
        }

        case 3:
            switch (z) {
                case 0:                     // RET cc
                    if (checkCondition(y)) { PC=pop(); return 11; }
                    return 5;

                case 1:
                    if (q==0) {             // POP rp2
                        uint16_t v=pop();
                        switch (p) {
                            case 0: B=v>>8; C=v&0xFF; break;
                            case 1: D=v>>8; E=v&0xFF; break;
                            case 2: H=v>>8; L=v&0xFF; break;
                            case 3: A=v>>8; F=v&0xFF; break;
                        }
                        return 10;
                    } else {
                        switch (p) {
                            case 0: PC=pop(); return 10;   // RET
                            case 1: {           // EXX
                                uint8_t t;
                                t=B;B=B_;B_=t; t=C;C=C_;C_=t;
                                t=D;D=D_;D_=t; t=E;E=E_;E_=t;
                                t=H;H=H_;H_=t; t=L;L=L_;L_=t;
                                return 4;
                            }
                            case 2: PC=(H<<8)|L; return 4;  // JP HL
                            case 3: SP=(H<<8)|L; return 6;  // LD SP,HL
                        }
                    }
                    break;

                case 2: {                   // JP cc,nn
                    uint16_t nn=fetch16();
                    if (checkCondition(y)) PC=nn;
                    return 10;
                }

                case 3:
                    switch (y) {
                        case 0: { uint16_t nn=fetch16(); PC=nn; return 10; }
                        case 1: return handleCB();
                        case 2: { uint8_t port=readByte(PC++); outPort(port,A); return 11; }
                        case 3: { uint8_t port=readByte(PC++); A=inPort(port);  return 11; }
                        case 4: { uint16_t t=readWord(SP); writeWord(SP,(H<<8)|L); H=t>>8; L=t&0xFF; return 19; } // 19 cicli per EX (SP),HL
                        case 5: { uint8_t t; t=D;D=H;H=t; t=E;E=L;L=t; return 4; }
                        case 6: IFF1=IFF2=false; return 4;  // DI
                        case 7: pendingEI=true;  return 4;  // EI
                    }
                    break;

                case 4: {                   // CALL cc,nn
                    uint16_t nn=fetch16();
                    if (checkCondition(y)) { push(PC); PC=nn; return 17; }
                    return 10;
                }

                case 5:
                    if (q==0) {             // PUSH rp2
                        uint16_t v;
                        switch (p) {
                            case 0: v=(B<<8)|C; break; case 1: v=(D<<8)|E; break;
                            case 2: v=(H<<8)|L; break; default: v=(A<<8)|F; break;
                        }
                        push(v); return 11;
                    } else {
                        switch (p) {
                            case 0: { uint16_t nn=fetch16(); push(PC); PC=nn; return 17; }
                            case 1: return handleDD();
                            case 2: return handleED();
                            case 3: return handleFD();
                        }
                    }
                    break;

                case 6: {                   // ALU A,n
                    uint8_t n=readByte(PC++);
                    switch (y) {
                        case 0: add8(n); break; case 1: adc8(n); break;
                        case 2: sub8(n); break; case 3: sbc8(n); break;
                        case 4: and8(n); break; case 5: xor8(n); break;
                        case 6: or8(n);  break; case 7: cp8(n);  break;
                    }
                    return 7;
                }

                case 7:                     // RST
                    push(PC); PC=y*8; return 11;
            }
    }
    return 4;
}

// ============================================================================
// HANDLER PER PREFISSI (leggono il secondo byte e incrementano R)
// ============================================================================

RAMFUNCZ80 int Z80::handleCB() {
    uint8_t op2 = readByte(PC++);
    R = (R & 0x80) | ((R + 1) & 0x7F);
    return executeCB(op2);
}

RAMFUNCZ80 int Z80::handleED() {
    uint8_t op2 = readByte(PC++);
    R = (R & 0x80) | ((R + 1) & 0x7F);
    return executeED(op2);
}

RAMFUNCZ80 int Z80::handleDD() {
    uint8_t op2 = readByte(PC++);
    R = (R & 0x80) | ((R + 1) & 0x7F);
    return executeDDFD(true, op2);
}

RAMFUNCZ80 int Z80::handleFD() {
    uint8_t op2 = readByte(PC++);
    R = (R & 0x80) | ((R + 1) & 0x7F);
    return executeDDFD(false, op2);
}

// ============================================================================
// PREFIX CB (riceve l'opcode secondario)
// ============================================================================

RAMFUNCZ80 int Z80::executeCB(uint8_t op) {
    uint8_t x = op >> 6;
    uint8_t y = (op >> 3) & 7;
    uint8_t z = op & 7;

    // OTTIMIZZAZIONE: _regPtr condiviso invece dell'array locale
    // "uint8_t* reg[] = {&B,&C,...}" ricostruito ad ogni CB-prefixed
    // su registro (rotazioni/shift/BIT/RES/SET non-(HL))

    if (z == 6) {
        uint16_t addr = (H<<8)|L;
        uint8_t  val  = readByte(addr);
        switch (x) {
            case 0:
                switch (y) {
                    case 0: val=rlc(val); break; case 1: val=rrc(val); break;
                    case 2: val=rl(val);  break; case 3: val=rr(val);  break;
                    case 4: val=sla(val); break; case 5: val=sra(val); break;
                    case 6: val=sll(val); break; case 7: val=srl(val); break;
                }
                writeByte(addr, val); return 15;
            case 1: {
                uint8_t masked = val & (1 << y);
                F = (F & FLAG_C) | FLAG_H |
                    (masked ? 0 : FLAG_Z | FLAG_PV) |
                    (val & (FLAG_Y | FLAG_X)) |
                    (masked & FLAG_S);
                return 12;
            }
            case 2: writeByte(addr, val & ~(1<<y)); return 15;
            case 3: writeByte(addr, val |  (1<<y)); return 15;
        }
    } else {
        uint8_t* t = _regPtr[z];
        switch (x) {
            case 0:
                switch (y) {
                    case 0: *t=rlc(*t); break; case 1: *t=rrc(*t); break;
                    case 2: *t=rl(*t);  break; case 3: *t=rr(*t);  break;
                    case 4: *t=sla(*t); break; case 5: *t=sra(*t); break;
                    case 6: *t=sll(*t); break; case 7: *t=srl(*t); break;
                }
                return 8;
            case 1: {
                uint8_t masked = *t & (1 << y);
                F = (F & FLAG_C) | FLAG_H |
                    (masked ? 0 : FLAG_Z | FLAG_PV) |
                    (*t & (FLAG_Y | FLAG_X)) |
                    (masked & FLAG_S);
                return 8;
            }
            case 2: *t &= ~(1<<y); return 8;
            case 3: *t |=  (1<<y); return 8;
        }
    }
    return 8;
}

// ============================================================================
// PREFIX ED (riceve l'opcode secondario)
// ============================================================================

RAMFUNCZ80 int Z80::executeED(uint8_t op) {
    uint8_t x = op >> 6;
    uint8_t y = (op >> 3) & 7;
    uint8_t z = op & 7;
    uint8_t p = y >> 1;

    if (x == 1) {
        switch (z) {
            case 0: {   // IN r,(C)
                // OTTIMIZZAZIONE: _regPtr condiviso
                uint8_t  val   = inPort(C);
                if (y != 6) *_regPtr[y] = val;
                F = (F & FLAG_C) | sz53Table[val] | parityTable[val];
                return 12;
            }
            case 1: {   // OUT (C),r
                outPort(C, y==6 ? 0 : *_regPtr[y]);
                return 12;
            }
            case 2: {   // SBC/ADC HL,rp
                uint16_t rp;
                switch (p) {
                    case 0: rp=(B<<8)|C; break; case 1: rp=(D<<8)|E; break;
                    case 2: rp=(H<<8)|L; break; default: rp=SP; break;
                }
                if (y & 1) adc16(rp); else sbc16(rp);
                return 15;
            }
            case 3: {   // LD (nn),rp / LD rp,(nn)
                uint16_t addr = fetch16();
                if (y & 1) {
                    uint16_t v = readWord(addr);
                    switch (p) {
                        case 0: B=v>>8; C=v&0xFF; break; case 1: D=v>>8; E=v&0xFF; break;
                        case 2: H=v>>8; L=v&0xFF; break; default: SP=v; break;
                    }
                } else {
                    uint16_t v;
                    switch (p) {
                        case 0: v=(B<<8)|C; break; case 1: v=(D<<8)|E; break;
                        case 2: v=(H<<8)|L; break; default: v=SP; break;
                    }
                    writeWord(addr, v);
                }
                return 20;
            }
            case 4:     // NEG
                { uint8_t a=A; A=0; sub8(a); return 8; }
            case 5: {   // RETN / RETI
                IFF1 = IFF2;
                PC = pop();
                return 14;
            }
            case 6: {   // IM 0/1/2
                static const uint8_t im_table[] = {0,0,1,2,0,0,1,2};
                IM = im_table[y]; return 8;
            }
            case 7:
                switch (y) {
                    case 0: I=A; return 9;
                    case 1: R=A; return 9;
                    case 2: A=I; F=(F&FLAG_C)|sz53Table[A]|(IFF2?FLAG_PV:0) | (A == 0 ? FLAG_Z : 0); return 9;
                    case 3: A=R; F=(F&FLAG_C)|sz53Table[A]|(IFF2?FLAG_PV:0) | (A == 0 ? FLAG_Z : 0); return 9;
                    case 4: {   // RRD
                        uint8_t mem=readByte((H<<8)|L), tmp=mem;
                        mem=(mem>>4)|(A<<4); A=(A&0xF0)|(tmp&0x0F);
                        writeByte((H<<8)|L,mem);
                        F=(F&FLAG_C)|sz53Table[A]|parityTable[A];
                        return 18;
                    }
                    case 5: {   // RLD
                        uint8_t mem=readByte((H<<8)|L), tmp=mem;
                        mem=(mem<<4)|(A&0x0F); A=(A&0xF0)|(tmp>>4);
                        writeByte((H<<8)|L,mem);
                        F=(F&FLAG_C)|sz53Table[A]|parityTable[A];
                        return 18;
                    }
                }
                break;
        }
    }

    if (x == 2) {
        switch (op) {
            case 0xA0: {    // LDI
                uint8_t v=readByte((H<<8)|L);
                writeByte((D<<8)|E,v);
                uint16_t hl=((H<<8)|L)+1, de=((D<<8)|E)+1, bc=((B<<8)|C)-1;
                H=hl>>8;L=hl&0xFF; D=de>>8;E=de&0xFF; B=bc>>8;C=bc&0xFF;
                F=(F&(FLAG_C|FLAG_Z|FLAG_S))|(bc?FLAG_PV:0);
                return 16;
            }
            case 0xA8: {    // LDD
                uint8_t v=readByte((H<<8)|L);
                writeByte((D<<8)|E,v);
                uint16_t hl=((H<<8)|L)-1, de=((D<<8)|E)-1, bc=((B<<8)|C)-1;
                H=hl>>8;L=hl&0xFF; D=de>>8;E=de&0xFF; B=bc>>8;C=bc&0xFF;
                F=(F&(FLAG_C|FLAG_Z|FLAG_S))|(bc?FLAG_PV:0);
                return 16;
            }
            case 0xB0: {    // LDIR
                uint8_t v=readByte((H<<8)|L);
                writeByte((D<<8)|E,v);
                uint16_t hl=((H<<8)|L)+1, de=((D<<8)|E)+1, bc=((B<<8)|C)-1;
                H=hl>>8;L=hl&0xFF; D=de>>8;E=de&0xFF; B=bc>>8;C=bc&0xFF;
                if (bc) { PC-=2; F=(F&(FLAG_C|FLAG_Z|FLAG_S))|FLAG_PV; return 21; }
                F=(F&(FLAG_C|FLAG_Z|FLAG_S));
                return 16;
            }
            case 0xB8: {    // LDDR
                uint8_t v=readByte((H<<8)|L);
                writeByte((D<<8)|E,v);
                uint16_t hl=((H<<8)|L)-1, de=((D<<8)|E)-1, bc=((B<<8)|C)-1;
                H=hl>>8;L=hl&0xFF; D=de>>8;E=de&0xFF; B=bc>>8;C=bc&0xFF;
                if (bc) { PC-=2; F=(F&(FLAG_C|FLAG_Z|FLAG_S))|FLAG_PV; return 21; }
                F=(F&(FLAG_C|FLAG_Z|FLAG_S));
                return 16;
            }
            case 0xA1: {    // CPI
                uint8_t v=readByte((H<<8)|L);
                uint8_t r=A-v;
                uint16_t hl=((H<<8)|L)+1, bc=((B<<8)|C)-1;
                H=hl>>8;L=hl&0xFF; B=bc>>8;C=bc&0xFF;
                F=(F&FLAG_C)|FLAG_N|sz53Table[r]|(bc?FLAG_PV:0)|((A^v^r)&FLAG_H);
                return 16;
            }
            case 0xA9: {    // CPD
                uint8_t v=readByte((H<<8)|L);
                uint8_t r=A-v;
                uint16_t hl=((H<<8)|L)-1, bc=((B<<8)|C)-1;
                H=hl>>8;L=hl&0xFF; B=bc>>8;C=bc&0xFF;
                F=(F&FLAG_C)|FLAG_N|sz53Table[r]|(bc?FLAG_PV:0)|((A^v^r)&FLAG_H);
                return 16;
            }
            case 0xB1: {    // CPIR
                uint8_t v=readByte((H<<8)|L);
                uint8_t r=A-v;
                uint16_t hl=((H<<8)|L)+1, bc=((B<<8)|C)-1;
                H=hl>>8;L=hl&0xFF; B=bc>>8;C=bc&0xFF;
                F=(F&FLAG_C)|FLAG_N|sz53Table[r]|(bc?FLAG_PV:0)|((A^v^r)&FLAG_H);
                if (bc && r!=0) { PC-=2; return 21; }
                return 16;
            }
            case 0xB9: {    // CPDR
                uint8_t v=readByte((H<<8)|L);
                uint8_t r=A-v;
                uint16_t hl=((H<<8)|L)-1, bc=((B<<8)|C)-1;
                H=hl>>8;L=hl&0xFF; B=bc>>8;C=bc&0xFF;
                F=(F&FLAG_C)|FLAG_N|sz53Table[r]|(bc?FLAG_PV:0)|((A^v^r)&FLAG_H);
                if (bc && r!=0) { PC-=2; return 21; }
                return 16;
            }
            case 0xA2: {    // INI
                uint8_t v=inPort(C);
                writeByte((H<<8)|L, v);
                uint16_t hl=((H<<8)|L)+1;
                H=hl>>8; L=hl&0xFF;
                B=dec8(B);
                F |= FLAG_N;
                return 16;
            }
            case 0xAA: {    // IND
                uint8_t v=inPort(C);
                writeByte((H<<8)|L, v);
                uint16_t hl=((H<<8)|L)-1;
                H=hl>>8; L=hl&0xFF;
                B=dec8(B);
                F |= FLAG_N;
                return 16;
            }
            case 0xB2: {    // INIR
                uint8_t v=inPort(C);
                writeByte((H<<8)|L, v);
                uint16_t hl=((H<<8)|L)+1;
                H=hl>>8; L=hl&0xFF;
                B=dec8(B);
                F |= FLAG_N;
                if (B != 0) { PC-=2; return 21; }
                return 16;
            }
            case 0xBA: {    // INDR
                uint8_t v=inPort(C);
                writeByte((H<<8)|L, v);
                uint16_t hl=((H<<8)|L)-1;
                H=hl>>8; L=hl&0xFF;
                B=dec8(B);
                F |= FLAG_N;
                if (B != 0) { PC-=2; return 21; }
                return 16;
            }
            case 0xA3: {    // OUTI
                uint8_t v=readByte((H<<8)|L);
                uint16_t hl=((H<<8)|L)+1;
                H=hl>>8; L=hl&0xFF;
                B=dec8(B);
                outPort(C, v);
                F |= FLAG_N;
                return 16;
            }
            case 0xAB: {    // OUTD
                uint8_t v=readByte((H<<8)|L);
                uint16_t hl=((H<<8)|L)-1;
                H=hl>>8; L=hl&0xFF;
                B=dec8(B);
                outPort(C, v);
                F |= FLAG_N;
                return 16;
            }
            case 0xB3: {    // OTIR
                uint8_t v=readByte((H<<8)|L);
                uint16_t hl=((H<<8)|L)+1;
                H=hl>>8; L=hl&0xFF;
                B=dec8(B);
                outPort(C, v);
                F |= FLAG_N;
                if (B != 0) { PC-=2; return 21; }
                return 16;
            }
            case 0xBB: {    // OTDR
                uint8_t v=readByte((H<<8)|L);
                uint16_t hl=((H<<8)|L)-1;
                H=hl>>8; L=hl&0xFF;
                B=dec8(B);
                outPort(C, v);
                F |= FLAG_N;
                if (B != 0) { PC-=2; return 21; }
                return 16;
            }
        }
    }
    return 8;
}

// ============================================================================
// PREFIX DD (IX) / FD (IY) - riceve l'opcode secondario
// ============================================================================

RAMFUNCZ80 int Z80::executeDDFD(bool isIX, uint8_t op) {
    uint16_t& idx = isIX ? IX : IY;

    uint8_t idxH = idx >> 8;
    uint8_t idxL = idx & 0xFF;

    switch (op) {
        case 0x09: { uint32_t r=idx+((B<<8)|C); F=(F&(FLAG_S|FLAG_Z|FLAG_PV))|(((idx^((B<<8)|C)^r)>>8)&FLAG_H)|((r>>16)&FLAG_C)|((r>>8)&(FLAG_Y|FLAG_X)); idx=r&0xFFFF; return 15; }
        case 0x19: { uint32_t r=idx+((D<<8)|E); F=(F&(FLAG_S|FLAG_Z|FLAG_PV))|(((idx^((D<<8)|E)^r)>>8)&FLAG_H)|((r>>16)&FLAG_C)|((r>>8)&(FLAG_Y|FLAG_X)); idx=r&0xFFFF; return 15; }
        case 0x29: { uint32_t r=idx+idx;        F=(F&(FLAG_S|FLAG_Z|FLAG_PV))|(((idx^idx^r)>>8)&FLAG_H)|((r>>16)&FLAG_C)|((r>>8)&(FLAG_Y|FLAG_X));         idx=r&0xFFFF; return 15; }
        case 0x39: { uint32_t r=idx+SP;         F=(F&(FLAG_S|FLAG_Z|FLAG_PV))|(((idx^SP^r)>>8)&FLAG_H)|((r>>16)&FLAG_C)|((r>>8)&(FLAG_Y|FLAG_X));          idx=r&0xFFFF; return 15; }

        case 0x21: idx = fetch16(); return 14;
        case 0x22: { uint16_t a=fetch16(); writeWord(a,idx); return 20; }
        case 0x2A: { uint16_t a=fetch16(); idx=readWord(a);  return 20; }

        case 0x23: idx++; return 10;
        case 0x2B: idx--; return 10;

        case 0x24: { uint8_t h=idx>>8;   h=inc8(h); idx=(idx&0x00FF)|((uint16_t)h<<8); return 8; }
        case 0x25: { uint8_t h=idx>>8;   h=dec8(h); idx=(idx&0x00FF)|((uint16_t)h<<8); return 8; }
        case 0x2C: { uint8_t l=idx&0xFF; l=inc8(l); idx=(idx&0xFF00)|l; return 8; }
        case 0x2D: { uint8_t l=idx&0xFF; l=dec8(l); idx=(idx&0xFF00)|l; return 8; }

        case 0x26: { uint8_t n=readByte(PC++); idx=(idx&0x00FF)|((uint16_t)n<<8); return 11; }
        case 0x2E: { uint8_t n=readByte(PC++); idx=(idx&0xFF00)|n; return 11; }

        case 0xE1: idx=pop();  return 14;
        case 0xE5: push(idx);  return 15;
        case 0xE9: PC=idx;     return 8;
        case 0xF9: SP=idx;     return 10;

        case 0xE3: { uint16_t t=readWord(SP); writeWord(SP,idx); idx=t; return 19; }

        case 0x34: { int8_t d=(int8_t)readByte(PC++); uint16_t a=idx+d; writeByte(a,inc8(readByte(a))); return 23; }
        case 0x35: { int8_t d=(int8_t)readByte(PC++); uint16_t a=idx+d; writeByte(a,dec8(readByte(a))); return 23; }
        case 0x36: { int8_t d=(int8_t)readByte(PC++); uint8_t n=readByte(PC++); writeByte(idx+d,n); return 19; }

        case 0x44: B=idxH; return 8;
        case 0x45: B=idxL; return 8;
        case 0x4C: C=idxH; return 8;
        case 0x4D: C=idxL; return 8;
        case 0x54: D=idxH; return 8;
        case 0x55: D=idxL; return 8;
        case 0x5C: E=idxH; return 8;
        case 0x5D: E=idxL; return 8;
        case 0x7C: A=idxH; return 8;
        case 0x7D: A=idxL; return 8;

        case 0x60: idx=(idx&0x00FF)|((uint16_t)B<<8); return 8;
        case 0x61: idx=(idx&0x00FF)|((uint16_t)C<<8); return 8;
        case 0x62: idx=(idx&0x00FF)|((uint16_t)D<<8); return 8;
        case 0x63: idx=(idx&0x00FF)|((uint16_t)E<<8); return 8;
        case 0x64: return 8;
        case 0x65: idx=(idx&0x00FF)|((uint16_t)idxL<<8); return 8;
        case 0x67: idx=(idx&0x00FF)|((uint16_t)A<<8); return 8;
        case 0x6C: idx=(idx&0xFF00)|idxH; return 8;
        case 0x6D: return 8;
        case 0x6F: idx=(idx&0xFF00)|A; return 8;

        case 0x46: { int8_t d=(int8_t)readByte(PC++); B=readByte(idx+d); return 19; }
        case 0x4E: { int8_t d=(int8_t)readByte(PC++); C=readByte(idx+d); return 19; }
        case 0x56: { int8_t d=(int8_t)readByte(PC++); D=readByte(idx+d); return 19; }
        case 0x5E: { int8_t d=(int8_t)readByte(PC++); E=readByte(idx+d); return 19; }
        case 0x66: { int8_t d=(int8_t)readByte(PC++); H=readByte(idx+d); return 19; }
        case 0x6E: { int8_t d=(int8_t)readByte(PC++); L=readByte(idx+d); return 19; }
        case 0x7E: { int8_t d=(int8_t)readByte(PC++); A=readByte(idx+d); return 19; }

        case 0x70: { int8_t d=(int8_t)readByte(PC++); writeByte(idx+d,B); return 19; }
        case 0x71: { int8_t d=(int8_t)readByte(PC++); writeByte(idx+d,C); return 19; }
        case 0x72: { int8_t d=(int8_t)readByte(PC++); writeByte(idx+d,D); return 19; }
        case 0x73: { int8_t d=(int8_t)readByte(PC++); writeByte(idx+d,E); return 19; }
        case 0x74: { int8_t d=(int8_t)readByte(PC++); writeByte(idx+d,H); return 19; }
        case 0x75: { int8_t d=(int8_t)readByte(PC++); writeByte(idx+d,L); return 19; }
        case 0x77: { int8_t d=(int8_t)readByte(PC++); writeByte(idx+d,A); return 19; }

        case 0x86: { int8_t d=(int8_t)readByte(PC++); add8(readByte(idx+d)); return 19; }
        case 0x8E: { int8_t d=(int8_t)readByte(PC++); adc8(readByte(idx+d)); return 19; }
        case 0x96: { int8_t d=(int8_t)readByte(PC++); sub8(readByte(idx+d)); return 19; }
        case 0x9E: { int8_t d=(int8_t)readByte(PC++); sbc8(readByte(idx+d)); return 19; }
        case 0xA6: { int8_t d=(int8_t)readByte(PC++); and8(readByte(idx+d)); return 19; }
        case 0xAE: { int8_t d=(int8_t)readByte(PC++); xor8(readByte(idx+d)); return 19; }
        case 0xB6: { int8_t d=(int8_t)readByte(PC++); or8(readByte(idx+d));  return 19; }
        case 0xBE: { int8_t d=(int8_t)readByte(PC++); cp8(readByte(idx+d));  return 19; }

        case 0x84: add8(idxH); return 8;
        case 0x85: add8(idxL); return 8;
        case 0x8C: adc8(idxH); return 8;
        case 0x8D: adc8(idxL); return 8;
        case 0x94: sub8(idxH); return 8;
        case 0x95: sub8(idxL); return 8;
        case 0x9C: sbc8(idxH); return 8;
        case 0x9D: sbc8(idxL); return 8;
        case 0xA4: and8(idxH); return 8;
        case 0xA5: and8(idxL); return 8;
        case 0xAC: xor8(idxH); return 8;
        case 0xAD: xor8(idxL); return 8;
        case 0xB4: or8(idxH);  return 8;
        case 0xB5: or8(idxL);  return 8;
        case 0xBC: cp8(idxH);  return 8;
        case 0xBD: cp8(idxL);  return 8;

        case 0xCB: {
            int8_t  d   = (int8_t)readByte(PC++);
            uint8_t op2 = readByte(PC++);
            return executeDDFDCB(isIX, d, op2);
        }

        default:
            return exec(op);
    }
}

// ============================================================================
// PREFIX DDCB / FDCB - con mascheramento indirizzo 16 bit
// ============================================================================

RAMFUNCZ80 int Z80::executeDDFDCB(bool isIX, int8_t offset, uint8_t op) {
    uint16_t addr = (isIX ? IX : IY) + offset;
    addr &= 0xFFFF;
    uint8_t val = readByte(addr);
    uint8_t bit = (op >> 3) & 7;
    uint8_t z   = op & 7;

    // OTTIMIZZAZIONE: _regPtr condiviso

    if ((op & 0xC0) == 0x00) {             // Rotate/shift
        switch (op >> 3) {
            case 0: val=rlc(val); break; case 1: val=rrc(val); break;
            case 2: val=rl(val);  break; case 3: val=rr(val);  break;
            case 4: val=sla(val); break; case 5: val=sra(val); break;
            case 6: val=sll(val); break; case 7: val=srl(val); break;
        }
        writeByte(addr, val);
        if (z != 6 && _regPtr[z]) *_regPtr[z] = val;
        return 23;
    } else if ((op & 0xC0) == 0x40) {      // BIT
        uint8_t masked = val & (1 << bit);
        F = (F & FLAG_C) | FLAG_H |
            (masked ? 0 : FLAG_Z | FLAG_PV) |
            (val & (FLAG_Y | FLAG_X)) |
            (masked & FLAG_S);
        return 20;
    } else if ((op & 0xC0) == 0x80) {      // RES
        val &= ~(1 << bit);
        writeByte(addr, val);
        if (z != 6 && _regPtr[z]) *_regPtr[z] = val;
        return 23;
    } else {                               // SET
        val |= (1 << bit);
        writeByte(addr, val);
        if (z != 6 && _regPtr[z]) *_regPtr[z] = val;
        return 23;
    }
}