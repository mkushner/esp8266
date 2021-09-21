#include <Arduino.h>                                       
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>    
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <SDS011.h>
#include "ThingSpeak.h"
#include <ArduinoJson.h>
#include <StreamUtils.h>

// basic measurement setup
  bool DEBUG = true;                           // true for Serial output
  bool INTERVAL_M = true;                      // interval measurement
  int  INTERVAL_CO = 300000;                   // ms between CO measures
  int  INTERVAL_PM = 600000;                   // ms between PM measures - not less than WAKEUP
  int  INTERVAL_WAKEUP = 20000;                // ms for warmup after sleep
  bool cloudWS = true;                         // run ThingSpeak integration

// setup WIFI
  const char* ssid = <"SSID">;
  const char* pass = <"PASS">;
  bool wifisignal = false;
  WiFiClient client;

// setup ThingSpeak
  int channelID = <ID>;                       // ThingSpeak channel ID
  const char * APIkey = <"key">;              // ThingSpeak WriteAPI key
                                           
// connect MH-Z19b via digitalPin 
  SoftwareSerial myMHZ(13, 15);                                                    //  actual pinout here https://chewett.co.uk/blog/1066/pin-numbering-for-wemos-d1-mini-esp8266/
  byte MHZcmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};         // official datasheet w/byte commands: https://www.winsen-sensor.com/d/files/MH-Z19B.pdf
  //byte cmd_conf[9] = {0xFF, 0x01, 0x99, 0x00, 0x00, 0x00, 0x13, 0x88, 0xCB};     // 5000ppm range - a list of useful byte cmds: https://revspace.nl/MHZ19
  //byte cmd_conf[9] = {0xFF, 0x01, 0x99, 0x00, 0x00, 0x00, 0x07, 0xD0, 0x8F};     // 2000ppm range
  char result_raw[32];
  unsigned char response[9];
  int ppm=0;
  unsigned long millisMeasureCO=0;

// connect SDS011 via digitalPin
  SDS011 mySDS;
  float p10, p25 = 0;
  unsigned long millisMeasurePM=0;

void getCloudConfig () {

  std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
  client->setInsecure();

  HTTPClient https;
  
// Send request
  https.useHTTP10(true);
  https.begin(*client, "URL");
  int httpCode = https.GET();

  if (httpCode > 0) {
    Serial.println("GET response code: " + String(httpCode));
  }
  
// Parse response
  StaticJsonDocument<500> doc;
  DeserializationError error = deserializeJson(doc, https.getStream());

  if (error) {
    Serial.println(error.f_str());
  }
  
// Read cloud config values
  DEBUG = doc["DEBUG"].as<bool>();
  INTERVAL_M = doc["INTERVAL_M"].as<bool>();
  INTERVAL_CO = doc["timeoutCO"].as<int>();
  INTERVAL_PM = doc["timeoutPM"].as<int>();
  INTERVAL_WAKEUP = doc["timeoutWake"].as<int>();
  cloudWS = doc["cloudWS"].as<bool>();         

// Disconnect
  https.end();
}

void setup() {
  delay(5000);  
  Serial.begin(9600);                       // WEMOS works at 9600
  Serial.println("Wemos 9600 init"); 

// WIFI setup
  Serial.println("Connecting WIFI");
  WiFi.begin(ssid,pass);
    
  if (DEBUG) {
    Serial.println("Connecting to " + String(ssid));
    }

  int wifi_try = 0;
  while ( (WiFi.status() != WL_CONNECTED) && (wifi_try < 10) ) {
    if (DEBUG) {
        Serial.println("Couldn't connect on the try " + String(wifi_try));
      }
    wifi_try ++;
    delay(1000);
    }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Error connecting to WIFI after " + String(wifi_try) + " tries");
    Serial.println("WebServer is not supported");
    Serial.println("CloudConfig is not supported, using defaults");
    }
  else {
    Serial.println("Connected to WIFI on " + String(wifi_try) + " try");
    Serial.println(WiFi.localIP());
    wifisignal = true;

  //begin reading cloud configuration file
    std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
    client->setInsecure();
    HTTPClient https;
    
  // Send request
    https.useHTTP10(true);
    https.begin(*client, "https://mkushner.github.io/cloudConfig.json");
    int httpCode = https.GET();
  
    if (httpCode > 0) {
      Serial.println("GET response code: " + String(httpCode));
    }
    
  // Parse response
    StaticJsonDocument<500> doc;
    DeserializationError error = deserializeJson(doc, https.getStream());
  
    if (error) {
      Serial.println(error.f_str());
    }
    
  // Read cloud config values
    DEBUG = doc["DEBUG"].as<bool>();
    INTERVAL_M = doc["INTERVAL_M"].as<bool>();
    INTERVAL_CO = doc["timeoutCO"].as<int>();
    INTERVAL_PM = doc["timeoutPM"].as<int>();
    INTERVAL_WAKEUP = doc["timeoutWake"].as<int>();
    cloudWS = doc["cloudWS"].as<bool>();
  
  // Disconnect
    https.end();
    }

  Serial.println("DEBUG mode: " + String(DEBUG));
  Serial.println("cloudWS support: " + String(cloudWS));

