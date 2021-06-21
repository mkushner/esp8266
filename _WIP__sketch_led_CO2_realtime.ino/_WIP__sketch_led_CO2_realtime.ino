#include <Arduino.h>                                       
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>    
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <SDS011.h>
#include "ThingSpeak.h"
//#include "SH1106Wire.h"

const bool DEBUG = true;              // true for Serial output
const bool INTERVAL_M = true;         // interval measurement

int  INTERVAL_CO = 60000;             // ms between CO measures
int  INTERVAL_PM = 300000;            // ms between PM measures - not less than WAKEUP
const int  INTERVAL_WAKEUP = 15000;   // ms for warmup after sleep

const bool localWS = false;           // run local ws  
const bool cloudWS = true;            // run ThingSpeak integration

bool wifisignal = false;

// setup WIFI
const char* ssid = SSID_HERE;
const char* pass = PASS_HERE;
WiFiClient client;

// setup ThingSpeak
unsigned int channelID = ID_HERE;           // ThingSpeak channel ID
const char * APIkey = API_HERE;  // ThingSpeak WriteAPI key

// setup WS
ESP8266WebServer server(80);
int status = WL_IDLE_STATUS;
String wsDataArray[500];                    // local WS cache 500 measures
                                           
// connect MH-Z19b via digitalPin 
SoftwareSerial myMHZ(13, 15);                                         //  actual pinout here https://chewett.co.uk/blog/1066/pin-numbering-for-wemos-d1-mini-esp8266/
byte MHZcmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};      // official datasheet w/byte commands: https://www.winsen-sensor.com/d/files/MH-Z19B.pdf
byte cmd_conf[9] = {0xFF, 0x01, 0x99, 0x00, 0x00, 0x00, 0x13, 0x88, 0xCB};    // 5000ppm range - a list of useful byte cmds: https://revspace.nl/MHZ19
// byte cmd_conf[9] = {0xFF, 0x01, 0x99, 0x00, 0x00, 0x00, 0x07, 0xD0, 0x8F};   // 2000ppm range
char result_raw[32];
unsigned char response[9];
int ppm=0;
long millisMeasureCO=0;

// connect SDS011 via digitalPin
SDS011 mySDS;
float p10, p25 = 0;
long millisMeasurePM=0;

// connect LED (using 1.3' I2C SH1106) via SDA/SCK
// SH1106Wire display(0x3c, 5, 2);     // start address, SDA, SCL


void setup()
{

    // built-in LED init in DEBUG mode
    if (DEBUG) {
    pinMode(LED_BUILTIN, OUTPUT);
    }
    
    //OLED init
    //display.init();
    //display.flipScreenVertically();
    
    //drawText("Initializing..."); 
    delay(1000);

    Serial.begin(9600);                       // WEMOS works at 115200 -> 9600
    if (DEBUG) {
      Serial.println("Wemos 9600 init"); 
    }
    //drawText("Wemos: 9600");
    delay(1000);

    //show config
    //drawText("Reading config");
    //delay(1000);
    //drawText("DEBUG mode: " + String(DEBUG));
    Serial.println("DEBUG mode: " + String(DEBUG));
    //delay(1000);
    //drawText("localWS support: " + String(localWS));
    Serial.println("localWS support: " + String(localWS));
    //delay(1000);
    //drawText("cloudWS support: " + String(cloudWS));
    Serial.println("cloudWS support: " + String(cloudWS));
    //delay(1000);

    // CO2 init
    myMHZ.begin(9600);   // MH-Z19b works at 9600                           
    if (DEBUG) {
      Serial.println("CO2 9600 init");
    }
    //drawText("MH-Z19b: 9600");
    //delay(5000);

    myMHZ.write(cmd_conf, 9);                 // uncomment and change cmd_conf for setting up measurement range (default 0-5000)

    if (DEBUG) {
      Serial.println("PPM 0-5000");
    }
    //drawText("PPM: 0-5000");

    if ( INTERVAL_M == false) {
      Serial.println("Interval ovverride: 30s"); 
      INTERVAL_CO = 30000;
      INTERVAL_PM = 30000;
    }

    if (DEBUG) {
      Serial.println("Interval_CO: " + String(INTERVAL_CO/1000) + "s");
      Serial.println("Interval_PM: " + String(INTERVAL_PM/1000) + "s");
    }
    //drawText("Interval: " + String(INTERVAL_CO/1000) + "s");
    //delay(5000);

    // SDS011 init
    if (DEBUG) {
      Serial.println("PM sensor init");
    }
    //drawText("PM sensor init");
    //delay(5000);
    mySDS.begin(4, 14); //D1, D4, Tx, Rx

    // WIFI setup
    if (DEBUG) {
      Serial.println("Connecting WIFI");
    }
    //drawText("Connecting WIFI");
    //delay(5000);
    WiFi.begin(ssid,pass);
    
    if (DEBUG) {
    Serial.println("Connecting to " + String(ssid) + " via pass " + String(pass));
    }

    int wifi_try = 0;
    while ( (WiFi.status() != WL_CONNECTED) && (wifi_try < 10) ) {
      if (DEBUG) {
        Serial.println("Couldn't connect on try " + String(wifi_try));
      }
      wifi_try ++;
      delay(1000);
    }

    if (WiFi.status() != WL_CONNECTED) {
      if (DEBUG) {
      Serial.println("Error connecting to WIFI after " + String(wifi_try) + " tries");
      Serial.println("WebServer not supported");
      }
      //drawText("WIFI not found");
      //delay(5000);
      //drawText("WS not supported");
      //delay(5000);
    }
    else {
      if (DEBUG) {
        Serial.println("Connected to WIFI on " + String(wifi_try) + " try");
        Serial.println(WiFi.localIP());
        
        }
        wifisignal = true;
        //drawText(WiFi.localIP().toString());
        //delay(5000);

        //TS setup
        if (cloudWS) {
          ThingSpeak.begin(client);
          if (DEBUG) {
          Serial.println("TS ready");
          }
          //drawText("TS ready");
          //delay(5000);
        }
    }

    //WS setup
    if (localWS) {
       server.on ("/", WebServer);    
       server.begin();
      
       if (DEBUG) {
          Serial.println("local WS ready");
          }
       //drawText("WS ready");
       //delay(5000);

       server.handleClient();
     
       }

     //deay 30s for sensors init
     if (DEBUG) {
          Serial.println("delay 30s to init sensors");
          }
     delay(30000);
    
}

