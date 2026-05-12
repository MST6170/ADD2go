#include "display.h"
#include "config.h"
#include "state.h"
#include "flow_tab.h"
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <math.h>

static TFT_eSPI tft = TFT_eSPI();

// Layout-Konstanten 1:1 wie v1.0.2 — kein Tab-Bar-Shift. Stattdessen
// rechts-oben ein quadratischer Tab-Switch-Button (siehe TAB_SWITCH_*).
static constexpr int SCHAUFEL_BTN_X = 5;
static constexpr int SCHAUFEL_BTN_Y = 5;
static constexpr int SCHAUFEL_BTN_W = 220;
static constexpr int SCHAUFEL_BTN_H = 35;

static constexpr int ADD2_Y = 50;
static constexpr int ADD2_H = 100;

// Quadratischer Tab-Switch-Button rechts oben, unter Signal-Bars (y>27)
// Zeigt das ZIEL des Switches: Wassertropfen = Wechsel zu Flow, "ADD2" = zurueck.
static constexpr int TAB_SWITCH_X = 283;
static constexpr int TAB_SWITCH_Y = 30;
static constexpr int TAB_SWITCH_W = 35;
static constexpr int TAB_SWITCH_H = 35;

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

// Wassertropfen-Symbol (dunkelblau): Spitze oben, runder Bauch unten.
// Etwas groesser als zuvor (r=6, total-Hoehe 17), mittig im Button-Center
// (cy zeigt auf Bauch-Center, deshalb das ganze Symbol leicht nach unten shiften).
static void drawWaterdrop(int cx, int cy, uint16_t color) {
    constexpr int r = 6;        // Bauch-Radius
    constexpr int upper = 11;   // Distanz Spitze ueber Bauch-Center
    int top    = cy - upper;
    int leftX  = cx - r;
    int rightX = cx + r;
    tft.fillTriangle(cx, top, leftX, cy, rightX, cy, color);
    tft.fillCircle(cx, cy, r, color);
}

void zeichneTabSwitchButton() {
    tft.fillRoundRect(TAB_SWITCH_X, TAB_SWITCH_Y, TAB_SWITCH_W, TAB_SWITCH_H, 5, TFT_DARKGREY);
    tft.drawRoundRect(TAB_SWITCH_X, TAB_SWITCH_Y, TAB_SWITCH_W, TAB_SWITCH_H, 5, TFT_WHITE);

    int cx = TAB_SWITCH_X + TAB_SWITCH_W / 2;
    int cy = TAB_SWITCH_Y + TAB_SWITCH_H / 2;

    if (currentTab == TAB_SCHAUFEL) {
        // cy ist Bauch-Center, Spitze geht 11 px hoch + Bauch r=6 nach unten.
        // Symbol-Vertikal-Mitte = (cy - upper + cy + r) / 2 = cy - (upper - r) / 2.
        // Damit Symbol-Mitte == Button-Center: dropCy = cy + (upper - r) / 2 = cy + 2.
        drawWaterdrop(cx, cy + 2, TFT_NAVY);
    } else {
        // "ADD" oben, "2" unten, rubinrot, kleine Schrift (Font 2 statt FreeSans9pt)
        constexpr uint16_t RUBY = 0x9883;
        tft.setTextColor(RUBY);
        tft.setTextDatum(MC_DATUM);
        tft.setTextFont(2);
        tft.drawString("ADD", cx, cy - 7);
        tft.drawString("2",   cx, cy + 7);
        tft.setTextFont(1);
        tft.setTextDatum(TL_DATUM);
    }
}

void zeichneFlowTabPlaceholder() {
    tft.setTextColor(TFT_LIGHTGREY);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSans12pt7b);
    tft.drawString("Flow-Tab", 160, 100);
    tft.setFreeFont(&FreeSans9pt7b);
    tft.drawString("folgt in Schritt 5", 160, 130);
    tft.setFreeFont(NULL);
    tft.setTextDatum(TL_DATUM);
}

// ---- Phase 3c Schritt 5: Flow-Tab Rendering ----

static uint16_t stateBadgeColor(flow::FlowState s) {
    switch (s) {
        case flow::FS_FILLING:        return TFT_ORANGE;
        case flow::FS_PAUSED:         return TFT_YELLOW;
        case flow::FS_DONE:           return TFT_GREEN;
        case flow::FS_ERROR:          return TFT_RED;
        case flow::FS_RESUME_PROMPT:  return TFT_BLUE;
        case flow::FS_IDLE:
        case flow::FS_SETTINGS:
        case flow::FS_SETTINGS_ADV:
        case flow::FS_CALIBRATING:    return TFT_DARKGREY;
        default:                      return TFT_DARKGREY;
    }
}

