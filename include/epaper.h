#ifndef EPAPER_H
#define EPAPER_H

//E-Paper Display pins
#define EPD_CS A5
#define DC A1
#define RST A0
#define BUSY 14

// base class GxEPD2_GFX can be used to pass references or pointers to the display instance as parameter, uses ~1.2k more code
// enable or disable GxEPD2_GFX base class
#define ENABLE_GxEPD2_GFX 0

//E-Paper Display Selection
// b/w display class
#define GxEPD2_DISPLAY_CLASS GxEPD2_BW
// 2.7" b/w display driver class
#define GxEPD2_DRIVER_CLASS GxEPD2_270     // GDEW027W3   176x264, EK79652 (IL91874)

#define GxEPD2_BW_IS_GxEPD2_BW true
#define IS_GxEPD(c, x) (c##x)
#define IS_GxEPD2_BW(x) IS_GxEPD(GxEPD2_BW_IS_, x)

#define MAX_DISPLAY_BUFFER_SIZE 65536ul // e.g.
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))

#endif

void initDisplay();

//Full and partial update support
void homepage(uint16_t co2, float temp, float hum, int partialUpdate, int percentage, boolean useSD, boolean useWiFi);
//Generic display functions
//Full update
void twoRows_centered(const char *first, String second);
void threeRows_centered(String first, String second, String third);
void title_centered_2rows_wdata(const char *first, const char *second_text, String second_data, const char *third_text, String third_data);
//Full and partial update support
void title_centered_1row_intinput(const char *first, int value, boolean yes_no, int partialUpdate);
void twoRows_centered_1row_intinput(const char *first, String second, int value, boolean yes_no, int partialUpdate);