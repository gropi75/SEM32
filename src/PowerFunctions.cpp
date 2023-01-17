#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

WiFiClient current_clamp_client; // or WiFiClientSecure for HTTPS; to connect to current sensor
HTTPClient http;                 // to connect to current sensor

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
        {                                                                     // and we still have sufficient solar power
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
        {                                                                 // and we get close to 0 imported power
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
    {                                      // check the returning code
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