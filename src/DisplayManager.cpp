#include "DisplayManager.h"
#include "Config.h"
#include "DataTypes.h"
#include "DisplayConfig.h"
#include "drawing_utils.h"
#include "SplashScreen.h"
#include "NotoSans_Bold6pt7b.h"
#include "NotoSans_Bold16pt7b.h"
#include <EEPROM.h>
#if ENABLE_SIMULATOR
#include "Simulator.h"
#endif

// Font definitions
#define AA_FONT_FREE_SMALL &NotoSans_Bold6pt7b
#define AA_FONT_FREE_MEDIUM &NotoSans_Bold16pt7b

TFT_eSPI display = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&display);

// Display settings
bool isColorFull = false;

void setupDisplay() {
  display.init();
  display.setRotation(3);
  // Initialize display configuration
  initializeDisplayConfig();
}

void drawSplashScreenWithImage() {
  // Use the new modular animated splash screen
  showAnimatedSplashScreen();
}

// Forward declarations
void drawDynamicDataPanel(const DisplayPanel &panel, bool setup);
void drawRPMPanel(int x, int y, const char *label, unsigned int value, uint16_t color, unsigned int lastValue, bool setup);
void drawIntPanel(int x, int y, const char *label, int value, uint16_t color, int lastValue, bool setup);
void drawFloatPanel(int x, int y, const char *label, float value, uint16_t color, float lastValue, int decimals, bool setup);
void addDataPanel(int position, const char* label, uint8_t dataSource, bool enabled, int decimals);
void addIndicator(int position, const char* label, uint8_t indicator, bool enabled);
void lablDraw(int x, int y, const char *label, int type);

void drawConfigurablePanels(bool setup) {
  // Draw each enabled panel using stored coordinates
  for (int i = 0; i < currentDisplayConfig.activePanelCount; i++) {
    DisplayPanel &panel = currentDisplayConfig.panels[i];
    if (panel.enabled) {
      drawDynamicDataPanel(panel, setup);
    }
  }
}

// New dynamic panel drawing function using position mapping
void drawDynamicDataPanel(const DisplayPanel &panel, bool setup) {
  // Panel positions for 320x170 display - 9 panels layout (4+5)
  // Top row (4 panels): CLT, IAT, AFR, BAT  
  // Bottom row (5 panels): RPM, FP, TPS, MAP, ADV
  const int panelPositions[9][2] = {
    // Baris atas (4 panel) - Width per panel: 320/4 = 80px
    {0, 10},    // Position 0: CLT (top row)
    {70, 10},   // Position 1: IAT (top row) 
    {145, 10},  // Position 2: AFR (top row)
    {245, 10},  // Position 3: BAT (top row)
    
    // Baris bawah (5 panel) - Width per panel: 320/5 = 64px
    {0, 80},    // Position 4: RPM (bottom row)
    {74, 80},   // Position 5: FP (bottom row)
    {138, 80},  // Position 6: TPS (bottom row)
    {202, 80},  // Position 7: MAP (bottom row)
    {256, 80}   // Position 8: ADV (bottom row) - exact 64px spacing
  };
  
  if (panel.position >= 9) return;
  
  // Get coordinates from position array
  int x = panelPositions[panel.position][0];
  int y = panelPositions[panel.position][1];
  
  // Get current value based on data source
  float currentValue = getDataValue(panel.dataSource);
  
  // Get color based on data source and value
  uint16_t color = getDataSourceColor(panel.dataSource, currentValue);
  
  // Use static array to track last values for each panel position
  static float lastValues[9];
  static bool initialized = false;
  
  // Initialize array on first run
  if (!initialized) {
    for (int i = 0; i < 9; i++) {
      lastValues[i] = -999.0;
    }
    initialized = true;
  }
  
  // Only redraw if value changed or setup
  int panelIndex = panel.position < 9 ? panel.position : 0;
  if (setup || lastValues[panelIndex] != currentValue || first_run) {
    
    // For RPM, use special drawing function 
    if (panel.dataSource == DATA_SOURCE_RPM) {
      drawRPMPanel(x, y, panel.label, (unsigned int)currentValue, color, (unsigned int)lastValues[panelIndex], setup || first_run);
    } else {
      // Use appropriate drawing function based on decimals
      if (panel.decimals > 0) {
        drawFloatPanel(x, y, panel.label, currentValue, color, lastValues[panelIndex], panel.decimals, setup || first_run);
      } else {
        drawIntPanel(x, y, panel.label, (int)currentValue, color, (int)lastValues[panelIndex], setup || first_run);
      }
    }
    
    lastValues[panelIndex] = currentValue;
  }
}

