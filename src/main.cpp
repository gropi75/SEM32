/*
Project homepage: https://github.com/gropi75/SEM32

*/

// #define test_debug  // uncomment to just test without equipment
// #define wifitest    // uncomment to debug wifi status

#ifndef BSC_Hardware          // for SEM32 HW
#define Soyo_RS485_PORT_TX 18 // GPIO18    -- DI
#define Soyo_RS485_PORT_RX 19 // GPIO19    -- RO
#define Soyo_RS485_PORT_EN 23 // GPIO23    -- DE/RE

#define JKBMS_RS485_PORT_TX 25 // GPIO25    -- DI
#define JKBMS_RS485_PORT_RX 26 // GPIO26    -- RO
#define JKBMS_RS485_PORT_EN 33 // GPIO33    -- DE/RE
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
#endif

#define FORMAT_LITTLEFS_IF_FAILED true
// You only need to format the filesystem once
// #define FORMAT_FILESYSTEM       true
#define FORMAT_FILESYSTEM false


#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <CAN.h>
#include <WiFiManager.h>       // https://github.com/tzapu/WiFiManager
#define WEBSERVER_H
#include <ESPAsyncWebServer.h> // https://github.com/lorol/ESPAsyncWebServer.git
#include <AsyncTCP.h>
#include "FS.h"
#include <LittleFS.h>
FS *filesystem = &LittleFS;
#define FileFS LittleFS
#define FS_Name "LittleFS"

#include <ESPUI.h>
#include <PubSubClient.h>
#include "huawei.h"
#include "commands.h"
#include "main.h"
#include "soyosource.h"
#include "PowerFunctions.h"
#include "jkbms.h"
#include "secrets.h"
#include "sunrise.h"

TaskHandle_t TaskCan;
int packetSize;