//void drawText( String s) {
//
//    display.clear();
//    display.setTextAlignment(TEXT_ALIGN_LEFT);
//    display.setFont(ArialMT_Plain_10);
//    display.drawString(0, 10, s);
//    display.display();
//    
//}

void getPM() {

    // call wakeup() timing output
    if (DEBUG && INTERVAL_M) {
        Serial.println("call wakeup() on millis=" + String(millis()) + " millisMeasurePM=" + String(millisMeasurePM)); 
      }

    // wakeup if itervals are on
    if (INTERVAL_M) {
        mySDS.wakeup();
        delay(INTERVAL_WAKEUP);
      }

    // read data from the sensor
    int error = mySDS.read(&p25, &p10);
    
    if (!error) {
      Serial.println("P2.5: " + String(p25));
      Serial.println("P10:  " + String(p10));
    }

    else {
      Serial.println("error reading PM: code " + String(error));
    } 

    // call sleep() timing
    if (DEBUG && INTERVAL_M) {
        Serial.println("call sleep() on millis=" + String(millis()) + " millisMeasurePM=" + String(millisMeasurePM)); 
      }

    // sleep if itervals are on
    if (INTERVAL_M) {
        mySDS.sleep();
        
      }

    // write getPM timing
    millisMeasurePM = millis();

  }

void getCO2() {

    if (DEBUG) {
    digitalWrite(LED_BUILTIN, LOW);
    }
        
    if (DEBUG) {
    Serial.println("Checking Serial2");
    Serial.println(myMHZ.available());
    Serial.println("Sending Serial2 CMD");
    } 
    else {
      myMHZ.available(); // handle 1-byte offset 
      }       

    //clear buffer
    if ( !(response[0] == 0xFF && response[1] == 0x86) ) {
        Serial.println("CRC error: " + String(response[8]));
    
        while (myMHZ.available()) {
            myMHZ.read();        
            }
    }
    
    myMHZ.write(MHZcmd, 9);
    
    if (DEBUG) {
    Serial.println("Reading response:");
    }
    memset(response, 0, 9);
    
    delay(10);
    myMHZ.readBytes(response, 9);

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

    if (DEBUG) {
    digitalWrite(LED_BUILTIN, HIGH);
    }

    // write getCO timing
    millisMeasureCO = millis();
    
}

void WebServer() {

  if (DEBUG) {
  Serial.println("ws reply");
  }
  
  String reply = "<!DOCTYPE HTML>";
  reply += "<html><body><hr><table border=1>";
  reply +="<tr><td>Current CO2:</td><td>" + String(wsDataArray[0]) + "</td></tr>";
   
  for (int i = 0; i < 500; i++) {
    reply += "<tr><td>" + String(i+1) + "</td><td>" + String(wsDataArray[i]) + "</td><tr>"; 
  }
  
  reply += "</table><hr></body></html>";
  server.send (200, "text/html", reply);
  
}

void sendWSData(String s) {

  for (int i = 499; i > 0; i--) {
    wsDataArray[i] = wsDataArray[i - 1];
  }
  wsDataArray[0] = s;
  
}

void sendTSData() {

  ThingSpeak.setField(1, ppm);
  ThingSpeak.setField(2, p25);
  ThingSpeak.setField(3, p10);
  
  int httpCode = ThingSpeak.writeFields(channelID, APIkey);

  if (DEBUG) {
    if (httpCode == 200) {
        Serial.println("TS data successfully sent: status " + String(httpCode)); 
      }
      else {
        Serial.println("error sending TS data: status " + String(httpCode)); 
        }
    }
  
}

void loop()
{   
    
    //display.clear();
    //display.setTextAlignment(TEXT_ALIGN_LEFT);
    //display.setFont(ArialMT_Plain_10);
    //display.drawString(0, 10, "CO2: " + String(ppm) + " ppm");
    //display.drawString(0, 30, "P10: " + String(p10) + ", P2.5: " + String(p25));
    //display.display();

    if ( (millis() - millisMeasureCO) > INTERVAL_CO )           // min delay recommended 10s not to overload MH-Z19b sensor
    {
            
      if (DEBUG) {
        Serial.println("call getCO2() on millis=" + String(millis()) + " millisMeasureCO=" + String(millisMeasureCO) ); 
      }
      getCO2();

      if (localWS) {
        if (DEBUG) {
          Serial.println("write to local WS");
        }
        sendWSData(String(ppm));
      }

      if (cloudWS && wifisignal) {
        if (DEBUG) {
          Serial.println("write to cloud TS");
        }
        sendTSData();
      }
 
    }

    if ( (millis() - millisMeasurePM) > INTERVAL_PM ) {
      
      if (DEBUG) {
        Serial.println("call getPM() on millis=" + String(millis()) + " millisMeasurePM=" + String(millisMeasurePM)); 
      }
      getPM();
      
    }  
}
