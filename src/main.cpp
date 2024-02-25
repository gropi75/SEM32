/*
Project homepage: https://github.com/gropi75/SEM32

*/

//#define test_debug // uncomment to just test without equipment
// #define wifitest    // uncomment to debug wifi status
// #define TCAN485_Hardware

#ifdef ESP32DEV               // for SEM32 HW
#define Soyo_RS485_PORT_TX 18 // GPIO18    -- DI
#define Soyo_RS485_PORT_RX 19 // GPIO19    -- RO
#define Soyo_RS485_PORT_EN 23 // GPIO23    -- DE/RE

#define JKBMS_RS485_PORT_TX 25 // GPIO25    -- DI
#define JKBMS_RS485_PORT_RX 26 // GPIO26    -- RO
#define JKBMS_RS485_PORT_EN 33 // GPIO33    -- DE/RE

// #define CAN_TX_PIN xx
// #define CAN_RX_PIN xx

#endif

#ifdef TCAN485_Hardware       // for LILYGO TCAN485 Board
#define Soyo_RS485_PORT_TX 25 // GPIO18    -- DI
#define Soyo_RS485_PORT_RX 32 // GPIO19    -- RO
#define Soyo_RS485_PORT_EN 33 // GPIO23    -- DE/RE

#define JKBMS_RS485_PORT_TX 22 // GPIO25    -- DI
#define JKBMS_RS485_PORT_RX 21 // GPIO26    -- RO
#define JKBMS_RS485_PORT_EN 17 // GPIO33    -- DE/RE
#define RS485_SE_PIN 19        // 22 /SHDN

#define PIN_5V_EN 16

#define CAN_TX_PIN 26
#define CAN_RX_PIN 27
#define CAN_SE_PIN 23

#define SD_MISO_PIN 2
#define SD_MOSI_PIN 15
#define SD_SCLK_PIN 14
#define SD_CS_PIN 13

#define WS2812_PIN 4

#endif

#ifdef BSC_Hardware // for BSC HW
/* Serial 0: RX: 3      TX: 1               -- used for programming, ISP
   Serial 1: RX: 16     TX: 17    EN: 18
   Serial 2: RX: 23     TX: 25    EN: 2
   Serial 3: RX: 35     TX: 33    EN: 32
*/

// use serial 1
#define Soyo_RS485_PORT_TX 17 //     -- DI
#define Soyo_RS485_PORT_RX 16 //     -- RO
#define Soyo_RS485_PORT_EN 18 //     -- DE/RE

// use serial 2
#define JKBMS_RS485_PORT_TX 25 //     -- DI
#define JKBMS_RS485_PORT_RX 23 //     -- RO
#define JKBMS_RS485_PORT_EN 2  //     -- DE/RE

#define CAN_TX_PIN 4
#define CAN_RX_PIN 5

#endif

#define FORMAT_LITTLEFS_IF_FAILED true
// You only need to format the filesystem once
// #define FORMAT_FILESYSTEM       true
#define FORMAT_FILESYSTEM false

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <CAN.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#define WEBSERVER_H
#include <AsyncTCP.h>
#include <WebSocketsServer.h>
#include "FS.h"
#include <LittleFS.h>
FS *filesystem = &LittleFS;
#define FileFS LittleFS
#define FS_Name "LittleFS"

#include <ESPAsyncWebServer.h> // https://github.com/lorol/ESPAsyncWebServer.git
#include <PubSubClient.h>
#include "huawei.h"
#include "commands.h"
#include "main.h"
#include "soyosource.h"
#include "PowerFunctions.h"
#include "jkbms.h"
#include "secrets.h"
#include "sunrise.h"
// #include "webui.h"

TaskHandle_t TaskCan;
TaskHandle_t webUI;
int packetSize;

// Servers and socket definition
WiFiServer server(23);
WiFiClient serverClient; // for OTA
WiFiClient mqttclient;   // to connect to MQTT broker
PubSubClient PSclient(mqttclient);

AsyncWebServer aserver(80);                         // the server uses port 80 (standard port for websites
WebSocketsServer webSocket = WebSocketsServer(81); // the websocket uses port 81 (standard port for websockets

