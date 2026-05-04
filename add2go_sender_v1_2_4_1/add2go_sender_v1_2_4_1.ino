/*
 * ADD2go Sender - v1.2.4
 * ======================
 * 
 * Wireless Brücke für Dinamica Generale ADD2 Waage
 * Liest Gewichtsdaten via RS232 und sendet per WiFi/UDP + WebSocket
 * 
 * NEU in v1.2.4:
 * - Fix: WDT-Reset in Boot-Warteschleife
 * - Fix: idle_core_mask konservativer (nur loopTask)
 * - Fix: JSON SSID Escaping
 * 
 * NEU in v1.2.3:
 * - PWA Support: Als App installierbar
 * - Manifest + Icons für Startbildschirm
 * - Install-Banner in WebApp
 * 
 * NEU in v1.2.2:
 * - mDNS: Erreichbar unter http://add2go.local
 * - Warten auf WLAN-Verbindung beim Boot
 * - Bessere NVS Debug-Ausgaben
 * 
 * NEU in v1.2.1:
 * - Async WLAN-Scan (verhindert WDT-Trigger)
 * - Robustere loop() mit mehrfachen WDT-Resets
 * - WDT-Init mit Fehlerprüfung
 * - RS232 Byte-Limit pro loop()
 * - delay(1) für WiFi/TCP Stack
 * 
 * NEU in v1.2:
 * - WiFi-Manager WebUI unter /setup
 * - WLAN scannen und verbinden
 * - Einstellungen werden gespeichert (NVS)
 * - APSTA Modus: AP + Station gleichzeitig
 * - Erreichbar über ADD2go-WLAN UND Hof-WLAN
 * 
 * Features v1.1:
 * - WebSocket Server für Browser/Handy
 * - Eingebettete WebApp unter http://192.168.4.1
 * - Signalstärke-Anzeige
 * - Parallelbetrieb: Hardware-Empfänger + Handy gleichzeitig
 * 
 * Features v1.0:
 * - WiFi Access Point "ADD2go" (Empfänger verbindet sich)
 * - RS232 Empfang mit Frame-Parsing
 * - UDP Broadcast alle 100ms (10 Hz) -> Hardware-Empfänger
 * - Hardware Watchdog (10 Sekunden)
 * - "OFF" senden wenn Waage aus (3 Sek Timeout)
 * 
 * Benötigte Library:
 * - WebSockets by Markus Sattler (Arduino Library Manager)
 * 
 * (c) MST 2026
 */

#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include <esp_task_wdt.h>
#include <esp_wifi.h>

// ============================================
// KONFIGURATION
// ============================================

// WiFi Access Point (immer aktiv)
const char* AP_SSID = "ADD2go";
const char* AP_PASS = "";  // Offen, kein Passwort

// mDNS Hostname (erreichbar unter add2go.local)
const char* MDNS_HOSTNAME = "add2go";

// UDP (für Hardware-Empfänger)
const int UDP_PORT = 5005;
IPAddress broadcastIP(192, 168, 4, 255);
WiFiUDP udp;

// WebServer + WebSocket (für Handy/Browser)
WebServer server(80);
WebSocketsServer webSocket(81);

// Preferences für WLAN-Speicherung
Preferences preferences;

// RS232 Pins
#define RXD2 16
#define TXD2 17

// Timing
#define SEND_INTERVAL 100       // UDP/WebSocket senden alle 100ms (10 Hz)
#define WAAGE_TIMEOUT 3000      // Nach 3 Sek ohne Daten = "OFF"
#define WATCHDOG_TIMEOUT_SEC 10 // Watchdog 10 Sekunden
#define AP_CHECK_INTERVAL 30000 // AP alle 30 Sekunden prüfen
#define STA_RETRY_INTERVAL 30000 // Station Reconnect alle 30 Sek
#define WIFI_CONNECT_TIMEOUT 10000 // 10 Sek Timeout beim Verbinden

// WebSocket
#define MAX_WS_CLIENTS 4        // Maximal 4 Browser gleichzeitig

// ============================================
// VARIABLEN
// ============================================

// Buffer für RS232
#define BUFFER_SIZE 64
uint8_t buffer[BUFFER_SIZE];
int bufferIndex = 0;
bool frameStarted = false;

// Gewicht
String aktuellesGewicht = "OFF";
unsigned long letzterEmpfang = 0;
bool waageOnline = false;

// Timing
unsigned long lastSendTime = 0;
unsigned long lastAPCheck = 0;
unsigned long lastSTARetry = 0;

// Station Mode (Verbindung zu externem WLAN)
String savedSSID = "";
String savedPassword = "";
bool staEnabled = false;
bool staConnected = false;
IPAddress staIP;

// Statistik
unsigned long frameCount = 0;
unsigned long sendCount = 0;

// Async WLAN-Scan
bool scanRunning = false;

// ============================================
// FORWARD DECLARATIONS
// ============================================
void handleRoot();
void handleSetup();
void handleManifest();
void handleIcon192();
void handleIcon512();
void handleApiStatus();
void handleApiScan();
void handleApiConnect();
void handleApiDisconnect();
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length);
int8_t getSignalRSSI();
void leseRS232();
void pruefeWaageTimeout();
void sendeUDP();
void sendeWebSocket();
void pruefeAP();
void verarbeiteFrame();

