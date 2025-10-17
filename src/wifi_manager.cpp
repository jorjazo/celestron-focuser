/*
    WiFi Manager Implementation for ESP32 Celestron Focuser Controller
    
    Copyright (C) 2024
*/

#include "wifi_manager.h"

// Global WiFi Manager instance
WiFiManager wifiManager;

// ============================================================================
// Constructor/Destructor
// ============================================================================

WiFiManager::WiFiManager() {
    _apMode = false;
    _stationMode = false;
    _wifiConnected = false;
    _webServer = nullptr;
    _webSocketServer = nullptr;
    _hostname = DEFAULT_HOSTNAME;
}

WiFiManager::~WiFiManager() {
    if (_webServer) {
        delete _webServer;
    }
    if (_webSocketServer) {
        delete _webSocketServer;
    }
}

// ============================================================================
// Initialization
// ============================================================================

bool WiFiManager::begin() {
    Serial.println("INFO: Initializing WiFi Manager...");
    
    // Initialize SPIFFS (optional)
    if (!SPIFFS.begin(true)) {
        Serial.println("WARNING: SPIFFS Mount Failed - using inline HTML fallback");
        // Continue anyway, we have fallback HTML
    } else {
        Serial.println("INFO: SPIFFS initialized successfully");
    }
    
    // Initialize preferences
    _preferences.begin(PREF_NAMESPACE, false);
    
    // Load saved configuration
    if (!loadWiFiConfig()) {
        Serial.println("INFO: No saved WiFi configuration found");
        _hostname = DEFAULT_HOSTNAME;
    }
    
    // Set hostname
    WiFi.setHostname(_hostname.c_str());
    
    // Register WiFi event handler
    WiFi.onEvent(std::bind(&WiFiManager::_onWiFiEvent, this, std::placeholders::_1));
    
    // Try to connect to saved WiFi first
    if (!_ssid.isEmpty()) {
        Serial.println("INFO: Attempting to connect to saved WiFi: " + _ssid);
        if (startStation()) {
            return true;
        }
    }
    
    // If station mode fails, start AP mode
    Serial.println("INFO: Starting AP mode for WiFi configuration");
    return startAP();
}

void WiFiManager::handle() {
    // Handle WebSocket connections
    if (_webSocketServer) {
        _webSocketServer->loop();
    }
    
    // Handle WiFi reconnection in station mode
    if (_stationMode && !_wifiConnected && !_ssid.isEmpty()) {
        static unsigned long lastReconnectAttempt = 0;
        if (millis() - lastReconnectAttempt > WIFI_RECONNECT_DELAY) {
            Serial.println("INFO: Attempting to reconnect to WiFi...");
            connectToWiFi();
            lastReconnectAttempt = millis();
        }
    }
}

// ============================================================================
// WiFi Control
// ============================================================================

bool WiFiManager::startAP() {
    Serial.println("INFO: Starting Access Point mode...");
    
    // Disconnect from any existing WiFi
    WiFi.disconnect(true);
    delay(100);
    
    // Configure AP mode
    WiFi.mode(WIFI_AP);
    
    // Start AP
    bool success = WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL, false, WIFI_AP_MAX_CONNECTIONS);
    
    if (success) {
        _apMode = true;
        _stationMode = false;
        _wifiConnected = true; // AP is always "connected"
        
        Serial.println("SUCCESS: Access Point started");
        Serial.println("INFO: AP SSID: " + String(WIFI_AP_SSID));
        Serial.println("INFO: AP Password: " + String(WIFI_AP_PASSWORD));
        Serial.println("INFO: AP IP: " + WiFi.softAPIP().toString());
        
        // Setup web server
        setupWebServer();
        
        return true;
    } else {
        Serial.println("ERROR: Failed to start Access Point");
        return false;
    }
}

bool WiFiManager::startStation() {
    Serial.println("INFO: Starting Station mode...");
    
    if (_ssid.isEmpty()) {
        Serial.println("ERROR: No SSID configured");
        return false;
    }
    
    // Disconnect from any existing WiFi
    WiFi.disconnect(true);
    delay(100);
    
    // Configure station mode
    WiFi.mode(WIFI_STA);
    
    // Set hostname
    WiFi.setHostname(_hostname.c_str());
    
    // Connect to WiFi
    _stationMode = true;
    _apMode = false;
    
    return connectToWiFi();
}

