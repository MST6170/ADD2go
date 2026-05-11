// Globale Shared-State-Variablen — 1:1 aus add2go_empfaenger_v1_0_2.ino
// (Z. 100-188 + 130-148) als `extern` deklariert. Definitionen in state.cpp.
//
// Wer schreibt:
//   - add2*       : network.cpp (UDP-Empfang)
//   - schaufel*   : scale.cpp (ADC + Filter)
//   - wifi*       : network.cpp
//   - signalBars  : network.cpp
//   - menuState   : ui.cpp + touch.cpp
//   - display*    : display.cpp (last*-State-Tracking)
//
// Wer liest: alle Module.

#pragma once

#include <Arduino.h>

// ---- ADD2 Waage ----
extern int  add2Gewicht;
extern int  add2AnzeigeGewicht;
extern int  add2Offset;
extern bool add2ZeroModus;
extern bool add2DatenVorhanden;
extern bool add2WaageOff;
extern unsigned long add2LastData;

// ---- WiFi ----
enum WifiStatus { WIFI_OK, WIFI_SUCHE, WIFI_FEHLER };
extern WifiStatus wifiStatus;
extern unsigned long lastReconnect;
extern int signalBars;
extern unsigned long lastRSSIRead;

// ---- Schaufel ----
extern float schaufelRohwert;
extern int   schaufelGewicht;
extern int   schaufelGewichtGefiltert;
extern bool  schaufelInPosition;
extern float schaufelTara;
extern float schaufelFaktor;
extern unsigned long lastADCRead;
extern bool  adsVorhanden;

// ADS1115-Watchdog
extern int     adsFehlerZaehler;
extern int16_t adsLetzterRawWert;
extern int     adsGleicheWerteZaehler;

// ---- Filter ----
extern float filterBuffer[];          // size = FILTER_BUFFER_SIZE
extern int   filterBufferIndex;
extern int   filterBufferCount;
extern int   filterAnzahl;
extern int   filterSchwelle;
extern int   letzteAngezeigtesGewicht;

// ---- Tabs (Phase 3c) ----
enum Tab {
    TAB_SCHAUFEL = 0,   // Default beim Boot, klassische v1.0.2-Funktionalitaet
    TAB_FLOW     = 1    // add2flow-Empfaenger (Schritt 5)
};
extern Tab currentTab;

// ---- Menü ----
enum MenuState {
    MENU_NONE,
    MENU_SCHAUFEL,
    MENU_KALIBRIEREN,
    MENU_CONFIRM_TARA,
    MENU_FILTER
};
extern MenuState menuState;
extern int kalibierGewicht;
extern int tempFilterAnzahl;
extern int tempFilterSchwelle;

// ---- Display Update Flags ----
extern bool displayNeedsFullRedraw;
extern int  lastAdd2Anzeige;
extern bool lastAdd2DatenStatus;
extern WifiStatus lastWifiStatus;
extern int  lastSchaufelGewicht;
extern bool lastSchaufelPosition;
extern bool lastButtonsEnabled;
extern bool lastWaageOff;
extern int  lastSignalBars;