#ifdef test_debug
const char ESP_Hostname[] = "Battery_Control_ESP32_TEST"; // Battery_Control_ESP32
#else
const char ESP_Hostname[] = "Battery_Control_ESP32"; // Battery_Control_ESP32
#endif

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
    int MaxPowerInv = 100;
    int DynPowerInv = 800;
    bool g_EnableCharge = true;
    float solar_prognosis;
    int target_SOC;              // [%]
    float BatteryCapacity = 4.6; // [kWh]
    float expDaytimeUsage = 3.0;

    char date_today[9];    // = "yyyymmdd";
    char date_tomorrow[9]; // = "yyyymmdd";
    int date_dayofweek_today = 8;

    const int ARRAY_LENGTH = 10;        // websocket


    unsigned long g_Time500 = 0;
    unsigned long g_Time1000 = 0;
    unsigned long g_Time5000 = 0;
    unsigned long g_Time30Min = 0;

    char temp_char[20]; // for temporary storage of strings values
    char mqtt_topic[60];

    // Time information
    TimeStruct myTime; // create a TimeStruct instance to hold the returned data
    float time_now;
    bool is_day; // true: is day, false: is night

    // BMS values as structure
    JK_BMS_Data BMS;
    JK_BMS_RS485_Data receivedRawData;

    // byte receivedBytes_main[320];

    uint16_t   g_ManualSetPowerCharger;
    bool g_enableManualControl, g_enableDynamicload = false;
    bool wm_resetsetting = false;
    int MQTTdisconnect_counter = 0;
    DynamicJsonDocument json_data(2048);
    
    // flag for saving data inside WifiManager
    bool shouldSaveConfig = true;

    // callback notifying us of the need to save config
    void saveConfigCallback()
    {
        Serial.println("Should save config");
        shouldSaveConfig = true;
    }

// initialize Web-UI
void webUI_init(void *parameter);

// update the websocket
void websocket_update();

// send one variable to the webserver
void sendJsonValue(String l_type, String l_value);

// send several variables as a JSON string to the clients
void sendJson(StaticJsonDocument<2800> json);

// send more variables to the webserver
void sendJsonArray(String l_type, int l_array_values[]);

