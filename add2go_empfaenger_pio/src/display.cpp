#include "display.h"
#include "config.h"
#include "state.h"
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>

static TFT_eSPI tft = TFT_eSPI();

// Layout-Konstanten 1:1 aus Sketch Z. 84-97
static constexpr int SCHAUFEL_BTN_X = 5;
static constexpr int SCHAUFEL_BTN_Y = 5;
static constexpr int SCHAUFEL_BTN_W = 220;
static constexpr int SCHAUFEL_BTN_H = 35;

static constexpr int ADD2_Y = 50;
static constexpr int ADD2_H = 100;

static constexpr int BTN1_X = 20;
static constexpr int BTN1_Y = 180;
static constexpr int BTN2_X = 170;
static constexpr int BTN2_Y = 180;
static constexpr int BTN_W  = 130;
static constexpr int BTN_H  = 50;

// TFT_DARKERGREY ist nicht in TFT_eSPI definiert
#ifndef TFT_DARKERGREY
#define TFT_DARKERGREY 0x4208
#endif

// Boot-Screen-Werte (gemeinsam mit bootProgress geteilt)
static int s_barX = 15;
static int s_barY = 100;
static int s_barW = 290;
static int s_barH = 25;
static int s_statusY = 140;

namespace display {

void begin() {
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    Serial.println("[OK] Display initialisiert");
}

void zeichneBootScreen() {
    // Titel "ADD2go"
    tft.setFreeFont(&FreeSansBold18pt7b);
    tft.setTextColor(TFT_WHITE);
    String titel = "ADD2go";
    int titelW = tft.textWidth(titel);
    tft.setCursor((320 - titelW) / 2, 50);
    tft.print(titel);
    tft.setFreeFont(NULL);

    // Fortschrittsbalken-Rahmen
    tft.drawRoundRect(s_barX, s_barY, s_barW, s_barH, 5, TFT_WHITE);
    s_statusY = s_barY + s_barH + 15;
}

void bootProgress(int prozent, const char* text) {
    int fillW = (s_barW - 4) * prozent / 100;
    tft.fillRoundRect(s_barX + 2, s_barY + 2, fillW, s_barH - 4, 3, TFT_GREEN);

    tft.fillRect(s_barX, s_statusY - 5, s_barW, 20, TFT_BLACK);
    tft.setTextColor(TFT_LIGHTGREY);
    tft.setTextSize(1);
    tft.setCursor(s_barX, s_statusY);
    tft.print(text);

    delay(200);
}

void bootWifiResult(bool connected) {
    if (connected) {
        bootProgress(100, "WiFi OK!");
    } else {
        tft.fillRoundRect(s_barX + 2, s_barY + 2, s_barW - 4, s_barH - 4, 3, TFT_ORANGE);
        tft.fillRect(s_barX, s_statusY - 5, s_barW, 20, TFT_BLACK);
        tft.setTextColor(TFT_ORANGE);
        tft.setCursor(s_barX, s_statusY);
        tft.print("WiFi nicht gefunden...");
    }
}

void bootCopyright() {
    delay(300);
    tft.setTextColor(TFT_DARKGREY);
    tft.setTextSize(1);
    String copyright = "(c) MST 2026 - v1.1-pio";
    int copyW = tft.textWidth(copyright);
    tft.setCursor((320 - copyW) / 2, 200);
    tft.print(copyright);
    delay(800);
}

// ---- Rendering ----

void zeichneSchaufelButton() {
    uint16_t bgColor = schaufelInPosition ? TFT_DARKERGREY : TFT_DARKGREY;

    tft.fillRoundRect(SCHAUFEL_BTN_X, SCHAUFEL_BTN_Y, SCHAUFEL_BTN_W, SCHAUFEL_BTN_H, 5, bgColor);
    tft.drawRoundRect(SCHAUFEL_BTN_X, SCHAUFEL_BTN_Y, SCHAUFEL_BTN_W, SCHAUFEL_BTN_H, 5, TFT_WHITE);

    tft.setTextSize(1);
    tft.setFreeFont(&FreeSansBold9pt7b);

    if (!adsVorhanden) {
        tft.setTextColor(TFT_RED);
        const char* msg = "SENSOR-FEHLER!";
        int16_t w = tft.textWidth(msg);
        tft.setCursor(SCHAUFEL_BTN_X + (SCHAUFEL_BTN_W - w) / 2, SCHAUFEL_BTN_Y + 24);
        tft.print(msg);
    } else if (!schaufelInPosition) {
        tft.setTextColor(TFT_YELLOW);
        tft.setCursor(SCHAUFEL_BTN_X + 10, SCHAUFEL_BTN_Y + 24);
        tft.print("Schaufel: POSITION!");
    } else {
        tft.setTextColor(TFT_WHITE);
        tft.setCursor(SCHAUFEL_BTN_X + 10, SCHAUFEL_BTN_Y + 24);
        tft.print("Schaufel: ");
        tft.print(schaufelGewichtGefiltert);
        tft.print(" kg");
    }

    tft.setFreeFont(NULL);
}

void zeichneWiFiStatus() {
    int x = 300;
    int y = 15;
    int r = 8;

    uint16_t farbe = TFT_DARKGREY;
    switch (wifiStatus) {
        case WIFI_OK:     farbe = TFT_GREEN;  break;
        case WIFI_SUCHE:  farbe = TFT_ORANGE; break;
        case WIFI_FEHLER: farbe = TFT_RED;    break;
    }
    tft.fillCircle(x, y, r + 2, TFT_BLACK);
    tft.fillCircle(x, y, r, farbe);
}

void zeichneSignalBars(int activeBars) {
    tft.fillRect(260, 5, 32, 22, TFT_BLACK);
    for (int i = 0; i < 4; i++) {
        int barH = (i + 1) * 4;
        int barW = 5;
        int barX = 264 + i * 7;
        int barY = 23 - barH;
        uint16_t color = (i < activeBars) ? TFT_GREEN : TFT_DARKERGREY;
        tft.fillRect(barX, barY, barW, barH, color);
    }
}

void zeichneAdd2Gewicht() {
    tft.fillRect(0, ADD2_Y, 320, ADD2_H, TFT_BLACK);

    char buf[12];
    uint16_t textColor;
    bool istText = false;

    if (!add2DatenVorhanden) {
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
        istText = true;
        sprintf(buf, "OFF");
        textColor = TFT_RED;
    } else {
        sprintf(buf, "%d", add2AnzeigeGewicht);
        textColor = TFT_WHITE;
    }

    tft.setTextColor(textColor);
    if (istText) {
        tft.setFreeFont(&FreeSansBold24pt7b);
        tft.setTextSize(1);
        int16_t w = tft.textWidth(buf);
        int16_t x = (320 - w) / 2;
        tft.setCursor(x, ADD2_Y + 65);
        tft.print(buf);
        tft.setFreeFont(NULL);
    } else {
        tft.setTextSize(1);
        tft.setTextFont(8);
        int16_t w = tft.textWidth(buf);
        int16_t x = (320 - w) / 2;
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

    int16_t textW = tft.textWidth(text);
    int textX = x + (BTN_W - textW) / 2;
    tft.setCursor(textX, y + 33);
    tft.println(text);

    tft.setFreeFont(NULL);
}

void zeichneHauptbildschirm() {
    zeichneSchaufelButton();
    zeichneWiFiStatus();
    zeichneSignalBars(signalBars);
    tft.drawLine(0, 155, 320, 155, TFT_DARKGREY);
    zeichneAdd2Gewicht();
    zeichneButton(BTN1_X, BTN1_Y, "TOTAL", !add2ZeroModus, add2DatenVorhanden && !add2WaageOff);
    zeichneButton(BTN2_X, BTN2_Y, "ZERO",  add2ZeroModus,  add2DatenVorhanden && !add2WaageOff);
}

void updateHauptbildschirm() {
    if (!add2DatenVorhanden || add2WaageOff) {
        add2AnzeigeGewicht = -9999;
    } else if (add2ZeroModus) {
        add2AnzeigeGewicht = add2Gewicht - add2Offset;
    } else {
        add2AnzeigeGewicht = add2Gewicht;
    }

    if (schaufelGewichtGefiltert != lastSchaufelGewicht || schaufelInPosition != lastSchaufelPosition) {
        zeichneSchaufelButton();
        lastSchaufelGewicht = schaufelGewichtGefiltert;
        lastSchaufelPosition = schaufelInPosition;
    }

    if (wifiStatus != lastWifiStatus) {
        zeichneWiFiStatus();
        lastWifiStatus = wifiStatus;
    }

    if (signalBars != lastSignalBars) {
        zeichneSignalBars(signalBars);
        lastSignalBars = signalBars;
    }

    if (add2AnzeigeGewicht != lastAdd2Anzeige || add2DatenVorhanden != lastAdd2DatenStatus || add2WaageOff != lastWaageOff) {
        zeichneAdd2Gewicht();
        lastAdd2Anzeige = add2AnzeigeGewicht;
        lastAdd2DatenStatus = add2DatenVorhanden;
        lastWaageOff = add2WaageOff;
    }

    bool buttonsEnabled = add2DatenVorhanden && !add2WaageOff;
    if (buttonsEnabled != lastButtonsEnabled) {
        zeichneButton(BTN1_X, BTN1_Y, "TOTAL", !add2ZeroModus, buttonsEnabled);
        zeichneButton(BTN2_X, BTN2_Y, "ZERO",  add2ZeroModus,  buttonsEnabled);
        lastButtonsEnabled = buttonsEnabled;
    }
}

// ---- Menu-Renders ----

void zeichneSchaufelMenu() {
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setCursor(80, 30);
    tft.print("WAAGE SETUP");
    tft.setFreeFont(NULL);

    tft.setTextSize(1);
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setCursor(20, 47);
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

    // FILTER
    tft.fillRoundRect(40, 139, 240, 32, 6, TFT_ORANGE);
    tft.drawRoundRect(40, 139, 240, 32, 6, TFT_WHITE);
    tft.setCursor(125, 164);
    tft.print("FILTER");

    // ZURUECK
    tft.fillRoundRect(40, 181, 240, 32, 6, TFT_DARKGREY);
    tft.drawRoundRect(40, 181, 240, 32, 6, TFT_WHITE);
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(115, 206);
    tft.print("ZURUCK");

    tft.setFreeFont(NULL);

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

void zeichneKalibrierMenu() {
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setCursor(70, 30);
    tft.print("KALIBRIEREN");
    tft.setFreeFont(NULL);

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

    tft.fillRoundRect(30, 110, 100, 50, 8, TFT_ORANGE);
    tft.drawRoundRect(30, 110, 100, 50, 8, TFT_WHITE);
    tft.setTextColor(TFT_BLACK);
    tft.setFreeFont(&FreeSansBold18pt7b);
    tft.setCursor(55, 148);
    tft.print("-10");

    tft.fillRoundRect(190, 110, 100, 50, 8, TFT_ORANGE);
    tft.drawRoundRect(190, 110, 100, 50, 8, TFT_WHITE);
    tft.setCursor(210, 148);
    tft.print("+10");
    tft.setFreeFont(NULL);

    tft.fillRoundRect(20, 185, 100, 45, 8, TFT_RED);
    tft.drawRoundRect(20, 185, 100, 45, 8, TFT_WHITE);
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setCursor(50, 218);
    tft.print("ESC");

    tft.fillRoundRect(200, 185, 100, 45, 8, TFT_GREEN);
    tft.drawRoundRect(200, 185, 100, 45, 8, TFT_WHITE);
    tft.setTextColor(TFT_BLACK);
    tft.setCursor(220, 218);
    tft.print("SAVE");

    tft.setFreeFont(NULL);

    tft.setTextColor(TFT_LIGHTGREY);
    tft.setTextSize(1);
    tft.setCursor(110, 175);
    tft.print("U: ");
    tft.print(schaufelRohwert, 3);
    tft.print(" V");
}

void zeichneFilterMenu() {
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setCursor(110, 30);
    tft.print("FILTER");
    tft.setFreeFont(NULL);

    // Anzahl Werte Zeile
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setCursor(85, 65);
    tft.print("Anzahl Werte:");
    tft.setFreeFont(NULL);

    tft.fillRoundRect(20, 75, 50, 35, 6, TFT_ORANGE);
    tft.drawRoundRect(20, 75, 50, 35, 6, TFT_WHITE);
    tft.setTextColor(TFT_BLACK);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setCursor(35, 102);
    tft.print("-5");

    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(&FreeSansBold18pt7b);
    char buf[10];
    sprintf(buf, "%d", tempFilterAnzahl);
    int16_t w = tft.textWidth(buf);
    tft.setCursor(160 - w / 2, 105);
    tft.print(buf);
    tft.setFreeFont(NULL);

    tft.fillRoundRect(250, 75, 50, 35, 6, TFT_ORANGE);
    tft.drawRoundRect(250, 75, 50, 35, 6, TFT_WHITE);
    tft.setTextColor(TFT_BLACK);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setCursor(260, 102);
    tft.print("+5");

    // Schwelle Zeile
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setCursor(100, 130);
    tft.print("Schwelle:");
    tft.setFreeFont(NULL);

    tft.fillRoundRect(20, 140, 50, 35, 6, TFT_ORANGE);
    tft.drawRoundRect(20, 140, 50, 35, 6, TFT_WHITE);
    tft.setTextColor(TFT_BLACK);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setCursor(38, 167);
    tft.print("-1");

    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(&FreeSansBold18pt7b);
    sprintf(buf, "%d", tempFilterSchwelle);
    w = tft.textWidth(buf);
    tft.setCursor(160 - w / 2, 170);
    tft.print(buf);
    tft.setFreeFont(&FreeSans9pt7b);
    tft.print(" kg");
    tft.setFreeFont(NULL);

    tft.fillRoundRect(250, 140, 50, 35, 6, TFT_ORANGE);
    tft.drawRoundRect(250, 140, 50, 35, 6, TFT_WHITE);
    tft.setTextColor(TFT_BLACK);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setCursor(260, 167);
    tft.print("+1");
    tft.setFreeFont(NULL);

    tft.fillRoundRect(20, 190, 100, 42, 8, TFT_RED);
    tft.drawRoundRect(20, 190, 100, 42, 8, TFT_WHITE);
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setCursor(50, 220);
    tft.print("ESC");

    tft.fillRoundRect(200, 190, 100, 42, 8, TFT_GREEN);
    tft.drawRoundRect(200, 190, 100, 42, 8, TFT_WHITE);
    tft.setTextColor(TFT_BLACK);
    tft.setCursor(220, 220);
    tft.print("SAVE");

    tft.setFreeFont(NULL);
}

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

    tft.fillRoundRect(20, 185, 100, 45, 8, TFT_RED);
    tft.drawRoundRect(20, 185, 100, 45, 8, TFT_WHITE);
    tft.setTextColor(TFT_WHITE);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setCursor(50, 218);
    tft.print("ESC");

    tft.fillRoundRect(200, 185, 100, 45, 8, TFT_GREEN);
    tft.drawRoundRect(200, 185, 100, 45, 8, TFT_WHITE);
    tft.setTextColor(TFT_BLACK);
    tft.setCursor(220, 218);
    tft.print("SAVE");

    tft.setFreeFont(NULL);
}

void zeichneAktuellesMenu() {
    tft.fillScreen(TFT_BLACK);
    switch (menuState) {
        case MENU_NONE:         zeichneHauptbildschirm(); break;
        case MENU_SCHAUFEL:     zeichneSchaufelMenu();    break;
        case MENU_KALIBRIEREN:  zeichneKalibrierMenu();   break;
        case MENU_CONFIRM_TARA: zeichneTaraConfirm();     break;
        case MENU_FILTER:       zeichneFilterMenu();      break;
    }
}

void handle() {
    if (displayNeedsFullRedraw) {
        zeichneAktuellesMenu();
        displayNeedsFullRedraw = false;

        lastAdd2Anzeige      = add2AnzeigeGewicht;
        lastAdd2DatenStatus  = add2DatenVorhanden;
        lastWifiStatus       = wifiStatus;
        lastSchaufelGewicht  = schaufelGewichtGefiltert;
        lastSchaufelPosition = schaufelInPosition;
        lastButtonsEnabled   = add2DatenVorhanden;
        lastWaageOff         = add2WaageOff;
        lastSignalBars       = signalBars;
        return;
    }

    if (menuState == MENU_NONE) {
        updateHauptbildschirm();
    }
}

} // namespace display
