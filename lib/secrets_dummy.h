// rename this file to secrets.h

const char g_WIFI_SSID[] = "SSID";
const char g_WIFI_Passphrase[] = "password";

char mqtt_server[40] = "192.168.188.80";
char mqtt_port[6] = "1883";
char current_clamp_ip[40] = "192.168.188.127";
char current_clamp_cmd[40] = "/cm?cmnd=status+10";
char sensor_resp[20] = "SML";                           // or "MT175"
char sensor_resp_power[20] = "DJ_TPWRCURR";             // or "P"
bool g_EnableMQTT = true;

char solarprognose_token[40] = "token";         // token to get prognosis from http://www.solarprognose.de
char solarprognose_id[4] = "999";

// 
float lagmorning=0;   // time in decimal hours between real sunset and start of PV
float lagevening=0;     // time in decimal hours between end of PV power production and real sunset

float laenge     = 9.1234; // Postition of PV
float breite     = 48.9999; // Position of PV