/*JSON help: https://arduinojson.org/v6/assistant/#/step1

Implementen dashboard: https://ayushsharma82.github.io/ESP-DASH/
Open: Use of https://github.com/s00500/ESPUI


*/

#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <CAN.h>
#include <BluetoothSerial.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#include <ESPAsyncWebServer.h>
#include <ESPDash.h>

#include "huawei.h"
#include "commands.h"
#include "main.h"

WiFiServer server(23);
WiFiClient serverClient;                // for OTA
WiFiClient current_clamp_client;        // or WiFiClientSecure for HTTPS; to connect to current sensor
HTTPClient http;                        // to connect to current sensor

WiFiClient mqttclient;                  // to connect to MQTT broker
PubSubClient PSclient(mqttclient);

AsyncWebServer webserver(80);           // for Dashoard
ESPDash dashboard(&webserver);



BluetoothSerial SerialBT;

const char g_WIFI_SSID[] = "SSID";
const char g_WIFI_Passphrase[] = "PASSWD";


char mqtt_server[40] = "1.2.3.4";
char mqtt_port[6] = "1883";
char current_clamp_ip[40] = "1.2.3.5";
char current_clamp_cmd[40] = "/cm?cmnd=status+10";
char sensor_resp[20] = "SML";                           // or "MT175"
char sensor_resp_power[20] = "DJ_TPWRCURR";             // or "P"

#define ESP_Hostname="ESP32-R4830G2";

namespace Main
{

int g_CurrentChannel;
bool g_Debug[NUM_CHANNELS];
char g_SerialBuffer[NUM_CHANNELS][255];
int g_SerialBufferPos[NUM_CHANNELS];
int ActualPower = 0;                        // for current Power out of current clamp
float ActualVoltage = 0;
float ActualCurrent = 0;
int ActualSetPower = 0;
float ActualSetVoltage = 56.2;
float ActualSetCurrent = 0;
int PowerReserve = 25;
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
Card TargetPowerReserve(&dashboard, SLIDER_CARD, "Power Reserve", "W", 0, 100);
Card CardSetVoltage(&dashboard, SLIDER_CARD, "Charge Voltage", "V", 54.4, 57.6);
Card CardEnableCharge(&dashboard, BUTTON_CARD, "Enable charing");

unsigned long g_Time1000 = 0;
unsigned long g_Time5000 = 0;

int getActualPower() {
  int actPower;
  http.begin(current_clamp_client, "http://"+String(current_clamp_ip)+String(current_clamp_cmd));
  int httpCode = http.GET(); // send the request
  if (httpCode > 0)
  { // check the returning code
    String payload = http.getString(); //Get the request response payload
    // Stream input;
    StaticJsonDocument < 192 > doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      //return NULL;
      return 0;
    }
    JsonObject StatusSNS_SML = doc["StatusSNS"][String(sensor_resp)];
    actPower = StatusSNS_SML[String(sensor_resp_power)]; // -2546
  }
  return actPower;
}

int calculateSetPower(int ActPower, int ActSetPower, int minPowerReserve, int maxPower) 
{ // calculate here how to control the battery charger.
  // Input: available solar power from meter; Output: the power to be set on the charger
  int SetPower;

  if ((minPowerReserve + ActPower) < 0 ) {                                 // goal is to have a reserve of minPower
    SetPower = ActSetPower - int((minPowerReserve + ActPower) / 3);
  }
  else if (((minPowerReserve / 2) + ActPower) < 0 ) {                        // however till minPower/2 the set value is not adjusted
    SetPower = ActSetPower;
  }
  else {                                                            // if we come under minPower/2, than we
    SetPower = ActSetPower - int((minPowerReserve + ActPower) / 2) ;
  }
  SetPower = constrain(SetPower, 0, maxPower);                      // limit the range of control power to 0 and maxPower
    return SetPower;
}

