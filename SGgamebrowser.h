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
#ifndef SGGAMEBROWSER_H
#define SGGAMEBROWSER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SD.h>
#include <LittleFS.h>
#include <TJpg_Decoder.h>
#include "SMSloader.h"

#define GAME_DIR "/PiSG1000"
#define JOY_DEBOUNCE_MS 200
#define MAX_SORTED_GAMES 200  // Limite array ordinato

class SGGameBrowser {
public:
    SGGameBrowser(TFT_eSPI* display, SMSLoader* loaderRef, uint8_t sdCS = SD_CS);
    ~SGGameBrowser() { freeSortedFileList(); }  

    bool begin(FSType fsType = FSType::FS_SD);
    
    void setLogo(const unsigned short* logoData, int width, int height);
    void draw();
    void nextGame();
    void prevGame();
    bool update(uint8_t joyState);
    String getPrgSelected();
    void setPath(const char* cpath);
    void setFileExt(const char* fext);
    
private:
    TFT_eSPI* tft;
    SMSLoader* loader;
    uint8_t sdCSPin;
    FSType currentFS;
    char browserPath[20] = {0};
    char browserExt[8] = {0};
    
    const unsigned short* logo;
    int logoWidth;
    int logoHeight;
    bool hasLogo;
    
    // array ordinato alfabeticamente
    int totalGames;
    int currentIndex;
    char currentFile[32];
    uint32_t lastJoyTime;
    bool needsRedraw;
    char** sortedFileNames;  // array di nomi ordinati
    
    // filesystem
    File openFile(const char* path, const char* mode = "r");
    bool fileExists(const char* path);
    
    // sorting e accesso ordinato
    int countPRGFiles();
    bool buildSortedFileList();
    void freeSortedFileList();
    bool getFileAtIndex(int index, char* outName);
    
    // utility
    bool isPRG(const char* name);
    String getDisplayName(const char* filename);
    bool hasImageFile(const char* prgName);
    
    // disegno
    void drawBackground();
    void drawBckBoxes();
    void drawLogo();
    void drawGameImage();
    void drawGameName();
    void drawArrows();
    void drawCredits();
    void showError(const char* msg);
    
    // callback TJpg_Decoder
    static bool tftOutput(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap);
    static TFT_eSPI* staticTFT;
};

#endif