// callback function
void webSocketEvent(byte num, WStype_t type, uint8_t *payload, size_t length);



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

    void CoreTask1(void *parameter)
    {
        Serial.println("loop on core 2");
        for (;;)
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
            if (g_EnableMQTT)
                PSclient.loop();
            websocket_update();
            delay(200);
        }
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
                MQTTdisconnect_counter++;
                if (MQTTdisconnect_counter > 10) // if reconnect fails several times, MQTT is disabled
                {
                    g_EnableMQTT = false;
                    break;
                }
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

    // save configuration parameter like mqtt server, max inverter power
    void saveconfigfile()
    {
        // save the custom parameters to FS
        if (shouldSaveConfig)
        {
            Serial.println("saving config");
#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
            DynamicJsonDocument json(1024);
#else
            DynamicJsonBuffer jsonBuffer;
            JsonObject &json = jsonBuffer.createObject();
#endif
            json["mqtt_server"] = mqtt_server;
            json["mqtt_port"] = mqtt_port;
            json["cur_ip"] = current_clamp_ip;
            json["cur_cmd"] = current_clamp_cmd;
            json["sensor_resp"] = sensor_resp;
            json["sensor_resp_power"] = sensor_resp_power;
            json["g_EnableMQTT"] = g_EnableMQTT;
            json["wm_resetsetting"] = wm_resetsetting;
            json["PowerReserveCharger"] = PowerReserveCharger;
            json["PowerReserveInv"] = PowerReserveInv;
            json["MaxPowerCharger"] = MaxPowerCharger;
            json["MaxPowerInv"] = MaxPowerInv;
            json["g_enableDynamicload"] = g_enableDynamicload;

            File configFile = LittleFS.open("/config.json", "w");
            if (!configFile)
            {
                Serial.println("failed to open config file for writing");
            }

#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
            serializeJson(json, Serial);
            serializeJson(json, configFile);
#else
            json.printTo(Serial);
            json.printTo(configFile);
#endif
            configFile.close();
            // end save
        }
    }

    void init()
    {
        Serial.begin(115200);
        while (!Serial)
            ;
        //        pinMode(12, OUTPUT);
        //        pinMode(13, OUTPUT);
        //        pinMode(14, OUTPUT);
        //        pinMode(26, OUTPUT);

        //        digitalWrite(26, LOW);
        //        digitalWrite(14, HIGH);

        Serial.println("BOOTED!");
        // ESPUI.setVerbosity(Verbosity::Quiet);
        //  to prepare the filesystem
        //  ESPUI.prepareFileSystem();

        // clean FS, for testing
        // LittleFS.format();

        // read configuration from FS json
        Serial.println("mounting FS...");

        if (LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED))
        {
            Serial.println("mounted file system");
            Serial.printf("- Bytes total:   %ld\n", LittleFS.totalBytes());
            Serial.printf("- Bytes genutzt: %ld\n\n", LittleFS.usedBytes());
            if (LittleFS.exists("/config.json"))
            {
                // file exists, reading and loading
                Serial.println("reading config file");
                File configFile = LittleFS.open("/config.json", "r");
                if (configFile)
                {
                    Serial.println("opened config file");
                    size_t size = configFile.size();
                    // Allocate a buffer to store contents of the file.
                    std::unique_ptr<char[]> buf(new char[size]);

                    configFile.readBytes(buf.get(), size);

#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
                    DynamicJsonDocument json(1024);
                    auto deserializeError = deserializeJson(json, buf.get());
                    serializeJson(json, Serial);
                    if (!deserializeError)
                    {
#else
                    DynamicJsonBuffer jsonBuffer;
                    JsonObject &json = jsonBuffer.parseObject(buf.get());
                    json.printTo(Serial);
                    if (json.success())
                    {
#endif
                        Serial.println("\nparsed json");
                        strcpy(mqtt_server, json["mqtt_server"]);
                        strcpy(mqtt_port, json["mqtt_port"]);
                        strcpy(current_clamp_ip, json["cur_ip"]);
                        strcpy(current_clamp_cmd, json["cur_cmd"]);
                        strcpy(sensor_resp, json["sensor_resp"]);
                        strcpy(sensor_resp_power, json["sensor_resp_power"]);
                        PowerReserveCharger = json["PowerReserveCharger"];
                        PowerReserveInv = json["PowerReserveInv"];
                        MaxPowerCharger = json["MaxPowerCharger"];
                        MaxPowerInv = json["MaxPowerInv"];
                        g_EnableMQTT = json["g_EnableMQTT"];
                        wm_resetsetting = json["wm_resetsetting"];
                        g_enableDynamicload = json["g_enableDynamicload"];
                    }
                    else
                    {
                        Serial.println("failed to load json config");
                    }
                    configFile.close();
                }
            }
        }
        else
        {
            Serial.println("failed to mount FS");
        }
        // end read

        // The extra parameters to be configured (can be either global or just in the setup)
        // After connecting, parameter.getValue() will get you the configured value
        // id/name placeholder/prompt default length
        WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
        WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
        WiFiManagerParameter custom_current_clamp_ip("cur_ip", "current_clamp_ip", current_clamp_ip, 40);
        WiFiManagerParameter custom_current_clamp_cmd("cur_cmd", "current_clamp_command", current_clamp_cmd, 40);
        WiFiManagerParameter custom_sensor_resp("sensor_resp", "sensor_resp", sensor_resp, 20);
        WiFiManagerParameter custom_sensor_resp_power("sensor_resp_power", "sensor_resp_power", sensor_resp_power, 20);
        //        WiFiManagerParameter custom_g_EnableMQTT("g_EnableMQTT", "g_EnableMQTT", g_EnableMQTT, 1);

        // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
        WiFiManager wm;
        wm.setWiFiAutoReconnect(true);

        // set config save notify callback
        wm.setSaveConfigCallback(saveConfigCallback);

        // add all your parameters here
        wm.addParameter(&custom_mqtt_server);
        wm.addParameter(&custom_mqtt_port);
        wm.addParameter(&custom_current_clamp_ip);
        wm.addParameter(&custom_current_clamp_cmd);
        wm.addParameter(&custom_sensor_resp);
        wm.addParameter(&custom_sensor_resp_power);
        //        wm.addParameter(&custom_g_EnableMQTT);

        // reset settings - wipe stored credentials for testing
        // these are stored by the esp library
        // wm.resetSettings();
        if (wm_resetsetting)
        {
            wm.resetSettings();
            wm_resetsetting = false;
        }

        // Automatically connect using saved credentials,
        // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
        // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
        // then goes into a blocking loop awaiting configuration and will return success result

        bool res;
        // res = wm.autoConnect(); // auto generated AP name from chipid
        // res = wm.autoConnect("AutoConnectAP"); // anonymous ap
        res = wm.autoConnect("AutoConnectAP", "password"); // password protected ap

        if (!res)
        {
            Serial.println("Failed to connect");
            // ESP.restart();
        }
        else
        {
            // if you get here you have connected to the WiFi
            Serial.println("WiFi connected... :)");
        }

        // read updated parameters
        /*        strcpy(mqtt_server, custom_mqtt_server.getValue());
                strcpy(mqtt_port, custom_mqtt_port.getValue());
                strcpy(current_clamp_ip, custom_current_clamp_ip.getValue());
                strcpy(current_clamp_cmd, custom_current_clamp_cmd.getValue());
                strcpy(sensor_resp, custom_sensor_resp.getValue());
                strcpy(sensor_resp_power, custom_sensor_resp_power.getValue());
        */

        Serial.println("The values in the file are: ");
        Serial.println("\tmqtt_server : " + String(mqtt_server));
        Serial.println("\tmqtt_port : " + String(mqtt_port));
        Serial.println("\tcurrent clamp : " + String(current_clamp_ip));
        Serial.println("\tcommand : " + String(current_clamp_cmd));
        Serial.println("\tResponse  : " + String(sensor_resp));
        Serial.println("\tResponse power : " + String(sensor_resp_power));
        Serial.println("\tEnable MQTT : " + String(g_EnableMQTT));

        saveconfigfile();

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

#ifdef BSC_Hardware
        CAN.setPins(CAN_RX_PIN, CAN_TX_PIN); // BSC Hardware is using different pinout
#endif

#ifdef TCAN485_Hardware
        pinMode(PIN_5V_EN, OUTPUT);
        digitalWrite(PIN_5V_EN, HIGH);

        pinMode(RS485_SE_PIN, OUTPUT);
        digitalWrite(RS485_SE_PIN, HIGH);

        CAN.setPins(CAN_RX_PIN, CAN_TX_PIN); // Lilygo TCAN485 is using different pinout
#endif

        if (!CAN.begin(125E3))
        {
            Serial.println("Starting CAN failed!");
            while (1)
                ;
        }
        Serial.println("CAN setup done");
        CAN.filterExtended(0x1081407F, 0x1081FFFF);

        xTaskCreatePinnedToCore(
            CoreTask1, /* Task function. */
            "TaskCan", /* name of task. */
            30000,     /* Stack size of task */
            NULL,      /* parameter of the task */
            1,         /* priority of the task */
            &TaskCan,  /* Task handle to keep track of created task */
            1);        /* pin task to core 1 */

        // crashes when calling some functions inside interrupt
        //  CAN.onReceive(onCANReceive);
#ifndef test_debug
        Huawei::setCurrent(0, true); // set 0 A as default
#endif
        DynPowerInv = MaxPowerInv;
        Soyosource_init_RS485(Soyo_RS485_PORT_RX, Soyo_RS485_PORT_TX, Soyo_RS485_PORT_EN);
        Serial.println("Soyosource inverter RS485 setup done");

        // initialize RS485 interface for JK_BMS
        JKBMS_init_RS485(JKBMS_RS485_PORT_RX, JKBMS_RS485_PORT_TX, JKBMS_RS485_PORT_EN);
        Serial.println("JK-BMS RS485 setup done");

        // initialize NTP connection
        setuptimeClient();

        // initialize MQTT, however only connect, if it is enabled
        PSclient.setServer(mqtt_server, atoi(mqtt_port));
        PSclient.setCallback(callback);
        if (g_EnableMQTT)
            reconnect();

        // initialize the web ui and websocket
//        xTaskCreatePinnedToCore(
//            webUI_init, /* Task function. */
//            "webUI", /* name of task. */
//            10000,     /* Stack size of task */
//            NULL,      /* parameter of the task */
//            1,         /* priority of the task */
//            &webUI,  /* Task handle to keep track of created task */
//            1);        /* pin task to core 1 */


        webUI_init(NULL);
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
        // CAN handling and OTA shifted to core 1
        if ((millis() - g_Time500) > 500)
        {

            g_Time500 = millis();
        }

        if ((millis() - g_Time1000) > 1000)
        {
#ifndef test_debug
            Huawei::every1000ms();
#endif

            // update the value for the inverter every second
            sendpower2soyo(ActualSetPowerInv, Soyo_RS485_PORT_EN);

            // GUI_update();
#if defined(ARDUINOJSON_VERSION_MAJOR) && ARDUINOJSON_VERSION_MAJOR >= 6
            // DynamicJsonDocument json(2048);
#else
            DynamicJsonBuffer jsonBuffer;
            JsonObject &json = jsonBuffer.createObject();
#endif
            json_data.clear();
            json_data["gridP"] = String(ActualPower);
            json_data["chargeP"] = String(ActualSetPowerCharger);
            json_data["invP"] = String(ActualSetPowerInv);
            json_data["DynInvP"] = String(DynPowerInv);
            json_data["maxPInv"] = String(MaxPowerInv);
            json_data["maxPCharg"] = String(MaxPowerCharger);
            json_data["PresCharg"] = String(PowerReserveCharger);
            json_data["PresInv"] = String(PowerReserveInv);
            json_data["PmanCharg"] = String(g_ManualSetPowerCharger);
            json_data["enPManCharg"] = bool(g_enableManualControl);
            json_data["enDynload"] = bool(g_enableDynamicload);
            json_data["mqttSuccess"] = bool(1);
            json_data["mqttIP"] = String(mqtt_server);
            json_data["mqttPort"] = String(mqtt_port);
            json_data["mqttEnable"] = bool(g_EnableMQTT);
            json_data["SOC"] = String(BMS.SOC);
            json_data["Battery_Power"] = String(BMS.Battery_Power);

            sendJson(json_data);
            //char buffer[2000];
            //serializeJsonPretty(json_data, buffer);
            //Serial.println(buffer);
            //json_data.clear();

            json_data["sol_prog"] = String(solar_prognosis);
            json_data["cellVoltage1"] = String(BMS.cellVoltage[1]);
            json_data["cellVoltage2"] = String(BMS.cellVoltage[2]);
            json_data["cellVoltage3"] = String(BMS.cellVoltage[3]);
            json_data["cellVoltage4"] = String(BMS.cellVoltage[4]);
            json_data["cellVoltage5"] = String(BMS.cellVoltage[5]);
            json_data["cellVoltage6"] = String(BMS.cellVoltage[6]);
            json_data["cellVoltage7"] = String(BMS.cellVoltage[7]);
            json_data["cellVoltage8"] = String(BMS.cellVoltage[8]);
            json_data["cellVoltage9"] = String(BMS.cellVoltage[9]);
            json_data["cellVoltage10"] = String(BMS.cellVoltage[10]);
            json_data["cellVoltage11"] = String(BMS.cellVoltage[11]);
            json_data["cellVoltage12"] = String(BMS.cellVoltage[12]);
            json_data["cellVoltage13"] = String(BMS.cellVoltage[13]);
            json_data["cellVoltage14"] = String(BMS.cellVoltage[14]);
            json_data["cellVoltage15"] = String(BMS.cellVoltage[15]);
            json_data["cellVoltage16"] = String(BMS.cellVoltage[16]);


            json_data["MOS_Temp"] = String(BMS.MOS_Temp);
            json_data["Battery_T1"] = String(BMS.Battery_T1);
            json_data["Battery_T2"] = String(BMS.Battery_T2);
            json_data["Battery_Voltage"] = String(BMS.Battery_Voltage);
            json_data["Charge_Current"] = String(BMS.Charge_Current);
            json_data["TempSensCount"] = String(BMS.TempSensCount);
            json_data["Cycle_Count"] = String(BMS.Cycle_Count);
            json_data["totBattCycleCapacity"] = String(BMS.totBattCycleCapacity);
            json_data["CellCount"] = String(BMS.CellCount);
            json_data["WarningMsgWord"] = String(BMS.WarningMsgWord);
            json_data["BatteryStatus"] = String(BMS.BatteryStatus);
            json_data["sBatteryStatus"] = BMS.sBatteryStatus;
            json_data["OVP"] = String(BMS.OVP);               // Total Over Voltage Protection
            json_data["UVP"] = String(BMS.UVP);               // Total Under VOltage Protection
            json_data["sOVP"] = String(BMS.sOVP);             // single cell OVP
            json_data["sOVPR"] = String(BMS.sOVPR);           // single cell OVP recovery
            json_data["sOVP_delay"] = String(BMS.sOVP_delay); // Single overvoltage protection delay
            json_data["sUVP"] = String(BMS.sUVP);             // Single Under VOltage Protection
            json_data["sUVPR"] = String(BMS.sUVPR);           // Monomer undervoltage recovery voltage
            json_data["sUVP_delay"] = String(BMS.sUVP_delay); // Single undervoltage protection delay
            json_data["sVoltDiff"] = String(BMS.sVoltDiff);   // Cell pressure difference protection value


            json_data["dischargeOCP"] = String(BMS.dischargeOCP);             // Discharge overcurrent protection value
            json_data["dischargeOCP_delay"] = String(BMS.dischargeOCP_delay); // Discharge overcurrent delay

            json_data["chargeOCP"] = String(BMS.chargeOCP);             // charge overcurrent protection value
            json_data["chargeOCP_delay"] = String(BMS.chargeOCP_delay); // charge overcurrent delay

            json_data["BalanceStartVoltage"] = String(BMS.BalanceStartVoltage); // Balanced starting voltage
            json_data["BalanceVoltage"] = String(BMS.BalanceVoltage);           // Balanced opening pressure difference
            json_data["ActiveBalanceEnable"] = String(BMS.ActiveBalanceEnable); // Active balance switch

            json_data["MOSProtection"] = String(BMS.MOSProtection);       // Power tube temperature protection value
            json_data["MOSProtectionR"] = String(BMS.MOSProtectionR);     // Power tube temperature protection recovery
            json_data["TempProtection"] = String(BMS.TempProtection);     // Temperature protection value in the battery box
            json_data["TempProtectionR"] = String(BMS.TempProtectionR);   // Temperature protection recovery value in the battery box
            json_data["TempProtDiff"] = String(BMS.TempProtDiff);         // Battery temperature difference protection value
            json_data["ChargeHTemp"] = String(BMS.ChargeHTemp);           // Battery charging high temperature protection value
            json_data["DisChargeHTemp"] = String(BMS.DisChargeHTemp);     // Battery discharge high temperature protection value
            json_data["ChargeLTemp"] = String(BMS.ChargeLTemp);           // Charging low temperature protection value
            json_data["ChargeLTempR"] = String(BMS.ChargeLTempR);         // Charging low temperature protection recovery value
            json_data["DisChargeLTemp"] = String(BMS.DisChargeLTemp);     // Discharge low temperature protection value
            json_data["DisChargeLTempR"] = String(BMS.DisChargeLTempR);   // DisCharging low temperature protection recovery value
            json_data["Nominal_Capacity"] = String(BMS.Nominal_Capacity); // Battery capacity setting


            // float Average_Cell_Voltage);
            json_data["Delta_Cell_Voltage"] = String(BMS.Delta_Cell_Voltage);
            json_data["MinCellVoltage"] = String(BMS.MinCellVoltage);
            json_data["MaxCellVoltage"] = String(BMS.MaxCellVoltage);
            json_data["Current_Balancer"] = String(BMS.Current_Balancer);
            json_data["Capacity_Remain"] = String(BMS.Capacity_Remain);
            json_data["Capacity_Cycle"] = String(BMS.Capacity_Cycle);
            json_data["Uptime"] = String(BMS.Uptime);
            json_data["Balance_Curr"] = String(BMS.Balance_Curr);
            json_data["charge"] = BMS.charge;
            json_data["discharge"] = BMS.discharge;
            json_data["balance"] = BMS.balance;

            g_Time1000 = millis();
        }

        if ((millis() - g_Time5000) > 5000)
        {
#ifndef test_debug
            Huawei::sendGetData(0x00);
            //          Huawei::HuaweiInfo &info = Huawei::g_PSU;

            // get and interpret the BMS data
            receivedRawData = JKBMS_read_data(JKBMS_RS485_PORT_EN);
            BMS = JKBMS_DataAnalysis(receivedRawData);
#endif
            // reads actual grid power every 5 seconds
            ActualPower = getActualPower(current_clamp_ip, current_clamp_cmd, sensor_resp, sensor_resp_power);
            ActualVoltage = Huawei::g_PSU.output_voltage;
            ActualCurrent = Huawei::g_PSU.output_current;

            // calculate desired power, including Dynamic Load calculation for the inverter.
            if (g_enableDynamicload)
                ActualSetPower = CalculatePower(ActualPower, ActualSetPower, PowerReserveCharger, MaxPowerCharger, PowerReserveInv, DynPowerInv);
            else
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
                // ActualSetPowerCharger = 0;
            }

            // send commands to the charger and inverter
            sendpower2soyo(ActualSetPowerInv, Soyo_RS485_PORT_EN);
#ifndef test_debug
            Huawei::setVoltage(ActualSetVoltage, 0x00, false);
#endif
            if (!g_enableManualControl)
            {
                ActualSetCurrent = CalculateChargingCurrent(ActualSetPowerCharger, ActualSetVoltage, BMS.MaxCellVoltage, BMS.Battery_T1, BMS.Nominal_Capacity, 0);
//                ActualSetCurrent = ActualSetPowerCharger / ActualSetVoltage;
#ifndef test_debug
                Huawei::setCurrent(ActualSetCurrent, false);
#endif
            }
            else if (g_enableManualControl)
            {
                ActualSetCurrent = g_ManualSetPowerCharger / ActualSetVoltage;
#ifndef test_debug
                Huawei::setCurrent(ActualSetCurrent, false);
#endif
            }

            if (g_EnableMQTT)
            {
                if (!PSclient.connected())
                    reconnect();          // oif the server is disconnected, reconnect to it
                if (PSclient.connected()) // only send data, if the server is connected
                {
                    char out[2048];
                    int b =serializeJson(json_data, out);

                    boolean rc = PSclient.publish(ESP_Hostname, out);


                    sprintf(temp_char, "%d", BMS.SOC);
                    sprintf(mqtt_topic, "%s/Data/SOC", ESP_Hostname);
                    PSclient.publish(mqtt_topic, temp_char);

                    sprintf(mqtt_topic, "%s/Status", ESP_Hostname);
                    PSclient.publish(mqtt_topic, BMS.sBatteryStatus);

                    sprintf(temp_char, "%d", BMS.Cycle_Count);
                    sprintf(mqtt_topic, "%s/Data/Cycle_Count", ESP_Hostname);
                    PSclient.publish(mqtt_topic, temp_char);

                    sprintf(mqtt_topic, "%s/Data/Battery_Voltage", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.Battery_Voltage, 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Charge_Current", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.Charge_Current, 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Delta_Cell_Voltage", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.Delta_Cell_Voltage, 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/MOS_Temp", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.MOS_Temp, 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Battery_T1", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.Battery_T1, 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Battery_T2", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.Battery_T2, 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Cell_00", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.cellVoltage[0], 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Cell_01", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.cellVoltage[1], 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Cell_02", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.cellVoltage[2], 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Cell_03", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.cellVoltage[3], 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Cell_04", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.cellVoltage[4], 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Cell_05", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.cellVoltage[5], 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Cell_06", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.cellVoltage[6], 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Cell_07", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.cellVoltage[7], 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Cell_08", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.cellVoltage[8], 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Cell_09", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.cellVoltage[9], 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Cell_10", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.cellVoltage[10], 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Cell_11", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.cellVoltage[11], 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Cell_12", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.cellVoltage[12], 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Cell_13", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.cellVoltage[13], 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Cell_14", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.cellVoltage[14], 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Cell_15", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.cellVoltage[15], 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Battery_Power", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.Battery_Power, 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Balance_Current", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.Balance_Curr, 6, 3, temp_char));

                    sprintf(mqtt_topic, "%s/Data/Max_Cell_Voltage", ESP_Hostname);
                    PSclient.publish(mqtt_topic, dtostrf(BMS.MaxCellVoltage, 6, 3, temp_char));
                }
            }
            g_Time5000 = millis();
        }

#ifdef test_debug
        if ((millis() - g_Time30Min) > 10000) // every 10 seconds in case of debugging
#endif
#ifndef test_debug
            if (((millis() - g_Time30Min) > (180 * 60 * 1000)) || (date_dayofweek_today == 8)) // 7.200.000 ms = 2 hours, 10.800.000 = 3h due to daily API call limits
#endif
            {
                TimeData(&myTime, lagmorning, lagevening, laenge, breite); // call the TimeData function with the parameters and the pointer to the TimeStruct instance
                                                                           // for testing purpose we execute it several time a day
#ifndef test_debug
                solar_prognosis = getSolarPrognosis(solarprognose_token, solarprognose_id, myTime.date_today, myTime.date_tomorrow);
#endif
                // execute the following only once a day due to API limitations
                if (date_dayofweek_today != myTime.day_of_week)
                {
                    //                    solar_prognosis = getSolarPrognosis(solarprognose_token, solarprognose_id, myTime.date_today, myTime.date_tomorrow);
                    date_dayofweek_today = myTime.day_of_week;
                }
                g_Time30Min = millis();
            }

        // at sunrise and sunset execute the following section
        if (is_day != setPVstartflag(lagmorning, lagevening, laenge, breite))
        {
            is_day = !(is_day);
            if (!is_day) // execute at sunset the calculation of the dynamic power for the night
            {
                BatteryCapacity = BMS.CellCount * BMS.Nominal_Capacity * 3.2 / 1000;
                if (solar_prognosis < expDaytimeUsage)
                {
                    target_SOC = 100;
                }
                else if (solar_prognosis < (BatteryCapacity + expDaytimeUsage))
                {
                    target_SOC = int(100 * (BatteryCapacity - solar_prognosis + expDaytimeUsage) / (BatteryCapacity));
                }
                else
                    target_SOC = 0;
                DynPowerInv = CalculateBalancedDischargePower(BMS.Nominal_Capacity, BMS.Battery_Voltage, BMS.SOC, target_SOC, myTime.sunset_today, myTime.sunrise_tomorrow);
            }
        }
    }