// Specialized drawing functions for different data types
void drawRPMPanel(int x, int y, const char *label, unsigned int value, uint16_t color, unsigned int lastValue, bool setup) {
  lablDraw(x, y, label, 0);
  if (lastValue != value || forceRefresh || setup || first_run) {
    // Apply color logic based on isColorFull
    uint16_t textColor = TFT_ORANGE;  // Default when isColorFull = false
    if (isColorFull) {
      if (value > 6000) {
        textColor = TFT_RED;
      } else if (value > 4000) {
        textColor = TFT_YELLOW;
      } else {
        textColor = TFT_GREEN;
      }
    }
    
    spr.setFreeFont(AA_FONT_FREE_MEDIUM);
    spr.createSprite(90, 40);
    spr.fillSprite(TFT_BLACK);  // Clear sprite background
    spr.setTextDatum(TR_DATUM);
    spr_width = spr.textWidth("9999");
    spr.setTextColor(textColor, TFT_BLACK, true);
    spr.drawNumber(value, spr_width, 3);
    spr.pushSprite(x, y + 35 - 15);
    spr.deleteSprite();
  }
}

void drawIntPanel(int x, int y, const char *label, int value, uint16_t color, int lastValue, bool setup) {
  if (lastValue != value || forceRefresh || setup || first_run) {
    lablDraw(x, y, label, 0);
    
    // Apply color logic based on isColorFull and label
    uint16_t textColor = TFT_ORANGE;  // Default when isColorFull = false
    if (isColorFull) {
      if (strcmp(label, "CLT") == 0) {
        if (value > 110) {
          textColor = TFT_RED;
        } else if (value > 80) {
          textColor = TFT_YELLOW;
        } else {
          textColor = TFT_GREEN;
        }
      } else if (strcmp(label, "IAT") == 0) {
        if (value > 60) {
          textColor = TFT_RED;
        } else if (value > 40) {
          textColor = TFT_YELLOW;
        } else {
          textColor = TFT_GREEN;
        }
      } else if (strcmp(label, "TPS") == 0) {
        if (value > 100) {
          textColor = TFT_RED;
        } else if (value > 80) {
          textColor = TFT_YELLOW;
        } else {
          textColor = TFT_GREEN;
        }
      } else if (strcmp(label, "MAP") == 0) {
        if (value > 100) {
          textColor = TFT_RED;
        } else if (value > 80) {
          textColor = TFT_YELLOW;
        } else {
          textColor = TFT_GREEN;
        }
      } else if (strcmp(label, "ADV") == 0) {
        if (value < 0) {
          textColor = TFT_RED;
        } else {
          textColor = TFT_GREEN;
        }
      } else if (strcmp(label, "FP") == 0) {
        if (value < 28) {
          textColor = TFT_RED;
        } else if (value < 30) {
          textColor = TFT_YELLOW;
        } else {
          textColor = TFT_GREEN;
        }
      }
    }
    
    spr.setFreeFont(AA_FONT_FREE_MEDIUM);
    
    // Use smaller sprite width for panels at right edge (like ADV at x=256)
    int spriteWidth = 64;  
    spr.createSprite(spriteWidth, 38);
    
    spr.fillSprite(TFT_BLACK);  // Clear sprite background to prevent artifacts
    spr.setTextDatum(TC_DATUM);
    spr_width = spr.textWidth("9999");
    spr.setTextColor(textColor, TFT_BLACK, true);
    spr.drawNumber(value, spriteWidth/2, 3);
    spr.pushSprite(x, y + 35 - 15);
    spr.deleteSprite();
  }
}