bool WiFiManager::connectToWiFi() {
    if (_ssid.isEmpty()) {
        return false;
    }
    
    Serial.println("INFO: Connecting to WiFi: " + _ssid);
    
    WiFi.begin(_ssid.c_str(), _password.c_str());
    
    // Wait for connection
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < (WIFI_CONNECT_TIMEOUT * 1000)) {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        _wifiConnected = true;
        Serial.println("");
        Serial.println("SUCCESS: WiFi connected!");
        Serial.println("INFO: IP Address: " + WiFi.localIP().toString());
        Serial.println("INFO: SSID: " + WiFi.SSID());
        Serial.println("INFO: Signal Strength: " + String(WiFi.RSSI()) + " dBm");
        
        // Setup web server
        setupWebServer();
        
    // Start mDNS service
    startmDNS();
    
    // Call connected callback
    if (_onConnected) {
        _onConnected();
    }
    
    return true;
    } else {
        _wifiConnected = false;
        Serial.println("");
        Serial.println("ERROR: WiFi connection failed");
        return false;
    }
}

void WiFiManager::disconnect() {
    stopmDNS();
    WiFi.disconnect(true);
    _wifiConnected = false;
    _apMode = false;
    _stationMode = false;
}

// ============================================================================
// Configuration Management
// ============================================================================

void WiFiManager::saveWiFiConfig(const String& ssid, const String& password) {
    _preferences.putString(PREF_SSID_KEY, ssid);
    _preferences.putString(PREF_PASSWORD_KEY, password);
    _ssid = ssid;
    _password = password;
    
    Serial.println("INFO: WiFi configuration saved");
}

void WiFiManager::saveHostname(const String& hostname) {
    _preferences.putString(PREF_HOSTNAME_KEY, hostname);
    _hostname = hostname;
    WiFi.setHostname(_hostname.c_str());
    
    Serial.println("INFO: Hostname saved: " + _hostname);
}

bool WiFiManager::loadWiFiConfig() {
    _ssid = _preferences.getString(PREF_SSID_KEY, "");
    _password = _preferences.getString(PREF_PASSWORD_KEY, "");
    _hostname = _preferences.getString(PREF_HOSTNAME_KEY, DEFAULT_HOSTNAME);
    
    return !_ssid.isEmpty();
}

void WiFiManager::clearWiFiConfig() {
    _preferences.clear();
    _ssid = "";
    _password = "";
    _hostname = DEFAULT_HOSTNAME;
    
    Serial.println("INFO: WiFi configuration cleared");
}

// ============================================================================
// Status
// ============================================================================

bool WiFiManager::isConnected() {
    return _wifiConnected;
}

bool WiFiManager::isAPMode() {
    return _apMode;
}

String WiFiManager::getIPAddress() {
    if (_apMode) {
        IPAddress apIP = WiFi.softAPIP();
        return apIP.toString();
    } else if (_wifiConnected) {
        IPAddress localIP = WiFi.localIP();
        return localIP.toString();
    }
    return "";
}

String WiFiManager::getSSID() {
    if (_apMode) {
        return String(WIFI_AP_SSID);
    } else if (_wifiConnected) {
        String ssid = WiFi.SSID();
        return ssid.isEmpty() ? _ssid : ssid;
    }
    return _ssid;
}

String WiFiManager::getHostname() {
    return _hostname;
}

// ============================================================================
// Web Interface
// ============================================================================

void WiFiManager::setupWebServer() {
    if (_webServer) {
        delete _webServer;
    }
    
    _webServer = new AsyncWebServer(WEB_SERVER_PORT);
    _setupWebRoutes();
    _webServer->begin();
    
    // Setup WebSocket server
    if (_webSocketServer) {
        delete _webSocketServer;
    }
    
    _webSocketServer = new WebSocketsServer(WEBSOCKET_PORT);
    _webSocketServer->onEvent([](uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
        if (type == WStype_TEXT) {
            wifiManager.handleWebSocketMessage(num, payload, length);
        }
    });
    _webSocketServer->begin();
    
    Serial.println("INFO: Web server started on port " + String(WEB_SERVER_PORT));
    Serial.println("INFO: WebSocket server started on port " + String(WEBSOCKET_PORT));
}