// ============================================
// WEBAPP HTML - GEWICHTSANZEIGE
// ============================================
const char WEBAPP_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no, viewport-fit=cover">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-status-bar-style" content="black">
  <meta name="apple-mobile-web-app-title" content="ADD2go">
  <meta name="mobile-web-app-capable" content="yes">
  <meta name="theme-color" content="#000000">
  <title>ADD2go</title>
  <link rel="manifest" href="/manifest.json">
  <link rel="icon" type="image/svg+xml" href="/icon-192.png">
  <link rel="apple-touch-icon" href="/icon-192.png">
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    html, body {
      width: 100%; height: 100%;
      background: #000; color: #fff;
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Arial, sans-serif;
      overflow: hidden; touch-action: manipulation;
      user-select: none; -webkit-user-select: none;
    }
    .container { display: flex; flex-direction: column; height: 100vh; height: 100dvh; width: 100%; }
    .header {
      display: flex; justify-content: space-between; align-items: center;
      padding: 10px 16px; background: #111; flex-shrink: 0;
    }
    .title { font-size: 1.1em; font-weight: 600; color: #fff; text-decoration: none; }
    .signal { font-size: 1em; letter-spacing: 1px; }
    .weight-container { flex: 1; display: flex; justify-content: center; align-items: center; min-height: 0; }
    .weight {
      font-size: min(26vw, 22vh); font-weight: 700; font-variant-numeric: tabular-nums;
      text-align: center; line-height: 1;
    }
    .weight.off { color: #f59e0b; }
    .weight.disconnected { color: #ef4444; }
    .weight .unit { font-size: 0.35em; color: #666; margin-left: 8px; }
    .buttons {
      display: flex; gap: 12px; padding: 12px 16px; flex-shrink: 0;
    }
    .btn {
      flex: 1; padding: 16px 0; border-radius: 10px; border: 2px solid #333;
      font-size: 1.1em; font-weight: 700; cursor: pointer;
      transition: all 0.15s ease; text-align: center;
    }
    .btn.active {
      background: #f59e0b; color: #000; border-color: #fff;
    }
    .btn.inactive {
      background: #222; color: #666; border-color: #333;
    }
    .btn.disabled {
      background: #111; color: #333; border-color: #222; pointer-events: none;
    }
    .btn:active:not(.disabled) { transform: scale(0.97); }
    .footer {
      padding: 10px 16px; background: #111; text-align: center; flex-shrink: 0;
    }
    .status { display: flex; align-items: center; justify-content: center; gap: 8px; font-size: 0.9em; }
    .status-dot { width: 10px; height: 10px; border-radius: 50%; }
    .status-dot.connected { background: #22c55e; box-shadow: 0 0 6px #22c55e; }
    .status-dot.waage-off { background: #f59e0b; box-shadow: 0 0 6px #f59e0b; }
    .status-dot.disconnected { background: #ef4444; box-shadow: 0 0 6px #ef4444; }
    .bar-empty { opacity: 0.3; }
    .bar-full { opacity: 1.0; }
    .setup-link { margin-top: 6px; }
    .setup-link a { color: #444; text-decoration: none; font-size: 0.75em; }
    .zero-info { font-size: 0.7em; color: #f59e0b; margin-top: 4px; }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <a href="/" class="title">ADD2go</a>
      <span class="signal">📶<span id="bars">▂▄▆█</span></span>
    </div>
    <div class="weight-container">
      <div class="weight" id="weight">---</div>
    </div>
    <div class="buttons">
      <button class="btn disabled" id="btnTotal" onclick="setTotal()">TOTAL</button>
      <button class="btn disabled" id="btnZero" onclick="setZero()">ZERO</button>
    </div>
    <div class="footer">
      <div class="status">
        <div class="status-dot disconnected" id="statusDot"></div>
        <span id="statusText">Verbinde...</span>
      </div>
      <div class="zero-info" id="zeroInfo"></div>
      <div class="setup-link"><a href="/setup">⚙️ Setup</a></div>
    </div>
  </div>
  <script>
    const weightEl = document.getElementById('weight');
    const statusDot = document.getElementById('statusDot');
    const statusText = document.getElementById('statusText');
    const barsEl = document.getElementById('bars');
    const btnTotal = document.getElementById('btnTotal');
    const btnZero = document.getElementById('btnZero');
    const zeroInfo = document.getElementById('zeroInfo');
    
    let ws = null, reconnectTimer = null, lastMessage = 0;
    let zeroModus = false;
    let zeroOffset = 0;
    let aktuellesGewicht = 0;
    let waageOk = false;
    
    function updateSignalBars(rssi) {
      const levels = ['▂', '▄', '▆', '█'];
      let activeBars = rssi > -50 ? 4 : rssi > -60 ? 3 : rssi > -70 ? 2 : rssi > -80 ? 1 : 0;
      let bars = '';
      for (let i = 0; i < 4; i++) {
        bars += '<span class="bar-' + (i < activeBars ? 'full' : 'empty') + '">' + levels[i] + '</span>';
      }
      barsEl.innerHTML = bars;
    }
    
    function updateButtons() {
      if (waageOk) {
        btnTotal.className = 'btn ' + (zeroModus ? 'inactive' : 'active');
        btnZero.className = 'btn ' + (zeroModus ? 'active' : 'inactive');
      } else {
        btnTotal.className = 'btn disabled';
        btnZero.className = 'btn disabled';
      }
    }
    
    function setTotal() {
      if (!waageOk) return;
      zeroModus = false;
      zeroOffset = 0;
      zeroInfo.textContent = '';
      updateButtons();
      updateDisplay();
    }
    
    function setZero() {
      if (!waageOk) return;
      zeroModus = true;
      zeroOffset = aktuellesGewicht;
      zeroInfo.textContent = 'ZERO: ' + zeroOffset + ' kg';
      updateButtons();
      updateDisplay();
    }
    
    function updateDisplay() {
      if (!waageOk) return;
      let anzeige = zeroModus ? (aktuellesGewicht - zeroOffset) : aktuellesGewicht;
      weightEl.innerHTML = anzeige + '<span class="unit">kg</span>';
      weightEl.className = 'weight';
    }
    
    function setConnected(gewicht) {
      lastMessage = Date.now();
      aktuellesGewicht = parseInt(gewicht) || 0;
      waageOk = true;
      updateDisplay();
      updateButtons();
      statusDot.className = 'status-dot connected';
      statusText.textContent = 'Verbunden';
    }
    
    function setWaageOff() {
      lastMessage = Date.now();
      waageOk = false;
      weightEl.textContent = 'OFF';
      weightEl.className = 'weight off';
      statusDot.className = 'status-dot waage-off';
      statusText.textContent = 'Waage aus';
      updateButtons();
    }
    
    function setDisconnected() {
      waageOk = false;
      weightEl.textContent = '---';
      weightEl.className = 'weight disconnected';
      statusDot.className = 'status-dot disconnected';
      statusText.textContent = 'Keine Verbindung';
      barsEl.innerHTML = '<span class="bar-empty">▂▄▆█</span>';
      updateButtons();
    }
    
    function connect() {
      if (ws && ws.readyState === WebSocket.OPEN) return;
      try {
        ws = new WebSocket('ws://' + window.location.hostname + ':81');
        ws.onopen = function() {
          if (reconnectTimer) { clearInterval(reconnectTimer); reconnectTimer = null; }
        };
        ws.onmessage = function(event) {
          const parts = event.data.split('|');
          const gewicht = parts[0];
          const rssi = parts.length > 1 ? parseInt(parts[1]) : -100;
          if (gewicht === 'OFF') setWaageOff(); else setConnected(gewicht);
          updateSignalBars(rssi);
        };
        ws.onclose = function() { setDisconnected(); scheduleReconnect(); };
        ws.onerror = function() { ws.close(); };
      } catch (e) { scheduleReconnect(); }
    }
    
    function scheduleReconnect() {
      if (!reconnectTimer) reconnectTimer = setInterval(connect, 2000);
    }
    
    setInterval(function() {
      if (Date.now() - lastMessage > 3000 && lastMessage > 0 && ws && ws.readyState === WebSocket.OPEN) {
        statusText.textContent = 'Warte auf Daten...';
      }
    }, 1000);
    
    connect();
  </script>
</body>
</html>
)rawliteral";

// ============================================
// WEBAPP HTML - SETUP SEITE
// ============================================
const char SETUP_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>ADD2go - WLAN Setup</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    html, body {
      width: 100%; min-height: 100%;
      background: #000; color: #fff;
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Arial, sans-serif;
    }
    .container { max-width: 400px; margin: 0 auto; padding: 20px; }
    .header {
      display: flex; justify-content: space-between; align-items: center;
      padding: 15px 0; border-bottom: 1px solid #333; margin-bottom: 20px;
    }
    .title { font-size: 1.3em; font-weight: 600; }
    .back { color: #3b82f6; text-decoration: none; font-size: 0.9em; }
    h2 { font-size: 1.1em; color: #888; margin: 20px 0 10px 0; font-weight: 500; }
    .status-box {
      background: #1a1a1a; border-radius: 8px; padding: 15px; margin-bottom: 20px;
    }
    .status-row { display: flex; justify-content: space-between; margin: 8px 0; }
    .status-label { color: #888; }
    .status-value { font-weight: 500; }
    .status-value.connected { color: #22c55e; }
    .status-value.disconnected { color: #888; }
    .network-list { background: #1a1a1a; border-radius: 8px; overflow: hidden; }
    .network-item {
      display: flex; justify-content: space-between; align-items: center;
      padding: 12px 15px; border-bottom: 1px solid #333; cursor: pointer;
    }
    .network-item:last-child { border-bottom: none; }
    .network-item:active { background: #333; }
    .network-item.selected { background: #1e3a5f; }
    .network-name { font-weight: 500; }
    .network-signal { color: #888; font-size: 0.9em; }
    .network-lock { margin-left: 8px; }
    input[type="password"], input[type="text"] {
      width: 100%; padding: 12px 15px; border-radius: 8px; border: 1px solid #333;
      background: #1a1a1a; color: #fff; font-size: 1em; margin: 10px 0;
    }
    input:focus { outline: none; border-color: #3b82f6; }
    .btn {
      width: 100%; padding: 14px; border-radius: 8px; border: none;
      font-size: 1em; font-weight: 600; cursor: pointer; margin: 8px 0;
    }
    .btn-primary { background: #3b82f6; color: #fff; }
    .btn-primary:disabled { background: #1e3a5f; color: #666; }
    .btn-secondary { background: #333; color: #fff; }
    .btn-danger { background: #991b1b; color: #fff; }
    .btn:active { opacity: 0.8; }
    .loading { text-align: center; padding: 20px; color: #888; }
    .spinner { display: inline-block; width: 20px; height: 20px; border: 2px solid #333;
      border-top-color: #3b82f6; border-radius: 50%; animation: spin 1s linear infinite; }
    @keyframes spin { to { transform: rotate(360deg); } }
    .message { padding: 12px 15px; border-radius: 8px; margin: 10px 0; }
    .message.success { background: #14532d; color: #86efac; }
    .message.error { background: #7f1d1d; color: #fca5a5; }
    .ip-display { font-family: monospace; font-size: 1.1em; }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <span class="title">⚙️ WLAN Setup</span>
      <a href="/" class="back">← Zurück</a>
    </div>
    
    <div class="status-box">
      <div class="status-row">
        <span class="status-label">AP (immer aktiv):</span>
        <span class="status-value connected">ADD2go</span>
      </div>
      <div class="status-row">
        <span class="status-label">AP IP:</span>
        <span class="status-value ip-display">192.168.4.1</span>
      </div>
      <div class="status-row">
        <span class="status-label">Externes WLAN:</span>
        <span class="status-value" id="staStatus">-</span>
      </div>
      <div class="status-row" id="staIPRow" style="display:none;">
        <span class="status-label">Externe IP:</span>
        <span class="status-value ip-display" id="staIP">-</span>
      </div>
    </div>
    
    <div id="message"></div>
    
    <h2>Verfügbare Netzwerke</h2>
    <div id="networkList" class="network-list">
      <div class="loading"><span class="spinner"></span> Scanne...</div>
    </div>
    <button class="btn btn-secondary" onclick="scanNetworks()">🔄 Neu scannen</button>
    
    <div id="connectForm" style="display:none;">
      <h2>Verbinden mit: <span id="selectedSSID"></span></h2>
      <input type="password" id="password" placeholder="Passwort eingeben">
      <button class="btn btn-primary" id="connectBtn" onclick="connectWifi()">Verbinden</button>
    </div>
    
    <div id="disconnectSection" style="display:none;">
      <h2>Verbindung trennen</h2>
      <button class="btn btn-danger" onclick="disconnectWifi()">WLAN trennen</button>
    </div>
  </div>
  
  <script>
    let selectedSSID = '';
    let isConnected = false;
    
    function showMessage(text, type) {
      const el = document.getElementById('message');
      el.innerHTML = '<div class="message ' + type + '">' + text + '</div>';
      setTimeout(() => el.innerHTML = '', 5000);
    }
    
    function updateStatus() {
      fetch('/api/status')
        .then(r => r.json())
        .then(data => {
          const staStatus = document.getElementById('staStatus');
          const staIPRow = document.getElementById('staIPRow');
          const staIP = document.getElementById('staIP');
          const disconnectSection = document.getElementById('disconnectSection');
          
          if (data.sta_connected) {
            staStatus.textContent = data.sta_ssid;
            staStatus.className = 'status-value connected';
            staIP.textContent = data.sta_ip;
            staIPRow.style.display = 'flex';
            disconnectSection.style.display = 'block';
            isConnected = true;
          } else if (data.sta_enabled) {
            staStatus.textContent = 'Verbinde mit ' + data.sta_ssid + '...';
            staStatus.className = 'status-value';
            staIPRow.style.display = 'none';
            disconnectSection.style.display = 'block';
            isConnected = false;
          } else {
            staStatus.textContent = 'Nicht verbunden';
            staStatus.className = 'status-value disconnected';
            staIPRow.style.display = 'none';
            disconnectSection.style.display = 'none';
            isConnected = false;
          }
        })
        .catch(() => {});
    }
    
    function scanNetworks() {
      document.getElementById('networkList').innerHTML = '<div class="loading"><span class="spinner"></span> Scanne...</div>';
      document.getElementById('connectForm').style.display = 'none';
      
      // Async Scan mit Polling
      function pollScan() {
        fetch('/api/scan')
          .then(r => {
            // 202 = noch am Scannen, weiter pollen
            if (r.status === 202) {
              setTimeout(pollScan, 500);
              return null;
            }
            return r.json();
          })
          .then(data => {
            if (!data) return;  // War 202, wird weiter gepollt
            
            let html = '';
            if (data.networks && data.networks.length > 0) {
              data.networks.forEach(net => {
                const bars = net.rssi > -50 ? '████' : net.rssi > -60 ? '███░' : net.rssi > -70 ? '██░░' : net.rssi > -80 ? '█░░░' : '░░░░';
                const lock = net.secure ? '🔒' : '';
                html += '<div class="network-item" onclick="selectNetwork(\'' + net.ssid.replace(/'/g, "\\'") + '\', ' + net.secure + ')">';
                html += '<span class="network-name">' + net.ssid + '<span class="network-lock">' + lock + '</span></span>';
                html += '<span class="network-signal">' + bars + ' ' + net.rssi + '</span>';
                html += '</div>';
              });
            } else {
              html = '<div class="loading">Keine Netzwerke gefunden</div>';
            }
            document.getElementById('networkList').innerHTML = html;
          })
          .catch(() => {
            document.getElementById('networkList').innerHTML = '<div class="loading">Fehler beim Scannen</div>';
          });
      }
      
      pollScan();
    }
    
    function selectNetwork(ssid, secure) {
      selectedSSID = ssid;
      document.getElementById('selectedSSID').textContent = ssid;
      document.getElementById('connectForm').style.display = 'block';
      document.getElementById('password').value = '';
      document.getElementById('password').type = secure ? 'password' : 'text';
      document.getElementById('password').placeholder = secure ? 'Passwort eingeben' : 'Kein Passwort nötig';
      
      // Highlight
      document.querySelectorAll('.network-item').forEach(el => el.classList.remove('selected'));
      event.currentTarget.classList.add('selected');
    }
    
    function connectWifi() {
      const password = document.getElementById('password').value;
      const btn = document.getElementById('connectBtn');
      btn.disabled = true;
      btn.textContent = 'Verbinde...';
      
      fetch('/api/connect', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'ssid=' + encodeURIComponent(selectedSSID) + '&password=' + encodeURIComponent(password)
      })
        .then(r => r.json())
        .then(data => {
          btn.disabled = false;
          btn.textContent = 'Verbinden';
          if (data.success) {
            showMessage('Verbindung wird hergestellt... Neue IP: ' + data.ip, 'success');
            setTimeout(updateStatus, 3000);
          } else {
            showMessage('Fehler: ' + data.error, 'error');
          }
        })
        .catch(() => {
          btn.disabled = false;
          btn.textContent = 'Verbinden';
          showMessage('Verbindungsfehler', 'error');
        });
    }
    
    function disconnectWifi() {
      fetch('/api/disconnect', { method: 'POST' })
        .then(r => r.json())
        .then(data => {
          showMessage('WLAN getrennt', 'success');
          updateStatus();
        })
        .catch(() => showMessage('Fehler', 'error'));
    }
    
    // Initial
    updateStatus();
    scanNetworks();
    setInterval(updateStatus, 5000);
  </script>
</body>
</html>
)rawliteral";

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println();
  Serial.println("========================================");
  Serial.println("ADD2go Sender - v1.2.4");
  Serial.println("========================================");
  Serial.println();
  
  // Gespeicherte WLAN-Daten laden
  preferences.begin("add2go", false);
  savedSSID = preferences.getString("ssid", "");
  savedPassword = preferences.getString("pass", "");
  staEnabled = preferences.getBool("sta_enabled", false);
  preferences.end();
  
  Serial.println("[NVS] Lade gespeicherte Daten...");
  if (savedSSID.length() > 0) {
    Serial.print("[NVS] SSID: ");
    Serial.println(savedSSID);
    Serial.print("[NVS] Passwort: ");
    Serial.println(savedPassword.length() > 0 ? "(gespeichert)" : "(leer)");
    Serial.print("[NVS] Auto-Connect: ");
    Serial.println(staEnabled ? "JA" : "NEIN");
  } else {
    Serial.println("[NVS] Keine WLAN-Daten gespeichert");
  }
  
  // RS232
  Serial2.begin(19200, SERIAL_8N1, RXD2, TXD2);
  Serial.println("[OK] RS232 gestartet (19200 Baud, GPIO16)");
  
  // Watchdog FRÜH initialisieren (vor WLAN-Warteschleife!)
  // idle_core_mask = 0: Nur loopTask überwachen, nicht IDLE Tasks
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WATCHDOG_TIMEOUT_SEC * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  
  esp_err_t err;
  err = esp_task_wdt_init(&wdt_config);
  if (err == ESP_ERR_INVALID_STATE) {
    Serial.println("[WDT] Bereits vom System initialisiert");
  }
  err = esp_task_wdt_add(NULL);
  if (err == ESP_OK) {
    Serial.printf("[OK] Watchdog: %d Sekunden\n", WATCHDOG_TIMEOUT_SEC);
  }
  
  // WiFi APSTA Modus (AP + Station gleichzeitig)
  WiFi.mode(WIFI_AP_STA);
  
  // Access Point starten (immer aktiv)
  WiFi.softAP(AP_SSID, AP_PASS);
  wifi_config_t conf;
  esp_wifi_get_config(WIFI_IF_AP, &conf);
  conf.ap.beacon_interval = 100;
  esp_wifi_set_config(WIFI_IF_AP, &conf);
  
  Serial.print("[OK] WiFi AP gestartet: ");
  Serial.println(AP_SSID);
  Serial.print("[OK] AP IP: ");
  Serial.println(WiFi.softAPIP());
  
  // Station verbinden (falls gespeichert)
  if (staEnabled && savedSSID.length() > 0) {
    Serial.print("[STA] Verbinde mit: ");
    Serial.println(savedSSID);
    WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
    
    // Warten auf Verbindung (max 15 Sek beim Boot)
    // WDT-Reset damit Watchdog nicht während Boot zuschlägt
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
      delay(500);
      Serial.print(".");
      esp_task_wdt_reset();  // Watchdog füttern während Warten
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
      staConnected = true;
      staIP = WiFi.localIP();
      Serial.print("[STA] Verbunden! IP: ");
      Serial.println(staIP);
    } else {
      Serial.println("[STA] Verbindung fehlgeschlagen - läuft im Hintergrund weiter");
    }
  }
  
  // mDNS starten (add2go.local)
  if (MDNS.begin(MDNS_HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws", "tcp", 81);
    Serial.print("[OK] mDNS: http://");
    Serial.print(MDNS_HOSTNAME);
    Serial.println(".local");
  } else {
    Serial.println("[!] mDNS fehlgeschlagen");
  }
  
  // UDP (für Hardware-Empfänger)
  udp.begin(UDP_PORT);
  Serial.print("[OK] UDP Port: ");
  Serial.println(UDP_PORT);
  
  // WebServer Routen
  server.on("/", HTTP_GET, handleRoot);
  server.on("/setup", HTTP_GET, handleSetup);
  server.on("/manifest.json", HTTP_GET, handleManifest);
  server.on("/icon-192.png", HTTP_GET, handleIcon192);
  server.on("/icon-512.png", HTTP_GET, handleIcon512);
  server.on("/api/status", HTTP_GET, handleApiStatus);
  server.on("/api/scan", HTTP_GET, handleApiScan);
  server.on("/api/connect", HTTP_POST, handleApiConnect);
  server.on("/api/disconnect", HTTP_POST, handleApiDisconnect);
  server.begin();
  Serial.println("[OK] WebServer gestartet (Port 80)");
  
  // WebSocket Server
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("[OK] WebSocket gestartet (Port 81)");
  
  Serial.println();
  Serial.println("=========== ZUGRIFF ===========");
  Serial.println("Im ADD2go WLAN:");
  Serial.println("  WebApp:  http://192.168.4.1");
  Serial.println("  Setup:   http://192.168.4.1/setup");
  Serial.println();
  Serial.println("Im Heimnetz (mDNS):");
  Serial.println("  WebApp:  http://add2go.local");
  Serial.println("  Setup:   http://add2go.local/setup");
  if (staConnected) {
    Serial.print("  oder:    http://");
    Serial.println(staIP);
  }
  Serial.println("===============================");
  Serial.println();
  Serial.println("Warte auf ADD2 Waage...");
  Serial.println();
}

// ============================================
// WEBSERVER HANDLER
// ============================================
void handleRoot() {
  server.send_P(200, "text/html", WEBAPP_HTML);
}

void handleSetup() {
  server.send_P(200, "text/html", SETUP_HTML);
}

void handleManifest() {
  String manifest = "{"
    "\"name\":\"ADD2go Waage\","
    "\"short_name\":\"ADD2go\","
    "\"description\":\"Wireless Waagen-Display\","
    "\"start_url\":\"/\","
    "\"display\":\"standalone\","
    "\"background_color\":\"#000000\","
    "\"theme_color\":\"#000000\","
    "\"orientation\":\"any\","
    "\"icons\":["
      "{\"src\":\"/icon-192.png\",\"sizes\":\"192x192\",\"type\":\"image/svg+xml\",\"purpose\":\"any maskable\"},"
      "{\"src\":\"/icon-512.png\",\"sizes\":\"512x512\",\"type\":\"image/svg+xml\",\"purpose\":\"any maskable\"}"
    "]"
  "}";
  server.send(200, "application/json", manifest);
}

void handleIcon192() {
  // SVG Icon: "ADD2" oben, "go" unten, Orange auf Schwarz
  String svg = R"(
<svg xmlns="http://www.w3.org/2000/svg" width="192" height="192" viewBox="0 0 192 192">
  <rect width="192" height="192" fill="#000"/>
  <text x="96" y="85" text-anchor="middle" font-family="Arial,sans-serif" font-weight="bold" font-size="52" fill="#f59e0b">ADD2</text>
  <text x="96" y="145" text-anchor="middle" font-family="Arial,sans-serif" font-weight="bold" font-size="52" fill="#f59e0b">go</text>
</svg>)";
  server.send(200, "image/svg+xml", svg);
}

void handleIcon512() {
  // Gleiches SVG, skaliert automatisch
  String svg = R"(
<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 512 512">
  <rect width="512" height="512" fill="#000"/>
  <text x="256" y="220" text-anchor="middle" font-family="Arial,sans-serif" font-weight="bold" font-size="138" fill="#f59e0b">ADD2</text>
  <text x="256" y="380" text-anchor="middle" font-family="Arial,sans-serif" font-weight="bold" font-size="138" fill="#f59e0b">go</text>
</svg>)";
  server.send(200, "image/svg+xml", svg);
}

void handleApiStatus() {
  String json = "{";
  json += "\"ap_ssid\":\"" + String(AP_SSID) + "\",";
  json += "\"ap_ip\":\"" + WiFi.softAPIP().toString() + "\",";
  json += "\"sta_enabled\":" + String(staEnabled ? "true" : "false") + ",";
  json += "\"sta_connected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"sta_ssid\":\"" + savedSSID + "\",";
  json += "\"sta_ip\":\"" + WiFi.localIP().toString() + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleApiScan() {
  // Async-Scan: Startet Scan und pollt Ergebnis
  // Verhindert WDT-Trigger bei langem Scan
  
  if (!scanRunning) {
    // Neuen Scan starten
    WiFi.scanDelete();
    WiFi.scanNetworks(true);  // true = async
    scanRunning = true;
    server.send(202, "application/json", "{\"status\":\"scanning\"}");
    return;
  }
  
  // Scan-Status prüfen
  int n = WiFi.scanComplete();
  
  if (n == WIFI_SCAN_RUNNING) {
    // Noch am Scannen
    server.send(202, "application/json", "{\"status\":\"scanning\"}");
    return;
  }
  
  // Scan beendet
  scanRunning = false;
  
  if (n < 0) {
    // Fehler
    WiFi.scanDelete();
    server.send(200, "application/json", "{\"networks\":[]}");
    return;
  }
  
  // Ergebnisse senden
  String json = "{\"networks\":[";
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    // SSID escapen (Anführungszeichen und Backslash)
    String ssid = WiFi.SSID(i);
    ssid.replace("\\", "\\\\");
    ssid.replace("\"", "\\\"");
    json += "{";
    json += "\"ssid\":\"" + ssid + "\",";
    json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
    json += "\"secure\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false");
    json += "}";
  }
  json += "]}";
  
  WiFi.scanDelete();
  server.send(200, "application/json", json);
}

void handleApiConnect() {
  if (!server.hasArg("ssid")) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"SSID fehlt\"}");
    return;
  }
  
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  
  Serial.print("[STA] Verbinde mit: ");
  Serial.println(ssid);
  
  // Verbindung herstellen
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid.c_str(), password.c_str());
  
  // Warten auf Verbindung (max 10 Sek)
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < WIFI_CONNECT_TIMEOUT) {
    delay(250);
    esp_task_wdt_reset();  // Watchdog während Wartezeit füttern
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    // Speichern
    preferences.begin("add2go", false);
    preferences.putString("ssid", ssid);
    preferences.putString("pass", password);
    preferences.putBool("sta_enabled", true);
    preferences.end();
    
    savedSSID = ssid;
    savedPassword = password;
    staEnabled = true;
    staConnected = true;
    staIP = WiFi.localIP();
    
    Serial.print("[STA] Verbunden! IP: ");
    Serial.println(staIP);
    
    String json = "{\"success\":true,\"ip\":\"" + staIP.toString() + "\"}";
    server.send(200, "application/json", json);
  } else {
    Serial.println("[STA] Verbindung fehlgeschlagen");
    WiFi.disconnect();
    server.send(200, "application/json", "{\"success\":false,\"error\":\"Verbindung fehlgeschlagen\"}");
  }
}

void handleApiDisconnect() {
  Serial.println("[STA] Trenne WLAN...");
  
  WiFi.disconnect();
  
  preferences.begin("add2go", false);
  preferences.putBool("sta_enabled", false);
  preferences.end();
  
  staEnabled = false;
  staConnected = false;
  
  server.send(200, "application/json", "{\"success\":true}");
}

// ============================================
// WEBSOCKET EVENT HANDLER
// ============================================
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WS] Client #%u getrennt\n", num);
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[WS] Client #%u verbunden von %s\n", num, ip.toString().c_str());
        String msg = aktuellesGewicht + "|" + String(getSignalRSSI());
        webSocket.sendTXT(num, msg);
      }
      break;
    default:
      break;
  }
}

// ============================================
// SIGNAL RSSI ERMITTELN
// ============================================
int8_t getSignalRSSI() {
  // Wenn mit externem WLAN verbunden: dessen RSSI
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.RSSI();
  }
  
  // Sonst: RSSI des ersten AP-Clients
  wifi_sta_list_t stationList;
  if (esp_wifi_ap_get_sta_list(&stationList) == ESP_OK && stationList.num > 0) {
    return stationList.sta[0].rssi;
  }
  
  return -100;
}

// ============================================
// LOOP
// ============================================
void loop() {
  esp_task_wdt_reset();
  
  // RS232 lesen
  leseRS232();
  esp_task_wdt_reset();
  
  // Waage Timeout
  pruefeWaageTimeout();
  
  // WebServer (kann bei vielen Requests blockieren)
  server.handleClient();
  esp_task_wdt_reset();
  
  // WebSocket (kann bei Client-Problemen blockieren)
  webSocket.loop();
  esp_task_wdt_reset();
  
  // UDP + WebSocket senden (10 Hz)
  if (millis() - lastSendTime >= SEND_INTERVAL) {
    sendeUDP();
    sendeWebSocket();
    lastSendTime = millis();
  }
  
  // AP Health Check
  if (millis() - lastAPCheck >= AP_CHECK_INTERVAL) {
    pruefeAP();
    lastAPCheck = millis();
  }
  
  // Station Reconnect prüfen
  if (staEnabled && WiFi.status() != WL_CONNECTED) {
    if (millis() - lastSTARetry >= STA_RETRY_INTERVAL) {
      Serial.println("[STA] Reconnect...");
      WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
      lastSTARetry = millis();
    }
  }
  
  // Station Status aktualisieren
  staConnected = (WiFi.status() == WL_CONNECTED);
  if (staConnected) {
    staIP = WiFi.localIP();
  }
  
  // Yield für WiFi/TCP Stack
  delay(1);
}

// ============================================
// RS232 LESEN
// ============================================
void leseRS232() {
  // Max 256 Bytes pro loop() verarbeiten
  // Verhindert Endlos-Schleife bei kontinuierlichem Datenfluss
  int processed = 0;
  
  while (Serial2.available() > 0 && processed < 256) {
    processed++;
    uint8_t b = Serial2.read();
    
    if (b == 0x23) {
      bufferIndex = 0;
      buffer[bufferIndex++] = b;
      frameStarted = true;
    }
    else if (frameStarted) {
      if (bufferIndex < BUFFER_SIZE) {
        buffer[bufferIndex++] = b;
      }
      if (b == 0x0A) {
        verarbeiteFrame();
        frameStarted = false;
        bufferIndex = 0;
      }
    }
  }
}

// ============================================
// FRAME VERARBEITEN
// ============================================
void verarbeiteFrame() {
  frameCount++;
  
  if (bufferIndex < 7) return;
  if (buffer[0] != 0x23) return;
  if (buffer[1] != 0x03) return;
  if (buffer[3] != 0x18 || buffer[4] != 0x00) return;
  if (buffer[bufferIndex - 1] != 0x0A) return;
  
  String gewichtStr = "";
  for (int i = 5; i < bufferIndex - 1; i++) {
    char c = (char)buffer[i];
    if (c >= 0x20 && c <= 0x7E) {
      gewichtStr += c;
    }
  }
  
  gewichtStr.trim();
  
  if (gewichtStr.length() > 0) {
    aktuellesGewicht = gewichtStr;
    letzterEmpfang = millis();
    waageOnline = true;
    
    if (frameCount % 10 == 0) {
      Serial.print("[RS232] Gewicht: ");
      Serial.print(aktuellesGewicht);
      Serial.print(" (Frame #");
      Serial.print(frameCount);
      Serial.println(")");
    }
  }
}

// ============================================
// WAAGE TIMEOUT
// ============================================
void pruefeWaageTimeout() {
  if (waageOnline && (millis() - letzterEmpfang > WAAGE_TIMEOUT)) {
    waageOnline = false;
    aktuellesGewicht = "OFF";
    Serial.println("[WARNUNG] Waage Timeout - sende OFF");
  }
}

// ============================================
// UDP SENDEN
// ============================================
void sendeUDP() {
  udp.beginPacket(broadcastIP, UDP_PORT);
  udp.print(aktuellesGewicht);
  udp.endPacket();
  
  sendCount++;
  
  if (sendCount % 100 == 0) {
    int clients = WiFi.softAPgetStationNum();
    Serial.print("[UDP] Gesendet: \"");
    Serial.print(aktuellesGewicht);
    Serial.print("\" (");
    Serial.print(clients);
    Serial.println(" AP Clients)");
  }
}

// ============================================
// WEBSOCKET SENDEN
// ============================================
void sendeWebSocket() {
  uint8_t numClients = webSocket.connectedClients();
  if (numClients == 0) return;
  
  int8_t rssi = getSignalRSSI();
  String msg = aktuellesGewicht + "|" + String(rssi);
  webSocket.broadcastTXT(msg);
}

// ============================================
// AP HEALTH CHECK
// ============================================
void pruefeAP() {
  wifi_mode_t mode;
  esp_wifi_get_mode(&mode);
  
  if (mode != WIFI_MODE_AP && mode != WIFI_MODE_APSTA) {
    Serial.println("[WARNUNG] AP nicht mehr aktiv - Neustart...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(AP_SSID, AP_PASS);
    if (staEnabled && savedSSID.length() > 0) {
      WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
    }
    Serial.println("[OK] AP neu gestartet");
  }
  
  // Status Log
  int apClients = WiFi.softAPgetStationNum();
  uint8_t wsClients = webSocket.connectedClients();
  
  Serial.print("[STATUS] AP Clients: ");
  Serial.print(apClients);
  Serial.print(" | WS Clients: ");
  Serial.print(wsClients);
  if (staConnected) {
    Serial.print(" | STA: ");
    Serial.print(savedSSID);
    Serial.print(" (");
    Serial.print(staIP);
    Serial.print(")");
  }
  Serial.println();
}
