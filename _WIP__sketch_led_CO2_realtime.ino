#include <Arduino.h>                                       
#include <SoftwareSerial.h>
#include <Wire.h>
#include "SH1106Wire.h"
#include <ESP8266WiFi.h> // http://esp8266.github.io/Arduino/versions/2.0.0/doc/libraries.html#wifi-esp8266wifi-library
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>

const bool DEBUG = false; // true for Serial output
const int INTERVAL = 30000; // ms between measurement

// setup WIFI
const char* ssid = "ssid";
const char* pass = "pass";

ESP8266WebServer server(80);
int status = WL_IDLE_STATUS;
String wsDataArray[1000];
                                           
// connect MH-Z19b via digitalPin 
SoftwareSerial mySerial(13, 15);  //Rx->D8, Tx->D7 - actual pinout here https://chewett.co.uk/blog/1066/pin-numbering-for-wemos-d1-mini-esp8266/
byte cmd[9] = {0xFF,0x01,0x86,0x00,0x00,0x00,0x00,0x00,0x79}; // official datasheet w/byte commands: https://www.winsen-sensor.com/d/files/MH-Z19B.pdf
//byte cmd_conf[9] = {0xFF,0x01,0x99,0x00,0x00,0x00,0x13,0x88,0xCB}; //5000ppm range - a list of useful byte cmds: https://revspace.nl/MHZ19
char result_raw[32];
unsigned char response[9];
int ppm=0;
long millisMeasure=0;
long millisCurrent=0;

// connect LED (using 1.3' I2C SH1106)
SH1106Wire display(0x3c, 4, 14);     // start address, SDA->D2, SCL->D5

void setup()
{

    // led init
    display.init();
    display.flipScreenVertically();
    
    drawText("Initializing..."); 
    delay(5000);

    Serial.begin(115200); // WEMOS works at 115200
    if (DEBUG) {
      Serial.println("Wemos 115200 init"); 
    }
    drawText("Wemos = 115200");
    delay(5000);

    //CO2 init
    mySerial.begin(9600); // MH-Z19b works at 9600                           
    if (DEBUG) {
      Serial.println("CO2 9600 init");
    }
    drawText("MH-Z19b = 9600");
    delay(5000);

    //mySerial.write(cmd_conf, 9); // uncomment and change cmd_conf for setting up measurement range (default 0-5000)

    drawText("PPM = 0-5000");
    delay(5000);

    drawText("Interval = " + String((INTERVAL/1000)) + "s");
    delay(5000);

    //WIFI setup
    drawText("Connecting WIFI");
    delay(5000);
    
    if (DEBUG) {
    Serial.println("Connecting to " + String(ssid) + " via pass " + String(pass));
    }
    while (WiFi.status() != WL_CONNECTED) {
      if (DEBUG) {
        Serial.println("Couldn't connect");
      }
      delay(1000);
    }

    if (DEBUG) {
    Serial.println(WiFi.status());
    Serial.println(WiFi.localIP());
    }

    drawText(WiFi.localIP().toString());
    delay(5000);

    //WS setup
    server.on ("/", WebServer);    
    server.begin();

    if (DEBUG) {
    Serial.println("WS ready");
    }

    drawText("WS ready");
    delay(5000);
    
}

void drawText( String s) {

    display.clear();
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_16);
    display.drawString(0, 10, s);
    display.display();
    
}

void getCO2() {

    if (DEBUG) {
    Serial.println("Checking Serial2");
    Serial.println(mySerial.available());
    Serial.println("Sending Serial2 CMD");
    } 
    else {
      mySerial.available(); // handle 1-byte offset 
      }       
    
    mySerial.write(cmd, 9);

    if (DEBUG) {
    Serial.println("Reading response:");
    }
    memset(response, 0, 9);
    mySerial.readBytes(response, 9);

    if (DEBUG) {
    sprintf(result_raw, "RAW: %02X %02X %02X %02X %02X %02X %02X %02X %02X", response[0], response[1], response[2], response[3], response[4], response[5], response[6], response[7], response[8]);
    Serial.println(result_raw);
    }

    unsigned int responseHigh = (unsigned int) response[2];
    unsigned int responseLow = (unsigned int) response[3];
    ppm = (256 * responseHigh) + responseLow;

    if (DEBUG) {
    Serial.println("CO2 PPM:" + String(ppm));
    } 
    sendWSData(String(ppm));

    millisMeasure = millis();
    
}

void WebServer() {

  if (DEBUG) {
  Serial.println("ws reply");
  }
  
  String reply = "<!DOCTYPE HTML>";
  reply += "<html><body><hr><table border=1>";
  reply +="<tr><td>Current CO2:</td><td>" + String(wsDataArray[0]) + "</td></tr>";
   
  for (int i = 0; i < 1000; i++) {
    reply += "<tr><td>" + String(i+1) + "</td><td>" + String(wsDataArray[i]) + "</td><tr>"; 
  }
  
  reply += "</table><hr></body></html>";
  server.send (200, "text/html", reply);
  
}

void sendWSData(String s) {

  for (int i = 999; i > 0; i--) {
    wsDataArray[i] = wsDataArray[i - 1];
  }
  wsDataArray[0] = s;
  
}

void loop()
{   
    server.handleClient();
    
    drawText("CO2: " + String(ppm) + " ppm");

    if (millisMeasure == 0 || (millis() - millisMeasure) > INTERVAL) // min delay recommended 10s not to overload MH-Z19b sensor
    {
      getCO2();
      if (DEBUG) {
      Serial.println("call getCO2()"); 
      }
    }

}