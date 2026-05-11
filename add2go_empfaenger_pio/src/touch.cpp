#include "touch.h"
#include "config.h"
#include "state.h"
#include "storage.h"
#include "scale.h"
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

static XPT2046_Touchscreen ts(TOUCH_CS_PIN, TOUCH_IRQ);

// Layout-Konstanten 1:1 aus Sketch
static constexpr int SCHAUFEL_BTN_X = 5;
static constexpr int SCHAUFEL_BTN_Y = 5;
static constexpr int SCHAUFEL_BTN_W = 220;
static constexpr int SCHAUFEL_BTN_H = 35;
static constexpr int BTN1_X = 20;
static constexpr int BTN1_Y = 180;
static constexpr int BTN2_X = 170;
static constexpr int BTN2_Y = 180;
static constexpr int BTN_W  = 130;
static constexpr int BTN_H  = 50;

static void handleTouchHauptbildschirm(int tx, int ty);
static void handleTouchSchaufelMenu(int tx, int ty);
static void handleTouchKalibrieren(int tx, int ty);
static void handleTouchFilter(int tx, int ty);
static void handleTouchTaraConfirm(int tx, int ty);

namespace touch {

void begin() {
    ts.begin();
    ts.setRotation(1);
    Serial.println("[OK] Touch initialisiert");
}

void handle() {
    if (!ts.touched()) return;

    TS_Point p = ts.getPoint();
    int tx = map(p.x, 3900, 300, 0, 320);
    int ty = map(p.y, 3900, 300, 0, 240);

    delay(50);

    switch (menuState) {
        case MENU_NONE:         handleTouchHauptbildschirm(tx, ty); break;
        case MENU_SCHAUFEL:     handleTouchSchaufelMenu(tx, ty);    break;
        case MENU_KALIBRIEREN:  handleTouchKalibrieren(tx, ty);     break;
        case MENU_CONFIRM_TARA: handleTouchTaraConfirm(tx, ty);     break;
        case MENU_FILTER:       handleTouchFilter(tx, ty);          break;
    }

    delay(150);
}

} // namespace touch

// ---- Touch-Handler 1:1 aus Sketch ----

static void handleTouchHauptbildschirm(int tx, int ty) {
    if (tx >= SCHAUFEL_BTN_X && tx <= SCHAUFEL_BTN_X + SCHAUFEL_BTN_W &&
        ty >= SCHAUFEL_BTN_Y && ty <= SCHAUFEL_BTN_Y + SCHAUFEL_BTN_H) {
        Serial.println(">>> Schaufel-Button gedrueckt");
        menuState = MENU_SCHAUFEL;
        displayNeedsFullRedraw = true;
        return;
    }

    if (!add2DatenVorhanden || add2WaageOff) {
        if (ty >= BTN1_Y && ty <= BTN1_Y + BTN_H) {
            Serial.println("[INFO] Buttons deaktiviert - keine Daten oder Waage OFF");
        }
        return;
    }

    if (tx >= BTN1_X && tx <= BTN1_X + BTN_W &&
        ty >= BTN1_Y && ty <= BTN1_Y + BTN_H) {
        Serial.println(">>> TOTAL gedrueckt");
        add2ZeroModus = false;
        add2Offset = 0;
        displayNeedsFullRedraw = true;
    }

    if (tx >= BTN2_X && tx <= BTN2_X + BTN_W &&
        ty >= BTN2_Y && ty <= BTN2_Y + BTN_H) {
        Serial.println(">>> ZERO gedrueckt");
        add2ZeroModus = true;
        add2Offset = add2Gewicht;
        Serial.print("Offset gesetzt: ");
        Serial.println(add2Offset);
        displayNeedsFullRedraw = true;
    }
}

static void handleTouchSchaufelMenu(int tx, int ty) {
    // TARA (y: 50-85)
    if (ty >= 50 && ty <= 85 && tx >= 40 && tx <= 280) {
        Serial.println(">>> TARA gewaehlt");
        if (schaufelInPosition) {
            menuState = MENU_CONFIRM_TARA;
            displayNeedsFullRedraw = true;
        } else {
            Serial.println("[INFO] Nicht in Wiegeposition!");
        }
        return;
    }

    // KALIBRIEREN (y: 95-130)
    if (ty >= 95 && ty <= 130 && tx >= 40 && tx <= 280) {
        Serial.println(">>> KALIBRIEREN gewaehlt");
        if (schaufelInPosition) {
            kalibierGewicht = 100;
            menuState = MENU_KALIBRIEREN;
            displayNeedsFullRedraw = true;
        } else {
            Serial.println("[INFO] Nicht in Wiegeposition!");
        }
        return;
    }

    // FILTER (y: 140-175)
    if (ty >= 140 && ty <= 175 && tx >= 40 && tx <= 280) {
        Serial.println(">>> FILTER gewaehlt");
        tempFilterAnzahl = filterAnzahl;
        tempFilterSchwelle = filterSchwelle;
        menuState = MENU_FILTER;
        displayNeedsFullRedraw = true;
        return;
    }

    // ZURUECK (y: 185-220)
    if (ty >= 185 && ty <= 220 && tx >= 40 && tx <= 280) {
        Serial.println(">>> ZURUECK gewaehlt");
        menuState = MENU_NONE;
        displayNeedsFullRedraw = true;
        return;
    }
}

