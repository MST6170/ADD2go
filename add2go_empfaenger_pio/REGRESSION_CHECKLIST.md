# REGRESSION CHECKLIST — v1.0.2 → PIO

Sicherheits-Netz vor und nach dem Spaghetti-zu-Module-Refactor in Phase 3c
Schritt 3. **Vor jedem Modul-Cut den Punkt im Sketch lesen, nach Build-grün
gegen die Hardware durchspielen.**

Quelle: [`../add2go_empfaenger_v1_0_2/add2go_empfaenger_v1_0_2.ino`](../add2go_empfaenger_v1_0_2/add2go_empfaenger_v1_0_2.ino)
(1397 Zeilen). Zeilennummern beziehen sich auf diese Datei.

## Schaufel-Sensorik (ADS1115 + Reed + Filter)

- [ ] **1. Tara setzen + Persistenz** — Schaufel-Menü → „Tara setzen" → ConfirmTara „Ja".
      `prefs.putFloat("tara", ...)` + `resetFilter()` läuft, Anzeige geht auf 0 kg.
      Nach Power-Cycle bleibt Tara-Wert erhalten. (Sketch Z. 195-203 `resetFilter`, Z. 242-247 NVS-Load, Z. 730-770 ConfirmTara-Save-Pfad)

- [ ] **2. Kalibrieren** — Menü „Kalibrieren" → bekanntes Gewicht aufgeben →
      Faktor wird korrekt berechnet (`schaufelFaktor = bekanntesGewicht / spannungsdifferenz`)
      und in NVS gespeichert. (Sketch Z. 245 NVS-Load, Z. 700-730 Kalibrieren-Menu)

- [ ] **3. Filter-Mittelwert** — Filter-Anzahl auf 10 (Standard), Schaufel mit
      konstanter Last → Anzeige glatt ohne 1-kg-Sprünge. (Sketch Z. 562-569
      Mittelwert-Berechnung)

- [ ] **4. Filter-Hysterese** — Filter-Schwelle 5 kg (Standard) → kleine
      Last-Schwankungen <5 kg ändern Anzeige nicht. (Sketch Z. 571-576
      Hysterese-Logik)

- [ ] **5. Filter-Buffer-Reset nach Tara** — Tara setzen während Last drauf →
      Filter-Buffer wird geleert, kein Mischwert aus alter und neuer Tara.
      (Sketch Z. 195-203 `resetFilter`, aufgerufen in ConfirmTara-Save-Pfad)

- [ ] **6. ADS1115-Sensor-Fehler-Erkennung** — Sensor-Adern abziehen → nach 10
      I²C-Fehlern „SENSOR-FEHLER"-Anzeige zentriert im Schaufel-Button.
      (Sketch Z. 521-533 Fehler-Counter, Z. 142-147 ADS_FEHLER_LIMIT=10)

- [ ] **7. ADS1115-Stuck-Detection** — Wenn Sensor 50× identischen Wert
      liefert (~5 s) → ebenfalls SENSOR-FEHLER (eingefroren).
      (Sketch Z. 535-547 Stuck-Detection, Z. 146-148 ADS_STUCK_LIMIT=50)

- [ ] **8. Reed-Kontakt-Position** — Schaufel hochheben (Reed offen) → `schaufelInPosition = false`,
      kein ADC-Sample. Schaufel runter (Reed LOW) → `schaufelInPosition = true`,
      ADC läuft. (Sketch Z. 551 `if (schaufelInPosition)`)

## WiFi & UDP

- [ ] **9. WiFi-Reconnect-Loop** — Sender vom Strom → Empfänger geht in
      `WIFI_SUCHE` (rotes Symbol), versucht alle 5 s neu (non-blocking),
      kommt nach Wiederanschalten zurück in `WIFI_OK`. (Sketch Z. 409-438
      `handleWiFi()`, RECONNECT_INTERVAL=5000)

- [ ] **10. UDP-Empfang** — Sender sendet Gewicht → Empfänger zeigt es auf
      Display, `add2DatenVorhanden = true`, `add2LastData` aktualisiert.
      (Sketch Z. 471-505 `handleUDP()`)

- [ ] **11. ZERO-Auto-Reset (3 Trigger)** — ZERO-Modus wird automatisch
      zurückgesetzt (`add2ZeroModus = false`, `add2Offset = 0`) wenn:
      (a) Waage von ON → OFF (Z. 481-485)
      (b) WiFi-Verbindung verloren (Z. 418-419)
      (c) UDP-Daten-Timeout > 2 s (Z. 499-505)
      (Sketch Z. 205-212 `resetZero()`)

- [ ] **12. UDP-Daten-Timeout** — Sender vom Strom (ohne dass WiFi-Loss eintritt) →
      nach 2 s `TIMEOUT_MS` → `add2DatenVorhanden = false`. (Sketch Z. 499-505)

## Display & UI

- [ ] **13. OFF-Anzeige** — ADD2-Waage OFF (Sender sendet „OFF") → Display
      zeigt „OFF" groß statt Zahl. (Sketch Z. 481-488 OFF-Erkennung, Render
      in `zeichneHauptbildschirm`)

- [ ] **14. Signal-Bars 4-Stufen** — 4 Balken bei RSSI > -50 dBm, 3 bei > -60,
      2 bei > -70, 1 bei > -80, 0 bei <= -80 oder Disconnect. Update ~1 Hz.
      (Sketch Z. 583-593 `handleRSSI()`)

- [ ] **15. Boot-Screen mit Fortschrittsbalken** — Power-On → ADD2go-Titel
      groß zentriert + Fortschrittsbalken, dann Übergang zum Hauptbildschirm.
      (Sketch Z. 279-370 Boot-Screen-Sequenz)

- [ ] **16. Menü-Navigation** — Hauptbildschirm → Schaufel-Menu → Kalibrieren /
      Filter / Tara-Confirm. Back-Button funktioniert. Im Menü zeigt
      `handleMenuLiveUpdate` live ADC-Werte. (Sketch Z. 597-620 Live-Update,
      Z. 621-868 Touch-Handler pro Menü)

- [ ] **17. Partial-Redraw-State-Tracking** — Display flackert nicht — Werte
      werden nur neu gezeichnet wenn sie sich ändern (`lastAdd2Anzeige`,
      `lastSchaufelGewicht`, etc.). (Sketch Z. 180-188 State-Tracking-Vars)

## Boot & System

- [ ] **18. Watchdog 5 s** — Mit Sketch-Code soll der Watchdog 5 s sein und
      während Boot-Phase + WiFi-Connect-Loop explizit gefeedet werden.
      (Sketch Z. 113 WATCHDOG_TIMEOUT_SEC=5, Z. 411 `esp_task_wdt_reset` in
      `handleWiFi`, Z. 451 in `verbindeWiFi`)

- [ ] **19. NVS-Load mit Defaults + Limits** — `filterAnzahl` zwischen 5..30,
      `filterSchwelle` zwischen 1..10. Defaults wenn NVS leer. (Sketch
      Z. 246-253)

## Hinweis zum Ablauf

1. Sketch v1.0.2 unflashed lassen, **nur PIO-Build flashen** in Schritt 3.
2. Hardware-Test pro Punkt durchspielen, abhaken.
3. Bei Regression: in `D:\Add2go\add2go_empfaenger_v1_0_2\` zurück, Sketch
   flashen, gegen-testen ob es im Sketch noch geht (Bisect: ist's Migration
   oder schon im v1.0.2 broken?).
4. Punkte als „failed" markieren statt zu löschen — für künftige Refactors
   archivieren.