void drawFloatPanel(int x, int y, const char *label, float value, uint16_t color, float lastValue, int decimals, bool setup) {
  if (lastValue != value || forceRefresh || setup || first_run) {
    lablDraw(x, y, label, 1);
    
    // Apply color logic based on isColorFull and label
    uint16_t textColor = TFT_ORANGE;  // Default when isColorFull = false
    if (isColorFull) {
      if (strcmp(label, "AFR") == 0) {
        if ((value > 15.2) && (value < 20)) {
          textColor = TFT_RED;
        } else if (value < 13) {
          textColor = TFT_YELLOW;
        } else {
          textColor = TFT_GREEN;
        }
      } else if (strcmp(label, "BAT") == 0) {
        if (value < 12.0) {
          textColor = TFT_RED;
        } else if (value < 12.5) {
          textColor = TFT_YELLOW;
        } else {
          textColor = TFT_GREEN;
        }
      }
    }
    
    spr.setFreeFont(AA_FONT_FREE_MEDIUM);
    spr.createSprite(70, 38);
    spr.fillSprite(TFT_BLACK);  // Clear sprite background to prevent artifacts
    spr.setTextDatum(TC_DATUM);
    spr_width = spr.textWidth("99.9");
    spr.setTextColor(textColor, TFT_BLACK, true);
    spr.drawFloat(value, decimals, 35, 5);  // This will correctly display 0.0
    spr.pushSprite(x, y + 35 - 15);
    spr.deleteSprite();
  }
}

// Keep legacy function for compatibility
void drawModularDataPanel(const DisplayPanel &panel, bool setup) {
  drawDynamicDataPanel(panel, setup);
}

void drawConfigurableIndicators() {
  // Draw indicators in single row below panels for 320x170 display
  // Bottom panels are at Y=90 with height ~70, so indicators at Y=150
  int indicatorY = 150;  // Below bottom row panels (90 + 70 = 160, so 150 for spacing)
  int buttonWidth = 50;  // Button width (matches BTN_WIDTH in drawing_utils.cpp)
  
  // Optimal layout calculation for 6 indicators in 320px width:
  // 6 buttons × 50px = 300px, remaining 20px for spacing
  // Use 3px margin + 5 gaps of 2px + 3px margin = 16px total spacing
  int marginLeft = 3;
  int buttonSpacing = 2;
  
  // Static arrays to track last state to prevent flicker
  static bool lastStates[6] = {false, false, false, false, false, false};
  static bool initialized = false;
  
  // Clear the entire indicator area once when first run or force refresh to remove disabled indicators
  if (!initialized || first_run || forceRefresh) {
    display.fillRect(0, indicatorY, 320, 20, TFT_BLACK);  // Clear entire indicator row
  }
  
  // Pack enabled indicators with proper spacing - limit to 6 indicators
  int currentPosition = 0;
  int maxIndicators = 6;  // Maximum 6 indicators to fit in display width
  
  for (int i = 0; i < currentDisplayConfig.activeIndicatorCount && currentPosition < maxIndicators; i++) {
    IndicatorConfig &indicator = currentDisplayConfig.indicators[i];
    
    if (indicator.enabled) {  // Only process enabled indicators
      bool state = getIndicatorValue(indicator.indicator);
      
      // Calculate X position: margin + (button_width + spacing) * position  
      int x = marginLeft + (buttonWidth + buttonSpacing) * currentPosition;
      
      // Only redraw if state changed or first run (with bounds check)
      if (currentPosition < 6 && (!initialized || lastStates[currentPosition] != state || first_run || forceRefresh)) {
        drawSmallButton(x, indicatorY, indicator.label, state);
        if (currentPosition < 6) {
          lastStates[currentPosition] = state;
        }
      }
      
      currentPosition++; // Increment position for next enabled indicator (packed left)
    }
  }
  
  initialized = true;
}

