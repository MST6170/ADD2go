// add2go-empfaenger-pio config.h
// Pin-Defines + Konstanten (zentral). 1:1 aus add2go_empfaenger_v1_0_2.ino
// uebernommen, plus Phase-3c-Erweiterungen.

#pragma once

#include <stdint.h>

// ---- Hardware Pins ----
#define REED_SENSOR_PIN     36   // Reed-Kontakt fuer Wiegeposition (LOW = drauf)
#define I2C_SDA             21
#define I2C_SCL             22
// TFT-Pins + TOUCH_CS werden via build_flags an TFT_eSPI uebergeben (siehe platformio.ini).
// TOUCH_CS_PIN brauchen wir aber als Compile-Zeit-Konstante fuer XPT2046_Touchscreen-Konstruktor.
#define TOUCH_CS_PIN        15
#define TOUCH_IRQ           14

// ---- Watchdog ----
#define WATCHDOG_TIMEOUT_SEC  5

// ---- WiFi ----
#define WIFI_AP_ADD2GO       "ADD2go"
#define WIFI_AP_ADD2FLOW     "add2flow"
#define WIFI_RECONNECT_INTERVAL_MS  5000
#define WIFI_STA_SWITCH_TIMEOUT_MS  5000   // Phase 3c: Tab-Switch Fallback-Screen nach 5 s

// ---- UDP (add2go-Sender) ----
#define UDP_PORT_ADD2GO     5005
#define UDP_BUFFER_SIZE     64

// ---- ADC / Schaufel ----
#define ADC_INTERVAL_MS     100   // 10 Hz
#define ADS_FEHLER_LIMIT    10    // 10 I2C-Fehler -> Sensor-Fehler
#define ADS_STUCK_LIMIT     50    // 50 identische Werte -> stuck

// ---- Filter ----
#define FILTER_BUFFER_SIZE  30
#define FILTER_ANZAHL_DEFAULT  10
#define FILTER_ANZAHL_MIN   5
#define FILTER_ANZAHL_MAX   30
#define FILTER_SCHWELLE_DEFAULT  5
#define FILTER_SCHWELLE_MIN  1
#define FILTER_SCHWELLE_MAX  10

// ---- RSSI / Signal-Bars ----
#define RSSI_INTERVAL_MS    1000  // 1 Hz Polling
