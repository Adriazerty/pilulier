// https://fr.rs-online.com/web/p/interrupteurs-de-fin-de-course/7768542?gb=s
// https://fr.rs-online.com/web/p/interrupteurs-de-fin-de-course/2543103?gb=s

#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <string.h>

#define GMT_OFFSET 1  //France : GMT+1
#define ASCII_OFFSET 0x30 //link ascii digits to integer digits
#define pinSwitch1 4 
#define pinSwitch2 32
#define pinSwitch3 16
#define pinSwitch4 17
#define pinSwitch5 21
int stateSwitch1 = 0;
int stateSwitch2 = 0;
int stateSwitch3 = 0;
int stateSwitch4 = 0;
int stateSwitch5 = 0;
String formattedTime; //GMT time in string format
uint8_t integerTime[3]; //hour, minute, second integer array
enum time {hour, minute, second}; //names previous array cells

const char *SSID = "Redmidedridri"; //Device name
const char *PWD = "dridrilefour"; //Wifi password
unsigned long lastMillis = 0; //Wifi retry variable

WiFiUDP ntpUDP; //define WifiUDP Class instance
NTPClient timeClient(ntpUDP, "pool.ntp.org");//define NTPClient Class instance

#define FCLK 80000000UL //basic clock frquency
#define PRESCALER 8000  //divides the clock frquency
#define Te 1 //checking period in s
const uint16_t nbOfTicks = FCLK/PRESCALER*Te; //definition of number of ticks with previous parameters

hw_timer_t *timer = NULL; //creates an object "timer" which contain settings to apply as a pointer on a hardware timer
bool timeCheck = false;

/* Function executed once each interrupt */
void Timer_ISR(){
  timeCheck = true;  //let active time collection
}

String get_wifi_status(int status){
  switch(status){
    case WL_IDLE_STATUS:
      return "Idle";
    case WL_SCAN_COMPLETED:
      return "Scan completed";
    case WL_NO_SSID_AVAIL:
      return "No SSID available";
    case WL_CONNECT_FAILED:
      return "Connection failed";
    case WL_CONNECTION_LOST:
      return "Connection lost";
    case WL_CONNECTED:
      return "Connected";
    case WL_DISCONNECTED:
      return "Disconnected";
    default :
      return "Default";
    break;
  }
}

void connectToWiFi(){
  int status = WL_IDLE_STATUS;
  Serial.println(PWD);
  Serial.print("Connecting to WiFi : ");
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PWD);
  status = WiFi.status();
  Serial.println(get_wifi_status(status));
  Serial.println(SSID);
  lastMillis = millis();
  while (status != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
    status = WiFi.status();
    Serial.println(get_wifi_status(status));
    Serial.print("remaining time: ");
    Serial.println(millis() - lastMillis);
    if (millis() - lastMillis > 20000) {
      lastMillis = millis();
      Serial.println("This is taking a lot of time, restarting");
      ESP.restart();
    }
  }

  Serial.println("Connection success.");
  Serial.print("Local ESP32 IP: ");
  Serial.println(WiFi.localIP());  
}

void getTime(uint8_t *intTime){
  formattedTime = timeClient.getFormattedTime();  //return GMT time in string format
  char arrayTime[formattedTime.length()+1]; 
  strcpy(arrayTime, formattedTime.c_str()); //convert string formated time to a char array
  /*fill in an integer array with time from previous char array*/
  integerTime[hour] = (arrayTime[0]-ASCII_OFFSET)*10 + arrayTime[1]-ASCII_OFFSET + GMT_OFFSET;
  integerTime[minute] = (arrayTime[3]-ASCII_OFFSET)*10 + arrayTime[4]-ASCII_OFFSET;
  integerTime[second] = (arrayTime[6]-ASCII_OFFSET)*10 + arrayTime[7]-ASCII_OFFSET;
}

/*Definition of 4 deadlines activating the system*/
#define NB_OF_DL 4

const uint8_t deadLines[NB_OF_DL][3] = {
  {9, 30, 0},
  {14, 0, 0},
  {18, 30, 0},
  {20, 0, 0},
};

int stateTime = 0;
int stateSwitch = 0;

int stateTimeReturn(){
  for(int i = 0; i <= NB_OF_DL ; i++){
    if(integerTime[hour] < deadLines[i][hour]){
      return i;
    }
    else if(integerTime[hour] == deadLines[i][hour]){
      if(integerTime[minute] < deadLines[i][minute]){
        return i;
      }
      else{
        return i + 1;
      }
    }
  }
  return 12;
}

