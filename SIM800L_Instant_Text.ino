#include <SoftwareSerial.h>

//Create software serial object to communicate with SIM800L
SoftwareSerial SIM800L(8, 9); //SIM800L Rx & Tx is connected to Arduino #8 & #9

void setup()
{

  /*   
  AT+CPAS - check if device is ready
  AT+CGREG? - check registration status in network
  AT+CGATT? - check if device "attached to network"
  AT+CSQ - get signal level
  */
  //Begin serial communication with Arduino and Arduino IDE (Serial Monitor)
  Serial.begin(9600);
  
  //Begin serial communication with Arduino and SIM800L
  SIM800L.begin(9600);

  Serial.println("Initializing..."); 
  delay(1000);

  SIM800L.println("AT\r\n"); //Once the handshake test is successful, it will return OK
  updateSerial();

  SIM800L.println("AT+CSCS=\"GSM\""); //Set to GSM mode
  updateSerial();

  SIM800L.println("AT+SAPBR=1,1"); //
  updateSerial();
  
  SIM800L.println("AT+CMGF=1"); // Configuring TEXT mode
  updateSerial();
  SIM800L.println("AT+CNMI=1,2,0,0,0"); // Decides how newly arrived SMS messages should be handled
  updateSerial();

  sendSMS("TEST");
}

void loop()
{
  updateSerial();
}

void updateSerial()
{
  delay(500);
  while (Serial.available()) 
  {
    SIM800L.write(Serial.read());//Forward what Serial received to Software Serial Port
  }
  while(SIM800L.available()) 
  {
    Serial.write(SIM800L.read());//Forward what Software Serial received to Serial Port
  }
}

void sendSMS(String message){
  // AT command to set SIM800L to SMS mode
  SIM800L.print("AT+CMGF=1\r"); 
  delay(100);

  // REPLACE THE X's WITH THE RECIPIENT'S MOBILE NUMBER
  // USE INTERNATIONAL FORMAT CODE FOR MOBILE NUMBERS
  SIM800L.println("AT+CMGS=\"+XXXXXXXXXX\"");
  delay(100);
  // Send the SMS
  SIM800L.println(message); 
  delay(100);

  // End AT command with a ^Z, ASCII code 26
  SIM800L.println((char)26); 
  delay(100);
  SIM800L.println();
  // Give module time to send SMS
  delay(5000);  
}
