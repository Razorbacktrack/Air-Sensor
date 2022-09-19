#include <Wire.h> //This library allows you to communicate with I2C/TWI devices
#include <SparkFun_SCD30_Arduino_Library.h> // Sparkfun library for SCD30 Sensor
#include <SPI.h> //library to communicate with devices using SPI

//libraries for BMP390
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP3XX.h>

//Library for E-Paper Display
#include <epaper.h>

#ifndef SENSORS_H
#define SENSORS_H

//Min-Max Values
typedef struct  {
  uint16_t min_co2;
  uint16_t max_co2;
  float min_temp;
  float max_temp;
  float min_hum;
  float max_hum;
} minmax;

//#define SCD30_ADDRESS 0x61 //I2C Address of SCD30 sensor, already in SparkFun_SCD30_Arduino_Library.h
//#define SCD30Wire Wire
#define CO2_OFFSET 415 //Force Recalibration Factor, 400-2000ppm
#define CALIBRATION_WAIT 300000 //millis, 5 minutes

//BMP390 (same SPI as E-Paper Display)
#define BMP_CS 33
/* 1013.25 is default value. It should  be the hPa pressure at the level sea
for the current locatOTA ion and day to maximize the altitude accuracy */
#define SEALEVELPRESSURE_HPA (1013.25)

#endif

void initializeSensors(boolean ASC, int samplingInterval);
bool SCD30Calibration();
void updateMinMaxValues(uint16_t co2, float temp, float hum, minmax *minmaxvalues);
bool getDataFromSensors(uint16_t *co2, float *temp, float *hum, float *pressure, float *altitude, volatile int *partialUpdate);