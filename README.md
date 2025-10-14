# ESP32 Celestron Focuser Controller

A USB-to-AUX serial bridge that allows you to control your Celestron focuser through simple serial commands from your computer.

## Features

- **Simple Command Interface**: Single-character commands for easy control
- **Motor Speed Control**: Adjustable speed settings (1-9)
- **Absolute Positioning**: Go to specific focus positions
- **Real-time Status**: Monitor position and movement status
- **Safety Features**: Built-in error handling and timeout protection
- **Verified Protocol**: Based on actual INDI driver source code
- **WiFi Connectivity**: Built-in WiFi with AP/Station modes
- **Web Interface**: Easy WiFi configuration and focuser control through web browser
- **Remote Control**: Full focuser control over WiFi network with real-time status
- **Real-time Updates**: Live position tracking and status updates via WebSocket
- **mDNS Support**: Access device by friendly hostname (e.g., celestron-focuser.local)

## Hardware Requirements

### ESP32 DevKit v1 Board
- ESP32 microcontroller with built-in WiFi
- USB connection for programming and communication
- GPIO16/17 for AUX port communication
- Built-in WiFi antenna for network connectivity

### Level Shifter (Required!)
- **74HC125** Quad 3-State Buffer (recommended)
- Alternative: 74HC245, 74LVC1T45, TXS0108E, or BSS138-based level shifter
- **Critical**: ESP32 operates at 3.3V, AUX port at 5V TTL

### Celestron Equipment
- Celestron telescope with AUX port
- Celestron FocusMotor or compatible focuser
- 12V power supply for focuser

## Wiring Diagram

```
ESP32 DevKit v1         74HC125 Level Shifter        Celestron AUX Port
┌─────────────────┐     ┌─────────────────────┐     ┌─────────────────┐
│                 │     │                     │     │                 │
│  GPIO16 (RX2)   │────▶│ 1A (Input)          │     │                 │
│                 │     │ 1Y (Output)    ────▶│────▶│ AUX TX          │
│                 │     │                     │     │                 │
│  GPIO17 (TX2)   │◀────│ 2Y (Output)         │     │                 │
│                 │     │ 2A (Input)     ◀────│────│ AUX RX          │
│                 │     │                     │     │                 │
│  GND            │────▶│ GND                 │     │                 │
│                 │     │                     │     │                 │
│  3.3V           │────▶│ VCC (3.3V side)     │     │                 │
│                 │     │                     │     │                 │
│  USB            │     │ VCC (5V side)  ────▶│────▶│ 12V (external)  │
│  (Power)        │     │                     │     │                 │
└─────────────────┘     └─────────────────────┘     └─────────────────┘
```

### Pin Connections

| ESP32 Pin | 74HC125 Pin | AUX Port | Function |
|-----------|-------------|----------|----------|
| GPIO16    | 1A (Input)  | TX       | ESP32 RX → AUX TX |
| GPIO17    | 2A (Input)  | RX       | ESP32 TX → AUX RX |
| GND       | GND         | GND      | Common Ground |
| 3.3V      | VCC (3.3V)  | -        | 3.3V Power Supply |
| -         | VCC (5V)    | -        | External 5V Supply (separate) |
| -         | -           | 12V      | External 12V Supply (separate) |

## Installation

### Option 1: Using Makefile (Recommended)

#### 1. Install Arduino CLI
```bash
# Install Arduino CLI
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# Or using package manager
# Ubuntu/Debian: sudo apt install arduino-cli
# macOS: brew install arduino-cli
# Windows: Download from https://arduino.github.io/arduino-cli/
```

#### 2. Setup and Build
```bash
# Clone the repository
git clone <repository-url>
cd celestron-focuser

# Setup ESP32 core (first time only)
make setup

# Build the project
make build

# Upload to ESP32 (replace /dev/ttyUSB0 with your port)
make upload SERIAL_PORT=/dev/ttyUSB0

# Upload and start monitoring
make upload-monitor SERIAL_PORT=/dev/ttyUSB0
```

#### 3. Quick Commands
```bash
# Quick build, upload, and monitor
make quick SERIAL_PORT=/dev/ttyUSB0

# List available serial ports
make list-ports

# Show help
make help
```

#### 4. Using Build Script (Alternative)
```bash
# Make script executable (first time only)
chmod +x build.sh

# Quick development workflow
./build.sh quick -p /dev/ttyUSB0

# Setup environment
./build.sh setup

# Build only
./build.sh build

# Upload and monitor
./build.sh upload -p /dev/ttyUSB0
./build.sh monitor -p /dev/ttyUSB0

# Show help
./build.sh help
```

### Option 2: Using PlatformIO

