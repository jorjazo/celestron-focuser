/*
    ESP32 Celestron Focuser Controller
    USB-to-AUX Serial Bridge with Command Parser
    
    Copyright (C) 2024
    
    Hardware: ESP32 DevKit v1
    - USB Serial (Serial) at 115200 baud for computer communication
    - Hardware Serial2 (GPIO16/17) at 19200 baud for AUX port communication
*/

#include <Arduino.h>
#include "celestron_aux.h"
#include "wifi_manager.h"

using namespace CelestronAux;

// ============================================================================
// Configuration
// ============================================================================

// Serial Configuration
#define USB_BAUD_RATE    115200
#define AUX_BAUD_RATE    19200

// GPIO Pins for ESP32 DevKit v1
#define AUX_RX_PIN       16  // GPIO16 (RX2)
#define AUX_TX_PIN       17  // GPIO17 (TX2)

// Command Configuration
#define MAX_COMMAND_LEN  32
#define POSITION_TIMEOUT 5000  // 5 seconds for position queries

// ============================================================================
// Global Variables
// ============================================================================

// Serial Communication
HardwareSerial auxSerial(2);  // Serial2
CelestronAux::Communicator communicator;

// Focuser State
uint32_t currentPosition = 0;
uint32_t targetPosition = 0;
uint8_t currentSpeed = 5;  // Default speed (1-9)
bool isMoving = false;
bool focuserConnected = false;

// Status checking timing
unsigned long lastStatusCheck = 0;
const unsigned long STATUS_CHECK_INTERVAL = 500;   // Check every 0.5 seconds

// Command Buffer
String commandBuffer = "";
bool commandReady = false;

// WiFi Status
bool wifiInitialized = false;

// ============================================================================
// Function Declarations
// ============================================================================

void setupSerial();
void setupPins();
bool initializeFocuser();
void initializeWiFi();
void processCommands();
void handleCommand(char command);
void handleGotoCommand(String value);
void displayHelp();
void displayStatus();
bool handleWebFocuserCommand(String command, JsonDocument& doc);

// Focuser Control Functions
bool getFocuserPosition();
bool moveFocuser(uint8_t direction, uint8_t speed);
bool stepFocuser(uint8_t direction, uint32_t steps, uint8_t speed);
bool gotoPosition(uint32_t position);
bool stopFocuser();
bool setSpeed(uint8_t speed);
bool checkFocuserStatus();

// Utility Functions
void printError(String message);
void printSuccess(String message);
void printInfo(String message);
uint32_t parsePosition(String value);
void runDiagnostics();
void testBaudRates();

// ============================================================================
// Setup Function
// ============================================================================

void setup() {
    // Initialize serial communication
    setupSerial();
    
    // Initialize GPIO pins
    setupPins();
    
    // Initialize AUX serial communication
    auxSerial.begin(AUX_BAUD_RATE, SERIAL_8N1, AUX_RX_PIN, AUX_TX_PIN);
    
    // Wait for serial to be ready
    delay(1000);
    
    // Display startup message
    printInfo("ESP32 Celestron Focuser Controller");
    printInfo("==================================");
    printInfo("Hardware: ESP32 DevKit v1");
    printInfo("USB Serial: " + String(USB_BAUD_RATE) + " baud");
    printInfo("AUX Serial: " + String(AUX_BAUD_RATE) + " baud");
    printInfo("AUX Pins: RX=" + String(AUX_RX_PIN) + ", TX=" + String(AUX_TX_PIN));
    printInfo("");
    
    // Initialize WiFi
    initializeWiFi();
    
    // Initialize focuser (with safety check)
    printInfo("Attempting to initialize focuser...");
    
    // Try to initialize focuser with timeout
    if (initializeFocuser()) {
        printSuccess("Focuser initialized successfully");
        focuserConnected = true;
        displayStatus();
    } else {
        printError("Failed to initialize focuser");
        printError("Check wiring and power connections");
        printInfo("Focuser will remain disconnected");
        printInfo("You can still test basic functionality");
        focuserConnected = false;
    }
    
    printInfo("");
    displayHelp();
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
    // Handle WiFi operations
    if (wifiInitialized) {
        wifiManager.handle();
        
        // Send periodic status updates to web clients
        static unsigned long lastWebStatusUpdate = 0;
        if (millis() - lastWebStatusUpdate > 1000) { // Every second
            if (focuserConnected) {
                // Send status to all connected web clients
                for (int i = 0; i < 8; i++) {
                    wifiManager.sendFocuserStatus(i, focuserConnected, currentPosition, targetPosition, currentSpeed, isMoving);
                }
            }
            lastWebStatusUpdate = millis();
        }
    }
    
    // Process incoming commands
    processCommands();
    
    // Update focuser status if connected and moving (with rate limiting)
    if (focuserConnected && isMoving) {
        unsigned long currentTime = millis();
        if (currentTime - lastStatusCheck >= STATUS_CHECK_INTERVAL) {
            checkFocuserStatus();
            lastStatusCheck = currentTime;
        }
    }
    
    // Small delay to prevent overwhelming the system
    delay(10);
}