static void handleTouchKalibrieren(int tx, int ty) {
    // -10
    if (tx >= 30 && tx <= 130 && ty >= 110 && ty <= 160) {
        kalibierGewicht -= 10;
        if (kalibierGewicht < 0) kalibierGewicht = 0;
        displayNeedsFullRedraw = true;
        return;
    }

    // +10
    if (tx >= 190 && tx <= 290 && ty >= 110 && ty <= 160) {
        kalibierGewicht += 10;
        if (kalibierGewicht > 9990) kalibierGewicht = 9990;
        displayNeedsFullRedraw = true;
        return;
    }

    // ESC
    if (tx >= 20 && tx <= 120 && ty >= 185 && ty <= 230) {
        menuState = MENU_SCHAUFEL;
        displayNeedsFullRedraw = true;
        return;
    }

    // SAVE
    if (tx >= 200 && tx <= 300 && ty >= 185 && ty <= 230) {
        if (kalibierGewicht > 0) {
            float spannungBereinigt = schaufelRohwert - schaufelTara;
            if (spannungBereinigt > 0.01f) {
                schaufelFaktor = (float)kalibierGewicht / spannungBereinigt;
                storage::setFaktor(schaufelFaktor);
                scale::resetFilter();
                Serial.print("[OK] Kalibriert! Neuer Faktor: ");
                Serial.println(schaufelFaktor);
            }
        }
        menuState = MENU_NONE;
        displayNeedsFullRedraw = true;
        return;
    }
}

static void handleTouchFilter(int tx, int ty) {
    // Anzahl -5 (y: 70-115)
    if (tx >= 20 && tx <= 70 && ty >= 70 && ty <= 115) {
        tempFilterAnzahl -= 5;
        if (tempFilterAnzahl < FILTER_ANZAHL_MIN) tempFilterAnzahl = FILTER_ANZAHL_MIN;
        displayNeedsFullRedraw = true;
        return;
    }
    // Anzahl +5
    if (tx >= 250 && tx <= 300 && ty >= 70 && ty <= 115) {
        tempFilterAnzahl += 5;
        if (tempFilterAnzahl > FILTER_ANZAHL_MAX) tempFilterAnzahl = FILTER_ANZAHL_MAX;
        displayNeedsFullRedraw = true;
        return;
    }
    // Schwelle -1 (y: 130-175)
    if (tx >= 20 && tx <= 70 && ty >= 130 && ty <= 175) {
        tempFilterSchwelle -= 1;
        if (tempFilterSchwelle < FILTER_SCHWELLE_MIN) tempFilterSchwelle = FILTER_SCHWELLE_MIN;
        displayNeedsFullRedraw = true;
        return;
    }
    // Schwelle +1
    if (tx >= 250 && tx <= 300 && ty >= 130 && ty <= 175) {
        tempFilterSchwelle += 1;
        if (tempFilterSchwelle > FILTER_SCHWELLE_MAX) tempFilterSchwelle = FILTER_SCHWELLE_MAX;
        displayNeedsFullRedraw = true;
        return;
    }
    // ESC (y: 190-235)
    if (tx >= 20 && tx <= 120 && ty >= 190 && ty <= 235) {
        menuState = MENU_SCHAUFEL;
        displayNeedsFullRedraw = true;
        return;
    }
    // SAVE
    if (tx >= 200 && tx <= 300 && ty >= 190 && ty <= 235) {
        filterAnzahl = tempFilterAnzahl;
        filterSchwelle = tempFilterSchwelle;
        storage::setFilterAnzahl(filterAnzahl);
        storage::setFilterSchwelle(filterSchwelle);
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

static void handleTouchTaraConfirm(int tx, int ty) {
    // ESC (y: 185-230)
    if (tx >= 20 && tx <= 120 && ty >= 185 && ty <= 230) {
        Serial.println(">>> TARA abgebrochen");
        menuState = MENU_SCHAUFEL;
        displayNeedsFullRedraw = true;
        return;
    }
    // SAVE
    if (tx >= 200 && tx <= 300 && ty >= 185 && ty <= 230) {
        Serial.println(">>> TARA bestaetigt");
        schaufelTara = schaufelRohwert;
        storage::setTara(schaufelTara);
        scale::resetFilter();
        Serial.print("[OK] Tara gesetzt: ");
        Serial.println(schaufelTara, 3);
        menuState = MENU_NONE;
        displayNeedsFullRedraw = true;
        return;
    }
}
