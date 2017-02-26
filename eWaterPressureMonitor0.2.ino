/*
  Water pressure reader
  by Etienne Maricq maricq@ieee.org
  ©2017
  Hardware
  1. sensor
  8-Dec-2015  5V 0-12HPa Press transducer SKU237545  $12.55   banggood  30-Dec-2015 banggood
  Working voltage 5.0VDC
  Output voltage 0.5 - 4.5VDC
  Working current < 10mA
  Max P 2.4 MPa
  Response time 2 ms
  Version 0.1 - testing sensor and display mv and converted pressure on serial monitor
  Version 0.2 - instead of powering with 5VDC, power with digital pin say D4
  as response time is 2ms, do a delay of 50ms to ensure stable sensor VCC

*/


int pin = 13; //This, GPIO13 is D7 according to diagram

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(pin, OUTPUT);

}

void loop() {
  // put your main code here, to run repeatedly:


  float sensorVoltage = analogRead(0); // read sensor voltage
  int psi = ((sensorVoltage - 95) / 205) * 50;
  Serial.print("Sensor Voltage(in mV): ");
  Serial.println(sensorVoltage);
  Serial.print("Water pressure (in PSI): ");
  Serial.println(psi);

  delay(2000);
  digitalWrite(pin, HIGH);
  delay(1000);
  digitalWrite(pin, LOW);

}
