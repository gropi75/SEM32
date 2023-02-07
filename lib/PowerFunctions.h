// Calculate the set power both for charger and inverter.
int CalculatePower(int ActGridPower, int ActBatPower, int PowResCharger, int MaxPowCharger, int PowResInv, int MaxPowerInv);

// Get actual power from the central power meter.
int getActualPower(char ip[40], char cmd[40], char resp[20], char resp_power[20]);
