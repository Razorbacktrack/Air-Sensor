#include <Arduino.h>
#include <Adafruit_NeoPixel.h> //Arduino library for controlling single-wire-based LED pixels (NeoPixel)
#include <esp32/rom/rtc.h> //library for reset debug

//Library for E-Paper Display
#include <epaper.h>
//Library for SD
#include <microsd.h>
//Library for BMP390 and SCD30
#include <sensors.h>
//HTML Template
#include <template.h>

//libraries for WiFi
#include <WiFi.h>   
#include <WebServer.h>  
#include <AutoConnect.h> //It uses SPIFFS on ESP32

//libraries for Time/NTP
#include <time.h>
#include <sntp.h>

/* ----------------------------------------------- */

//This format is required by Autoconnect function
#define FIRMWARE_VERSION  "1.0.1"
const char* fw_ver = FIRMWARE_VERSION;

/* Default Pins
#define SCK 5
#define MOSI 19
#define MISO 21
#define SDA 22
#define SCL 20
#define NEOPIXEL_I2C_POWER 2
#define BATT_MONITOR 35 //or A13 */

//Buttons
#define ONBOARD_BTN 38
#define KEY1 A2 //34, PIN29 on HAT
#define KEY2 37 //PIN31 on HAT
#define KEY3 15 //PIN33 on HAT
#define KEY4 27 //PIN35 on HAT

#define NEOPIXEL_PIN 0 //NeoPixel Pin
#define NUM_LEDS 1 //NeoPixel Number
//Set pin, number and model
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

//Debouncing (replaced with larger times to let the screen to be ready)
#define DEBOUNCE_TIME 300 //debounce time in milliseconds 
volatile unsigned long debouncingLastMicros=0; //store the last time a button has been pressed

//General display
#define PARTIAL_UPD_LIMIT 7 //Full update after 6 partial updates
volatile int partialUpdate = 0; //Variable used to control the partial update (it is resetted after changing state or errors)

//Battery
#define SEQUENTIAL_READS 64 //multisampling
#define READS_DISTANCE 0 //ms
//This values has been found with some tests
#define MAX_BATTERY_VALUE 2280 //2288 or slightly above, 4.147v
#define MIN_BATTERY_VALUE 1715 // or slightly below, 3.057v
//It's not possible to use that value because it changes 2000 when the battery level is too low
//#define MIN_CHARGING_VALUE 2358

//Reset debug
typedef struct  {
  String resetReason;
  String verboseResetReason;
} ri;

//Menu
volatile int currentInput = 0; //Input value to modify a variable with the buttons
volatile unsigned currentState = 1;
volatile unsigned previousState = 1;
volatile bool bootState = true; //Boot state to select the switch (boot menu) and change the buttons behaviour

volatile bool firstTime = true; //Used to "stop" the loop and execute the code of the state only one time. This can be reset with setInput and setState funcions

//Time, DST and SNTP
//This must be volatile to avoid a race condition with the wile loop
volatile boolean isTimeSet = false;
const char* ntpServer1 = "pool.ntp.org";
const char* ntpServer2 = "time.google.com";
const char* ntpServer3 = "time.cloudflare.com";
const char* timeZone = "CET-1CEST,M3.5.0,M10.5.0/3"; //Europe/Rome

//WiFi
WebServer Server; //WebServer variable
AutoConnect Portal(Server); //AutoConnect variable
AutoConnectConfig Config; //AutoConnectConfig variable
//WebPage autentication
const char httpUser[]= "user";
const char httpPwd[]= "password";

//Sensors
SCD30 airSensor;
Adafruit_BMP3XX bmpSensor;

//Global variables moved here for WebServer
uint16_t co2=0; //ppm
float temp=0; //C
float hum=0; //%
float pressure =0; //Pascals. 100 Pascals = 1 hPa = 1 millibar
float altitude =0; //m

/* ----------------------------------------------- */

//Battery
int batteryAnalog() { //trying to stabilize analogRead
  int reads[SEQUENTIAL_READS]={0};
  int maxIndex=0;
  int minIndex=0;
  int max=0;
  int min=30000;
  unsigned long sum=0;
  int averageValue = 0;

  for(int i=0;i<SEQUENTIAL_READS;i++) { //save 5 reads on the array and find max/min value/index
    int analogValue = analogRead(BATT_MONITOR);
    reads[i]=analogValue;

    if (analogValue < min) {
      min=analogValue;
      minIndex=i;
    }
    if (analogValue > max) {
      max=analogValue;
      maxIndex=i;
    }

    delay(READS_DISTANCE);
  } 
    //cancel ONE max/min value
    reads[maxIndex]=0;
    reads[minIndex]=0;

    for(int i=0;i<SEQUENTIAL_READS;i++) {
      sum += reads[i];
    }
    averageValue = (sum/(SEQUENTIAL_READS-2)); //because 2 values are 0 now

    return averageValue;
}
void batteryPercentage(int *percentage, int *lastPercentage, int *resetForCharging) {
  int fixedValue = batteryAnalog();

  //if (fixedValue>MIN_CHARGING_VALUE) { //this check is disabled because MIN_CHARGING_VALUE is reliable only when the battery is at least half charged
  //  percentageString = "Batt. Charging";
  //  *lastPercentage = 100; //reset
  //} else {
    *percentage = map(fixedValue,MIN_BATTERY_VALUE,MAX_BATTERY_VALUE,0,100);
    if (*percentage < 0) {
      *percentage = 0;  
    } else if (*percentage > 100) {
      *percentage = 100;
    }

    //Workaround for unstable analogRead
    //to avoid fake values when *percentage <*lastPercentage but too low
    // *lastPercentage != 100 because the battery may be low on boot 
    if(*lastPercentage !=100 && ((*lastPercentage-*percentage)>30) && ((*lastPercentage-*percentage)<60)) {
      *percentage=*lastPercentage;
    //to avoid fake values with *percentage>*lastPercentage with a simple reset for charging state
    } else if(*percentage>*lastPercentage) {
      if (*resetForCharging <3) { 
        *percentage=*lastPercentage;
        (*resetForCharging)++;
      } else { // the battery is probably charging
        *lastPercentage=*percentage;
        *resetForCharging=0;
      }
    } else {
      *lastPercentage=*percentage;
    }
  //}
}

