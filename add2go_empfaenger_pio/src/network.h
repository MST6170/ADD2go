// WiFi STA + UDP-Empfaenger fuer add2go-Sender + RSSI
// Migriert aus add2go_empfaenger_v1_0_2.ino Z. 409-505, 583-593

#pragma once

namespace network {

void verbindeWiFi();         // blocking initial connect, max 10 * 500 ms
void handleWiFi();           // non-blocking reconnect-Loop
void handleUDP();            // Empfaenger fuer add2go-Sender (Port 5005)
void handleRSSI();           // 1 Hz RSSI -> signalBars
void resetZero();            // ZERO-Modus zuruecksetzen (3 Trigger)

} // namespace network
