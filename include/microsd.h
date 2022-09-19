#include <Arduino.h>
#include <SPI.h> //library to communicate with devices using SPI

//Library for E-Paper Display
#include <epaper.h>

//libraries for SD
#include <FS.h> //file system wrapper
#include <SD.h> //Enables reading and writing on SD cards with SPI

#ifndef MICROSD_H
#define MICROSD_H

//MicroSD extra pins (same SPI as E-Paper Display)
#define SD_CS 32
//#define SD_DET A2 //False= no card

#endif


bool mountSD();
bool unMountSD();
int saveLog (uint16_t co2, float temp, float hum, float pressure, float altitude, fs::FS &fs);
void removeAllFiles (fs::FS &fs);
