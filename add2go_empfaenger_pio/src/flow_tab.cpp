#include "flow_tab.h"
#include "config.h"
#include "state.h"
#include <WiFi.h>
#include <WebSocketsClient.h>

static WebSocketsClient ws;
static bool     s_wsBegan         = false;
static uint32_t s_staSwitchStart  = 0;
static bool     s_staSwitchActive = false;
static uint8_t  s_pendingPreset   = 0;

static flow::Snapshot s_snap = {
    flow::FS_UNKNOWN, 0, 0, 0.0f, 0, "", -128, 0, 0, 0, 0
};

// ---- Parser ----

static flow::FlowState parseState(const char* s) {
    if (!s || !*s) return flow::FS_UNKNOWN;
    if (!strcmp(s, "IDLE"))           return flow::FS_IDLE;
    if (!strcmp(s, "RESUME_PROMPT"))  return flow::FS_RESUME_PROMPT;
    if (!strcmp(s, "FILLING"))        return flow::FS_FILLING;
    if (!strcmp(s, "PAUSED"))         return flow::FS_PAUSED;
    if (!strcmp(s, "DONE"))           return flow::FS_DONE;
    if (!strcmp(s, "ABORTED"))        return flow::FS_ABORTED;
    if (!strcmp(s, "ERROR"))          return flow::FS_ERROR;
    if (!strcmp(s, "SETTINGS"))       return flow::FS_SETTINGS;
    if (!strcmp(s, "SETTINGS_ADV"))   return flow::FS_SETTINGS_ADV;
    if (!strcmp(s, "CALIBRATING"))    return flow::FS_CALIBRATING;
    return flow::FS_UNKNOWN;
}

// Pipe-Split-Parser. Erwartet 11 Felder: v1|STATE|done|target|lpm|preset|err|rssi|p1|p2|p3
static void parseFrame(const char* payload, size_t len) {
    if (len < 4) return;

    static char buf[160];
    size_t n = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
    memcpy(buf, payload, n);
    buf[n] = '\0';

    // Tokenize
    char* fields[11] = {nullptr};
    int fi = 0;
    char* p = buf;
    fields[fi++] = p;
    while (*p && fi < 11) {
        if (*p == '|') {
            *p = '\0';
            fields[fi++] = p + 1;
        }
        p++;
    }
    if (fi < 11) {
        Serial.printf("[flow] Frame zu kurz: %d Felder\n", fi);
        return;
    }
    if (strcmp(fields[0], "v1") != 0) {
        Serial.printf("[flow] Unbekannte Protokoll-Version: %s\n", fields[0]);
        return;
    }

    s_snap.state       = parseState(fields[1]);
    s_snap.doneL       = (uint16_t)atoi(fields[2]);
    s_snap.targetL     = (uint16_t)atoi(fields[3]);
    s_snap.lpm         = atof(fields[4]);
    s_snap.preset      = (uint8_t)atoi(fields[5]);
    strncpy(s_snap.errMsg, fields[6], sizeof(s_snap.errMsg) - 1);
    s_snap.errMsg[sizeof(s_snap.errMsg) - 1] = '\0';
    s_snap.rssi        = (int8_t)atoi(fields[7]);
    s_snap.p1          = (uint16_t)atoi(fields[8]);
    s_snap.p2          = (uint16_t)atoi(fields[9]);
    s_snap.p3          = (uint16_t)atoi(fields[10]);
    s_snap.lastFrameMs = millis();
}

// ---- WS-Event-Handler ----

static void onWsEvent(WStype_t type, uint8_t* payload, size_t len) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.println("[ws] disconnected");
            break;
        case WStype_CONNECTED:
            Serial.printf("[ws] connected to %s\n", (const char*)payload);
            break;
        case WStype_TEXT:
            parseFrame((const char*)payload, len);
            break;
        default:
            break;
    }
}

// ---- API ----