#### 1. Install PlatformIO
```bash
# Install PlatformIO Core
pip install platformio

# Or install PlatformIO IDE extension in VS Code
```

#### 2. Clone and Build
```bash
# Clone the repository
git clone <repository-url>
cd celestron-focuser

# Build the project
pio run

# Upload to ESP32
pio run --target upload
```

#### 3. Monitor Serial Output
```bash
# Monitor serial communication
pio device monitor --baud 115200
```

## Usage

### Serial Terminal Setup
- **Baud Rate**: 115200
- **Data Bits**: 8
- **Parity**: None
- **Stop Bits**: 1
- **Flow Control**: None

### Command Reference

#### Basic Movement Commands
- `+` - Move focuser **INWARD** (continuous)
- `-` - Move focuser **OUTWARD** (continuous)
- `s` or `0` - **Stop** movement immediately

#### Position Commands
- `p` - Get **current position**
- `g####` - **Go to** absolute position (e.g., `g5000`)

#### Speed Control
- `1` - Set speed to **1** (slowest)
- `2` - Set speed to **2**
- `3` - Set speed to **3**
- `4` - Set speed to **4**
- `5` - Set speed to **5** (default)
- `6` - Set speed to **6**
- `7` - Set speed to **7**
- `8` - Set speed to **8**
- `9` - Set speed to **9** (fastest)

#### Information Commands
- `?` - Show **help** menu
- `i` - Show **status** information

### Example Session

```
INFO: ESP32 Celestron Focuser Controller
INFO: ==================================
INFO: Hardware: ESP32 DevKit v1
INFO: USB Serial: 115200 baud
INFO: AUX Serial: 19200 baud
INFO: AUX Pins: RX=16, TX=17
INFO: 
SUCCESS: Focuser initialized successfully
SUCCESS: Firmware Version: 1.2
INFO: Current Position: 15000
INFO: Target Position: 15000
INFO: Current Speed: 5
INFO: Moving: No
INFO: 
INFO: Available Commands:
INFO:   +     - Move focuser INWARD (continuous)
INFO:   -     - Move focuser OUTWARD (continuous)
INFO:   s, 0  - Stop movement
INFO:   p     - Get current position
INFO:   g#### - Go to absolute position (e.g., g5000)
INFO:   1-9   - Set motor speed (1=slowest, 9=fastest)
INFO:   ?     - Show this help
INFO:   i     - Show status information

> 3
SUCCESS: Speed set to 3
> +
INFO: Moving focuser INWARD at speed 3
> s
INFO: Stopping focuser
SUCCESS: Focuser reached target position: 14850
> g20000
INFO: Moving to position 20000
SUCCESS: Focuser reached target position: 20000
```

## WiFi Setup and Web Interface

### Initial WiFi Configuration

When the ESP32 starts for the first time (or if no WiFi credentials are saved), it will automatically start in Access Point (AP) mode:

1. **Connect to ESP32 AP**:
   - SSID: `Celestron-Focuser`
   - Password: `focuser123`

2. **Open Web Interface**:
   - Navigate to: `http://192.168.4.1`
   - The web interface will load automatically

3. **Configure WiFi**:
   - Enter your WiFi network name (SSID)
   - Enter your WiFi password
   - Optionally set a custom hostname
   - Click "Save & Connect"

4. **Device Restart**:
   - The ESP32 will restart and attempt to connect to your WiFi
   - Once connected, it will switch to Station mode
   - The web interface will be available at the ESP32's IP address
   - mDNS will make the device accessible at `celestron-focuser.local` (or your custom hostname)

### WiFi Operation Modes

#### Access Point (AP) Mode
- **When**: No saved WiFi credentials or connection failed
- **SSID**: `Celestron-Focuser`
- **Password**: `focuser123`
- **IP**: `192.168.4.1`
- **Purpose**: Initial setup and configuration

#### Station Mode
- **When**: Successfully connected to configured WiFi network
- **IP**: Assigned by your router (check serial monitor or use `w` command)
- **Hostname**: `celestron-focuser` (or custom name)
- **mDNS**: `celestron-focuser.local` (or custom-name.local)
- **Purpose**: Normal operation on your network

### Web Interface Features

The web interface provides:

- **Focuser Control Panel**: Complete focuser control interface with:
  - Real-time position display (current and target)
  - Speed control slider (1-9)
  - Move IN/OUT buttons for continuous movement
  - Go to Position for precise positioning
  - Emergency STOP button
  - Connection status indicators
- **WiFi Configuration**: Easy setup of network credentials
- **Real-time Status**: Current WiFi and focuser status with live updates
- **Device Information**: IP address, signal strength, hostname, mDNS name
- **Configuration Management**: Save/clear WiFi settings
- **Responsive Design**: Works on desktop and mobile devices
- **mDNS Integration**: Shows friendly hostname for easy access
- **WebSocket Communication**: Real-time bidirectional communication

