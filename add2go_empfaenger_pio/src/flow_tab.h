// Flow-Tab — WebSocket-Client zu add2flow (Phase 3c, Schritt 5).
// Verantwortung: WS-Verbindung verwalten, Wire-Protokoll parsen, State-Cache
// pflegen. Rendering passiert in display::zeichneFlowTab().

#pragma once

#include <Arduino.h>

namespace flow {

// FlowState-Enum spiegelt das STATE-Feld aus dem add2flow Wire-Protokoll
// (siehe d:\add2flow\PHASE_3B_STATUS.md).
enum FlowState : uint8_t {
    FS_UNKNOWN = 0,
    FS_IDLE,
    FS_RESUME_PROMPT,
    FS_FILLING,
    FS_PAUSED,
    FS_DONE,
    FS_ABORTED,
    FS_ERROR,
    FS_SETTINGS,
    FS_SETTINGS_ADV,
    FS_CALIBRATING
};

struct Snapshot {
    FlowState  state;
    uint16_t   doneL;
    uint16_t   targetL;
    float      lpm;
    uint8_t    preset;          // 0/1/2/3
    char       errMsg[24];
    int8_t     rssi;
    uint16_t   p1, p2, p3;
    uint32_t   lastFrameMs;     // millis() vom letzten empfangenen Frame
};

void begin();                   // Beim System-Boot — nur Vorbereitung, kein WS-Connect.
void loop();                    // Jeden Loop-Iteration; nur aktiv wenn currentTab==TAB_FLOW.

// Tab-Wechsel:
void onTabEnter();              // currentTab gerade auf TAB_FLOW gesetzt — start WS-Connect-Sequenz.
void onTabExit();               // Zurueck zu Schaufel — WS disconnect.

// Touch-Routing aus touch.cpp.
void handleTouch(int tx, int ty);

// Lese-Zugriff fuer display.cpp.
Snapshot snapshot();
bool     wsConnected();         // Sieht den letzten WS-Frame innerhalb 5 s?
bool     staSwitchInProgress(); // True solange STA nicht WL_CONNECTED nach Tab-Enter.
bool     staSwitchTimedOut();   // True wenn STA-Switch > WIFI_STA_SWITCH_TIMEOUT_MS dauerte.
uint8_t  pendingConfirmPreset();// 0 = kein Modal aktiv, 1/2/3 = Preset will gestartet werden.

} // namespace flow
