# ManarHarake-sEMGDevice Firmware flashing Guide

## Prerequisites 
Install J-Link tools

Download J-Link tools from [here](https://www.segger.com/downloads/jlink/)
We used v7.94e. Download the latest from this link for the tools [here](https://www.segger.com/downloads/jlink/JLink_Windows_x86_64.exe)

Follow the installation guide and install.

<p align="center">
    <img src="https://github.com/eSpark-Consultants/raafat-BLEproduct/assets/143803999/a6c424dc-f132-4e7d-9920-9985fdb77baf"  height="300"/>
</p>


## 1 ATMEL SAM4s
### 1.1 Setting up the Hardware
The hardware mainly the control board expected to power up over 8.4V (Two cell LiPo / Li-Ion battery). However, due to slight issues with the battery supervisor circuit, the Voltage Divider network expected to bring voltages to safe range had to be removed. **The board is now powered up over 4V for safe operations**.

####  1.1.1 Tools required
1. Power Supply (5V / 1A)
2. JLink Debugger / Programmer
3. USB to USB Type C cable (For serial debugging)


####  1.1.2 Software required
1. [Microchip Studio](https://www.microchip.com/en-us/tools-resources/develop/microchip-studio#Downloads)

#### 1.1.3 Step by Step configuration
1. Power up the supply and set it to 4V.
2. Connect the power supply cables to the power input pins on the PCB as can be seen in the screenshot below. Make sure the polarity is correct after observing the -ve and +ve pin identifiers
<p align="center" width="100%">
<img src="https://user-images.githubusercontent.com/76909080/195995410-549fbed7-de78-469c-aeab-4af878973c96.png"/>
</p>
3. Connect the debugger with the board as per the table below. The debugger pin snapshot as well as the PCB pin names are given in screenshots below:
<div align="center" width="100%">

|Board Pin Name|Debugger Pin Name|
|----|----|
|VCC_3V3|VTRef|
|TDI|Do not connect|
|TDO|Do not connect|
|TMS|SWDIO|
|TCK|SWCLK|
|NRST|nRESET|
|ERASE|Do not connect|
|GND|GND|
</div>
<p align="center" width="100%">
<img src="https://user-images.githubusercontent.com/76909080/195996062-b666f94c-5eff-4f73-bf63-309af3ffa576.png"/><img src="https://user-images.githubusercontent.com/76909080/195996295-b1c436bd-0b0d-452f-8828-67160a2047d9.png"/>
</p>

4. Once the connection are properly made, proceed to launch Microchip Studio and verify device connection with debugger using the **Device Programming** option in the software or press **CTRL + SHIFT + P**.

</div>
<p align="center" width="80%">
<img src="https://github.com/eSpark-Consultants/ManarHarake-sEMGDevice/assets/143803999/e3c499a6-006e-4eca-8c52-337c576bf4fb" width = "600" />
</p>

5. After successful connection between hardware and Microchip, proceed to upload the provided .hex file to the board from the **Memories** option of the **Device Programming** window.

## 2 NRF52810 
### 2.1 Setting up the Hardware
The hardware mainly the control board expected to power up over 8.4V (Two cell LiPo / Li-Ion battery). However, due to slight issues with the battery supervisor circuit, the Voltage Divider network expected to bring voltages to safe range had to be removed. **The board is now powered up with 4V for safe operations**.

####  2.1.1 Tools required
1. Power Supply (5V / 1A)
2. JLink Debugger / Programmer
3. USB to USB Type C cable (For serial debugging)

####  2.1.2 Software required
1. [NRF Connect Desktop](https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-Desktop)
2. [NRF Connect](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp&hl=en&gl=US&pli=1) (To connect with BLE device)

#### 2.1.3 Step by Step configuration
1. Power up the supply and set it to 4V.
2. Connect the power supply cables to the power input pins on the PCB as can be seen in the screenshot below. Make sure the polarity is correct after observing the -ve and +ve pin identifiers
<p align="center" width="100%">
<img src="https://user-images.githubusercontent.com/76909080/195995410-549fbed7-de78-469c-aeab-4af878973c96.png"/>
</p>
3. Connect the debugger with the board as per the table below. The debugger pin snapshot as well as the PCB pin names are given in screenshots below:
<div align="center" width="100%">

|Board Pin Name|Debugger Pin Name|
|----|----|
|VCC_3V3|VTRef|
|SWDIO|SWDIO|
|SWCLK|SWCLK|
|RST_NRF|nRESET|
|GND|GND|
</div>
<p align="center" width="400">
<img src="https://user-images.githubusercontent.com/76909080/195996062-b666f94c-5eff-4f73-bf63-309af3ffa576.png"/><br>

<img src="https://github.com/eSpark-Consultants/ManarHarake-sEMGDevice/assets/143803999/e4995451-d11e-4dc7-b677-f1a327353996" width = "300"/>
</p>

4. Once the connection are properly made, proceed to launch **nRF Connect for Desktop**. Select **Programmer** app from the apps list by clicking **Open**, Programmer window will appear. If its not available press **Install**.

<p align="center" width="80%">
<img src="https://github.com/eSpark-Consultants/ManarHarake-sEMGDevice/assets/143803999/23bfe67c-0dbe-4790-8614-79c6c961b25d" width = "600" />
</p>

5. Select Device from top left option **Select Device** and connect your debugger. **Add File** and select provided .hex file to flash on NRF. Click on **Erase & Write** to flash the binaries.

<p align="center" width="80%">
<img src="https://github.com/eSpark-Consultants/ManarHarake-sEMGDevice/assets/143803999/08fe4dab-733c-43ee-b735-05af7f059d69" width = "600" />
</p>

6. Open NRF Connect application on mobile and then Connect to **Nordic_UART_Service**, request MTU of 512 to receive larger data packets and lastly Enable Notifications.

<p align="center" width="80%">
<img src="https://github.com/eSpark-Consultants/ManarHarake-sEMGDevice/assets/143803999/93b1acd2-ef7b-40b2-9524-849af836a4e3" width = "200" />
<img src="https://github.com/eSpark-Consultants/ManarHarake-sEMGDevice/assets/143803999/dcb7669d-4f9c-4ba5-ad80-fbdabe7fc282" width = "200" />
<img src="https://github.com/eSpark-Consultants/ManarHarake-sEMGDevice/assets/143803999/46cab39d-f7b2-4d3b-be63-d9914f793960" width = "200" />
</p>



