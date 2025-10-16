#include "drawing_utils.h"
#include <TFT_eSPI.h>

// External display objects
extern TFT_eSPI display;

void drawCenteredTextSmall(int x, int y, int w, int h, const char* text, int textSize, uint16_t color) {
  display.setTextDatum(MC_DATUM);
  display.setTextColor(color, TFT_BLACK);
  display.drawString(text, x, y); 
}

void drawSmallButton(int x, int y, const char* label, bool value) {
  const int BTN_WIDTH = 50;  // Reduced to fit 6 indicators (320/6 = 53, so 50 with gaps)
  const int BTN_HEIGHT = 18; // Slightly smaller height
  uint16_t activeColor = TFT_ORANGE;
  
  // Special colors for specific indicators
  if (strcmp(label, "REV") == 0 || strcmp(label, "LCH") == 0) {
    activeColor = TFT_RED;
  }
  
  uint16_t fillColor = TFT_BLACK;
  uint16_t textColor = value ? activeColor : TFT_WHITE;
  
  // Draw button background
  display.fillRoundRect(x, y, BTN_WIDTH, BTN_HEIGHT, 3, fillColor);
  display.drawRoundRect(x, y, BTN_WIDTH, BTN_HEIGHT, 3, TFT_WHITE);
  
  // Draw button text
  display.setTextDatum(MC_DATUM);
  display.setTextColor(textColor);
  display.drawString(label, x + BTN_WIDTH/2, y + BTN_HEIGHT/2);
}
