# UART Gesture-Controlled Modes Firmware

This project implements a firmware that receives gestures via UART, processes them, and executes commands based on the active mode. The supported modes are:

1. **Palm PCB Control**
2. **Mouse Control** (currently not Implemented)
3. **Gamepad Control** (currently not Implemented)
4. **VR Control** (currently not Implemented)

Each mode responds to six gesture commands (`1-6`) received from UART and performs specific actions accordingly.

---

## Project Features
- Handles four different control modes using an interrupt-driven mode switch mechanism.
- Supports debounced mode switching with an external button.
- Processes six distinct gesture commands (`1-6`).
- Executes unique actions per mode:
  - Palm commands (e.g., `G1`, `G2`).

---

## Setup

### 1. **Hardware Connections**
- **UART Connections**:
  - UART0 (`MANAR_UART_RX` `RX2 D16` and `MANAR_UART_TX` `TX2 D17`): Connect this to your UART tool or device for sending and receiving commands.
  - Make sure to common the ground connection.
- **Button for Mode Switching**:
  - Connect a momentary push button to the GPIO pin defined for `MODE_SWITCH_PIN` in the firmware (default: `GPIO_NUM_23`).
- **LED (Optional)**:
  - Connect an LED to `GPIO_NUM_2` (ESP32 BUILTIN LED) defined in the firmware to visualize mode switching (e.g., blinking interval 200ms).

---

## Hardware Requirements
- ESP32 Development Board.
- Push button for mode switching.
- LED (optional, for visual feedback).
- UART communication device (e.g., Palm PCB).

---

## How to Test

### 1. **Flash the Firmware**

To test the project, you can flash the ESP32 using the provided binary files. The files should be placed at the following addresses:
Flash Commands
- **Bootloader**: `bootloader.bin` → Flash at address `0x1000`
- **Firmware**: `palm_pcb_control.bin` → Flash at address `0x10000`
- **Partition Table**: `partition-table.bin` → Flash at address `0x8000`

These binaries can be flashed using the following command (using `esptool.py`):

```bash
esptool.py --port /dev/ttyUSB0 write_flash 0x1000 bootloader.bin 0x10000 palm_pcb_control.bin 0x8000 partition-table.bin
```

- Power the ESP32 and monitor logs via UART.
- Use a UART terminal program (e.g., `CoolTerm`) to send commands to the ESP32.
- Set UART terminal program baudrate to 38400

### 2. **Send Gesture Commands**
- Commands `1`, `2`, `3`, `4`, `5`, and `6` represent gesture inputs.
- Send these commands over UART (configured with `38400 baud` rate) to test different responses for the current mode.

### 3. **Switch Modes**
- Button GPIO 23
- Press the mode-switching button to change the mode.
- Interrupt on Negative Edge.
- Debounce Period 200ms
- The mode order is:
  ```
  MODE_PALM -> MODE_MOUSE -> MODE_GAMEPAD -> MODE_VR -> MODE_PALM (loop)
  ```
- Debounce ensures that a single button press doesn’t cause multiple mode switches.

---

## Mode Details and Behavior

### **Palm PCB Control Mode**
- Gesture commands (`1-6`) are mapped to `G1` to `G6`.
- Each command is forwarded to another UART (`MANAR_UART_TX`).
- Example:
  - Input: `1`
  - Output: `G1` written to UART.

### **Mouse Control Mode**
(currently not Implemented)

### **Gamepad Control Mode**
(currently not Implemented)

### **VR Control Mode**
(currently not Implemented)

