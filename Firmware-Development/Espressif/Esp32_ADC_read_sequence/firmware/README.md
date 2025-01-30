# ESP32 ADC Reader

## Overview

This ESP32 project reads ADC values from various ADC1 and ADC2 channels and outputs the data in a format that is compatible with the Arduino Serial Plotter. The readings are continuously printed in real-time, allowing easy visualization of ADC behavior.

### Features:
- Reads from multiple ADC channels on ESP32.
- Prints ADC readings in a format compatible with the Arduino Serial Plotter.
- Supports ADC1 and ADC2 channels for flexible configuration.
- Binaries provided for easy testing and flashing to the ESP32.

## Hardware Setup

The following GPIO pins on the ESP32 are used for ADC readings:

- **ADC1 Channels:**
  - Channel 0 @ GPIO 36
  - Channel 5 @ GPIO 33
  - Channel 4 @ GPIO 32
  - Channel 7 @ GPIO 35
  - Channel 6 @ GPIO 34
  - Channel 3 @ GPIO 39
  
- **ADC2 Channels:**
  - Channel 9 @ GPIO 26
  - Channel 8 @ GPIO 25

### Wiring

- Connect the sensors or voltage sources to the specified GPIO pins (e.g., GPIO 36 for ADC1 channel 0, GPIO 33 for ADC1 channel 5, etc.).
- Ensure that the voltage levels applied to these pins are within the ESP32's ADC input range (0V to 3.3V).

## Flashing the ESP32

### Binary Files
To test the project, you can flash the ESP32 using the provided binary files. The files should be placed at the following addresses:

- **Bootloader**: `bootloader.bin` → Flash at address `0x1000`
- **Firmware**: `firmware.bin` → Flash at address `0x10000`
- **Partition Table**: `partition-table.bin` → Flash at address `0x8000`

These binaries can be flashed using the following command (using `esptool.py`):

```bash
esptool.py --port /dev/ttyUSB0 write_flash 0x1000 bootloader.bin 0x10000 firmware.bin 0x8000 partition-table.bin
