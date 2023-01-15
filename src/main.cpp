/*JSON help: https://arduinojson.org/v6/assistant/#/step1
ESP32 infos: https://www.upesy.com/blogs/tutorials/how-to-connect-wifi-acces-point-with-esp32


Implementen dashboard: https://ayushsharma82.github.io/ESP-DASH/
Open: Use of https://github.com/s00500/ESPUI

Pinpout:

Charger (CAN Bus)
GPIO4:  CAN
GPIO5:  CAN

Inverter (RS485)
GPIO18: TX 
GPIO19: RX
GPIO4: EN (if needed by the transceiver)

Display (I2C)


BMS (RS485)



*/

// #define test_debug // uncomment to just test the power calculation part.
// #define wifitest    // uncomment to debug wifi status

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <CAN.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#include <ESPAsyncWebServer.h>
#include <ESPDash.h>

#include "huawei.h"
#include "commands.h"
#include "main.h"

#include "soyosource.h"
#include "CalculatePower.h"

#include "secrets.h"

WiFiServer server(23);
WiFiClient serverClient;         // for OTA
WiFiClient current_clamp_client; // or WiFiClientSecure for HTTPS; to connect to current sensor
HTTPClient http;                 // to connect to current sensor

WiFiClient mqttclient; // to connect to MQTT broker
PubSubClient PSclient(mqttclient);

AsyncWebServer webserver(80); // for Dashoard
ESPDash dashboard(&webserver);

const char ESP_Hostname[] = "Battery_Control_ESP32";

namespace Main
{

    int g_CurrentChannel;
    bool g_Debug[NUM_CHANNELS];
    char g_SerialBuffer[NUM_CHANNELS][255];
    int g_SerialBufferPos[NUM_CHANNELS];
    int ActualPower = 0; // for current Power out of current clamp
    float ActualVoltage = 0;
    float ActualCurrent = 0;
    int ActualSetPower = 0;
    int ActualSetPowerInv = 0;
    int ActualSetPowerCharger = 0;
    float ActualSetVoltage = 56.2;
    float ActualSetCurrent = 0;
    int PowerReserveCharger = 15;
    int PowerReserveInv = 15;
    bool g_EnableCharge = true;

    /*
      Dashboard Cards
      Format - (Dashboard Instance, Card Type, Card Name, Card Symbol(optional) )
    */
    Card CardDeviceStatus(&dashboard, STATUS_CARD, "Device Status", "idle");
    Card CardActualPower(&dashboard, GENERIC_CARD, "Home Grid Power", "W");
    Card CardActualVoltage(&dashboard, GENERIC_CARD, "Battery Voltage", "V");
    Card CardActualCurrent(&dashboard, GENERIC_CARD, "Charing Current", "A");
    Card CardActualSetPower(&dashboard, GENERIC_CARD, "Actual Set Power", "W");
    Card CardActualSetVoltage(&dashboard, GENERIC_CARD, "Actual Set Voltage", "V");
    Card CardActualSetCurrent(&dashboard, GENERIC_CARD, "Actual Set Current", "A");
    Card TargetPowerReserveCharger(&dashboard, SLIDER_CARD, "Power Reserve", "W", 0, 100);
    Card CardSetVoltage(&dashboard, SLIDER_CARD, "Charge Voltage", "V", 54.4, 57.6);
    Card CardEnableCharge(&dashboard, BUTTON_CARD, "Enable charing");

    unsigned long g_Time1000 = 0;
    unsigned long g_Time5000 = 0;

    // Get actual power from the central power meter.
    int getActualPower()
    {
        int actPower;
        http.begin(current_clamp_client, "http://" + String(current_clamp_ip) + String(current_clamp_cmd));
        int httpCode = http.GET(); // send the request
        if (httpCode > 0)
        {                                      // check the returning code
            String payload = http.getString(); // Get the request response payload
            // Stream input;
            StaticJsonDocument<192> doc;
            DeserializationError error = deserializeJson(doc, payload);
            if (error)
            {
                Serial.print(F("deserializeJson() failed: "));
                Serial.println(error.f_str());
                // return NULL;
                return 0;
            }
            JsonObject StatusSNS_SML = doc["StatusSNS"][String(sensor_resp)];
            actPower = StatusSNS_SML[String(sensor_resp_power)]; // -2546
        }
        return actPower;
    }

    void onCANReceive(int packetSize)
    {
        if (!CAN.packetExtended())
            return;
        if (CAN.packetRtr())
            return;

        uint32_t msgid = CAN.packetId();

        uint8_t data[packetSize];
        CAN.readBytes(data, sizeof(data));

        Huawei::onRecvCAN(msgid, data, packetSize);
    }