// ============================================================================
// Setup Functions
// ============================================================================

void setupSerial() {
    Serial.begin(USB_BAUD_RATE);
    // Don't wait for Serial on ESP32 - it may not be available immediately
    delay(1000);  // Give time for serial to initialize
}

void setupPins() {
    // Configure AUX serial pins
    pinMode(AUX_RX_PIN, INPUT);
    pinMode(AUX_TX_PIN, OUTPUT);
}

void initializeWiFi() {
    printInfo("Initializing WiFi...");
    
    // Set up WiFi callbacks
    wifiManager.onWiFiConnected([]() {
        printSuccess("WiFi connected successfully!");
        printInfo("WiFi SSID: " + wifiManager.getSSID());
        printInfo("WiFi IP: " + wifiManager.getIPAddress());
        printInfo("Web interface: http://" + wifiManager.getIPAddress());
        printInfo("mDNS hostname: " + wifiManager.getmDNSHostname());
        printInfo("Web interface (mDNS): http://" + wifiManager.getmDNSHostname());
    });
    
    wifiManager.onWiFiDisconnected([]() {
        printInfo("WiFi disconnected, switching to AP mode");
    });
    
    // Set up focuser control callback
    wifiManager.setFocuserCallback([](String command, JsonDocument& doc) -> bool {
        return handleWebFocuserCommand(command, doc);
    });
    
    // Initialize WiFi manager
    if (wifiManager.begin()) {
        wifiInitialized = true;
        printSuccess("WiFi Manager initialized");
        
        if (wifiManager.isConnected()) {
            printInfo("Connected to WiFi network");
            printInfo("Web interface: http://" + wifiManager.getIPAddress());
        } else {
            printInfo("WiFi AP mode active");
            printInfo("Connect to: " + String(WIFI_AP_SSID));
            printInfo("Password: " + String(WIFI_AP_PASSWORD));
            printInfo("Web interface: http://" + wifiManager.getIPAddress());
        }
    } else {
        printError("Failed to initialize WiFi Manager");
        wifiInitialized = false;
    }
}

// ============================================================================
// Command Processing
// ============================================================================

void processCommands() {
    while (Serial.available()) {
        char c = Serial.read();
        
        if (c == '\n' || c == '\r') {
            if (commandBuffer.length() > 0) {
                commandReady = true;
            }
        } else if (c >= 32 && c <= 126) {  // Printable characters
            commandBuffer += c;
            
            // Limit command length
            if (commandBuffer.length() >= MAX_COMMAND_LEN) {
                commandBuffer = commandBuffer.substring(0, MAX_COMMAND_LEN - 1);
            }
        }
        
        // Process command if ready
        if (commandReady) {
            if (commandBuffer.length() == 1) {
                // Single character command
                handleCommand(commandBuffer[0]);
            } else if (commandBuffer.startsWith("g")) {
                // Goto command with position
                handleGotoCommand(commandBuffer.substring(1));
            } else {
                printError("Unknown command: " + commandBuffer);
            }
            
            commandBuffer = "";
            commandReady = false;
        }
    }
}

