/*Adafruit MQTT Library ESP8266 applicatio  https://github.com/esp8266/Arduino
  Written by Tony DiCola for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
  Modified 25Feb2017 by Etienne Maricq @LEMD49 maricq@ieee.org */
/*
  Hardware

  1. sensor

  8-Dec-2015  5V 0-1.2MPa (0 to 12 bar) Press transducer SKU237545  $12.55   banggood  30-Dec-2015 banggood
  Working voltage 5.0VDC
  Output voltage 0.5 - 4.5VDC
  Working current < 10mA
  Max P 2.4 MPa
  Response time 2 ms

  2. MCU  D1 Mini with ESP8266 12

  Version 0.1 - testing sensor and display mv and converted pressure on serial monitor
  Version 0.2 - instead of powering with 5VDC, power with digital pin say D4
  as response time is 2ms, do a delay of 50ms to ensure stable sensor VCC //not done
  Version 0.3 - hook up to AIO MQTT (Adafruit). Display data and command button for control
  Version 0.4 - calibration of sensor volts (x) vs indicated pressure by SD boiler (y)
  Version 0.5 - LED_BUILTIN is broken. Used another def.
  Version 0.6 - new calibration, OTA, clean up
  Version 0.7 - onced triggered for LP sends notifications forever, so define notifCtr to limit. Local var in loop()
  Version 0.8 - added first order filtering to A0. See eSmoothAnalogPhotocell.ino
  Version 0.9 - modified filter only 5 samples

  LED flashes during WiFi connection
  LED is ON once WiFi connected and initiates OTA
  LED remains  ON during OTA allowed time
  Then LED subcribes to MQTT onoff switch
  

  There are non-ideal elements
  - no absolute standards were used, this is an empirical calibration
  - because of the expansion vase the initial response to increased volume is non-linear i.e. PV/T != ct since volume is variable until
  expansion vase pressure balances cct pressure
  - strong histerysis
  Not a major factor however since the output is used as a early warning system before cutoff of boiler when indicated pressure is at 0.6 bar or below

  CALIBRATION DATA
  x sensorValue from Pressure transducer
  y indicated pressure by digital manometer of SD boiler
  n =>5 datapoints per station
  measure going up to 1.9 and down to 0.7 bar
  initial conditions: sensorVolts: 236, indicated pressure: 1.0 bar

  Linear equation for increasing pressure:
  y = 0.01x - 1.5558
  Linear equation for decreasing pressure:
  y = 0.0111x - 1.5876
  y = 0.007x -0.7358  new cal R^2 is 0.82

  Vcc 5v from micro USB or 2-screw terminal
  A0 input sensor from 3-pin header or 3-screw terminal
  D0 output to 3.3 to 5.0 level shifter to 2-pin header
*/

#define FILTER
#define DEBUG
#include <ESP8266WiFi.h>  //basic w/o OTA
#include <ESP8266mDNS.h>  //for OTA
#include <WifiUdp.h>      //for OTA
#include <ArduinoOTA.h>   //for OTA
#include "Adafruit_MQTT.h"  //basic w/o OTA
#include "Adafruit_MQTT_Client.h"  //basic w/o OTA
#include <credentialsAdafruitIO.h>  //no need to put confidential details below now

static const uint8_t LED_ESP = 2;// GPIO2 or D4
float x = 0.0;
float y = 0.0;
float filteredx = 0.0;
uint8_t pinD0 = 16;  //pin D0 is GPIO 16 on D1 Mini
bool LPAlarm = 0;
uint8_t notifCtr = 0; //notification counter to limit notifications


/*  WiFi Access Point and MQTT Broker setup, for manual def if not using my lib credentiaslAdafruitIO
  #define WLAN_SSID       "xxx"
  #define WLAN_PASS       "xxx"
  #define AIO_SERVER      "io.adafruit.com"
  #define AIO_SERVERPORT  1883                   // use 8883 for SSL
  #define AIO_USERNAME    "xxx"
  #define AIO_KEY         "xxx"
*/


WiFiClient client; // Create an ESP8266 WiFiClient class to connect to the MQTT server.
//WiFiClientSecure client;  //alternative with secure client

Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY); // Setup the MQTT client class

// Setup feeds using the form: <username>/feeds/<feedname> for publish and subscribe
Adafruit_MQTT_Publish sensorValue = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/sensorValue");
Adafruit_MQTT_Publish boilerPressure = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/boilerPressure");
Adafruit_MQTT_Publish rssi = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/rssi");
Adafruit_MQTT_Publish LowPressAlarm = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/LowPressAlarm");
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/onoff");

void MQTT_connect();

void setup() {

  pinMode(LED_ESP, OUTPUT); //not needed, for checking command button. active low

#ifdef DEBUG
  Serial.begin(115200);
  delay(10);


  Serial.println();
  Serial.println(F("<< eWater Pressure Monitoring with MQTT >>"));
  Serial.println();
  Serial.print("Connecting to ");// Connect to WiFi access point.
  Serial.println(WLAN_SSID4);
#endif

  WiFi.begin(WLAN_SSID4, WLAN_PASS4);
  while (WiFi.status() != WL_CONNECTED) {

    //blink to show connected Wifi _.  negative logic
    delay(500);
#ifdef DEBUG
    Serial.print(".");
#endif
    digitalWrite(LED_ESP, !HIGH);
    delay(100);
    digitalWrite(LED_ESP, !LOW);
    delay(100);
    digitalWrite(LED_ESP, !HIGH);
    delay(100);
    digitalWrite(LED_ESP, !LOW);

  }
#ifdef DEBUG
  Serial.println();
  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());
