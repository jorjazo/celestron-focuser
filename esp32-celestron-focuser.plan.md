# ESP32 Celestron Focuser Controller (DevKit v1)

## Project Structure

Create a PlatformIO project with the following structure:

- `platformio.ini` - ESP32 DevKit v1 configuration with 19200 baud for AUX port
- `src/main.cpp` - Main program with command parser and serial bridge
- `src/celestron_aux.h` - AUX protocol packet definitions and utilities
- `src/celestron_aux.cpp` - AUX protocol implementation (packet building, checksum)
- `README.md` - Wiring instructions and usage guide
- `wiring-diagram.md` - Detailed wiring diagram with pin connections

## Key Implementation Details

### Hardware Interface (ESP32 DevKit v1)

- USB Serial (Serial) at 115200 baud for computer communication
- Hardware Serial2 (GPIO16/17) at 19200 baud for AUX port communication
- **GPIO Pinout for ESP32 DevKit v1:**
  - GPIO16 (RX2) → AUX Port TX (via level shifter)
  - GPIO17 (TX2) → AUX Port RX (via level shifter)
  - GND → AUX Port GND
  - 3.3V → Level shifter VCC (3.3V side)
- **Level Shifter Required:** ESP32 DevKit v1 operates at 3.3V, AUX port at 5V TTL
- **Power:** AUX port provides 12V for focuser, ESP32 powered via USB

### Celestron AUX Protocol (VERIFIED)

Based on INDI driver source code analysis:

- Packet format: `[0x3B, length, source, dest, command, data..., checksum]`
- Focuser device ID: 0x12 (FOCUSER)
- Source device ID: 0x20 (APP)
- Key commands: MC_GET_POSITION (0x01), MC_GOTO_FAST (0x02), MC_MOVE_POS/NEG (0x24/0x25)
- Motor speed commands: MC_MOVE_POS/NEG with rate parameter (0-9, where 0=stop, 1=slowest, 9=fastest)
- Checksum: `(-sum & 0xFF)` of bytes 1 through (length+2)

### Command Interface

Single-character commands from serial terminal:

- `+` - Move focuser inward (continuous)
- `-` - Move focuser outward (continuous)
- `s` - Stop movement
- `p` - Get current position
- `g` followed by number - Go to absolute position (e.g., "g5000")
- `1`-`9` - Set motor speed (1=slowest, 9=fastest)
- `0` - Stop movement (same as 's')
- `?` - Display help menu

### Error Handling

- Timeout detection for AUX responses
- Invalid command feedback
- Position limit checking
- Serial buffer overflow protection

## Implementation Steps

1. Create PlatformIO project structure with ESP32 DevKit v1 board configuration
2. Implement verified AUX protocol packet building and checksum calculation
3. Implement command parser for single-character commands
4. Add position tracking and absolute/relative movement functions
5. Create comprehensive README with ESP32 DevKit v1 wiring diagram and usage instructions
6. Create detailed wiring diagram showing ESP32, level shifter, and AUX port connections

### To-dos

- [ ] Create platformio.ini with ESP32 DevKit v1 configuration and dependencies
- [ ] Implement verified Celestron AUX protocol packet structure, checksum, and command definitions
- [ ] Implement dual serial communication (USB and AUX port) with command parser
- [ ] Implement focuser control functions (move, goto, get position, stop)
- [ ] Create README with ESP32 DevKit v1 wiring instructions, command reference, and safety notes
- [ ] Create detailed wiring diagram with ESP32, level shifter, and AUX port pin connections