int stateSwitchReturn(){
  if(stateSwitch1 == LOW && stateSwitch2 == LOW && stateSwitch3 == LOW  && stateSwitch4 == LOW && stateSwitch5 == LOW){
    return 0;
  }
  else if(stateSwitch1 == HIGH && stateSwitch2 == LOW && stateSwitch3 == LOW  && stateSwitch4 == LOW && stateSwitch5 == LOW){
    return 1;
  }
  else if(stateSwitch1 == HIGH && stateSwitch2 == HIGH && stateSwitch3 == LOW  && stateSwitch4 == LOW && stateSwitch5 == LOW){
    return 2;
  }
  else if(stateSwitch1 == HIGH && stateSwitch2 == HIGH && stateSwitch3 == HIGH  && stateSwitch4 == LOW && stateSwitch5 == LOW){
    return 3;
  }
  else if(stateSwitch1 == HIGH && stateSwitch2 == HIGH && stateSwitch3 == HIGH  && stateSwitch4 == HIGH && stateSwitch5 == LOW){
    return 4;
  }
  else if(stateSwitch1 == HIGH && stateSwitch2 == HIGH && stateSwitch3 == HIGH  && stateSwitch4 == HIGH && stateSwitch5 == HIGH){
    return 5;
  }
  else{
    return 13;
  }
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Motion (l293d) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#define INPUT1 26 //connected to l293 pin 2 
#define INPUT2 17 //connected to l293 pin 7
#define ENABLE 13 //connected to l293 pin 1


enum direction {closingDirection, openingDirection, stop}; //links 0,1,2 to different directions
bool action = false;
bool isRunning = false;
int dutyCycle = 200;

void motorMotion(int direction){
  switch(direction){
    case closingDirection :
      digitalWrite(INPUT1, 0);
      digitalWrite(INPUT2, 1);
    break;
    case openingDirection :
      digitalWrite(INPUT1, 1);
      digitalWrite(INPUT2, 0);
    break;
    case stop :
      digitalWrite(INPUT1, 0);
      digitalWrite(INPUT2, 0);
    break;
    default :
      Serial.println("direction parameter you defined does not exixt !");
    break;
  }
}


/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Execution ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
void setup(){
  Serial.begin(115200);

  /*Wifi and NTP client setup*/
  connectToWiFi();
  timeClient.begin();
  delay(1000);

  /*timer setup*/
  timer = timerBegin(3, PRESCALER, true); //timer 3, prescaler : 8000, count up
  timerAttachInterrupt(timer, &Timer_ISR, true); //pointer on timer object, pointer on our interrupt function, append on rising edge (timer overflow in our case)
  timerAlarmWrite(timer, nbOfTicks, true); //pointer on timer object, overflow value on the timer, autoreload
  timerAlarmEnable(timer);

  /*Pinout setup*/
  pinMode(INPUT1, OUTPUT);
  pinMode(INPUT2, OUTPUT);
  pinMode(ENABLE, OUTPUT);
  pinMode(pinSwitch1, INPUT);
  pinMode(pinSwitch2, INPUT);  
  pinMode(pinSwitch3, INPUT);  
  pinMode(pinSwitch4, INPUT);  
  pinMode(pinSwitch5, INPUT);
}

void loop(){
  Serial.println("bonjour");  
  int stateSwitch1 = digitalRead(pinSwitch1);
  int stateSwitch2 = digitalRead(pinSwitch2);  
  int stateSwitch3 = digitalRead(pinSwitch3);  
  int stateSwitch4 = digitalRead(pinSwitch4);  
  int stateSwitch5 = digitalRead(pinSwitch5);    
  delay(1000);
  if (WiFi.status() != WL_CONNECTED) { 
    Serial.println("WiFi connection lost. Reconnecting...");
    connectToWiFi();
  }
  else if(timeCheck){ //execute only if the device is connected to wifi
    timeCheck = false;
    timeClient.update();
    getTime(integerTime);

    Serial.print(integerTime[hour]);
    Serial.print(':');
    Serial.print(integerTime[minute]);
    Serial.print(':');
    Serial.print(integerTime[second]);
    Serial.println();
    stateTime = stateTimeReturn();
    Serial.println(stateTime);
    int stateSwitch = stateSwitchReturn();
    Serial.println(stateSwitch);
  }
  if(stateTime == 0 && stateSwitch != 0){
    motorMotion(closingDirection);
  }
  else if(stateTime == 1 && stateSwitch != 2){
    if(stateSwitch > 2){
      motorMotion(closingDirection);
    }
    if(stateSwitch < 2){
      motorMotion(openingDirection);
    }
    else{
      motorMotion(stop);
    }
  }
  else if(stateTime == 2 && stateSwitch != 3){
    if(stateSwitch > 3){
      motorMotion(closingDirection);
    }
    if(stateSwitch < 3){
      motorMotion(openingDirection);
    }
    else{
      motorMotion(stop);
    }
  }
  else if(stateTime == 3 && stateSwitch != 4){
    if(stateSwitch > 4){
      motorMotion(closingDirection);
    }
    if(stateSwitch < 4){
      motorMotion(openingDirection);
    }
    else{
      motorMotion(stop);
    }
  }
  else if(stateTime == 4 && stateSwitch != 5){
    motorMotion(closingDirection);
  }
  else{
    motorMotion(stop);
  }
}