#include <sensors.h>

extern SCD30 airSensor;
extern Adafruit_BMP3XX bmpSensor;

//Sensors
void initializeSensors(boolean ASC, int samplingInterval) {

  Wire.begin();; //Initialize the I2C library
  //Wire.begin((int) SDA,(int) SCL);; //Initialize the I2C library (alt)

  //Power enable for Pixel and Stemma QT Connector
  digitalWrite(NEOPIXEL_I2C_POWER, HIGH); 
  delay(1500); //let the Stemma board receive power and be ready

  //Start sensor using the Wire port, and DISABLE the auto-calibration
  if (!airSensor.begin(Wire, ASC)) { //Initialize the SCD30 library
    twoRows_centered("SCD30 sensor not detected", "Please check wiring. Freezing");
    while (1); //infinite loop
    }

  //airSensor.enableDebugging();

  //Change number of seconds between measurements: 1 to 1800 (30 minutes), stored in non-volatile memory of SCD30
  //Power consumption minimized and response time maximized at high sampling intervals
  airSensor.setMeasurementInterval(samplingInterval);

  airSensor.useStaleData(true); //output the previous (stale) reading instead of zero

  //if (! bmp.begin_SPI(BMP_CS, BMP_SCK, BMP_MISO, BMP_MOSI)) {  // software SPI mode
  if (!bmpSensor.begin_SPI(BMP_CS)) {  //hardware SPI mode, Initialize the BMP library
    twoRows_centered("BMP390 sensor not detected", "Please check wiring. Freezing");
    while (1);
  }

  // Set up BMP oversampling and filter initialization (Default)
  bmpSensor.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
  bmpSensor.setPressureOversampling(BMP3_OVERSAMPLING_4X);
  bmpSensor.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
  bmpSensor.setOutputDataRate(BMP3_ODR_50_HZ);
}
bool SCD30Calibration() {
    uint16_t interval = airSensor.getMeasurementInterval();
    if (interval !=2)
      airSensor.setMeasurementInterval(2);

    unsigned long current_millis = millis();

    while (millis() < (current_millis + CALIBRATION_WAIT)) {
      //do nothing
    }

    if (airSensor.setForcedRecalibrationFactor(CO2_OFFSET)) {
      twoRows_centered("Calibration Done.", "Loading Homepage...");
      if (interval !=2)
        airSensor.setMeasurementInterval(interval); //change back to original value
      delay(3000); //let the user read
      return 1;
    } else {
      twoRows_centered("Calibration failed.", "Please try again");
      if (interval !=2)
        airSensor.setMeasurementInterval(interval); //change back to original value
      delay(3000); //let the user read
      return 0;
    }
}
void updateMinMaxValues(uint16_t co2, float temp, float hum, minmax *minmaxvalues) {
  if (co2<minmaxvalues->min_co2) minmaxvalues->min_co2=co2;
  if (co2>minmaxvalues->max_co2) minmaxvalues->max_co2=co2;
  if (temp<minmaxvalues->min_temp) minmaxvalues->min_temp=temp;
  if (temp>minmaxvalues->max_temp) minmaxvalues->max_temp=temp;
  if (hum<minmaxvalues->min_hum) minmaxvalues->min_hum=hum;
  if (hum>minmaxvalues->max_hum) minmaxvalues->max_hum=hum;
}
bool getDataFromSensors(uint16_t *co2, float *temp, float *hum, float *pressure, float *altitude, volatile int *partialUpdate) {
    static int hangCheckSCD30 = 0; //used to reset the SCD30 sensor after 4 times the data are not available

    if (airSensor.dataAvailable() && bmpSensor.performReading()) {
        static int first_reads =0; //Fix for initial bad values from the BMP390

        hangCheckSCD30 =0; //reset 

        if (first_reads <3) first_reads++; //ignore the "unstable" values of the first 3 cycles after the boot
        else {
          //temp2= bmpSensor.temperature; 
          *pressure = bmpSensor.pressure / 100.0; //mbar
          *altitude = bmpSensor.readAltitude(SEALEVELPRESSURE_HPA);
          airSensor.setAmbientPressure(*pressure); //mbar, can be 700 to 1200
          airSensor.setAltitudeCompensation(*altitude); //m
        }

        uint16_t tempco2 =airSensor.getCO2();

        if (tempco2 <380) {
          threeRows_centered("Bad CO2 (<380 ppm). Please","recalibrate the SCD30","or wait 20s");
          delay(20000);
          *partialUpdate = 0; //try again immediately
          return 0;
        }else {
        
        *co2=tempco2;
        *temp=airSensor.getTemperature();
        // if (first_reads >=3) 
        //  temp = (temp+temp2)/2;
        *hum=airSensor.getHumidity();
        return 1;
        }
      }
      else if (!airSensor.dataAvailable()) {
        if (hangCheckSCD30 <4) {
          threeRows_centered("Waiting for new data","from the SCD30 Sensor","(20s)");

          hangCheckSCD30++;
        } else {
          threeRows_centered("Resetting the SCD30 Sensor.","Please Wait","(20s)");
          airSensor.reset();
          hangCheckSCD30 = 0;
        }

        delay(20000);
        *partialUpdate = 0;
        return 0;
      }
      else {
        twoRows_centered("Waiting for new data","from the BMP390 Sensor");
        delay(20000);
        *partialUpdate = 0; 
        return 0;
      }
}
