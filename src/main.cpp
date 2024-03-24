// https://fr.rs-online.com/web/p/interrupteurs-de-fin-de-course/7768542?gb=s
// https://fr.rs-online.com/web/p/interrupteurs-de-fin-de-course/2543103?gb=s

#include <Arduino.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <string.h>
#include <WebServer.h> 

#define GMT_OFFSET 1  //France : GMT+1
#define ASCII_OFFSET 0x30 //link ascii digits to integer digits
#define pinSwitch1 4 
#define pinSwitch2 32
#define pinSwitch3 16
#define pinSwitch4 17
#define pinSwitch5 21
int stateSwitchArray[5] = {0, 0, 0, 0, 0};
int previousStateSwitchArray[5];
String formattedTime; //GMT time in string format
uint8_t integerTime[3]; 
enum time {hour, minute, second}; 

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
  integerTime[hour] = (arrayTime[0]-ASCII_OFFSET)*10 + arrayTime[1]-ASCII_OFFSET + GMT_OFFSET;
  integerTime[minute] = (arrayTime[3]-ASCII_OFFSET)*10 + arrayTime[4]-ASCII_OFFSET;
  integerTime[second] = (arrayTime[6]-ASCII_OFFSET)*10 + arrayTime[7]-ASCII_OFFSET;
}

#define NB_OF_DL 4
int deadLines[NB_OF_DL][3] = {
  {9, 30, 0},
  {14, 0, 0},
  {18, 30, 0},
  {20, 0, 0},
};

int stateTime = 0;
int stateSwitch = 0;
bool isActivated = 0;

void previousState(){
  for(int i = 0; i <= 4; i++){
    previousStateSwitchArray[i] = stateSwitchArray[i];
  }
}

bool isActivatedReturn(){
  for(int i = 0; i <= 4; i++){
    if(previousStateSwitchArray[i] != stateSwitchArray[i]){
      return true;
    }
  }
  return false;
}

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
  if(stateSwitchArray[0] == LOW && stateSwitchArray[1] == LOW && stateSwitchArray[2] == LOW  && stateSwitchArray[3] == LOW && stateSwitchArray[4] == LOW){
    return 0;
  }
  else if(stateSwitchArray[0] == HIGH && stateSwitchArray[1] == LOW && stateSwitchArray[2] == LOW  && stateSwitchArray[3] == LOW && stateSwitchArray[4] == LOW){
    return 1;
  }
  else if(stateSwitchArray[0] == HIGH && stateSwitchArray[1] == HIGH && stateSwitchArray[2] == LOW  && stateSwitchArray[3] == LOW && stateSwitchArray[4] == LOW){
    return 2;
  }
  else if(stateSwitchArray[0] == HIGH && stateSwitchArray[1] == HIGH && stateSwitchArray[2] == HIGH  && stateSwitchArray[3] == LOW && stateSwitchArray[4] == LOW){
    return 3;
  }
  else if(stateSwitchArray[0] == HIGH && stateSwitchArray[1] == HIGH && stateSwitchArray[2] == HIGH  && stateSwitchArray[3] == HIGH && stateSwitchArray[4] == LOW){
    return 4;
  }
  else if(stateSwitchArray[0] == HIGH && stateSwitchArray[1] == HIGH && stateSwitchArray[2] == HIGH  && stateSwitchArray[3] == HIGH && stateSwitchArray[4] == HIGH){
    return 5;
  }
  else{
    return 13;
  }
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Wifi server ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
WebServer myServer(80);

struct User {
    const char *username;
    const char *password;
};

User users[] = {
    {"admin", "jesuisingenieurinformaticien"},
    {"moi", "cestmoi"},
    {"uncollegue", "cestuncollegue"}
};

bool authentification() {
    for (int i = 0; i < sizeof(users) / sizeof(users[0]); i++) {
        if (myServer.authenticate(users[i].username, users[i].password))
            return true;
    }
    return false;
}

