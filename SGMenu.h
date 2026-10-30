/*
 * Questo file fa parte del progetto PiSG1000.
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

#ifndef SG_MENU_H
#define SG_MENU_H

#include <TFT_eSPI.h>
#include "FunduinoJoystick.h"

// ===== VOCI MENU =====
enum MenuItem {
    MENU_GAME = 0,
    MENU_OPT = 1,
    //MENU_BIOS = 2,
    //MENU_ITEMS_COUNT = 3
    MENU_ITEMS_COUNT = 2
};

// ===== AZIONI MENU =====
enum MenuAction {
    ACTION_NONE = 0,
    ACTION_START_GAME = 1,
    ACTION_START_OPT = 2
    //ACTION_START_BIOS = 3
};

class SGMenu {
public:
    SGMenu(TFT_eSPI* display, FunduinoJoystick* joystick);
    
    // Inizializza menu (disegna tutto)
    void begin();
    
    // Aggiorna menu (chiamare in loop)
    // Ritorna ACTION_NONE finché non viene selezionata una voce
    MenuAction update();
    
    // Disegna menu completo
    void draw();
    
    // Imposta logo (opzionale, chiamare prima di begin())
    void setLogo(const unsigned short* logoData, int width, int height);
    
private:
    TFT_eSPI* tft;
    FunduinoJoystick* joy;
    
    // Logo
    const unsigned short* logo;
    int logoWidth;
    int logoHeight;
    bool hasLogo;
    
    // Stato menu
    MenuItem selectedItem;
    MenuItem lastSelectedItem;
    
    // Debouncing joystick
    uint32_t lastJoyTime;
    static const uint32_t JOY_DEBOUNCE_MS = 200;
    
    // Disegno
    void drawBackground();
    void drawBckBoxes();
    void drawLogo();
    void drawMenuItems();
    void drawCredits();
    void drawMenuItem(MenuItem item, bool selected);
    
    // Calcolo posizioni
    int getMenuItemY(MenuItem item);
    
    // Testi menu
    const char* getMenuItemText(MenuItem item);
};

#endif