void handleCommand(char command) {
    // Handle commands that don't require focuser connection first
    switch (command) {
        case 'c':
            printInfo("Attempting to connect to focuser...");
            if (initializeFocuser()) {
                focuserConnected = true;
                printSuccess("Focuser connected successfully");
                displayStatus();
            } else {
                focuserConnected = false;
                printError("Failed to connect to focuser");
            }
            return;
            
        case '?':
            displayHelp();
            return;
            
        case 'i':
            displayStatus();
            return;
            
        case 'd':
            runDiagnostics();
            return;
            
        case 't':
            testBaudRates();
            return;
            
        case 'w':
            if (wifiInitialized) {
                printInfo("WiFi Status:");
                printInfo("  Connected: " + String(wifiManager.isConnected() ? "Yes" : "No"));
                printInfo("  Mode: " + String(wifiManager.isAPMode() ? "AP" : "Station"));
                printInfo("  SSID: " + wifiManager.getSSID());
                printInfo("  IP: " + wifiManager.getIPAddress());
                printInfo("  Hostname: " + wifiManager.getHostname());
                printInfo("  Web interface: http://" + wifiManager.getIPAddress());
                if (wifiManager.isConnected() && !wifiManager.isAPMode()) {
                    printInfo("  mDNS hostname: " + wifiManager.getmDNSHostname());
                    printInfo("  Web interface (mDNS): http://" + wifiManager.getmDNSHostname());
                }
            } else {
                printError("WiFi not initialized");
            }
            return;
    }
    
    // For all other commands, check if focuser is connected
    if (!focuserConnected) {
        printError("Focuser not connected");
        printInfo("Use 'c' command to try connecting");
        return;
    }
    
    // Handle focuser-specific commands
    switch (command) {
        case '+':
            printInfo("Moving focuser INWARD at speed " + String(currentSpeed));
            if (moveFocuser(1, currentSpeed)) {
                isMoving = true;
            }
            break;
            
        case '-':
            printInfo("Moving focuser OUTWARD at speed " + String(currentSpeed));
            if (moveFocuser(0, currentSpeed)) {
                isMoving = true;
            }
            break;
            
        case 's':
        case '0':
            printInfo("Stopping focuser");
            if (stopFocuser()) {
                isMoving = false;
            }
            break;
            
        case 'p':
            printInfo("Getting current position...");
            if (getFocuserPosition()) {
                printInfo("Current position: " + String(currentPosition));
            }
            break;
            
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            {
                uint8_t speed = command - '0';
                if (setSpeed(speed)) {
                    currentSpeed = speed;
                    printSuccess("Speed set to " + String(speed));
                }
            }
            break;
            
        default:
            printError("Unknown command: " + String(command));
            printInfo("Type '?' for help");
            break;
    }
}

void handleGotoCommand(String value) {
    if (!focuserConnected) {
        printError("Focuser not connected");
        return;
    }
    
    uint32_t position = parsePosition(value);
    if (position == 0 && value != "0") {
        printError("Invalid position: " + value);
        return;
    }
    
    printInfo("Moving to position " + String(position));
    if (gotoPosition(position)) {
        targetPosition = position;
        isMoving = true;
    }
}

// ============================================================================
// Focuser Control Functions
// ============================================================================

bool initializeFocuser() {
    printInfo("Initializing focuser...");
    
    // Check if AUX serial is available
    if (!auxSerial) {
        printError("AUX serial not available");
        return false;
    }
    
    // Try to get firmware version with timeout
    printInfo("Sending version request...");
    Buffer reply;
    
    // Set a reasonable timeout for the command
    unsigned long startTime = millis();
    bool success = false;
    
    if (communicator.sendCommand(auxSerial, Target::FOCUSER, Command::GET_VER, reply)) {
        if (reply.size() >= 2) {
            printSuccess("Firmware Version: " + String(reply[0]) + "." + String(reply[1]));
            if (reply.size() >= 4) {
                uint16_t build = (reply[2] << 8) + reply[3];
                printInfo("Build: " + String(build));
            }
            success = true;
        } else {
            printError("Invalid version response (too short)");
        }
    } else {
        printError("No response from focuser");
        printError("Check AUX port wiring and power");
    }
    
    unsigned long elapsed = millis() - startTime;
    printInfo("Initialization took " + String(elapsed) + "ms");
    
    return success;
}

bool getFocuserPosition() {
    Buffer reply;
    if (communicator.sendCommand(auxSerial, Target::FOCUSER, Command::MC_GET_POSITION, reply)) {
        if (reply.size() >= 3) {
            currentPosition = (reply[0] << 16) + (reply[1] << 8) + reply[2];
            return true;
        }
    }
    return false;
}

bool moveFocuser(uint8_t direction, uint8_t speed) {
    Command cmd = (direction == 1) ? Command::MC_MOVE_POS : Command::MC_MOVE_NEG;
    Buffer data = {speed};
    Buffer reply;
    
    // MC_MOVE_POS and MC_MOVE_NEG expect a response from the focuser
    return communicator.sendCommand(auxSerial, Target::FOCUSER, cmd, data, reply);
}

