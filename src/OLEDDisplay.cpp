// OLEDdisplay.cpp
#include "OLEDdisplay.h"
#include "Icons.h"
#include <Adafruit_GFX.h>

void displaySensorData(Adafruit_SSD1306 &display, float temp, float hum) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE); // Set text color to white

  // Temperature icon
  display.drawBitmap(0, 0, tempIcon, 64, 32, SSD1306_WHITE);
  display.setCursor(30, 10);
  display.setTextSize(2.5);
  display.print(temp, 1);

  // Degree symbol
  int x = display.getCursorX();
  int y = display.getCursorY();
  x += 5;
  display.setCursor(x, y);
  display.drawCircle(x + 2, y + 2, 2, SSD1306_WHITE);
  display.setCursor(x + 7, y);
  display.print("C");

  // Humidity icon
  display.drawBitmap(0, 32, humidIcon, 64, 32, SSD1306_WHITE);
  display.setCursor(30, 42);
  display.setTextSize(2.5);
  display.print(hum, 1);
  display.print(" %");

  display.display();
}
