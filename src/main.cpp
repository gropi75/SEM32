/*JSON help: https://arduinojson.org/v6/assistant/#/step1
ESP32 infos: https://www.upesy.com/blogs/tutorials/how-to-connect-wifi-acces-point-with-esp32


Open: Use of https://github.com/s00500/ESPUI

Pinpout:

Charger (CAN Bus)
CAN RX:   GPIO4
CAN TX:   GPIO5

Inverter (RS485)
TX:     GPIO18
RX:     GPIO19
EN:     GPIO23  (if needed by the transceiver)

Display (I2C)
SDA:   GPIO21
SCK:   GPIO22

BMS (RS485)???
TX:  GPIO25
RX:  GPIO26
EN:  GPIO

1-wire sensors
Data:   GPIO23

Digital switches:
1
2
3
4




*/

// #define test_debug // uncomment to just test the power calculation part.
// #define wifitest    // uncomment to debug wifi status

// pins for Soyosource
#define Soyo_RS485_PORT_TX 18 // GPIO18
#define Soyo_RS485_PORT_RX 19 // GPIO19
#define Soyo_RS485_PORT_EN 23 // GPIO23

#define JKBMS_RS485_PORT_TX 25 // GPIO25
#define JKBMS_RS485_PORT_RX 26 // GPIO26
#define JKBMS_RS485_PORT_EN 33 // GPIO33    ???????

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <CAN.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#define WEBSERVER_H
#include <ESPAsyncWebServer.h> // https://github.com/lorol/ESPAsyncWebServer.git
// #define USE_LittleFS

#if defined(ESP8266)
/* ESP8266 Dependencies */
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h> // https://github.com/lorol/ESPAsyncWebServer.git
#include <ESP8266mDNS.h>
//  #include <FS.h>
#include <LittleFS.h>
FS *filesystem = &LittleFS;
#define FileFS LittleFS
#define FS_Name "LittleFS"

#elif defined(ESP32)
/* ESP32 Dependencies */
#include <WiFi.h>
#include <AsyncTCP.h>
//  #include <SPIFFS.h>
#include "FS.h"
#include <LittleFS.h>
FS *filesystem = &LittleFS;
#define FileFS LittleFS
#define FS_Name "LittleFS"
#endif
// Littfs error. Solution: update : pio pkg update -g -p espressif32

#include <ESPUI.h>
#include <PubSubClient.h>
#include "huawei.h"
#include "commands.h"
#include "main.h"
#include "soyosource.h"
#include "PowerFunctions.h"
#include "jkbms2.h"
#include "secrets.h"

TaskHandle_t TaskCan;
int packetSize;

WiFiServer server(23);
WiFiClient serverClient; // for OTA

WiFiClient mqttclient; // to connect to MQTT broker
PubSubClient PSclient(mqttclient);

// You only need to format the filesystem once
// #define FORMAT_FILESYSTEM       true
#define FORMAT_FILESYSTEM false

