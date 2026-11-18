#!/bin/bash
#
# Setup script for Arduino UART persistent device name
# This creates a udev rule to map the CH340 device to /dev/arduino_uart
#

set -e  # Exit on error

echo "========================================="
echo "Arduino UART Device Name Setup"
echo "========================================="

# Check if running as root
if [ "$EUID" -ne 0 ]; then 
    echo "Please run as root or with sudo"
    exit 1
fi

# Define the udev rule file
UDEV_RULE_FILE="/etc/udev/rules.d/99-arduino-uart.rules"

echo ""
echo "Creating udev rule at: $UDEV_RULE_FILE"

# Create the udev rule
# This rule matches the CH340 USB-to-serial converter
# idVendor=1a86, idProduct=7523
cat > "$UDEV_RULE_FILE" << 'EOF'
# Arduino UART - CH340 Serial Converter
# QinHeng Electronics CH340 (idVendor=1a86, idProduct=7523)
#
# Creates symlink: /dev/arduino_uart -> /dev/ttyUSBx
# Also creates: /dev/serial/by-id/arduino_uart

# Match by Vendor ID and Product ID
SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", SYMLINK+="arduino_uart", MODE="0666", GROUP="dialout"

# Alternative: Match by serial number if you have multiple CH340 devices
# Uncomment and replace SERIAL_NUMBER with actual serial number from: udevadm info -a -n /dev/ttyUSB1 | grep ATTRS{serial}
# Note: Many CH340 chips don't have unique serial numbers, so this might not work
# SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", ATTRS{serial}=="SERIAL_NUMBER", SYMLINK+="arduino_uart", MODE="0666", GROUP="dialout"

# Alternative: Match by physical USB port location (most reliable if devices stay in same ports)
# Uncomment and replace KERNELS value with actual value from: udevadm info -a -n /dev/ttyUSB1 | grep KERNELS
# SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", KERNELS=="1-1.1", SYMLINK+="arduino_uart", MODE="0666", GROUP="dialout"
EOF

echo "✓ Udev rule created successfully"

# Reload udev rules
echo ""
echo "Reloading udev rules..."
udevadm control --reload-rules
udevadm trigger --subsystem-match=tty

echo "✓ Udev rules reloaded"

# Display the created rule
echo ""
echo "========================================="
echo "Created udev rule:"
echo "========================================="
cat "$UDEV_RULE_FILE"

echo ""
echo "========================================="
echo "Setup Complete!"
echo "========================================="
echo ""
echo "Next steps:"
echo "1. Unplug the Arduino USB cable"
echo "2. Wait 2 seconds"
echo "3. Plug it back in"
echo "4. Check with: ls -la /dev/arduino_uart"
echo ""
echo "The device will be available at: /dev/arduino_uart"
echo ""
echo "To verify which physical device it is:"
echo "  udevadm info /dev/arduino_uart | grep ID_MODEL"
echo ""
echo "To remove this rule in the future:"
echo "  sudo rm $UDEV_RULE_FILE"
echo "  sudo udevadm control --reload-rules"
echo ""