    // (re)connect to MQTT broker
    void reconnect()
    {
        // Loop until we're reconnected
        while (!PSclient.connected())
        {
            Serial.print("Attempting MQTT connection...");
            // Create a client ID
            String clientId = ESP_Hostname;
            clientId += String(random(0xffff), HEX);
            // Attempt to connect
            if (PSclient.connect(clientId.c_str()))
            {
                Serial.println("connected");
                // Once connected, publish an announcement...
                PSclient.publish("myoutTopic", "hello world");
                // ... and resubscribe
                PSclient.subscribe("BatteryCharger/enable");
            }
            else
            {
                Serial.print("failed, rc=");
                Serial.print(PSclient.state());
                Serial.println(" try again in 5 seconds");
                // Wait 5 seconds before retrying
                delay(5000);
            }
        }
    }

    // callback function for MQTT
    void callback(char *topic, byte *message, unsigned int length)
    {
        Serial.print("Message arrived on topic: ");
        Serial.print(topic);
        Serial.print(". Message: ");
        String messageTemp;

        for (int i = 0; i < length; i++)
        {
            Serial.print((char)message[i]);
            messageTemp += (char)message[i];
        }
        Serial.println();

        // Feel free to add more if statements to control more GPIOs with MQTT

        // If a message is received on the topic esp32/output, you check if the message is either "on" or "off".
        // Changes the output state according to the message
        if (String(topic) == "BatteryCharger/enable")
        {
            Serial.print("Changing output to ");
            if (messageTemp == "1")
            {
                //  Serial.println("on");
                g_EnableCharge = true;
            }
            else if (messageTemp == "0")
            {
                //  Serial.println("off");
                g_EnableCharge = false;
            }
            CardEnableCharge.update(g_EnableCharge);
            dashboard.sendUpdates();
        }
    }

