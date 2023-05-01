// https://www.geeksforgeeks.org/how-to-return-multiple-values-from-a-function-in-c-or-cpp/
// #include <stdio.h>
#include <Arduino.h>
const int JKBMS_numBytes = 320;   // data to receive from BMS



//BMS values
struct BMS_Data {


float cellVoltage[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
float MOS_Temp = 0;
float Battery_T1 = 0;
float Battery_T2 = 0;
float Battery_Voltage = 0.0;
float Charge_Current = 0;
int   SOC = 0;
byte  TempSensCount = 0;
byte  Cycle_Count = 0;
int   totBattCycleCapacity = 0;
byte  CellCount = 0;
int   WarningMsgWord = 0;
int   BatteryStatus = 0;
char  sBatteryStatus[40] ="undefined";
float OVP = 0;                // Total Over Voltage Protection
float UVP = 0;                // Total Under VOltage Protection
float sOVP = 0;               // single cell OVP
float sOVPR = 0;              // single cell OVP recovery
byte sOVP_delay = 0;          // Single overvoltage protection delay
float sUVP = 0;               // Single Under VOltage Protection
float sUVPR = 0;              // Monomer undervoltage recovery voltage
byte sUVP_delay = 0;          // Single undervoltage protection delay
int sVoltDiff = 0;            // Cell pressure difference protection value

int dischargeOCP = 0;         // Discharge overcurrent protection value
byte dischargeOCP_delay = 0;  // Discharge overcurrent delay

int chargeOCP = 0;            // charge overcurrent protection value
byte chargeOCP_delay = 0;     // charge overcurrent delay

float BalanceStartVoltage = 0;  // Balanced starting voltage
float BalanceVoltage = 0;       // Balanced opening pressure difference
bool ActiveBalanceEnable = false; //Active balance switch

byte MOSProtection = 0;        // Power tube temperature protection value
byte MOSProtectionR = 0;       // Power tube temperature protection recovery
byte TempProtection = 0;       // Temperature protection value in the battery box
byte TempProtectionR = 0;      // Temperature protection recovery value in the battery box
byte TempProtDiff = 0;         // Battery temperature difference protection value
byte ChargeHTemp = 0;          // Battery charging high temperature protection value
byte DisChargeHTemp = 0;      // Battery discharge high temperature protection value
byte ChargeLTemp = 0;         // Charging low temperature protection value
byte ChargeLTempR = 0;        // Charging low temperature protection recovery value
byte DisChargeLTemp = 0;      // Discharge low temperature protection value
byte DisChargeLTempR = 0;     // DisCharging low temperature protection recovery value
int Nominal_Capacity = 0;     // Battery capacity setting


// float Average_Cell_Voltage = 0;
float Delta_Cell_Voltage = 0;
float MinCellVoltage = 0;
float MaxCellVoltage = 0;

float Current_Balancer = 0;

float Battery_Power = 0;
float Capacity_Remain = 0;


float Capacity_Cycle = 0;
uint32_t Uptime;
uint8_t sec, mi, hr, days;
float Balance_Curr = 0;
char charge[4] = "off";
char discharge[4] = "off";
char balance[4] = "off";


};

typedef struct BMS_Data JK_BMS_Data;

// raw RS485 data
struct BMS_RS485_Data {
    byte data[JKBMS_numBytes];
    int length;
};

typedef struct BMS_RS485_Data JK_BMS_RS485_Data;



// initialize RS485 bus for JK_BMS communication
void JKBMS_init_RS485(uint8_t rx_pin, uint8_t tx_pin, uint8_t en_pin);

// get data from JKBMS
JK_BMS_RS485_Data JKBMS_read_data(uint8_t en_pin);


// interpret/decode the data from the JK-BMS
JK_BMS_Data JKBMS_DataAnalysis(JK_BMS_RS485_Data data2decode);