bool stepFocuser(uint8_t direction, uint32_t steps, uint8_t speed) {
    // Get current position
    uint32_t startPosition = currentPosition;
    uint32_t targetPosition;
    
    if (direction == 1) {
        targetPosition = startPosition + steps;
    } else {
        // Handle underflow
        if (steps > startPosition) {
            targetPosition = 0;
        } else {
            targetPosition = startPosition - steps;
        }
    }
    
    // Use goto command for precise stepping
    return gotoPosition(targetPosition);
}

bool gotoPosition(uint32_t position) {
    Buffer data = {
        static_cast<uint8_t>((position >> 16) & 0xFF),
        static_cast<uint8_t>((position >> 8) & 0xFF),
        static_cast<uint8_t>(position & 0xFF)
    };
    
    return communicator.commandBlind(auxSerial, Target::FOCUSER, Command::MC_GOTO_FAST, data);
}

bool stopFocuser() {
    Buffer data = {0};
    return communicator.commandBlind(auxSerial, Target::FOCUSER, Command::MC_MOVE_POS, data);
}

bool setSpeed(uint8_t speed) {
    if (speed < 1 || speed > 9) {
        return false;
    }
    return true;  // Speed is stored in software, actual command sent during movement
}

bool checkFocuserStatus() {
    if (!isMoving) {
        return true;
    }
    
    Buffer reply;
    if (communicator.sendCommand(auxSerial, Target::FOCUSER, Command::MC_SLEW_DONE, reply)) {
        if (!reply.empty()) {
            uint8_t status = reply[0];
            bool stillMoving = (status != 0xFF);
            
            // Debug output
            Serial.printf("DEBUG: MC_SLEW_DONE status = 0x%02X, stillMoving = %s\n", 
                         status, stillMoving ? "true" : "false");
            
            if (!stillMoving) {
                isMoving = false;
                getFocuserPosition();  // Update current position
                printSuccess("Focuser reached target position: " + String(currentPosition));
            }
        }
    }
    
    return true;
}

// ============================================================================
// Utility Functions
// ============================================================================

void displayHelp() {
    printInfo("Available Commands:");
    printInfo("  +     - Move focuser INWARD (continuous)");
    printInfo("  -     - Move focuser OUTWARD (continuous)");
    printInfo("  s, 0  - Stop movement");
    printInfo("  p     - Get current position");
    printInfo("  g#### - Go to absolute position (e.g., g5000)");
    printInfo("  1-9   - Set motor speed (1=slowest, 9=fastest)");
    printInfo("  c     - Connect to focuser (retry connection)");
    printInfo("  d     - Run diagnostics (troubleshoot connection)");
    printInfo("  t     - Test different baud rates");
    printInfo("  w     - Show WiFi status and web interface URL");
    printInfo("  ?     - Show this help");
    printInfo("  i     - Show status information");
    printInfo("");
    
    if (wifiInitialized) {
        printInfo("WiFi Web Interface:");
        printInfo("  URL: http://" + wifiManager.getIPAddress());
        printInfo("  Use web interface to configure WiFi settings");
        printInfo("");
    }
}

void displayStatus() {
    printInfo("Focuser Status:");
    printInfo("  Connected: " + String(focuserConnected ? "Yes" : "No"));
    printInfo("  Current Position: " + String(currentPosition));
    printInfo("  Target Position: " + String(targetPosition));
    printInfo("  Current Speed: " + String(currentSpeed));
    printInfo("  Moving: " + String(isMoving ? "Yes" : "No"));
    printInfo("");
    
    if (wifiInitialized) {
        printInfo("WiFi Status:");
        printInfo("  Connected: " + String(wifiManager.isConnected() ? "Yes" : "No"));
        printInfo("  Mode: " + String(wifiManager.isAPMode() ? "AP" : "Station"));
        printInfo("  SSID: " + wifiManager.getSSID());
        printInfo("  IP: " + wifiManager.getIPAddress());
        printInfo("  Web interface: http://" + wifiManager.getIPAddress());
        if (wifiManager.isConnected() && !wifiManager.isAPMode()) {
            printInfo("  mDNS hostname: " + wifiManager.getmDNSHostname());
            printInfo("  Web interface (mDNS): http://" + wifiManager.getmDNSHostname());
        }
        printInfo("");
    }
}

uint32_t parsePosition(String value) {
    value.trim();
    if (value.length() == 0) {
        return 0;
    }
    
    // Check if it's a valid number
    for (size_t i = 0; i < value.length(); i++) {
        if (!isdigit(value[i])) {
            return 0;
        }
    }
    
    return value.toInt();
}

