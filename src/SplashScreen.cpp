#include "SplashScreen.h"
#include "Config.h"
#include "GlobalVariables.h"
#include "splash.h"
#include "NotoSans_Bold6pt7b.h"
#include "NotoSans_Bold16pt7b.h"

// Font definitions
#define AA_FONT_FREE_SMALL &NotoSans_Bold6pt7b
#define AA_FONT_FREE_MEDIUM &NotoSans_Bold16pt7b

// External display object
extern TFT_eSPI display;
extern const char* version;

void showAnimatedSplashScreen() {
  display.fillScreen(TFT_BLACK);
  display.setFreeFont(AA_FONT_FREE_MEDIUM);
  display.setTextColor(TFT_WHITE, TFT_BLACK);
  display.setTextDatum(TC_DATUM);
  
  int centerX = display.width() / 2;
  int centerY = (display.height() / 2) - 35;
  
  // Draw "Made for" text
  display.setFreeFont(AA_FONT_FREE_SMALL);
  display.drawString("Made for", centerX - 20, centerY - 8);
  
  // Draw Speeduino logo bitmap (180x57 pixels)
  // Center the logo horizontally: (320 - 180) / 2 = 70
  display.drawBitmap(70, centerY, splash_logo, 180, 57, TFT_WHITE);
  
  // Draw version info
  display.setFreeFont(AA_FONT_FREE_SMALL);
  // display.drawString("Firmware v" + String(version), centerX, 170 - 35);
  display.drawString("Powered by " + String(ESP.getChipModel()) + " Rev" + String(ESP.getChipRevision()), centerX, 170 - 15);

  delay(2000); // Show splash for 2 seconds
  display.fillScreen(TFT_BLACK);
}