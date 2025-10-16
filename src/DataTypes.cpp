#include "DataTypes.h"
#include <WebServer.h>

// Temperature variables
uint8_t iat = 0, clt = 0;
uint8_t refreshRate = 0;
unsigned int rpm = 0, lastRpm = 0, vss = 0;
int mapData = 0, tps = 0, adv = 0, fp = 0, triggerError = 0;
float bat = 0.0, afrConv = 0.0;
bool syncStatus = false, fan = false, ase = false, wue = false, rev = false, launch = false, airCon = false, dfco = false;

// Last values for comparison
int lastIat = -1, lastClt = -1, lastTps = -1, lastAdv = -1, lastMapData = -1, lastFp = -1, lastTriggerError = -1;
float lastBat = -1, lastAfrConv = -1;
unsigned int lastRefreshRate = 0;

// System variables
bool first_run = true;
bool forceRefresh = false;
uint32_t lastPrintTime = 0;
uint32_t startupTime = 0;
uint16_t spr_width = 0;

// Communication variables
int commMode = 0;
bool sent = false, received = true;
bool isCANMode = true;

// WiFi variables
bool wifiActive = true;
uint32_t lastClientCheck = 0;
uint32_t lastClientCheckTimeout = 0;
uint32_t wifiTimeout = 30000;
bool clientConnected = true;

// Debug variables
bool debugMode = false;
float cpuUsage = 0.0;
float fps = 0.0;
uint32_t frameCount = 0;
uint32_t lastFpsUpdate = 0;
uint32_t lastCpuMeasure = 0;
uint32_t loopStartTime = 0;

// Time variables
uint32_t lazyUpdateTime = 0;

// WebServer
WebServer server(80);

// Configuration variables
const char *version = "1.2.1";
const char *ssid = "MAZDUINO_Display";
const char *password = "12345678";