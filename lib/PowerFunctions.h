// Calculate the set power both for charger and inverter.
int CalculatePower(int ActGridPower, int ActBatPower, int PowResCharger, int MaxPowCharger, int PowResInv, int MaxPowerInv);

// Get actual power from the central power meter.
int getActualPower(char ip[40], char cmd[40], char resp[20], char resp_power[20]);

// calculate max inverter load to have a balanced discharge of the battery over the whole night
int CalculateBalancedDischargePower(int capacity, float voltage, int actualSOC, int targetSOC, float sunset, float sunrise);

// get solar prognosis
float getSolarPrognosis(char token[40], char id[4], char today[9], char tomorrow[9]);

// calculate the charging current based on the max cell voltage
float CalculateChargingCurrent (int ChargerPower, float ChargerVoltage, float maxCellVoltage, int BatteryTemperature, int BattCapacity, int TimerAccumulation);

/* This Library contols the power plugs for the inverter and the carger.
The code controls TASMOTA Smart pugs 
Version: TASMOTA 12.1.0
Conditions: there must be an defined Wifi connection with the name : espclient

Charger:
  IP: 192.168.178.65
Inverter
  IP: 192.168.178.68

  Functions:
    setplug (plugId,onoff)
    plugId    : IP address as string
    onoff: Boolean; on high, off low

GetSmartPlugData(plugId, topic)
plugId: : IP address as string
topic: 
1: Voltage          (V)
2: current          (A)
3: active Power     (W)
4: Energy today     (kWh)
5: Energy yesterday (kWh)
6: Energy Total     (kWh)
7: Status on or off
User Manual TASMOTA
https://tasmota.github.io/docs/Commands/#with-mqtt
https://ubwh.com.au/documents/UBWH-SP-MON_QSG.pdf

Example how to connect to client
  http.begin(espclient, "http://192.168.188.127/cm?cmnd=status+10");
  http.begin(espclient, "http://"+String(current_clamp_ip)+String(current_clamp_cmd));
HTTP Commands to turn on and off:
Inverter
http://192.168.178.68/cm?cmnd=Power%20On
Charger
http://192.168.178.65/cm?cmnd=Power%20off
*/
void setplug (char plugIp, bool onoff);

/* Function to get Plug values
GetSmartPlugData(plugId, topic)
plugId: : IP address as string
topic: 
1: Voltage          (V)
2: current          (A)
3: active Power     (W)
4: Energy today     (kWh)
5: Energy yesterday (kWh)
6: Energy Total     (kWh)
*/
float GetSmartPlugData(char plugId, short topic);