### WiFi Commands

New serial commands for WiFi management:

- `w` - Show WiFi status and web interface URL
- `?` - Show help (includes WiFi web interface URL)

### Web Control Usage

Once connected to WiFi, you can control the focuser through the web interface:

#### Accessing the Web Interface
1. **Via mDNS**: `http://celestron-focuser.local` (recommended)
2. **Via IP**: `http://[ESP32-IP-ADDRESS]` (use `w` command to see IP)
3. **Via AP Mode**: `http://192.168.4.1` (when in Access Point mode)

#### Focuser Control Features
- **Real-time Position**: Shows current focuser position with live updates
- **Speed Control**: Adjustable speed from 1 (slowest) to 9 (fastest)
- **Continuous Movement**: Move IN/OUT buttons for continuous focuser movement
- **Precise Positioning**: Go to Position for exact focuser positioning
- **Emergency Stop**: STOP button to immediately halt all movement
- **Connection Status**: Visual indicators for focuser and movement status
- **Auto-refresh**: Position updates every 2 seconds automatically

#### Web Interface Controls
- **Connect Focuser**: Establishes connection to the focuser hardware
- **Get Position**: Manually refresh current position
- **Speed Slider**: Real-time speed adjustment (1-9)
- **Movement Buttons**: Large, easy-to-use IN/OUT movement controls
- **Position Input**: Enter exact position for precise focusing
- **Status Indicators**: Color-coded connection and movement status

#### Mobile-Friendly Design
- **Responsive Layout**: Works perfectly on phones and tablets
- **Touch-Optimized**: Large buttons for easy touch control
- **Auto-reconnection**: WebSocket automatically reconnects if connection is lost
- **Visual Feedback**: Clear status indicators and real-time updates

### mDNS Support

The ESP32 supports mDNS (multicast DNS) for easy device discovery:

#### How mDNS Works
- **Automatic Discovery**: Once connected to WiFi, the device advertises itself as `celestron-focuser.local`
- **Easy Access**: Simply type `http://celestron-focuser.local` in any browser on the same network
- **Custom Hostnames**: Set a custom hostname in the web interface (e.g., `my-focuser`) and access it at `my-focuser.local`

#### Benefits
- **No IP Address Needed**: Forget about remembering or finding the device's IP address
- **Network Independent**: Works regardless of your router's IP range
- **Cross-Platform**: Works on Windows, macOS, Linux, and mobile devices
- **Automatic Updates**: If the IP changes, mDNS automatically updates

#### Requirements
- **Same Network**: Both the ESP32 and your computer must be on the same WiFi network
- **mDNS Support**: Most modern operating systems support mDNS out of the box
  - **Windows**: Requires Bonjour Print Services (usually pre-installed)
  - **macOS**: Built-in support
  - **Linux**: Requires `avahi-daemon` (usually pre-installed)
  - **Mobile**: iOS and Android support mDNS

### Troubleshooting WiFi

#### Cannot Connect to AP
- Ensure ESP32 is powered on and running
- Check that you're connecting to `Celestron-Focuser`
- Use password `focuser123`
- Try different device or WiFi settings

#### Web Interface Not Loading
- Check IP address in serial monitor output
- Ensure you're on the same network (AP mode: connect to ESP32 AP)
- Try refreshing the page
- Clear browser cache if needed