//TS setup
  if (cloudWS) {
    ThingSpeak.begin(client);
    if (DEBUG) {
      Serial.println("TS ready");
      }
    }

// CO2 init
  myMHZ.begin(9600);   // MH-Z19b works at 9600                           
  if (DEBUG) {
    Serial.println("CO2 9600 init");
    }

  //myMHZ.write(cmd_conf, 9);                 // uncomment and change cmd_conf for setting up measurement range (default 0-5000)

  if (DEBUG) {
    Serial.println("PPM 0-5000");
    }
    
  if ( INTERVAL_M == false) {
    Serial.println("Interval ovverride: 30s"); 
    INTERVAL_CO = 30000;
    INTERVAL_PM = 30000;
    }

  if (DEBUG) {
    Serial.println("Interval_CO: " + String(INTERVAL_CO/1000) + "s");
    Serial.println("Interval_PM: " + String(INTERVAL_PM/1000) + "s");
    }

// SDS011 init
  if (DEBUG) {
    Serial.println("PM sensor init");
    }
  mySDS.begin(4, 14); //D1, D4, Tx, Rx

  if (DEBUG) {
    Serial.println("delay 5s to init sensors");
    }
  delay(5000);   
}

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

  if (DEBUG) { 
    if (!error) {
      Serial.println("P2.5: " + String(p25));
      Serial.println("P10:  " + String(p10));
      }
      else {
        Serial.println("error reading PM: code " + String(error));
      } 
    }

// call sleep() timing
  if (DEBUG && INTERVAL_M) {
    Serial.println("call sleep() on millis=" + String(millis()) + " millisMeasurePM=" + String(millisMeasurePM)); 
    }

// sleep if itervals are turned on
  if (INTERVAL_M) {
  mySDS.sleep();
  }

// write getPM timing
  millisMeasurePM = millis();
}

void getCO2() {

  if (DEBUG) {
    Serial.println("Checking Serial");
    Serial.println(myMHZ.available());
    Serial.println("Sending Serial CMD");
    } 
  else {
    myMHZ.available(); // handle 1-byte offset 
    }       

//clear buffer
  if ( !(response[0] == 0xFF && response[1] == 0x86) ) {
    Serial.println("CRC error: " + String(response[8]));
    sprintf(result_raw, "RAW: %02X %02X %02X %02X %02X %02X %02X %02X %02X", response[0], response[1], response[2], response[3], response[4], response[5], response[6], response[7], response[8]);
    Serial.println(result_raw);
    
    while (myMHZ.available()) {
      myMHZ.read();        
      }
    }
    
  myMHZ.write(MHZcmd, 9);
    
  if (DEBUG) {
    Serial.println("Reading response:");
    }
  memset(response, 0, 9);
  delay(100);
  myMHZ.readBytes(response, 9);

  if (DEBUG) {
    sprintf(result_raw, "RAW: %02X %02X %02X %02X %02X %02X %02X %02X %02X", response[0], response[1], response[2], response[3], response[4], response[5], response[6], response[7], response[8]);
    Serial.println(result_raw);
    }

  unsigned int responseHigh = (unsigned int) response[2];
  unsigned int responseLow = (unsigned int) response[3];

// check range consistency
  if ( ((256 * responseHigh) + responseLow) < 2000) {
    ppm = (256 * responseHigh) + responseLow;
    }

  if (DEBUG) {
    Serial.println("CO2 PPM:" + String(ppm));
    }

// write getCO timing
  millisMeasureCO = millis();  
}

void sendTSData(int dataSet) {

  switch (dataSet) {
    case 1:
      ThingSpeak.setField(1, ppm);
      break;
    case 2:
      ThingSpeak.setField(2, p25);
      ThingSpeak.setField(3, p10);
      break;
    default:
      Serial.println("Error reading dataSet switcher: expected 1/2 got " + String(dataSet));
    }
    
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

void loop() {   
  
  if ( ( millis() - millisMeasureCO ) > INTERVAL_CO )  {         // min delay recommended 10s not to overload MH-Z19b sensor
    if (DEBUG) {
      Serial.println("called  getCO2() on millis=" + String(millis()) + " millisMeasureCO=" + String(millisMeasureCO) ); 
      }
    getCO2();
  
    if (cloudWS && wifisignal) {
      if (DEBUG) {
        Serial.println("Writing CO data to cloud TS");
        }
      sendTSData(1);
      }
    }

  if ( ( millis() - millisMeasurePM ) > INTERVAL_PM ) {
    if (DEBUG) {
      Serial.println("called getPM() on millis=" + String(millis()) + " millisMeasurePM=" + String(millisMeasurePM)); 
      }
    getPM();

    if (cloudWS && wifisignal) {
      if (DEBUG) {
        Serial.println("Writing PM data to cloud TS");
        }
      sendTSData(2);
      }
    }  
}