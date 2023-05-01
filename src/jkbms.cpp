
#include "jkbms.h"
HardwareSerial JKBMS_RS485_Port (2);

// 4E 57 00 13 00 00 00 00 06 03 00 00 00 00 00 00 68 00 00 01 29
// BMS commands
byte ReadAllData[21] = {0x4E, 0x57, 0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x01, 0x29};    // read all
byte ReadIdentifier[21] = {0x4E, 0x57, 0x00, 0x13, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x00, 0x83, 0x00, 0x00, 0x00, 0x00, 0x68, 0x00, 0x00, 0x01, 0xA9}; // Read total voltage


// initialize RS485 bus for JK_BMS communication
void JKBMS_init_RS485(uint8_t rx_pin, uint8_t tx_pin, uint8_t en_pin)
{

  pinMode(en_pin, OUTPUT);
  digitalWrite(en_pin, LOW);
  JKBMS_RS485_Port.begin(115200, SERIAL_8N1, rx_pin, tx_pin, false, 500);
}

// get raw data from JK-BMS
JK_BMS_RS485_Data JKBMS_read_data(uint8_t en_pin)
{

  JK_BMS_RS485_Data Returnvalue;
  Returnvalue.length = 0;
  unsigned long receivingtimer = 0;
  const int receivingtime = 300; // 0.3 Sekunde

  digitalWrite(en_pin, HIGH);
  delay(100);
  JKBMS_RS485_Port.write(ReadAllData, sizeof(ReadAllData));
  JKBMS_RS485_Port.flush();
  receivingtimer = millis();

  digitalWrite(en_pin, LOW); // set RS485 to receive
  delay(10);

  while ((receivingtimer + receivingtime) > millis())
  {
    if (JKBMS_RS485_Port.available() && (Returnvalue.length < JKBMS_numBytes))
    {
      Returnvalue.data[Returnvalue.length] = JKBMS_RS485_Port.read();
      Returnvalue.length++;
    }
  }
  return Returnvalue;
}

// Interprets the data from the JK-BMS; returns a structure as defined in jkbms.h
JK_BMS_Data JKBMS_DataAnalysis2(byte *data2decode, int d2dlength)