static const char* stateBadgeText(flow::FlowState s) {
    switch (s) {
        case flow::FS_IDLE:           return "BEREIT";
        case flow::FS_FILLING:        return "BEFUELLT";
        case flow::FS_PAUSED:         return "PAUSE";
        case flow::FS_DONE:           return "FERTIG";
        case flow::FS_ABORTED:        return "ABBRUCH";
        case flow::FS_ERROR:          return "FEHLER";
        case flow::FS_RESUME_PROMPT:  return "FORTSETZEN?";
        case flow::FS_SETTINGS:       return "EINSTELLUNG";
        case flow::FS_SETTINGS_ADV:   return "EINSTELLUNG";
        case flow::FS_CALIBRATING:    return "KALIBRIERUNG";
        default:                      return "...";
    }
}

void zeichneFlowSpinner() {
    tft.setTextColor(TFT_LIGHTGREY);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSans12pt7b);
    tft.drawString("Verbinde zu add2flow...", 160, 100);
    tft.setFreeFont(&FreeSans9pt7b);
    tft.drawString("(bitte warten)", 160, 135);
    tft.setFreeFont(NULL);
    tft.setTextDatum(TL_DATUM);
}

void zeichneFlowFallback() {
    tft.setTextColor(TFT_RED);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.drawString("add2flow nicht erreichbar", 160, 90);
    tft.setFreeFont(&FreeSans9pt7b);
    tft.setTextColor(TFT_LIGHTGREY);
    tft.drawString("Strom + WLAN pruefen", 160, 125);
    tft.drawString("oder oben rechts ADD2 antippen", 160, 150);
    tft.setFreeFont(NULL);
    tft.setTextDatum(TL_DATUM);
}

void zeichneFlowConfirmModal() {
    // Vollbild-Overlay (ueberzeichnet alles unter Header — auch den Tab-Switch
    // ist OK, da Tab-Switch nach Modal wieder gezeichnet wird)
    tft.fillRect(0, 0, 320, 240, TFT_BLACK);

    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSansBold18pt7b);
    tft.drawString("Sind Sie sicher?", 160, 50);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.setTextColor(TFT_YELLOW);
    tft.drawString("Tank pruefen!", 160, 90);

    uint8_t pp = flow::pendingConfirmPreset();
    char line[32];
    snprintf(line, sizeof(line), "Preset %u starten?", (unsigned)pp);
    tft.setTextColor(TFT_LIGHTGREY);
    tft.setFreeFont(&FreeSans12pt7b);
    tft.drawString(line, 160, 125);

    // Ja-Button gruen links, Abbruch rot rechts
    tft.fillRoundRect(20, 170, 130, 50, 8, TFT_GREEN);
    tft.drawRoundRect(20, 170, 130, 50, 8, TFT_WHITE);
    tft.setTextColor(TFT_BLACK);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.drawString("Ja, start", 85, 195);

    tft.fillRoundRect(170, 170, 130, 50, 8, TFT_RED);
    tft.drawRoundRect(170, 170, 130, 50, 8, TFT_WHITE);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Abbruch", 235, 195);

    tft.setFreeFont(NULL);
    tft.setTextDatum(TL_DATUM);
}

static void drawFlowPresetBtn(int x, int y, int w, int h, uint16_t liter,
                              uint8_t presetIdx, bool active, bool enabled) {
    (void)presetIdx;
    uint16_t bg = enabled ? (active ? TFT_ORANGE : TFT_DARKGREY) : TFT_DARKGREY;
    uint16_t fg = enabled ? (active ? TFT_BLACK  : TFT_WHITE)    : TFT_DARKGREY;
    tft.fillRoundRect(x, y, w, h, 6, bg);
    tft.drawRoundRect(x, y, w, h, 6, TFT_WHITE);

    // Nur die Zahl ohne "L" — passt sicher in 70 px breite Buttons.
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", (unsigned)liter);
    tft.setTextColor(fg);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.drawString(buf, x + w / 2, y + h / 2);
    tft.setFreeFont(NULL);
    tft.setTextDatum(TL_DATUM);
}