    void init()
    {
        Serial.begin(115200);
        while (!Serial)
            ;
        Serial.println("BOOTED!");

        // PSU enable pin
        // pinMode(POWER_EN_GPIO, OUTPUT_OPEN_DRAIN);
        // digitalWrite(POWER_EN_GPIO, 0); // Default = ON

        WiFi.setHostname(ESP_Hostname);
        WiFi.begin(g_WIFI_SSID, g_WIFI_Passphrase);
        while (WiFi.status() != WL_CONNECTED)
        {
            delay(500);
            Serial.print(".");
        }
        WiFi.setAutoReconnect(true);
        WiFi.persistent(true);

        Serial.print("[*] Network information for ");
        Serial.println(ssid);

        Serial.println("[+] BSSID : " + WiFi.BSSIDstr());
        Serial.print("[+] Gateway IP : ");
        Serial.println(WiFi.gatewayIP());
        Serial.print("[+] Subnet Mask : ");
        Serial.println(WiFi.subnetMask());
        Serial.println((String)"[+] RSSI : " + WiFi.RSSI() + " dB");
        Serial.print("[+] ESP32 IP : ");
        Serial.println(WiFi.localIP());
        Serial.print("[+] ESP32 hostnape : ");
        Serial.println(WiFi.getHostname());

        ArduinoOTA.onStart([]()
                           {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
        else // U_SPIFFS
            type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type); })
            .onEnd([]()
                   { Serial.println("\nEnd"); })
            .onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
            .onError([](ota_error_t error)
                     {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed"); });

        ArduinoOTA.begin();

        server.begin();
        server.setNoDelay(true);

        if (!CAN.begin(125E3))
        {
            Serial.println("Starting CAN failed!");
            while (1)
                ;
        }
        Serial.println("CAN setup done");

// crashes when calling some functions inside interrupt
// CAN.onReceive(onCANReceive);
#ifndef test_debug
        Huawei::setCurrent(0, true); // set 0 A as default
#endif

        init_RS485();
        Serial.println("RS485 setup done");

        /*PSclient.setServer(mqtt_server, atoi(mqtt_port));
        PSclient.setCallback(callback);
        reconnect();
        */

        TargetPowerReserveCharger.attachCallback([&](int value) { /* Attach Slider Callback */
                                                                  /* Make sure we update our slider's value and send update to dashboard */
                                                                  TargetPowerReserveCharger.update(value);
                                                                  dashboard.sendUpdates();
                                                                  PowerReserveCharger = value;
        });

        /*
        We provide our attachCallback with a lambda function to handle incomming data
        `value` is the boolean sent from your dashboard
        */
        CardEnableCharge.update(g_EnableCharge);
        CardEnableCharge.attachCallback([&](bool value)
                                        {
        //Serial.println("[CardEnableCharge] Button Callback Triggered: "+String((value)?"true":"false"));
        g_EnableCharge = value;
        CardEnableCharge.update(value);
        dashboard.sendUpdates(); });

        CardSetVoltage.attachCallback([&](int value) { /* Attach Slider Callback */
                                                       /* Make sure we update our slider's value and send update to dashboard */
                                                       CardSetVoltage.update(value);
                                                       dashboard.sendUpdates();
                                                       ActualSetVoltage = value;
        });

        webserver.begin(); // start Asynchron Webserver

        // update dashboard
        TargetPowerReserveCharger.update(PowerReserveCharger);
        CardSetVoltage.update(ActualSetVoltage);
        CardDeviceStatus.update("Running", "success");
        dashboard.sendUpdates();
        Serial.println("Init done");
    }

    Stream *channel(int num)
    {
        if (num == -1)
            num = g_CurrentChannel;

        if (num == TCPSERIAL && serverClient)
            return &serverClient;

        return &Serial;
    }

#ifdef test_debug
    int increm = 1, count = 70;
    int ActualSetPower_old = 0;
#endif

    void loop()
    {
#ifndef test_debug
        int packetSize = CAN.parsePacket();
        if (packetSize)
            onCANReceive(packetSize);
#endif

        ArduinoOTA.handle();
        if (server.hasClient())
        {
            if (serverClient) // disconnect current client if any
                serverClient.stop();
            serverClient = server.available();
        }
        if (!serverClient)
            serverClient.stop();


        if ((millis() - g_Time1000) > 1000)
        {
            #ifndef test_debug

            Huawei::every1000ms();
            
            PSclient.loop();
            #endif

            // update the value for the inverter every second
            sendpowertosoyo(ActualSetPowerInv);
            g_Time1000 = millis();
        }


        if ((millis() - g_Time5000) > 5000)
        {
#ifndef test_debug
            Huawei::HuaweiInfo &info = Huawei::g_PSU;

            // reads actual power every 5 seconds
            ActualPower = getActualPower();
            ActualVoltage = info.output_voltage;
            ActualCurrent = info.output_current;
#endif

#ifdef test_debug
            ActualPower = getActualPower(); //-ActualSetPower;
            //ActualPower = increm * random(20) + ActualPower - ActualSetPower + ActualSetPower_old; // for simulation with random values
            // ActualPower+= 10*increm;
            ActualSetPower_old = ActualSetPower;
            if (count == 100)
            {
                increm = 0 - increm;
                count = 0;
            }
            count++;

#ifdef wifitest

            switch (WiFi.status())
            {
            case WL_NO_SSID_AVAIL:
                Serial.println("Configured SSID cannot be reached");
                break;
            case WL_CONNECTED:
                Serial.println("Connection successfully established");
                break;
            case WL_CONNECT_FAILED:
                Serial.println("Connection failed");
                break;
            }
            Serial.printf("Connection status: %d\n", WiFi.status());
            Serial.print("RRSI: ");
            Serial.println(WiFi.RSSI());

#endif

#endif

            // calculate desired power
            ActualSetPower = CalculatePower(ActualPower, ActualSetPower, PowerReserveCharger, 1000, PowerReserveInv, 600);

            // decide, whether the charger or inverter shall be activated

            if (ActualSetPower >= 0)
            { // inverter
                ActualSetPowerCharger = 0;
                ActualSetPowerInv = ActualSetPower;
            }
            else if (ActualPower < 0)
            { // charger
                ActualSetPowerCharger = -ActualSetPower;
                ActualSetPowerInv = 0;
            }

#ifndef test_debug
            if (g_EnableCharge)
            {
                CardDeviceStatus.update("Charging", "success");
            }
            else if (!g_EnableCharge)
            {
                ActualSetPowerCharger = 0;
                CardDeviceStatus.update("Output disabled", "idle");
            }
#endif

// send commands to the charger and inverter
            sendpowertosoyo(ActualSetPowerInv);
#ifndef test_debug

            Huawei::setVoltage(ActualSetVoltage, false);
            ActualSetCurrent = ActualSetPowerCharger / ActualSetVoltage;
            Huawei::setCurrent(ActualSetCurrent, false);
#endif

#ifdef test_debug
            Serial.print(count);
            Serial.print("   ");
#endif

            Serial.print(ActualPower);
            Serial.print("   ");
            Serial.print(ActualSetPower);
            Serial.print("   ");
            Serial.print(ActualSetPowerInv);
            Serial.print("   ");
            Serial.print(ActualSetPowerCharger);
/*
#ifndef test_debug
            Serial.print("   ");
            Serial.print(ActualSetVoltage);
            Serial.print("   ");
            Serial.print(ActualSetCurrent);
#endif
*/
            Serial.println();
            // char temp[60];
            // dtostrf(ActualSetCurrent,2,2,temp);
            // sprintf(temp, "{ \"idx\" : 326, \"nvalue\" : 0, \"svalue\" : \"%.2f\" }", ActualSetCurrent);
            // PSclient.publish("domoticz/in", temp);
            // PSclient.loop();
            CardActualPower.update(ActualPower);
            CardActualVoltage.update(ActualVoltage);
            CardActualCurrent.update(ActualCurrent);
            CardActualSetPower.update(ActualSetPower);
            CardActualSetVoltage.update(ActualSetVoltage);
            CardActualSetCurrent.update(ActualSetCurrent);
            CardEnableCharge.update(g_EnableCharge);
            dashboard.sendUpdates();

            g_Time5000 = millis();
        }
    }

}

void setup()
{
    Main::init();
}

void loop()
{
    Main::loop();
}
