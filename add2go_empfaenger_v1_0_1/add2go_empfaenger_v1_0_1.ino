/*
 * ADD2go Empfänger - v1.0.1
 * =========================
 * 
 * Wireless Waagen-Display für Dinamica Generale ADD2 Waage
 * Empfängt Gewichtsdaten vom Sender per WiFi/UDP
 * 
 * Features:
 * - ADD2 Gewichtsanzeige (TOTAL/ZERO Funktion)
 * - Schaufel-Wiegung mit Drucksensor (optional)
 * - Filter (Mittelwert + Hysterese)
 * - "OFF" Anzeige wenn Waage aus
 * - Auto-Reconnect bei WiFi-Verlust
 * - Hardware Watchdog (5 Sekunden)
 * - Boot-Screen mit Fortschrittsbalken
 * 
 * Changelog v1.0.1:
 * - Fix: ZERO-Modus wird automatisch zurückgesetzt bei OFF oder Reconnect
 *        (verhindert falschen Offset nach Wiedereinschalten der Waage)
 * - Fix: Filter-Buffer wird nach TARA-Save geleert
 *        (verhindert Mischwerte aus alter und neuer Tara)
 * - Fix: ADS1115-Watchdog - bei wiederholten Lesefehlern wird der Sensor
 *        als ausgefallen markiert (für robusten Feldeinsatz)
 * - Fix: tft.textWidth() statt strlen()*14 für exakte Button-Zentrierung
 * 
 * Hardware:
 * - ESP32-WROOM-32U mit externer Antenne
 * - ILI9341 Display: CS=5, RST=2, DC=4, MOSI=23, SCK=18, MISO=19
 * - XPT2046 Touch: T_CS=15, T_IRQ=14
 * - ADS1115 ADC: SDA=21, SCL=22, Adresse 0x48
 * - Reed-Kontakt: GPIO36 (LOW = Wiegeposition)
 * 
 * WiFi: Verbindet sich mit "ADD2go" Access Point vom Sender
 * UDP Port: 5005
 * 
 * (c) MST 2026
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <Wire.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <Adafruit_ADS1X15.h>
#include <Preferences.h>
#include <esp_task_wdt.h>

// ============================================
// OBJEKTE
// ============================================
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(15, 14);
Adafruit_ADS1115 ads;
Preferences prefs;
WiFiUDP udp;

// ============================================
// PINS
// ============================================
#define REED_SENSOR_PIN 36  // Reed-Kontakt für Wiegeposition

// ============================================
// WIFI
// ============================================
const char* WIFI_SSID = "ADD2go";
const char* WIFI_PASS = "";
const int UDP_PORT = 5005;
char udpBuffer[64];

// ============================================
// FARBEN
// ============================================
// TFT_ORANGE ist bereits in TFT_eSPI.h definiert (0xFDA0) - kein Redefine nötig
#define TFT_DARKERGREY 0x4208

// ============================================
// DISPLAY BEREICHE
// ============================================
#define SCHAUFEL_BTN_X 5
#define SCHAUFEL_BTN_Y 5
#define SCHAUFEL_BTN_W 220
#define SCHAUFEL_BTN_H 35

#define ADD2_Y 50
#define ADD2_H 100

#define BTN1_X 20
#define BTN1_Y 180
#define BTN2_X 170
#define BTN2_Y 180
#define BTN_W 130
#define BTN_H 50

// ============================================
// STATUS VARIABLEN - ADD2 Waage
// ============================================
int add2Gewicht = 0;
int add2AnzeigeGewicht = 0;
int add2Offset = 0;
bool add2ZeroModus = false;
bool add2DatenVorhanden = false;
bool add2WaageOff = false;  // Waage ist ausgeschaltet
unsigned long add2LastData = 0;

// ============================================
// WATCHDOG
// ============================================
#define WATCHDOG_TIMEOUT_SEC 5

// ============================================
// STATUS VARIABLEN - WiFi
// ============================================
enum WifiStatus { WIFI_OK, WIFI_SUCHE, WIFI_FEHLER };
WifiStatus wifiStatus = WIFI_SUCHE;
unsigned long lastReconnect = 0;
const int TIMEOUT_MS = 2000;
const int RECONNECT_INTERVAL = 5000;

// ============================================
// STATUS VARIABLEN - Schaufel
// ============================================
float schaufelRohwert = 0;
int schaufelGewicht = 0;
int schaufelGewichtGefiltert = 0;  // Nach Filter
bool schaufelInPosition = false;
float schaufelTara = 0;
float schaufelFaktor = 1.0;
unsigned long lastADCRead = 0;
bool adsVorhanden = false;  // ADS1115 gefunden?
const int ADC_INTERVAL = 100;  // 10Hz

// ADS1115-Watchdog (NEU v1.0.1)
int adsFehlerZaehler = 0;
const int ADS_FEHLER_LIMIT = 10;  // Nach 10 Fehlern: ADS als ausgefallen markieren
int16_t adsLetzterRawWert = 0;
int adsGleicheWerteZaehler = 0;
const int ADS_STUCK_LIMIT = 50;   // 50 identische Werte (~5 Sek) = hängt

// ============================================
// FILTER
// ============================================
#define FILTER_BUFFER_SIZE 30
float filterBuffer[FILTER_BUFFER_SIZE];
int filterBufferIndex = 0;
int filterBufferCount = 0;
int filterAnzahl = 10;      // Mittelwert über X Werte (5-30)
int filterSchwelle = 5;     // Hysterese in kg (1-10)
int letzteAngezeigtesGewicht = 0;

// ============================================
// MENÜ SYSTEM
// ============================================
enum MenuState { 
  MENU_NONE,
  MENU_SCHAUFEL,
  MENU_KALIBRIEREN,
  MENU_CONFIRM_TARA,
  MENU_FILTER
};
MenuState menuState = MENU_NONE;
int kalibierGewicht = 100;

// Temporäre Werte für Filter-Menü
int tempFilterAnzahl = 10;
int tempFilterSchwelle = 5;

// ============================================
// DISPLAY UPDATE FLAGS
// ============================================
bool displayNeedsFullRedraw = true;
int lastAdd2Anzeige = -99999;
bool lastAdd2DatenStatus = false;
WifiStatus lastWifiStatus = WIFI_SUCHE;
int lastSchaufelGewicht = -99999;
bool lastSchaufelPosition = false;
bool lastButtonsEnabled = false;
bool lastWaageOff = false;  // (v1.0.1: aus updateHauptbildschirm rausgezogen)

// ============================================
// HILFSFUNKTIONEN (NEU v1.0.1)
// ============================================

// Filter-Buffer komplett zurücksetzen
void resetFilter() {
  for (int i = 0; i < FILTER_BUFFER_SIZE; i++) {
    filterBuffer[i] = 0;
  }
  filterBufferIndex = 0;
  filterBufferCount = 0;
  letzteAngezeigtesGewicht = 0;
  schaufelGewichtGefiltert = 0;
}

// ZERO-Modus zurücksetzen (z.B. bei OFF oder Reconnect)
void resetZero() {
  if (add2ZeroModus) {
    Serial.println("[INFO] ZERO automatisch zurueckgesetzt");
    add2ZeroModus = false;
    add2Offset = 0;
  }
}

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("========================================");
  Serial.println("ADD2go Empfaenger - v1.0.1");
  Serial.println("========================================");
  Serial.println();
  
  // Reed-Kontakt Pin
  pinMode(REED_SENSOR_PIN, INPUT);
  Serial.println("[OK] Reed-Kontakt GPIO36 konfiguriert");
  
  // I2C für ADS1115
  Wire.begin(21, 22);
  if (ads.begin()) {
    Serial.println("[OK] ADS1115 gefunden");
    ads.setGain(GAIN_ONE);
    adsVorhanden = true;
  } else {
    Serial.println("[FEHLER] ADS1115 nicht gefunden!");
    adsVorhanden = false;
  }
  
  // NVS laden
  prefs.begin("schaufel", false);
  schaufelTara = prefs.getFloat("tara", 0.0);
  schaufelFaktor = prefs.getFloat("faktor", 100.0);
  filterAnzahl = prefs.getInt("filterAnz", 10);
  filterSchwelle = prefs.getInt("filterSchw", 5);
  
  // Grenzen prüfen
  if (filterAnzahl < 5) filterAnzahl = 5;
  if (filterAnzahl > 30) filterAnzahl = 30;
  if (filterSchwelle < 1) filterSchwelle = 1;
  if (filterSchwelle > 10) filterSchwelle = 10;
  
  Serial.print("[OK] NVS geladen - Tara: ");
  Serial.print(schaufelTara, 3);
  Serial.print("V, Faktor: ");
  Serial.print(schaufelFaktor, 1);
  Serial.print(" kg/V, Filter: ");
  Serial.print(filterAnzahl);
  Serial.print(" Werte, Schwelle: ");
  Serial.print(filterSchwelle);
  Serial.println(" kg");
  
  // Filter-Buffer initialisieren
  resetFilter();  // (v1.0.1: über Hilfsfunktion)
  
  // Display
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  Serial.println("[OK] Display initialisiert");
  
  // Touch
  ts.begin();
  ts.setRotation(1);
  Serial.println("[OK] Touch initialisiert");
  
  // ========================================
  // BOOT-SCREEN MIT FORTSCHRITTSBALKEN
  // ========================================
  
  // Titel "ADD2go" - groß und zentriert
  tft.setFreeFont(&FreeSansBold18pt7b);
  tft.setTextColor(TFT_WHITE);
  String titel = "ADD2go";
  int titelW = tft.textWidth(titel);
  tft.setCursor((320 - titelW) / 2, 50);
  tft.print(titel);
  tft.setFreeFont(NULL);
  
  // Fortschrittsbalken - volle Breite mit kleinem Rand
  int barX = 15;
  int barY = 100;
  int barW = 290;  // 320 - 2*15
  int barH = 25;
  tft.drawRoundRect(barX, barY, barW, barH, 5, TFT_WHITE);
  
  // Status Y-Position (linksbündig mit Balken)
  int statusY = barY + barH + 15;
  
  // Hilfsfunktion inline - Fortschritt zeichnen
  auto zeichneProgress = [&](int prozent, const char* text) {
    // Balken füllen (grün)
    int fillW = (barW - 4) * prozent / 100;
    tft.fillRoundRect(barX + 2, barY + 2, fillW, barH - 4, 3, TFT_GREEN);
    
    // Status Text linksbündig unter Balken
    tft.fillRect(barX, statusY - 5, barW, 20, TFT_BLACK);
    tft.setTextColor(TFT_LIGHTGREY);
    tft.setTextSize(1);
    tft.setCursor(barX, statusY);
    tft.print(text);
    
    delay(200);
  };
  
  zeichneProgress(20, "Display OK");
  
  zeichneProgress(40, "Touch OK");
  
  // Watchdog konfigurieren (5 Sekunden) - VOR WiFi!
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WATCHDOG_TIMEOUT_SEC * 1000,
    .idle_core_mask = (1 << 0) | (1 << 1),
    .trigger_panic = true
  };
  esp_err_t wdt_err = esp_task_wdt_init(&wdt_config);
  if (wdt_err == ESP_OK || wdt_err == ESP_ERR_INVALID_STATE) {
    esp_task_wdt_add(NULL);
    Serial.print("[OK] Watchdog: ");
    Serial.print(WATCHDOG_TIMEOUT_SEC);
    Serial.println(" Sekunden");
  }
  
  zeichneProgress(60, "Watchdog OK");
  
  // WiFi (Watchdog wird gefüttert in verbindeWiFi)
  zeichneProgress(80, "WiFi verbinden...");
  verbindeWiFi();
  
  if (WiFi.status() == WL_CONNECTED) {
    zeichneProgress(100, "WiFi OK!");
  } else {
    // Balken orange bei Fehler
    tft.fillRoundRect(barX + 2, barY + 2, barW - 4, barH - 4, 3, TFT_ORANGE);
    tft.fillRect(barX, statusY - 5, barW, 20, TFT_BLACK);
    tft.setTextColor(TFT_ORANGE);
    tft.setCursor(barX, statusY);
    tft.print("WiFi nicht gefunden...");
  }
  
  // Copyright - klein und zentriert
  delay(300);
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextSize(1);
  String copyright = "(c) MST 2026 - v1.0.1";
  int copyW = tft.textWidth(copyright);
  tft.setCursor((320 - copyW) / 2, 200);
  tft.print(copyright);
  
  delay(800);
  
  // UI zeichnen
  displayNeedsFullRedraw = true;
  
  Serial.println();
  Serial.println("System bereit!");
  Serial.println();
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
  // Watchdog füttern
  esp_task_wdt_reset();
  
  // 1. Reed-Kontakt SOFORT lesen (schnelle Reaktion!)
  schaufelInPosition = (digitalRead(REED_SENSOR_PIN) == LOW);
  
  // 2. WiFi managen
  handleWiFi();
  
  // 3. UDP Daten empfangen
  handleUDP();
  
  // 4. ADS1115 lesen (alle 100ms = 10Hz)
  handleADC();
  
  // 5. Touch verarbeiten
  handleTouch();
  
  // 6. Display aktualisieren
  handleDisplay();
  
  // 7. Im Menü: Live-Update
  handleMenuLiveUpdate();
  
  delay(10);
}

// ============================================
// WIFI HANDLING
// ============================================
void handleWiFi() {
  // Watchdog füttern bei WiFi-Problemen
  esp_task_wdt_reset();
  
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiStatus == WIFI_OK) {
      Serial.println("[WARNUNG] WiFi Verbindung verloren!");
      wifiStatus = WIFI_FEHLER;
      lastReconnect = millis();
      // (v1.0.1) ZERO zurücksetzen wenn Verbindung weg
      resetZero();
    } 
    else if (millis() - lastReconnect > RECONNECT_INTERVAL) {
      // Nicht-blockierender Reconnect
      Serial.println("[INFO] Starte Reconnect...");
      wifiStatus = WIFI_SUCHE;
      WiFi.disconnect(true);  // true = WiFi auch abschalten
      delay(100);             // Kurz warten
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      lastReconnect = millis();
    }
  } else {
    if (wifiStatus != WIFI_OK) {
      wifiStatus = WIFI_OK;
      Serial.print("[OK] WiFi verbunden! IP: ");
      Serial.println(WiFi.localIP());
      udp.stop();
      udp.begin(UDP_PORT);
    }
  }
}

void verbindeWiFi() {
  Serial.print("Verbinde mit ");
  Serial.print(WIFI_SSID);
  Serial.print("...");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  int versuche = 0;
  while (WiFi.status() != WL_CONNECTED && versuche < 10) {
    esp_task_wdt_reset();  // Watchdog füttern während wir warten!
    delay(500);
    Serial.print(".");
    versuche++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[OK] Verbunden! IP: ");
    Serial.println(WiFi.localIP());
    udp.stop();
    udp.begin(UDP_PORT);
  } else {
    Serial.println("[FEHLER] Verbindung fehlgeschlagen");
  }
}

// ============================================
// UDP HANDLING
// ============================================
void handleUDP() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    int len = udp.read(udpBuffer, sizeof(udpBuffer) - 1);
    if (len > 0) {
      udpBuffer[len] = '\0';
      
      // Prüfen ob "OFF" empfangen
      if (strcmp(udpBuffer, "OFF") == 0) {
        // (v1.0.1) Beim Übergang ON->OFF: ZERO zurücksetzen
        if (!add2WaageOff) {
          resetZero();
        }
        add2WaageOff = true;
        add2DatenVorhanden = true;  // Verbindung ist da, nur Waage aus
        add2LastData = millis();
      } else {
        add2WaageOff = false;
        add2Gewicht = atoi(udpBuffer);
        add2DatenVorhanden = true;
        add2LastData = millis();
      }
    }
  }
  
  // Timeout prüfen (keine UDP Pakete mehr)
  if (add2DatenVorhanden && (millis() - add2LastData > TIMEOUT_MS)) {
    add2DatenVorhanden = false;
    add2WaageOff = false;
    // (v1.0.1) ZERO zurücksetzen bei Daten-Timeout
    resetZero();
    Serial.println("[WARNUNG] ADD2 Daten-Timeout");
  }
}

// ============================================
// ADC HANDLING MIT FILTER
// ============================================
void handleADC() {
  // Nur wenn ADS1115 vorhanden!
  if (!adsVorhanden) return;
  
  if (millis() - lastADCRead < ADC_INTERVAL) return;
  lastADCRead = millis();
  
  // ADC lesen
  int16_t rawADC = ads.readADC_SingleEnded(0);
  
  // (v1.0.1) ADS1115-Watchdog: erkenne hängenden oder defekten Sensor
  // Fehlerwerte: -1 oder konstant 0 / max bei nicht angeschlossenem Sensor
  if (rawADC == -1) {
    adsFehlerZaehler++;
    if (adsFehlerZaehler >= ADS_FEHLER_LIMIT) {
      Serial.println("[FEHLER] ADS1115 liefert Fehlerwerte - als ausgefallen markiert");
      adsVorhanden = false;
      adsFehlerZaehler = 0;
    }
    return;
  } else {
    adsFehlerZaehler = 0;
  }
  
  // Hängender Wert? (z.B. I2C-Bus eingefroren)
  if (rawADC == adsLetzterRawWert) {
    adsGleicheWerteZaehler++;
    if (adsGleicheWerteZaehler >= ADS_STUCK_LIMIT) {
      Serial.println("[FEHLER] ADS1115 haengt - als ausgefallen markiert");
      adsVorhanden = false;
      adsGleicheWerteZaehler = 0;
      return;
    }
  } else {
    adsGleicheWerteZaehler = 0;
    adsLetzterRawWert = rawADC;
  }
  
  schaufelRohwert = rawADC * 0.000125;
  
  if (schaufelInPosition) {
    // Rohgewicht berechnen
    float spannungBereinigt = schaufelRohwert - schaufelTara;
    schaufelGewicht = (int)(spannungBereinigt * schaufelFaktor);
    if (schaufelGewicht < 0) schaufelGewicht = 0;
    
    // In Filter-Buffer speichern
    filterBuffer[filterBufferIndex] = schaufelGewicht;
    filterBufferIndex = (filterBufferIndex + 1) % FILTER_BUFFER_SIZE;
    if (filterBufferCount < FILTER_BUFFER_SIZE) filterBufferCount++;
    
    // Mittelwert berechnen (über filterAnzahl Werte)
    float summe = 0;
    int anzahl = min(filterAnzahl, filterBufferCount);
    for (int i = 0; i < anzahl; i++) {
      int idx = (filterBufferIndex - 1 - i + FILTER_BUFFER_SIZE) % FILTER_BUFFER_SIZE;
      summe += filterBuffer[idx];
    }
    int mittelwert = (int)(summe / anzahl);
    
    // Hysterese anwenden
    if (abs(mittelwert - letzteAngezeigtesGewicht) >= filterSchwelle) {
      schaufelGewichtGefiltert = mittelwert;
      letzteAngezeigtesGewicht = mittelwert;
    }
    // Sonst bleibt schaufelGewichtGefiltert unverändert
  }
}

// ============================================
// MENÜ LIVE UPDATE
// ============================================
void handleMenuLiveUpdate() {
  if (menuState == MENU_NONE) return;
  
  static bool letztePositionImMenu = true;
  
  if (schaufelInPosition != letztePositionImMenu) {
    letztePositionImMenu = schaufelInPosition;
    
    Serial.print("[INFO] Wiegeposition geändert: ");
    Serial.println(schaufelInPosition ? "JA" : "NEIN");
    
    displayNeedsFullRedraw = true;
    
    if (!schaufelInPosition && (menuState == MENU_KALIBRIEREN || menuState == MENU_CONFIRM_TARA)) {
      Serial.println("[WARNUNG] Position verloren - zurück zum Menü");
      menuState = MENU_SCHAUFEL;
    }
  }
}

// ============================================
// TOUCH HANDLING
// ============================================
void handleTouch() {
  if (!ts.touched()) return;
  
  TS_Point p = ts.getPoint();
  int tx = map(p.x, 3900, 300, 0, 320);
  int ty = map(p.y, 3900, 300, 0, 240);
  
  delay(50);
  
  switch (menuState) {
    case MENU_NONE:
      handleTouchHauptbildschirm(tx, ty);
      break;
    case MENU_SCHAUFEL:
      handleTouchSchaufelMenu(tx, ty);
      break;
    case MENU_KALIBRIEREN:
      handleTouchKalibrieren(tx, ty);
      break;
    case MENU_CONFIRM_TARA:
      handleTouchTaraConfirm(tx, ty);
      break;
    case MENU_FILTER:
      handleTouchFilter(tx, ty);
      break;
  }
  
  delay(150);
}

void handleTouchHauptbildschirm(int tx, int ty) {
  // Schaufel-Button oben?
  if (tx >= SCHAUFEL_BTN_X && tx <= SCHAUFEL_BTN_X + SCHAUFEL_BTN_W &&
      ty >= SCHAUFEL_BTN_Y && ty <= SCHAUFEL_BTN_Y + SCHAUFEL_BTN_H) {
    Serial.println(">>> Schaufel-Button gedrückt");
    menuState = MENU_SCHAUFEL;
    displayNeedsFullRedraw = true;
    return;
  }
  
  // Untere Buttons nur wenn ADD2 Daten vorhanden UND Waage nicht OFF
  if (!add2DatenVorhanden || add2WaageOff) {
    // Nur loggen wenn auf Button-Bereich gedrückt wurde
    if (ty >= BTN1_Y && ty <= BTN1_Y + BTN_H) {
      Serial.println("[INFO] Buttons deaktiviert - keine Daten oder Waage OFF");
    }
    return;
  }
  
  // TOTAL Button
  if (tx >= BTN1_X && tx <= BTN1_X + BTN_W &&
      ty >= BTN1_Y && ty <= BTN1_Y + BTN_H) {
    Serial.println(">>> TOTAL gedrückt");
    add2ZeroModus = false;
    add2Offset = 0;
    displayNeedsFullRedraw = true;
  }
  
  // ZERO Button
  if (tx >= BTN2_X && tx <= BTN2_X + BTN_W &&
      ty >= BTN2_Y && ty <= BTN2_Y + BTN_H) {
    Serial.println(">>> ZERO gedrückt");
    add2ZeroModus = true;
    add2Offset = add2Gewicht;
    Serial.print("Offset gesetzt: ");
    Serial.println(add2Offset);
    displayNeedsFullRedraw = true;
  }
}

void handleTouchSchaufelMenu(int tx, int ty) {
  // TARA Button (y: 50-85)
  if (ty >= 50 && ty <= 85 && tx >= 40 && tx <= 280) {
    Serial.println(">>> TARA gewählt");
    if (schaufelInPosition) {
      menuState = MENU_CONFIRM_TARA;
      displayNeedsFullRedraw = true;
    } else {
      Serial.println("[INFO] Nicht in Wiegeposition!");
    }
    return;
  }
  
  // KALIBRIEREN Button (y: 95-130)
  if (ty >= 95 && ty <= 130 && tx >= 40 && tx <= 280) {
    Serial.println(">>> KALIBRIEREN gewählt");
    if (schaufelInPosition) {
      kalibierGewicht = 100;
      menuState = MENU_KALIBRIEREN;
      displayNeedsFullRedraw = true;
    } else {
      Serial.println("[INFO] Nicht in Wiegeposition!");
    }
    return;
  }
  
  // FILTER Button (y: 140-175)
  if (ty >= 140 && ty <= 175 && tx >= 40 && tx <= 280) {
    Serial.println(">>> FILTER gewählt");
    tempFilterAnzahl = filterAnzahl;
    tempFilterSchwelle = filterSchwelle;
    menuState = MENU_FILTER;
    displayNeedsFullRedraw = true;
    return;
  }
  
  // ZURÜCK Button (y: 185-220)
  if (ty >= 185 && ty <= 220 && tx >= 40 && tx <= 280) {
    Serial.println(">>> ZURÜCK gewählt");
    menuState = MENU_NONE;
    displayNeedsFullRedraw = true;
    return;
  }
}

void handleTouchKalibrieren(int tx, int ty) {
  // -10 Button
  if (tx >= 30 && tx <= 130 && ty >= 110 && ty <= 160) {
    Serial.println(">>> -10 kg");
    kalibierGewicht -= 10;
    if (kalibierGewicht < 0) kalibierGewicht = 0;
    displayNeedsFullRedraw = true;
    return;
  }
  
  // +10 Button
  if (tx >= 190 && tx <= 290 && ty >= 110 && ty <= 160) {
    Serial.println(">>> +10 kg");
    kalibierGewicht += 10;
    if (kalibierGewicht > 9990) kalibierGewicht = 9990;
    displayNeedsFullRedraw = true;
    return;
  }
  
  // ESC Button
  if (tx >= 20 && tx <= 120 && ty >= 185 && ty <= 230) {
    Serial.println(">>> ESC");
    menuState = MENU_SCHAUFEL;
    displayNeedsFullRedraw = true;
    return;
  }
  
  // SAVE Button
  if (tx >= 200 && tx <= 300 && ty >= 185 && ty <= 230) {
    Serial.println(">>> SAVE");
    if (kalibierGewicht > 0) {
      float spannungBereinigt = schaufelRohwert - schaufelTara;
      if (spannungBereinigt > 0.01) {
        schaufelFaktor = (float)kalibierGewicht / spannungBereinigt;
        prefs.putFloat("faktor", schaufelFaktor);
        // (v1.0.1) Filter zurücksetzen, sonst Mischwerte
        resetFilter();
        Serial.print("[OK] Kalibriert! Neuer Faktor: ");
        Serial.println(schaufelFaktor);
      }
    }
    menuState = MENU_NONE;
    displayNeedsFullRedraw = true;
    return;
  }
}

void handleTouchFilter(int tx, int ty) {
  // Anzahl -5 Button (Y: 70-115)
  if (tx >= 20 && tx <= 70 && ty >= 70 && ty <= 115) {
    Serial.println(">>> Anzahl -5");
    tempFilterAnzahl -= 5;
    if (tempFilterAnzahl < 5) tempFilterAnzahl = 5;
    displayNeedsFullRedraw = true;
    return;
  }
  
  // Anzahl +5 Button (Y: 70-115)
  if (tx >= 250 && tx <= 300 && ty >= 70 && ty <= 115) {
    Serial.println(">>> Anzahl +5");
    tempFilterAnzahl += 5;
    if (tempFilterAnzahl > 30) tempFilterAnzahl = 30;
    displayNeedsFullRedraw = true;
    return;
  }
  
  // Schwelle -1 Button (Y: 130-175)
  if (tx >= 20 && tx <= 70 && ty >= 130 && ty <= 175) {
    Serial.println(">>> Schwelle -1");
    tempFilterSchwelle -= 1;
    if (tempFilterSchwelle < 1) tempFilterSchwelle = 1;
    displayNeedsFullRedraw = true;
    return;
  }
  
  // Schwelle +1 Button (Y: 130-175)
  if (tx >= 250 && tx <= 300 && ty >= 130 && ty <= 175) {
    Serial.println(">>> Schwelle +1");
    tempFilterSchwelle += 1;
    if (tempFilterSchwelle > 10) tempFilterSchwelle = 10;
    displayNeedsFullRedraw = true;
    return;
  }
  
  // ESC Button (Y: 190-235)
  if (tx >= 20 && tx <= 120 && ty >= 190 && ty <= 235) {
    Serial.println(">>> ESC");
    menuState = MENU_SCHAUFEL;
    displayNeedsFullRedraw = true;
    return;
  }
  
  // SAVE Button (Y: 190-235)
  if (tx >= 200 && tx <= 300 && ty >= 190 && ty <= 235) {
    Serial.println(">>> SAVE Filter");
    filterAnzahl = tempFilterAnzahl;
    filterSchwelle = tempFilterSchwelle;
    prefs.putInt("filterAnz", filterAnzahl);
    prefs.putInt("filterSchw", filterSchwelle);
    Serial.print("[OK] Filter gespeichert: ");
    Serial.print(filterAnzahl);
    Serial.print(" Werte, ");
    Serial.print(filterSchwelle);
    Serial.println(" kg");
    menuState = MENU_NONE;
    displayNeedsFullRedraw = true;
    return;
  }
}

void handleTouchTaraConfirm(int tx, int ty) {
  // ESC Button (links, Y: 185-230)
  if (tx >= 20 && tx <= 120 && ty >= 185 && ty <= 230) {
    Serial.println(">>> TARA abgebrochen");
    menuState = MENU_SCHAUFEL;
    displayNeedsFullRedraw = true;
    return;
  }
  
  // SAVE Button (rechts, Y: 185-230)
  if (tx >= 200 && tx <= 300 && ty >= 185 && ty <= 230) {
    Serial.println(">>> TARA bestätigt");
    schaufelTara = schaufelRohwert;
    prefs.putFloat("tara", schaufelTara);
    // (v1.0.1) Filter zurücksetzen, sonst zeigt das Display alte Werte
    resetFilter();
    Serial.print("[OK] Tara gesetzt: ");
    Serial.println(schaufelTara, 3);
    menuState = MENU_NONE;
    displayNeedsFullRedraw = true;
    return;
  }
}

// ============================================
// DISPLAY HANDLING
// ============================================
void handleDisplay() {
  if (displayNeedsFullRedraw) {
    zeichneAktuellesMenu();
    displayNeedsFullRedraw = false;
    
    lastAdd2Anzeige = add2AnzeigeGewicht;
    lastAdd2DatenStatus = add2DatenVorhanden;
    lastWifiStatus = wifiStatus;
    lastSchaufelGewicht = schaufelGewichtGefiltert;
    lastSchaufelPosition = schaufelInPosition;
    lastButtonsEnabled = add2DatenVorhanden;
    lastWaageOff = add2WaageOff;
    return;
  }
  
  if (menuState == MENU_NONE) {
    updateHauptbildschirm();
  }
}

void zeichneAktuellesMenu() {
  tft.fillScreen(TFT_BLACK);
  
  switch (menuState) {
    case MENU_NONE:
      zeichneHauptbildschirm();
      break;
    case MENU_SCHAUFEL:
      zeichneSchaufelMenu();
      break;
    case MENU_KALIBRIEREN:
      zeichneKalibrierMenu();
      break;
    case MENU_CONFIRM_TARA:
      zeichneTaraConfirm();
      break;
    case MENU_FILTER:
      zeichneFilterMenu();
      break;
  }
}

// ============================================
// HAUPTBILDSCHIRM
// ============================================
void zeichneHauptbildschirm() {
  zeichneSchaufelButton();
  zeichneWiFiStatus();
  tft.drawLine(0, 155, 320, 155, TFT_DARKGREY);
  zeichneAdd2Gewicht();
  zeichneButton(BTN1_X, BTN1_Y, "TOTAL", !add2ZeroModus, add2DatenVorhanden && !add2WaageOff);
  zeichneButton(BTN2_X, BTN2_Y, "ZERO", add2ZeroModus, add2DatenVorhanden && !add2WaageOff);
}

void updateHauptbildschirm() {
  // ADD2 Gewicht berechnen
  if (!add2DatenVorhanden || add2WaageOff) {
    add2AnzeigeGewicht = -9999;
  } else if (add2ZeroModus) {
    add2AnzeigeGewicht = add2Gewicht - add2Offset;
  } else {
    add2AnzeigeGewicht = add2Gewicht;
  }
  
  // Schaufel-Button updaten (mit gefiltertem Gewicht!)
  if (schaufelGewichtGefiltert != lastSchaufelGewicht || schaufelInPosition != lastSchaufelPosition) {
    zeichneSchaufelButton();
    lastSchaufelGewicht = schaufelGewichtGefiltert;
    lastSchaufelPosition = schaufelInPosition;
  }
  
  // WiFi Status updaten
  if (wifiStatus != lastWifiStatus) {
    zeichneWiFiStatus();
    lastWifiStatus = wifiStatus;
  }
  
  // ADD2 Gewicht updaten (auch bei OFF-Wechsel!)
  if (add2AnzeigeGewicht != lastAdd2Anzeige || add2DatenVorhanden != lastAdd2DatenStatus || add2WaageOff != lastWaageOff) {
    zeichneAdd2Gewicht();
    lastAdd2Anzeige = add2AnzeigeGewicht;
    lastAdd2DatenStatus = add2DatenVorhanden;
    lastWaageOff = add2WaageOff;
  }
  
  // Buttons updaten wenn Daten-Status oder OFF-Status sich ändert
  bool buttonsEnabled = add2DatenVorhanden && !add2WaageOff;
  if (buttonsEnabled != lastButtonsEnabled) {
    zeichneButton(BTN1_X, BTN1_Y, "TOTAL", !add2ZeroModus, buttonsEnabled);
    zeichneButton(BTN2_X, BTN2_Y, "ZERO", add2ZeroModus, buttonsEnabled);
    lastButtonsEnabled = buttonsEnabled;
  }
}

void zeichneSchaufelButton() {
  uint16_t bgColor = schaufelInPosition ? TFT_DARKERGREY : TFT_DARKGREY;
  
  tft.fillRoundRect(SCHAUFEL_BTN_X, SCHAUFEL_BTN_Y, SCHAUFEL_BTN_W, SCHAUFEL_BTN_H, 5, bgColor);
  tft.drawRoundRect(SCHAUFEL_BTN_X, SCHAUFEL_BTN_Y, SCHAUFEL_BTN_W, SCHAUFEL_BTN_H, 5, TFT_WHITE);
  
  tft.setTextSize(1);
  tft.setFreeFont(&FreeSansBold9pt7b);
  
  // (v1.0.1) Schaufel-Button zeigt jetzt auch ADS-Ausfall
  if (!adsVorhanden) {
    tft.setTextColor(TFT_RED);
    tft.setCursor(SCHAUFEL_BTN_X + 10, SCHAUFEL_BTN_Y + 24);
    tft.print("Schaufel: SENSOR-FEHLER");
  } else if (!schaufelInPosition) {
    tft.setTextColor(TFT_YELLOW);
    tft.setCursor(SCHAUFEL_BTN_X + 10, SCHAUFEL_BTN_Y + 24);
    tft.print("Schaufel: POSITION!");
  } else {
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(SCHAUFEL_BTN_X + 10, SCHAUFEL_BTN_Y + 24);
    tft.print("Schaufel: ");
    tft.print(schaufelGewichtGefiltert);  // Gefiltertes Gewicht!
    tft.print(" kg");
  }
  
  tft.setFreeFont(NULL);
}

void zeichneWiFiStatus() {
  int x = 300;
  int y = 15;
  int r = 8;
  
  uint16_t farbe = TFT_DARKGREY;  // Default falls switch nichts trifft
  switch (wifiStatus) {
    case WIFI_OK: farbe = TFT_GREEN; break;
    case WIFI_SUCHE: farbe = TFT_ORANGE; break;
    case WIFI_FEHLER: farbe = TFT_RED; break;
  }
  
  tft.fillCircle(x, y, r + 2, TFT_BLACK);
  tft.fillCircle(x, y, r, farbe);
}

void zeichneAdd2Gewicht() {
  tft.fillRect(0, ADD2_Y, 320, ADD2_H, TFT_BLACK);  // Volle Breite 320!
  
  char buf[12];
  uint16_t textColor;
  bool istText = false;
  
  if (!add2DatenVorhanden) {
    // Keine Verbindung zum Sender
    istText = true;
    if (wifiStatus == WIFI_FEHLER) {
      sprintf(buf, "OFFLINE");
      textColor = TFT_RED;
    } else if (wifiStatus == WIFI_SUCHE) {
      sprintf(buf, "SUCHE..");
      textColor = TFT_ORANGE;
    } else {
      sprintf(buf, "- - - -");
      textColor = TFT_DARKGREY;
      istText = false;
    }
  } else if (add2WaageOff) {
    // Verbindung da, aber Waage ist aus
    istText = true;
    sprintf(buf, "OFF");
    textColor = TFT_RED;
  } else {
    // Normale Gewichtsanzeige
    sprintf(buf, "%d", add2AnzeigeGewicht);
    textColor = TFT_WHITE;
  }
  
  tft.setTextColor(textColor);
  
  if (istText) {
    tft.setFreeFont(&FreeSansBold24pt7b);
    tft.setTextSize(1);
    int16_t w = tft.textWidth(buf);
    int16_t x = (320 - w) / 2;  // Zentriert auf 320px!
    tft.setCursor(x, ADD2_Y + 65);
    tft.print(buf);
    tft.setFreeFont(NULL);
  } else {
    tft.setTextSize(1);
    tft.setTextFont(8);
    int16_t w = tft.textWidth(buf);
    int16_t x = (320 - w) / 2;  // Zentriert auf 320px!
    tft.setCursor(x, ADD2_Y + 15);
    tft.print(buf);
    tft.setTextFont(1);
  }
}

void zeichneButton(int x, int y, const char* text, bool aktiv, bool enabled) {
  uint16_t bgColor = enabled ? (aktiv ? TFT_ORANGE : TFT_DARKGREY) : TFT_DARKGREY;
  uint16_t borderColor = enabled ? (aktiv ? TFT_WHITE : TFT_DARKGREY) : TFT_DARKGREY;
  uint16_t textColor = enabled ? TFT_BLACK : TFT_DARKGREY;
  
  tft.fillRoundRect(x, y, BTN_W, BTN_H, 8, bgColor);
  tft.drawRoundRect(x, y, BTN_W, BTN_H, 8, borderColor);
  
  tft.setTextColor(textColor);
  tft.setTextSize(1);
  tft.setFreeFont(&FreeSansBold12pt7b);
  
  // (v1.0.1) Exakte Zentrierung über tft.textWidth() statt strlen()*14
  int16_t textW = tft.textWidth(text);
  int textX = x + (BTN_W - textW) / 2;
  tft.setCursor(textX, y + 33);
  tft.println(text);
  
  tft.setFreeFont(NULL);
}

// ============================================
// SCHAUFEL MENÜ (mit FILTER Button)
// ============================================
void zeichneSchaufelMenu() {
  // Titel
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setCursor(80, 30);
  tft.print("WAAGE SETUP");
  tft.setFreeFont(NULL);
  
  // Status
  tft.setTextSize(1);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setCursor(20, 47);
  // (v1.0.1) ADS-Ausfall im Setup anzeigen
  if (!adsVorhanden) {
    tft.setTextColor(TFT_RED);
    tft.print("SENSOR-FEHLER!");
  } else if (schaufelInPosition) {
    tft.setTextColor(TFT_GREEN);
    tft.print("In Wiegeposition");
  } else {
    tft.setTextColor(TFT_YELLOW);
    tft.print("NICHT in Wiegeposition!");
  }
  tft.setFreeFont(NULL);
  
  uint16_t btnColor = (schaufelInPosition && adsVorhanden) ? TFT_ORANGE : TFT_DARKGREY;
  
  // TARA
  tft.fillRoundRect(40, 55, 240, 32, 6, btnColor);
  tft.drawRoundRect(40, 55, 240, 32, 6, TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setCursor(130, 80);
  tft.print("TARA");
  
  // KALIBRIEREN
  tft.fillRoundRect(40, 97, 240, 32, 6, btnColor);
  tft.drawRoundRect(40, 97, 240, 32, 6, TFT_WHITE);
  tft.setCursor(90, 122);
  tft.print("KALIBRIEREN");
  
  // FILTER (immer aktiv)
  tft.fillRoundRect(40, 139, 240, 32, 6, TFT_ORANGE);
  tft.drawRoundRect(40, 139, 240, 32, 6, TFT_WHITE);
  tft.setCursor(125, 164);
  tft.print("FILTER");
  
  // ZURÜCK
  tft.fillRoundRect(40, 181, 240, 32, 6, TFT_DARKGREY);
  tft.drawRoundRect(40, 181, 240, 32, 6, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(115, 206);
  tft.print("ZURUCK");
  
  tft.setFreeFont(NULL);
  
  // Aktuelle Werte
  tft.setTextColor(TFT_LIGHTGREY);
  tft.setTextSize(1);
  tft.setCursor(20, 232);
  tft.print("T:");
  tft.print(schaufelTara, 1);
  tft.print("V F:");
  tft.print((int)schaufelFaktor);
  tft.print(" M:");
  tft.print(filterAnzahl);
  tft.print(" S:");
  tft.print(filterSchwelle);
}

// ============================================
// KALIBRIEREN MENÜ
// ============================================
void zeichneKalibrierMenu() {
  // Titel
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setCursor(70, 30);
  tft.print("KALIBRIEREN");
  tft.setFreeFont(NULL);
  
  // Status
  tft.setTextSize(1);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setCursor(20, 50);
  if (schaufelInPosition) {
    tft.setTextColor(TFT_GREEN);
    tft.print("In Wiegeposition");
  } else {
    tft.setTextColor(TFT_YELLOW);
    tft.print("NICHT in Wiegeposition!");
  }
  tft.setFreeFont(NULL);
  
  // Gewichtsanzeige
  tft.setFreeFont(&FreeSansBold24pt7b);
  tft.setTextColor(TFT_WHITE);
  char buf[10];
  sprintf(buf, "%d", kalibierGewicht);
  int16_t w = tft.textWidth(buf);
  tft.setCursor((320 - w) / 2 - 20, 95);
  tft.print(buf);
  tft.setTextSize(1);
  tft.print(" kg");
  tft.setFreeFont(NULL);
  
  // -10 Button
  tft.fillRoundRect(30, 110, 100, 50, 8, TFT_ORANGE);
  tft.drawRoundRect(30, 110, 100, 50, 8, TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
  tft.setFreeFont(&FreeSansBold18pt7b);
  tft.setCursor(55, 148);
  tft.print("-10");
  
  // +10 Button
  tft.fillRoundRect(190, 110, 100, 50, 8, TFT_ORANGE);
  tft.drawRoundRect(190, 110, 100, 50, 8, TFT_WHITE);
  tft.setCursor(210, 148);
  tft.print("+10");
  tft.setFreeFont(NULL);
  
  // ESC Button
  tft.fillRoundRect(20, 185, 100, 45, 8, TFT_RED);
  tft.drawRoundRect(20, 185, 100, 45, 8, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setCursor(50, 218);
  tft.print("ESC");
  
  // SAVE Button
  tft.fillRoundRect(200, 185, 100, 45, 8, TFT_GREEN);
  tft.drawRoundRect(200, 185, 100, 45, 8, TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(220, 218);
  tft.print("SAVE");
  
  tft.setFreeFont(NULL);
  
  // Spannung
  tft.setTextColor(TFT_LIGHTGREY);
  tft.setTextSize(1);
  tft.setCursor(110, 175);
  tft.print("U: ");
  tft.print(schaufelRohwert, 3);
  tft.print(" V");
}

// ============================================
// FILTER MENÜ
// ============================================
void zeichneFilterMenu() {
  // Titel
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setCursor(110, 30);
  tft.print("FILTER");
  tft.setFreeFont(NULL);
  
  // --- Anzahl Werte Zeile ---
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setCursor(85, 65);
  tft.print("Anzahl Werte:");
  tft.setFreeFont(NULL);
  
  // -5 Button (Y: 75-110)
  tft.fillRoundRect(20, 75, 50, 35, 6, TFT_ORANGE);
  tft.drawRoundRect(20, 75, 50, 35, 6, TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setCursor(35, 102);
  tft.print("-5");
  
  // Wert
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeSansBold18pt7b);
  char buf[10];
  sprintf(buf, "%d", tempFilterAnzahl);
  int16_t w = tft.textWidth(buf);
  tft.setCursor(160 - w/2, 105);
  tft.print(buf);
  tft.setFreeFont(NULL);
  
  // +5 Button (Y: 75-110)
  tft.fillRoundRect(250, 75, 50, 35, 6, TFT_ORANGE);
  tft.drawRoundRect(250, 75, 50, 35, 6, TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setCursor(260, 102);
  tft.print("+5");
  
  // --- Schwelle Zeile ---
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.setCursor(100, 130);
  tft.print("Schwelle:");
  tft.setFreeFont(NULL);
  
  // -1 Button (Y: 140-175)
  tft.fillRoundRect(20, 140, 50, 35, 6, TFT_ORANGE);
  tft.drawRoundRect(20, 140, 50, 35, 6, TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setCursor(38, 167);
  tft.print("-1");
  
  // Wert
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeSansBold18pt7b);
  sprintf(buf, "%d", tempFilterSchwelle);
  w = tft.textWidth(buf);
  tft.setCursor(160 - w/2, 170);
  tft.print(buf);
  tft.setFreeFont(&FreeSans9pt7b);
  tft.print(" kg");
  tft.setFreeFont(NULL);
  
  // +1 Button (Y: 140-175)
  tft.fillRoundRect(250, 140, 50, 35, 6, TFT_ORANGE);
  tft.drawRoundRect(250, 140, 50, 35, 6, TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setCursor(260, 167);
  tft.print("+1");
  tft.setFreeFont(NULL);
  
  // ESC Button (Y: 190-232)
  tft.fillRoundRect(20, 190, 100, 42, 8, TFT_RED);
  tft.drawRoundRect(20, 190, 100, 42, 8, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setCursor(50, 220);
  tft.print("ESC");
  
  // SAVE Button (Y: 190-232)
  tft.fillRoundRect(200, 190, 100, 42, 8, TFT_GREEN);
  tft.drawRoundRect(200, 190, 100, 42, 8, TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(220, 220);
  tft.print("SAVE");
  
  tft.setFreeFont(NULL);
}

// ============================================
// TARA BESTÄTIGUNG
// ============================================
void zeichneTaraConfirm() {
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setCursor(120, 40);
  tft.print("TARA");
  tft.setFreeFont(NULL);
  
  tft.setFreeFont(&FreeSans12pt7b);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(70, 90);
  tft.print("Schaufel leer?");
  tft.setFreeFont(NULL);
  
  tft.setTextColor(TFT_LIGHTGREY);
  tft.setTextSize(1);
  tft.setCursor(70, 120);
  tft.print("Aktuell: ");
  tft.print(schaufelRohwert, 3);
  tft.print(" V");
  
  // ESC (links, gleiche Position wie andere Menüs)
  tft.fillRoundRect(20, 185, 100, 45, 8, TFT_RED);
  tft.drawRoundRect(20, 185, 100, 45, 8, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setFreeFont(&FreeSansBold12pt7b);
  tft.setCursor(50, 218);
  tft.print("ESC");
  
  // SAVE (rechts, gleiche Position wie andere Menüs)
  tft.fillRoundRect(200, 185, 100, 45, 8, TFT_GREEN);
  tft.drawRoundRect(200, 185, 100, 45, 8, TFT_WHITE);
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(220, 218);
  tft.print("SAVE");
  
  tft.setFreeFont(NULL);
}
