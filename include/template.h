#include <Arduino.h>

#ifndef TEMPLATE_H
#define TEMPLATE_H

String PageHTML(){
  //if you have to use " on the html page you have to add \ before "
  String content = 
"<!DOCTYPE html>"
"<html>"
"<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">"
"<title>AirSensor Homepage</title>"
"<style>"
"html { font-family: Helvetica; display: inline-block;	margin: 0px auto; text-align: center;}"
"body{ margin-top: 50px;}"
"h1 { color: #444444; margin: 50px auto 30px;}"
"h2 { color: #444444; margin-bottom: 50px;}"
".dataname { font-size: 28px; color: #444444; margin-bottom: 10px;}"
".value { font-size: 28px; color: #888; margin-bottom: 10px;}"
".button{ display: block; width: 80px; background-color: #3498db; border: none; color: white; padding: 13px 30px; text-decoration: none; font-size: 25px; margin: 0px auto 35px; cursor: pointer; border-radius: 4px;}"
"</style>"
"</head>"
"<body>"
"<h1>SCD30 Values</h1>"
"<p class=\"dataname\"><strong>CO2 [ppm]</strong></p>"
"<p class=\"value\">{{co2value}}</p>"
"<p class=\"dataname\"><strong>Temperature [C]</strong></p>"
"<p class=\"value\">{{tmpvalue}}</p>"
"<p class=\"dataname\"><strong>Humidity [%]</strong></p>"
"<p class=\"value\">{{humvalue}}</p>"
"<br>"
"<br>"
"<h1>BMP390 Values</h1>"
"<p class=\"dataname\"><strong>Pressure [mbar]</strong></p>"
"<p class=\"value\">{{pressurevalue}}</p>"
"<p class=\"dataname\"><strong>Altitude [m]</strong></p>"
"<p class=\"value\">{{altitudevalue}}</p>"
"<br>"
"<br>"
"<a class=\"button\" href=\"/_ac/update\">OTA</a>"
"<a class=\"button\" href=\"/_ac\">Setup</a>"
"</body>"
"</html>"
;

return content;
}

#endif
