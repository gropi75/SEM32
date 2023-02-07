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

#include "PowerFunctions.h"
#include "jkbms.h"
#include "secrets.h"
=======
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


    char temp_char[10]; // for temporary storage of strings values
    char *pointer_to_temp_char;
    float tempfloat;
    char mqtt_topic[60];

    // BMS values as structure
    JK_BMS_Data BMS;
    JK_BMS_RS485_Data receivedRawData;

    // byte receivedBytes_main[320];

    char current_clamp_ip[40] = "192.168.188.127";
    char current_clamp_cmd[40] = "/cm?cmnd=status+10";
    char sensor_resp[20] = "SML";               // or "MT175"
    char sensor_resp_power[20] = "DJ_TPWRCURR"; // or "P"

    uint16_t gui_PowerReserveCharger, gui_PowerReserveInv, gui_MaxPowerCharger, gui_MaxPowerInv, gui_SetVoltageCharger;
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

    // initialize ESPUI
    void GUI_init()
    {
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
        gui_SetVoltageCharger = ESPUI.addControl(Slider, "Charging voltage", "56.2", Alizarin, TabSettings, generalCallback);
        ESPUI.addControl(Min, "", "42", None, gui_SetVoltageCharger);
        ESPUI.addControl(Max, "", "58", None, gui_SetVoltageCharger);        

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
    }

    // update ESPUI
    void GUI_update()
    {
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
        ESPUI.updateLabel(gui_ChargerACCurrent, String(Huawei::g_PSU.input_current) + "A");
        ESPUI.updateLabel(gui_ChargerACPower, String(Huawei::g_PSU.input_power) + "W");
        ESPUI.updateLabel(gui_ChargerACfreq, String(Huawei::g_PSU.input_freq) + "Hz");

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

        Huawei::setCurrent(0, true); // set 0 A as default

        Soyosource_init_RS485(Soyo_RS485_PORT_RX, Soyo_RS485_PORT_TX, Soyo_RS485_PORT_EN);
        Serial.println("Soyosource inverter RS485 setup done");

        // initialize RS485 power for JK_BMS
        JKBMS_init_RS485(JKBMS_RS485_PORT_RX, JKBMS_RS485_PORT_TX, JKBMS_RS485_PORT_EN);
        Serial.println("JK-BMS RS485 setup done");

        // initialize MQTT, however only connect, if it is enabled
        PSclient.setServer(mqtt_server, atoi(mqtt_port));
        PSclient.setCallback(callback);
        if (g_EnableMQTT)
            reconnect();

        // initialize ESPUI
        GUI_init();

        // start ESPUI
        ESPUI.begin("Battery Management");



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

        // CAN handling and OTA shifted to core 1

        if ((millis() - g_Time500) > 500)
        {


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

            Huawei::sendGetData(0x00);
  //          Huawei::HuaweiInfo &info = Huawei::g_PSU;

            // get the BMS data
            receivedRawData = JKBMS_read_data(JKBMS_RS485_PORT_EN);
            // BMS = JKBMS_DataAnalysis(&receivedRawData.data, receivedRawData.length);
            BMS = JKBMS_DataAnalysis2(receivedRawData);

            // reads actual grid power every 5 seconds
            ActualPower = getActualPower(current_clamp_ip, current_clamp_cmd, sensor_resp, sensor_resp_power);
            ActualVoltage = Huawei::g_PSU.output_voltage;
//            ActualVoltage = info.output_voltage;
            ActualCurrent = Huawei::g_PSU.output_current;
//            ActualCurrent = info.output_current;


            Huawei::HuaweiInfo &info = Huawei::g_PSU;

            // reads actual power every 5 seconds
            ActualPower = getActualPower();
            ActualVoltage = info.output_voltage;
            ActualCurrent = info.output_current;



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

            //Huawei::setCurrent(2.0, false);

            GUI_update();

            if (g_EnableMQTT)
            {
                if (!PSclient.connected()) reconnect();
                if (PSclient.connected()) // only send data, if the server is connected
                {
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
                }
            }
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
        if (sender->id == gui_SetVoltageCharger)
        {
            ActualSetVoltage = (sender->value).toFloat();
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



}

void setup()
{
    Main::init();
}

void loop()
{
    Main::loop();
}
