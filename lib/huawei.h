#ifndef HUAWEI_H
#define HUAWEI_H
#include <stdint.h>

#define HUAWEI_R48XX_PROTOCOL_ID		0x21
#define HUAWEI_R48XX_MSG_CONTROL_ID		0x80 // used to change things like current and voltage
#define HUAWEI_R48XX_MSG_CONFIG_ID		0x81 // ACK frame after setting values?? -- was not used in this code
#define HUAWEI_R48XX_MSG_QUERY_ID		0x82 // what does this do? - not used in any implementation I could see
#define HUAWEI_R48XX_MSG_DATA_ID		0x40 // returns the general status data
#define HUAWEI_R48XX_MSG_INFO_ID		0x50 // just used to display the rated current in this code
#define HUAWEI_R48XX_MSG_DESC_ID		0xD2 // display sserial number and other info
#define HUAWEI_R48XX_MSG_CURRENT_ID		0x11 // real time current data - used only in columb counter in this code

namespace Huawei {

struct HuaweiEAddr
{
    uint8_t protoId : 6;
    uint8_t addr : 7;
    uint8_t cmdId : 8;
    uint8_t fromSrc : 1;
    uint8_t rev : 6;
    uint8_t count : 1;

    uint32_t pack();
    static HuaweiEAddr unpack(uint32_t val);
};

struct HuaweiInfo
{
    float input_voltage;
    float input_freq;
    float input_current;
    float input_power;
    float input_temp;
    float efficiency;
    float output_voltage;
    float output_current;
    float output_current_max;
    float output_power;
    float output_temp;
};

void onRecvCAN(uint32_t msgid, uint8_t *data, uint8_t length);
void every1000ms();

void sendCAN(uint32_t msgid, uint8_t *data, uint8_t length, bool rtr = false);

void setReg(uint8_t reg, uint16_t val, uint8_t addr);

void setVoltageHex(uint16_t hex, uint8_t addr, bool perm);
void setVoltage(float u, uint8_t addr, bool perm);

void setCurrentHex(uint16_t hex, bool perm);
void setCurrent(float i, bool perm);

void sendGetData(uint8_t addr);
void sendGetInfo();
void sendGetDescription();

extern uint16_t g_UserVoltage;
extern uint16_t g_UserCurrent;
extern float g_Current;
extern float g_CoulombCounter;

extern HuaweiInfo g_PSU;


}
#endif
