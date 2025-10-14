# ESP32 Celestron Focuser Controller Makefile
# PlatformIO-based build system

# ============================================================================
# Configuration
# ============================================================================

# PlatformIO Configuration
ENV = esp32dev
PROJECT_NAME = celestron-focuser

# Serial Configuration
MONITOR_BAUD = 115200
SERIAL_PORT ?= /dev/ttyUSB0

# ============================================================================
# Default Target
# ============================================================================

.PHONY: all
all: build

# ============================================================================
# Build Targets
# ============================================================================

.PHONY: build
build:
	@echo "Building $(PROJECT_NAME) with PlatformIO..."
	pio run

.PHONY: build-verbose
build-verbose:
	@echo "Building $(PROJECT_NAME) with verbose output..."
	pio run -v

.PHONY: clean
clean:
	@echo "Cleaning build directory..."
	pio run --target clean

.PHONY: clean-all
clean-all: clean
	@echo "Cleaning all PlatformIO files..."
	rm -rf .pio
	rm -rf build

# ============================================================================
# Upload Targets
# ============================================================================

.PHONY: upload
upload: build
	@echo "Uploading to ESP32 on $(SERIAL_PORT)..."
	pio run --target upload --upload-port $(SERIAL_PORT)

.PHONY: upload-fast
upload-fast: build
	@echo "Uploading to ESP32 (fast mode) on $(SERIAL_PORT)..."
	pio run --target upload --upload-port $(SERIAL_PORT) --upload-speed 921600

.PHONY: upload-only
upload-only:
	@echo "Uploading without building..."
	pio run --target upload --upload-port $(SERIAL_PORT)

# ============================================================================
# Monitor Targets
# ============================================================================

.PHONY: monitor
monitor:
	@echo "Starting serial monitor on $(SERIAL_PORT) at $(MONITOR_BAUD) baud..."
	pio device monitor --port $(SERIAL_PORT) --baud $(MONITOR_BAUD)

.PHONY: upload-monitor
upload-monitor: upload
	@echo "Starting serial monitor after upload..."
	pio device monitor --port $(SERIAL_PORT) --baud $(MONITOR_BAUD)

.PHONY: monitor-only
monitor-only:
	@echo "Starting serial monitor (no upload)..."
	pio device monitor --baud $(MONITOR_BAUD)

# ============================================================================
# Development Targets
# ============================================================================

.PHONY: check
check:
	@echo "Checking PlatformIO installation..."
	@which pio > /dev/null || (echo "Error: PlatformIO not found. Please install it first." && exit 1)
	@echo "PlatformIO found: $$(pio --version)"

.PHONY: init
init:
	@echo "Initializing PlatformIO project..."
	pio project init --board esp32dev

.PHONY: update
update:
	@echo "Updating PlatformIO platform and libraries..."
	pio platform update
	pio lib update

.PHONY: libs
libs:
	@echo "Installing required libraries..."
	pio lib install

# ============================================================================
# Device Management
# ============================================================================

.PHONY: list-ports
list-ports:
	@echo "Available serial ports:"
	pio device list

.PHONY: board-info
board-info:
	@echo "Board information for $(ENV):"
	pio boards esp32dev

# ============================================================================
# Testing and Validation
# ============================================================================

.PHONY: test-build
test-build: clean build
	@echo "Build test completed successfully"

.PHONY: verify
verify: build
	@echo "Verifying build..."
	@if [ -f ".pio/build/$(ENV)/firmware.bin" ]; then \
		echo "✓ Binary file created successfully"; \
		ls -lh .pio/build/$(ENV)/firmware.bin; \
		echo "✓ Build size information:"; \
		pio run --target size; \
	else \
		echo "✗ Binary file not found"; \
		exit 1; \
	fi

.PHONY: size
size: build
	@echo "Build size information:"
	pio run --target size

# ============================================================================
# Advanced Targets
# ============================================================================

.PHONY: check-firmware
check-firmware:
	@echo "Checking firmware integrity..."
	pio run --target checkprogsize

.PHONY: erase-flash
erase-flash:
	@echo "Erasing ESP32 flash..."
	pio run --target erase

.PHONY: program
program: upload
	@echo "Programming completed"

# ============================================================================
# Documentation
# ============================================================================

.PHONY: help
help:
	@echo "ESP32 Celestron Focuser Controller - PlatformIO Makefile Help"
	@echo "============================================================="
	@echo ""
	@echo "Build Targets:"
	@echo "  build          - Build the project"
	@echo "  build-verbose  - Build with verbose output"
	@echo "  clean          - Clean build directory"
	@echo "  clean-all      - Clean all PlatformIO files"
	@echo ""
	@echo "Upload Targets:"
	@echo "  upload         - Build and upload to ESP32"
	@echo "  upload-fast    - Build and upload with fast baud rate"
	@echo "  upload-only    - Upload without building"
	@echo "  upload-monitor - Upload and start serial monitor"
	@echo ""
	@echo "Monitor Targets:"
	@echo "  monitor        - Start serial monitor"
	@echo "  monitor-only   - Start serial monitor (no upload)"
	@echo ""
	@echo "Development Targets:"
	@echo "  check          - Check PlatformIO installation"
	@echo "  init           - Initialize PlatformIO project"
	@echo "  update         - Update platform and libraries"
	@echo "  libs           - Install required libraries"
	@echo "  list-ports     - List available serial ports"
	@echo "  board-info     - Show board information"
	@echo ""
	@echo "Testing Targets:"
	@echo "  test-build     - Clean build test"
	@echo "  verify         - Verify build output"
	@echo "  size           - Show build size information"
	@echo "  check-firmware - Check firmware integrity"
	@echo ""
	@echo "Advanced Targets:"
	@echo "  erase-flash    - Erase ESP32 flash"
	@echo "  program        - Complete programming sequence"
	@echo ""
	@echo "Configuration:"
	@echo "  SERIAL_PORT    - Serial port (default: /dev/ttyUSB0)"
	@echo "  MONITOR_BAUD   - Monitor baud rate (default: 115200)"
	@echo "  ENV            - PlatformIO environment (default: esp32dev)"
	@echo ""
	@echo "Examples:"
	@echo "  make build                    # Build the project"
	@echo "  make upload SERIAL_PORT=/dev/ttyUSB1  # Upload to specific port"
	@echo "  make upload-monitor           # Upload and monitor"
	@echo "  make clean build             # Clean and build"
	@echo "  make verify                  # Build and verify"
	@echo "  make size                    # Show build size"

# ============================================================================
# Quick Commands
# ============================================================================

.PHONY: quick
quick: clean build upload monitor

.PHONY: dev
dev: build upload-monitor

.PHONY: rebuild
rebuild: clean-all build

# ============================================================================
# Environment Setup
# ============================================================================

.PHONY: setup
setup: check
	@echo "Setting up PlatformIO environment..."
	pio platform install espressif32
	@echo "Setup completed. You can now build the project with 'make build'"

# ============================================================================
# Arduino CLI Compatibility (Optional)
# ============================================================================

.PHONY: arduino-build
arduino-build:
	@echo "Using Arduino CLI to build (requires arduino-cli)..."
	@which arduino-cli > /dev/null || (echo "Error: arduino-cli not found. Please install it first." && exit 1)
	arduino-cli compile --fqbn esp32:esp32:esp32dev --build-path build src/main.cpp

.PHONY: arduino-upload
arduino-upload: arduino-build
	@echo "Using Arduino CLI to upload..."
	arduino-cli upload --fqbn esp32:esp32:esp32dev --port $(SERIAL_PORT) --input-dir build

# ============================================================================
# End of Makefile
# ============================================================================