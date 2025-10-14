/*
    Celestron AUX Protocol Implementation for ESP32
    Based on INDI driver source code analysis
    
    Copyright (C) 2024
*/

#include "celestron_aux.h"

namespace CelestronAux {

// ============================================================================
// Packet Class Implementation
// ============================================================================

Packet::Packet() {
    length = 0;
    source = Target::APP;
    destination = Target::FOCUSER;
    command = Command::GET_VER;
    data.clear();
}

Packet::Packet(Target source, Target destination, Command command, Buffer data) {
    this->source = source;
    this->destination = destination;
    this->command = command;
    this->data = data;
    this->length = data.size() + 3;  // source + dest + command
}

Packet::Packet(Target source, Target destination, Command command) {
    this->source = source;
    this->destination = destination;
    this->command = command;
    this->data.clear();
    this->length = 3;  // source + dest + command
}

void Packet::fillBuffer(Buffer &buf) {
    // Resize buffer: header + length + source + dest + command + data + checksum
    buf.resize(length + 3);
    
    // Fill packet
    buf[0] = CelestronAux::AUX_HDR;      // Header
    buf[1] = length;                     // Length
    buf[2] = static_cast<uint8_t>(source);      // Source
    buf[3] = static_cast<uint8_t>(destination); // Destination
    buf[4] = static_cast<uint8_t>(command);     // Command
    
    // Copy data
    for (size_t i = 0; i < data.size(); i++) {
        buf[5 + i] = data[i];
    }
    
    // Calculate and add checksum
    buf.back() = calculateChecksum(buf);
    
    // Debug output
    Serial.printf("TX: %s\n", bufferToHex(buf).c_str());
}

bool Packet::parse(Buffer packet) {
    // Minimum packet size: header + length + source + dest + command + checksum
    if (packet.size() < 6) {
        Serial.printf("Parse error: packet too small (%d bytes)\n", packet.size());
        return false;
    }
    
    // Check header
    if (packet[0] != CelestronAux::AUX_HDR) {
        Serial.printf("Parse error: invalid header (0x%02X)\n", packet[0]);
        return false;
    }
    
    // Extract length
    length = packet[1];
    
    // Verify packet size
    if (packet.size() != length + 3) {
        Serial.printf("Parse error: size mismatch (got %d, expected %d)\n", 
                     packet.size(), length + 3);
        return false;
    }
    
    // Extract fields
    source = static_cast<Target>(packet[2]);
    destination = static_cast<Target>(packet[3]);
    command = static_cast<Command>(packet[4]);
    
    // Extract data (everything between command and checksum)
    data = Buffer(packet.begin() + 5, packet.end() - 1);
    
    // Verify checksum
    uint8_t calculatedChecksum = calculateChecksum(packet);
    uint8_t receivedChecksum = packet[length + 2];
    
    if (calculatedChecksum != receivedChecksum) {
        Serial.printf("Parse error: checksum mismatch (calc 0x%02X, recv 0x%02X)\n",
                     calculatedChecksum, receivedChecksum);
        return false;
    }
    
    // Debug output
    Serial.printf("RX: %s\n", bufferToHex(packet).c_str());
    
    return true;
}

uint8_t Packet::calculateChecksum(Buffer packet) {
    // Checksum calculation from INDI source: sum of bytes 1 through (length+2), then negated
    int sum = 0;
    for (int i = 1; i < packet[1] + 2; i++) {
        sum += packet[i];
    }
    return (-sum & 0xFF);
}

String Packet::bufferToHex(Buffer data) {
    String result = "";
    for (size_t i = 0; i < data.size(); i++) {
        if (i > 0) result += " ";
        if (data[i] < 16) result += "0";
        result += String(data[i], HEX);
    }
    return result;
}

Buffer Packet::hexToBuffer(String hex) {
    Buffer result;
    hex.replace(" ", "");
    hex.toUpperCase();
    
    for (size_t i = 0; i < hex.length(); i += 2) {
        if (i + 1 < hex.length()) {
            String byteStr = hex.substring(i, i + 2);
            result.push_back(strtol(byteStr.c_str(), NULL, 16));
        }
    }
    
    return result;
}

// ============================================================================
// Communicator Class Implementation
// ============================================================================

Communicator::Communicator() {
    this->source = Target::APP;
}

Communicator::Communicator(Target source) {
    this->source = source;
}

bool Communicator::sendCommand(HardwareSerial &serial, Target dest, Command cmd, Buffer data, Buffer &reply) {
    int retryCount = 0;
    
    while (retryCount++ < RETRY_COUNT) {
        // Send packet
        if (!sendPacket(serial, dest, cmd, data)) {
            Serial.printf("Send failed on attempt %d\n", retryCount);
            continue;
        }
        
        // Read response
        Packet responsePacket;
        if (!readPacket(serial, responsePacket)) {
            Serial.printf("Read failed on attempt %d\n", retryCount);
            continue;
        }
        
        // Verify response
        if (responsePacket.command != cmd || 
            responsePacket.destination != Target::APP || 
            responsePacket.source != dest) {
            Serial.printf("Invalid response on attempt %d\n", retryCount);
            continue;
        }
        
        // Success
        reply = responsePacket.data;
        return true;
    }
    
    Serial.printf("Command failed after %d attempts\n", RETRY_COUNT);
    return false;
}

bool Communicator::sendCommand(HardwareSerial &serial, Target dest, Command cmd, Buffer &reply) {
    Buffer emptyData;
    return sendCommand(serial, dest, cmd, emptyData, reply);
}

bool Communicator::commandBlind(HardwareSerial &serial, Target dest, Command cmd, Buffer data) {
    // For blind commands, just send the packet without waiting for response
    return sendPacket(serial, dest, cmd, data);
}

bool Communicator::sendPacket(HardwareSerial &serial, Target dest, Command cmd, Buffer data) {
    Packet packet(source, dest, cmd, data);
    Buffer txBuffer;
    packet.fillBuffer(txBuffer);
    
    // Flush serial buffer
    flushSerial(serial);
    
    // Send packet
    size_t bytesWritten = serial.write(txBuffer.data(), txBuffer.size());
    
    if (bytesWritten != txBuffer.size()) {
        Serial.printf("Send error: wrote %d of %d bytes\n", bytesWritten, txBuffer.size());
        return false;
    }
    
    serial.flush();
    return true;
}

bool Communicator::readPacket(HardwareSerial &serial, Packet &reply) {
    // Read all available data first
    Buffer rawData;
    uint32_t startTime = millis();
    
    // Wait a bit for all data to arrive
    while (millis() - startTime < 100) {  // Wait up to 100ms for complete packet
        if (serial.available()) {
            rawData.push_back(serial.read());
            startTime = millis();  // Reset timer when we get data
        }
        delay(1);
    }
    
    if (rawData.empty()) {
        Serial.println("No data received");
        return false;
    }
    
    // Debug: Show raw data received
    Serial.print("DEBUG: Raw data received: ");
    for (size_t i = 0; i < rawData.size(); i++) {
        Serial.printf("0x%02X ", rawData[i]);
    }
    Serial.println();
    
    // Build packet - check if it has header or not
    Buffer packet;
    size_t startIndex = 0;
    
    if (rawData[0] == AUX_HDR) {
        // Has header, use as-is
        packet = rawData;
    } else {
        // Missing header, add it
        packet.push_back(AUX_HDR);
        for (size_t i = 0; i < rawData.size(); i++) {
            packet.push_back(rawData[i]);
        }
    }
    
    uint8_t length = packet[1];
    Serial.printf("DEBUG: Packet length: 0x%02X\n", length);
    
    // Verify packet size
    size_t expectedSize = length + 3;  // header + length + data + checksum
    if (packet.size() != expectedSize) {
        Serial.printf("DEBUG: Packet size mismatch - got %d, expected %d\n", 
                     packet.size(), expectedSize);
        return false;
    }
    
    // Debug: Show final packet
    Serial.print("DEBUG: Final packet: ");
    for (size_t i = 0; i < packet.size(); i++) {
        Serial.printf("0x%02X ", packet[i]);
    }
    Serial.println();
    
    // Parse packet
    return reply.parse(packet);
}

void Communicator::flushSerial(HardwareSerial &serial) {
    while (serial.available()) {
        serial.read();
    }
}

bool Communicator::waitForHeader(HardwareSerial &serial, uint32_t timeoutMs) {
    uint32_t startTime = millis();
    
    while (millis() - startTime < timeoutMs) {
        if (serial.available()) {
            uint8_t byte = serial.read();
            if (byte == CelestronAux::AUX_HDR) {
                return true;
            }
        }
        delay(1);
    }
    
    return false;
}

} // namespace CelestronAux
