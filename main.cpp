#include <Arduino.h>
#include <TimeLib.h>
#include <time.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

//Create software serial object to communicate with SIM800L
SoftwareSerial SIM800L(9, 8); //SIM800L Rx & Tx is connected to Arduino #8 & #9

const int ROWS = 10;
const int COLUMNS = 50;
const float BASE_RSSI = 20;
const bool DEBUG_PRINT = true;

char response_array[ROWS][COLUMNS];
int response_lines;

float rssi = 99;//default signal strength, (not yet known)
float rssi_diff = 1;

//forward declarations
void establishConnection();
void GSM_setup();
time_t getTime();
void getResponse2(int timeout, bool queries_network);
void getResponse1(int timeout);
void sendSMS();
void flushBuffer();
void reset_SIM();
void check_position();

void setup()
{
  //hold arduino lifeline
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);

  pinMode(10, OUTPUT);
  digitalWrite(10, HIGH);

  delay(500);

  //check trap status and power off if in the 'armed' position
  pinMode(11, INPUT_PULLUP);
  check_position();

  reset_SIM();
  check_position();

  //test basic AT commands
  establishConnection();
  check_position();

  GSM_setup();
  check_position();

  //get current power on time from network
  time_t power_on_time = getTime();
  check_position();

  // get last power on time from EEPROM
  time_t last_power_on_time;
  EEPROM.get(0, last_power_on_time);

  //Serial.println(difftime(power_on_time, last_power_on_time));
  if (difftime(power_on_time, last_power_on_time) < 1800){ //30 minutes
    //EEPROM.put(0, power_on_time);
    digitalWrite(7, LOW); //power arduino off
  }

  sendSMS();

 //write most recent time to EEPROM
  EEPROM.put(0, power_on_time);

  //power arduino off
  digitalWrite(7, LOW);
}

void loop(){
}

void establishConnection(){
  Serial.begin(9600);
  SIM800L.begin(9600);
  flushBuffer();

  Serial.print("Initialized");
  delay(1000);

  SIM800L.println("AT\r");
  getResponse1(3000);

  for(unsigned int m=0; m<100; m++){
    SIM800L.println("AT+CGREG?\r"); // Check if the device is registered
    getResponse1(5000);
    char reg_info[strlen(response_array[response_lines-1]) - strlen("+CGREG: ")];
    strcpy(reg_info, &response_array[response_lines-1][strlen("+CGREG: ")]);
    strtok(reg_info, ",");
    int reg = atoi(strtok(NULL, ","));
    if(reg == 0){ //not attempting to attach
      SIM800L.println("AT+CGREG=1"); //attempt to attach
      getResponse1(5000);
      }
    if(reg == 1){ //attached to network and ready to send/recive commands
      return;
    }


    delay(5000); //delay between attempts
  }
}

void GSM_setup(){
  SIM800L.println("AT+CSQ\r");
  getResponse2(10000, true);
  char signal_info[strlen(response_array[response_lines-1]) - strlen("+CSQ: ")];
  strcpy(signal_info, &response_array[response_lines-1][strlen("+CSQ: ")]);
  rssi = atoi(strtok(signal_info, ","));
  if(rssi != 99 and rssi != 0){
    rssi_diff = ((pow(10, (2*BASE_RSSI-114)/10))/(pow(10, (2*rssi-114)/10)));
  }
  Serial.print("RSSI: ");
  Serial.println(rssi);
  Serial.print("Signal Power Difference: ");
  Serial.println(rssi_diff);
  Serial.println("");

  SIM800L.println("AT+CSCS=\"GSM\"\r"); //Set to GSM mode
  getResponse1(1000);

  SIM800L.println("AT+CMGF=1\r"); // Configuring TEXT mode
  getResponse1(1000);

  SIM800L.println("AT+CNMI=1,2,0,0,0\r"); // Decides how newly arrived SMS messages should be handled
  getResponse1(1000);
}

void getResponse2(int base_timeout, bool queries_network){

  memset(response_array, '\0', sizeof response_array); //clear response array
  int timeout;
  unsigned int i = 0, j= 0;
  int time_since_last_char = 0;
  int delay_time = 20;

  if(queries_network){
    //convert signal quality to a timeout time here (UPDATE THIS METHOD)
    if(rssi == 99){
      timeout = 3*base_timeout;
    }
    else{
      Serial.print(rssi_diff*base_timeout);
      timeout = base_timeout + (rssi_diff*base_timeout);
    }
  }
  else{ //if the command has no network interaction, the timeout duration is fixed
    timeout = base_timeout;
  }

  Serial.print("timeout - ");
  Serial.print(timeout);


  while(time_since_last_char < timeout){ //waits until timeout (UNFINISHED)
    if (SIM800L.available() > 0){
      //read new lines as soon as they arrive
      time_since_last_char = 0;
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
              for(unsigned int m=0; m<i+1; m++){
                Serial.println(response_array[m]);
              }
              Serial.println(""); //inserts blank line between AT commands
            }
            response_lines = i; //i is the number of full rows
            return;
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
  Serial.println("TIMEOUT");
}

void getResponse1(int base_timeout){
  getResponse2(base_timeout, false); //uese default argument
}

time_t getTime(){
  SIM800L.println("AT+SAPBR=0,1\r");
  getResponse1(5000);

  //remove this line to test time error functionality
  SIM800L.println("AT+SAPBR=1,1\r");
  getResponse1(5000);

  SIM800L.println("AT+CIPGSMLOC=2,1\r");
  getResponse2(10000, true);

  if (strcmp("OK", response_array[response_lines-1]) == 0){
    Serial.println("Time response line: ");
    Serial.println(response_array[response_lines-1]);

    //declare new string with enough room to fit time data without prefix
    char strip_string[] = "+CIPGSMLOC: ";
    char current_time[strlen(response_array[response_lines-1]) - strlen(strip_string)];
    strcpy(current_time, &response_array[response_lines-1][strlen(strip_string)]);

    //if only location code is returned, it must be an error
    if (strstr(current_time, ",") == NULL){
      Serial.println("Error fetching time; Non-zero location code");
    }

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
  getResponse1(1000);

  // USE INTERNATIONAL FORMAT CODE FOR MOBILE NUMBERS
  SIM800L.print("AT+CMGS=\"+64277606066\"\r"); //+64277606066
  getResponse1(5000);

  //Message to send
  SIM800L.print("Your stoat trap was set off");
  getResponse1(5000);

  // End AT command with a ^Z, ASCII code 26
  SIM800L.println((char)26);
  getResponse2(15000, true);
  delay(15000);
}

void reset_SIM(){
  digitalWrite(10, LOW);
  delay(100);
  digitalWrite(10, HIGH);
  delay(5000);
}

void check_position(){
  if (digitalRead(11)){
    digitalWrite(7, LOW);
  }
}
