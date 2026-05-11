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

// Phase 3c: SSID haengt vom aktiven Tab ab — Schaufel-Tab → ADD2go (UDP-Sender),
// Flow-Tab → add2flow (WebSocket-Server).
static const char* activeSsid() {
    return (currentTab == TAB_FLOW) ? WIFI_AP_ADD2FLOW : WIFI_AP_ADD2GO;
}

void verbindeWiFi() {
    const char* ssid = activeSsid();
    Serial.print("Verbinde mit ");
    Serial.print(ssid);
    Serial.print("...");

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid);

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
        if (currentTab == TAB_SCHAUFEL) {
            udp.stop();
            udp.begin(UDP_PORT_ADD2GO);
        }
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
            const char* ssid = activeSsid();
            Serial.print("[INFO] Starte Reconnect zu ");
            Serial.print(ssid);
            Serial.println("...");
            wifiStatus = WIFI_SUCHE;
            WiFi.disconnect(true);
            delay(100);
            WiFi.begin(ssid);
            lastReconnect = millis();
        }
    } else {
        if (wifiStatus != WIFI_OK) {
            wifiStatus = WIFI_OK;
            Serial.print("[OK] WiFi verbunden! IP: ");
            Serial.println(WiFi.localIP());
            // UDP nur im Schaufel-Tab oeffnen — im Flow-Tab macht WS-Client das Routing.
            if (currentTab == TAB_SCHAUFEL) {
                udp.stop();
                udp.begin(UDP_PORT_ADD2GO);
            } else {
                udp.stop();
            }
        }
    }
}

// Externer Trigger fuer Tab-Wechsel: forciert STA-Reconnect mit der neuen SSID.
void onTabChanged() {
    Serial.println("[network] Tab gewechselt — STA disconnect, reconnect mit neuer SSID");
    udp.stop();
    WiFi.disconnect(true);
    wifiStatus = WIFI_SUCHE;
    lastReconnect = millis() - WIFI_RECONNECT_INTERVAL_MS;  // erlaubt sofortigen Reconnect-Versuch
    // Add2DatenVorhanden auf false damit Hauptbildschirm-Display nicht alte Schaufel-Daten zeigt
    add2DatenVorhanden = false;
    add2WaageOff = false;
    resetZero();
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
