#include <microsd.h>

//microSD
bool mountSD(){
  //if (digitalRead(SD_DET)==HIGH) { 
    if (!SD.begin(SD_CS)) { //Initialize SD library and card
      twoRows_centered("SD mount failed. Please","check SD or insert it again");
      delay(3000);
      return 0;
    } else {
      twoRows_centered("SD mounted!","Please wait");
      delay(3000);
      return 1;
    }
  //} else {
  //  twoRows_centered("SD not inserted. Please","check SD or insert it again");
  //    delay(3000);
  //    return 0;
  //}      
}
bool unMountSD(){
    SD.end(); 
    twoRows_centered("SD unmounted!","Please wait");
    delay(3000);
    return 0;
}

int saveLog (uint16_t co2, float temp, float hum, float pressure, float altitude, fs::FS &fs) {
  struct tm time;
  if(!getLocalTime(&time)){ //getLocalTime saves all the details about date and time and save them on timeinfo
    twoRows_centered("Failed to obtain time.","Freezing");
    delay(3000);
    return 0;
  }

  char datehour[30];
  strftime(datehour,30,"%Y-%m-%d %H", &time); //Specifiers from strftime function
  String filename= "/"+String(datehour)+".txt";
  
  Serial.print("Writing file: ");
  Serial.println(filename);

  File log = fs.open(filename); //Open the file
  if(!log) {
    Serial.println("File doesn't exist");
    Serial.println("Creating file...");
    log.close();
    File log = fs.open(filename, FILE_WRITE);
    if(log) {
      log.println("Day,Month,Year,Hour,Minute,Second,CO2 [ppm],Temp [C],Humidity [%],Pressure [mbar],Altitude [m]");
      log.print(&time,"%d");
      log.print(",");
      log.print(&time,"%m");
      log.print(",");
      log.print(&time,"%Y");
      log.print(",");
      log.print(&time,"%H");
      log.print(",");
      log.print(&time,"%M");
      log.print(",");
      log.print(&time,"%S");
      log.print(",");
      log.print(co2);
      log.print(",");
      log.printf("%.1f", temp);
      log.print(",");
      log.printf("%.1f", hum);
      log.print(",");
      log.printf("%.1f", pressure);
      log.print(",");
      log.printf("%.1f\n", altitude);
      log.close();
      Serial.println("File created");
      return 2;
    } else {
    threeRows_centered("Error creating the file.","Your SD may be", "damaged or full");
    delay(3000);
    return 1;
    }
  }
  else {
    Serial.println("File already exists");  
    log.close();
    File log = fs.open(filename, FILE_APPEND); //FILE_APPEND append the text
    if(log) {
      log.print(&time,"%d");
      log.print(",");
      log.print(&time,"%m");
      log.print(",");
      log.print(&time,"%Y");
      log.print(",");
      log.print(&time,"%H");
      log.print(",");
      log.print(&time,"%M");
      log.print(",");
      log.print(&time,"%S");
      log.print(",");
      log.print(co2);
      log.print(",");
      log.printf("%.1f", temp);
      log.print(",");
      log.printf("%.1f", hum);
      log.print(",");
      log.printf("%.1f", pressure);
      log.print(",");
      log.printf("%.1f\n", altitude);
      log.close();
      Serial.println("Data appended");
      return 2;
    } else {
      threeRows_centered("Failed to open file for","appending. Your SD may","be damaged or full");
      delay(3000);
      return 1;
    }
  }
}
void removeAllFiles (fs::FS &fs) {
    File file;
    file = fs.open("/");
    if(!file){
        Serial.println("Failed to open root");
        return;
    }
    File entry =  file.openNextFile();
    while (entry) {
      Serial.print("DELETING: ");
      Serial.println(entry.name());
      if(entry.isDirectory()) {
          if (fs.rmdir("/"+String(entry.name()))) 
            Serial.println("Dir removed");
          else 
            Serial.println("rmdir failed");
      } else
          if (fs.remove("/"+String(entry.name()))) 
            Serial.println("File removed");
          else 
          Serial.println("rm failed");
    entry = file.openNextFile();
    }
}
