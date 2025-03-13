# Project Compilation Guide

## Requirements

### Software
- **Arduino IDE 2.3.4** (Ensure all latest updates are installed)
- **ESP32 Board Package** (Use "ESP32 Dev Module" as the board)

### Libraries
1. Install the **NimBLE Library** developed by `h2zero` from the Arduino Library Manager.
2. Include the following libraries from this project (modified versions) and install these using add .zip library in arduino IDE:
   - `ESP32 BLE Mouse`
   - `ESP32 BLE Keyboard`
   - `ESP32 BLE Gamepad`

## Compilation Steps
1. Open **Arduino IDE 2.3.4**.
2. Install the required libraries as mentioned above.
3. Select **ESP32 Dev Module** as the board.
4. Include the modified libraries from this project.
5. Compile and upload the project to your ESP32 board.

## Notes
- Ensure that all dependencies are correctly installed before compilation.
- If you encounter any issues, verify that you are using the latest versions of all required libraries and board definitions.