// Setup default panel layout for ST7789 320x170 display
void setupDefaultPanelLayout() {
  // Clear existing configuration
  currentDisplayConfig.activePanelCount = 0;
  currentDisplayConfig.activeIndicatorCount = 0;
  
  // Define 9 panels: 4 baris atas + 5 baris bawah
  // Top row (4 panels): CLT, IAT, AFR, BAT (positions 0-3)
  // Bottom row (5 panels): RPM, FP, TPS, MAP, ADV (positions 4-8)
  
  // Baris atas (4 panel)
  addDataPanel(0, "CLT", DATA_SOURCE_COOLANT, true, 0);      // Position 0: Top row
  addDataPanel(1, "IAT", DATA_SOURCE_IAT, true, 0);          // Position 1: Top row
  addDataPanel(2, "AFR", DATA_SOURCE_AFR, true, 1);          // Position 2: Top row  
  addDataPanel(3, "BAT", DATA_SOURCE_VOLTAGE, true, 1);      // Position 3: Top row
  
  // Baris bawah (5 panel)
  addDataPanel(4, "RPM", DATA_SOURCE_RPM, true, 0);          // Position 4: Bottom row
  addDataPanel(5, "FP", DATA_SOURCE_FP, true, 0);            // Position 5: Bottom row
  addDataPanel(6, "TPS", DATA_SOURCE_TPS, true, 0);          // Position 6: Bottom row
  addDataPanel(7, "MAP", DATA_SOURCE_MAP, true, 0);          // Position 7: Bottom row
  addDataPanel(8, "ADV", DATA_SOURCE_ADV, true, 0);          // Position 8: Bottom row
  
  // Add some default indicators (6 indicators)
  addIndicator(0, "SYNC", INDICATOR_SYNC, true);
  addIndicator(1, "FAN", INDICATOR_FAN, true);  
  addIndicator(2, "REV", INDICATOR_REV, true);
  addIndicator(3, "LCH", INDICATOR_LCH, true);
  addIndicator(4, "AC", INDICATOR_AC, true);
  addIndicator(5, "DFCO", INDICATOR_DFCO, true);
}

// Helper function to add a data panel
void addDataPanel(int position, const char* label, uint8_t dataSource, bool enabled, int decimals) {
  if (currentDisplayConfig.activePanelCount < 9) {  // Now supports 9 panels
    DisplayPanel& panel = currentDisplayConfig.panels[currentDisplayConfig.activePanelCount];
    panel.position = position;
    strncpy(panel.label, label, sizeof(panel.label) - 1);
    panel.label[sizeof(panel.label) - 1] = '\0';
    panel.dataSource = dataSource;
    panel.enabled = enabled;
    panel.decimals = decimals;
    panel.dataType = 0; // Default data type
    strncpy(panel.unit, "", sizeof(panel.unit) - 1);
    panel.color = TFT_WHITE;
    
    currentDisplayConfig.activePanelCount++;
  }
}

// Helper function to add an indicator
void addIndicator(int position, const char* label, uint8_t indicator, bool enabled) {
  if (currentDisplayConfig.activeIndicatorCount < 8) {  // Max 8 indicators
    IndicatorConfig& ind = currentDisplayConfig.indicators[currentDisplayConfig.activeIndicatorCount];
    ind.position = position;
    strncpy(ind.label, label, sizeof(ind.label) - 1);
    ind.label[sizeof(ind.label) - 1] = '\0';
    ind.indicator = indicator;
    ind.enabled = enabled;
    currentDisplayConfig.activeIndicatorCount++;
  }
}

