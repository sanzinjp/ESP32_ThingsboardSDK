// OLEDdisplay.h
#ifndef OLED_DISPLAY_H
#define OLED_DISPLAY_H

#include <Adafruit_SSD1306.h>

void displaySensorData(Adafruit_SSD1306 &display, float temp, float hum);
extern Adafruit_SSD1306 display;


#endif