void onCANReceive(int packetSize)
{
    if(!CAN.packetExtended())
        return;
    if(CAN.packetRtr())
        return;

    uint32_t msgid = CAN.packetId();

    uint8_t data[packetSize];
    CAN.readBytes(data, sizeof(data));

    Huawei::onRecvCAN(msgid, data, packetSize);
}

void reconnect() {
  // Loop until we're reconnected
  while (!PSclient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a client ID
    String clientId = "ESP32-R4830G2";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (PSclient.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      PSclient.publish("myoutTopic", "hello world");
      // ... and resubscribe
      PSclient.subscribe("BatteryCharger/enable");
    } else {
      Serial.print("failed, rc=");
      Serial.print(PSclient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == "BatteryCharger/enable") {
    Serial.print("Changing output to ");
    if(messageTemp == "1"){
    //  Serial.println("on");
      g_EnableCharge = true;
    }
    else if(messageTemp == "0"){
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
    while(!Serial);
    Serial.println("BOOTED!");

    // PSU enable pin
    // pinMode(POWER_EN_GPIO, OUTPUT_OPEN_DRAIN);
    // digitalWrite(POWER_EN_GPIO, 0); // Default = ON

    WiFi.setHostname(ESP_Hostname);
    if(!WiFi.begin(g_WIFI_SSID, g_WIFI_Passphrase))
        Serial.println("WiFi config error!");
    else {
        WiFi.setAutoConnect(true);
    }
    Serial.println(WiFi.localIP());
    SerialBT.begin(ESP_Hostname);

    ArduinoOTA.onStart([]() {
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
        else // U_SPIFFS
            type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
        Serial.println("Start updating " + type);
    })
    .onEnd([]() {
        Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

    ArduinoOTA.begin();

    server.begin();
    server.setNoDelay(true);

    if(!CAN.begin(125E3)) {
        Serial.println("Starting CAN failed!");
        while(1);
    }
    Huawei::setCurrent(0,true);                             // set 0 A as default

    // crashes when calling some functions inside interrupt
    //CAN.onReceive(onCANReceive);
    Serial.println(WiFi.localIP());

    /*PSclient.setServer(mqtt_server, atoi(mqtt_port));
    PSclient.setCallback(callback);
    reconnect();
*/
    TargetPowerReserve.attachCallback([&](int value){               /* Attach Slider Callback */
        /* Make sure we update our slider's value and send update to dashboard */
        TargetPowerReserve.update(value);
        dashboard.sendUpdates();
        PowerReserve = value;
    });

    /*
    We provide our attachCallback with a lambda function to handle incomming data
    `value` is the boolean sent from your dashboard
    */
    CardEnableCharge.update(g_EnableCharge);
    CardEnableCharge.attachCallback([&](bool value){
        //Serial.println("[CardEnableCharge] Button Callback Triggered: "+String((value)?"true":"false"));
        g_EnableCharge = value;
        CardEnableCharge.update(value);
        dashboard.sendUpdates();
    });

    CardSetVoltage.attachCallback([&](int value){               /* Attach Slider Callback */
        /* Make sure we update our slider's value and send update to dashboard */
        CardSetVoltage.update(value);
        dashboard.sendUpdates();
        ActualSetVoltage = value;
    });


    webserver.begin();                                       // start Asynchron Webserver
    TargetPowerReserve.update(PowerReserve);
    CardSetVoltage.update(ActualSetVoltage);
    CardDeviceStatus.update("Running", "success");
    dashboard.sendUpdates();
}

Stream* channel(int num)
{
    if(num == -1)
        num = g_CurrentChannel;

    if(num == BTSERIAL && SerialBT.hasClient())
        return &SerialBT;
    else if(num == TCPSERIAL && serverClient)
        return &serverClient;

    return &Serial;
}

void loop()
{
    int packetSize = CAN.parsePacket();
    if(packetSize)
        onCANReceive(packetSize);

    ArduinoOTA.handle();

    if(server.hasClient())
    {
        if(serverClient) // disconnect current client if any
            serverClient.stop();
        serverClient = server.available();
    }
    if(!serverClient)
        serverClient.stop();

    for(int i = 0; i < NUM_CHANNELS; i++)
    {
        while(channel(i)->available())
        {
            g_CurrentChannel = i;
            int c = channel(i)->read();
            if(c == '\r' || c == '\n' || g_SerialBufferPos[i] == sizeof(*g_SerialBuffer))
            {
                g_SerialBuffer[i][g_SerialBufferPos[i]] = 0;

                if(g_SerialBufferPos[i])
                    Commands::parseLine(g_SerialBuffer[i]);

                g_SerialBufferPos[i] = 0;
                continue;
            }
            g_SerialBuffer[i][g_SerialBufferPos[i]] = c;
            ++g_SerialBufferPos[i];
        }
    }

    if((millis() - g_Time1000) > 1000)
    {
        Huawei::every1000ms();
        PSclient.loop();
        g_Time1000 = millis();
    }
    
    if((millis() - g_Time5000) > 5000)
    {
    Huawei::HuaweiInfo &info = Huawei::g_PSU;
    ActualPower = getActualPower();
    //ActualPower = -200-random(70)+ActualSetPower;               // for simulation with random values
    ActualVoltage = info.output_voltage;
    ActualCurrent = info.output_current;
//    ActualSetVoltage = 56.2;
    Huawei::setVoltage(ActualSetVoltage, false);
    
    if (g_EnableCharge) {
        ActualSetPower = calculateSetPower(ActualPower,ActualSetPower,PowerReserve,3000);
        CardDeviceStatus.update("Charging", "success");

    }
    else if (!g_EnableCharge) {
        ActualSetPower = 0;
        CardDeviceStatus.update("Output disabled", "idle");

    }

    ActualSetCurrent = ActualSetPower / ActualSetVoltage;
    Huawei::setCurrent(ActualSetCurrent,false);

    Serial.print(ActualPower);
    Serial.print("   ");
    Serial.print(ActualVoltage);
    Serial.print("   ");
    Serial.print(ActualCurrent);
    Serial.print("   ");

    Serial.print(ActualSetPower);
    Serial.print("   ");
    Serial.print(ActualSetVoltage);
    Serial.print("   ");
    Serial.println(ActualSetCurrent);
    
    //char temp[60];
    //dtostrf(ActualSetCurrent,2,2,temp);
    //sprintf(temp, "{ \"idx\" : 326, \"nvalue\" : 0, \"svalue\" : \"%.2f\" }", ActualSetCurrent);
    //PSclient.publish("domoticz/in", temp);
    //PSclient.loop();
    CardActualPower.update(ActualPower);
    CardActualVoltage.update(ActualVoltage);
    CardActualCurrent.update(ActualCurrent);
    CardActualSetPower.update(ActualSetPower);
    CardActualSetVoltage.update(ActualSetVoltage);
    CardActualSetCurrent.update(ActualSetCurrent);
    CardEnableCharge.update(g_EnableCharge);
    dashboard.sendUpdates();


/*    Main::channel()->println("--- STATUS ----");
    Main::channel()->printf("Input Voltage: %.2f V ~ %.2f Hz\n", info.input_voltage, info.input_freq);
    Main::channel()->printf("Input Current: %.2f A\n", info.input_current);
    Main::channel()->printf("Input Power: %.2f W\n", info.input_power);
    Main::channel()->printf("Input Temperature: %.2f °C\n", info.input_temp);
    Main::channel()->printf("PSU Efficiency: %.2f %%\n", info.efficiency * 100.0);
    Main::channel()->printf("Output Voltage: %.2f V\n", info.output_voltage);
    Main::channel()->printf("Output Current: %.2f A / %.2f A\n", info.output_current, info.output_current_max);
    Main::channel()->printf("Output Power: %.2f W\n", info.output_power);
    Main::channel()->printf("Output Temperature: %.2f °C\n", info.output_temp);
    Main::channel()->printf("Coulomb Counter: %.2f Ah\n", Huawei::g_CoulombCounter / 3600.0);
    Main::channel()->println("--- STATUS ----");
*/

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
