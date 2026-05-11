// Definitionen der Globals aus state.h. Init-Werte 1:1 aus Sketch.

#include "state.h"
#include "config.h"

// ---- ADD2 ----
int  add2Gewicht          = 0;
int  add2AnzeigeGewicht   = 0;
int  add2Offset           = 0;
bool add2ZeroModus        = false;
bool add2DatenVorhanden   = false;
bool add2WaageOff         = false;
unsigned long add2LastData = 0;

// ---- WiFi ----
WifiStatus    wifiStatus     = WIFI_SUCHE;
unsigned long lastReconnect  = 0;
int           signalBars     = 0;
unsigned long lastRSSIRead   = 0;

// ---- Schaufel ----
float schaufelRohwert           = 0.0f;
int   schaufelGewicht           = 0;
int   schaufelGewichtGefiltert  = 0;
bool  schaufelInPosition        = false;
float schaufelTara              = 0.0f;
float schaufelFaktor            = 1.0f;   // wird in setup() durch NVS ueberschrieben
unsigned long lastADCRead       = 0;
bool  adsVorhanden              = false;

int     adsFehlerZaehler        = 0;
int16_t adsLetzterRawWert       = 0;
int     adsGleicheWerteZaehler  = 0;

// ---- Filter ----
float filterBuffer[FILTER_BUFFER_SIZE] = {0};
int   filterBufferIndex      = 0;
int   filterBufferCount      = 0;
int   filterAnzahl           = FILTER_ANZAHL_DEFAULT;
int   filterSchwelle         = FILTER_SCHWELLE_DEFAULT;
int   letzteAngezeigtesGewicht = 0;

// ---- Tabs ----
Tab currentTab = TAB_SCHAUFEL;

// ---- Menü ----
MenuState menuState           = MENU_NONE;
int       kalibierGewicht     = 100;
int       tempFilterAnzahl    = FILTER_ANZAHL_DEFAULT;
int       tempFilterSchwelle  = FILTER_SCHWELLE_DEFAULT;

// ---- Display Update Flags ----
bool displayNeedsFullRedraw = true;
int  lastAdd2Anzeige        = -99999;
bool lastAdd2DatenStatus    = false;
WifiStatus lastWifiStatus   = WIFI_SUCHE;
int  lastSchaufelGewicht    = -99999;
bool lastSchaufelPosition   = false;
bool lastButtonsEnabled     = false;
bool lastWaageOff           = false;
int  lastSignalBars         = -1;