namespace flow {

void begin() {
    ws.onEvent(onWsEvent);
    ws.setReconnectInterval(2000);
}

void onTabEnter() {
    Serial.println("[flow] Tab Flow betreten - starte STA-Switch zu add2flow");
    s_wsBegan = false;
    s_staSwitchStart = millis();
    s_staSwitchActive = true;
    s_pendingPreset = 0;
    s_snap.lastFrameMs = 0;        // forciert Greying bis erster Frame
    // STA-Wechsel triggern (in network.cpp realisiert):
    // currentTab ist schon auf TAB_FLOW gesetzt; network::onTabChanged
    // wird vom Touch-Handler aufgerufen.
}

void onTabExit() {
    Serial.println("[flow] Tab Flow verlassen - WS disconnect");
    if (s_wsBegan) {
        ws.disconnect();
        s_wsBegan = false;
    }
    s_staSwitchActive = false;
    s_pendingPreset = 0;
}

void loop() {
    if (currentTab != TAB_FLOW) return;

    // STA-Switch ueberwachen: wenn WiFi up + WS noch nicht gestartet, start ws.
    if (WiFi.status() == WL_CONNECTED && !s_wsBegan) {
        Serial.println("[flow] STA verbunden — starte WS-Client zu 192.168.4.1:81");
        ws.begin("192.168.4.1", 81, "/");
        s_wsBegan = true;
        s_staSwitchActive = false;
        displayNeedsFullRedraw = true;     // Spinner -> echtes Flow-Tab-Layout
    }

    if (s_wsBegan) {
        ws.loop();
    }

    // Transition-Detection fuer Display: Spinner -> Fallback -> Connected -> Disconnected
    static bool lastTimedOut    = false;
    static bool lastWsConnected = false;
    bool nowTimedOut    = staSwitchTimedOut();
    bool nowWsConnected = wsConnected();
    if (nowTimedOut != lastTimedOut || nowWsConnected != lastWsConnected) {
        displayNeedsFullRedraw = true;
        lastTimedOut    = nowTimedOut;
        lastWsConnected = nowWsConnected;
    }
}

void handleTouch(int tx, int ty) {
    // Confirm-Modal aktiv? Touch nur auf Ja/Abbruch reagieren.
    if (s_pendingPreset > 0) {
        // Modal-Layout: Vollbild ueber Header. Ja-Button links unten, Abbruch rechts unten.
        // Ja: x=20..150, y=170..220
        if (tx >= 20 && tx <= 150 && ty >= 170 && ty <= 220) {
            char cmd[12];
            snprintf(cmd, sizeof(cmd), "START:%u", (unsigned)s_pendingPreset);
            Serial.printf("[flow] Start bestaetigt: %s\n", cmd);
            if (s_wsBegan) ws.sendTXT(cmd);
            s_pendingPreset = 0;
            displayNeedsFullRedraw = true;
            return;
        }
        // Abbruch: x=170..300, y=170..220
        if (tx >= 170 && tx <= 300 && ty >= 170 && ty <= 220) {
            Serial.println("[flow] Start abgebrochen");
            s_pendingPreset = 0;
            displayNeedsFullRedraw = true;
            return;
        }
        return;
    }

    // Bei STA-Switch oder Fallback-Screen — kein Touch ausser Tab-Switch
    if (s_staSwitchActive || !wsConnected()) {
        return;
    }

    // Preset-Buttons: 3 Stueck a 70 px breit, y=180..230
    // P1: x=10..80, P2: x=85..155, P3: x=160..230
    // STOP:        x=235..310
    if (ty >= 180 && ty <= 230) {
        if (tx >= 10 && tx <= 80) {
            s_pendingPreset = 1;
            displayNeedsFullRedraw = true;
        } else if (tx >= 85 && tx <= 155) {
            s_pendingPreset = 2;
            displayNeedsFullRedraw = true;
        } else if (tx >= 160 && tx <= 230) {
            s_pendingPreset = 3;
            displayNeedsFullRedraw = true;
        } else if (tx >= 235 && tx <= 310) {
            Serial.println("[flow] STOP gedrueckt");
            if (s_wsBegan) ws.sendTXT("STOP");
        }
    }
}

Snapshot snapshot()             { return s_snap; }
bool wsConnected()              { return s_snap.lastFrameMs != 0 && (millis() - s_snap.lastFrameMs) <= 5000; }
bool staSwitchInProgress()      { return s_staSwitchActive && WiFi.status() != WL_CONNECTED; }
bool staSwitchTimedOut()        {
    return s_staSwitchActive && WiFi.status() != WL_CONNECTED
        && (millis() - s_staSwitchStart) > WIFI_STA_SWITCH_TIMEOUT_MS;
}
uint8_t pendingConfirmPreset()  { return s_pendingPreset; }

} // namespace flow