//Menu
void setState(unsigned s) {
   //reset for full screen update only if the currentState is not the Homepage and the 
   //destination state is not defined
  if ((currentState !=1) || (s == 10)) partialUpdate = 0;

  previousState= currentState;
  currentState = s; //state that we want
  firstTime = true; //reset
}
void setInput(int s) {
  currentInput = s; //Input value that we want
  firstTime = true; //reset 
}

/*Multi-level in normal mode [Up to 9 menus and sub-menus (10-18,100-108,etc.)], linear in boot mode.
Micros function (for debouncing) returns the number of microseconds from the time
the board begins running the current program.
This number overflows i.e. goes back to zero after approximately 70 minutes so it's resetted in the loop*/
void IRAM_ATTR up_pressed() {
  if (!firstTime) {
    unsigned long currentMicros = micros();
    if((currentMicros - debouncingLastMicros) >= DEBOUNCE_TIME * 1000) {
      debouncingLastMicros = currentMicros;
      if (bootState) {
        if (currentInput > 0)
          setInput(currentInput-1); //input must be a positive number
      }
      else
        setState(currentState-1);
    }
  }
}
void IRAM_ATTR down_pressed() {
  if (!firstTime) {
    unsigned long currentMicros = micros();
    if((currentMicros - debouncingLastMicros) >= DEBOUNCE_TIME * 1000) {
      debouncingLastMicros = currentMicros;
      if (bootState)
        setInput(currentInput+1);
      else
        setState(currentState+1);
    }
  }
}
void IRAM_ATTR left_pressed() {
  if (!firstTime) {
    unsigned long currentMicros = micros();
    if((currentMicros - debouncingLastMicros) >= DEBOUNCE_TIME * 1000) {
      debouncingLastMicros = currentMicros;
      if (bootState) 
        setInput(currentInput+10); //cant go back on bootState, used to add 10+
      else
        setState(currentState/10);
    }
  }
}
void IRAM_ATTR right_pressed() {
  if (!firstTime) { //this check is used to avoid a race condition if a button has been pressed before the new code has been executed
    unsigned long currentMicros = micros();
    if((currentMicros - debouncingLastMicros) >= DEBOUNCE_TIME * 1000) {
      debouncingLastMicros = currentMicros;
      if (bootState)
        setState(currentState+1); //Linear advance
      else
        setState(currentState*10); //Multi-level advance
    }
  }
}

//Time, DST and SNTP
void printLocalTime(){
  struct tm time;
  if(!getLocalTime(&time)){ //getLocalTime saves all the details about date and time and save them on timeinfo
    twoRows_centered("Failed to obtain the","local date and time");
    delay(3000); //let the user read
    return;
  }
  char datetime[30];
  char zone[30];
  strftime(datetime,30,"%d-%m-%Y %H:%M:%S", &time);
  strftime(zone,30,"Zone %Z %z", &time);
  threeRows_centered("Local Date/Time",datetime,zone);
  delay(3000);
}
void setTime(int yr, int month, int mday, int hr, int minute, int sec, int isDst){
  struct tm time;

  time.tm_year = yr - 1900;   //tm_year: years SINCE 1900
  time.tm_mon = month-1; //tm_mon: months SINCE January (from 0 to 11)
  time.tm_mday = mday; //tm_mday: day of the month (from 1 to 31)
  time.tm_hour = hr;      //tm_hour: hours since midnight (from 0 to 23)
  time.tm_min = minute; //tm_min: minutes after the hour (from 0 to 59)
  time.tm_sec = sec; //tm_sec: seconds after the minute (from 0 to 59)
  time.tm_isdst = isDst;  //Daylight Saving Time flag, 1 or 0. 1 indicates that the time we set is in DST
  time_t t = mktime(&time);
  //Serial.printf("Setting time: %s", asctime(&tm));
  struct timeval now = { .tv_sec = t };
  settimeofday(&now, NULL);
  printLocalTime();
}

// Callback function (called when time adjusts via NTP)
void timeAvailable(struct timeval *t)
{ 
  twoRows_centered("Got time adjustment","from NTP Server!");
  delay(3000); //let the user read
  printLocalTime();
  isTimeSet=true;
  if (currentState == 1) partialUpdate = 0; //for a full refresh if the function is called while currentState is 1
}

//WiFi
void rootPage() {

  String content = PageHTML();
  if (!co2 && !temp && !hum) {
    content.replace("{{co2value}}", "Data not ready");
    content.replace("{{tmpvalue}}", "Data not ready");
    content.replace("{{humvalue}}", "Data not ready");
  } else {
    content.replace("{{co2value}}", String(co2));
    content.replace("{{tmpvalue}}", String(temp));
    content.replace("{{humvalue}}", String(hum));
  }
  if (!altitude  && !pressure) {
    content.replace("{{altitudevalue}}", "Data not ready");
    content.replace("{{pressurevalue}}", "Data not ready");
  } else {
    content.replace("{{altitudevalue}}", String(altitude));
    content.replace("{{pressurevalue}}", String(pressure));
  }

  Server.send(200, "text/html", content);
}

//Callback functions
void OTAStart() { //It is called only once when the OTA has been started
  twoRows_centered("OTA","Updating");
}
void OTAEnd() { //It is called only once when the OTA is finished.
  twoRows_centered("OTA","Update finished");
}
void OTAProgress(unsigned int amount, unsigned int size) { //It is called during the OTA update
  Serial.printf("Progress: %u(%u)\r", amount, size); //amount: Total amount of bytes received.size: Block size of current send.
}
void OTAError(uint8_t error) { //It is called when some error occurred during the OTA update.
  twoRows_centered("OTA Error:", String(error));
}

void SoftAP_Off(IPAddress& ipaddr) {
if (WiFi.getMode() && WIFI_AP) {
      WiFi.softAPdisconnect(true);
      WiFi.enableAP(false);
      //twoRows_centered("SoftAP shut down.","Please wait.");
      threeRows_centered("WiFi connected!", WiFi.SSID(), "IP: "+WiFi.localIP().toString());
      delay(3000);
    }
}