void WiFiManager::handleWebSocketMessage(uint8_t num, uint8_t *payload, size_t length) {
    String message = String((char*)payload, length);
    
    Serial.println("INFO: WebSocket message: " + message);
    
    // Parse JSON message
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    
    if (error) {
        Serial.println("ERROR: JSON parsing failed: " + String(error.c_str()));
        return;
    }
    
    String command = doc["command"];
    
    if (command == "getStatus") {
        String statusJson = _getWiFiStatusJSON();
        _webSocketServer->sendTXT(num, statusJson);
    }
    else if (command == "setWiFi") {
        String ssid = doc["ssid"];
        String password = doc["password"];
        String hostname = doc["hostname"];
        
        if (!hostname.isEmpty()) {
            saveHostname(hostname);
        }
        
        saveWiFiConfig(ssid, password);
        
        // Send response
        JsonDocument response;
        response["status"] = "success";
        response["message"] = "WiFi configuration saved";
        
        String responseStr;
        serializeJson(response, responseStr);
        _webSocketServer->sendTXT(num, responseStr);
        
        // Restart in station mode
        delay(1000);
        startStation();
    }
    else if (command == "clearWiFi") {
        clearWiFiConfig();
        
        // Send response
        JsonDocument response;
        response["status"] = "success";
        response["message"] = "WiFi configuration cleared";
        
        String responseStr;
        serializeJson(response, responseStr);
        _webSocketServer->sendTXT(num, responseStr);
        
        // Restart in AP mode
        delay(1000);
        startAP();
    }
    else if (command.startsWith("focuser:")) {
        // Handle focuser commands
        if (_focuserCallback) {
            bool success = _focuserCallback(command, doc);
            
            // Send response
            JsonDocument response;
            response["status"] = success ? "success" : "error";
            response["command"] = command;
            
            String responseStr;
            serializeJson(response, responseStr);
            _webSocketServer->sendTXT(num, responseStr);
        }
    }
}

// ============================================================================
// mDNS Support
// ============================================================================

bool WiFiManager::startmDNS() {
    if (!_wifiConnected || _apMode) {
        return false; // Only start mDNS in station mode
    }
    
    Serial.println("INFO: Starting mDNS service...");
    
    // Initialize mDNS
    if (!MDNS.begin(_hostname.c_str())) {
        Serial.println("ERROR: mDNS initialization failed");
        return false;
    }
    
    // Add service for web server
    MDNS.addService("http", "tcp", WEB_SERVER_PORT);
    MDNS.addService("ws", "tcp", WEBSOCKET_PORT);
    
    // Add service description
    MDNS.addServiceTxt("http", "tcp", "service", "celestron-focuser");
    MDNS.addServiceTxt("http", "tcp", "version", "1.0");
    MDNS.addServiceTxt("http", "tcp", "description", "Celestron Focuser WiFi Controller");
    
    Serial.println("SUCCESS: mDNS service started");
    Serial.println("INFO: Access device at: http://" + _hostname + ".local");
    Serial.println("INFO: WebSocket at: ws://" + _hostname + ".local:" + String(WEBSOCKET_PORT));
    
    return true;
}

void WiFiManager::stopmDNS() {
    Serial.println("INFO: Stopping mDNS service...");
    MDNS.end();
}

String WiFiManager::getmDNSHostname() {
    return _hostname + ".local";
}

// ============================================================================
// Callbacks
// ============================================================================

void WiFiManager::onWiFiConnected(std::function<void()> callback) {
    _onConnected = callback;
}

void WiFiManager::onWiFiDisconnected(std::function<void()> callback) {
    _onDisconnected = callback;
}

void WiFiManager::setFocuserCallback(std::function<bool(String, JsonDocument&)> callback) {
    _focuserCallback = callback;
}

