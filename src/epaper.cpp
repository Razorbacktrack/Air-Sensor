#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>

//minimal libraries for E-Paper Display
#include <GxEPD2_BW.h> //2-color E-Papers
#include <Fonts/FreeSansBold9pt7b.h>
#include "bitmaps/Bitmaps176x264.h" // bitmap for 2.7" b/w E-Paper
#include <epaper.h>

GxEPD2_DISPLAY_CLASS<GxEPD2_DRIVER_CLASS, MAX_HEIGHT(GxEPD2_DRIVER_CLASS)> display(GxEPD2_DRIVER_CLASS(EPD_CS, DC, RST, BUSY));

void initDisplay() {
  display.init(115200);
}

//Display

/*
0 x-->
y
|
v
*/

//Full and partial update support
void homepage(uint16_t co2, float temp, float hum, int partialUpdate, int percentage, boolean useSD, boolean useWiFi)
{

  display.setRotation(3); 
  display.setFont(&FreeSansBold9pt7b); 
  display.setTextColor(GxEPD_BLACK);

  const char co2base[] = "CO2 [ppm]:";
  const char tempbase[] = "Temp [C]:";
  const char humbase[] = "Humidity [%]:";
  
  //Convert the values to strings
  String co2string = String(co2);
  String tempstring = String(temp,1); //one decimal 
  String humstring = String(hum,1);

  int16_t tbx, tby; uint16_t tbw, tbh; //text base
  int16_t vsx, vsy; uint16_t vsw, vsh; //value string

  //Determine the smallest rectangle encompassing a string.
  display.getTextBounds(co2base, 0, 0, &tbx, &tby, &tbw, &tbh); 
  //string, starting cursor position, last four values will then contain the upper-left corner
  // and the width & height of the area covered by this text.
  display.getTextBounds(co2string, 0, 0, &vsx, &vsy, &vsw, &vsh);  

  // center the bounding box by transposition of the origin:

  //Variables for FULL and PARTIAL Update
  uint16_t co2y = ((display.height() * 1/4) - tbh / 2) - tby +10; //1/4 of the screen (y-axis), used for TEXT and VALUE, +10px to be closer to the center
  uint16_t co2vsx = display.width()- (vsw+10); //used for the VALUE, end of the screen - (width of the value string+10px)
  //x for text base is 10px

  if(partialUpdate !=0) { //Partial update of the VALUE ONLY

  //Variables for PARTIAL Update (partial window size)
  uint16_t wx = 10+tbx+tbw; //10+tbx is the starting point of co2base (TEXT), +tbw to be after the string (right after the :)
  uint16_t wy = co2y+tby; //co2y+tby is the starting point of co2base
  display.setPartialWindow(wx, wy, (display.width() - wx), vsh); //from ":" to the end of the screen, with the height of the value

  //paged drawing to use less RAM
  display.firstPage(); 
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(co2vsx, co2y); // print  from the end of the screen - (width of the value+10px) for X-AXIS
    display.print(co2string);
  }
  while (display.nextPage());
  }


  display.getTextBounds(tempbase, 0, 0, &tbx, &tby, &tbw, &tbh); 
  display.getTextBounds(tempstring, 0, 0, &vsx, &vsy, &vsw, &vsh);  
  uint16_t tempy = ((display.height() * 1/2) - tbh / 2) - tby; //2/4 (1/2) of the screen (y-axis)
  uint16_t tempvsx = display.width()- (vsw+10);


  if(partialUpdate !=0) {

  uint16_t wx = 10+tbx+tbw;
  uint16_t wy = tempy+tby;
  display.setPartialWindow(wx, wy, (display.width() - wx), vsh);

  display.firstPage(); 
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(tempvsx, tempy);
    display.print(tempstring);
  }
  while (display.nextPage());
  }


  display.getTextBounds(humbase, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.getTextBounds(humstring, 0, 0, &vsx, &vsy, &vsw, &vsh);  
  uint16_t humy = ((display.height() * 3/4) - tbh / 2) - tby -10; //3/4 of the screen (y-axis) -10px to be closer to the center
  uint16_t humvsx = display.width()- (vsw+10);

  if(partialUpdate !=0) {

  uint16_t wx = 10+tbx+tbw;
  uint16_t wy = humy+tby;
  display.setPartialWindow(wx, wy, (display.width() - wx), vsh);

  display.firstPage(); 
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(humvsx, humy);
    display.print(humstring);
  }
  while (display.nextPage());
  }


  if (partialUpdate==0) { //Full screen update (TEXT AND VALUE)

    String SDString;
    if(useSD) SDString = "SD: Yes"; else SDString = "SD: No";
    String WiFiString;
    if(useWiFi) WiFiString = " WiFi: "+WiFi.localIP().toString();
    else WiFiString = " WiFi: No";

    String batterystring = "Batt. " + String(percentage) + " %";
    display.getTextBounds(batterystring, 0, 0, &tbx, &tby, &tbw, &tbh);
    //uint16_t batx = display.width()- (tbw+10);
    uint16_t batx =((display.width() - tbw) / 2) - tbx; //center of the screen (x-axis)
    uint16_t baty = tbh+10;

    display.getTextBounds(SDString+WiFiString, 0, 0, &tbx, &tby, &tbw, &tbh);
    //uint16_t batx = display.width()- (tbw+10);
    uint16_t statusx =((display.width() - tbw) / 2) - tbx; //center of the screen (x-axis)
    uint16_t statusy = (display.height() - 10);

    //full screen update
    display.setFullWindow();

    //paged drawing to use less RAM
    display.firstPage(); 
    do
    {
      display.fillScreen(GxEPD_WHITE);

      display.setCursor(10, co2y);// print from 10px for X-AXIS, 1/4 of the screen (Y-AXIS)
    // display.setTextSize(1); //multiply the scale of the text by a given integer factor
      display.print(co2base);
    // display.setTextSize(2);
      display.setCursor(co2vsx,co2y);// print  from the end of the screen - (width of the value+10px) for X-AXIS
      display.print(co2string);

      display.setCursor(10, tempy);
      display.print(tempbase);
      display.setCursor(tempvsx,tempy);
      display.print(tempstring);

      display.setCursor(10, humy);
      display.print(humbase);
      display.setCursor(humvsx,humy);
      display.print(humstring);

      display.setCursor(batx, baty);
      display.print(batterystring);

      display.setCursor(statusx, statusy);
      display.print(SDString+WiFiString);
    }
    while (display.nextPage());
  }
  display.hibernate();
}
//Generic display functions
//Full update
void twoRows_centered(const char *first, String second) {
  display.setRotation(3); 
  display.setFont(&FreeSansBold9pt7b); 
  display.setTextColor(GxEPD_BLACK);
  
  int16_t tbx, tby; uint16_t tbw, tbh;

  display.getTextBounds(first, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t firstx = ((display.width() - tbw) / 2) - tbx; //center of the screen (x-axis)
  uint16_t firsty = ((display.height() * 1/3) - tbh / 2) - tby; //1/3 of the screen (y-axis)

  display.getTextBounds(second, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t secondx = ((display.width() - tbw) / 2) - tbx;
  uint16_t secondy = ((display.height() * 2/3) - tbh / 2) - tby; //2/3 of the screen (y-axis)

  display.setFullWindow();

  display.firstPage(); 
  do
  {
    display.fillScreen(GxEPD_WHITE);
  
    display.setCursor(firstx, firsty);
    //display.setTextSize(1); //multiply the scale of the text by a given integer factor
    display.print(first);

    display.setCursor(secondx, secondy);
    display.print(second);

  }
  while (display.nextPage());
  display.hibernate();

  Serial.print(first);
  Serial.print(" ");
  Serial.println(second);

}
void threeRows_centered(String first, String second, String third) {
  display.setRotation(3); 
  display.setFont(&FreeSansBold9pt7b); 
  display.setTextColor(GxEPD_BLACK);
  
  int16_t tbx, tby; uint16_t tbw, tbh;

  display.getTextBounds(first, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t firstx = ((display.width() - tbw) / 2) - tbx; //center of the screen (x-axis)
  uint16_t firsty = ((display.height() * 1/4) - tbh / 2) - tby; //1/4 of the screen (y-axis)

  display.getTextBounds(second, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t secondx = ((display.width() - tbw) / 2) - tbx;
  uint16_t secondy = ((display.height() * 1/2) - tbh / 2) - tby; //2/4 of the screen (y-axis)

  display.getTextBounds(third, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t thirdx = ((display.width() - tbw) / 2) - tbx;
  uint16_t thirdy = ((display.height() * 3/4) - tbh / 2) - tby; //3/4 of the screen (y-axis)

  display.setFullWindow();

  display.firstPage(); 
  do
  {
    display.fillScreen(GxEPD_WHITE);
  
    display.setCursor(firstx, firsty);
    //display.setTextSize(1); //multiply the scale of the text by a given integer factor
    display.print(first);

    display.setCursor(secondx, secondy);
    display.print(second);

    display.setCursor(thirdx, thirdy);
    display.print(third);

  }
  while (display.nextPage());
  display.hibernate();

  Serial.print(first);
  Serial.print(" ");
  Serial.print(second);
  Serial.print(" ");
  Serial.println(third);

}
void title_centered_2rows_wdata(const char *first, const char *second_text, String second_data, const char *third_text, String third_data) {
  display.setRotation(3); 
  display.setFont(&FreeSansBold9pt7b); 
  display.setTextColor(GxEPD_BLACK);
  
  int16_t tbx, tby; uint16_t tbw, tbh; //text
  int16_t vsx, vsy; uint16_t vsw, vsh; //data

  //Determine the smallest rectangle encompassing a string.
  display.getTextBounds(first, 0, 0, &tbx, &tby, &tbw, &tbh); 
  //string, starting cursor position, last four values will then contain the upper-left corner
  // and the width & height of the area covered by this text.

  // center the bounding box by transposition of the origin:
  uint16_t firstx = ((display.width() - tbw) / 2) - tbx; //center of the screen (x-axis)
  uint16_t firsty = ((display.height() * 1/4) - tbh / 2) - tby; //1/4 of the screen (y-axis)

  display.getTextBounds(second_text, 0, 0, &tbx, &tby, &tbw, &tbh); 
  display.getTextBounds(second_data, 0, 0, &vsx, &vsy, &vsw, &vsh);  
  uint16_t secondy = ((display.height() * 1/2) - tbh / 2) - tby; //2/4 of the screen (y-axis)
  uint16_t seconddx = display.width() - (vsw+10); // end of the screen - (width of the data+10px) (x-axis)

  display.getTextBounds(third_text, 0, 0, &tbx, &tby, &tbw, &tbh);
  display.getTextBounds(third_data, 0, 0, &vsx, &vsy, &vsw, &vsh);  
  uint16_t thirdy = ((display.height() * 3/4) - tbh / 2) - tby; //3/4 of the screen (y-axis)
  uint16_t thirddx = display.width()- (vsw+10);

  //full screen update
  display.setFullWindow();

  //paged drawing to use less RAM
  display.firstPage(); 
  do
  {
    display.fillScreen(GxEPD_WHITE);

    display.setCursor(firstx, firsty);
    //display.setTextSize(1); //multiply the scale of the text by a given integer factor
    display.print(first);

    display.setCursor(10, secondy); //print from 10px (x-axis)
    display.print(second_text);
    display.setCursor(seconddx,secondy);
    display.print(second_data);

    display.setCursor(10, thirdy);
    display.print(third_text);
    display.setCursor(thirddx,thirdy);
    display.print(third_data);
  }
  while (display.nextPage());
  display.hibernate();

  Serial.println(first);
  Serial.print(second_text);
  Serial.print(" ");
  Serial.println(second_data);
  Serial.print(third_text);
  Serial.print(" ");
  Serial.println(third_data);

}
//Full and partial update support
void title_centered_1row_intinput(const char *first, int value, boolean yes_no, int partialUpdate){
  display.setRotation(3); 
  display.setFont(&FreeSansBold9pt7b); 
  display.setTextColor(GxEPD_BLACK);
  
  int16_t tbx, tby; uint16_t tbw, tbh;
  int16_t vsx, vsy; uint16_t vsw, vsh; //data

  display.getTextBounds(first, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t firstx = ((display.width() - tbw) / 2) - tbx; //center of the screen (x-axis)
  uint16_t firsty = ((display.height() * 1/3) - tbh / 2) - tby; //1/3 of the screen (y-axis)

  String second;

  if (yes_no) {
    if (value == 0 ) second = "No";
    else second = "Yes";
  }
  else second = String(value);
   
  display.getTextBounds(second, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t secondx = ((display.width() - tbw) / 2) - tbx;
  uint16_t secondy = ((display.height() * 2/3) - tbh / 2) - tby; //2/3 of the screen (y-axis)

  if(partialUpdate !=0) { //Partial update of the VALUE ONLY

  //Variables for PARTIAL Update (partial window size)
  uint16_t wy = secondy+tby; //secondy+tby is the starting point of the second string
  display.setPartialWindow(0, wy, display.width(), tbh); 

  //paged drawing to use less RAM
  display.firstPage(); 
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(secondx, secondy); // print  from the end of the screen - (width of the value+10px) for X-AXIS
    display.print(second);
  }
  while (display.nextPage());
  }

  if(partialUpdate ==0) { //Full screen update (TEXT AND VALUE)
    display.setFullWindow();

    display.firstPage(); 
    do
    {
      display.fillScreen(GxEPD_WHITE);
    
      display.setCursor(firstx, firsty);
      //display.setTextSize(1); //multiply the scale of the text by a given integer factor
      display.print(first);

      display.setCursor(secondx, secondy);
      display.print(second);

    }
    while (display.nextPage());
  }
  display.hibernate();

  Serial.print(first);
  Serial.print(" ");
  Serial.println(value);

}
void twoRows_centered_1row_intinput(const char *first, String second, int value, boolean yes_no, int partialUpdate) {
  display.setRotation(3); 
  display.setFont(&FreeSansBold9pt7b); 
  display.setTextColor(GxEPD_BLACK);
  
  int16_t tbx, tby; uint16_t tbw, tbh;

  display.getTextBounds(first, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t firstx = ((display.width() - tbw) / 2) - tbx; //center of the screen (x-axis)
  uint16_t firsty = ((display.height() * 1/4) - tbh / 2) - tby; //1/4 of the screen (y-axis)

  display.getTextBounds(second, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t secondx = ((display.width() - tbw) / 2) - tbx;
  uint16_t secondy = ((display.height() * 1/2) - tbh / 2) - tby; //2/4 of the screen (y-axis)

  String third;

  if (yes_no) {
    if (value == 0 ) third = "No";
    else third = "Yes";
  }
  else third = String(value);

  display.getTextBounds(third, 0, 0, &tbx, &tby, &tbw, &tbh);
  uint16_t thirdx = ((display.width() - tbw) / 2) - tbx;
  uint16_t thirdy = ((display.height() * 3/4) - tbh / 2) - tby; //3/4 of the screen (y-axis)

  if(partialUpdate !=0) { //Partial update of the VALUE ONLY

  //Variables for PARTIAL Update (partial window size)
  uint16_t wy = thirdy+tby; //thirdy+tby is the starting point of the third string
  display.setPartialWindow(0, wy, display.width(), tbh); 

  //paged drawing to use less RAM
  display.firstPage(); 
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(thirdx, thirdy); // print  from the end of the screen - (width of the value+10px) for X-AXIS
    display.print(third);
  }
  while (display.nextPage());
  }

  if(partialUpdate ==0) { //Full screen update (TEXT AND VALUE)
    display.setFullWindow();

    display.firstPage(); 
    do
    {
      display.fillScreen(GxEPD_WHITE);
    
      display.setCursor(firstx, firsty);
      //display.setTextSize(1); //multiply the scale of the text by a given integer factor
      display.print(first);

      display.setCursor(secondx, secondy);
      display.print(second);

      display.setCursor(thirdx, thirdy);
      display.print(third);

    }
    while (display.nextPage());
  }
  display.hibernate();

  Serial.print(first);
  Serial.print(" ");
  Serial.print(second);
  Serial.print(" ");
  Serial.println(value);

}