int stringToInt(String input){
  char inputChar[input.length()+1];
  strcpy(inputChar, input.c_str());
  switch(input.length()){
    case 0 :
      return 99;
    break;
    case 1 : 
      return (inputChar[0]-0x30);
    case 2 :
      return (inputChar[0]-0x30)*10 + inputChar[1]-0x30;
    default :
      return 15;
    break;
  }
}

void implentHTML(){

  if (!authentification())
      return myServer.requestAuthentication();
  String page = "<!DOCTYPE html>";
  page += "";
  page += "";
  page += "";
  page += "";
  page += "<html lang='fr'>";
  page += "<head>";
  page += "    <meta charset='UTF-8'>";
  page += "    <meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  page += "    <title>Horaires distribution</title>";
  page += "    <link rel='icon' href='../assets/Snapchat-501841984.jpg'>";
  page += "    <link rel='preconnect' href='https://fonts.googleapis.com'>";
  page += "    <link rel='preconnect' href='https://fonts.gstatic.com' crossorigin>";
  page += "    <link href='https://fonts.googleapis.com/css2?family=Bevan:ital@0;1&family=Caprasimo&display=swap' rel='stylesheet'>";
  page += "    <style>";
  page += "        body{";
  page += "            font-family: Calibri;";
  page += "            font-size: 15px;";
  page += "            text-align: center;";
  page += "            margin: 0;";
  page += "        }";
  page += "        * {";
  page += "            transition: all ease 0.2s;";
  page += "        }";
  page += "        h1,h2 {";
  page += "            font-family: 'Bevan', serif;";
  page += "        }";
  page += "        h3 {";
  page += "            font-family: Calibri;";
  page += "        }";
  page += "        .title-wrapper {";
  page += "            background-color: aquamarine;";
  page += "            padding: 2em;";
  page += "        }";
  page += "        .times-wrapper {";
  page += "            margin: .8em 1em;";
  page += "            display: flex;";
  page += "            flex-direction: row;";
  page += "            gap: .8em;";
  page += "            flex-wrap: wrap;";
  page += "        }";
  page += "        .times-wrapper > form {";
  page += "            padding: .8em .2em;";
  page += "            flex-grow: 1;";
  page += "            min-width: 14em;";
  page += "            font-size: 1.1rem;";
  page += "            border-radius: .8em;";
  page += "            border: #333 solid 1px;";
  page += "        }";
  page += "        .times-wrapper > form > input:not(.submit-button) {";
  page += "            margin-top: 1em;";
  page += "            width: 60%;";
  page += "        }";
  page += "        .submit-button {";
  page += "            border: none;";
  page += "            border-radius: .7em;";
  page += "            background-color: #69a9f3;";
  page += "            padding: 1em 1.8em;";
  page += "            margin-top: 1.8em;";
  page += "        }";
  page += "        .submit-button:hover {";
  page += "            background-color: #c1ddfe;";
  page += "        }";
  page += "        .submit-button:focus {";
  page += "            outline: black solid 2px;";
  page += "        }";
  page += "        p{";
  page += "            margin-top: 1.8em; ";
  page += "        }";
  page += "    </style>";
  page += "</head>";
  page += "<body>";
  page += "    <div class='title-wrapper'>";
  page += "        <h1>Horaires de distribution</h1>";
  page += "    </div>";
  page += "    <div class='times-wrapper'>";
  page += "        <form method='get' action='/submit1'>";
  page += "             <h3>Premier horaire de distribution</h3>";
  page += "             <label for='intnumber' >Heure :&nbsp;&nbsp;</label>";
  page += "             <input type='number' id='input1Hour' name='input1Hour' placeholder='1ère heure' min='0' max='23'><br>";
  page += "             <label for='intnumber' >Minute :</label>";
  page += "             <input type='number' id='input1Minute' name='input1Minute' placeholder='1ère minute' min='0' max='59'><br>";
  page += "             <input type='submit' value='Envoyer' class='submit-button' >";
  page += "             <p>Heure actuelle : "; page += deadLines[0][hour]; page += "h"; page += deadLines[0][minute]; page += "min" "</p>";
  page += "        </form>";
  page += "        <form method='get' action='/submit2'>";
  page += "             <h3>Second horaire de distribution</h3>";
  page += "             <label for='intnumber' >Heure :&nbsp;&nbsp;</label>";
  page += "             <input type='number' id='input2Hour' name='input2Hour' placeholder='2ème heure' min='0' max='23'><br>";
  page += "             <label for='intnumber' >Minute :</label>";
  page += "             <input type='number' id='input2Minute' name='input2Minute' placeholder='2ème minute' min='0' max='59'><br>";
  page += "             <input type='submit' value='Envoyer' class='submit-button' >";
  page += "             <p>Heure actuelle : "; page += deadLines[1][hour]; page += "h"; page += deadLines[1][minute]; page += "min" "</p>";
  page += "        </form>";
  page += "        <form method='get' action='/submit3'>";
  page += "             <h3>Troisième horaire de distribution</h3>";
  page += "             <label for='intnumber' >Heure :&nbsp;&nbsp;</label>";
  page += "             <input type='number' id='input3Hour' name='input3Hour' placeholder='3ème heure' min='0' max='23'><br>";
  page += "             <label for='intnumber' >Minute :</label>";
  page += "             <input type='number' id='inputMinute' name='input3Minute' placeholder='3ème minute' min='0' max='59'><br>";
  page += "             <input type='submit' value='Envoyer' class='submit-button' >";
  page += "             <p>Heure actuelle : "; page += deadLines[2][hour]; page += "h"; page += deadLines[2][minute]; page += "min" "</p>";
  page += "        </form>";
  page += "        <form method='get' action='/submit4'>";
  page += "             <h3>Quatrième horaire de distribution</h3>";
  page += "             <label for='intnumber' >Heure :&nbsp;&nbsp;</label>";
  page += "             <input type='number' id='input4Hour' name='input4Hour' placeholder='4ème heure' min='0' max='23'><br>";
  page += "             <label for='intnumber' >Minute :</label>";
  page += "             <input type='number' id='input4Minute' name='input4Minute' placeholder='4ème minute' min='0' max='59'><br>";
  page += "             <input type='submit' value='Envoyer' class='submit-button' >";
  page += "             <p>Heure actuelle : "; page += deadLines[3][hour]; page += "h"; page += deadLines[3][minute]; page += "min" "</p>";
  page += "         </form>";
  page += "        </div>";
  page += "    </body>";
  page += "</html>";
 
  myServer.setContentLength(page.length()); 
  myServer.send(200, "text/html", page);
}