void WiFiManager::sendFocuserStatus(uint8_t num, bool connected, uint32_t position, uint32_t target, uint8_t speed, bool moving) {
    if (!_webSocketServer) return;
    
    JsonDocument doc;
    doc["type"] = "focuserStatus";
    doc["connected"] = connected;
    doc["position"] = position;
    doc["target"] = target;
    doc["speed"] = speed;
    doc["moving"] = moving;
    
    String jsonString;
    serializeJson(doc, jsonString);
    _webSocketServer->sendTXT(num, jsonString);
}

// ============================================================================
// Private Methods
// ============================================================================

void WiFiManager::_setupWebRoutes() {
    // Root page
    _webServer->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) {
        _handleRoot(request);
    });
    
    // WiFi configuration endpoint
    _webServer->on("/api/wifi", HTTP_POST, [this](AsyncWebServerRequest *request) {
        _handleWiFiConfig(request);
    });
    
    // Status endpoint
    _webServer->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
        _handleStatus(request);
    });
    
    // 404 handler
    _webServer->onNotFound([this](AsyncWebServerRequest *request) {
        _handleNotFound(request);
    });
}

void WiFiManager::_handleRoot(AsyncWebServerRequest *request) {
    // Try to serve from SPIFFS first, fallback to inline HTML
    if (SPIFFS.exists("/index.html")) {
        request->send(SPIFFS, "/index.html", "text/html");
    } else {
        // Fallback inline HTML with focuser control interface
        String html = R"rawliteral(<!DOCTYPE html>
<html>
<head>
    <title>Celestron Focuser Controller</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 0; padding: 20px; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; }
        .container { max-width: 1200px; margin: 0 auto; }
        .header { background: white; padding: 20px; border-radius: 15px; box-shadow: 0 4px 20px rgba(0,0,0,0.1); margin-bottom: 20px; text-align: center; }
        .main-content { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
        .panel { background: white; padding: 25px; border-radius: 15px; box-shadow: 0 4px 20px rgba(0,0,0,0.1); }
        .focuser-panel { grid-column: 1 / -1; }
        h1 { color: #333; margin: 0; font-size: 2.5em; }
        h2 { color: #555; margin-top: 0; border-bottom: 2px solid #eee; padding-bottom: 10px; }
        .status { padding: 15px; margin: 15px 0; border-radius: 10px; font-weight: bold; }
        .status.connected { background: linear-gradient(135deg, #d4edda, #c3e6cb); color: #155724; border: 1px solid #c3e6cb; }
        .status.disconnected { background: linear-gradient(135deg, #f8d7da, #f5c6cb); color: #721c24; border: 1px solid #f5c6cb; }
        .status.warning { background: linear-gradient(135deg, #fff3cd, #ffeaa7); color: #856404; border: 1px solid #ffeaa7; }
        .form-group { margin: 20px 0; }
        label { display: block; margin-bottom: 8px; font-weight: 600; color: #555; }
        input[type="text"], input[type="password"], input[type="number"] { width: 100%; padding: 12px; border: 2px solid #ddd; border-radius: 8px; box-sizing: border-box; font-size: 16px; transition: border-color 0.3s; }
        input[type="text"]:focus, input[type="password"]:focus, input[type="number"]:focus { outline: none; border-color: #667eea; }
        button { background: linear-gradient(135deg, #667eea, #764ba2); color: white; padding: 12px 24px; border: none; border-radius: 8px; cursor: pointer; margin: 5px; font-size: 16px; font-weight: 600; transition: transform 0.2s, box-shadow 0.2s; }
        button:hover { transform: translateY(-2px); box-shadow: 0 4px 15px rgba(102, 126, 234, 0.4); }
        button:active { transform: translateY(0); }
        button.danger { background: linear-gradient(135deg, #dc3545, #c82333); }
        button.success { background: linear-gradient(135deg, #28a745, #20c997); }
        button.warning { background: linear-gradient(135deg, #ffc107, #fd7e14); }
        button.small { padding: 8px 16px; font-size: 14px; }
        .buttons { text-align: center; margin: 20px 0; }
        .info { background: linear-gradient(135deg, #d1ecf1, #bee5eb); color: #0c5460; padding: 15px; border-radius: 10px; margin: 15px 0; }
        .focuser-controls { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; margin: 20px 0; }
        .position-display { background: linear-gradient(135deg, #f8f9fa, #e9ecef); padding: 20px; border-radius: 10px; text-align: center; margin: 15px 0; }
        .position-value { font-size: 2em; font-weight: bold; color: #667eea; }
        .speed-control { display: flex; align-items: center; gap: 10px; }
        .speed-slider { flex: 1; }
        .movement-controls { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; margin: 20px 0; }
        .movement-btn { padding: 20px; font-size: 18px; font-weight: bold; }
        .step-controls { margin: 20px 0; }
        .step-controls h3 { margin: 0 0 15px 0; color: #333; font-size: 16px; }
        .step-buttons { display: flex; gap: 15px; flex-wrap: wrap; }
        .step-group { display: flex; gap: 8px; }
        .step-btn { padding: 12px 16px; font-size: 14px; font-weight: bold; border: 2px solid #007bff; background: white; color: #007bff; border-radius: 6px; cursor: pointer; transition: all 0.3s; }
        .step-btn:hover { background: #007bff; color: white; }
        .goto-controls { display: flex; gap: 10px; align-items: center; }
        .goto-input { flex: 1; }
        .status-indicators { display: flex; gap: 15px; margin: 15px 0; }
        .status-indicator { flex: 1; padding: 10px; border-radius: 8px; text-align: center; font-weight: bold; }
        .indicator-connected { background: #d4edda; color: #155724; }
        .indicator-moving { background: #fff3cd; color: #856404; }
        .indicator-disconnected { background: #f8d7da; color: #721c24; }
        @media (max-width: 768px) {
            .main-content { grid-template-columns: 1fr; }
            .focuser-controls { grid-template-columns: 1fr; }
            .movement-controls { grid-template-columns: 1fr; }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Celestron Focuser Controller</h1>
            <div id="status" class="status disconnected">
                <strong>Status:</strong> <span id="statusText">Disconnected</span>
            </div>
            <div id="info" class="info">
                <strong>Device Info:</strong><br>
                <span id="deviceInfo">Loading...</span>
            </div>
        </div>

        <div class="main-content">
            <!-- Focuser Control Panel -->
            <div class="panel focuser-panel">
                <h2>Focuser Control</h2>
                
                <div class="status-indicators">
                    <div id="focuserStatus" class="status-indicator indicator-disconnected">
                        Focuser: <span id="focuserStatusText">Disconnected</span>
                    </div>
                    <div id="movementStatus" class="status-indicator indicator-disconnected">
                        Movement: <span id="movementStatusText">Stopped</span>
                    </div>
                </div>

                <div class="position-display">
                    <div>Current Position</div>
                    <div id="currentPosition" class="position-value">---</div>
                    <div>Target Position: <span id="targetPosition">---</span></div>
                </div>

                <div class="focuser-controls">
                    <div class="form-group">
                        <label for="focuserSpeed">Speed (1-9)</label>
                        <div class="speed-control">
                            <input type="range" id="focuserSpeed" class="speed-slider" min="1" max="9" value="5" onchange="updateSpeed()">
                            <span id="speedValue">5</span>
                        </div>
                    </div>

                    <div class="form-group">
                        <label for="gotoPosition">Go to Position</label>
                        <div class="goto-controls">
                            <input type="number" id="gotoPosition" class="goto-input" placeholder="Enter position" min="0">
                            <button onclick="gotoPosition()" class="success">Go</button>
                        </div>
                    </div>
                </div>

                <div class="movement-controls">
                    <button id="moveInBtn" onclick="moveFocuser('in')" class="movement-btn success" disabled>
                        Move IN
                    </button>
                    <button id="moveOutBtn" onclick="moveFocuser('out')" class="movement-btn success" disabled>
                        Move OUT
                    </button>
                </div>

                <div class="step-controls">
                    <h3>Quick Steps</h3>
                    <div class="step-buttons">
                        <div class="step-group">
                            <button onclick="stepFocuser('in', 5)" class="step-btn">+5</button>
                            <button onclick="stepFocuser('out', 5)" class="step-btn">-5</button>
                        </div>
                        <div class="step-group">
                            <button onclick="stepFocuser('in', 20)" class="step-btn">+20</button>
                            <button onclick="stepFocuser('out', 20)" class="step-btn">-20</button>
                        </div>
                        <div class="step-group">
                            <button onclick="stepFocuser('in', 50)" class="step-btn">+50</button>
                            <button onclick="stepFocuser('out', 50)" class="step-btn">-50</button>
                        </div>
                    </div>
                </div>

                <div class="buttons">
                    <button id="stopBtn" onclick="stopFocuser()" class="danger movement-btn" disabled>
                        STOP
                    </button>
                    <button onclick="getPosition()" class="warning">
                        Get Position
                    </button>
                    <button onclick="connectFocuser()" class="success">
                        Connect Focuser
                    </button>
                </div>
            </div>

            <!-- WiFi Configuration Panel -->
            <div class="panel">
                <h2>WiFi Configuration</h2>
                <form id="wifiForm">
                    <div class="form-group">
                        <label for="ssid">WiFi Network Name (SSID)</label>
                        <input type="text" id="ssid" name="ssid" required>
                    </div>
                    
                    <div class="form-group">
                        <label for="password">WiFi Password</label>
                        <input type="password" id="password" name="password">
                    </div>
                    
                    <div class="form-group">
                        <label for="hostname">Device Hostname</label>
                        <input type="text" id="hostname" name="hostname" value="celestron-focuser">
                    </div>
                    
                    <div class="buttons">
                        <button type="submit">Save & Connect</button>
                        <button type="button" onclick="clearConfig()" class="danger">Clear Config</button>
                        <button type="button" onclick="refreshStatus()" class="warning">Refresh Status</button>
                    </div>
                </form>
            </div>

            <!-- System Info Panel -->
            <div class="panel">
                <h2>System Information</h2>
                <div class="info">
                    <strong>Connection Info:</strong><br>
                    <span id="connectionInfo">Loading...</span>
                </div>
                <div class="buttons">
                    <button onclick="showHelp()" class="warning">Help</button>
                    <button onclick="showAbout()" class="success">About</button>
                </div>
            </div>
        </div>
    </div>

    <script>
        let ws;
        let focuserConnected = false;
        let currentPosition = 0;
        let targetPosition = 0;
        let currentSpeed = 5;
        let isMoving = false;
        
        function connectWebSocket() {
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = protocol + '//' + window.location.hostname + ':81';
            
            ws = new WebSocket(wsUrl);
            
            ws.onopen = function() {
                console.log('WebSocket connected');
                refreshStatus();
                getPosition(); // Get initial position
            };
            
            ws.onmessage = function(event) {
                const data = JSON.parse(event.data);
                console.log('Received:', data);
                
                if (data.type === 'focuserStatus') {
                    updateFocuserStatus(data);
                } else if (data.status === 'wifi') {
                    updateStatus(data);
                } else if (data.command && data.command.startsWith('focuser:')) {
                    handleFocuserResponse(data);
                }
            };
            
            ws.onclose = function() {
                console.log('WebSocket disconnected, retrying...');
                setTimeout(connectWebSocket, 3000);
            };
        }
        
        function updateStatus(data) {
            const statusDiv = document.getElementById('status');
            const statusText = document.getElementById('statusText');
            const deviceInfo = document.getElementById('deviceInfo');
            const connectionInfo = document.getElementById('connectionInfo');
            
            if (data.connected) {
                statusDiv.className = 'status connected';
                statusText.textContent = 'Connected to ' + data.ssid + ' (' + data.ip + ')';
                deviceInfo.innerHTML = 'Hostname: ' + data.hostname + '<br>Signal: ' + data.rssi + ' dBm<br>mDNS: ' + data.hostname + '.local';
                connectionInfo.innerHTML = 'WiFi: Connected<br>IP: ' + data.ip + '<br>mDNS: ' + data.hostname + '.local';
            } else {
                statusDiv.className = 'status disconnected';
                statusText.textContent = 'Not connected (AP Mode)';
                deviceInfo.innerHTML = 'AP SSID: ' + (data.ssid || 'Celestron-Focuser') + '<br>IP: ' + (data.ip || '192.168.4.1');
                connectionInfo.innerHTML = 'WiFi: AP Mode<br>IP: ' + (data.ip || '192.168.4.1') + '<br>Connect to: ' + (data.ssid || 'Celestron-Focuser');
            }
        }
        
        function updateFocuserStatus(data) {
            focuserConnected = data.connected;
            currentPosition = data.position;
            targetPosition = data.target;
            currentSpeed = data.speed;
            isMoving = data.moving;
            
            // Update UI
            document.getElementById('currentPosition').textContent = currentPosition.toLocaleString();
            document.getElementById('targetPosition').textContent = targetPosition.toLocaleString();
            document.getElementById('focuserSpeed').value = currentSpeed;
            document.getElementById('speedValue').textContent = currentSpeed;
            
            // Update status indicators
            const focuserStatus = document.getElementById('focuserStatus');
            const focuserStatusText = document.getElementById('focuserStatusText');
            const movementStatus = document.getElementById('movementStatus');
            const movementStatusText = document.getElementById('movementStatusText');
            
            if (focuserConnected) {
                focuserStatus.className = 'status-indicator indicator-connected';
                focuserStatusText.textContent = 'Connected';
                
                // Enable/disable controls
                document.getElementById('moveInBtn').disabled = isMoving;
                document.getElementById('moveOutBtn').disabled = isMoving;
                document.getElementById('stopBtn').disabled = !isMoving;
                
                if (isMoving) {
                    movementStatus.className = 'status-indicator indicator-moving';
                    movementStatusText.textContent = 'Moving';
                } else {
                    movementStatus.className = 'status-indicator indicator-connected';
                    movementStatusText.textContent = 'Stopped';
                }
            } else {
                focuserStatus.className = 'status-indicator indicator-disconnected';
                focuserStatusText.textContent = 'Disconnected';
                movementStatus.className = 'status-indicator indicator-disconnected';
                movementStatusText.textContent = 'Unknown';
                
                // Disable all controls
                document.getElementById('moveInBtn').disabled = true;
                document.getElementById('moveOutBtn').disabled = true;
                document.getElementById('stopBtn').disabled = true;
            }
        }
        
        function handleFocuserResponse(data) {
            if (data.status === 'success') {
                console.log('Focuser command successful:', data.command);
            } else {
                console.error('Focuser command failed:', data.command);
                alert('Focuser command failed: ' + data.command);
            }
        }
        
        function updateSpeed() {
            currentSpeed = parseInt(document.getElementById('focuserSpeed').value);
            document.getElementById('speedValue').textContent = currentSpeed;
            
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({
                    command: 'focuser:setSpeed',
                    speed: currentSpeed
                }));
            }
        }
        
        function moveFocuser(direction) {
            if (!focuserConnected) {
                alert('Focuser not connected!');
                return;
            }
            
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({
                    command: 'focuser:move',
                    direction: direction,
                    speed: currentSpeed
                }));
            }
        }
        
        function stepFocuser(direction, steps) {
            if (!focuserConnected) {
                alert('Focuser not connected!');
                return;
            }
            
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({
                    command: 'focuser:step',
                    direction: direction,
                    steps: steps,
                    speed: currentSpeed
                }));
            }
        }
        
        function stopFocuser() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({
                    command: 'focuser:stop'
                }));
            }
        }
        
        function gotoPosition() {
            const position = parseInt(document.getElementById('gotoPosition').value);
            if (isNaN(position) || position < 0) {
                alert('Please enter a valid position number');
                return;
            }
            
            if (!focuserConnected) {
                alert('Focuser not connected!');
                return;
            }
            
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({
                    command: 'focuser:goto',
                    position: position
                }));
            }
        }
        
        function getPosition() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({
                    command: 'focuser:getPosition'
                }));
            }
        }
        
        function connectFocuser() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                // Show loading state
                const btn = event.target;
                const originalText = btn.textContent;
                btn.textContent = 'Connecting...';
                btn.disabled = true;
                
                ws.send(JSON.stringify({
                    command: 'focuser:connect'
                }));
                
                // Reset button after a delay
                setTimeout(() => {
                    btn.textContent = originalText;
                    btn.disabled = false;
                }, 3000);
            }
        }
        
        function refreshStatus() {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify({command: 'getStatus'}));
            }
        }
        
        function clearConfig() {
            if (confirm('Are you sure you want to clear the WiFi configuration?')) {
                if (ws && ws.readyState === WebSocket.OPEN) {
                    ws.send(JSON.stringify({command: 'clearWiFi'}));
                }
            }
        }
        
        function showHelp() {
            alert('Focuser Control Help:\n\n' +
                  '• Use the speed slider to set movement speed (1-9)\n' +
                  '• Click Move IN/OUT for continuous movement\n' +
                  '• Use Go to Position for precise positioning\n' +
                  '• Click STOP to immediately stop movement\n' +
                  '• Get Position updates the current position display\n\n' +
                  'Make sure the focuser is connected before controlling!');
        }
        
        function showAbout() {
            alert('Celestron Focuser Controller v1.0\n\n' +
                  'ESP32-based WiFi focuser controller\n' +
                  'with mDNS support and web interface.\n\n' +
                  'Features:\n' +
                  '• Real-time position tracking\n' +
                  '• Speed control (1-9)\n' +
                  '• Absolute positioning\n' +
                  '• WiFi configuration\n' +
                  '• mDNS device discovery');
        }
        
        document.getElementById('wifiForm').addEventListener('submit', function(e) {
            e.preventDefault();
            
            const formData = {
                command: 'setWiFi',
                ssid: document.getElementById('ssid').value,
                password: document.getElementById('password').value,
                hostname: document.getElementById('hostname').value
            };
            
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(JSON.stringify(formData));
                alert('WiFi configuration saved! The device will restart and attempt to connect.');
            }
        });
        
        // Connect WebSocket when page loads
        window.addEventListener('load', function() {
            connectWebSocket();
            refreshStatus();
            
            // Auto-refresh focuser status every 2 seconds
            setInterval(function() {
                if (focuserConnected) {
                    getPosition();
                }
            }, 2000);
        });
    </script>
</body>
</html>)rawliteral";
        
        request->send(200, "text/html", html);
    }
}

void WiFiManager::_handleWiFiConfig(AsyncWebServerRequest *request) {
    // This endpoint is handled by WebSocket now, but keeping for compatibility
    request->send(200, "application/json", "{\"status\":\"use_websocket\"}");
}

void WiFiManager::_handleStatus(AsyncWebServerRequest *request) {
    String statusJson = _getWiFiStatusJSON();
    request->send(200, "application/json", statusJson);
}

void WiFiManager::_handleNotFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not Found");
}

String WiFiManager::_getWiFiStatusJSON() {
    JsonDocument doc;
    
    // Get values first
    String ssid = getSSID();
    String ip = getIPAddress();
    
    doc["status"] = "wifi";  // Add status field for JavaScript detection
    // For web interface, "connected" means connected to external WiFi (not AP mode)
    doc["connected"] = _wifiConnected && !_apMode;
    doc["apMode"] = _apMode;
    doc["ssid"] = ssid;
    doc["ip"] = ip;
    doc["hostname"] = _hostname;
    
    if (_wifiConnected && !_apMode) {
        doc["rssi"] = WiFi.RSSI();
    }
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    // Debug output (can be removed later)
    // Serial.println("DEBUG: WiFi Status JSON: " + jsonString);
    
    return jsonString;
}

void WiFiManager::_onWiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("INFO: WiFi station connected");
            break;
            
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.println("INFO: WiFi station got IP");
            _wifiConnected = true;
            
            // Start mDNS service
            startmDNS();
            
            if (_onConnected) {
                _onConnected();
            }
            break;
            
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.println("INFO: WiFi station disconnected");
            _wifiConnected = false;
            
            // Stop mDNS service
            stopmDNS();
            
            if (_onDisconnected) {
                _onDisconnected();
            }
            break;
            
        default:
            break;
    }
}