WiFiServer server(23);
WiFiClient serverClient; // for OTA
WiFiClient mqttclient;   // to connect to MQTT broker
PubSubClient PSclient(mqttclient);

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
    int target_SOC;   // [%]
    float BatteryCapacity= 4.6;    // [kWh]

    char date_today[9];    // = "yyyymmdd";
    char date_tomorrow[9]; // = "yyyymmdd";
    int date_dayofweek_today = 8;

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

    uint16_t gui_PowerReserveCharger, gui_PowerReserveInv, gui_MaxPowerCharger, gui_MaxPowerInv;
    uint16_t gui_ActualSetPower, gui_ActualSetPowerCharger, gui_ActualSetPowerInv, gui_DynPowerInv;
    uint16_t gui_SetMaxPowerInv, gui_SetMaxPowerCharger, gui_setMQTTIP, gui_setMQTTport, gui_testMQTT, gui_enableMQTT, gui_enableChange, gui_savesettings;
    uint16_t gui_GridPower, gui_ChargerVoltage, gui_ChargerCurrent, gui_ChargerPower;
    uint16_t gui_ChargerACVoltage, gui_ChargerACCurrent, gui_ChargerACPower, gui_ChargerACfreq;

    uint16_t guiVoltageCell0, guiVoltageCell1, guiVoltageCell2, guiVoltageCell3, guiVoltageCell4;
    uint16_t guiVoltageCell5, guiVoltageCell6, guiVoltageCell7, guiVoltageCell8, guiVoltageCell9;
    uint16_t guiVoltageCell10, guiVoltageCell11, guiVoltageCell12, guiVoltageCell13, guiVoltageCell14, guiVoltageCell15;
    uint16_t guiCapacity, guiSysWorkingTime, guiTotCapacity, guiChargeCurrent, guiLog;
    uint16_t guiSOC, guiBattVoltage, guiBattStatus, guiCellDelta, guiAvgCellVoltage, guiCellCount;
    uint16_t gui_today_sunrise, gui_today_sunset, gui_tomorrow_sunrise, gui_prediction;
    uint16_t guiMOST, guiT1, guiT2;
    uint16_t gui_resetsettings, gui_resetESP;
    uint16_t gui_enableManControl, gui_ManualSetPower, g_ManualSetPowerCharger, gui_enabledynamicload;
    uint16_t gui_SetPowerReserveCharger, gui_SetPowerReserveInverter, gui_BatPower, gui_PVprognosis;
    bool g_enableManualControl, g_enableDynamicload = false;
    bool wm_resetsetting = false;
    int MQTTdisconnect_counter = 0;

    // flag for saving data inside WifiManager
    bool shouldSaveConfig = true;

    // callback notifying us of the need to save config
    void saveConfigCallback()
    {
        Serial.println("Should save config");
        shouldSaveConfig = true;
    }

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
            delay(200);
        }
    }

    // initialize ESPUI
    void GUI_init()
    {
        uint16_t TabStatus = ESPUI.addControl(ControlType::Tab, "Status", "Status");
        uint16_t TabBatteryInfo = ESPUI.addControl(ControlType::Tab, "Info", "Info");
        uint16_t TabSettings = ESPUI.addControl(ControlType::Tab, "Settings", "Settings");

        //        ESPUI.addControl(ControlType::Separator, "Global", "", ControlColor::None, TabStatus);
        guiSOC = ESPUI.addControl(ControlType::Label, "SOC [%]", "0", ControlColor::Emerald, TabStatus);
        gui_GridPower = ESPUI.addControl(ControlType::Label, "Actual Grid Power [W]", "0", ControlColor::Emerald, TabStatus);
        gui_ChargerACPower = ESPUI.addControl(ControlType::Label, "Charger Power [W]", "0", ControlColor::Emerald, TabStatus);
        gui_ActualSetPowerInv = ESPUI.addControl(ControlType::Label, "Inverter Power [W]", "0", ControlColor::Emerald, TabStatus);
        gui_BatPower = ESPUI.addControl(ControlType::Label, "Battery Power [W]", "0", ControlColor::Emerald, TabStatus);

        // Battery Info Tab
        ESPUI.addControl(ControlType::Separator, "Battery Infos", "", ControlColor::None, TabBatteryInfo);
        guiBattStatus = ESPUI.addControl(ControlType::Label, "Battery status", "0", ControlColor::Emerald, TabBatteryInfo);
        guiCapacity = ESPUI.addControl(ControlType::Label, "Nominal capacity [Ah]", "0", ControlColor::Emerald, TabBatteryInfo);
        guiCellCount = ESPUI.addControl(ControlType::Label, "Cell count", "0", ControlColor::Emerald, TabBatteryInfo);
        guiSysWorkingTime = ESPUI.addControl(ControlType::Label, "Working time", "0", ControlColor::Emerald, TabBatteryInfo);
        guiTotCapacity = ESPUI.addControl(ControlType::Label, "Cycle Capacity", "0", ControlColor::Emerald, TabBatteryInfo);
        guiBattVoltage = ESPUI.addControl(ControlType::Label, "Battery Voltage [V]", "0", ControlColor::Emerald, TabBatteryInfo);
        guiChargeCurrent = ESPUI.addControl(ControlType::Label, "Current [A]", "0", ControlColor::Emerald, TabBatteryInfo);
        guiAvgCellVoltage = ESPUI.addControl(ControlType::Label, "Avg.Cell Voltage [V]", "0", ControlColor::Emerald, TabBatteryInfo);
        guiCellDelta = ESPUI.addControl(ControlType::Label, "Cell delta [V]", "0", ControlColor::Emerald, TabBatteryInfo);

        ESPUI.addControl(ControlType::Separator, "BMS", "", ControlColor::None, TabBatteryInfo);
        guiMOST = ESPUI.addControl(ControlType::Label, "MOS Temperature [°C]", "0", ControlColor::Emerald, TabBatteryInfo);
        guiT1 = ESPUI.addControl(ControlType::Label, "T1 [°C]", "0", ControlColor::Emerald, TabBatteryInfo);
        guiT2 = ESPUI.addControl(ControlType::Label, "T2 [°C]", "0", ControlColor::Emerald, TabBatteryInfo);

        ESPUI.addControl(ControlType::Separator, "Charger", "", ControlColor::None, TabBatteryInfo);
        gui_ActualSetPowerCharger = ESPUI.addControl(ControlType::Label, "Set Power Charger [W]", "0", ControlColor::Emerald, TabBatteryInfo);
        gui_ChargerPower = ESPUI.addControl(ControlType::Label, "DC Power [W]", "0", ControlColor::Emerald, TabBatteryInfo);
        gui_ChargerVoltage = ESPUI.addControl(ControlType::Label, "DC Voltage [V]", "0", ControlColor::Emerald, TabBatteryInfo);
        gui_ChargerCurrent = ESPUI.addControl(ControlType::Label, "DC Current [A]", "0", ControlColor::Emerald, TabBatteryInfo);
        gui_ChargerACVoltage = ESPUI.addControl(ControlType::Label, "AC Voltage [V]", "0", ControlColor::Emerald, TabBatteryInfo);
        gui_ChargerACCurrent = ESPUI.addControl(ControlType::Label, "AC Current [A]", "0", ControlColor::Emerald, TabBatteryInfo);
        gui_ChargerACfreq = ESPUI.addControl(ControlType::Label, "AC frequency [Hz]", "0", ControlColor::Emerald, TabBatteryInfo);

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

        ESPUI.addControl(ControlType::Separator, "Control values", "", ControlColor::None, TabBatteryInfo);
        gui_ActualSetPower = ESPUI.addControl(ControlType::Label, "Actual Set Power [W]", "0", ControlColor::Emerald, TabBatteryInfo);

        ESPUI.addControl(ControlType::Separator, "(Dis-)Charge Settings", "", ControlColor::None, TabBatteryInfo);
        gui_MaxPowerCharger = ESPUI.addControl(ControlType::Label, "Max Power Charger [W]", "0", ControlColor::Emerald, TabBatteryInfo);
        gui_MaxPowerInv = ESPUI.addControl(ControlType::Label, "Max Power Inverter[W]", "0", ControlColor::Emerald, TabBatteryInfo);
        gui_DynPowerInv = ESPUI.addControl(ControlType::Label, "Dynamic Power Inverter[W]", "0", ControlColor::Emerald, TabBatteryInfo);
        gui_PowerReserveCharger = ESPUI.addControl(ControlType::Label, "Power Reserve Charger [W]", "0", ControlColor::Emerald, TabBatteryInfo);
        gui_PowerReserveInv = ESPUI.addControl(ControlType::Label, "Power Reserve Inverter [W]", "0", ControlColor::Emerald, TabBatteryInfo);

        ESPUI.addControl(ControlType::Separator, "Solar info", "", ControlColor::None, TabBatteryInfo);
        gui_prediction = ESPUI.addControl(ControlType::Label, "Power prediction tomorrow [W]", "0", ControlColor::Emerald, TabBatteryInfo);
        gui_today_sunrise = ESPUI.addControl(ControlType::Label, "Sunrise today", "0", ControlColor::Emerald, TabBatteryInfo);
        gui_today_sunset = ESPUI.addControl(ControlType::Label, "Sunset today", "0", ControlColor::Emerald, TabBatteryInfo);
        gui_tomorrow_sunrise = ESPUI.addControl(ControlType::Label, "Sunrise tomorrow", "0", ControlColor::Emerald, TabBatteryInfo);

        // Settings tab
        gui_enableChange = ESPUI.addControl(ControlType::Switcher, "Edit", "", ControlColor::Alizarin, TabSettings, generalCallback);
        gui_savesettings = ESPUI.addControl(Button, "Save settings", "push", Alizarin, TabSettings, generalCallback);
        ESPUI.addControl(ControlType::Separator, "Management setting", "", ControlColor::None, TabSettings);
        gui_SetMaxPowerInv = ESPUI.addControl(Slider, "Max Power Inverter", String(MaxPowerInv), Alizarin, TabSettings, generalCallback);
        ESPUI.addControl(Min, "", "0", None, gui_SetMaxPowerInv);
        ESPUI.addControl(Max, "", "800", None, gui_SetMaxPowerInv);
        gui_SetMaxPowerCharger = ESPUI.addControl(Slider, "Max Power Charger", String(MaxPowerCharger), Alizarin, TabSettings, generalCallback);
        ESPUI.addControl(Min, "", "0", None, gui_SetMaxPowerCharger);
        ESPUI.addControl(Max, "", "4000", None, gui_SetMaxPowerCharger);
        gui_SetPowerReserveCharger = ESPUI.addControl(Slider, "Power Reserve Inverter", String(PowerReserveInv), Alizarin, TabSettings, generalCallback);
        ESPUI.addControl(Min, "", "0", None, gui_SetPowerReserveCharger);
        ESPUI.addControl(Max, "", "50", None, gui_SetPowerReserveCharger);
        gui_SetPowerReserveInverter = ESPUI.addControl(Slider, "Power Reserve Charger", String(PowerReserveCharger), Alizarin, TabSettings, generalCallback);
        ESPUI.addControl(Min, "", "0", None, gui_SetPowerReserveInverter);
        ESPUI.addControl(Max, "", "50", None, gui_SetPowerReserveInverter);

        gui_enableManControl = ESPUI.addControl(ControlType::Switcher, "Enable manual control", "", ControlColor::Alizarin, TabSettings, generalCallback);
        gui_ManualSetPower = ESPUI.addControl(Slider, "Set charging power manually", "0", Alizarin, TabSettings, generalCallback);
        ESPUI.addControl(Min, "", "0", None, gui_ManualSetPower);
        ESPUI.addControl(Max, "", "4000", None, gui_ManualSetPower);
        gui_enabledynamicload = ESPUI.addControl(ControlType::Switcher, "Enable dynamic load", "", ControlColor::Alizarin, TabSettings, generalCallback);

        ESPUI.addControl(ControlType::Separator, "Network settings", "", ControlColor::None, TabSettings);
        gui_setMQTTIP = ESPUI.addControl(Text, "MQTT Server:", mqtt_server, Alizarin, TabSettings, generalCallback);
        gui_setMQTTport = ESPUI.addControl(Text, "MQTT port:", mqtt_port, Alizarin, TabSettings, generalCallback);
        gui_testMQTT = ESPUI.addControl(Button, "Test MQTT", "push", Alizarin, TabSettings, generalCallback);
        gui_enableMQTT = ESPUI.addControl(ControlType::Switcher, "Enable MQTT", "", ControlColor::Alizarin, TabSettings, generalCallback);
        gui_resetESP = ESPUI.addControl(Button, "Restart controller", "push", Alizarin, TabSettings, generalCallback);
        gui_resetsettings = ESPUI.addControl(Button, "Reset wifi settings", "push", Alizarin, TabSettings, generalCallback);

        // disable editing on settings page
        ESPUI.updateSwitcher(gui_enableChange, false);
        ESPUI.updateSwitcher(gui_enableManControl, false);
        ESPUI.updateSwitcher(gui_enableMQTT, g_EnableMQTT);
        ESPUI.setEnabled(gui_savesettings, false);
        ESPUI.setEnabled(gui_setMQTTIP, false);
        ESPUI.setEnabled(gui_setMQTTport, false);
        ESPUI.setEnabled(gui_SetMaxPowerCharger, false);
        ESPUI.setEnabled(gui_SetMaxPowerInv, false);
        ESPUI.setEnabled(gui_testMQTT, false);
        ESPUI.setEnabled(gui_enableMQTT, false);
        ESPUI.setEnabled(gui_enableManControl, false);
        ESPUI.setEnabled(gui_ManualSetPower, false);
        ESPUI.setEnabled(gui_resetESP, false);
        ESPUI.setEnabled(gui_resetsettings, false);
        ESPUI.setEnabled(gui_SetPowerReserveCharger, false);
        ESPUI.setEnabled(gui_SetPowerReserveInverter, false);
        ESPUI.setEnabled(gui_enabledynamicload, false);
    }

    // update ESPUI
    void GUI_update()
    {
        ESPUI.updateLabel(gui_ActualSetPower, String(ActualSetPower) + "W");
        ESPUI.updateLabel(gui_ActualSetPowerCharger, String(ActualSetPowerCharger) + "W");
        ESPUI.updateLabel(gui_ActualSetPowerInv, String(ActualSetPowerInv) + "W");
        ESPUI.updateLabel(gui_MaxPowerCharger, String(MaxPowerCharger) + "W");
        ESPUI.updateLabel(gui_MaxPowerInv, String(MaxPowerInv) + "W");
        ESPUI.updateLabel(gui_DynPowerInv, String(DynPowerInv) + "W");
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
        ESPUI.updateLabel(gui_ChargerACPower, String(Huawei::g_PSU.input_power) + "W");

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
        ESPUI.updateLabel(gui_BatPower, String(BMS.Battery_Power, 0) + "W");
        ESPUI.updateLabel(gui_prediction, String(solar_prognosis, 0) + "kW");
        ESPUI.updateLabel(gui_today_sunrise, String(myTime.sunrise_today, 2));
        ESPUI.updateLabel(gui_today_sunset, String(myTime.sunset_today, 2));
        ESPUI.updateLabel(gui_tomorrow_sunrise, String(myTime.sunrise_tomorrow, 2));

        ESPUI.updateLabel(guiCapacity, String(BMS.Nominal_Capacity) + "Ah");
        ESPUI.updateLabel(guiSysWorkingTime, String(BMS.days) + " days " + String(BMS.hr) + ":" + String(BMS.mi));
        //    char str[8];
        //    sprintf(str, "%X", BMS.Uptime);
        //    ESPUI.updateLabel(guiSysWorkingTime, str);
        ESPUI.updateLabel(guiTotCapacity, String(BMS.totBattCycleCapacity) + "Ah");
        ESPUI.updateLabel(guiCellCount, String(BMS.CellCount));
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
                if (MQTTdisconnect_counter > 10)                    // if reconnect fails several times, MQTT is disabled
                {
                    g_EnableMQTT = false;
                    ESPUI.updateSwitcher(gui_enableMQTT, g_EnableMQTT);
                    GUI_update();
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
        while (!Serial);
        //        pinMode(12, OUTPUT);
        //        pinMode(13, OUTPUT);
        //        pinMode(14, OUTPUT);
        //        pinMode(26, OUTPUT);

        //        digitalWrite(26, LOW);
        //        digitalWrite(14, HIGH);

        Serial.println("BOOTED!");
        ESPUI.setVerbosity(Verbosity::Quiet);
        // to prepare the filesystem
        // ESPUI.prepareFileSystem();

        // clean FS, for testing
        // LittleFS.format();

        // read configuration from FS json
        Serial.println("mounting FS...");

        if (LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED))
        {
            Serial.println("mounted file system");
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
        CAN.setPins(5, 4); // BSC Hardware is using different pinout
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

        // initialize and start ESPUI
        GUI_init();
        ESPUI.begin("Solar Energy Manager32");
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
            

            GUI_update();

            if (g_EnableMQTT)
            {
                if (!PSclient.connected())
                    reconnect();          // oif the server is disconnected, reconnect to it
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
            if (((millis() - g_Time30Min) > 7200000) || (date_dayofweek_today == 8)) // 7.200.000 ms = 2 hours, due to daily API call limits
#endif
            {
                TimeData(&myTime, lagmorning, lagevening, laenge, breite); // call the TimeData function with the parameters and the pointer to the TimeStruct instance

                // execute the following only once a day due to API limitations
                if (date_dayofweek_today != myTime.day_of_week)
                {
                    solar_prognosis = getSolarPrognosis(solarprognose_token, solarprognose_id, myTime.date_today, myTime.date_tomorrow);
                    date_dayofweek_today = myTime.day_of_week;
                }

                if (is_day != setPVstartflag(lagmorning, lagevening, laenge, breite))
                {
                    is_day = !(is_day);
                    if (!is_day) // execute at sunset
                    {
                        BatteryCapacity = BMS.CellCount * BMS.Nominal_Capacity * 3.2 /1000;
                        if (solar_prognosis < BatteryCapacity)
                        {
                            target_SOC = int(100 * (BatteryCapacity - solar_prognosis) / (BatteryCapacity));
                        }
                        else
                            target_SOC = 0;
                        DynPowerInv = CalculateBalancedDischargePower(BMS.Nominal_Capacity, BMS.Battery_Voltage, BMS.SOC, target_SOC, myTime.sunset_today, myTime.sunrise_tomorrow);
                    }
                }

                g_Time30Min = millis();
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
        if (sender->id == gui_ManualSetPower)
        {
            g_ManualSetPowerCharger = (sender->value).toInt();
        }
        if (sender->id == gui_enableManControl)
        {
            if (type == S_ACTIVE)
            {
                g_enableManualControl = true;
            }
            if (type == S_INACTIVE)
            {
                g_enableManualControl = false;
            }
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
            MQTTdisconnect_counter = 0;         // if user actively enabling/disabling MQTT, than the counter is reset to 0 
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
        if (sender->id == gui_enabledynamicload)
        {
            if (type == S_ACTIVE)
            {
                g_enableDynamicload = true;
            }
            if (type == S_INACTIVE)
            {
                g_enableDynamicload = false;
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
                MQTTdisconnect_counter = 0;
                }
                else if (!PSclient.connected())
                {
                    g_EnableMQTT = false;
                    ESPUI.updateSwitcher(gui_enableMQTT, g_EnableMQTT);
                }
            }
        }
        if (sender->id == gui_SetPowerReserveCharger)
        {
            PowerReserveCharger = (sender->value).toInt();
        }
        if (sender->id == gui_SetPowerReserveInverter)
        {
            PowerReserveInv = (sender->value).toInt();
        }
        if (sender->id == gui_savesettings)
        {
            if (type == B_DOWN)
            {
                saveconfigfile();
            }
        }
        if (sender->id == gui_resetESP)
        {
            if (type == B_DOWN)
            {
                ESP.restart();
            }
        }
        if (sender->id == gui_resetsettings)
        {
            if (type == B_DOWN)
            {
                wm_resetsetting = true;
            }
        }
        if (sender->id == gui_enableChange)
        {
            if (type == S_ACTIVE)
            {

                ESPUI.setEnabled(gui_savesettings, true);
                ESPUI.setEnabled(gui_setMQTTIP, true);
                ESPUI.setEnabled(gui_setMQTTport, true);
                ESPUI.setEnabled(gui_SetMaxPowerCharger, true);
                ESPUI.setEnabled(gui_SetMaxPowerInv, true);
                ESPUI.setEnabled(gui_testMQTT, true);
                ESPUI.setEnabled(gui_enableMQTT, true);
                ESPUI.setEnabled(gui_enableManControl, true);
                ESPUI.setEnabled(gui_ManualSetPower, true);
                ESPUI.setEnabled(gui_resetESP, true);
                ESPUI.setEnabled(gui_resetsettings, true);
                ESPUI.setEnabled(gui_SetPowerReserveCharger, true);
                ESPUI.setEnabled(gui_SetPowerReserveInverter, true);
                ESPUI.setEnabled(gui_enabledynamicload, true);
            }
            if (type == S_INACTIVE)
            {
                ESPUI.setEnabled(gui_savesettings, false);
                ESPUI.setEnabled(gui_setMQTTIP, false);
                ESPUI.setEnabled(gui_setMQTTport, false);
                ESPUI.setEnabled(gui_SetMaxPowerCharger, false);
                ESPUI.setEnabled(gui_SetMaxPowerInv, false);
                ESPUI.setEnabled(gui_testMQTT, false);
                ESPUI.setEnabled(gui_enableMQTT, false);
                ESPUI.setEnabled(gui_enableManControl, false);
                ESPUI.setEnabled(gui_ManualSetPower, false);
                ESPUI.setEnabled(gui_resetESP, false);
                ESPUI.setEnabled(gui_resetsettings, false);
                ESPUI.setEnabled(gui_SetPowerReserveCharger, false);
                ESPUI.setEnabled(gui_SetPowerReserveInverter, false);
                ESPUI.setEnabled(gui_enabledynamicload, false);
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
