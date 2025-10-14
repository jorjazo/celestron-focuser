#!/bin/bash

# ESP32 Celestron Focuser Controller - Build Script
# PlatformIO-based build wrapper script

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
SERIAL_PORT="/dev/ttyUSB0"
ACTION="help"

# Function to print colored output
print_info() {
    echo -e "${BLUE}INFO:${NC} $1"
}

print_success() {
    echo -e "${GREEN}SUCCESS:${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}WARNING:${NC} $1"
}

print_error() {
    echo -e "${RED}ERROR:${NC} $1"
}

# Function to show usage
show_usage() {
    echo "ESP32 Celestron Focuser Controller - PlatformIO Build Script"
    echo "============================================================"
    echo ""
    echo "Usage: $0 [ACTION] [OPTIONS]"
    echo ""
    echo "Actions:"
    echo "  build     - Build the project"
    echo "  upload    - Build and upload to ESP32"
    echo "  monitor   - Start serial monitor"
    echo "  quick     - Build, upload, and monitor"
    echo "  setup     - Setup development environment"
    echo "  clean     - Clean build files"
    echo "  verify    - Build and verify output"
    echo "  size      - Show build size information"
    echo "  ports     - List available serial ports"
    echo "  help      - Show this help"
    echo ""
    echo "Options:"
    echo "  -p, --port PORT    - Serial port (default: $SERIAL_PORT)"
    echo "  -h, --help         - Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 build                    # Build the project"
    echo "  $0 upload -p /dev/ttyUSB1   # Upload to specific port"
    echo "  $0 quick -p /dev/ttyACM0    # Quick build, upload, monitor"
    echo "  $0 setup                    # Setup development environment"
    echo "  $0 verify                   # Build and verify"
    echo "  $0 size                     # Show build size"
}

# Function to check dependencies
check_dependencies() {
    print_info "Checking dependencies..."
    
    if ! command -v pio &> /dev/null; then
        print_error "PlatformIO not found. Please install it first."
        print_info "Install with: pip install platformio"
        print_info "Or visit: https://platformio.org/install"
        exit 1
    fi
    
    if ! command -v make &> /dev/null; then
        print_error "make not found. Please install build-essential."
        exit 1
    fi
    
    print_success "All dependencies found"
    print_info "PlatformIO version: $(pio --version)"
}

# Function to setup environment
setup_environment() {
    print_info "Setting up development environment..."
    make setup
    print_success "Environment setup complete"
}

# Function to build project
build_project() {
    print_info "Building project with PlatformIO..."
    make build
    print_success "Build complete"
}

# Function to upload to ESP32
upload_project() {
    print_info "Uploading to ESP32 on $SERIAL_PORT..."
    make upload SERIAL_PORT="$SERIAL_PORT"
    print_success "Upload complete"
}

# Function to start monitor
start_monitor() {
    print_info "Starting serial monitor on $SERIAL_PORT..."
    make monitor SERIAL_PORT="$SERIAL_PORT"
}

# Function to clean build files
clean_project() {
    print_info "Cleaning build files..."
    make clean
    print_success "Clean complete"
}

# Function to verify build
verify_build() {
    print_info "Building and verifying..."
    make verify
    print_success "Verification complete"
}

# Function to show build size
show_size() {
    print_info "Build size information:"
    make size
}

# Function to do quick build, upload, and monitor
quick_build() {
    print_info "Quick build, upload, and monitor..."
    make quick SERIAL_PORT="$SERIAL_PORT"
}

# Function to list available ports
list_ports() {
    print_info "Available serial ports:"
    make list-ports
}

# Function to check if port exists
check_port() {
    if [ ! -e "$SERIAL_PORT" ]; then
        print_warning "Port $SERIAL_PORT does not exist"
        print_info "Available ports:"
        list_ports
        return 1
    fi
    return 0
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        build|upload|monitor|quick|setup|clean|verify|size|ports|help)
            ACTION="$1"
            shift
            ;;
        -p|--port)
            SERIAL_PORT="$2"
            shift 2
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Main execution
case $ACTION in
    build)
        check_dependencies
        build_project
        ;;
    upload)
        check_dependencies
        check_port
        build_project
        upload_project
        ;;
    monitor)
        check_dependencies
        check_port
        start_monitor
        ;;
    quick)
        check_dependencies
        check_port
        quick_build
        ;;
    setup)
        check_dependencies
        setup_environment
        ;;
    clean)
        clean_project
        ;;
    verify)
        check_dependencies
        verify_build
        ;;
    size)
        check_dependencies
        show_size
        ;;
    ports)
        list_ports
        ;;
    help)
        show_usage
        ;;
    *)
        print_error "Unknown action: $ACTION"
        show_usage
        exit 1
        ;;
esac