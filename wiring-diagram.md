# ESP32 Celestron Focuser Wiring Diagram

## Overview

This diagram shows how to connect an ESP32 DevKit v1 to a Celestron AUX port through a level shifter for safe 3.3V to 5V communication.

## Components Required

- ESP32 DevKit v1 board
- 74HC125 Quad 3-State Buffer (or similar level shifter)
- Celestron telescope with AUX port
- Jumper wires
- Breadboard (optional)

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
│  GND            │────▶│ GND                 │────▶│ AUX GND         │
│                 │     │                     │     │                 │
│  3.3V           │────▶│ VCC (3.3V side)     │     │                 │
│                 │     │                     │     │                 │
│  USB            │     │ VCC (5V side)  ────▶│     │ 12V (external)  │
│  (Power)        │     │                     │     │ (separate)      │
└─────────────────┘     └─────────────────────┘     └─────────────────┘
```

## Detailed Pin Connections

### ESP32 DevKit v1 to 74HC125
| ESP32 Pin | 74HC125 Pin | Function |
|-----------|-------------|----------|
| GPIO16    | 1A (Input)  | ESP32 RX → Level Shifter Input |
| GPIO17    | 2A (Input)  | ESP32 TX → Level Shifter Input |
| GND       | GND         | Common Ground |
| 3.3V      | VCC         | 3.3V Power Supply |

### 74HC125 to Celestron AUX Port
| 74HC125 Pin | AUX Port | Function |
|-------------|----------|----------|
| 1Y (Output) | TX       | 5V TTL TX Signal |
| 2Y (Output) | RX       | 5V TTL RX Signal |
| GND         | GND      | Common Ground |
| VCC (5V)    | -        | External 5V Supply (separate) |

### Power Supply Connections
| Component | Power Source | Voltage | Notes |
|-----------|--------------|---------|-------|
| ESP32 | USB | 5V input, 3.3V regulated | USB power |
| 74HC125 (3.3V side) | ESP32 3.3V | 3.3V | From ESP32 |
| 74HC125 (5V side) | External supply | 5V | Separate 5V supply |
| AUX Port | External supply | 12V | Separate 12V supply |

## Important Notes

### Voltage Levels
- **ESP32 Logic:** 3.3V TTL
- **AUX Port Logic:** 5V TTL
- **Level Shifter:** Converts 3.3V ↔ 5V signals

### Power Supply
- **ESP32:** Powered via USB (5V input, 3.3V regulated)
- **AUX Port:** Requires 12V external supply (separate from level shifter)
- **Level Shifter:** Needs both 3.3V (from ESP32) and 5V (separate supply)
- **Important:** AUX port 12V and level shifter 5V are separate supplies

### Safety Considerations
- **NEVER** connect ESP32 directly to AUX port (5V will damage ESP32)
- **ALWAYS** use level shifter for signal conversion
- **VERIFY** voltage levels with multimeter before connecting
- **DISCONNECT** power before making connections

## Alternative Level Shifters

If 74HC125 is not available, these alternatives work:
- **74HC245** - Octal bus transceiver
- **74LVC1T45** - Single bit level shifter
- **TXS0108E** - 8-channel level shifter
- **BSS138** - MOSFET-based level shifter

## Testing Connections

1. **Power Test:** Verify 3.3V on ESP32, 5V on level shifter
2. **Continuity Test:** Check all connections with multimeter
3. **Signal Test:** Use oscilloscope to verify signal conversion
4. **Communication Test:** Run ESP32 program and test AUX communication

## Troubleshooting

### Common Issues
- **No Communication:** Check voltage levels and connections
- **Intermittent Signals:** Verify ground connections
- **ESP32 Damage:** Likely caused by direct 5V connection
- **Wrong Baud Rate:** Ensure 19200 baud on both sides

### Debug Steps
1. Check power supply voltages
2. Verify all connections
3. Test with simple serial communication
4. Use oscilloscope to check signal integrity
5. Enable debug output in ESP32 code
