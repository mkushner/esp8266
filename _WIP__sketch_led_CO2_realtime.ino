#include <Arduino.h>                                       
#include <SoftwareSerial.h>
#include <Wire.h>
#include "SH1106Wire.h"                                
                                           
//connect MH-Z19b via digitalPin 
SoftwareSerial mySerial(13, 15);  //Rx->D8, Tx->D7 - actual pinout here https://chewett.co.uk/blog/1066/pin-numbering-for-wemos-d1-mini-esp8266/

byte cmd[9] = {0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79}; // official datasheet w/byte commands: https://www.winsen-sensor.com/d/files/MH-Z19B.pdf
//byte cmd_conf[9] = {0xFF,0x01,0x99,0x00,0x00,0x00,0x13,0x88,0xCB}; //5000ppm range - a list of useful byte cmds: https://revspace.nl/MHZ19
char result_raw[32];
unsigned char response[9];
int ppm=0;

//connect LED (using 1.3' I2C SH1106)
SH1106Wire display(0x3c, 4, 14);     // start address, SDA->D2, SCL->D5

void setup()
{

    //led init
    display.init();
    display.flipScreenVertically();
    display.clear();
    
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 10, "Initializing...");
    display.display();
    delay(5000);
    
    Serial.begin(115200); // WEMOS works at 115200
    Serial.println("Wemos 115200 init");
    display.clear();
    display.drawString(0, 10, "Wemos > 115200");
    display.display();
    delay(5000);

    //CO2 init
    mySerial.begin(9600); // MH-Z19b works at 9600                           
    Serial.println("CO2 9600 init");
    Serial.println("delay 20 AFTER mzh init"); 
    display.clear();
    display.drawString(0, 10, "MH-Z19b > 9600");
    display.display();
    delay(5000);

    display.clear();
    display.drawString(0, 10, "Set conf > 5000ppm");
    display.display();
    delay(5000);

    //mySerial.write(cmd_conf, 9); // uncomment and change cmd_conf for setting up measurement range (default 0-5000)

    display.clear();
    display.drawString(0, 10, "CO2 conf setup");
    display.display();
    delay(5000);
}

void drawText() {

    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 10, "CO2: " + String(ppm) + " ppm");
    
}

void getCO2() {

    Serial.println("Checking Serial2");
    Serial.println(mySerial.available());
    
    Serial.println("Sending Serial2 CMD");        
    mySerial.write(cmd, 9);

    Serial.println("Reading response:");
    memset(response, 0, 9);
    mySerial.readBytes(response, 9);

    sprintf(result_raw, "RAW: %02X %02X %02X %02X %02X %02X %02X %02X %02X", response[0], response[1], response[2], response[3], response[4], response[5], response[6], response[7], response[8]);
    Serial.println(result_raw);

    unsigned int responseHigh = (unsigned int) response[2];
    unsigned int responseLow = (unsigned int) response[3];
    ppm = (256 * responseHigh) + responseLow;
        
    Serial.println("CO2 PPM:" + String(ppm)); 

}

void loop()
{

    getCO2();
    
    display.clear();
    drawText();
    display.display();

    delay(30000); // min delay recommended 10s not to overload MH-Z19b sensor
}
