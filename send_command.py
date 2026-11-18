import serial
import time

# Open serial port
ser = serial.Serial('/dev/ttyUSB0', 115200, timeout=1)

# Give the device a moment to reset (optional but common for MCU boards)
time.sleep(2)

# Text to send
command = "SET 0 0 0 0\n"   # add newline if your device expects it

# Send it
ser.write(command.encode(encoding='UTF-8'))

print("Command sent!")

# Close port
ser.close()
