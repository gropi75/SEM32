# ESP32_r4850g2
ESP32 project to control the Huawei R4850G2 Power Supply for a solar powered LIFEPO4 48V battery pack


Information: 

Power Equation for the whole house:         ActGridPower + ActBatPower + SolProd = Used Power

Hardware components:

Inverter:  
    Type:                   GNT 1200LIM 48 
    MAX: Const. Power:      900W
Charger:    
    Type:                   Huawei R4850G2
BMS: 
    Type:                   JK-BD6A20S8P
    Software V.:            V10.XY
    Serial No:              2032812241
Battary cells:
    Type:                   EVE LF90 (3,2V 90AH, Nominal)
    Datasheet No.:          LF90-73103
    Cut off Voltage:        3.65V/2.5V
    Std. charge/Discharge:  1.0C


Controller:                 ESPRESSIF ESP32-WROM-32U (with ext. Antenna)
CAN Driver:                 
    Driver IC:              SN65HVD230
Serial Driver:              
    Driver IC:              Max485 CSA (2 pcs. for BMS and Inverter)
Display: 
    Driver:                 SSD1306
    Description:            4 Pin 0.96 Zoll OLED IIC Display Modul 128x64; 
    I2C Addr.:              0x3C

Connectors:
USB upper: 
 Pin 2 (RXD/D-)             Output A Max 485 USB Driver for inverter, Inverter Input A+
 Pin 3 (TXD/D+)             Output B Max 485 USB Driver for inverter, Inverter Input B-

USB Lower:
    Pin 2 (RXD/D-)          CAN L
    Pin 3 (TXD/D+)          CAN H

RJ45
    Pin 1 (Pair 3)          Output A Max 485 USB Driver for BMS, BMS Input A+
    Pin 2 (Pair 3)          Output B Max 485 USB Driver for BMS, BMS Input B-

Pinleiste 6 Pol
    Pin1                    GPIO26 Temp Sensor
    Pin2                    Ground
    Pin3                    +3V3
    Pin4                    NC
    Pin5                    NC
    Pin6                    NC


Pinning Controller
GPIO        Function        Application             Driver
GPIO00      Boot            Push Burron 1

GPIO01
GPIO02
GPIO03
GPIO04      CAN HI          Charger                 SN65HVD230
GPIO05      CAN LO          Charger                 SN65HVD230
GPIO06
GPIO07
GPIO08
GPIO09
GPIO10
GPIO11
GPIO12      LED             Blue, Charging
GPIO13      LED             Green, Discharging
GPIO14      LED             Yellow, Standby
GPIO15      LED             Red, ERROR
GPIO16      U2RXD           Inverter                Max485 CSA
GPIO17      U2TxD           Inverter                Max485 CSA
GPIO18      RS485_EN        Chip enable, Inverter   Max485 CSA
GPIO19
GPIO21      I2C SDA1        Display  SSD1306        Address: 0x3C
GPIO22      I2C SLC1        Display SSD1306         Address: 0x3C
GPIO23
GPIO25
GPIO26      DAT             Temp. Sensors           DS18B20; inc. 4K7 pull up
GPIO27
GPIO32      RS485 RxD       BMS                     Max485 CSA
GPIO33      RS485 TxD       BMS                     Max485 CSA
GPIO34      RS485_EN        BMS                     Max485 CSA
GPIO35      Dig_input       Push Button 3
GPIO36      Dig_ input      Push Button 2
GPIO39      LED             auxiliary 







Function: CalculatePower.cpp\int CalculatePower(int ActGridPower, int ActBatPower, int PowResCharger, int MaxPowCharger, int PowResInv, int MaxPowerInv)

Positive ActGridPower means, we import power, so we should discharge the battery with the inverter.
Negative ActGridPower means, we export power, so we can charge the battery.

Same logic applies to the battery:
Positive ActBatPower means, we dischcharge the battery.
Negative ActBatPower means, we charge the battery.

Cases:
1) we already charge the battery and still have sufficient solar-power (ActGridPower is negative / less than PowerResCharger)

if ActBatPower is negative
    and if ActGridPower is less than PowerResCharger
        than decrease ActBatPower normally (0.3x)

2) we charge the battery but do not have sufficient solar power any more (ActGridPower is positive / bigger than PowResCharger). We have to reduce the charging power fast.

if ActBatPower is negative
    and if ActGridPower is more than PowerResCharger
        than increase ActBatPower fast (0.5x)


3) we discharge the battery and we export energy means, we have to reduce fast the inverter power (ActGridPower is negative/ less than PowResInv)

if ActBatPower is positive
    and if ActGridPower is negative
        than reduce ActBatPower fast (0.5x)


4) we discharge the battery and we import energy means, we can increase the inverter power (ActGridPower is positive, higher than PowResInv)

if ActBatPower is positive
    and if ActGridPower is positive
        than increase ActBatPower normally (0.3x)

Finally we have to limit the output power, not to exceed the capabilities of the system.
(-MaXPowerCharger)  <    ActBatPower     <  MaxPowerInv



Information: 

Power Equation for the whole house:         ActGridPower + ActBatPower + SolProd = Used Power