{
  float min = 4, max = 0; // to store the min an max cell voltage
  uint16_t temp;
  JK_BMS_Data Decoded;

  for (uint16_t i = 11; i < d2dlength - 5;)
  {
    switch (data2decode[i])
    {
    case 0x79: // Cell voltage
      Decoded.CellCount = data2decode[i + 1] / 3;
      i += 2;
      // Serial.print("Cell Voltages = ");
      for (uint8_t n = 0; n < Decoded.CellCount; n++)
      {
        Decoded.cellVoltage[n] = ((data2decode[i + 1] << 8) | data2decode[i + 2]) * 0.001;
        if (min > Decoded.cellVoltage[n])
        {
          min = Decoded.cellVoltage[n];
        }
        if (max < Decoded.cellVoltage[n])
        {
          max = Decoded.cellVoltage[n];
        }
        // Serial.print(cellVoltage[n], 3);
        // n<15 ? Serial.print("V, ") : Serial.println("V");
        i += 3;
      }
      Decoded.MinCellVoltage = min;
      Decoded.MaxCellVoltage = max;
      Decoded.Delta_Cell_Voltage = max - min;
      break;

    case 0x80: // Read tube temp.
      Decoded.MOS_Temp = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]);
      if (Decoded.MOS_Temp > 100)
      {
        Decoded.MOS_Temp = -(Decoded.MOS_Temp - 100);
      }
      // Serial.print("MOS Temp = ");
      // Serial.print(MOS_Temp, 3);
      // Serial.println("C");
      i += 3;
      break;

    case 0x81: // Battery inside temp
      Decoded.Battery_T1 = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]);
      if (Decoded.Battery_T1 > 100)
      {
        Decoded.Battery_T1 = -(Decoded.Battery_T1 - 100);
      }
      // Serial.print("T1 = ");
      // Serial.print(Battery_T1, 3);
      // Serial.println("C");

      i += 3;
      break;

    case 0x82: // Battery temp
      Decoded.Battery_T2 = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]);
      if (Decoded.Battery_T2 > 100)
      {
        Decoded.Battery_T2 = -(Decoded.Battery_T2 - 100);
      }
      // Serial.print("T2 = ");
      // Serial.print(Battery_T2, 3);
      // Serial.println("C");

      i += 3;
      break;

    case 0x83: // Total Batery Voltage
      Decoded.Battery_Voltage = (float)(((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]) * 0.01);
      i += 3;
      // Serial.print("Battery Voltage = ");
      // Serial.print(Battery_Voltage, 3);
      // Serial.println("V");

      break;

    case 0x84: // Current
      temp = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]);
      if (temp >> 15)
      { // Charge current; if the highest bit 1 than charging
        Decoded.Charge_Current = (float)(temp - 32768) * 0.01;
      }
      else
        Decoded.Charge_Current = (float)(temp) * (-0.01); // Else it is the discharge current
      i += 3;
      break;

    case 0x85:                          // Remaining Battery Capacity
      Decoded.SOC = data2decode[i + 1]; // in %
      i += 2;
      break;

    case 0x86:                                    // Number of Battery Temperature Sensors
      Decoded.TempSensCount = data2decode[i + 1]; // in %
      i += 2;
      break;

    case 0x87: // Cycle
      Decoded.Cycle_Count = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]);
      i += 3;
      break;

      // 0x88 does not exist

    case 0x89: // Total Battery cycle Capacity
      Decoded.totBattCycleCapacity = (((uint16_t)data2decode[i + 1] << 24 | data2decode[i + 2] << 16 | data2decode[i + 3] << 8 | data2decode[i + 4]));
      i += 5;
      break;

    case 0x8a: // Total number of battery strings
      Decoded.CellCount = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]);
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

      Decoded.WarningMsgWord = (((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]));
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

      Decoded.BatteryStatus = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]);
      i += 3;
      strcpy(Decoded.sBatteryStatus, "undefined");
      strcpy(Decoded.charge, "off");
      strcpy(Decoded.discharge, "off");
      strcpy(Decoded.balance, "off");

      if ((Decoded.BatteryStatus >> 3) & 0x01)
      { // looking for bit 3
        strcpy(Decoded.sBatteryStatus, "Battery Down");
      }
      else
      {
        if (Decoded.BatteryStatus & 0x01)
        { // looking for bit 0
          strcpy(Decoded.charge, "on ");
        }
        else
          strcpy(Decoded.charge, "off");

        strcpy(Decoded.sBatteryStatus, "Charge:");
        strncat(Decoded.sBatteryStatus, Decoded.charge, 3);

        if ((Decoded.BatteryStatus >> 1) & 0x01)
        { // looking for bit 1
          strcpy(Decoded.discharge, "on ");
        }
        else
          strcpy(Decoded.discharge, "off");

        strncat(Decoded.sBatteryStatus, " Discharge:", 12);
        strncat(Decoded.sBatteryStatus, Decoded.discharge, 3);

        if ((Decoded.BatteryStatus >> 2) & 0x01)
        { // looking for bit 2
          strcpy(Decoded.balance, "on ");
        }
        else
          strcpy(Decoded.charge, "off");

        strncat(Decoded.sBatteryStatus, " Balance:", 10);
        strncat(Decoded.sBatteryStatus, Decoded.balance, 3);
      }

      break;

    case 0x8e: // Total voltage overvoltage protection
      Decoded.OVP = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]) * 0.001;
      i += 3;
      break;

    case 0x8f: // Total voltage undervoltage protection
      Decoded.UVP = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]) * 0.001;
      i += 3;
      break;

    case 0x90: // Single overvoltage protection voltage
      Decoded.sOVP = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]);
      i += 3;
      break;

    case 0x91: // Cell overvoltage recovery voltage
      Decoded.sOVPR = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]);
      i += 3;
      break;

    case 0x92:                                                                       // Single overvoltage protection delay
      Decoded.sOVP_delay = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]); // in seconds
      i += 3;
      break;

    case 0x93: // Single undervoltage protection voltage
      Decoded.sUVP = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]);
      i += 3;
      break;

    case 0x94: // Monomer undervoltage recovery voltage
      Decoded.sUVPR = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]);
      i += 3;
      break;

    case 0x95:                                                                       // Single undervoltage protection delay
      Decoded.sUVP_delay = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]); // in seconds
      i += 3;
      break;

    case 0x96:                                                                      // Cell voltage?pressure difference protection value
      Decoded.sVoltDiff = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]); // in seconds
      i += 3;
      break;

    case 0x97:                                                                         // Discharge overcurrent protection value
      Decoded.dischargeOCP = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]); // in Ampere
      i += 3;
      break;

    case 0x98:                                                                               // Discharge overcurrent delay
      Decoded.dischargeOCP_delay = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]); // in seconds
      i += 3;
      break;

    case 0x99:                                                                      // Charge overcurrent protection value
      Decoded.chargeOCP = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]); // in Ampere
      i += 3;
      break;

    case 0x9a:                                                                            // Discharge overcurrent delay
      Decoded.chargeOCP_delay = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]); // in seconds
      i += 3;
      break;

    case 0x9b:                                                                                // Balanced starting voltage
      Decoded.BalanceStartVoltage = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]); // in Voltage
      i += 3;
      break;

    case 0x9c:                                                                           // Balanced opening pressure difference
      Decoded.BalanceVoltage = ((uint16_t)data2decode[i + 1] << 8 | data2decode[i + 2]); // in AVoltagempere
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
      Decoded.Nominal_Capacity = (((uint16_t)data2decode[i + 1] << 24 | data2decode[i + 2] << 16 | data2decode[i + 3] << 8 | data2decode[i + 4]));
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
      Decoded.Uptime = (((uint16_t)data2decode[i + 1] << 24 | data2decode[i + 2] << 16 | data2decode[i + 3] << 8 | data2decode[i + 4]));
      //        sec = Uptime % 60;
      //        sec = 0;
      //        Uptime /= 60;
      Decoded.mi = Decoded.Uptime % 60;
      Decoded.Uptime /= 60;
      Decoded.hr = Decoded.Uptime % 24;
      Decoded.days = Decoded.Uptime /= 24;

      i += 5;
      break;

    default:
      i++;
      break;
    }
  }
  Decoded.Battery_Power = Decoded.Battery_Voltage*Decoded.Charge_Current;
  return Decoded;
}