void setAutoconnectConfig(String apid, String apwd, IPAddress softIP) {
  //AutoConnect Configuration
  Config.autoReset = false;     // Not reset the module even by intentional disconnection using AutoConnect menu.
  Config.autoReconnect = true;    // Attempt automatic reconnection to one of the saved AP
  //Captive portal activation switch. True is default. False prevents starting the captive portal even if the connection at the 1st-WiFi.begin fails.
  Config.autoRise = true; 
  Config.reconnectInterval = 6;   // Seek interval time is 180[s].
  //Continue the portal function even if the captive portal times out. 
  //The STA + SoftAP mode of the ESP module continues and accepts the connection request to the AP.
  Config.retainPortal = true;
  //If multiple saved AP are available, it will attemp to connect to one with the best one of the reception sensitivity.
  Config.principle = AC_PRINCIPLE_RSSI; 
  Config.portalTimeout = 30000;  //It will time out Portal.begin in 30 seconds
  Config.immediateStart = false; //Disable the 1st-WiFi.begin and directly start the captive portal.
  Config.autoSave = AC_SAVECREDENTIAL_AUTO; //save a credential when the WiFi connection is established with an AP
  Config.title = "AirSensor"; //Menu Title
  Config.menuItems = Config.menuItems | AC_MENUITEM_DELETESSID; //Add the credentials removal feature
  Config.homeUri = "/"; //Default. This path would be linked from 'HOME' in the AutoConnect menu.
  //OTA
  Config.ota = AC_OTA_BUILTIN; //OTA Update enabled
  Config.otaExtraCaption = fw_ver; //Show FW Version on Update menu
  //SoftAP authentication
  Config.apid = apid;
  Config.psk  = apwd;
  Config.gateway = softIP;
  //HTML authentication settings to access the captive portal
  Config.auth = AC_AUTH_DIGEST;
  Config.authScope = AC_AUTHSCOPE_PORTAL;
  Config.username =httpUser;
  Config.password =httpPwd;

  Portal.config(Config); //Set Autoconnect Config
}
bool startAutoconnect(String apid, String apwd, IPAddress softIP) {

  String IPString = String() + softIP[0] + "." + softIP[1] + "." + softIP[2] + "." + softIP[3] + "/_ac";

  //Set the program behaviour after WiFi Connection of the ESP32 to the AP (Turn off SoftAP)
  Portal.onConnect(SoftAP_Off);
  //Set the program behaviour based on the status of the OTA update
  Portal.onOTAStart(OTAStart);
  Portal.onOTAEnd(OTAEnd);
  Portal.onOTAProgress(OTAProgress);
  Portal.onOTAError(OTAError);

  Server.on("/", rootPage); //URL handler, call "rootPage" when a client requests "/""

  threeRows_centered("Trying to connect with","a saved WiFi network.","Please wait (approx 30s)");

  /*AutoConnect internally performs WiFi.begin and Server.begin
  If portalTimeout is not defined, it will not exit until a WiFi connection is established.
  If the ssid and the passphrase are missing, its WiFi.begin has no SSID and Password.
  Regardless of the result, WebServer will start immediately after the first WiFi.begin.
  The captive portal will not be started if the connection has been established with first WiFi.begin. */

  //return true for Connection established, AutoConnect service started with WIFI_STA mode.
  //return false for Could not connected, Captive portal started with WIFI_AP_STA mode.
  if(Portal.begin()) {
    //Official workaround to use interrupt with GPIO 36&&39 while WiFi is enabled (part1)
    //WiFi.setSleep(false);
    return 1;
  } else {
    twoRows_centered("Failed to connect","Starting AP and CP");
    delay(3000);
    threeRows_centered("AP Name: "+apid,"Password: "+apwd, "IP: "+IPString);
    //Official workaround to use interrupt with GPIO 36&&39 while WiFi is enabled (part1)
    //WiFi.setSleep(false);
    delay(3000);
    return 0;
  }
}

bool WiFiOff() {
  Portal.end();
  Server.stop();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  btStop();
  isTimeSet = false; //reset isTimeSet
  twoRows_centered("WiFi Off!","Please wait");
  delay(3000);
  return 0;
}
void WiFiOn(String apid, String apwd, IPAddress softIP) {
  setAutoconnectConfig(apid, apwd, softIP);
  if(!startAutoconnect(apid, apwd, softIP))
    while (WiFi.status() != WL_CONNECTED) {
      //This replace Server.handleClient. Process the AutoConnect menu interface and captive portal
      Portal.handleClient();
    }
}

//Reset Debug
ri saveResetReason(int reason) {
  ri resetInfo;
  switch (reason) {
    case 1 : {
      resetInfo.resetReason="POWERON_RESET";
      resetInfo.verboseResetReason="Vbat power on reset";
    }break;
    case 3 : {
      resetInfo.resetReason="SW_RESET";
      resetInfo.verboseResetReason="Software reset digital core";
    }break;
    case 4 : {
      resetInfo.resetReason="OWDT_RESET";
      resetInfo.verboseResetReason="Legacy watch dog reset digital core";
    }break;
    case 5 : {
      resetInfo.resetReason="DEEPSLEEP_RESET";
      resetInfo.verboseResetReason="Deep Sleep reset digital core";
    }break;
    case 6 : {
      resetInfo.resetReason="SDIO_RESET";
      resetInfo.verboseResetReason="Reset by SLC module, reset digital core";
    }break;
    case 7 : {
      resetInfo.resetReason="TG0WDT_SYS_RESET";
      resetInfo.verboseResetReason="Timer Group0 Watch dog reset digital core";
    }break;
    case 8 : {
      resetInfo.resetReason="TG1WDT_SYS_RESET";
      resetInfo.verboseResetReason="Timer Group1 Watch dog reset digital core";
    }break;
    case 9 : {
      resetInfo.resetReason="RTCWDT_SYS_RESET";
      resetInfo.verboseResetReason="RTC Watch dog Reset digital core";
    }break;
    case 10 : {
      resetInfo.resetReason="INTRUSION_RESET";
      resetInfo.verboseResetReason="Instrusion tested to reset CPU";
    }break;
    case 11 : {
      resetInfo.resetReason="TGWDT_CPU_RESET";
      resetInfo.verboseResetReason="Time Group reset CPU";
    }break;
    case 12 : {
      resetInfo.resetReason="SW_CPU_RESET";
      resetInfo.verboseResetReason="Software reset CPU";
    }break;
    case 13 : {
      resetInfo.resetReason="RTCWDT_CPU_RESET";
      resetInfo.verboseResetReason="RTC Watch dog Reset CPU";
    }break;    
    case 14 : {
      resetInfo.resetReason="EXT_CPU_RESET";
      resetInfo.verboseResetReason="for APP CPU, reseted by PRO CPU";
    }break;
    case 15 : {
      resetInfo.resetReason="RTCWDT_BROWN_OUT_RESET";
      resetInfo.verboseResetReason="Reset when the vdd voltage is not stable";
    }break;
    case 16 : {
      resetInfo.resetReason="RTCWDT_RTC_RESET";
      resetInfo.verboseResetReason="RTC Watch dog reset digital core and rtc module";
    }break;      
    default : {
      resetInfo.resetReason="NO_MEAN";
      resetInfo.verboseResetReason="NO_MEAN";
    }    
  }
  return resetInfo;
}

