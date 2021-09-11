#include <Arduino.h>
#include <TimeLib.h>
#include <time.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

//Create software serial object to communicate with SIM800L
SoftwareSerial SIM800L(8, 9); //SIM800L Rx & Tx is connected to Arduino #8 & #9

const int ROWS = 10;
const int COLUMNS = 50;
const float BASE_RSSI = 20;
const bool DEBUG_PRINT = true;
char response_array[ROWS][COLUMNS];

float rssi = 99;//default signal strength, (not yet known)
float rssi_diff = 1;

//forward declarations
void establishConnection();
void waitForGPRS(int setup_delay, int timeout);
time_t getTime();
unsigned int getResponse(int timeout);
void sendSMS();
void flushBuffer();
void test_response_time();

void setup()
{
  //hold lifeline
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);

  //check trap status and power off
  pinMode(6, INPUT);
  if (digitalRead(6) == HIGH){
    digitalWrite(7, LOW); //power off without write
  }

  //test basic AT commands
  establishConnection();
  //test_response_time();

  time_t power_on_time = getTime();
  time_t last_power_on_time;
  EEPROM.get(0, last_power_on_time);
  // get last power on time from EEPROM

  //Serial.println(difftime(power_on_time, last_power_on_time));
  if (difftime(power_on_time, last_power_on_time) < 1800){ //30 minutes
    //EEPROM.put(0, power_on_time);
    digitalWrite(7, LOW); //power off with write
  }

  //sendSMS();

  //power off with write
  //EEPROM.put(0, power_on_time);
  //write new time to EEPROM
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

  SIM800L.println("AT+CSQ\r");
  int quality_lines = getResponse(5000);
  char signal_info[strlen(response_array[quality_lines-1]) - strlen("+CSQ: ")];
  strcpy(signal_info, &response_array[quality_lines-1][strlen("+CSQ: ")]);
  rssi = atoi(strtok(signal_info, ","));
  rssi_diff = ((pow(10, (2*BASE_RSSI-114)/10))/(pow(10, (2*rssi-114)/10)));
  Serial.print("RSSI: ");
  Serial.println(rssi);
  Serial.print("Signal Power Difference: ");
  Serial.println(rssi_diff);

  SIM800L.println("AT+CSCS=\"GSM\"\r"); //Set to GSM mode
  getResponse(1000);

  SIM800L.println("AT+CMGF=1\r"); // Configuring TEXT mode
  getResponse(1000);

  SIM800L.println("AT+CNMI=1,2,0,0,0\r"); // Decides how newly arrived SMS messages should be handled
  getResponse(1000);
}

void test_response_time(){
  SIM800L.println("AT+SAPBR=0,1\r");
  getResponse(1000);

  //remove if wanting to test time error functionality
  SIM800L.println("AT+SAPBR=1,1\r");
  getResponse(3000);

  SIM800L.println("AT+CIPGSMLOC=2,1\r"); //Set to GSM mode
  long current_time = millis();
  int current_chars = 0;
  int previous_chars = 0;
  int time_since_last_char = 0;
  int delay_time = 20;

  while(time_since_last_char < 10000){
    current_chars = SIM800L.available();
    if (current_chars > previous_chars){
      time_since_last_char += delay_time;
      Serial.println("More chars");
      Serial.println(current_chars);
      Serial.println(millis()-current_time);
    }
    else{
      time_since_last_char += delay_time;
    }
    previous_chars = current_chars;
    delay(delay_time);
  }
}

void waitForGPRS(int setup_delay, int timeout){
  delay(setup_delay);
  SIM800L.println("AT+CREG=?\r"); // Check if the device is registered
  getResponse(1000);

  SIM800L.println("AT+CGATT=?\r"); // Configuring TEXT mode
  getResponse(1000);
}

unsigned int getResponse(int base_timeout){

  memset(response_array, '\0', sizeof response_array); //clear response array
  unsigned int i = 0, j= 0;

  int time_since_last_char = 0;
  int delay_time = 20;

  while(time_since_last_char < 10000){ //waits until timeout (UNFINISHED)
    if (SIM800L.available() > 0){
      //read new lines as soon as they arrive
      char x = SIM800L.read();
      if (x != '\n' and x != '\r'){
        if (j < sizeof(response_array[0])){ //ensure no overflow occurs
          response_array[i][j] = x;
          j++;
        }
        else {
          Serial.println("OVERFLOW");
        }
      }
      else {
        if (j > 0){ //if line is not empty (ignore empty lines)
          response_array[i][j] = '\0';  //insert null-character at end of received string
          if(strcmp("OK", response_array[i]) == 0){ // if OK response recieved (end of response)
            if (DEBUG_PRINT){
              for (unsigned int m=0; m<i+1; m++){
                Serial.println(response_array[m]);
              }
              Serial.println(""); //inserts blank line between AT commands
            }
            return i;  //i is the number of full rows
          }
          i++;
          j = 0;
        }
      }
    }
    else{ //increment time since last reception
      time_since_last_char += delay_time;
    }
    delay(delay_time);
  }
  //TIMEOUT CODE HERE
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