// State-Tracking fuer Flow-Tab partial-redraw. Werte werden bei
// zeichneFlowTab() auf "ungueltig" zurueckgesetzt damit der naechste
// updateFlowTab()-Call alles neu zeichnet.
static uint16_t        s_lastFlowDone       = 0xFFFF;
static uint16_t        s_lastFlowTarget     = 0xFFFF;
static float           s_lastFlowLpm        = -1.0f;
static flow::FlowState s_lastFlowState      = flow::FS_UNKNOWN;
static uint8_t         s_lastFlowActivePr   = 0xFF;
static uint16_t        s_lastFlowP1         = 0xFFFF;
static uint16_t        s_lastFlowP2         = 0xFFFF;
static uint16_t        s_lastFlowP3         = 0xFFFF;
static bool            s_lastFlowConnected  = false;

static void drawFlowLiters(uint16_t done, uint16_t target, bool connected) {
    // fillRect nur bis x=280 — der Tab-Switch-Button (x=283..318, y=30..65)
    // ueberlappt y=50..65 und wuerde sonst seine untere Haelfte verlieren.
    tft.fillRect(0, 50, 280, 50, TFT_BLACK);
    char buf[20];
    snprintf(buf, sizeof(buf), "%u / %u", (unsigned)done, (unsigned)target);
    tft.setTextColor(connected ? TFT_WHITE : TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSansBold24pt7b);
    tft.drawString(buf, 160, 75);
    tft.setFreeFont(NULL);
    tft.setTextDatum(TL_DATUM);
}

static void drawFlowLpm(float lpm, bool connected) {
    tft.fillRect(0, 108, 320, 22, TFT_BLACK);
    char buf[18];
    snprintf(buf, sizeof(buf), "%.1f L/min", lpm);
    tft.setTextColor(connected ? TFT_LIGHTGREY : TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSans9pt7b);
    tft.drawString(buf, 160, 120);
    tft.setFreeFont(NULL);
    tft.setTextDatum(TL_DATUM);
}

static void drawFlowBadge(flow::FlowState s, bool connected) {
    constexpr int badgeW = 130;
    const int badgeX = (320 - badgeW) / 2;
    tft.fillRect(0, 132, 320, 35, TFT_BLACK);
    uint16_t bg = connected ? stateBadgeColor(s) : TFT_DARKGREY;
    uint16_t fg = connected ? TFT_BLACK : TFT_DARKERGREY;
    tft.fillRoundRect(badgeX, 138, badgeW, 22, 10, bg);
    tft.setTextColor(fg);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSansBold9pt7b);
    tft.drawString(stateBadgeText(s), 160, 149);
    if (!connected) {
        tft.setFreeFont(NULL);
        tft.setTextColor(TFT_RED);
        tft.drawString("Verbindung verloren", 160, 168);
    }
    tft.setFreeFont(NULL);
    tft.setTextDatum(TL_DATUM);
}

static void drawFlowButtons(const flow::Snapshot& snap, bool connected) {
    tft.fillRect(0, 175, 320, 60, TFT_BLACK);
    drawFlowPresetBtn( 10, 180,  70, 50, snap.p1, 1, snap.preset == 1, connected);
    drawFlowPresetBtn( 85, 180,  70, 50, snap.p2, 2, snap.preset == 2, connected);
    drawFlowPresetBtn(160, 180,  70, 50, snap.p3, 3, snap.preset == 3, connected);

    uint16_t stopBg = connected ? TFT_RED : TFT_DARKGREY;
    tft.fillRoundRect(235, 180, 75, 50, 6, stopBg);
    tft.drawRoundRect(235, 180, 75, 50, 6, TFT_WHITE);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(&FreeSansBold12pt7b);
    tft.drawString("STOP", 272, 205);
    tft.setFreeFont(NULL);
    tft.setTextDatum(TL_DATUM);
}

// Wird bei Full-Redraw (Tab-Wechsel) aufgerufen. Reset Tracking-Vars
// damit updateHauptbildschirm() im naechsten Tick alles neu zeichnet.
void zeichneFlowTab() {
    auto snap = flow::snapshot();
    bool connected = flow::wsConnected();

    drawFlowLiters(snap.doneL, snap.targetL, connected);
    drawFlowLpm(snap.lpm, connected);
    drawFlowBadge(snap.state, connected);
    drawFlowButtons(snap, connected);

    s_lastFlowDone      = snap.doneL;
    s_lastFlowTarget    = snap.targetL;
    s_lastFlowLpm       = snap.lpm;
    s_lastFlowState     = snap.state;
    s_lastFlowActivePr  = snap.preset;
    s_lastFlowP1        = snap.p1;
    s_lastFlowP2        = snap.p2;
    s_lastFlowP3        = snap.p3;
    s_lastFlowConnected = connected;
}

