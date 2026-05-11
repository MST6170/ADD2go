#include "ui.h"
#include "state.h"
#include <Arduino.h>

namespace ui {

void handleMenuLiveUpdate() {
    if (menuState == MENU_NONE) return;

    static bool letztePositionImMenu = true;

    if (schaufelInPosition != letztePositionImMenu) {
        letztePositionImMenu = schaufelInPosition;

        Serial.print("[INFO] Wiegeposition geaendert: ");
        Serial.println(schaufelInPosition ? "JA" : "NEIN");

        displayNeedsFullRedraw = true;

        if (!schaufelInPosition && (menuState == MENU_KALIBRIEREN || menuState == MENU_CONFIRM_TARA)) {
            Serial.println("[WARNUNG] Position verloren - zurueck zum Menue");
            menuState = MENU_SCHAUFEL;
        }
    }
}

} // namespace ui
