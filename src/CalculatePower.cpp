#include <Arduino.h>

// NOT FINISHED!!!!!
// Positive ActPower means, we import power, so we need the inverter
// Negative ActPower means, we export power, so we can charge the batter
// Calculate the set power both for charger and inverter.
int CalculatePower(int ActGridPower, int ActBatPower, int PowResCharger, int MaxPowCharger, int PowResInv, int MaxPowerInv)
{
        int SetPower;

        if ((PowResCharger + ActGridPower) < 0)
        { // goal is to have a reserve of minPower
            SetPower = ActBatPower - int((PowResCharger + ActGridPower) / 3);
        }
        else if (((PowResCharger / 2) + ActGridPower) < 0)
        { // however till minPower/2 the set value is not adjusted
            SetPower = ActBatPower;
        }
        else
        { // if we come under minPower/2, than we
            SetPower = ActGridPower - int((PowResCharger + ActGridPower) / 2);
        }

        SetPower = constrain(SetPower, 0, MaxPowCharger); // limit the range of control power to 0 and maxPower
        //return SetPower;
        return 0;
}