// Partial redraw — nur was sich geaendert hat. Verhindert 10 Hz Flicker.
static void updateFlowTab() {
    auto snap = flow::snapshot();
    bool connected = flow::wsConnected();

    // Connection-Toggle -> Full redraw aller Felder mit neuer Farbgebung.
    if (connected != s_lastFlowConnected) {
        drawFlowLiters(snap.doneL, snap.targetL, connected);
        drawFlowLpm(snap.lpm, connected);
        drawFlowBadge(snap.state, connected);
        drawFlowButtons(snap, connected);
        s_lastFlowConnected = connected;
        s_lastFlowDone = snap.doneL; s_lastFlowTarget = snap.targetL;
        s_lastFlowLpm = snap.lpm; s_lastFlowState = snap.state;
        s_lastFlowActivePr = snap.preset;
        s_lastFlowP1 = snap.p1; s_lastFlowP2 = snap.p2; s_lastFlowP3 = snap.p3;
        return;
    }

    if (snap.doneL != s_lastFlowDone || snap.targetL != s_lastFlowTarget) {
        drawFlowLiters(snap.doneL, snap.targetL, connected);
        s_lastFlowDone = snap.doneL;
        s_lastFlowTarget = snap.targetL;
    }

    // L/min: Threshold 0.05 verhindert Update bei winzigem Float-Jitter.
    if (fabsf(snap.lpm - s_lastFlowLpm) > 0.05f) {
        drawFlowLpm(snap.lpm, connected);
        s_lastFlowLpm = snap.lpm;
    }

    if (snap.state != s_lastFlowState) {
        drawFlowBadge(snap.state, connected);
        s_lastFlowState = snap.state;
    }

    if (snap.preset != s_lastFlowActivePr ||
        snap.p1 != s_lastFlowP1 || snap.p2 != s_lastFlowP2 || snap.p3 != s_lastFlowP3) {
        drawFlowButtons(snap, connected);
        s_lastFlowActivePr = snap.preset;
        s_lastFlowP1 = snap.p1;
        s_lastFlowP2 = snap.p2;
        s_lastFlowP3 = snap.p3;
    }
}

void zeichneHauptbildschirm() {
    zeichneWiFiStatus();
    zeichneSignalBars(signalBars);

    if (currentTab == TAB_FLOW) {
        // Confirm-Modal hat hoechste Prioritaet
        if (flow::pendingConfirmPreset() > 0) {
            zeichneFlowConfirmModal();
            zeichneTabSwitchButton();
            return;
        }
        if (flow::staSwitchTimedOut()) {
            zeichneFlowFallback();
        } else if (flow::staSwitchInProgress() || !flow::wsConnected()) {
            // Spinner solange noch keine Frames eingegangen sind
            if (flow::staSwitchInProgress()) {
                zeichneFlowSpinner();
            } else {
                zeichneFlowTab();   // mit Greying da !wsConnected
            }
        } else {
            zeichneFlowTab();
        }
        zeichneTabSwitchButton();
        return;
    }

    // Schaufel-Tab — klassisches v1.0.2-Hauptbildschirm-Layout
    zeichneSchaufelButton();
    tft.drawLine(0, 155, 320, 155, TFT_DARKGREY);
    zeichneAdd2Gewicht();
    zeichneButton(BTN1_X, BTN1_Y, "TOTAL", !add2ZeroModus, add2DatenVorhanden && !add2WaageOff);
    zeichneButton(BTN2_X, BTN2_Y, "ZERO",  add2ZeroModus,  add2DatenVorhanden && !add2WaageOff);
    // Tab-Switch ZULETZT, sonst loescht zeichneAdd2Gewicht (y=50..150) die untere Haelfte.
    zeichneTabSwitchButton();
}

void updateHauptbildschirm() {
    // Flow-Tab: bei aktivem Modal/Spinner/Fallback nicht ueberzeichnen.
    if (currentTab == TAB_FLOW) {
        if (flow::pendingConfirmPreset() > 0) return;
        if (flow::staSwitchInProgress() || flow::staSwitchTimedOut()) return;
        // Selective redraw — vermeidet 10 Hz Flicker. Tab-Switch bleibt unberuehrt
        // (wird nur bei full-redraw / Modal-Exit neu gezeichnet).
        updateFlowTab();
        return;
    }

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
        // Tab-Switch wird vom fillRect in zeichneAdd2Gewicht in der unteren Haelfte ueberschrieben — neu zeichnen.
        zeichneTabSwitchButton();
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