//Pixel
void setPixelColor() {
if (co2>800 && co2<1000) { //warning
  pixels.setPixelColor(0, pixels.Color(255, 255, 0)); //Yellow
  pixels.show();
  delay(500);

  pixels.clear();
  pixels.show();
  delay(500);

  pixels.setPixelColor(0, pixels.Color(255, 255, 0)); //Yellow
  pixels.show();
  delay(500);

  pixels.clear();
  pixels.show();
} else if (co2>=1000) { //bad co2
  pixels.setPixelColor(0, pixels.Color(255, 0, 0)); //Red
  pixels.show();
  delay(500);

  pixels.clear();
  pixels.show();
  delay(500);

  pixels.setPixelColor(0, pixels.Color(255, 0, 0)); //Red
  pixels.show();
  delay(500);

  pixels.clear();
  pixels.show();
} else {
  pixels.clear();
  pixels.show();
}
} 

void setup() {

  setCpuFrequencyMhz(80); 

  Serial.begin(115200);
  Serial.setDebugOutput(true);

  //Power pin for Pixel and Stemma QT Connector
  pinMode(NEOPIXEL_I2C_POWER, OUTPUT);
  digitalWrite(NEOPIXEL_I2C_POWER, LOW); //Off on setup

  //pinMode(ONBOARD_BTN, INPUT);
  //pinMode(SD_DET, INPUT);
  pinMode(KEY1, INPUT); //pin 34 don’t have internal pull-up. An external pull-up resistor has been added
  pinMode(KEY2, INPUT); //pin 37 don’t have internal pull-up. An external pull-up resistor has been added
  pinMode(KEY3, INPUT_PULLUP);
  pinMode(KEY4, INPUT_PULLUP);
  attachInterrupt(KEY1, up_pressed, RISING);
  attachInterrupt(KEY2, down_pressed, RISING);
  attachInterrupt(KEY3, left_pressed, RISING);
  attachInterrupt(KEY4, right_pressed, RISING);

  pixels.begin(); //Initialize the NeoPixel library
  pixels.clear(); //set pixel color to "Black" //same as pixels.setPixelColor(0,0,0,0)
  pixels.setBrightness(50); //0 is off, 255 is max
  pixels.show(); //Update the pixel settings
  
  initDisplay(); //Initialize the E-Paper Display library, uses standard SPI pins

  //set notification callback function (run timeavailable when time adjusts via NTP)
  sntp_set_time_sync_notification_cb( timeAvailable );
}

