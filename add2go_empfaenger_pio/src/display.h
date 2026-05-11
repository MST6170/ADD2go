// TFT-Init + Boot-Screen + alle zeichne*-Render-Funktionen
// Migriert aus add2go_empfaenger_v1_0_2.ino Z. 268-370, 873-1397

#pragma once

namespace display {

void begin();                       // tft.init() + setRotation(1) + clear screen
void zeichneBootScreen();           // Initial-Boot-Screen mit Fortschrittsbalken
void bootProgress(int prozent,
                  const char* text);     // Wird waehrend setup() aufgerufen
void bootWifiResult(bool connected);     // WiFi-Status im Boot-Balken zeichnen
void bootCopyright();                    // "(c) MST 2026" zentriert + delay

void handle();                      // displayNeedsFullRedraw + Hauptbildschirm-Update
void zeichneAktuellesMenu();        // dispatch nach menuState
void updateHauptbildschirm();       // Partial-Redraw fuer Hauptbildschirm

// Hauptbildschirm-Elemente
void zeichneHauptbildschirm();
void zeichneSchaufelButton();
void zeichneWiFiStatus();
void zeichneSignalBars(int activeBars);
void zeichneAdd2Gewicht();
void zeichneButton(int x, int y, const char* text, bool aktiv, bool enabled);

// Menue-Renders
void zeichneSchaufelMenu();
void zeichneKalibrierMenu();
void zeichneFilterMenu();
void zeichneTaraConfirm();

} // namespace display
