#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

WiFiClient current_clamp_client; // or WiFiClientSecure for HTTPS; to connect to current sensor
HTTPClient http;                 // to connect to current sensor

WiFiClient solarprognose_client; // to connect to Solarprognose.de
HTTPClient http_solar;

unsigned long g_AccumuationTimer = 0;

// Positive ActPower means, we import power, so we need the inverter
// Negative ActPower means, we export power, so we can charge the batter
// Calculate the set power both for charger and inverter.
int CalculatePower(int ActGridPower, int ActBatPower, int PowResCharger, int MaxPowCharger, int PowResInv, int MaxPowerInv)
{
  int SetPower;
  PowResCharger = -PowResCharger;
  if (ActBatPower < 0)
  { // if we charge the battery
    if (ActGridPower < (PowResCharger))
    {                                                                   // and we still have sufficient solar power
      SetPower = ActBatPower - int((PowResCharger - ActGridPower) / 3); // than increase the charging power
    }
    else if (ActGridPower <= (PowResCharger / 2))
    { // in the range of the reserve do not change
      SetPower = ActBatPower;
    }
    else if (ActGridPower > (PowResCharger / 2))
    { // if there if no reserve, than reduce the charging power fast
      SetPower = ActBatPower + int((ActGridPower - PowResCharger) / 2);
    }
  }
  else if (ActBatPower >= 0)
  { // if we discharge the battery
    if (ActGridPower < (PowResInv / 2))
    {                                                               // and we get close to 0 imported power
      SetPower = ActBatPower - int((PowResInv - ActGridPower) / 2); // than decrease the charging power fast
    }
    else if (ActGridPower <= (PowResInv))
    { // in the range of the reserve do not change
      SetPower = ActBatPower;
    }
    else if (ActGridPower > (PowResInv))
    { // if we are above the reseerve, than increase the discharging power
      SetPower = ActBatPower + int((ActGridPower - PowResInv) / 3);
    }
  }
  SetPower = constrain(SetPower, -MaxPowCharger, MaxPowerInv); // limit the range of control power to 0 and maxPower
  return SetPower;
}

// Get actual power from the grid power meter.
int getActualPower(char ip[40], char cmd[40], char resp[20], char resp_power[20])
{
  int actPower;
  http.begin(current_clamp_client, "http://" + String(ip) + String(cmd));
  int httpCode = http.GET(); // send the request
  if (httpCode > 0)
  {                                    // check the returning code
    String payload = http.getString(); // Get the request response payload
    // Stream input;
    // {"StatusSNS":{"Time":"2023-01-16T22:39:48","SML":{"DJ_TPWRIN":4831.6445,"DJ_TPWROUT":9007.7471,"DJ_TPWRCURR":200}}}
    // {"StatusSNS":{"Time":"2022-07-23T13:45:16","MT175":{"E_in":15503.5,"E_out":27890.3,"P":-4065.00,"L1":-1336.00,"L2":-1462.00,"L3":-1271.00,"Server_ID":"0649534b010e1f662b01"}}}

    StaticJsonDocument<400> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      // return NULL;
      return 0;
    }
    JsonObject StatusSNS_SML = doc["StatusSNS"][String(resp)];
    actPower = StatusSNS_SML[String(resp_power)]; // -2546
  }
  return actPower;
}

// calculate max inverter load to have a balanced discharge of the battery over the whole night
int CalculateBalancedDischargePower(int capacity, float voltage, int actualSOC, int targetSOC, float sunset, float sunrise)
{
  // shall we use https://github.com/buelowp/sunset
  if (actualSOC < targetSOC)
  {
    return 0;
  }
  else return (int)(capacity * 0.01 * (actualSOC - targetSOC) / (sunrise - sunset + 24));
}

// get solar prognosis
// http://www.solarprognose.de/web/solarprediction/api/v1?access-token=454jelfd&item=inverter&id=2&type=daily
float getSolarPrognosis(char token[40], char id[4], char today[9], char tomorrow[9])
{
  float prognosis_today, prognosis_tomorrow;
  char solarprognose_adress[83] = "http://www.solarprognose.de/web/solarprediction/api/v1?_format=json&access-token=";
  http_solar.begin(solarprognose_client, solarprognose_adress + String(token) + "&item=location&id=" + String(id) + "&type=daily");
  int httpCode = http_solar.GET(); // send the request
  if (httpCode > 0)
  {
    String payload = http_solar.getString(); // Get the request response payload
    StaticJsonDocument<768> doc;
    Serial.println(payload);
    DeserializationError error = deserializeJson(doc, payload);

    if (error)
    {
      Serial.print("deserializeJson() failed: ");
      Serial.println(error.c_str());
      return 0;
    }
    prognosis_today = doc["data"][String(today)];       // 9.322
    prognosis_tomorrow = doc["data"][String(tomorrow)]; // 20.653
    Serial.println(String(today));
    Serial.println(String(prognosis_today));
    Serial.println(String(tomorrow));
    Serial.println(String(prognosis_tomorrow));
  }
  return prognosis_tomorrow;
}