#### Cannot Connect to Home WiFi
- Verify WiFi credentials are correct
- Check WiFi signal strength
- Ensure 2.4GHz network (ESP32 doesn't support 5GHz)
- Try clearing WiFi config and reconfiguring

#### Device Keeps Switching to AP Mode
- Check WiFi network stability
- Verify credentials are saved correctly
- Check for WiFi interference
- Ensure router supports ESP32 devices

## Safety Warnings

### ⚠️ Critical Safety Information

1. **Voltage Protection**: 
   - **NEVER** connect ESP32 directly to AUX port
   - **ALWAYS** use level shifter (3.3V ↔ 5V conversion)
   - **VERIFY** voltage levels with multimeter before connecting

2. **Power Supply**:
   - ESP32 powered via USB (5V input, 3.3V regulated)
   - AUX port requires 12V external supply (separate)
   - Level shifter needs 3.3V (from ESP32) and 5V (separate supply)
   - **Important**: AUX port 12V and level shifter 5V are separate supplies

3. **Connections**:
   - Double-check all wiring before powering on
   - Ensure proper ground connections
   - Use appropriate wire gauge for power connections

4. **Testing**:
   - Test with simple commands first
   - Monitor serial output for errors
   - Stop immediately if unexpected behavior occurs

## Troubleshooting

### Common Issues

#### "Focuser not connected"
- Check power supply to focuser
- Verify AUX port connections
- Ensure level shifter is properly connected
- Check baud rate settings (19200 for AUX)

#### "Send failed" or "Read failed"
- Verify wiring connections
- Check voltage levels (3.3V on ESP32, 5V on level shifter)
- Ensure proper ground connections
- Try reducing baud rate to 9600

#### "Checksum mismatch"
- Indicates communication errors
- Check for loose connections
- Verify level shifter functionality
- Ensure stable power supply

#### ESP32 not responding
- Check USB connection
- Verify correct COM port
- Ensure ESP32 is in programming mode
- Try resetting ESP32

### Debug Steps

1. **Power Test**: Verify 3.3V on ESP32, 5V on level shifter
2. **Continuity Test**: Check all connections with multimeter
3. **Signal Test**: Use oscilloscope to verify signal conversion
4. **Communication Test**: Enable debug output in code
5. **Isolation Test**: Test each component separately

## Build System

### Makefile Features

The included Makefile provides a comprehensive build system with the following features:

#### Build Targets
- `make build` - Build the project
- `make clean` - Clean build directory
- `make clean-all` - Clean all generated files

#### Upload Targets
- `make upload` - Build and upload to ESP32
- `make upload-fast` - Upload with verification
- `make upload-monitor` - Upload and start serial monitor

#### Development Targets
- `make setup` - Install ESP32 core and check dependencies
- `make check` - Verify Arduino CLI installation
- `make list-ports` - List available serial ports
- `make board-info` - Show ESP32 board information

#### Quick Commands
- `make quick` - Build, upload, and monitor in one command
- `make dev` - Development workflow (build + upload + monitor)
- `make help` - Show all available commands

#### Configuration
- `SERIAL_PORT` - Set serial port (default: /dev/ttyUSB0)
- `MONITOR_BAUD` - Set monitor baud rate (default: 115200)
- `UPLOAD_BAUD` - Set upload baud rate (default: 921600)

#### PlatformIO Compatibility
- `make pio-build` - Use PlatformIO to build
- `make pio-upload` - Use PlatformIO to upload
- `make pio-monitor` - Use PlatformIO to monitor

### Example Usage
```bash
# Quick development workflow
make quick SERIAL_PORT=/dev/ttyUSB1

# Custom configuration
make upload SERIAL_PORT=/dev/ttyACM0 MONITOR_BAUD=9600

# Development with PlatformIO
make pio-build && make pio-upload
```

### Build Script Features

The included `build.sh` script provides a user-friendly interface with the following features:

#### Available Actions
- `./build.sh build` - Build the project
- `./build.sh upload` - Build and upload to ESP32
- `./build.sh monitor` - Start serial monitor
- `./build.sh quick` - Build, upload, and monitor
- `./build.sh setup` - Setup development environment
- `./build.sh clean` - Clean build files
- `./build.sh ports` - List available serial ports
- `./build.sh help` - Show help

#### Options
- `-p, --port PORT` - Specify serial port
- `-h, --help` - Show help

#### Features
- **Colored Output** - Easy to read status messages
- **Error Handling** - Automatic dependency checking
- **Flexible Port Selection** - Easy port specification
- **Quick Workflows** - One-command build, upload, and monitor

## Technical Details

### Protocol Information
- **AUX Protocol**: Based on verified INDI driver source code
- **Packet Format**: `[0x3B, length, source, dest, command, data..., checksum]`
- **Checksum**: `(-sum & 0xFF)` of bytes 1 through (length+2)
- **Device IDs**: Focuser=0x12, Application=0x20

### Communication Settings
- **USB Serial**: 115200 baud, 8N1
- **AUX Serial**: 19200 baud, 8N1
- **Timeout**: 2 seconds for command responses
- **Retry**: 3 attempts for failed commands

### Supported Commands
- `MC_GET_POSITION` (0x01) - Get current position
- `MC_GOTO_FAST` (0x02) - Go to absolute position
- `MC_MOVE_POS` (0x24) - Move positive direction
- `MC_MOVE_NEG` (0x25) - Move negative direction
- `MC_SLEW_DONE` (0x13) - Check movement status
- `GET_VER` (0xFE) - Get firmware version

## License

This project is open source. Please ensure compliance with any applicable licenses for the INDI library components used as reference.

## Contributing

Contributions are welcome! Please ensure any changes maintain compatibility with the verified AUX protocol implementation.

## Support

For issues and questions:
1. Check the troubleshooting section
2. Verify wiring and connections
3. Test with simple commands first
4. Check serial monitor output for error messages