void notFound(){
  myServer.send(404, "text/plain", "404 : Not found !");
}

void valuesReading(String arg, int line, int column){
  String inputString = myServer.arg(arg);
  Serial.print("Numéro entrant: ");
  Serial.println(inputString);
  int inputNumber = stringToInt(inputString);
  if(column == hour){
    if(inputNumber == 99){
    }
    else{
      deadLines[line][column] = inputNumber;
    } 
  }
  else {
     if(inputNumber == 99){
    }
    else{
      deadLines[line][column] = inputNumber;
    }
    
  }
}

void changing1Deadline(){
  valuesReading("input1Hour", 0, hour);
  valuesReading("input1Minute", 0, minute);
  myServer.sendHeader("Location", "/");
  myServer.send(303);
}

void changing2Deadline(){
  valuesReading("input2Hour", 1, hour);
  valuesReading("input2Minute", 1, minute);
  myServer.sendHeader("Location", "/");
  myServer.send(303);
}

void changing3Deadline(){
  valuesReading("input3Hour", 2, hour);
  valuesReading("input3Minute", 2, minute);
  myServer.sendHeader("Location", "/");
  myServer.send(303);
}

void changing4Deadline(){
  valuesReading("input4Hour", 3, hour);
  valuesReading("input4Minute", 3, minute);
  myServer.sendHeader("Location", "/");
  myServer.send(303);
}