// Dynamic configurable layout system for ST7789 320x170 display
void drawConfigurableData(bool setup) {
  // Static flag to ensure default layout is only set up once
  static bool defaultLayoutInitialized = false;
  
  // If no panels configured OR less than 9 panels, use default layout (but only once)
  if (!defaultLayoutInitialized && currentDisplayConfig.activePanelCount < 9) {
    setupDefaultPanelLayout();
    defaultLayoutInitialized = true;
  }
  
  // Draw configurable panels with optimized frequency
  static uint32_t lastPanelUpdate = 0;
  if (setup || (millis() - lastPanelUpdate > 50)) { // Update panels every 50ms max
    drawConfigurablePanels(setup);
    lastPanelUpdate = millis();
  }
  
  // Draw configurable indicators with optimized frequency  
  static uint32_t lastIndicatorUpdate = 0;
  if (setup || (millis() - lastIndicatorUpdate > 100)) { // Update indicators every 100ms max
    drawConfigurableIndicators();
    lastIndicatorUpdate = millis();
  }
}

void startUpDisplay() {
  display.fillScreen(TFT_BLACK);
  spr.setColorDepth(16);
  
  // Use configurable display system for initial display
  drawConfigurableData(true);
  
  // Reset first_run flag after initial display is complete
  first_run = false;
}

void lablDraw(int x, int y, const char *label, int type) {
  if (first_run || forceRefresh) {
    uint16_t textColor = TFT_CYAN;

    if (isColorFull) {
      textColor = TFT_WHITE;
    }
    if (type == 0) {
      spr.setFreeFont(AA_FONT_FREE_SMALL);
      spr.createSprite(50, 70);
      spr.setTextColor(textColor, TFT_BLACK, true);
      spr.setTextDatum(TC_DATUM);
      spr.drawString(label, 15, 5);
      spr.pushSprite(x + 10, y);
      spr.deleteSprite();
    } else {
      spr.setFreeFont(AA_FONT_FREE_SMALL);
      spr.createSprite(50, 70);
      spr.setTextDatum(TC_DATUM);
      spr.setTextColor(textColor, TFT_BLACK, true);
      spr.drawString(label, 15, 5);
      spr.pushSprite(x + 10, y);
      spr.deleteSprite();
    }
  }
}

void itemDrawDataUsignedInt(unsigned int value, unsigned int &lastValue, int x, int y, const char *label) {
  lablDraw(x, y, label, 0);

  if (lastValue != value || forceRefresh) {
    uint16_t textColor = TFT_ORANGE;
    if (isColorFull) {
      if (strcmp(label, "RPM") == 0) {
        if (value > 6000) {
          textColor = TFT_RED;
        } else if (value > 4000) {
          textColor = TFT_YELLOW;
        } else {
          textColor = TFT_GREEN;
        }
      }
    }

    spr.setFreeFont(AA_FONT_FREE_MEDIUM);
    spr.createSprite(90, 40);
    spr.setTextDatum(TR_DATUM);
    spr_width = spr.textWidth("9999");
    spr.setTextColor(textColor, TFT_BLACK, true);
    spr.drawNumber(value, 90, 5);
    spr.pushSprite(x - 10, y + 35 - 15);
    spr.deleteSprite();
    lastValue = value;
  }
}

void itemDrawDataInt(int value, int &lastValue, int x, int y, const char *label) {
  if (lastValue != value || forceRefresh) {
    uint16_t textColor = TFT_ORANGE;
    if (isColorFull) {
      if (strcmp(label, "CLT") == 0) {
        if (value > 110) {
          textColor = TFT_RED;
        } else if (value > 80) {
          textColor = TFT_YELLOW;
        } else {
          textColor = TFT_GREEN;
        }
      } else if (strcmp(label, "IAT") == 0) {
        if (value > 60) {
          textColor = TFT_RED;
        } else if (value > 40) {
          textColor = TFT_YELLOW;
        } else {
          textColor = TFT_GREEN;
        }
      } else if (strcmp(label, "TPS") == 0) {
        if (value > 100) {
          textColor = TFT_RED;
        } else if (value > 80) {
          textColor = TFT_YELLOW;
        } else {
          textColor = TFT_GREEN;
        }
      } else if (strcmp(label, "MAP") == 0) {
        if (value > 100) {
          textColor = TFT_RED;
        } else if (value > 80) {
          textColor = TFT_YELLOW;
        } else {
          textColor = TFT_GREEN;
        }
      } else if (strcmp(label, "ADV") == 0) {
        if (value < 0) {
          textColor = TFT_RED;
        } else {
          textColor = TFT_GREEN;
        }
      } else if (strcmp(label, "FP") == 0) {
        if (value < 28) {
          textColor = TFT_RED;
        } else if (value < 30) {
          textColor = TFT_YELLOW;
        } else {
          textColor = TFT_GREEN;
        }
      }
    }
    lablDraw(x, y, label, 0);
    spr.setFreeFont(AA_FONT_FREE_MEDIUM);
    spr.createSprite(50, 38);
    spr.setTextDatum(TC_DATUM);
    spr_width = spr.textWidth("0");
    spr.setTextColor(textColor, TFT_BLACK, true);
    spr.drawNumber(value, 30, 3);
    if (strcmp(label, "ADV") == 0) {
      spr.pushSprite(x, y + 35 - 15);
    } else {
      spr.pushSprite(x, y + 35 - 15);
    }
    spr.deleteSprite();
    lastValue = value;
  }
}