// interpret/decode the data from the JK-BMS
JK_BMS_Data JKBMS_DataAnalysis(JK_BMS_RS485_Data data2decode)
{
  float min = 4, max = 0; // to store the min an max cell voltage
  uint16_t temp;
  JK_BMS_Data Decoded;

  for (uint16_t i = 11; i < data2decode.length - 5;)
  {
    switch (data2decode.data[i])
    {
    case 0x79: // Cell voltage
      Decoded.CellCount = data2decode.data[i + 1] / 3;
      i += 2;
      // Serial.print("Cell Voltages = ");
      for (uint8_t n = 0; n < Decoded.CellCount; n++)
      {
        Decoded.cellVoltage[n] = ((data2decode.data[i + 1] << 8) | data2decode.data[i + 2]) * 0.001;
        if (min > Decoded.cellVoltage[n])
        {
          min = Decoded.cellVoltage[n];
        }
        if (max < Decoded.cellVoltage[n])
        {
          max = Decoded.cellVoltage[n];
        }
        // Serial.print(cellVoltage[n], 3);
        // n<15 ? Serial.print("V, ") : Serial.println("V");
        i += 3;
      }
      Decoded.Delta_Cell_Voltage = max - min;
      break;

    case 0x80: // Read tube temp.
      Decoded.MOS_Temp = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]);
      if (Decoded.MOS_Temp > 100)
      {
        Decoded.MOS_Temp = -(Decoded.MOS_Temp - 100);
      }
      // Serial.print("MOS Temp = ");
      // Serial.print(MOS_Temp, 3);
      // Serial.println("C");
      i += 3;
      break;

    case 0x81: // Battery inside temp
      Decoded.Battery_T1 = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]);
      if (Decoded.Battery_T1 > 100)
      {
        Decoded.Battery_T1 = -(Decoded.Battery_T1 - 100);
      }
      // Serial.print("T1 = ");
      // Serial.print(Battery_T1, 3);
      // Serial.println("C");

      i += 3;
      break;

    case 0x82: // Battery temp
      Decoded.Battery_T2 = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]);
      if (Decoded.Battery_T2 > 100)
      {
        Decoded.Battery_T2 = -(Decoded.Battery_T2 - 100);
      }
      // Serial.print("T2 = ");
      // Serial.print(Battery_T2, 3);
      // Serial.println("C");

      i += 3;
      break;

    case 0x83: // Total Batery Voltage
      Decoded.Battery_Voltage = (float)(((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]) * 0.01);
      i += 3;
      // Serial.print("Battery Voltage = ");
      // Serial.print(Battery_Voltage, 3);
      // Serial.println("V");

      break;

    case 0x84: // Current
      temp = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]);
      if (temp >> 15)
      { // Charge current; if the highest bit 1 than charging
        Decoded.Charge_Current = (float)(temp - 32768) * 0.01;
      }
      else
        Decoded.Charge_Current = (float)(temp) * (-0.01); // Else it is the discharge current
      i += 3;
      break;

    case 0x85:                          // Remaining Battery Capacity
      Decoded.SOC = data2decode.data[i + 1]; // in %
      i += 2;
      break;

    case 0x86:                                    // Number of Battery Temperature Sensors
      Decoded.TempSensCount = data2decode.data[i + 1]; // in %
      i += 2;
      break;

    case 0x87: // Cycle
      Decoded.Cycle_Count = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]);
      i += 3;
      break;

      // 0x88 does not exist

    case 0x89: // Total Battery cycle Capacity
      Decoded.totBattCycleCapacity = (((uint16_t)data2decode.data[i + 1] << 24 | data2decode.data[i + 2] << 16 | data2decode.data[i + 3] << 8 | data2decode.data[i + 4]));
      i += 5;
      break;

    case 0x8a: // Total number of battery strings
      Decoded.CellCount = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]);
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

      Decoded.WarningMsgWord = (((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]));
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

      Decoded.BatteryStatus = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]);
      i += 3;
      strcpy(Decoded.sBatteryStatus, "undefined");
      strcpy(Decoded.charge, "off");
      strcpy(Decoded.discharge, "off");
      strcpy(Decoded.balance, "off");

      if ((Decoded.BatteryStatus >> 3) & 0x01)
      { // looking for bit 3
        strcpy(Decoded.sBatteryStatus, "Battery Down");
      }
      else
      {
        if (Decoded.BatteryStatus & 0x01)
        { // looking for bit 0
          strcpy(Decoded.charge, "on ");
        }
        else
          strcpy(Decoded.charge, "off");

        strcpy(Decoded.sBatteryStatus, "Charge:");
        strncat(Decoded.sBatteryStatus, Decoded.charge, 3);

        if ((Decoded.BatteryStatus >> 1) & 0x01)
        { // looking for bit 1
          strcpy(Decoded.discharge, "on ");
        }
        else
          strcpy(Decoded.discharge, "off");

        strncat(Decoded.sBatteryStatus, " Discharge:", 12);
        strncat(Decoded.sBatteryStatus, Decoded.discharge, 3);

        if ((Decoded.BatteryStatus >> 2) & 0x01)
        { // looking for bit 2
          strcpy(Decoded.balance, "on ");
        }
        else
          strcpy(Decoded.charge, "off");

        strncat(Decoded.sBatteryStatus, " Balance:", 10);
        strncat(Decoded.sBatteryStatus, Decoded.balance, 3);
      }

      break;

    case 0x8e: // Total voltage overvoltage protection
      Decoded.OVP = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]) * 0.001;
      i += 3;
      break;

    case 0x8f: // Total voltage undervoltage protection
      Decoded.UVP = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]) * 0.001;
      i += 3;
      break;

    case 0x90: // Single overvoltage protection voltage
      Decoded.sOVP = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]);
      i += 3;
      break;

    case 0x91: // Cell overvoltage recovery voltage
      Decoded.sOVPR = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]);
      i += 3;
      break;

    case 0x92:                                                                       // Single overvoltage protection delay
      Decoded.sOVP_delay = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]); // in seconds
      i += 3;
      break;

    case 0x93: // Single undervoltage protection voltage
      Decoded.sUVP = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]);
      i += 3;
      break;

    case 0x94: // Monomer undervoltage recovery voltage
      Decoded.sUVPR = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]);
      i += 3;
      break;

    case 0x95:                                                                       // Single undervoltage protection delay
      Decoded.sUVP_delay = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]); // in seconds
      i += 3;
      break;

    case 0x96:                                                                      // Cell voltage?pressure difference protection value
      Decoded.sVoltDiff = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]); // in seconds
      i += 3;
      break;

    case 0x97:                                                                         // Discharge overcurrent protection value
      Decoded.dischargeOCP = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]); // in Ampere
      i += 3;
      break;

    case 0x98:                                                                               // Discharge overcurrent delay
      Decoded.dischargeOCP_delay = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]); // in seconds
      i += 3;
      break;

    case 0x99:                                                                      // Charge overcurrent protection value
      Decoded.chargeOCP = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]); // in Ampere
      i += 3;
      break;

    case 0x9a:                                                                            // Discharge overcurrent delay
      Decoded.chargeOCP_delay = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]); // in seconds
      i += 3;
      break;

    case 0x9b:                                                                                // Balanced starting voltage
      Decoded.BalanceStartVoltage = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]); // in Voltage
      i += 3;
      break;

    case 0x9c:                                                                           // Balanced opening pressure difference
      Decoded.BalanceVoltage = ((uint16_t)data2decode.data[i + 1] << 8 | data2decode.data[i + 2]); // in AVoltagempere
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
      Decoded.Nominal_Capacity = (((uint16_t)data2decode.data[i + 1] << 24 | data2decode.data[i + 2] << 16 | data2decode.data[i + 3] << 8 | data2decode.data[i + 4]));
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
      Decoded.Uptime = (((uint16_t)data2decode.data[i + 1] << 24 | data2decode.data[i + 2] << 16 | data2decode.data[i + 3] << 8 | data2decode.data[i + 4]));
      //        sec = Uptime % 60;
      //        sec = 0;
      //        Uptime /= 60;
      Decoded.mi = Decoded.Uptime % 60;
      Decoded.Uptime /= 60;
      Decoded.hr = Decoded.Uptime % 24;
      Decoded.days = Decoded.Uptime /= 24;

      i += 5;
      break;

    default:
      i++;
      break;
    }
  }
  Decoded.Battery_Power = Decoded.Battery_Voltage*Decoded.Charge_Current;
  return Decoded;
}

