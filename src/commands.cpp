#include <Arduino.h>
#include <WiFi.h>

#include "utils.h"
#include "main.h"
#include "huawei.h"
#include "commands.h"

namespace Commands {

int parseLine(char *line)
{
    const int MAX_ARGV = 16;
    char *lineStart = line;
    char *argv[MAX_ARGV];
    int argc = 0;
    bool found = false;
    bool end = false;
    bool inString = false;

    while(1)
    {
		if(*line == '"' && (line == lineStart || *(line - 1) != '\\'))
		{
			inString ^= true;
			if(!inString)
			{
                found = true;
				end = true;
				lineStart++;
			}
		}

        if(!inString && *line == ' ')
            end = true;

        if(end || !*line)
        {
            if(found && argc < MAX_ARGV)
                argv[argc++] = lineStart;

            found = false;
            end = false;

            if(!*line)
                break;

            *line = 0;
            lineStart = ++line;
            continue;
        }

        line++;
        found = true;
    }

    if(!argc)
        return -1;

    for(int i = 0; g_Commands[i].cmd; i++)
    {
        if(strcmp(g_Commands[i].cmd, argv[0]) == 0)
        {
            int ret = g_Commands[i].fun(argc, argv);
            return ret;
        }
    }

    return -1;
}

int CMD_help(int argc, char **argv)
{
    Serial.println("Available Commands\n------------------");

    int i = 0;
    while(g_Commands[i].cmd)
    {
        Serial.printf("%17s %s\n", g_Commands[i].cmd, g_Commands[i].help);
        i++;
    }
    Serial.println();

    return 0;
}

int CMD_debug(int argc, char **argv)
{
    if(argc != 2) {
        Serial.println("Usage: debug <0|1>");
        return 1;
    }

    bool debug = false;
    if(strtoul(argv[1], NULL, 10))
        debug = true;

   // Main::g_Debug[Main::g_CurrentChannel] = debug;

    return 0;
}

int CMD_status(int argc, char **argv)
{
    Huawei::HuaweiInfo &info = Huawei::g_PSU;

    Serial.println("--- STATUS ----");
    Serial.printf("Input Voltage: %.2f V ~ %.2f Hz\n", info.input_voltage, info.input_freq);
    Serial.printf("Input Current: %.2f A\n", info.input_current);
    Serial.printf("Input Power: %.2f W\n", info.input_power);
    Serial.printf("Input Temperature: %.2f °C\n", info.input_temp);
    Serial.printf("PSU Efficiency: %.2f %%\n", info.efficiency * 100.0);
    Serial.printf("Output Voltage: %.2f V\n", info.output_voltage);
    Serial.printf("Output Current: %.2f A\n", info.output_current);
    Serial.printf("Output Current Setpoint: %.2f A\n", info.output_current_max);
    Serial.printf("Output Power: %.2f W\n", info.output_power);
    Serial.printf("Output Temperature: %.2f °C\n", info.output_temp);
    Serial.printf("Coulomb Counter: %.2f Ah\n", Huawei::g_CoulombCounter / 3600.0);
    Serial.println("--- STATUS ----");

    return 0;
}

int CMD_info(int argc, char **argv)
{
    Huawei::sendGetInfo();
    return 0;
}

int CMD_description(int argc, char **argv)
{
    Huawei::sendGetDescription();
    return 0;
}
/*
int CMD_voltage(int argc, char **argv)
{
    if(argc < 2 || argc > 3) {
        Serial.println("Usage: voltage <volts> [perm]");
        return 1;
    }

    float u = strtof(argv[1], NULL);

    bool perm = false;
    if(argc == 3) {
        if(strtoul(argv[2], NULL, 10))
            perm = true;
    }

    Huawei::setVoltage(u, perm);

    Huawei::HuaweiInfo &info = Huawei::g_PSU;
    Serial.printf("Output Voltage: %.2f V\n", info.output_voltage);

    return 0;
}
*/
int CMD_current(int argc, char **argv)
{
    if(argc < 2 || argc > 3) {
        Serial.println("Usage: current <amps> [perm]");
        return 1;
    }

    float i = strtof(argv[1], NULL);

    bool perm = false;
    if(argc == 3) {
        if(strtoul(argv[2], NULL, 10))
            perm = true;
    }

    Huawei::setCurrent(i, perm);

    return 0;
}

int CMD_can(int argc, char **argv)
{
    if(argc < 3 || argc > 4) {
        Serial.println("Usage: can <msgid> <hex> [rtr]");
        return 1;
    }

    uint32_t msgid = strtoul(argv[1], NULL, 16);
    uint8_t data[8];

    int len = hex2bytes(argv[2], data, sizeof(data));

    bool rtr = false;
    if(argc == 4) {
        if(strtoul(argv[3], NULL, 10))
            rtr = true;
    }

    if(len <= 0 && !rtr)
        return 1;

    Huawei::sendCAN(msgid, data, len, rtr);

    return 0;
}

int CMD_onoff(int argc, char **argv)
{
    if(argc != 2) {
        Serial.println("Usage: onoff <0|1>");
        return 1;
    }

    int val = !strtoul(argv[1], NULL, 10);
    digitalWrite(POWER_EN_GPIO, val);

    return 0;
}



CommandEntry g_Commands[] =
{
    {"help",        CMD_help,       " : Display list of commands"},
   // {"voltage",     CMD_voltage,    " : voltage <volts> [perm]"},
    {"current",     CMD_current,    " : current <amps> [perm]"},
    {"status",      CMD_status,     " : print current PSU status/info"},
    {"info",        CMD_info,       " : print internal PSU information"},
    {"description", CMD_description," : print internal PSU description"},
    {"debug",       CMD_debug,      " : debug <0|1>"},
    {"can",         CMD_can,        " : can <msgid> <hex> [rtr]"},
    {"onoff",       CMD_onoff,      " : onoff <0|1>"},
    { 0, 0, 0 }
};

}