// calculate the charging current based on the max cell voltage
float CalculateChargingCurrent(int ChargerPower, float ChargerVoltage, float maxCellVoltage, int BatteryTemperature, int BattCapacity, int TimerAccumulation)
{
  float retvalue = 0;
  if (BatteryTemperature > 45)
  {
    retvalue = 0;
  }
  else
  {
    if (maxCellVoltage >= 3.5)                                                    // accumulation stage?
    {
      if ((millis() - g_AccumuationTimer) < 3600000)                                // limit this stage to 60 minutes
      {                                                                            
        retvalue = constrain(ChargerPower / ChargerVoltage, 0, BattCapacity / 10); // limit the charging current to 0.1C
      }
      else
        retvalue = 0;
    }
    else
    {
      retvalue = constrain(ChargerPower / ChargerVoltage, 0, BattCapacity / 2); // limit the charging current to 0.5C
      g_AccumuationTimer = millis();
    }
  }
  return retvalue;
}

// ______________________________________________________________________Smart Plugs
/* This Library controls the power plugs for the inverter and the carger.
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

HTTP Commands:
Inverter
http://192.168.178.68/cm?cmnd=Power%20On
Charger
http://192.168.178.65/cm?cmnd=Power%20off



We can use that last example to retrieve the following JSON body:
https://jsonpath.com/

{
    "StatusSNS": {
        "Time": "2022-01-11T22:12:32",
        "ENERGY": {
            "TotalStartTime": "2020-10-21T22:15:00",
            "Total": 3.705,
            "Yesterday": 0.028,
            "Today": 0.026,
            "Period": 1,
            "Power": 5,
            "ApparentPower": 5,
            "ReactivePower": 0,
            "Factor": 1.00,
      "Voltage": 233,
      "Current": 0.021
    }
  }
}
Example:
$.StatusSNS.ENERGY.Power
Result: 5


Example how to pars JSON object
JsonObject StatusSNS_MT175 = doc["StatusSNS"]["MT175"];
    //JsonObject StatusSNS_SML = doc["StatusSNS"]["SML"];
    //double StatusSNS_SML_DJ_TPWRIN = StatusSNS_SML["DJ_TPWRIN"]; // 4112.0703
    //double StatusSNS_SML_DJ_TPWROUT = StatusSNS_SML["DJ_TPWROUT"]; // 7311.2319
    //StatusSNS_SML_DJ_TPWRCURR = StatusSNS_SML["DJ_TPWRCURR"]; // -2546

    actPower = StatusSNS_MT175["P"]; // -2739

SENSOR = {"Time":"2023-01-25T21:35:03","ENERGY":{"TotalStartTime":"2023-01-25T18:06:18","Total":0.000,"Yesterday":0.000,"Today":0.000,"Period": 0,"Power": 0,"ApparentPower": 0,"ReactivePower": 0,"Factor":0.00,"Voltage": 0,"Current":0.000}}

A client must be defined:
WiFiClient espclient; // or WiFiClientSecure for HTTPS
HTTPClient http;

Example how to connect to client
  http.begin(espclient, "http://192.168.188.127/cm?cmnd=status+10");
  http.begin(espclient, "http://"+String(current_clamp_ip)+String(current_clamp_cmd));

*/

void setplug(char plugip[40], bool onoff)
{

  char cmd_plugon[40] = "/cm?cmnd=Power%20On";
  char cmd_plugoff[40] = "/cm?cmnd=Power%20off";

  if (onoff)
  {
    http.begin(current_clamp_client, "http://" + String(plugip) + String(cmd_plugon));
  }
  else
  {
    http.begin(current_clamp_client, "http://" + String(plugip) + String(cmd_plugoff));
  }
  int httpCode = http.GET();         // send the request
  String payload = http.getString(); // Get the request response payload
                                     //  Idea: get here the status of the plug on or off and compare with command that has been sent out. function can return true if o.k.
  return;
}

float GetSmartPlugData(char plugip[40], short topic)
{

  char cmd_plugstatus[40] = "/cm?cmnd=status+10";
  float topicresult;
  /*
  1: Voltage          (V)
  2: current          (A)
  3: active Power     (W)
  4: Energy today     (kWh)
  5: Energy yesterday (kWh)
  6: Energy Total     (kWh)
  7: (Status on or off)
  SENSOR = {"Time":"2023-01-25T21:35:03","ENERGY":{"TotalStartTime":"2023-01-
  25T18:06:18","Total":0.000,"Yesterday":0.000,"Today":0.000,"Period": 0,"Power":
  0,"ApparentPower": 0,"ReactivePower": 0,"Factor":0.00,"Voltage": 0,"Current":0.000}}
  */
  // http://192.168.178.68/cm?cmnd=status+10
  http.begin(current_clamp_client, "http://" + String(plugip) + String(cmd_plugstatus));

  int httpCode = http.GET(); // send the request
  if (httpCode > 0)
  {                                    // check the returning code
    String payload = http.getString(); // Get the request response payload
    // Stream input;
    StaticJsonDocument<384> doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      return 0; // NULL removed
    }
    //__________________add here evaluation og JSON Object

    JsonObject StatusSNS_ENERGY = doc["StatusSNS"]["ENERGY"];
    switch (topic)
    {

    // Voltage
    case 1:
      topicresult = StatusSNS_ENERGY["Voltage"]; // -2739
      break;

    // Current
    case 2:
      topicresult = StatusSNS_ENERGY["Current"];
      break;

    // Active Power
    case 3:
      topicresult = StatusSNS_ENERGY["Power"];
      break;

    // Energy Today
    case 4:
      topicresult = StatusSNS_ENERGY["Today"];
      break;

    // Energy yeserday
    case 5:
      topicresult = StatusSNS_ENERGY["Yesterday"];
      break;

    // Energy total
    case 6:
      topicresult = StatusSNS_ENERGY["Total"];
      break;
    }

    //__________________end of evaluation
  }

  return topicresult;
}