/* Testprogramm for soyosource Serial input interface to control the output power of the inverter.
Serial Protokol for Soyo 
Data rate @ 4800bps, 8 data, 1 stop
Packet size : 8 Bytes
byte0 = 36
byte1 = 86
byte2 = 0
byte3 = 33
byte4 = 0 ##(2 byte watts as short integer xaxb)
byte5 = 0 ##(2 byte watts as short integer xaxb)
byte6 = 128
byte7 = 8 ## checksum


byte4 = int(demand/256) ## (2 byte watts as short integer xaxb)
    if byte4 <= 0 or byte4 >= 256:
        byte4 = 0
    byte5 = int(demand)-(byte4 * 256) ## (2 byte watts as short integer xaxb)
    if byte5 <= 0 or byte5 >= 256:
        byte5 = 0
    byte7 = (264 - byte4 - byte5) #checksum calculation
    if byte7 >= 256:
        byte7 = 8
    return byte4, byte5, byte7
Basis of code was:
https://github.com/ChrisHomewood/MQTT_to_Soyosource-Inverter_RS485/blob/main/output_control_MQTT_Soyosource_wifikit32.ino

*/

#include <Arduino.h>
// #include <SoftwareSerial.h>
// SoftwareSerial RS485_Port;


HardwareSerial RS485_Port (1);
const int SoyonumBytes = 20;


// initialize RS485 bus for Soyosource communication
void Soyosource_init_RS485(uint8_t rx_pin, uint8_t tx_pin, uint8_t en_pin) {
    pinMode(en_pin, OUTPUT);
    digitalWrite(en_pin, LOW);

//    RS485_Port.begin(4800, SWSERIAL_8N1, rx_pin, tx_pin, false, SoyonumBytes);
    RS485_Port.begin(4800, SERIAL_8N1, rx_pin, tx_pin, false, 100);
    
    // If the object did not initialize, then its configuration is invalid
/*    if (!RS485_Port) {  
    Serial.println("Invalid SoftwareSerial pin configuration, check config"); 
        while (1) {     // Don't continue with invalid configuration
        delay (1000);
        }
    } */
}



// Function prepares serial string and sends powerdemand by serial interface  to Soyo
// needs as input also the enable pin of the RS485 interface
void sendpower2soyo (short demandpowersend, uint8_t en_pin) 
 {
 byte sendbyte[8]; // sendbyte is an array of 8 integers 8 bit (1 byte) long 

/* define Serial command for SOYO */
 sendbyte[0] = 36;
 sendbyte[1] = 86;
 sendbyte[2] = 0;
 sendbyte[3] = 33; 
 sendbyte[4] = 0;      // ##(2 byte watts as short integer xaxb)
 sendbyte[5] = 0;     // ##(2 byte watts as short integer xaxb)
 sendbyte[6] = 128; 
 sendbyte[7]=8;       // ## checksum

 sendbyte[4] = int (demandpowersend/256);
 if (sendbyte[4] <0 || sendbyte[4] >256)
  {
    sendbyte[4]=0;
  }

 sendbyte[5] = int (demandpowersend) - (sendbyte[4]*256);
 if (sendbyte[5] <0 || sendbyte[5] >256)
  {
    sendbyte[5]=0;
  }

 sendbyte[7]=264-sendbyte[4]- sendbyte[5]; // checks umcalculator
 if (sendbyte[7] >256)
  {
    sendbyte[7]=8;
  }
 //___________________________________________END Sendpowertosoyo

  digitalWrite(en_pin, HIGH);
  delay(100);
  RS485_Port.write(sendbyte, sizeof(sendbyte));
  RS485_Port.flush();
}