void itemDrawDataFloat(float value, float &lastValue, int x, int y, const char *label) {
  if (lastValue != value || forceRefresh) {
    uint16_t textColor = TFT_ORANGE;
    if (isColorFull) {
      if (strcmp(label, "AFR") == 0) {
        if ((value > 15.2) && (value < 20)) {
          textColor = TFT_RED;
        } else if (value < 13) {
          textColor = TFT_YELLOW;
        } else {
          textColor = TFT_GREEN;
        }
      } else if (strcmp(label, "BAT") == 0) {
        if (value < 12.0) {
          textColor = TFT_RED;
        } else if (value < 12.5) {
          textColor = TFT_YELLOW;
        } else {
          textColor = TFT_GREEN;
        }
      }
    }
    lablDraw(x, y, label, 1);
    spr.setFreeFont(AA_FONT_FREE_MEDIUM);
    spr.createSprite(70, 38);
    spr.setTextDatum(TC_DATUM);
    spr_width = spr.textWidth("99.9");
    spr.setTextColor(textColor, TFT_BLACK, true);
    spr.drawFloat(value, 1, 30, 5);
    spr.pushSprite(x, y + 35 - 15);
    spr.deleteSprite();
    lastValue = value;
  }
}

void itemDraw(bool setup) {
  // Use dynamic configurable panel system instead of hardcoded layout
  drawConfigurableData(setup);
}

void drawDataBox(int x, int y, const char *label, const float value, uint16_t labelColor, const float valueToCompare, const int decimal, bool setup) {
  const int BOX_WIDTH = 75;  // Adjusted for 320x170 display (320/4 columns = 80, minus margin = 75)
  const int BOX_HEIGHT = 70;  // Adjusted for 2 rows in 170 height
  const int LABEL_HEIGHT = BOX_HEIGHT / 2;

  if (setup) {
    // Clear the entire data box area first only during setup
    display.fillRect(x, y, BOX_WIDTH, BOX_HEIGHT, TFT_BLACK);
    
    spr.setFreeFont(AA_FONT_FREE_SMALL);
    spr.createSprite(BOX_WIDTH, LABEL_HEIGHT);
    spr.fillSprite(TFT_BLACK);  // Clear sprite background
    spr.setTextColor(labelColor, TFT_BLACK, true);
    spr.setTextDatum(TC_DATUM);
    spr.drawString(label, BOX_WIDTH/2, 5);
    if (label == "AFR") {
      spr.pushSprite(x - 10, y);
    } else {
      spr.pushSprite(x, y);
    }
    spr.deleteSprite();
  }
  
  if (setup || valueToCompare != value) {
    spr.setFreeFont(AA_FONT_FREE_MEDIUM);
    spr.createSprite(BOX_WIDTH, LABEL_HEIGHT);
    spr.fillSprite(TFT_BLACK);  // Clear sprite background
    spr.setTextDatum(TC_DATUM);
    spr_width = spr.textWidth("333");
    spr.setTextColor(labelColor, TFT_BLACK, true);
    if (decimal > 0) {
      spr.drawFloat(value, decimal, BOX_WIDTH/2, 5);
    } else {
      spr.drawNumber(value, BOX_WIDTH/2, 5);
    }
    spr.pushSprite(x, y + LABEL_HEIGHT - 15);
    spr.deleteSprite();
  }
}