#endif

  //OTA Setup
  ArduinoOTA.setPort(8266); //  Port default 8266
  ArduinoOTA.setHostname("D1mini1"); //  Host name default esp8266-chipID
  ArduinoOTA.setPassword((const char *)"4949");
  ArduinoOTA.onStart([]() {
    #ifdef DEBUG
    Serial.println(F("Start"));
    #endif
  });
  ArduinoOTA.onEnd([] () {
    #ifdef DEBUG
    Serial.println(F("\nEnd"));
    #endif
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    #ifdef DEBUG
    Serial.printf("Progresss: %u%%\r", (progress / (total / 100)));
    #endif
  });
  ArduinoOTA.onError([](ota_error_t error) {
    #ifdef DEBUG
    Serial.printf("Error[%u]:", error);
    if  (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
    else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
    #endif
  });
  ArduinoOTA.begin();
  #ifdef DEBUG
  Serial.println(F("OTA Ready"));
  #endif

  mqtt.subscribe(&onoffbutton); // Setup MQTT subscription onoff feed.
}


bool otaFlag = true;
uint32_t timeElapsed = 0;

void loop() {
  //120 sec allowing OTA
  if (otaFlag) {
    while (timeElapsed < 120000) {
      //wait 120 s for upload
      ArduinoOTA.handle();
      digitalWrite(LED_ESP, !HIGH);
      timeElapsed = millis();
      #ifdef DEBUG
      Serial.print("\ntimeElapsed is: ");
      Serial.println(timeElapsed);
      #endif
      delay(10);
    }
    otaFlag = false;
    digitalWrite(LED_ESP, !LOW);
  }

  MQTT_connect(); // Ensure the connection to the MQTT server is alive


  Adafruit_MQTT_Subscribe *subscription;  //check button command
  while ((subscription = mqtt.readSubscription(5000))) {
    if (subscription == &onoffbutton) {
      #ifdef DEBUG
      Serial.print(F("Got On-Off button: "));
      Serial.println((char *)onoffbutton.lastread);
      #endif
      
      /* Apply Message fo lamp  Since adafruit.io publishes the data as a string,
        using strcmp (string compare) is an easy way to determine whether you got a particular value. Don't forget it returns 0 on success! */
      if (strcmp((char *)onoffbutton.lastread, "ON") == 0) {
        digitalWrite(LED_ESP, !HIGH); //active low
      }
      if (strcmp((char *)onoffbutton.lastread, "OFF") == 0) {
        digitalWrite(LED_ESP, !LOW);//active low
      }
    }
  }

  //publish to MQTT broker
  x = analogRead(A0); //value of pressure sensorVolts

  #ifdef FILTER
  filteredx = firstOrderFilter(x, filteredx, 5);
  x = filteredx;
  #endif
 
  delay(10); //response time of sensor 2ms
  y = x * 0.007 - 0.7358; //equation from down going linear regression of x vs y

  if (y >= 1.0) {
    notifCtr = 0;
  }
  else {
    notifCtr++;
  }
  #ifdef DEBUG
  Serial.print("\nnotifCtr after  if pres ok is: ");
  Serial.println(notifCtr);
  #endif
  
  if (y < 1.0 && notifCtr < 3) {
    LPAlarm = 1; //set low pressure alarm high so that 3 times MQTT notifies
    LowPressAlarm.publish(LPAlarm);

  }

  else {
    LPAlarm = 0; //reset warning as already notified 3 times
    LowPressAlarm.publish(LPAlarm);  //reset feed also
  }

  if (notifCtr > 10) {
    notifCtr = 10; //to avoid overflow since device on long time
  }
  #ifdef DEBUG
  Serial.print("\nnotifCtr after  if pres not ok is: ");
  Serial.println(notifCtr);
  Serial.print("\nLPAlarm is: ");
  Serial.println(LPAlarm);
  #endif
  
  /*
    if (! LowPressAlarm.publish(LPAlarm)) {
      Serial.println(F("Failed"));
    } else {
      Serial.println(F("OK!"));

    else {
      LPAlarm = 0; //reset warning
      LowPressAlarm.publish(LPAlarm);  //reset feed also
    }



  */
  #ifdef DEBUG
  Serial.print(F("\n*************"));
  Serial.print(F("\nSending sensorValue "));
  Serial.print(x); Serial.print("...");
  #endif
  
  if (! sensorValue.publish(x)) {
    #ifdef DEBUG
    Serial.println(F("Failed"));
    #endif
    
  } else {
    #ifdef DEBUG
    Serial.println(F("OK!"));
    #endif
    
  }

  Serial.print(F("Sending boilerPressure (bar) "));
  y = constrain(y, 0, 12); //constrain the calculated value to positive sensor range
  Serial.print(y);
  Serial.print("...");
  if (! boilerPressure.publish(y)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!"));
  }

  // Print RSSI  (-50 excellent, -75 good, -90 at limit)
  int rssiVal = WiFi.RSSI();
  Serial.print(F("RSSI:"));
  Serial.print(rssiVal);
  Serial.print("...");
  // rssi.publish(rssiVal);  //without safety net

  if (! rssi.publish(rssiVal)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!"));
  }
  delay(30000);


  /*// ping the server to keep the mqtt connection alive
    if(! mqtt.ping()) {
    mqtt.disconnect();
    }// NOT required if you are publishing once every KEEPALIVE seconds
  */
}

//filtering function

float firstOrderFilter(float inputValue, float currentValue, uint16_t filter) {
  float newValue = (inputValue + (currentValue * filter)) / (filter + 1);
  return newValue;
}

void MQTT_connect() {   //connect and reconnect as necessary to the MQTT server
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
    retries--;
    if (retries == 0) {
      // basically die and wait for WDT to reset me
      while (1);
    }
  }
  Serial.println("MQTT Connected!");
}
