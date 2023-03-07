/*
This Libary handles the data to the Display
https://randomnerdtutorials.com/esp32-ssd1306-oled-display-arduino-ide/
https://randomnerdtutorials.com/esp32-i2c-communication-arduino-ide/
// Pinout Reference
https://randomnerdtutorials.com/esp32-pinout-reference-gpios/ 
https://randomnerdtutorials.com/esp32-i2c-communication-arduino-ide/

// Display Driver SSD 1306
// Size 0,96 Inch 128*64 pixel monocolour
// Pin wiring
//Vin 3,3V
// GND GND#
// SCL GPIO21 #define Display_I2C_SDA1 21
// SDA GPIO22 #define Display_I2C_SLC1 22

/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com  
*********/
 #include <Wire.h>
 //#include <spi.h>
 #include <Adafruit_GFX.h>
 #include <Adafruit_SSD1306.h>

 #define SCREEN_WIDTH 128 // OLED display width, in pixels
 #define SCREEN_HEIGHT 64 // OLED display height, in pixels 

 // Declaration for an SSD1306 display connected to I2C (SDA GPIO21, SCL GPIO22 pins)
 Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);




void setup_display(){


  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3D for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(2000);
  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  // Display static text
  display.println("Hui,");
  display.println("Battary");
  display.println("Storage!");
  
  display.display(); 

}