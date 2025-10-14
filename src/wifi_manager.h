/*
    WiFi Manager for ESP32 Celestron Focuser Controller
    Handles WiFi configuration, AP/Station modes, and web interface
    
    Copyright (C) 2024
*/

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>

// WiFi Configuration
#define WIFI_AP_SSID "Celestron-Focuser"
#define WIFI_AP_PASSWORD "focuser123"
#define WIFI_AP_CHANNEL 1
#define WIFI_AP_MAX_CONNECTIONS 4

// Web Server Configuration
#define WEB_SERVER_PORT 80
#define WEBSOCKET_PORT 81

// Preferences keys
#define PREF_NAMESPACE "wifi_config"
#define PREF_SSID_KEY "wifi_ssid"
#define PREF_PASSWORD_KEY "wifi_password"
#define PREF_HOSTNAME_KEY "hostname"

// Default values
#define DEFAULT_HOSTNAME "celestron-focuser"

// Connection timeout (seconds)
#define WIFI_CONNECT_TIMEOUT 30
#define WIFI_RECONNECT_DELAY 5000

/**
 * WiFi Manager Class
 * Handles WiFi configuration, AP/Station modes, and web interface
 */
class WiFiManager {
public:
    // Constructor/Destructor
    WiFiManager();
    ~WiFiManager();
    
    // Initialization
    bool begin();
    void handle();
    
    // WiFi Control
    bool startAP();
    bool startStation();
    bool connectToWiFi();
    void disconnect();
    
    // Configuration Management
    void saveWiFiConfig(const String& ssid, const String& password);
    void saveHostname(const String& hostname);
    bool loadWiFiConfig();
    void clearWiFiConfig();
    
    // Status
    bool isConnected();
    bool isAPMode();
    String getIPAddress();
    String getSSID();
    String getHostname();
    
    // Web Interface
    void setupWebServer();
    void handleWebSocketMessage(uint8_t num, uint8_t *payload, size_t length);
    
    // Focuser Control via WebSocket
    void setFocuserCallback(std::function<bool(String, JsonDocument&)> callback);
    void sendFocuserStatus(uint8_t num, bool connected, uint32_t position, uint32_t target, uint8_t speed, bool moving);
    
    // mDNS Support
    bool startmDNS();
    void stopmDNS();
    String getmDNSHostname();
    
    // Callbacks
    void onWiFiConnected(std::function<void()> callback);
    void onWiFiDisconnected(std::function<void()> callback);
    
private:
    // WiFi State
    bool _apMode;
    bool _stationMode;
    bool _wifiConnected;
    
    // Configuration
    String _ssid;
    String _password;
    String _hostname;
    
    // Web Server
    AsyncWebServer* _webServer;
    WebSocketsServer* _webSocketServer;
    
    // Preferences
    Preferences _preferences;
    
    // Callbacks
    std::function<void()> _onConnected;
    std::function<void()> _onDisconnected;
    std::function<bool(String, JsonDocument&)> _focuserCallback;
    
    // Internal Methods
    void _setupWebRoutes();
    void _handleRoot(AsyncWebServerRequest *request);
    void _handleWiFiConfig(AsyncWebServerRequest *request);
    void _handleStatus(AsyncWebServerRequest *request);
    void _handleNotFound(AsyncWebServerRequest *request);
    String _getWiFiStatusJSON();
    void _onWiFiEvent(WiFiEvent_t event);
    
};

// Global WiFi Manager instance
extern WiFiManager wifiManager;
