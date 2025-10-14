/*
    Celestron AUX Protocol Implementation for ESP32
    Based on INDI driver source code analysis
    
    Copyright (C) 2024
*/

#pragma once

#include <Arduino.h>
#include <vector>

/**
 * Celestron AUX Protocol Implementation
 * Based on verified INDI driver source code
 */
namespace CelestronAux {

// Type definitions
typedef std::vector<uint8_t> Buffer;

// Constants
const uint8_t AUX_HDR = 0x3B;  // Packet preamble

/**
 * AUX Protocol Command Definitions
 * Verified from INDI celestronauxpacket.h
 */
enum Command {
    MC_GET_POSITION = 0x01,         // Return 24 bit position
    MC_GOTO_FAST = 0x02,            // Send 24 bit target
    MC_SET_POSITION = 0x04,         // Send 24 bit new position
    MC_SET_POS_GUIDERATE = 0x06,
    MC_SET_NEG_GUIDERATE = 0x07,
    MC_LEVEL_START = 0x0b,
    MC_SET_POS_BACKLASH = 0x10,     // 1 byte, 0-99
    MC_SET_NEG_BACKLASH = 0x11,     // 1 byte, 0-99
    MC_SLEW_DONE = 0x13,            // Return 0xFF when move finished
    MC_GOTO_SLOW = 0x17,            // Send 24 bit target
    MC_SEEK_INDEX = 0x19,
    MC_MOVE_POS = 0x24,             // Send move rate 0-9
    MC_MOVE_NEG = 0x25,             // Send move rate 0-9
    MC_GET_POS_BACKLASH = 0x40,     // 1 byte, 0-99
    MC_GET_NEG_BACKLASH = 0x41,     // 1 byte, 0-99
    
    // Common to all devices
    GET_VER = 0xfe,                 // Return 2 or 4 bytes major.minor.build
    
    // Focuser specific commands
    FOC_CALIB_ENABLE = 42,          // Send 0 to start or 1 to stop
    FOC_CALIB_DONE = 43,            // Returns 2 bytes [0] done, [1] state 0-12
    FOC_GET_HS_POSITIONS = 44       // Returns 2 ints low and high limits
};

/**
 * AUX Protocol Target Device IDs
 * Verified from INDI celestronauxpacket.h
 */
enum Target {
    ANY = 0x00,
    MB = 0x01,
    HC = 0x04,                      // Hand Controller
    HCP = 0x0d,
    AZM = 0x10,                     // Azimuth/hour angle axis motor
    ALT = 0x11,                     // Altitude/declination axis motor
    FOCUSER = 0x12,                 // Focuser motor
    APP = 0x20,                     // Application (our ESP32)
    NEX_REMOTE = 0x22,
    GPS = 0xb0,                     // GPS Unit
    WiFi = 0xb5,                    // WiFi Board
    BAT = 0xb6,
    CHG = 0xb7,
    LIGHT = 0xbf
};

/**
 * AUX Protocol Packet Class
 * Handles packet construction, parsing, and checksum calculation
 */
class Packet {
public:
    // Packet fields
    uint32_t length;
    Target source;
    Target destination;
    Command command;
    Buffer data;
    
    // Constructors
    Packet();
    Packet(Target source, Target destination, Command command, Buffer data);
    Packet(Target source, Target destination, Command command);
    
    // Methods
    void fillBuffer(Buffer &buf);
    bool parse(Buffer buf);
    uint8_t calculateChecksum(Buffer packet);
    
    // Utility methods
    static String bufferToHex(Buffer data);
    static Buffer hexToBuffer(String hex);
};

/**
 * AUX Protocol Communicator Class
 * Handles high-level communication with Celestron devices
 */
class Communicator {
public:
    // Constructor
    Communicator();
    Communicator(Target source);
    
    // Communication methods
    bool sendCommand(HardwareSerial &serial, Target dest, Command cmd, Buffer data, Buffer &reply);
    bool sendCommand(HardwareSerial &serial, Target dest, Command cmd, Buffer &reply);
    bool commandBlind(HardwareSerial &serial, Target dest, Command cmd, Buffer data);
    
    // Properties
    Target source;
    
    // Configuration
    static const uint32_t TIMEOUT_MS = 2000;  // 2 second timeout
    static const uint32_t RETRY_COUNT = 3;    // Retry failed commands
    
private:
    // Low-level communication
    bool sendPacket(HardwareSerial &serial, Target dest, Command cmd, Buffer data);
    bool readPacket(HardwareSerial &serial, Packet &reply);
    
    // Utility methods
    void flushSerial(HardwareSerial &serial);
    bool waitForHeader(HardwareSerial &serial, uint32_t timeoutMs);
};

} // namespace CelestronAux
