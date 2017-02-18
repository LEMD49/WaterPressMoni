/*
  Water pressure reader
  by Etienne Maricq maricq@ieee.org
  ©2017
  Version 0.1
*/



void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  float sensorVoltage = analogRead(0); // read sensor voltage
  int psi = ((sensorVoltage - 95) / 205) * 50;
  Serial.print("Sensor Voltage(in mV): ");
  Serial.println(sensorVoltage);
  Serial.print("Water pressure (in PSI): ");
  Serial.println(psi);
  delay(1000);
}
