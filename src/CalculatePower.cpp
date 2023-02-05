#include <Arduino.h>

// NOT FINISHED!!!!!
// Positive ActPower means, we import power, so we need the inverter
// Negative ActPower means, we export power, so we can charge the batter
// Calculate the set power both for charger and inverter.
int CalculatePower(int ActGridPower, int ActBatPower, int PowResCharger, int MaxPowCharger, int PowResInv, int MaxPowerInv)
{
    int SetPower;
    PowResCharger = -PowResCharger;

    if (ActBatPower < 0) {                                          // if we charge the battery

        if (ActGridPower < (PowResCharger) ) {                        // and we still have sufficient solar power
           SetPower = ActBatPower  - int((PowResCharger - ActGridPower) / 3);   // than increase the charging power
        }
        else if (ActGridPower <= (PowResCharger/2)) {                // in the range of the reserve do not change
            SetPower = ActBatPower;
        }
        else if (ActGridPower > (PowResCharger/2)) {                // if there if no reserve, than reduce the charging power fast
            SetPower = ActBatPower + int(( ActGridPower - PowResCharger  ) / 2);

        }

    }
    else if (ActBatPower >= 0 ) {                                    // if we discharge the battery

        if (ActGridPower < (PowResInv /2) ) {                        // and we get close to 0 imported power
           SetPower = ActBatPower  - int((PowResInv - ActGridPower) / 2);   // than decrease the charging power fast
        }
        else if (ActGridPower <= (PowResInv)) {                // in the range of the reserve do not change
            SetPower = ActBatPower;
        }
        else if (ActGridPower > (PowResInv)) {                // if we are above the reseerve, than increase the discharging power
            SetPower = ActBatPower + int(( ActGridPower - PowResInv ) / 3);
        }

    }

        SetPower = constrain(SetPower, -MaxPowCharger, MaxPowerInv); // limit the range of control power to 0 and maxPower
        return SetPower;
}
