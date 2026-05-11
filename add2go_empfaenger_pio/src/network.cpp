#include "network.h"
#include "config.h"
#include "state.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_task_wdt.h>

static WiFiUDP udp;
static char udpBuffer[UDP_BUFFER_SIZE];
static const int TIMEOUT_MS = 2000;

namespace network {

void resetZero() {
    if (add2ZeroModus) {
        Serial.println("[INFO] ZERO automatisch zurueckgesetzt");
        add2ZeroModus = false;
        add2Offset = 0;
    }
}

void verbindeWiFi() {
    Serial.print("Verbinde mit ");
    Serial.print(WIFI_AP_ADD2GO);
    Serial.print("...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_AP_ADD2GO);

    int versuche = 0;
    while (WiFi.status() != WL_CONNECTED && versuche < 10) {
        esp_task_wdt_reset();
        delay(500);
        Serial.print(".");
        versuche++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[OK] Verbunden! IP: ");
        Serial.println(WiFi.localIP());
        udp.stop();
        udp.begin(UDP_PORT_ADD2GO);
    } else {
        Serial.println("[FEHLER] Verbindung fehlgeschlagen");
    }
}

void handleWiFi() {
    esp_task_wdt_reset();

    if (WiFi.status() != WL_CONNECTED) {
        if (wifiStatus == WIFI_OK) {
            Serial.println("[WARNUNG] WiFi Verbindung verloren!");
            wifiStatus = WIFI_FEHLER;
            lastReconnect = millis();
            resetZero();
        } else if (millis() - lastReconnect > WIFI_RECONNECT_INTERVAL_MS) {
            Serial.println("[INFO] Starte Reconnect...");
            wifiStatus = WIFI_SUCHE;
            WiFi.disconnect(true);
            delay(100);
            WiFi.begin(WIFI_AP_ADD2GO);
            lastReconnect = millis();
        }
    } else {
        if (wifiStatus != WIFI_OK) {
            wifiStatus = WIFI_OK;
            Serial.print("[OK] WiFi verbunden! IP: ");
            Serial.println(WiFi.localIP());
            udp.stop();
            udp.begin(UDP_PORT_ADD2GO);
        }
    }
}

void handleUDP() {
    if (WiFi.status() != WL_CONNECTED) return;

    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
        int len = udp.read(udpBuffer, sizeof(udpBuffer) - 1);
        if (len > 0) {
            udpBuffer[len] = '\0';

            if (strcmp(udpBuffer, "OFF") == 0) {
                if (!add2WaageOff) {
                    resetZero();
                }
                add2WaageOff = true;
                add2DatenVorhanden = true;
                add2LastData = millis();
            } else {
                add2WaageOff = false;
                add2Gewicht = atoi(udpBuffer);
                add2DatenVorhanden = true;
                add2LastData = millis();
            }
        }
    }

    // Daten-Timeout
    if (add2DatenVorhanden && (millis() - add2LastData > TIMEOUT_MS)) {
        add2DatenVorhanden = false;
        add2WaageOff = false;
        resetZero();
        Serial.println("[WARNUNG] ADD2 Daten-Timeout");
    }
}

void handleRSSI() {
    if (millis() - lastRSSIRead < RSSI_INTERVAL_MS) return;
    lastRSSIRead = millis();

    if (WiFi.status() == WL_CONNECTED) {
        int8_t rssi = WiFi.RSSI();
        signalBars = rssi > -50 ? 4 : rssi > -60 ? 3 : rssi > -70 ? 2 : rssi > -80 ? 1 : 0;
    } else {
        signalBars = 0;
    }
}

} // namespace network
