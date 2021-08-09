#include <Arduino.h>
#include <TimeLib.h>
#include <time.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

//Create software serial object to communicate with SIM800L
SoftwareSerial SIM800L(8, 9); //SIM800L Rx & Tx is connected to Arduino #8 & #9

const int rows = 10;
const int columns = 50;
const bool debug_print = true;
char response_array[rows][columns];

//forward declarations
void establishConnection();
time_t getTime();
unsigned int getResponse(int timeout);
void sendSMS();
void flushBuffer();

void setup()
{
  //hold lifeline
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);

  //test basic AT commands
  establishConnection();

  //check trap status and power off
  pinMode(6, INPUT);
  if (digitalRead(6) == HIGH){
    digitalWrite(7, LOW); //power off without write
  }

  time_t power_on_time = getTime();
  time_t last_power_on_time;
  EEPROM.get(0, last_power_on_time);

  //Serial.println(difftime(power_on_time, last_power_on_time));
  if (difftime(power_on_time, last_power_on_time) < 1800){ //30 minutes
    //EEPROM.put(0, power_on_time);
    digitalWrite(7, LOW); //power off with write
  }

  //sendSMS();

  //power off with write
  //EEPROM.put(0, power_on_time);
  digitalWrite(7, LOW);
}

void loop(){

}

void establishConnection(){
  Serial.begin(9600);
  SIM800L.begin(9600);
  flushBuffer();

  Serial.print("Initialized");
  delay(500);

  SIM800L.println("AT\r");
  getResponse(1000);
}

unsigned int getResponse(int timeout)
{
  memset(response_array, '\0', sizeof response_array); //clear array
  unsigned int i = 0;
  unsigned int j = 0;

  delay(timeout);

  while (SIM800L.available() > 0){
    char x = SIM800L.read();
    if (x != '\n' and x != '\r'){
      if (j < sizeof(response_array[0])){
        response_array[i][j] = x;
        j++;
      }
      else {
        Serial.println("OVERFLOW");
      }
    }
    else {
      if (j > 0){ //line is not empty
        response_array[i][j] = '\0';  //insert null-character at end of received string
        i++;
        j = 0;
      }
    }
  }

  //find usage data of array
  unsigned int num_used_rows = 0;
  for (int k=0; k<rows; k++){
    if (strlen(response_array[k]) > 0) {
      num_used_rows++;
    }
  }

  if (debug_print){
    for (unsigned int m=0; m<i; m++){
      Serial.println(response_array[m]);
    }
    Serial.println("");
  }

  //number of full rows
  return i - 1;
}

time_t getTime(){
  SIM800L.println("AT+SAPBR=0,1\r");
  getResponse(1000);

  //remove if wanting to test time error functionality
  SIM800L.println("AT+SAPBR=1,1\r");
  getResponse(3000);

  SIM800L.println("AT+CIPGSMLOC=2,1\r");
  int time_lines = getResponse(10000);

  if (strcmp("OK", response_array[time_lines]) == 0){
    Serial.println("Time response line: ");
    Serial.println(response_array[time_lines-1]);

    //declare new string with enough room to fit time data without prefix
    char strip_string[] = "+CIPGSMLOC: ";
    char current_time[strlen(response_array[time_lines-1]) - strlen(strip_string)];
    strcpy(current_time, &response_array[time_lines-1][strlen(strip_string)]);

    //if only location code is returned, it must be an error
    if (strstr(current_time, ",") == NULL){
      Serial.println("Error fetching time; Non-zero location code");
    }

    //otherwise parse date and time info
    char *date;
    char *time;
    strtok(current_time, ","); //remove location code
    date = strtok(NULL, ",");
    time = strtok(NULL, ",");

    Serial.print("Date is: ");
    Serial.println(date);
    Serial.print("Time is: ");
    Serial.println(time);

    uint8_t year = atoi(strtok(date, "/")) - 1970;
    uint8_t month = atoi(strtok(NULL, "/"));
    uint8_t day = atoi(strtok(NULL, "/"));
    uint8_t hour = atoi(strtok(time, ":"));
    uint8_t minute = atoi(strtok(NULL, ":"));
    uint8_t second = atoi(strtok(NULL, ":"));

    tmElements_t current_time_struct = {
      second, minute, hour,
      0, //weekday parameter is not important
      day, month, year
    };

    Serial.print("Unix Timestamp is: ");
    Serial.println(makeTime(current_time_struct));

    return makeTime(current_time_struct);
  }
  else{
    Serial.println("Error fetching time; Response was not 'OK'");
  }

}

void flushBuffer(){
  while(SIM800L.available())
  {
    SIM800L.read();
  }
}

void sendSMS(){
  SIM800L.print("AT+CMGF=1\r");
  getResponse(500);

  // USE INTERNATIONAL FORMAT CODE FOR MOBILE NUMBERS
  SIM800L.println("AT+CMGS=\"+64277606066\""); //+64277606066
  getResponse(500);

  //Message to send
  SIM800L.println("Test");
  getResponse(500);

  // End AT command with a ^Z, ASCII code 26
  SIM800L.println((char)26);
  getResponse(10000);
}