void loop() {

//SCD30
int samplingInterval = 2; //default, seconds
uint16_t settingVal = 0;

//BMP390
//float temp2 =0; //C

//Initialize the minimum value to a big number
//Initialize the maximum value to a small number
static minmax minmaxvalues= {10000, 0, 10000, 0, 10000, 0};

//Battery
float percentage = 0;

//WiFi
String apid = "AirSensor-" + String((uint32_t)(ESP.getEfuseMac() >> 32), HEX); //SoftAP with unique SSID 
String apwd = "damiano22"; //SoftAP Password
IPAddress softIP (172,217,28,1); //SoftAP IP

//To avoid infinite loop in case of NTP problems
//static is not necessary because it's used within a while loop
unsigned long ntpWaitTimeout = 0;

//Homepage
static int homepageUpdTime = 30; //update time in seconds
unsigned long currentMillis =0; //store the current millis
static unsigned long homepageLastMillis = 0; //store the last time the homepage has been updated

//Boot variables
static boolean useSD = false;
static boolean useWiFi = false;
static boolean usePixel = false;
static boolean ASC = false; //automatic self-calibration
static int year = 0;
static int month = 0;
static int day = 0;
static int hour = 0;
static int minute = 0;
static int isDst = 0;

//Debouncing 
if (micros()<debouncingLastMicros) debouncingLastMicros = micros(); //because debouncingLastMicros is bigger after micros overflow
//Homepage
if (millis()<homepageLastMillis) homepageLastMillis = millis(); //because homepageLastMillis is bigger after millis overflow

//Official workaround to use interrupt with GPIO 36&&39 while using analogRead
//detachInterrupt(KEY1);
//detachInterrupt(KEY2);
//detachInterrupt(KEY3);
//detachInterrupt(KEY4);
//percentage = batteryPercentage();
//attachInterrupt(KEY1, up_pressed, RISING);
//attachInterrupt(KEY2, down_pressed, RISING);
//attachInterrupt(KEY3, left_pressed, RISING);
//attachInterrupt(KEY4, right_pressed, RISING);

//This replace Server.handleClient. Process the AutoConnect menu interface and captive portal
if(useWiFi)
  Portal.handleClient(); 

if(bootState) 
switch(currentState) {

  case 1: {
    if(firstTime){
      currentInput=currentInput%2; //Modulo operation returns the remainder of the division
      if (partialUpdate>=PARTIAL_UPD_LIMIT) partialUpdate=0; //reset partialUpdate
      twoRows_centered_1row_intinput("Enable the ASC?", "(Automatic Self-Calibration)", currentInput, 1, partialUpdate);
      partialUpdate ++;
      firstTime=false;
    }  
  } break;

  case 2: {
      ASC= currentInput;
      setState(3);
  } break;

  case 3: {
    if(firstTime){
      if (currentInput <30) currentInput= 30;
      if (partialUpdate>=PARTIAL_UPD_LIMIT) partialUpdate=0; //reset partialUpdate
      twoRows_centered_1row_intinput("Please enter the Homepage", "update time in seconds", currentInput, 0, partialUpdate);
      partialUpdate ++;
      firstTime=false;
    }
  } break;

  case 4: {
    homepageUpdTime= currentInput;

    //The SCD30 initialization is here to preserve power usage and beam life in case of random reboots
    initializeSensors(ASC, samplingInterval);

    setState(5);
  } break;

  case 5: {
    if(firstTime){
      currentInput=currentInput%2; //Modulo operation returns the remainder of the division
      if (partialUpdate>=PARTIAL_UPD_LIMIT) partialUpdate=0; //reset partialUpdate
      twoRows_centered_1row_intinput("Do you want to use WiFi to", "get the time/date from NTP?", currentInput, 1, partialUpdate);
      partialUpdate ++;
      firstTime=false;
    }
  } break;

  case 6: {
    useWiFi= currentInput;

      if (useWiFi) { //yes
        WiFiOn(apid, apwd, softIP);
        //currentState =6; //Official workaround to use interrupt with GPIO 36&&39 while WiFi is enabled (part2)
        setState(21);
      } else setState(7);
  } break;

  case 7: {
    if(firstTime){
      currentInput=currentInput%2; //Modulo operation returns the remainder of the division
      if (partialUpdate>=PARTIAL_UPD_LIMIT) partialUpdate=0; //reset partialUpdate
      twoRows_centered_1row_intinput("Is Daylight Saving Time",	"(DST) active now?", currentInput, 1, partialUpdate);
      partialUpdate ++;
      firstTime=false;
    }
  } break;

  case 8: {
    isDst= currentInput;
    setState(9);
  } break;

  case 9: {
    if(firstTime){
      if(currentInput == 0 || currentInput >= 32) currentInput=1;
      if (partialUpdate>=PARTIAL_UPD_LIMIT) partialUpdate=0; //reset partialUpdate
      title_centered_1row_intinput("Please enter the current day", currentInput, 0, partialUpdate);
      partialUpdate ++;
      firstTime=false;
    }
  } break;

  case 10: {
    day= currentInput;
    setState(11);
  } break;

  case 11: {
    if(firstTime){
      if(currentInput == 0 || currentInput >= 13) currentInput=1;
      if (partialUpdate>=PARTIAL_UPD_LIMIT) partialUpdate=0; //reset partialUpdate
      title_centered_1row_intinput("Please enter the current month", currentInput, 0, partialUpdate);
      partialUpdate ++;
      firstTime=false;
    }
  } break;

  case 12: {
    month= currentInput;
    setState(13);
  } break;

  case 13: {
    if(firstTime){

      if(currentInput <= 2021) currentInput=2022;
      if (partialUpdate>=PARTIAL_UPD_LIMIT) partialUpdate=0; //reset partialUpdate
      title_centered_1row_intinput("Please enter the current year", currentInput, 0, partialUpdate);
      partialUpdate ++;
      firstTime=false;
    }
  } break;

  case 14: {
    year= currentInput;
    setState(15);
  } break;

  case 15: {
    if(firstTime){
      if(currentInput >= 24) currentInput=0;
      if (partialUpdate>=PARTIAL_UPD_LIMIT) partialUpdate=0; //reset partialUpdate
      title_centered_1row_intinput("Please enter the current hour", currentInput, 0, partialUpdate);
      partialUpdate ++;
      firstTime=false;
    }
  } break;

  case 16: {
    hour= currentInput;
    setState(17);
  } break;

  case 17: {
    if(firstTime){
      if(currentInput >= 60) currentInput=0;
      if (partialUpdate>=PARTIAL_UPD_LIMIT) partialUpdate=0; //reset partialUpdate
      title_centered_1row_intinput("Please enter the current min.", currentInput, 0, partialUpdate);
      partialUpdate ++;
      firstTime=false;
    }
  } break;

  case 18: {
    minute= currentInput;
    setState(19);
  } break;

  case 19: {
    if(firstTime){
      currentInput=currentInput%2; //Modulo operation returns the remainder of the division
      String dst;
      if (isDst) dst="Yes"; else dst="No";
      String datetime;
      if(minute <10)
        datetime= String (day)+ "-" + String (month)+ "-" + String (year)+ " " + String (hour)+ ":0" + String (minute) + " DST: " + dst;
      else
        datetime= String (day)+ "-" + String (month)+ "-" + String (year)+ " " + String (hour)+ ":" + String (minute) + " DST: " + dst;
      if (partialUpdate>=PARTIAL_UPD_LIMIT) partialUpdate=0; //reset partialUpdate
      twoRows_centered_1row_intinput("Confirm this date/time?", datetime, currentInput, 1, partialUpdate);
      partialUpdate ++;
      firstTime=false;
    }
  } break;

  case 20: {
    if (currentInput) { //yes
      setState(21);
    } else setState(7);
  } break;

  case 21: {
  
    if (useWiFi) { //yes
      configTzTime(timeZone, ntpServer1, ntpServer2, ntpServer3);

      currentMillis = millis();
      ntpWaitTimeout = currentMillis+60000; //timeout after 60 seconds

      while ((!isTimeSet) && (currentMillis<ntpWaitTimeout)) {
        //wait for time set from NTP Server
        currentMillis = millis();
      }

      if (isTimeSet) { //NTP OK
        setState(22);
      } else { 
        threeRows_centered("No response from","NTP Server.","Going back to WiFi selection.");
        delay(3000);
        useWiFi=WiFiOff();
        setState(5);
      }

    } else {
      setTime(year,month,day,hour,minute,0,isDst);
      setState(24);
    }
    
  }break;

  case 22: {
    if(firstTime){
      currentInput=currentInput%2; //Modulo operation returns the remainder of the division
      if (partialUpdate>=PARTIAL_UPD_LIMIT) partialUpdate=0; //reset partialUpdate
      twoRows_centered_1row_intinput("Do you want to keep WiFi on?", "You can enable/disable it later", currentInput, 1, partialUpdate);
      partialUpdate ++;
      firstTime=false;
    }  
  }break;

  case 23: {
    bool input = currentInput;
    if(!input)
      useWiFi=WiFiOff();
    setState(24);
  }break;

  case 24: {
    if(firstTime){
      currentInput=currentInput%2; //Modulo operation returns the remainder of the division
      if (partialUpdate>=PARTIAL_UPD_LIMIT) partialUpdate=0; //reset partialUpdate
      twoRows_centered_1row_intinput("Enable the CO2 Led monitor?", "You can enable/disable it later", currentInput, 1, partialUpdate);
      partialUpdate ++;
      firstTime=false;
    }  
  }break;

  case 25: {
    usePixel= currentInput;
    setState(26);
  }break;

  case 26: {
    if(firstTime){
      //if (digitalRead(SD_DET)==HIGH) {
      currentInput=currentInput%2; //Modulo operation returns the remainder of the division
      if (partialUpdate>=PARTIAL_UPD_LIMIT) partialUpdate=0; //reset partialUpdate
      twoRows_centered_1row_intinput("Do you want to use the SD", "to log the ambient values?", currentInput, 1, partialUpdate);
      partialUpdate ++;
      firstTime=false;
      //} else {
      //  useSD = false;
      //  setState(27);
      //}
    }
  } break;

  case 27: {
      useSD= currentInput;

      if (useSD) { //yes
  	    if(mountSD()) {
          setState(28);
        } else {
          setState(26);
        }
      } else setState(28);
  } break;

  case 28: {
    bootState = 0;
    setState(1);
  }break;

  default: {
    currentState = previousState; //The state is not defined so it'll go back to the previous state
    firstTime=false; //used to not execute the currentState again because it's not really changed
  } 

}
else
switch(currentState) {

  case 1: {
    static int percentage = 0;
    static int lastPercentage = 100; //last resort to get stabilized percentage on screen
    static int resetForCharging = 0; //used to update the percentage while the battery is charging (so percentage>lastPercentage it's normal
    
    currentMillis = millis();
    firstTime=false;
    if (partialUpdate>=PARTIAL_UPD_LIMIT) partialUpdate=0; //reset partialUpdate

    if(((currentMillis - homepageLastMillis) >= (homepageUpdTime * 1000)) || partialUpdate == 0) {
      homepageLastMillis = currentMillis;

      if(getDataFromSensors(&co2, &temp, &hum, &pressure, &altitude, &partialUpdate)) {
        updateMinMaxValues(co2, temp, hum, &minmaxvalues);
        
        if(partialUpdate==0) batteryPercentage(&percentage, &lastPercentage, &resetForCharging); //because it is updated only with full screen update
        homepage(co2, temp, hum, partialUpdate, percentage, useSD, useWiFi); //print values on screen

        Serial.print("CO2 [ppm]: ");
        Serial.println(co2);
        Serial.print("Temp [C]: ");
        Serial.println(temp);
        Serial.print("Humidity [%]: ");
        Serial.println(hum);
        Serial.print("Pressure [mbar]: ");
        if(!pressure) Serial.println("Data not ready"); else Serial.println(pressure);
        Serial.print("Altitude [m]: ");
        if(!altitude) Serial.println("Data not ready"); else Serial.println(altitude);  

        partialUpdate ++;

        //if (useSD && digitalRead(SD_DET)==HIGH) {
        if (useSD) {
          int logstate = saveLog (co2, temp, hum, pressure, altitude, SD);
          if (logstate == 0) { //failed to obtain time
            while (1); //infinite loop (freezing)
          } else if (logstate == 1) { //failed to create/modify the log file
            SD.end(); 
            useSD=false;
            partialUpdate =0; //full refresh
          }

          //} else if (useSD && digitalRead(SD_DET)==LOW) {
          //  threeRows_centered("Log was on but SD is", "not inserted. Please","mount it again in settings");
          //  SD.end(); 
          //  useSD=false;
          //  partialUpdate =0;
          //  delay(3000);
        }

        if(usePixel) setPixelColor();
      }
    }


  } break;

  case 10: {
    if(firstTime){
    twoRows_centered("Menu:", "Min/Max Values");
    firstTime=false;
    }
  } break;

  case 100: {
    if(firstTime){
    twoRows_centered("Menu:", "Show Min/Max Values");
    firstTime=false;
    }
  } break;

  case 1000: {
    if(firstTime){
    String min_co2_string;
    String max_co2_string;
    if (minmaxvalues.min_co2 == 10000) min_co2_string = "N.A."; else min_co2_string = String(minmaxvalues.min_co2);
    if (minmaxvalues.max_co2 == 0) max_co2_string = "N.A."; else max_co2_string = String(minmaxvalues.max_co2);
    title_centered_2rows_wdata("CO2 [ppm]", "Min.",min_co2_string, "Max.", max_co2_string);
    firstTime=false;
    }
  } break;

  case 1001: {
    if(firstTime){
    String min_temp_string;
    String max_temp_string;
    if (minmaxvalues.min_temp == 10000) min_temp_string = "N.A."; else min_temp_string = String(minmaxvalues.min_temp,1);
    if (minmaxvalues.max_temp == 0) max_temp_string = "N.A."; else max_temp_string = String(minmaxvalues.max_temp,1);
    title_centered_2rows_wdata("Temp [C]", "Min.", min_temp_string, "Max.", max_temp_string);
    firstTime=false;
    }
  } break;

  case 1002: {
    if(firstTime){
    String min_hum_string;
    String max_hum_string;
    if (minmaxvalues.min_hum == 10000) min_hum_string = "N.A."; else min_hum_string = String(minmaxvalues.min_hum,1);
    if (minmaxvalues.max_hum == 0) max_hum_string = "N.A."; else max_hum_string = String(minmaxvalues.max_hum,1);
    title_centered_2rows_wdata("Humidity [%]", "Min.", min_hum_string, "Max.", max_hum_string);
    firstTime=false;
    }
  } break;

  case 101: {
    if(firstTime){
    twoRows_centered("Menu:", "Reset Min/Max Values");
    firstTime=false;
    }
  } break;

  case 1010: {

    minmaxvalues.min_co2=10000;
    minmaxvalues.max_co2=0;
    minmaxvalues.min_temp=10000;
    minmaxvalues.max_temp=0;
    minmaxvalues.min_hum=10000;
    minmaxvalues.max_hum=0;

    twoRows_centered("Min/Max values reset done.", "Going back to Homepage");
    delay(3000); //let the user read
    setState(1);
  } break;

  case 11: {
    if(firstTime){
    twoRows_centered("Menu:", "SD Card");
    firstTime=false;
    }
  } break;

  case 110: {
    if(firstTime){
    if (useSD) {
      twoRows_centered("Menu:", "Unmount SD");
    } else {
      twoRows_centered("Menu:", "Mount SD");
    }
    firstTime=false;
    }
  } break;

  case 1100: {
    if (useSD) {
      useSD=unMountSD();
      setState(1);
    } else {
      if(mountSD()) {
          useSD= true;
          setState(11);
        } else {
          setState(110);
        }
    }
  } break;

  case 111: {
    if(firstTime){
    twoRows_centered("Menu:", "Delete SD files");
    firstTime=false;
    }
  } break;

  case 1110: {
    if(firstTime){
      if(useSD) {
        threeRows_centered("This process will delete", "all the files in the SD Root.", "Press -> to continue.");
        firstTime=false;
      //} else if (digitalRead(SD_DET)==HIGH){
      } else {
        twoRows_centered("SD not mounted.", "Going back to Menu");
        delay(3000);
        setState(11);
      //} else {
      //  twoRows_centered("No SD card inserted", "Going back to Menu");
      //  delay(3000);
      //setState(11);
      }

    }
  } break;

  case 11100: {
    twoRows_centered("Deleting files from SD Root.", "Do not remove the SD");
    removeAllFiles(SD);

    twoRows_centered("All files deleted.", "Going back back to Menu");
    delay(3000); //let the user read
    setState(11);
  } break;

  case 112: {
    if(firstTime){
    twoRows_centered("Menu:", "Show SD Info");
    firstTime=false;
    }
  } break;

  case 1120: {
    if(firstTime){
      if(useSD) {
        uint8_t cardType = SD.cardType();
        long unsigned usedMBytes = SD.usedBytes() / (1024 * 1024); //MB
        long unsigned totalMBytes = SD.totalBytes() / (1024 * 1024); //MB
        String stringtype;
    
        if(cardType == CARD_MMC){
          stringtype="MMC";
        } else if(cardType == CARD_SD){
          stringtype="SDSC";
        } else if(cardType == CARD_SDHC){
          stringtype="SDHC";
        } else {
          stringtype="UNKNOWN";
        }

        threeRows_centered("SD Type: "+stringtype,"Used Space: ",String(usedMBytes)+"/"+String(totalMBytes)+" MB");
        firstTime=false;
      //} else if (digitalRead(SD_DET)==HIGH){
      } else {
        twoRows_centered("SD not mounted.", "Going back to Menu");
        delay(3000);
        setState(11);
      //} else {
      //  twoRows_centered("No SD card inserted", "Going back to Menu");
      //  delay(3000);
      //  setState(11);
      }

    }
  } break;

  case 12: {
    if(firstTime){
    twoRows_centered("Menu:", "WiFi");
    firstTime=false;
    }
  } break;

  case 120: {
    if(firstTime){
    if (useWiFi) {
      twoRows_centered("Menu:", "Turn WiFi Off");
    } else {
      twoRows_centered("Menu:", "Turn WiFi On");
    }
    firstTime=false;
    }
  } break;

    case 1200: {
    if (useWiFi) {
      useWiFi=WiFiOff(); 
      setState(1);
    } else {
      useWiFi=true;
      WiFiOn(apid, apwd, softIP);
      //currentState =1200; //Official workaround to use interrupt with GPIO 36&&39 while WiFi is enabled (part2)
      configTzTime(timeZone, ntpServer1, ntpServer2, ntpServer3);

      currentMillis = millis();
      ntpWaitTimeout = currentMillis+60000; //timeout after 60 seconds

      while ((!isTimeSet) && (currentMillis<ntpWaitTimeout)) {
        //wait for time set from NTP Server
        currentMillis = millis();
      }

      if (!isTimeSet) {
        threeRows_centered("No response from","NTP Server.","Going back to Homepage.");
        delay(3000);
        useWiFi=WiFiOff();
      }
      setState(1);

    }
  } break;

  case 121: {
    if(firstTime){
    twoRows_centered("WiFi Settings","and Info");
    firstTime=false;
    }
  } break;

  case 1210: {
    if(firstTime){
      if(useWiFi) {
        title_centered_2rows_wdata("WiFi_STA", "SSID",WiFi.SSID(), "IP", WiFi.localIP().toString());
        firstTime=false;
        } else {
        twoRows_centered("WiFi is off.", "Going back to Menu");
        delay(3000);
        setState(12);
      }
    }
  } break;

  case 1211: {
    if(firstTime){
      if(useWiFi) {
      title_centered_2rows_wdata("WiFi_AP", "SSID",apid, "PWD",apwd);
      firstTime=false;
      } else {
        twoRows_centered("WiFi is off.", "Going back to Menu");
        delay(3000);
        setState(12);
      }
    }
  } break;

  case 1212: {
    if(firstTime){
      if(useWiFi) {
      title_centered_2rows_wdata("Browser credentials", "User",httpUser, "PWD",httpPwd);
      firstTime=false;
      } else {
        twoRows_centered("WiFi is off.", "Going back to Menu");
        delay(3000);
        setState(12);
        }
      }
  } break;

  case 13: {
    if(firstTime){
    twoRows_centered("Menu:", "SCD30");
    firstTime=false;
    }
  } break;

  case 130: {
    if(firstTime){
    //FRC calibration takes place inmediately, and it can be do multiple times at aribtrary intervals.
    //The sensor has to be run in a stable environment in continuous mode at a measurement rateof 2s for at least 2 minutes before triggering this calibration.
    twoRows_centered("Menu:", "Forced Calibration");
    firstTime=false;
    }
  } break;

  case 1300: {
    if(firstTime){
    threeRows_centered("Put the sensor outside", "and leave it there for 5m.", "Press -> when you are ready.");
    firstTime=false;
    }
  } break;

  case 13000: {
    twoRows_centered("Calibration in progress.", "Do not move the sensor");

    if(SCD30Calibration()) 
      setState(1);
    else
      setState(130);
  } break;

  case 131: {
    if(firstTime){
    twoRows_centered("SCD30 Configuration","and Firmware");
    firstTime=false;
    }
  } break;
  
  case 1310: {
    if(firstTime){
      if (airSensor.getForcedRecalibration(&settingVal) == true) { 
        twoRows_centered("Current Forced recalibration","factor is "+ String(settingVal)+ " ppm");
        firstTime=false;
      }
      else { 
        threeRows_centered("getForcedRecalibration", "failed.", "Loading Homepage...");
        delay(3000);
        setState(1);
      }
    }
  } break;

  case 1311: {
    if(firstTime){
      if (airSensor.getMeasurementInterval(&settingVal) == true) { 
        twoRows_centered("Measurement interval is", String(settingVal)+ " s");
        firstTime=false;
      }
      else { 
        threeRows_centered("getMeasurementInterval", "failed.", "Loading Homepage...");
        delay(3000);
        setState(1);
      }
    }
  } break;

  case 1312: {
    if(firstTime){
      if (airSensor.getTemperatureOffset(&settingVal) == true) { 
        twoRows_centered("Temperature offset is", String(((float)settingVal) / 100.0, 2)+" C");
        firstTime=false;
      }
      else { 
        threeRows_centered("getTemperatureOffset", "failed.", "Loading Homepage...");
        delay(3000);
        setState(1);
      }
    }
  } break;

  case 1313: {
    if(firstTime){
      if (airSensor.getAltitudeCompensation(&settingVal) == true) { 
        twoRows_centered("Altitude offset is", String(settingVal)+" m");
        firstTime=false;
      }
      else { 
        threeRows_centered("getAltitudeCompensation", "failed.", "Loading Homepage...");
        delay(3000);
        setState(1);
      }
    }
  } break;

  case 1314: {
    if(firstTime){
      if (airSensor.getAutoSelfCalibration()) { 
        twoRows_centered("Auto calibration is set to", "True");
      }
      else { 
        twoRows_centered("Auto calibration is set to", "False");
      }
    firstTime=false;
    }
  } break;

  case 1315: {
    if(firstTime){
      if (airSensor.getFirmwareVersion(&settingVal) == true) { 
        twoRows_centered("Firmware version is", "0x"+String(settingVal, HEX));
        firstTime=false;
      }
      else { 
        threeRows_centered("getFirmwareVersion", "failed.", "Loading Homepage...");
        delay(3000);
        setState(1);
      }
    }
  } break;

  case 14: {
    if(firstTime){
    twoRows_centered("Menu:", "General settings");
    firstTime=false;
    }
  } break;

  case 140: {
    if(firstTime){
    if (usePixel) {
      twoRows_centered("Menu:", "Turn Off Pixel Led");
    } else {
      twoRows_centered("Menu:", "Turn On Pixel Led");
    }
    firstTime=false;
    }
  } break;

  case 1400: {
    if (usePixel) {
      usePixel = false;
      twoRows_centered("Pixel Led is Off now","Going back to Homepage");
      delay(3000);
    } else {
      usePixel = true;
      twoRows_centered("Pixel Led is On now","Going back to Homepage");
      delay(3000);      
    }
    setState(1);
  } break;
  
  case 15: {
    if(firstTime){
    twoRows_centered("Menu:", "Power Options");
    firstTime=false;
    }
  } break;

  case 150: {
    if(firstTime){
    twoRows_centered("Menu:", "Power Off");
    firstTime=false;
    }
  } break;

  case 1500: {
    if (useSD) useSD=unMountSD();
    if (useWiFi) useWiFi=WiFiOff();
    twoRows_centered("","");
    esp_deep_sleep_start(); //wake up only on reset or restart
  } break;

  case 151: {
    if(firstTime){
    twoRows_centered("Menu:", "Reboot");
    firstTime=false;
    }
  } break;

  case 1510: {
    twoRows_centered("Rebooting", "Please wait");
    delay(3000);
    airSensor.reset();
    ESP.restart();
  } break;

  case 16: {
    if(firstTime){
    twoRows_centered("Menu:", "Last reset reason");
    firstTime=false;
    }
  } break;

  case 160: {
    if(firstTime){
    ri resetInfoCPU0 = saveResetReason(rtc_get_reset_reason(0));
    threeRows_centered("CPU0 reset reason:", resetInfoCPU0.resetReason, resetInfoCPU0.verboseResetReason);
    firstTime=false;
    }
  } break;

  case 161: {
    if(firstTime){
    ri resetInfoCPU1 = saveResetReason(rtc_get_reset_reason(1));
    threeRows_centered("CPU1 reset reason:", resetInfoCPU1.resetReason, resetInfoCPU1.verboseResetReason);
    firstTime=false;
    }
  } break;
  
  case 17: {
    if(firstTime){
    twoRows_centered("Menu:", "System Info");
    firstTime=false;
    }
  } break;

  case 170: {
    if(firstTime){
    twoRows_centered("AirSensor Firmware:", FIRMWARE_VERSION);
    firstTime=false;
    }
  } break;

  case 171: {
    if(firstTime){
    settingVal = bmpSensor.chipID();
    twoRows_centered("BMP390 Chip ID",String(settingVal));
    firstTime=false;
    }
  } break;

  case 172: {
    if(firstTime){
    twoRows_centered("CPU Frequency",String(getCpuFrequencyMhz())+" Mhz");
    firstTime=false;
    }
  } break;  

  default: {
    currentState = previousState; //The state is not defined so it'll go back to the previous state
    firstTime=false; //used to not execute the currentState again because it's not really changed
  } 
}

}