/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Motion (l293d) ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

#define INPUT1 19
#define INPUT2 23
#define ENABLE 18


enum direction {closingDirection, openingDirection, stop}; 
bool action = false;
bool isRunning = false;
int dutyCycle = 200;

void setDirection(int direction){
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
  connectToWiFi();
  timeClient.begin();
  delay(1000);

  myServer.on("/", implentHTML);  
  myServer.onNotFound(notFound);
  myServer.on("/submit1", HTTP_GET, changing1Deadline);
  myServer.on("/submit2", HTTP_GET, changing2Deadline);
  myServer.on("/submit3", HTTP_GET, changing3Deadline);
  myServer.on("/submit4", HTTP_GET, changing4Deadline);
  myServer.begin();
  Serial.println("Serveur actif !");

  timer = timerBegin(3, PRESCALER, true); //timer 3, prescaler : 8000, count up
  timerAttachInterrupt(timer, &Timer_ISR, true); //pointer on timer object, pointer on our interrupt function, append on rising edge (timer overflow in our case)
  timerAlarmWrite(timer, nbOfTicks, true); //pointer on timer object, overflow value on the timer, autoreload
  timerAlarmEnable(timer);

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
  myServer.handleClient();
  previousState();
  stateSwitchArray[0] = digitalRead(pinSwitch1);
  stateSwitchArray[1] = digitalRead(pinSwitch2);  
  stateSwitchArray[2] = digitalRead(pinSwitch3);  
  stateSwitchArray[3] = digitalRead(pinSwitch4);  
  stateSwitchArray[4] = digitalRead(pinSwitch5);   
  delay(1000);
  if (WiFi.status() != WL_CONNECTED) { 
    Serial.println("Connexion wifi perdue. Reconnexion...");
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
    stateSwitch = stateSwitchReturn();
    Serial.println(stateSwitch);
    isActivated = isActivatedReturn();
    Serial.println(isActivated);
  if(isActivatedReturn() == 1){
    setDirection(stop);
    Serial.println("stop");
  }
  }
  if(stateTime == 0 && stateSwitch != 0){
    setDirection(closingDirection);
    analogWrite(ENABLE, dutyCycle);
    Serial.println("closing");
  }
  else if(stateTime == 1 && stateSwitch != 2){
    if(stateSwitch > 2){
      setDirection(closingDirection);
      analogWrite(ENABLE, dutyCycle);
      Serial.println("closing");
    }
    if(stateSwitch < 2){
      setDirection(openingDirection);
      analogWrite(ENABLE, dutyCycle);
      Serial.println("opening");
    }
  }
  else if(stateTime == 2 && stateSwitch != 3){
    if(stateSwitch > 3){
      setDirection(closingDirection);
      analogWrite(ENABLE, dutyCycle);
      Serial.println("closing");
    }
    if(stateSwitch < 3){
      setDirection(openingDirection);
      analogWrite(ENABLE, dutyCycle);
      Serial.println("opening");
    }
  }
  else if(stateTime == 3 && stateSwitch != 4){
    if(stateSwitch > 4){
      setDirection(closingDirection);
      analogWrite(ENABLE, dutyCycle);
      Serial.println("closing");
    }
    if(stateSwitch < 4){
      setDirection(openingDirection);
      analogWrite(ENABLE, dutyCycle);
      Serial.println("opening");
    }
  }
  else if(stateTime == 4 && stateSwitch != 5){
    setDirection(closingDirection);
    analogWrite(ENABLE, dutyCycle);
    Serial.println("closing");
  }
  else{
    setDirection(stop);
    Serial.println("stop");
  }
}