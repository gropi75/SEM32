# Solar Energy Manager for ESP32 (SEM.32)
![GitHub Repo stars](https://img.shields.io/github/stars/gropi75/ESP32_r4850g2?style=plastic)
![GitHub watchers](https://img.shields.io/github/watchers/gropi75/ESP32_r4850g2?style=plastic)
![GitHub forks](https://img.shields.io/github/forks/gropi75/ESP32_r4850g2?style=plastic)
![Release](https://img.shields.io/github/v/release/gropi75/ESP32_r4850g2?include_prereleases)

SEM.32 (Solar Energy Manager 32) based on ESP32 controller, Soyosource Inverter, Huawei r4850g2 power supply, JK-BMS and 48V battery. It helps You to maximize the usage of self generated solar power.



# Main features:
- [x] control of inverter to minimize power usage out of the grid (over RS485)
- [x] control of charger to maximize own usage of solar power (over CAN-bus)
- [x] adjustable power thresholds
- [x] optional manual control of charging power
- [x] access to JK-BMS data (over RS485)
- [ ] web GUI
- [x] wifi configuration portal
- [x] MQTT client
- [x] scalable code for new features
- [ ] CAN dbc file for the charger
- [x] runs ESP32 boards with RS485+CAN shield
- [ ] in prepareation to run on the [BSC-Hardware](https://github.com/shining-man/bsc_hw)

## Web GUI

![Summary page](https://github.com/gropi75/ESP32_r4850g2/blob/main/components/Screenshot_GUI_Status.jpg)

![Info page](https://github.com/gropi75/ESP32_r4850g2/blob/main/components/Screenshot_GUI_Info.jpg)

![Settings page](https://github.com/gropi75/ESP32_r4850g2/blob/main/components/Screenshot_GUI_Settings.jpg)


## Hardware components:

- **Inverter:**
  - Type: GNT 1200LIM 48
  - MAX: Const. Power: 800W (1200W solar operation)
- **Charger:**
  - Type: Huawei R4850G2
- **BMS:**
  - Type: JK-BD6A20S8P
  - Software V.: V10.XY
- **Battery cells:**
  - Type: EVE LF90 (3,2V 90AH, Nominal)
  - Datasheet No.: LF90-73103
  - Cut off Voltage: 3.65V/2.5V
  - Std. charge/Discharge: 1.0C
- **Power Meter:**
  - Type: Tasmota based smart meter from [Hitchi](https://www.photovoltaikforum.com/thread/173032-lesekopf-bei-heise-getestet/)


- **Controller:**
  - [ESPRESSIF ESP32-WROOM-32U DEVKITC(with ext. Antenna)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/esp32/get-started-devkitc.html)
- **CAN interface:**
  - Driver IC: SN65HVD230
- **RS485 Serial interface:**
  - Driver IC: Max485 CSA (2 pcs. for BMS and Inverter)
- **0.96 inch OLED IIC 128x64 Display:**
  - Driver: SSD1306
  - I2C Addr.: 0x3C
- **Temperature Sensors:**
  - 1-Wire
  - Type: DS18B20 waterproof

## External connectors:

**USB upper:**

Pin 2 (RXD/D-) Output A Max 485 USB Driver for inverter, Inverter Input A+

Pin 3 (TXD/D+) Output B Max 485 USB Driver for inverter, Inverter Input B-

**USB Lower:**

Pin 2 (RXD/D-) CAN L

Pin 3 (TXD/D+) CAN H

**RJ45:**

Pin 1 (Pair 3) Output A Max 485 USB Driver for BMS, BMS Input A+

Pin 2 (Pair 3) Output B Max 485 USB Driver for BMS, BMS Input B-

**6 pin connector:**

Pin1 GPIO26 Temp Sensor

Pin2 Ground

Pin3 +3V3

Pin4 NC

Pin5 NC

Pin6 NC

## Pinning Controller

| GPIO   | Function  | Application           | Driver                    |
| :----- | :-------- | :-------------------- | :------------------------ |
| GPIO00 | Boot      | Push Burron 1         |                           |
| GPIO01 |           |                       |                           |
| GPIO02 |           |                       |                           |
| GPIO03 |           |                       |                           |
| GPIO04 | CAN RX    | Charger               | SN65HVD230                |
| GPIO05 | CAN TX    | Charger               | SN65HVD230                |
| GPIO06 |           |                       |                           |
| GPIO07 |           |                       |                           |
| GPIO08 |           |                       |                           |
| GPIO09 |           |                       |                           |
| GPIO10 |           |                       |                           |
| GPIO11 |           |                       |                           |
| GPIO12 | LED       | Blue, Charging        |                           |
| GPIO13 | LED       | Green, Discharging    |                           |
| GPIO14 | LED       | Yellow, Standby       |                           |
| GPIO15 | LED       | Red, ERROR            |                           |
| GPIO16 | U2RXD     | Inverter              | Max485 CSA                |
| GPIO17 | U2TxD     | Inverter              | Max485 CSA                |
| GPIO18 | RS485_EN  | Chip enable, Inverter | Max485 CSA                |
| GPIO19 |           |                       |                           |
| GPIO21 | I2C SDA1  | Display SSD1306       | Address: 0x3C             |
| GPIO22 | I2C SLC1  | Display SSD1306       | Address: 0x3C             |
| GPIO23 |           |                       |                           |
| GPIO25 |           |                       |                           |
| GPIO26 | DAT       | Temp. Sensors         | DS18B20; inc. 4K7 pull up |
| GPIO27 |           |                       |                           |
| GPIO32 | RS485 RxD | BMS                   | Max485 CSA                |
| GPIO33 | RS485 TxD | BMS                   | Max485 CSA                |
| GPIO34 | RS485_EN  | BMS                   | Max485 CSA                |
| GPIO35 | Dig_input | Push Button 3         |                           |
| GPIO36 | Dig_input | Push Button 2         |                           |
| GPIO39 | LED       | auxiliary             |                           |

# Information:

Power Equation for the whole house: $\ ActGridPower + ActBatPower + SolProd = Used Power$

JSON help: https://arduinojson.org/v6/assistant/#/step1

ESP32 infos: https://www.upesy.com/blogs/tutorials/how-to-connect-wifi-acces-point-with-esp32

BSC (Battery Safety Controller) homepage: https://github.com/shining-man/bsc_fw


# Known issues:

Solution for Littlefs error: `pio pkg update -g -p espressif32`