void websocket_update()
{
    webSocket.loop(); // Update function for the webSockets
}

// Simple function to send information to the web clients
void sendJsonValue(String l_type, String l_value)
{
    String jsonString = "";                     // create a JSON string for sending data to the client
    StaticJsonDocument<200> doc;                // create JSON container
    JsonObject object = doc.to<JsonObject>();   // create a JSON Object
    object["type"] = l_type;                    // write data into the JSON object
    object["value"] = l_value;
    serializeJson(doc, jsonString);             // convert JSON object to string
    webSocket.broadcastTXT(jsonString);         // send JSON string to all clients
}

// send several variables as a JSON string to the clients
void sendJson(StaticJsonDocument<2800> jsonS)
{
    char jsonString[2800] = "";                     // create a JSON string for sending data to the client
    serializeJson(jsonS, jsonString);           // convert JSON object to string
    webSocket.broadcastTXT(jsonString);         // send JSON string to all clients
}

// Simple function to send information to the web clients
void sendJsonArray(String l_type, int l_array_values[])
{
    String jsonString = ""; // create a JSON string for sending data to the client
    const size_t CAPACITY = JSON_ARRAY_SIZE(ARRAY_LENGTH) + 100;
    StaticJsonDocument<CAPACITY> doc; // create JSON container

    JsonObject object = doc.to<JsonObject>(); // create a JSON Object
    object["type"] = l_type;                  // write data into the JSON object
    JsonArray value = object.createNestedArray("value");
    for (int i = 0; i < ARRAY_LENGTH; i++)
    {
        value.add(l_array_values[i]);
    }
    serializeJson(doc, jsonString);     // convert JSON object to string
    webSocket.broadcastTXT(jsonString); // send JSON string to all clients
}

