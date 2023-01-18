/*JSON help: https://arduinojson.org/v6/assistant/#/step1
ESP32 infos: https://www.upesy.com/blogs/tutorials/how-to-connect-wifi-acces-point-with-esp32


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
#include <PubSubClient.h>
#include <ESPAsyncWebServer.h>

#include "huawei.h"
#include "commands.h"
#include "main.h"
#include "soyosource.h"
#include "PowerFunctions.h"
#include "secrets.h"

WiFiServer server(23);
WiFiClient serverClient; // for OTA

WiFiClient mqttclient; // to connect to MQTT broker
PubSubClient PSclient(mqttclient);

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
    int MaxPowerCharger = 2000;
    int MaxPowerInv = 200;
    bool g_EnableCharge = true;

    unsigned long g_Time1000 = 0;
    unsigned long g_Time5000 = 0;

    char current_clamp_ip[40] = "192.168.188.127";
    char current_clamp_cmd[40] = "/cm?cmnd=status+10";
    char sensor_resp[20] = "SML";               // or "MT175"
    char sensor_resp_power[20] = "DJ_TPWRCURR"; // or "P"

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
        }
    }

    void init()
    {
        Serial.begin(115200);
        while (!Serial)
            ;
        Serial.println("BOOTED!");

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
        Serial.println(g_WIFI_SSID);

        Serial.println("[+] BSSID : " + WiFi.BSSIDstr());
        Serial.print("[+] Gateway IP : ");
        Serial.println(WiFi.gatewayIP());
        Serial.print("[+] Subnet Mask : ");
        Serial.println(WiFi.subnetMask());
        Serial.println((String) "[+] RSSI : " + WiFi.RSSI() + " dB");
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

    void loop()
    {

        int packetSize = CAN.parsePacket();
        if (packetSize)
            onCANReceive(packetSize);

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
            Huawei::HuaweiInfo &info = Huawei::g_PSU;

            // reads actual power every 5 seconds
            ActualPower = getActualPower(current_clamp_ip, current_clamp_cmd, sensor_resp, sensor_resp_power);
            ActualVoltage = info.output_voltage;
            ActualCurrent = info.output_current;

            // calculate desired power
            ActualSetPower = CalculatePower(ActualPower, ActualSetPower, PowerReserveCharger, MaxPowerCharger, PowerReserveInv, MaxPowerInv);

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

            if (g_EnableCharge)
            {
            }
            else if (!g_EnableCharge)
            {
                ActualSetPowerCharger = 0;
            }

            // send commands to the charger and inverter
            sendpowertosoyo(ActualSetPowerInv);

            Huawei::setVoltage(ActualSetVoltage, false);
            ActualSetCurrent = ActualSetPowerCharger / ActualSetVoltage;
            Huawei::setCurrent(ActualSetCurrent, false);

            Serial.print(ActualPower);
            Serial.print("   ");
            Serial.print(ActualSetPower);
            Serial.print("   ");
            Serial.print(ActualSetPowerInv);
            Serial.print("   ");
            Serial.print(ActualSetPowerCharger);

            Serial.println();
            // char temp[60];
            // dtostrf(ActualSetCurrent,2,2,temp);
            // sprintf(temp, "{ \"idx\" : 326, \"nvalue\" : 0, \"svalue\" : \"%.2f\" }", ActualSetCurrent);
            // PSclient.publish("domoticz/in", temp);
            // PSclient.loop();

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
