// add2go-empfaenger-pio — Phase 3c Modul-Migration
// Orchestrierung aus add2go_empfaenger_v1_0_2.ino Z. 217-404 (setup + loop)

#include "config.h"
#include "state.h"
#include "storage.h"
#include "network.h"
#include "scale.h"
#include "display.h"
#include "touch.h"
#include "ui.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

static void setupWatchdog() {
    // Alte 2-Arg-API fuer ESP32-Core 2.x / IDF 4.4.
    // Sketch v1.0.2 nutzt die struct-API (IDF 5+), kompiliert dort aber gegen
    // ein neueres Core. Hier auf Core ~6.5.0 muss die alte API genommen werden
    // — analog zu add2flow Phase 3b (siehe d:\add2flow\GOTCHAS.md).
    esp_task_wdt_init(WATCHDOG_TIMEOUT_SEC, true);
    esp_task_wdt_add(NULL);
    Serial.printf("[OK] Watchdog: %d s\n", WATCHDOG_TIMEOUT_SEC);
}

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("add2go-empfaenger-pio v1.1");
    Serial.println("Phase 3c Modul-Migration");
    Serial.println("========================================");
    Serial.println();

    // 1. Reed-Sensor + I2C/ADS1115
    scale::begin();

    // 2. NVS laden
    storage::begin();
    schaufelTara   = storage::getTara();
    schaufelFaktor = storage::getFaktor();
    filterAnzahl   = storage::getFilterAnzahl();
    filterSchwelle = storage::getFilterSchwelle();

    Serial.print("[OK] NVS geladen - Tara: ");
    Serial.print(schaufelTara, 3);
    Serial.print("V, Faktor: ");
    Serial.print(schaufelFaktor, 1);
    Serial.print(" kg/V, Filter: ");
    Serial.print(filterAnzahl);
    Serial.print(" Werte, Schwelle: ");
    Serial.print(filterSchwelle);
    Serial.println(" kg");

    scale::resetFilter();

    // 3. Display + Touch
    display::begin();
    touch::begin();

    // 4. Boot-Screen mit Fortschrittsbalken
    display::zeichneBootScreen();
    display::bootProgress(20, "Display OK");
    display::bootProgress(40, "Touch OK");

    // 5. Watchdog initialisieren (VOR WiFi-Connect)
    setupWatchdog();
    display::bootProgress(60, "Watchdog OK");

    // 6. WiFi (Watchdog wird gefuettert in verbindeWiFi)
    display::bootProgress(80, "WiFi verbinden...");
    network::verbindeWiFi();

    display::bootWifiResult(WiFi.status() == WL_CONNECTED);
    display::bootCopyright();

    // 7. UI initial zeichnen
    displayNeedsFullRedraw = true;

    Serial.println();
    Serial.println("System bereit!");
    Serial.println();
}

void loop() {
    esp_task_wdt_reset();

    // 1. Reed-Kontakt SOFORT lesen
    scale::handleReed();

    // 2. WiFi managen
    network::handleWiFi();

    // 3. UDP empfangen
    network::handleUDP();

    // 4. ADC lesen (10 Hz)
    scale::handleADC();

    // 4b. RSSI (1 Hz)
    network::handleRSSI();

    // 5. Touch verarbeiten
    touch::handle();

    // 6. Display aktualisieren
    display::handle();

    // 7. Menue Live-Update
    ui::handleMenuLiveUpdate();

    delay(10);
}