// handler for incoming data over websocket
void webSocketEvent(byte num, WStype_t type, uint8_t *payload, size_t length)
{ // the parameters of this callback function are always the same -> num: id of the client who send the event, type: type of message, payload: actual data sent and length: length of payload
    switch (type)
    {                         // switch on the type of information sent
    case WStype_DISCONNECTED: // if a client is disconnected, then type == WStype_DISCONNECTED
        Serial.println("Client " + String(num) + " disconnected");
        break;
    case WStype_CONNECTED: // if a client is connected, then type == WStype_CONNECTED
        Serial.println("Client " + String(num) + " connected");
        break;
    case WStype_TEXT: // if a client has sent data, then type == WStype_TEXT
        // try to decipher the JSON string received
        StaticJsonDocument<200>  doc; // create JSON container
        DeserializationError error = deserializeJson(doc, payload);
        if (error)
        {
            Serial.print(F("deserializeJson() failed: "));
            Serial.println(error.f_str());
            return;
        }
        else
        {
            // JSON string was received correctly, so information can be retrieved:
            String l_type = doc["command"];
            int l_value = doc["value"];
            String really = (char * )payload;
            Serial.println(really);
            Serial.println(l_type);
            if (l_type == "maxPInv"){
                    Serial.println("Type: " + String(l_type));
                    Serial.println("Value: " + String(l_value));
                    MaxPowerInv = l_value;
                    break;
            }
            else if (l_type == "maxPCharg") {
                    Serial.println("Type: " + String(l_type));
                    Serial.println("Value: " + String(l_value));
                    MaxPowerCharger = l_value;
                    break;
            }
            else if (l_type == "enManControl") {
                    Serial.println("Type: " + String(l_type));
                    Serial.println("Value: " + String(l_value));
                    g_enableManualControl = l_value;
                    break;
            }
            else if (l_type == "manPCharg") {
                    Serial.println("Type: " + String(l_type));
                    Serial.println("Value: " + String(l_value));
                    g_ManualSetPowerCharger = l_value;
                    break;
            }
            else if (l_type == "PresCharg") {
                    Serial.println("Type: " + String(l_type));
                    Serial.println("Value: " + String(l_value));
                    PowerReserveCharger = l_value;
                    break;
            }
            else if (l_type == "enDynControl") {
                    Serial.println("Type: " + String(l_type));
                    Serial.println("Value: " + String(l_value));
                    g_enableDynamicload = l_value;
                    break;
            }
            else if (l_type == "PresInv") {
                    Serial.println("Type: " + String(l_type));
                    Serial.println("Value: " + String(l_value));
                    PowerReserveInv = l_value;
                    break;
            }
            else if (l_type == "SaveBattSett") {
                    Serial.println("Type: " + String(l_type));
                    Serial.println("Value: " + String(l_value));
                    saveconfigfile();
                    break;
            }
            else if (l_type == "set_mqtt") {
                    int l_value = doc["value"];
                    strcpy(mqtt_server , doc["ip"]);
                    strcpy(mqtt_port , doc["port"]);
                    g_EnableMQTT = bool(doc["enable"]);
                    Serial.println("Type: " + String(l_type));
                    Serial.println("IP: " + String(mqtt_server));
                    Serial.println("Port: " + String(mqtt_port));
                    Serial.print("Enable: ");
                    Serial.println(g_EnableMQTT ? "true" : "false");           
                    //saveconfigfile();
                    break;
            }
            else {
                Serial.println("nothing");
            }

        }
        Serial.println("");
        break;
    }
}

//Start webserver and websocket
void webUI_init(void *parameter)
{
    aserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse(LittleFS, "/index.html.gz", "text/html");
        response->addHeader("Content-Encoding", "gzip");
        request->send(response); 
    });

    aserver.onNotFound([](AsyncWebServerRequest *request)
                      { request->send(404, "text/plain", "File not found"); });
    aserver.serveStatic("/", LittleFS, "/");    //.setDefaultFile("index.html.gz")
    webSocket.begin();                 // start websocket
    webSocket.onEvent(webSocketEvent); // define a callback function -> what does the ESP32 need to do when an event from the websocket is received? -> run function "webSocketEvent()"
    Serial.println("Websocket started");
    aserver.begin(); // start server -> best practise is to start the server after the websocket
    Serial.println("Webserver started");
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