void drawData() {

  itemDraw(false);
  
  // Reset forceRefresh after first update
  if (forceRefresh) {
    forceRefresh = false;
    Serial.println("[DISPLAY] Force refresh completed");
  }
  
#if ENABLE_SIMULATOR
  // Draw simulator indicator if simulator is active (with reduced update frequency)
  static uint8_t lastSimMode = SIMULATOR_MODE_OFF;
  static uint32_t lastSimUpdate = 0;
  uint8_t currentSimMode = getSimulatorMode();
  
  // Only redraw if simulator mode has changed or every 500ms
  if (currentSimMode != lastSimMode || (millis() - lastSimUpdate > 500)) {
    if (currentSimMode != SIMULATOR_MODE_OFF) {
      // Clear the SIM area first
      display.fillRect(display.width() - 30, 5, 25, 15, TFT_BLACK);
      
      // Draw SIM indicator
      display.setFreeFont(AA_FONT_FREE_SMALL);
      display.setTextColor(TFT_YELLOW, TFT_BLACK);
      display.setTextDatum(TR_DATUM);
      display.drawString("SIM", display.width() - 5, 5);
    } else {
      // Clear the SIM indicator when simulator is turned off
      display.fillRect(display.width() - 30, 5, 25, 15, TFT_BLACK);
    }
    
    lastSimMode = currentSimMode;
    lastSimUpdate = millis();
  }
#endif

  // // Draw communication mode indicator (top left) with reduced update frequency
  // static bool lastCommMode = true;  // Track changes
  // static String lastCommText = "";
  // static uint32_t lastCommUpdate = 0;
  
  // String currentCommText = isCANMode ? "CAN" : "SER";
  
  // // Only redraw if communication mode has changed or every 1000ms
  // if (isCANMode != lastCommMode || currentCommText != lastCommText || (millis() - lastCommUpdate > 1000)) {
  //   // Clear the comm mode area first
  //   display.fillRect(5, 5, 40, 15, TFT_BLACK);
    
  //   // Draw new communication mode
  //   display.setFreeFont(AA_FONT_FREE_SMALL);
  //   display.setTextColor(isCANMode ? TFT_GREEN : TFT_ORANGE, TFT_BLACK);
  //   display.setTextDatum(TL_DATUM);
  //   display.drawString(currentCommText, 5, 5);
    
  //   lastCommMode = isCANMode;
  //   lastCommText = currentCommText;
  //   lastCommUpdate = millis();
  // }

#if ENABLE_DEBUG_MODE
  static bool lastDebugMode = false;
  static String lastDebugInfo = "";
  
  if (debugMode) {
    // Create debug info string - show only essential info in one line
    String debugInfo = "CPU:" + String(cpuUsage, 1) + "% FPS:" + String(fps, 1) + " Heap:" + String(ESP.getFreeHeap()/1024) + "K";
    
    if (debugInfo != lastDebugInfo || !lastDebugMode) {
      int centerX = display.width() / 2;
      
      // Clear the debug area first to prevent font overlap
      display.fillRect(centerX - 120, 5, 240, 20, TFT_BLACK);
      
      // Draw debug info
      display.setFreeFont(AA_FONT_FREE_SMALL);
      display.setTextColor(TFT_CYAN, TFT_BLACK);
      display.setTextDatum(TC_DATUM);
      display.drawString(debugInfo, centerX, 5);
      
      lastDebugInfo = debugInfo;
    }
    
    lastDebugMode = true;
  } else {
    // Clear debug area when debug mode is turned off
    if (lastDebugMode) {
      // Clear the top center area where debug info was displayed
      display.fillRect(0, 5, display.width(), 20, TFT_BLACK);
      lastDebugMode = false;
      lastDebugInfo = "";
    }
  }
#endif
}
