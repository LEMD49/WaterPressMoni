/*Adafruit MQTT Library ESP8266 Example
  Must use ESP8266 Arduino from:
    https://github.com/esp8266/Arduino

  Works great with Adafruit's Huzzah ESP board & Feather
  ----> https://www.adafruit.com/product/2471
  ----> https://www.adafruit.com/products/2821

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!
  Written by Tony DiCola for Adafruit Industries.
  MIT license, all text above must be included in any redistribution
  Modified 25Feb2017 by Etienne Maricq @LEMD49 maricq@ieee.org */

#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include <credentialsAdafruitIO.h>  //no need to put confidential details below now
const int lamp_pin = 5;  //GPIO5 is D1 on D1 mini

/*  WiFi Access Point and MQTT Broker setup, for manual def if not using my lib credentiaslAdafruitIO
  #define WLAN_SSID       "xxx"
  #define WLAN_PASS       "xxx"
  #define AIO_SERVER      "io.adafruit.com"
  #define AIO_SERVERPORT  1883                   // use 8883 for SSL
  #define AIO_USERNAME    "xxx"
  #define AIO_KEY         "xxx"
*/

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;
//WiFiClientSecure client;  //alternative with secure client

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// Setup a feed called 'sensorVolts' for publishing.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
Adafruit_MQTT_Publish sensorVolts = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/sensorVolts");

// Setup a feed called 'RSSI' for publishing.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
Adafruit_MQTT_Publish rssi = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/rssi");

// Setup a feed called 'onoff' for subscribing to changes.
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/onoff");


// Bug workaround for Arduino 1.6.6, it seems to need a function declaration
// for some reason (only affects ESP8266, likely an arduino-builder bug).
void MQTT_connect();

void setup() {
  Serial.begin(115200);
  delay(10);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(lamp_pin, OUTPUT);
  Serial.println(F("Adafruit MQTT demo"));

  // Connect to WiFi access point.
  Serial.println(); Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WLAN_SSID);



  WiFi.begin(WLAN_SSID, WLAN_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("WiFi connected");
  Serial.println("IP address: "); Serial.println(WiFi.localIP());

  // Setup MQTT subscription for onoff feed.
  mqtt.subscribe(&onoffbutton);
}

uint32_t x = 0;

void loop() {
  // Ensure the connection to the MQTT server is alive (this will make the first
  // connection and automatically reconnect when disconnected).  See the MQTT_connect
  // function definition further below.
  MQTT_connect();

  // this is our 'wait for incoming subscription packets' busy subloop
  // try to spend your time here

  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(5000))) {
    if (subscription == &onoffbutton) {
      Serial.print(F("Got On-Off button: "));
      Serial.println((char *)onoffbutton.lastread);
      /* Apply Message fo lamp  Since adafruit.io publishes the data as a string,
        using strcmp (string compare) is an easy way to determine whether you got a particular value. Don't forget it returns 0 on success! */
      if (strcmp((char *)onoffbutton.lastread, "ON") == 0) {
        digitalWrite(lamp_pin, HIGH);
        digitalWrite(BUILTIN_LED, HIGH);
        //Serial.println("degug loop led high");
      }
      if (strcmp((char *)onoffbutton.lastread, "OFF") == 0) {
        digitalWrite(lamp_pin, LOW);
        digitalWrite(BUILTIN_LED, LOW);
        //Serial.println("degug loop led low");
      }
    }
  }

  // Now we can publish stuff!
  x = analogRead(A0);
  Serial.print(F("\nSending sensorVolts val "));
  Serial.print(x);
  Serial.print("...");
  if (! sensorVolts.publish(x)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!"));
  }

  // Print RSSI
  int rssiVal = WiFi.RSSI();
  Serial.print("RSSI:");
  Serial.println(rssiVal);
 // rssi.publish(rssiVal);
  if (! rssi.publish(rssiVal)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK!"));
  }



  delay(1000);




  // ping the server to keep the mqtt connection alive
  // NOT required if you are publishing once every KEEPALIVE seconds
  /*
    if(! mqtt.ping()) {
    mqtt.disconnect();
    }
  */
}

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
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