const char ESP_Hostname[] = "Battery_Control_ESP32";       // Battery_Control_ESP32

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
    bool g_EnableCharge = true;
    bool g_EnableMQTT = true;

    unsigned long g_Time500 = 0;
    unsigned long g_Time1000 = 0;
    unsigned long g_Time5000 = 0;

    char temp_char[10]; // for temporary storage of strings values
    float tempfloat;
    char mqtt_topic[34];

    // BMS values as structure
    JK_BMS_Data BMS;
    JK_BMS_RS485_Data receivedRawData;
    // ///////////////////////////////////////for testing

    HardwareSerial JKBMS_RS485_Port2 (2);

    byte receivedBytes_main[320];
    int receivedLength = 0;
    unsigned long receivingtimer = 0;
    const int receivingtime500 = 500; // 0.5 Sekunde
    int ndx = 0;
    void DataAnalysis();
    // ///////////////////////////////for testing

    char current_clamp_ip[40] = "192.168.188.127";
    char current_clamp_cmd[40] = "/cm?cmnd=status+10";
    char sensor_resp[20] = "SML";               // or "MT175"
    char sensor_resp_power[20] = "DJ_TPWRCURR"; // or "P"

    uint16_t gui_PowerReserveCharger, gui_PowerReserveInv, gui_MaxPowerCharger, gui_MaxPowerInv;
    uint16_t gui_ActualSetPower, gui_ActualSetPowerCharger, gui_ActualSetPowerInv;
    uint16_t gui_SetMaxPowerInv, gui_SetMaxPowerCharger, gui_setMQTTIP, gui_setMQTTport, gui_testMQTT, gui_enableMQTT, gui_enableChange;
    uint16_t gui_GridPower, gui_ChargerVoltage, gui_ChargerCurrent, gui_ChargerPower;
    uint16_t gui_ChargerACVoltage, gui_ChargerACCurrent, gui_ChargerACPower, gui_ChargerACfreq;

    uint16_t guiVoltageCell0, guiVoltageCell1, guiVoltageCell2, guiVoltageCell3, guiVoltageCell4;
    uint16_t guiVoltageCell5, guiVoltageCell6, guiVoltageCell7, guiVoltageCell8, guiVoltageCell9;
    uint16_t guiVoltageCell10, guiVoltageCell11, guiVoltageCell12, guiVoltageCell13, guiVoltageCell14, guiVoltageCell15;
    uint16_t guiCapacity, guiSysWorkingTime, guiTotCapacity, guiChargeCurrent, guiLog;
    uint16_t guiSOC, guiBattVoltage, guiBattStatus, guiCellDelta, guiAvgCellVoltage, guiCellCount;
    uint16_t guiMOST, guiT1, guiT2;

    // Most UI elements are assigned this generic callback which prints some
    // basic information. Event types are defined in ESPUI.h
    void generalCallback(Control *sender, int type);

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

        ESPUI.setVerbosity(Verbosity::Quiet);
        // to prepare the filesystem
        // ESPUI.prepareFileSystem();

        // WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
        WiFiManager wm;
        wm.setWiFiAutoReconnect(true);

        // reset settings - wipe stored credentials for testing
        // these are stored by the esp library
        // wm.resetSettings();

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

        Huawei::setCurrent(0, true); // set 0 A as default
        Soyosource_init_RS485(Soyo_RS485_PORT_RX, Soyo_RS485_PORT_TX, Soyo_RS485_PORT_EN);
        Serial.println("Soyosource inverter RS485 setup done");

        // initialize RS485 power for JK_BMS
        // JKBMS_init_RS485(JKBMS_RS485_PORT_RX, JKBMS_RS485_PORT_TX, JKBMS_RS485_PORT_EN);

        /////////////////////////////////////////////////////////////
        pinMode(JKBMS_RS485_PORT_EN, OUTPUT);
        digitalWrite(JKBMS_RS485_PORT_EN, LOW);
        JKBMS_RS485_Port2.begin(115200, SERIAL_8N1, JKBMS_RS485_PORT_RX, JKBMS_RS485_PORT_TX, false, 500);
        
        ////////////////////////////////////////////////////////////

        Serial.println("JK-BMS RS485 setup done");

        PSclient.setServer(mqtt_server, atoi(mqtt_port));
        PSclient.setCallback(callback);
        if (g_EnableMQTT)
            reconnect();

        // initialize ESPUI
        uint16_t TabStatus = ESPUI.addControl(ControlType::Tab, "Status", "Status");
        uint16_t TabBatteryInfo = ESPUI.addControl(ControlType::Tab, "Info", "Info");
        uint16_t TabSettings = ESPUI.addControl(ControlType::Tab, "Settings", "Settings");

        ESPUI.addControl(ControlType::Separator, "Global", "", ControlColor::None, TabStatus);
        guiBattStatus = ESPUI.addControl(ControlType::Label, "Battery status", "0", ControlColor::Emerald, TabStatus);
        guiSOC = ESPUI.addControl(ControlType::Label, "SOC [%]", "0", ControlColor::Emerald, TabStatus);
        gui_ActualSetPower = ESPUI.addControl(ControlType::Label, "Actual Set Power [W]", "0", ControlColor::Emerald, TabStatus);
        gui_GridPower = ESPUI.addControl(ControlType::Label, "Actual Grid Power [W]", "0", ControlColor::Emerald, TabStatus);

        ESPUI.addControl(ControlType::Separator, "Charger", "", ControlColor::None, TabStatus);
        gui_ActualSetPowerCharger = ESPUI.addControl(ControlType::Label, "Set Power Charger [W]", "0", ControlColor::Emerald, TabStatus);
        gui_ChargerPower = ESPUI.addControl(ControlType::Label, "DC Power [W]", "0", ControlColor::Emerald, TabStatus);
        gui_ChargerVoltage = ESPUI.addControl(ControlType::Label, "DC Voltage [V]", "0", ControlColor::Emerald, TabStatus);
        gui_ChargerCurrent = ESPUI.addControl(ControlType::Label, "DC Current [A]", "0", ControlColor::Emerald, TabStatus);
        gui_ChargerACPower = ESPUI.addControl(ControlType::Label, "AC Power [W]", "0", ControlColor::Emerald, TabStatus);
        gui_ChargerACVoltage = ESPUI.addControl(ControlType::Label, "AC Voltage [V]", "0", ControlColor::Emerald, TabStatus);
        gui_ChargerACCurrent = ESPUI.addControl(ControlType::Label, "AC Current [A]", "0", ControlColor::Emerald, TabStatus);
        gui_ChargerACfreq = ESPUI.addControl(ControlType::Label, "AC frequency [Hz]", "0", ControlColor::Emerald, TabStatus);

        ESPUI.addControl(ControlType::Separator, "Inverter", "", ControlColor::None, TabStatus);
        gui_ActualSetPowerInv = ESPUI.addControl(ControlType::Label, "Actual Power Inverter [W]", "0", ControlColor::Emerald, TabStatus);

        ESPUI.addControl(ControlType::Separator, "BMS", "", ControlColor::None, TabStatus);
        guiBattVoltage = ESPUI.addControl(ControlType::Label, "Battery Voltage [V]", "0", ControlColor::Emerald, TabStatus);
        guiChargeCurrent = ESPUI.addControl(ControlType::Label, "Current [A]", "0", ControlColor::Emerald, TabStatus);
        guiAvgCellVoltage = ESPUI.addControl(ControlType::Label, "Avg.Cell Voltage [V]", "0", ControlColor::Emerald, TabStatus);
        guiCellDelta = ESPUI.addControl(ControlType::Label, "Cell delta [V]", "0", ControlColor::Emerald, TabStatus);
        guiMOST = ESPUI.addControl(ControlType::Label, "MOS Temperature [°C]", "0", ControlColor::Emerald, TabStatus);
        guiT1 = ESPUI.addControl(ControlType::Label, "T1 [°C]", "0", ControlColor::Emerald, TabStatus);
        guiT2 = ESPUI.addControl(ControlType::Label, "T2 [°C]", "0", ControlColor::Emerald, TabStatus);

        // Battery Info Tab
        ESPUI.addControl(ControlType::Separator, "Battery Infos", "", ControlColor::None, TabBatteryInfo);
        guiCapacity = ESPUI.addControl(ControlType::Label, "Nominal capacity [Ah]", "0", ControlColor::Emerald, TabBatteryInfo);
        guiCellCount = ESPUI.addControl(ControlType::Label, "Cell count", "0", ControlColor::Emerald, TabBatteryInfo);
        guiSysWorkingTime = ESPUI.addControl(ControlType::Label, "Working time", "0", ControlColor::Emerald, TabBatteryInfo);
        guiTotCapacity = ESPUI.addControl(ControlType::Label, "Cycle Capacity", "0", ControlColor::Emerald, TabBatteryInfo);

        ESPUI.addControl(ControlType::Separator, "Cell Voltages [V]", "", ControlColor::None, TabBatteryInfo);
        guiVoltageCell0 = ESPUI.addControl(ControlType::Label, "Cell 0", "0", ControlColor::Emerald, TabBatteryInfo);
        guiVoltageCell1 = ESPUI.addControl(ControlType::Label, "Cell 1", "0", ControlColor::Emerald, TabBatteryInfo);
        guiVoltageCell2 = ESPUI.addControl(ControlType::Label, "Cell 2", "0", ControlColor::Emerald, TabBatteryInfo);
        guiVoltageCell3 = ESPUI.addControl(ControlType::Label, "Cell 3", "0", ControlColor::Emerald, TabBatteryInfo);
        guiVoltageCell4 = ESPUI.addControl(ControlType::Label, "Cell 4", "0", ControlColor::Emerald, TabBatteryInfo);
        guiVoltageCell5 = ESPUI.addControl(ControlType::Label, "Cell 5", "0", ControlColor::Emerald, TabBatteryInfo);
        guiVoltageCell6 = ESPUI.addControl(ControlType::Label, "Cell 6", "0", ControlColor::Emerald, TabBatteryInfo);
        guiVoltageCell7 = ESPUI.addControl(ControlType::Label, "Cell 7", "0", ControlColor::Emerald, TabBatteryInfo);
        guiVoltageCell8 = ESPUI.addControl(ControlType::Label, "Cell 8", "0", ControlColor::Emerald, TabBatteryInfo);
        guiVoltageCell9 = ESPUI.addControl(ControlType::Label, "Cell 9", "0", ControlColor::Emerald, TabBatteryInfo);
        guiVoltageCell10 = ESPUI.addControl(ControlType::Label, "Cell 10", "0", ControlColor::Emerald, TabBatteryInfo);
        guiVoltageCell11 = ESPUI.addControl(ControlType::Label, "Cell 11", "0", ControlColor::Emerald, TabBatteryInfo);
        guiVoltageCell12 = ESPUI.addControl(ControlType::Label, "Cell 12", "0", ControlColor::Emerald, TabBatteryInfo);
        guiVoltageCell13 = ESPUI.addControl(ControlType::Label, "Cell 13", "0", ControlColor::Emerald, TabBatteryInfo);
        guiVoltageCell14 = ESPUI.addControl(ControlType::Label, "Cell 14", "0", ControlColor::Emerald, TabBatteryInfo);
        guiVoltageCell15 = ESPUI.addControl(ControlType::Label, "Cell 15", "0", ControlColor::Emerald, TabBatteryInfo);

        ESPUI.addControl(ControlType::Separator, "(Dis-)Charge Settings", "", ControlColor::None, TabBatteryInfo);
        gui_MaxPowerCharger = ESPUI.addControl(ControlType::Label, "Max Power Charger [W]", "0", ControlColor::Emerald, TabBatteryInfo);
        gui_MaxPowerInv = ESPUI.addControl(ControlType::Label, "Max Power Inverter[W]", "0", ControlColor::Emerald, TabBatteryInfo);
        gui_PowerReserveCharger = ESPUI.addControl(ControlType::Label, "Power Reserve Charger [W]", "0", ControlColor::Emerald, TabBatteryInfo);
        gui_PowerReserveInv = ESPUI.addControl(ControlType::Label, "Power Reserve Inverter [W]", "0", ControlColor::Emerald, TabBatteryInfo);

        // Settings tab
        gui_enableChange = ESPUI.addControl(ControlType::Switcher, "Edit", "", ControlColor::Alizarin, TabSettings, generalCallback);
        ESPUI.addControl(ControlType::Separator, "Management setting", "", ControlColor::None, TabSettings);
        gui_SetMaxPowerInv = ESPUI.addControl(Slider, "Max Power Inverter", "200", Alizarin, TabSettings, generalCallback);
        ESPUI.addControl(Min, "", "0", None, gui_SetMaxPowerInv);
        ESPUI.addControl(Max, "", "800", None, gui_SetMaxPowerInv);
        gui_SetMaxPowerCharger = ESPUI.addControl(Slider, "Max Power Charger", "2000", Alizarin, TabSettings, generalCallback);
        ESPUI.addControl(Min, "", "0", None, gui_SetMaxPowerCharger);
        ESPUI.addControl(Max, "", "4000", None, gui_SetMaxPowerCharger);

        ESPUI.addControl(ControlType::Separator, "Network settings", "", ControlColor::None, TabSettings);
        gui_setMQTTIP = ESPUI.addControl(Text, "MQTT Server:", mqtt_server, Alizarin, TabSettings, generalCallback);
        gui_setMQTTport = ESPUI.addControl(Text, "MQTT port:", mqtt_port, Alizarin, TabSettings, generalCallback);
        gui_testMQTT = ESPUI.addControl(Button, "Test MQTT", "push", Alizarin, TabSettings, generalCallback);
        gui_enableMQTT = ESPUI.addControl(ControlType::Switcher, "Enable MQTT", "", ControlColor::Alizarin, TabSettings, generalCallback);

        // disable editing on settings page
        ESPUI.updateSwitcher(gui_enableChange, false);
        ESPUI.updateSwitcher(gui_enableMQTT, g_EnableMQTT);
        ESPUI.setEnabled(gui_setMQTTIP, false);
        ESPUI.setEnabled(gui_setMQTTport, false);
        ESPUI.setEnabled(gui_SetMaxPowerCharger, false);
        ESPUI.setEnabled(gui_SetMaxPowerInv, false);
        ESPUI.setEnabled(gui_testMQTT, false);
        ESPUI.setEnabled(gui_enableMQTT, false);

        // start ESPUI
        ESPUI.begin("Battery Management");

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
        // shifted to core 1
        /*        int packetSize = CAN.parsePacket();
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
                    serverClient.stop();   */

        if ((millis() - g_Time500) > 500)
        {

            g_Time500 = millis();
        }

        if ((millis() - g_Time1000) > 1000)
        {
            Huawei::every1000ms();
            // PSclient.loop();

            // update the value for the inverter every second
            sendpower2soyo(ActualSetPowerInv, Soyo_RS485_PORT_EN);

            g_Time1000 = millis();
        }

        if ((millis() - g_Time5000) > 5000)
        {
            Huawei::sendGetData(0x00);
            Huawei::HuaweiInfo &info = Huawei::g_PSU;

            /*            // get the BMS data
                        receivedRawData = JKBMS_read_data(JKBMS_RS485_PORT_EN);
                        // BMS = JKBMS_DataAnalysis(&receivedRawData.data, receivedRawData.length);
                        BMS = JKBMS_DataAnalysis2(receivedRawData);
            */

            ///////////////////////////////////////////////////////////////

            digitalWrite(JKBMS_RS485_PORT_EN, HIGH);
            // char hex[5];
            ndx = 0;
            delay(10);
            JKBMS_RS485_Port2.write(ReadAllData, sizeof(ReadAllData));
            JKBMS_RS485_Port2.flush();
            receivingtimer = millis();
            digitalWrite(JKBMS_RS485_PORT_EN, LOW); // set RS485 to receive
            delay(10);
            while ((receivingtimer + 300) > millis())          // anstelle von 1000, receivingtime500
            {
                if (JKBMS_RS485_Port2.available() && ndx < 320)
                {
                    receivedBytes_main[ndx] = JKBMS_RS485_Port2.read();
                    // sprintf(hex, "%02X", receivedBytes_main[ndx]);
                    // Serial.print(hex);
                    // Serial.print(" ");
                    ndx++;
                }
                receivedLength = ndx;
            }
            DataAnalysis();

            /////////////////////////////////////////////////////////////////

            // reads actual grid power every 5 seconds
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
            sendpower2soyo(ActualSetPowerInv, Soyo_RS485_PORT_EN);
            Huawei::setVoltage(ActualSetVoltage, 0x00, false);
            ActualSetCurrent = ActualSetPowerCharger / ActualSetVoltage;
            Huawei::setCurrent(ActualSetCurrent, false);

            // update WEB-GUI
            ESPUI.updateLabel(gui_ActualSetPower, String(ActualSetPower) + "W");
            ESPUI.updateLabel(gui_ActualSetPowerCharger, String(ActualSetPowerCharger) + "W");
            ESPUI.updateLabel(gui_ActualSetPowerInv, String(ActualSetPowerInv) + "W");
            ESPUI.updateLabel(gui_MaxPowerCharger, String(MaxPowerCharger) + "W");
            ESPUI.updateLabel(gui_MaxPowerInv, String(MaxPowerInv) + "W");
            ESPUI.updateLabel(gui_PowerReserveCharger, String(PowerReserveCharger) + "W");
            ESPUI.updateLabel(gui_PowerReserveInv, String(PowerReserveInv) + "W");
            ESPUI.updateLabel(gui_GridPower, String(ActualPower) + "W");
            ESPUI.updateLabel(gui_ChargerVoltage, String(ActualVoltage) + "V");
            ESPUI.updateLabel(gui_ChargerCurrent, String(ActualCurrent) + "A");
            ESPUI.updateLabel(gui_ChargerPower, String(Huawei::g_PSU.output_power) + "W");
            ESPUI.updateLabel(gui_ChargerACVoltage, String(Huawei::g_PSU.input_voltage) + "V");
            ESPUI.updateLabel(gui_ChargerACCurrent, String(info.input_current) + "A");
            ESPUI.updateLabel(gui_ChargerACPower, String(info.input_power) + "W");
            ESPUI.updateLabel(gui_ChargerACfreq, String(info.input_freq) + "Hz");

            // update the BMS values also only every 5 sec

            ESPUI.updateLabel(guiVoltageCell0, String(BMS.cellVoltage[0], 3) + "V");
            ESPUI.updateLabel(guiVoltageCell1, String(BMS.cellVoltage[1], 3) + "V");
            ESPUI.updateLabel(guiVoltageCell2, String(BMS.cellVoltage[2], 3) + "V");
            ESPUI.updateLabel(guiVoltageCell3, String(BMS.cellVoltage[3], 3) + "V");
            ESPUI.updateLabel(guiVoltageCell4, String(BMS.cellVoltage[4], 3) + "V");
            ESPUI.updateLabel(guiVoltageCell5, String(BMS.cellVoltage[5], 3) + "V");
            ESPUI.updateLabel(guiVoltageCell6, String(BMS.cellVoltage[6], 3) + "V");
            ESPUI.updateLabel(guiVoltageCell7, String(BMS.cellVoltage[7], 3) + "V");
            ESPUI.updateLabel(guiVoltageCell8, String(BMS.cellVoltage[8], 3) + "V");
            ESPUI.updateLabel(guiVoltageCell9, String(BMS.cellVoltage[9], 3) + "V");
            ESPUI.updateLabel(guiVoltageCell10, String(BMS.cellVoltage[10], 3) + "V");
            ESPUI.updateLabel(guiVoltageCell11, String(BMS.cellVoltage[11], 3) + "V");
            ESPUI.updateLabel(guiVoltageCell12, String(BMS.cellVoltage[12], 3) + "V");
            ESPUI.updateLabel(guiVoltageCell13, String(BMS.cellVoltage[13], 3) + "V");
            ESPUI.updateLabel(guiVoltageCell14, String(BMS.cellVoltage[14], 3) + "V");
            ESPUI.updateLabel(guiVoltageCell15, String(BMS.cellVoltage[15], 3) + "V");

            ESPUI.updateLabel(guiSOC, String(BMS.SOC) + "%");
            ESPUI.updateLabel(guiBattStatus, String(BMS.sBatteryStatus));
            ESPUI.updateLabel(guiBattVoltage, String(BMS.Battery_Voltage, 3) + "V");
            ESPUI.updateLabel(guiChargeCurrent, String(BMS.Charge_Current, 3) + "A");
            ESPUI.updateLabel(guiAvgCellVoltage, String(BMS.Battery_Voltage / BMS.CellCount, 3) + "V");
            ESPUI.updateLabel(guiCellDelta, String(BMS.Delta_Cell_Voltage, 3) + "V");
            ESPUI.updateLabel(guiMOST, String(BMS.MOS_Temp, 0) + "°C");
            ESPUI.updateLabel(guiT1, String(BMS.Battery_T1, 0) + "°C");
            ESPUI.updateLabel(guiT2, String(BMS.Battery_T2, 0) + "°C");

            ESPUI.updateLabel(guiCapacity, String(BMS.Nominal_Capacity) + "Ah");
            ESPUI.updateLabel(guiSysWorkingTime, String(BMS.days) + " days " + String(BMS.hr) + ":" + String(BMS.mi));
            //    char str[8];
            //    sprintf(str, "%X", BMS.Uptime);
            //    ESPUI.updateLabel(guiSysWorkingTime, str);
            ESPUI.updateLabel(guiTotCapacity, String(BMS.totBattCycleCapacity) + "Ah");
            ESPUI.updateLabel(guiCellCount, String(BMS.CellCount));

            if (g_EnableMQTT)
            {
                if (PSclient.connected()) // only send data, if the server is connected
                {
                    sprintf(temp_char, "%d", BMS.SOC);
                    sprintf(mqtt_topic, "%s/Data/SOC", ESP_Hostname);
                    PSclient.publish(mqtt_topic, temp_char);

                    sprintf(temp_char, "%.3f", BMS.Battery_Voltage);
                    sprintf(mqtt_topic, "%s/Data/Battery_Voltage2", ESP_Hostname);
                    PSclient.publish(mqtt_topic, temp_char);

                    sprintf(temp_char, "%.3f", BMS.Charge_Current);
                    sprintf(mqtt_topic, "%s/Data/Charge_Current2", ESP_Hostname);
                    PSclient.publish(mqtt_topic, temp_char);

                    sprintf(temp_char, "%.3f", BMS.Delta_Cell_Voltage);
                    sprintf(mqtt_topic, "%s/Data/Delta_Cell_Voltage2", ESP_Hostname);
                    PSclient.publish(mqtt_topic, temp_char);

                    sprintf(temp_char, "%.3f", BMS.MOS_Temp);
                    sprintf(mqtt_topic, "%s/Data/MOS_Temp", ESP_Hostname);
                    PSclient.publish(mqtt_topic, temp_char);

                    sprintf(temp_char, "%.3f", BMS.Battery_T1);
                    sprintf(mqtt_topic, "%s/Data/Battery_T1", ESP_Hostname);
                    PSclient.publish(mqtt_topic, temp_char);

                    sprintf(temp_char, "%.3f", BMS.Battery_T2);
                    sprintf(mqtt_topic, "%s/Data/Battery_T2", ESP_Hostname);
                    PSclient.publish(mqtt_topic, temp_char);
                }
            }

            /*
                        Serial.print(ActualPower);
                        Serial.print("   ");
                        Serial.print(ActualSetPower);
                        Serial.print("   ");
                        Serial.print(ActualSetPowerInv);
                        Serial.print("   ");
                        Serial.print(ActualSetPowerCharger);

                        Serial.println();
            */
            // char temp[60];
            // dtostrf(ActualSetCurrent,2,2,temp);
            // sprintf(temp, "{ \"idx\" : 326, \"nvalue\" : 0, \"svalue\" : \"%.2f\" }", ActualSetCurrent);
            // PSclient.publish("domoticz/in", temp);
            // PSclient.loop();

            g_Time5000 = millis();
        }
    }

    // Most UI elements are assigned this generic callback which prints some
    // basic information. Event types are defined in ESPUI.h
    void generalCallback(Control *sender, int type)
    {
        if (sender->id == gui_SetMaxPowerInv)
        {
            MaxPowerInv = (sender->value).toInt();
        }
        if (sender->id == gui_SetMaxPowerCharger)
        {
            MaxPowerCharger = (sender->value).toInt();
        }
        if (sender->id == gui_setMQTTIP)
        {
            (sender->value).toCharArray(mqtt_server, ((sender->value).length() + 1));
        }
        if (sender->id == gui_setMQTTport)
        {
            (sender->value).toCharArray(mqtt_port, ((sender->value).length() + 1));
        }
        if (sender->id == gui_enableMQTT)
        {
            if (type == S_ACTIVE)
            {
                g_EnableMQTT = true;
                reconnect();
            }
            if (type == S_INACTIVE)
            {
                g_EnableMQTT = false;
                PSclient.disconnect();
            }
        }
        if (sender->id == gui_testMQTT)
        {
            if (type == B_DOWN)
            {
                ESPUI.updateButton(gui_testMQTT, "failed");
                reconnect();
                if (PSclient.connected())
                {
                    ESPUI.updateButton(gui_testMQTT, "OK");
                    if (!g_EnableMQTT)
                    { // is the general switch is disabled, than disconnect
                        PSclient.disconnect();
                    }
                    else if (!PSclient.connected())
                    {
                        g_EnableMQTT = false;
                        ESPUI.updateSwitcher(gui_enableMQTT, g_EnableMQTT);
                    }
                }
            }
        }
        if (sender->id == gui_enableChange)
        {
            if (type == S_ACTIVE)
            {
                ESPUI.setEnabled(gui_setMQTTIP, true);
                ESPUI.setEnabled(gui_setMQTTport, true);
                ESPUI.setEnabled(gui_SetMaxPowerCharger, true);
                ESPUI.setEnabled(gui_SetMaxPowerInv, true);
                ESPUI.setEnabled(gui_testMQTT, true);
                ESPUI.setEnabled(gui_enableMQTT, true);
            }
            if (type == S_INACTIVE)
            {
                ESPUI.setEnabled(gui_setMQTTIP, false);
                ESPUI.setEnabled(gui_setMQTTport, false);
                ESPUI.setEnabled(gui_SetMaxPowerCharger, false);
                ESPUI.setEnabled(gui_SetMaxPowerInv, false);
                ESPUI.setEnabled(gui_testMQTT, false);
                ESPUI.setEnabled(gui_enableMQTT, false);
            }
        }
        /*
        // ESPUI.setEnabled(controlId, enabled)

            Serial.print("CB: id(");
            Serial.print(sender->id);
            Serial.print(") Type(");
            Serial.print(type);
            Serial.print(") '");
            Serial.print(sender->label);
            Serial.print("' = ");
            Serial.println(sender->value);
        */
    }

    //////////////////////////////////////////////////////

    // interpret the data from the JK-BMS
    void DataAnalysis()
    {
        float min = 4, max = 0; // to store the min an max cell voltage
        uint16_t temp;

        for (uint16_t i = 11; i < receivedLength - 5;)
        {
            switch (receivedBytes_main[i])
            {
            case 0x79: // Cell voltage
                BMS.CellCount = receivedBytes_main[i + 1] / 3;
                i += 2;
                // Serial.print("Cell Voltages = ");
                for (uint8_t n = 0; n < BMS.CellCount; n++)
                {
                    BMS.cellVoltage[n] = ((receivedBytes_main[i + 1] << 8) | receivedBytes_main[i + 2]) * 0.001;
                    if (min > BMS.cellVoltage[n])
                    {
                        min = BMS.cellVoltage[n];
                    }
                    if (max < BMS.cellVoltage[n])
                    {
                        max = BMS.cellVoltage[n];
                    }
                    // Serial.print(cellVoltage[n], 3);
                    // n<15 ? Serial.print("V, ") : Serial.println("V");
                    i += 3;
                }
                BMS.Delta_Cell_Voltage = max - min;
                break;

            case 0x80: // Read tube temp.
                BMS.MOS_Temp = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]);
                if (BMS.MOS_Temp > 100)
                {
                    BMS.MOS_Temp = -(BMS.MOS_Temp - 100);
                }
                // Serial.print("MOS Temp = ");
                // Serial.print(MOS_Temp, 3);
                // Serial.println("C");
                i += 3;
                break;

            case 0x81: // Battery inside temp
                BMS.Battery_T1 = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]);
                if (BMS.Battery_T1 > 100)
                {
                    BMS.Battery_T1 = -(BMS.Battery_T1 - 100);
                }
                // Serial.print("T1 = ");
                // Serial.print(Battery_T1, 3);
                // Serial.println("C");

                i += 3;
                break;

            case 0x82: // Battery temp
                BMS.Battery_T2 = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]);
                if (BMS.Battery_T2 > 100)
                {
                    BMS.Battery_T2 = -(BMS.Battery_T2 - 100);
                }
                // Serial.print("T2 = ");
                // Serial.print(Battery_T2, 3);
                // Serial.println("C");

                i += 3;
                break;

            case 0x83: // Total Batery Voltage
                BMS.Battery_Voltage = (float)(((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]) * 0.01);
                i += 3;
                // Serial.print("Battery Voltage = ");
                // Serial.print(Battery_Voltage, 3);
                // Serial.println("V");

                break;

            case 0x84: // Current
                temp = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]);
                if (temp >> 15)
                { // Charge current; if the highest bit 1 than charging
                    BMS.Charge_Current = (float)(temp - 32768) * 0.01;
                }
                else
                    BMS.Charge_Current = (float)(temp) * (-0.01); // Else it is the discharge current
                i += 3;
                break;

            case 0x85:                               // Remaining Battery Capacity
                BMS.SOC = receivedBytes_main[i + 1]; // in %
                i += 2;
                break;

            case 0x86:                                         // Number of Battery Temperature Sensors
                BMS.TempSensCount = receivedBytes_main[i + 1]; // in %
                i += 2;
                break;

            case 0x87: // Cycle
                BMS.Cycle_Count = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]);
                i += 3;
                break;

                // 0x88 does not exist

            case 0x89: // Total Battery cycle Capacity
                BMS.totBattCycleCapacity = (((uint16_t)receivedBytes_main[i + 1] << 24 | receivedBytes_main[i + 2] << 16 | receivedBytes_main[i + 3] << 8 | receivedBytes_main[i + 4]));
                i += 5;
                break;

            case 0x8a: // Total number of battery strings
                BMS.CellCount = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]);
                i += 3;
                break;

            case 0x8b: // Battery_Warning_Massage

                /*bmsErrors                                           | JBD | JK |
                bit0  single cell overvoltage protection              |  x  |    |
                bit1  single cell undervoltage protection             |  x  |    |
                bit2  whole pack overvoltage protection (Batterie)    |  x  | x  |
                bit3  Whole pack undervoltage protection (Batterie)   |  x  | x  |
                bit4  charging over-temperature protection            |  x  |    |
                bit5  charging low temperature protection             |  x  |    |
                bit6  Discharge over temperature protection           |  x  |    |
                bit7  discharge low temperature protection            |  x  |    |
                bit8  charging overcurrent protection                 |  x  | x  |
                bit9  Discharge overcurrent protection                |  x  | x  |
                bit10 short circuit protection                        |  x  |    |
                bit11 Front-end detection IC error                    |  x  |    |
                bit12 software lock MOS                               |  x  |    |
                */

                /* JKBMS
                Bit 0:  Low capacity alarm
                Bit 1:  MOS tube overtemperature alarm
                Bit 2:  Charge over voltage alarm
                Bit 3:  Discharge undervoltage alarm
                Bit 4:  Battery overtemperature alarm
                Bit 5:  Charge overcurrent alarm                        -> Bit 8
                Bit 6:  discharge over current alarm                    -> Bit 9
                Bit 7:  core differential pressure alarm
                Bit 8:  overtemperature alarm in the battery box
                Bit 9:  Battery low temperature
                Bit 10: Unit overvoltage                                -> Bit 2
                Bit 11: Unit undervoltage                               -> Bit 3
                Bit 12: 309_A protection
                Bit 13: 309_B protection
                Bit 14: reserved
                Bit 15: Reserved
                */

                BMS.WarningMsgWord = (((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]));
                i += 3;
                break;

            case 0x8c: // Battery status information
                /* JKBMS
                Bit 0:  charging on
                Bit 1:  discharge on
                Bit 2:  equilization on
                Bit 3:  battery down
                Bit 4..15: reserved
                */
                BMS.BatteryStatus = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]);
                i += 3;
                strcpy(BMS.sBatteryStatus, "undefined");
                strcpy(BMS.charge, "off");
                strcpy(BMS.discharge, "off");
                strcpy(BMS.balance, "off");

                if ((BMS.BatteryStatus >> 3) & 0x01)
                { // looking for bit 3
                    strcpy(BMS.sBatteryStatus, "Battery Down");
                }
                else
                {
                    if (BMS.BatteryStatus & 0x01)
                    { // looking for bit 0
                        strcpy(BMS.charge, "on ");
                    }
                    else
                        strcpy(BMS.charge, "off");

                    strcpy(BMS.sBatteryStatus, "Charge:");
                    strncat(BMS.sBatteryStatus, BMS.charge, 3);

                    if ((BMS.BatteryStatus >> 1) & 0x01)
                    { // looking for bit 1
                        strcpy(BMS.discharge, "on ");
                    }
                    else
                        strcpy(BMS.discharge, "off");

                    strncat(BMS.sBatteryStatus, " Discharge:", 12);
                    strncat(BMS.sBatteryStatus, BMS.discharge, 3);

                    if ((BMS.BatteryStatus >> 2) & 0x01)
                    { // looking for bit 2
                        strcpy(BMS.balance, "on ");
                    }
                    else
                        strcpy(BMS.charge, "off");

                    strncat(BMS.sBatteryStatus, " Balance:", 10);
                    strncat(BMS.sBatteryStatus, BMS.balance, 3);
                }

                break;

            case 0x8e: // Total voltage overvoltage protection
                BMS.OVP = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]) * 0.001;
                i += 3;
                break;

            case 0x8f: // Total voltage undervoltage protection
                BMS.UVP = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]) * 0.001;
                i += 3;
                break;

            case 0x90: // Single overvoltage protection voltage
                BMS.sOVP = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]);
                i += 3;
                break;

            case 0x91: // Cell overvoltage recovery voltage
                BMS.sOVPR = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]);
                i += 3;
                break;

            case 0x92:                                                                                   // Single overvoltage protection delay
                BMS.sOVP_delay = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]); // in seconds
                i += 3;
                break;

            case 0x93: // Single undervoltage protection voltage
                BMS.sUVP = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]);
                i += 3;
                break;

            case 0x94: // Monomer undervoltage recovery voltage
                BMS.sUVPR = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]);
                i += 3;
                break;

            case 0x95:                                                                                   // Single undervoltage protection delay
                BMS.sUVP_delay = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]); // in seconds
                i += 3;
                break;

            case 0x96:                                                                                  // Cell voltage?pressure difference protection value
                BMS.sVoltDiff = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]); // in seconds
                i += 3;
                break;

            case 0x97:                                                                                     // Discharge overcurrent protection value
                BMS.dischargeOCP = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]); // in Ampere
                i += 3;
                break;

            case 0x98:                                                                                           // Discharge overcurrent delay
                BMS.dischargeOCP_delay = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]); // in seconds
                i += 3;
                break;

            case 0x99:                                                                                  // Charge overcurrent protection value
                BMS.chargeOCP = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]); // in Ampere
                i += 3;
                break;

            case 0x9a:                                                                                        // Discharge overcurrent delay
                BMS.chargeOCP_delay = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]); // in seconds
                i += 3;
                break;

            case 0x9b:                                                                                            // Balanced starting voltage
                BMS.BalanceStartVoltage = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]); // in Voltage
                i += 3;
                break;

            case 0x9c:                                                                                       // Balanced opening pressure difference
                BMS.BalanceVoltage = ((uint16_t)receivedBytes_main[i + 1] << 8 | receivedBytes_main[i + 2]); // in AVoltagempere
                i += 3;
                break;

            case 0x9d: // Active balance switch
                i += 2;
                break;

            case 0x9e: // Power tube temperature protection value
                i += 3;
                break;

            case 0x9f: // Power tube temperature recovery value
                i += 3;
                break;

            case 0xa0: // Temperature protection value in the battery box
                i += 3;
                break;

            case 0xa1: // Temperature recovery value in the battery box
                i += 3;
                break;

            case 0xa2: // Battery temperature difference protection value
                i += 3;
                break;

            case 0xa3: // Battery charging high temperature protection value
                i += 3;
                break;

            case 0xa4: // Battery discharge high temperature protection value
                i += 3;
                break;

            case 0xa5: // Charging low temperature protection value
                i += 3;
                break;

            case 0xa6: // Charging low temperature protection recovery value
                i += 3;
                break;
            case 0xa7:
                i += 3;
                break;
            case 0xa8:
                i += 3;
                break;
            case 0xa9: // Battery string setting / Cell count
                i += 2;
                break;

            case 0xaa: // Battery capacity setting; in AH
                BMS.Nominal_Capacity = (((uint16_t)receivedBytes_main[i + 1] << 24 | receivedBytes_main[i + 2] << 16 | receivedBytes_main[i + 3] << 8 | receivedBytes_main[i + 4]));
                i += 5;
                // Serial.print("Battery capacity = ");
                // Serial.print(Nominal_Capacity);
                // Serial.println("Ah");
                break;

            case 0xab:
                i += 2;
                break;
            case 0xac:
                i += 2;
                break;
            case 0xad:
                i += 3;
                break;
            case 0xae:
                i += 2;
                break;
            case 0xaf:
                i += 2;
                break;
            case 0xb0:
                i += 3;
                break;

            case 0xb5: // Date of manufacture
                i += 5;
                break;

            case 0xb6: // System working time in MINUTES
                BMS.Uptime = (((uint16_t)receivedBytes_main[i + 1] << 24 | receivedBytes_main[i + 2] << 16 | receivedBytes_main[i + 3] << 8 | receivedBytes_main[i + 4]));
                //        sec = Uptime % 60;
                //        sec = 0;
                //        Uptime /= 60;
                /*        mi = Uptime % 60;
                        Uptime /= 60;
                        hr = Uptime % 24;
                        days = Uptime /= 24;    */
                i += 5;
                break;

            default:
                i++;
                break;
            }
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
