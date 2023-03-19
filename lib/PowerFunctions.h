// Calculate the set power both for charger and inverter.
int CalculatePower(int ActGridPower, int ActBatPower, int PowResCharger, int MaxPowCharger, int PowResInv, int MaxPowerInv);

// Get actual power from the central power meter.
int getActualPower(char ip[40], char cmd[40], char resp[20], char resp_power[20]);

// calculate max inverter load to have a balanced discharge of the battery over the whole night
int CalculateBalancedDischargePower(int capacity, float voltage, int actualSOC, int targetSOC, float sunset, float sunrise);

// get solar prognosis
float getSolarPrognosis(char token[40], char id[4], char today[9], char tomorrow[9]);

