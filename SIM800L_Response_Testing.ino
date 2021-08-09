#include <SoftwareSerial.h>

//Create software serial object to communicate with SIM800L
SoftwareSerial SIM800L(8, 9); //SIM800L Rx & Tx is connected to Arduino #8 & #9

void setup()
{
  Serial.begin(9600);
  SIM800L.begin(9600);

  SIM800L.println("AT\r\n");
  String _response = readResponse(1000);

  Serial.println(_response);
}

void loop(){
}

String readResponse(int delay_ms){
  delay(delay_ms);
  String _response = SIM800L.readString();
  return _response;
}
