#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <Adafruit_ADS1X15.h>
#include <WebSocketsClient.h>

// Stub-Instanzen, damit lib_ldf (chain-mode) die Libs als „verwendet" erkennt.
// In Schritt 3 (Modul-Refactor) entstehen die echten Instanzen in den
// jeweiligen Modulen (display.cpp, scale.cpp, flow_tab.cpp etc.), dann sind
// die Stubs hier weg.
static TFT_eSPI _stubTft;
static XPT2046_Touchscreen _stubTs(15, 14);
static Adafruit_ADS1115 _stubAds;
static WebSocketsClient _stubWs;

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println();
    Serial.println("========================================");
    Serial.println("add2go-empfaenger-pio - Phase 3c");
    Serial.println("Build-Skelett (Schritt 2)");
    Serial.println("========================================");
}

void loop() {
    delay(1000);
}