void printError(String message) {
    Serial.println("ERROR: " + message);
}

void printSuccess(String message) {
    Serial.println("SUCCESS: " + message);
}

void printInfo(String message) {
    Serial.println("INFO: " + message);
}

// ============================================================================
// Diagnostics Function
// ============================================================================

void runDiagnostics() {
    printInfo("=== ESP32 Focuser Diagnostics ===");
    printInfo("");
    
    // Check AUX serial status
    printInfo("AUX Serial Status:");
    printInfo("  Port: Serial2 (GPIO16/17)");
    printInfo("  Baud Rate: " + String(AUX_BAUD_RATE));
    printInfo("  RX Pin: " + String(AUX_RX_PIN));
    printInfo("  TX Pin: " + String(AUX_TX_PIN));
    printInfo("");
    
    // Test AUX serial communication
    printInfo("Testing AUX Serial Communication:");
    printInfo("  Sending test packet...");
    
    // Send a simple test packet
    Buffer testPacket = {0x3B, 0x03, 0x20, 0x12, 0xFE, 0xCD};
    size_t bytesWritten = auxSerial.write(testPacket.data(), testPacket.size());
    auxSerial.flush();
    
    printInfo("  Bytes written: " + String(bytesWritten) + "/" + String(testPacket.size()));
    
    // Check for any incoming data
    printInfo("  Checking for incoming data...");
    delay(100);  // Wait a bit for response
    
    int availableBytes = auxSerial.available();
    printInfo("  Available bytes: " + String(availableBytes));
    
    if (availableBytes > 0) {
        printInfo("  Incoming data:");
        for (int i = 0; i < availableBytes && i < 20; i++) {
            uint8_t byte = auxSerial.read();
            Serial.printf("    0x%02X ", byte);
        }
        Serial.println();
    } else {
        printInfo("  No incoming data detected");
    }
    
    printInfo("");
    printInfo("Hardware Check:");
    printInfo("  1. Verify AUX port wiring (RX/TX/GND)");
    printInfo("  2. Check level shifter connections (3.3V <-> 5V)");
    printInfo("  3. Ensure focuser is powered on");
    printInfo("  4. Verify AUX port baud rate is 19200");
    printInfo("  5. Check AUX port is not used by other devices");
    printInfo("");
    printInfo("Troubleshooting Tips:");
    printInfo("  - Try different baud rates: 9600, 19200, 38400");
    printInfo("  - Check AUX port with multimeter for voltage levels");
    printInfo("  - Verify focuser responds to other controllers");
    printInfo("  - Test with simple serial terminal first");
}

void testBaudRates() {
    printInfo("=== Baud Rate Testing ===");
    printInfo("");
    
    int baudRates[] = {9600, 19200, 38400, 57600, 115200};
    int numRates = sizeof(baudRates) / sizeof(baudRates[0]);
    
    for (int i = 0; i < numRates; i++) {
        int baud = baudRates[i];
        printInfo("Testing baud rate: " + String(baud));
        
        // Reinitialize AUX serial with new baud rate
        auxSerial.end();
        delay(100);
        auxSerial.begin(baud, SERIAL_8N1, AUX_RX_PIN, AUX_TX_PIN);
        delay(100);
        
        // Send test packet
        Buffer testPacket = {0x3B, 0x03, 0x20, 0x12, 0xFE, 0xCD};
        auxSerial.write(testPacket.data(), testPacket.size());
        auxSerial.flush();
        
        // Wait for response
        delay(200);
        
        int availableBytes = auxSerial.available();
        if (availableBytes > 0) {
            printInfo("  ✓ Response received! (" + String(availableBytes) + " bytes)");
            printInfo("  Data: ");
            for (int j = 0; j < availableBytes && j < 10; j++) {
                uint8_t byte = auxSerial.read();
                Serial.printf("0x%02X ", byte);
            }
            Serial.println();
            printInfo("  This baud rate might work!");
        } else {
            printInfo("  ✗ No response");
        }
        printInfo("");
    }
    
    // Restore original baud rate
    auxSerial.end();
    delay(100);
    auxSerial.begin(AUX_BAUD_RATE, SERIAL_8N1, AUX_RX_PIN, AUX_TX_PIN);
    delay(100);
    
    printInfo("Restored original baud rate: " + String(AUX_BAUD_RATE));
}

// ============================================================================
// Web Focuser Command Handler
// ============================================================================

