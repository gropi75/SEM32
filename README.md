# ESP32_r4850g2
ESP32 project to control the Huawei R4850G2 Power Supply for a solar powered LIFEPO4 48V battery pack


 
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