bool handleWebFocuserCommand(String command, JsonDocument& doc) {
    printInfo("Web command: " + command);
    
    if (command == "focuser:connect") {
        // Connect to focuser
        if (initializeFocuser()) {
            // Send status update to all connected clients
            for (int i = 0; i < 8; i++) {
                wifiManager.sendFocuserStatus(i, focuserConnected, currentPosition, targetPosition, currentSpeed, isMoving);
            }
            return true;
        }
        return false;
    }
    else if (command == "focuser:getPosition") {
        // Get current position
        if (getFocuserPosition()) {
            // Send status update to all connected clients
            for (int i = 0; i < 8; i++) {
                wifiManager.sendFocuserStatus(i, focuserConnected, currentPosition, targetPosition, currentSpeed, isMoving);
            }
            return true;
        }
        return false;
    }
    else if (command == "focuser:setSpeed") {
        // Set focuser speed
        if (doc["speed"].is<uint8_t>()) {
            uint8_t newSpeed = doc["speed"];
            if (newSpeed >= 1 && newSpeed <= 9) {
                currentSpeed = newSpeed;
                printInfo("Speed set to: " + String(currentSpeed));
                
                // Send status update to all connected clients
                for (int i = 0; i < 8; i++) {
                    wifiManager.sendFocuserStatus(i, focuserConnected, currentPosition, targetPosition, currentSpeed, isMoving);
                }
                return true;
            }
        }
        return false;
    }
    else if (command == "focuser:move") {
        // Move focuser in specified direction
        if (doc["direction"].is<String>() && doc["speed"].is<uint8_t>()) {
            String direction = doc["direction"];
            uint8_t speed = doc["speed"];
            
            if (direction == "in") {
                if (moveFocuser(1, speed)) {
                    isMoving = true;
                    // Send status update to all connected clients
                    for (int i = 0; i < 8; i++) {
                        wifiManager.sendFocuserStatus(i, focuserConnected, currentPosition, targetPosition, currentSpeed, isMoving);
                    }
                    return true;
                }
            } else if (direction == "out") {
                if (moveFocuser(0, speed)) {
                    isMoving = true;
                    // Send status update to all connected clients
                    for (int i = 0; i < 8; i++) {
                        wifiManager.sendFocuserStatus(i, focuserConnected, currentPosition, targetPosition, currentSpeed, isMoving);
                    }
                    return true;
                }
            }
        }
        return false;
    }
    else if (command == "focuser:step") {
        // Step focuser by specified number of steps
        if (doc["direction"].is<String>() && doc["steps"].is<uint32_t>() && doc["speed"].is<uint8_t>()) {
            String direction = doc["direction"];
            uint32_t steps = doc["steps"];
            uint8_t speed = doc["speed"];
            
            if (direction == "in") {
                if (stepFocuser(1, steps, speed)) {
                    isMoving = true;
                    // Send status update to all connected clients
                    for (int i = 0; i < 8; i++) {
                        wifiManager.sendFocuserStatus(i, focuserConnected, currentPosition, targetPosition, currentSpeed, isMoving);
                    }
                    return true;
                }
            } else if (direction == "out") {
                if (stepFocuser(0, steps, speed)) {
                    isMoving = true;
                    // Send status update to all connected clients
                    for (int i = 0; i < 8; i++) {
                        wifiManager.sendFocuserStatus(i, focuserConnected, currentPosition, targetPosition, currentSpeed, isMoving);
                    }
                    return true;
                }
            }
        }
        return false;
    }
    else if (command == "focuser:stop") {
        // Stop focuser movement
        if (stopFocuser()) {
            isMoving = false;
            // Send status update to all connected clients
            for (int i = 0; i < 8; i++) {
                wifiManager.sendFocuserStatus(i, focuserConnected, currentPosition, targetPosition, currentSpeed, isMoving);
            }
            return true;
        }
        return false;
    }
    else if (command == "focuser:goto") {
        // Go to specific position
        if (doc["position"].is<uint32_t>()) {
            uint32_t position = doc["position"];
            if (gotoPosition(position)) {
                targetPosition = position;
                isMoving = true;
                // Send status update to all connected clients
                for (int i = 0; i < 8; i++) {
                    wifiManager.sendFocuserStatus(i, focuserConnected, currentPosition, targetPosition, currentSpeed, isMoving);
                }
                return true;
            }
        }
        return false;
    }
    
    printError("Unknown focuser command: " + command);
    return false;
}
