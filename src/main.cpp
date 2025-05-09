#include <WiFi.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <Update.h>
#include <EEPROM.h>
#include <cstring>
#include "Arduino.h"
#include "SPI.h"
#include <TFT_eSPI.h>
#include "NotoSansBold15.h"
#include "NotoSansBold36.h"
#include "main.h"
#include "splash.h"

#define AA_FONT_SMALL NotoSansBold15
#define AA_FONT_LARGE NotoSansBold36
#include "NotoSans_Bold6pt7b.h"
#include "NotoSans_Bold16pt7b.h"
#define AA_FONT_FREE_SMALL &NotoSans_Bold6pt7b
#define AA_FONT_FREE_MEDIUM &NotoSans_Bold16pt7b

const char *version = "0.1.1";

const char *ssid = "MAZDUINO_Display";
const char *password = "12345678";
IPAddress ip(192, 168, 1, 80);
IPAddress netmask(255, 255, 255, 0);

WebServer server(80);

TFT_eSPI display = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&display);

#include "Comms.h"
#include "text_utils.h"
#include "drawing_utils.h"
#include <esp32_can.h>

#define UART_BAUD 115200

#if defined(ESP32C3)
#define RXD 9
#define TXD 10
#else
#define RXD 16
#define TXD 17
#endif

#define COMM_CAN 0
#define COMM_SERIAL 1

#define EEPROM_SIZE 512

bool isColorFull = false;

bool forceRefresh = false;
bool dataReceived = false;
int commMode = COMM_CAN;

uint8_t iat = 20, clt = 30;
uint8_t refreshRate = 0;
unsigned int rpm = 0, lastRpm = 1, vss = 0;
int mapData = 0, tps = 0, adv = 0, fp = 0;
float bat = 12.1, afr = 14.7;
bool syncStatus, fan, ase, wue, rev, launch, ac, dfco;

int lastIat = -1, lastClt = -1, lastTps = -1, lastAdv = -1, lastMapData = -1, lastFp = -1;
float lastBat = -1, lastAfr = -1;
unsigned int lastRefreshRate = -1;
bool first_run = true;
uint32_t lazyUpdateTime;
uint32_t demoRefreshTime;
uint32_t demoRefreshSlowTime;
bool demoaccel = false;
uint16_t spr_width = 0;
uint32_t lastPrintTime = 0;

void runDemo()
{
	if (millis() - demoRefreshTime > 50)
	{
		afr = float(random(100, 200)) / 10;
		rpm = random(500, 2000);
		mapData = random(90, 100);
		tps = random(0, 110);
		adv = random(-15, 20);

		demoRefreshTime = millis();
	}

	if (millis() - demoRefreshSlowTime > 500)
	{
		clt = random(70, 120);
		iat = random(30, 120);
		fp = random(28, 35);
		bat = float(random(111, 142)) / 10;
		demoRefreshSlowTime = millis();
	}
}

void setup()
{
	pinMode(22, INPUT_PULLUP);

	Serial.begin(UART_BAUD);
	Serial.setTimeout(1000);
	delay(1000);

	EEPROM.begin(EEPROM_SIZE);

	isColorFull = EEPROM.read(0);
	commMode = EEPROM.read(1);
	if (commMode == COMM_CAN)
	{

		CAN0.setCANPins(GPIO_NUM_17, GPIO_NUM_16); // RX, TX
		CAN0.begin(500000);						   // 500Kbps
		CAN0.watchFor(0x360);					   // RPM, MAP, TPS
		// CAN0.watchFor(0x361); // Fuel Pressure
		CAN0.watchFor(0x368); // AFR 01
		// CAN0.watchFor(0x370); // VSS
		CAN0.watchFor(0x372); // Voltage
		CAN0.watchFor(0x3E0); // CLT, IAT
		Serial.println("CAN mode aktif.");
	}
	else
	{
		Serial1.begin(UART_BAUD, SERIAL_8N1, RXD, TXD);
		Serial.println("Serial mode aktif.");
	}
	display.init();
	display.setRotation(1);
	drawSplashScreenWithImage();
	display.fillScreen(TFT_BLACK);
  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAPConfig(ip, ip, netmask);
  WiFi.softAP(ssid, password);

  server.on("/", HTTP_GET, handleRoot);
  server.on(
      "/update", HTTP_POST, []()
      {
      server.send(200, "text/plain", (Update.hasError()) ? "Gagal update!" : "Update berhasil! MAZDUINO Display akan restart.");
      delay(1000);
      ESP.restart(); },
      handleUpdate);
  server.on("/toggle", HTTP_POST, handleToggle); // Endpoint untuk toggle
  server.on("/setMode", HTTP_POST, []()
            {
              String mode = server.arg("mode");
              if (mode == "serial")
              {
                commMode = COMM_SERIAL;
              }
              else if (mode == "can")
              {
                commMode = COMM_CAN;
              }
              EEPROM.write(1, commMode);
              EEPROM.commit();
              server.send(200, "text/plain", "Mode updated");
              ESP.restart(); // Restart untuk menerapkan perubahan
            });
  server.begin();
  Serial.println("Web server aktif.");

}

void loop()
{
	isColorFull = EEPROM.read(0);

	if (commMode == COMM_CAN)
	{
		handleCANCommunication();
	}
	else
	{
		handleSerialCommunication();
	}

	// runDemo();
	drawData();
	first_run = false;
	int pinState = digitalRead(22);
	if (pinState == LOW)
	{
		// Button is pressed
		// Do something, e.g., toggle a flag or call a function
		EEPROM.write(0, !isColorFull);
		EEPROM.commit();
		forceRefresh = true;
	}
	else
	{
		forceRefresh = false;
	}
}

void drawSplashScreenWithImage()
{
	display.fillScreen(TFT_BLACK);
	display.loadFont(AA_FONT_LARGE);
	display.setTextColor(TFT_WHITE, TFT_BLACK);
	display.setTextDatum(TC_DATUM);
	int centerX = display.width() / 2;
	int centerY = (display.height() / 2) - 35;
	display.loadFont(AA_FONT_SMALL);
	display.drawString("Made for", centerX - 20, centerY - 8);
	// display.loadFont(AA_FONT_LARGE);
	// display.drawString("Speeduino", centerX, centerY);
	display.drawBitmap(centerX / 2, centerY, splash_logo, 180, 57, TFT_WHITE);
	display.loadFont(AA_FONT_SMALL);
	display.drawString("Firmware v" + String(version), centerX, 170 - 35);
	display.drawString("Powered by " + String(ESP.getChipModel()) + " Rev" + String(ESP.getChipRevision()), centerX, 170 - 15);

	delay(1000);
}

void handleCANCommunication()
{
	static uint32_t lastRefresh = millis();
	uint32_t elapsed = millis() - lastRefresh;
	refreshRate = (elapsed > 0) ? (1000 / elapsed) : 0;
	lastRefresh = millis();
	unsigned long currentTime = millis();
	if (CAN0.available())
	{ // Periksa apakah ada pesan di buffer
		CAN_FRAME can_message;
		if (CAN0.read(can_message))
		{
			// Serial.print("ID: ");
			// Serial.print(can_message.id, HEX);
			// Serial.print(" Data: ");
			// for (int i = 0; i < can_message.length; i++)
			// {
			//   Serial.print(can_message.data.byte[i], HEX);
			//   Serial.print(" ");
			// }
			// Serial.println();

			// Proses data berdasarkan ID
			switch (can_message.id)
			{
			case 0x360:
			{																				   // RPM, MAP, TPS
				rpm = (can_message.data.byte[0] << 8) | can_message.data.byte[1];			   // Byte 0-1
				uint16_t map = (can_message.data.byte[2] << 8) | can_message.data.byte[3];	   // Byte 2-3
				uint16_t tps_raw = (can_message.data.byte[4] << 8) | can_message.data.byte[5]; // Byte 4-5
				mapData = map / 10.0;														   // Konversi ke kPa
				tps = tps_raw / 10.0;														   // Konversi ke kPa
				break;
			}
			case 0x361:
			{																						 // Fuel Pressure
				uint16_t fuel_pressure = (can_message.data.byte[0] << 8) | can_message.data.byte[1]; // Byte 0-1
				fp = fuel_pressure / 10 - 101.3;
				break;
			}
			case 0x368:
			{																				   // AFR 01
				uint16_t afr_raw = (can_message.data.byte[0] << 8) | can_message.data.byte[1]; // Byte 0-1
				float lambda = afr_raw / 1000.0;
				afr = lambda * 14.7;
				break;
			}
			case 0x370:
			{																				   // VSS
				uint16_t vss_raw = (can_message.data.byte[0] << 8) | can_message.data.byte[1]; // Byte 0-1
				vss = vss_raw / 10.0;														   // Konversi ke km/h
				break;
			}
			case 0x372:
			{																				   // Voltage
				uint16_t voltage = (can_message.data.byte[0] << 8) | can_message.data.byte[1]; // Byte 0-1
				bat = voltage / 10.0;														   // Konversi ke volt
				break;
			}
			case 0x3E0:
			{																				   // CLT
				uint16_t clt_raw = (can_message.data.byte[0] << 8) | can_message.data.byte[1]; // Byte 0-1
				uint16_t iat_raw = (can_message.data.byte[2] << 8) | can_message.data.byte[3]; // Byte 0-1
				float clt_k = clt_raw / 10.0;
				float iat_k = iat_raw / 10.0;

				clt = clt_k - 273.15;
				iat = iat_k - 273.15;
				break;
			}
			default:
				break;
			}
		}
		else
		{
			Serial.println("Error reading CAN message.");
		}
	}

	if (currentTime - lastPrintTime >= 1000)
	{ // Interval 1000ms
		Serial.print("RPM: ");
		Serial.print(rpm);
		Serial.print(" MAP: ");
		Serial.print(mapData);
		Serial.print(" kPa TPS: ");
		Serial.print(tps);
		Serial.print(" % Fuel Pressure: ");
		Serial.print(fp);
		Serial.print(" kPa AFR: ");
		Serial.print(afr, 2);
		Serial.print(" VSS: ");
		Serial.print(vss);
		Serial.print(" km/h Voltage: ");
		Serial.print(bat, 2);
		Serial.print(" V CLT: ");
		Serial.print(clt);
		Serial.print(" °C IAT: ");
		Serial.print(iat);
		Serial.println(" °C");

		lastPrintTime = currentTime;
	}
}

void handleSerialCommunication()
{
	static uint32_t lastUpdate = millis();
	if (millis() - lastUpdate > 10)
	{
		dataReceived = requestData(50);
		if (!dataReceived)
		{
			// Set semua nilai ke 0
			rpm = 0;
			clt = 0;
			iat = 0;
			mapData = 0;
			tps = 0;
			adv = 0;
			fp = 0;
			afr = 0.0;
			bat = 0.0;
			refreshRate = 0;
		}
		lastUpdate = millis();
	}

	static uint32_t lastRefresh = millis();
	uint32_t elapsed = millis() - lastRefresh;
	refreshRate = (elapsed > 0) ? (1000 / elapsed) : 0;
	lastRefresh = millis();
	unsigned long currentTime = millis();
	if (dataReceived)
	{
		if (lastRefresh - lazyUpdateTime > 100 || rpm < 100)
		{
			clt = getByte(7) - 40;
			iat = getByte(6) - 40;
			bat = float(getByte(9) * 0.10);
		}
		rpm = getWord(14);
		mapData = getWord(4);
		afr = float(getByte(10) * 0.10);
		tps = getByte(24) * 0.5;
		adv = getByte(23);
		fp = getByte(103);

		syncStatus = getBit(31, 7);
		ase = getBit(2, 2);
		wue = getBit(2, 3);
		rev = getBit(31, 2);
		launch = getBit(31, 0);
		ac = getByte(122);
		fan = getBit(106, 3);
		dfco = getBit(1, 4);
	}

	if (currentTime - lastPrintTime > 1000)
	{
		lastPrintTime = currentTime;
		Serial.printf("RPM: %d, CLT: %d, IAT: %d, MAP: %d, TPS: %d, ADV: %d, FP: %d, AFR: %.1f, BAT: %.1f, isColorFull: %s, Last Update: %lu ms\n",
					  rpm, clt, iat, mapData, tps, adv, fp, afr, bat, isColorFull ? "true" : "false", lastUpdate);
	}
}

void drawData()
{
	itemDraw(false);
}

void lablDraw(int x, int y, const char *label, int type)
{
	if (first_run || forceRefresh)
	{
		uint16_t textColor = TFT_CYAN;

		if (isColorFull)
		{
			textColor = TFT_WHITE;
		}
		if (type == 0)
		{
			spr.setFreeFont(AA_FONT_FREE_SMALL);
			// spr.loadFont(AA_FONT_SMALL);
			spr.createSprite(50, 70);
			spr.setTextColor(textColor, TFT_BLACK, true);
			spr.setTextDatum(TC_DATUM);
			spr.drawString(label, 15, 5);
			spr.pushSprite(x + 10, y);
			spr.deleteSprite();
		}
		else
		{
			spr.setFreeFont(AA_FONT_FREE_SMALL);
			// spr.loadFont(AA_FONT_SMALL);
			spr.createSprite(50, 70);
			spr.setTextDatum(TC_DATUM);
			spr.setTextColor(textColor, TFT_BLACK, true);
			spr.drawString(label, 15, 5);
			spr.pushSprite(x + 10, y);
			spr.deleteSprite();
		}
	}
}

void itemDrawDataUsignedInt(unsigned int value, unsigned int &lastValue, int x, int y, const char *label)
{
	lablDraw(x, y, label, 0);

	if (lastValue != value || forceRefresh)
	{
		uint16_t textColor = TFT_ORANGE;
		if (isColorFull)
		{
			if (strcmp(label, "RPM") == 0)
			{
				if (value > 6000)
				{
					textColor = TFT_RED;
				}
				else if (value > 4000)
				{
					textColor = TFT_YELLOW;
				}
				else
				{
					textColor = TFT_GREEN;
				}
			}
		}

		// spr.loadFont(AA_FONT_LARGE);
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

void itemDrawDataInt(int value, int &lastValue, int x, int y, const char *label)
{
	if (lastValue != value || forceRefresh)
	{
		uint16_t textColor = TFT_ORANGE;
		if (isColorFull)
		{
			if (strcmp(label, "CLT") == 0)
			{
				if (value > 110)
				{
					textColor = TFT_RED;
				}
				else if (value > 80)
				{
					textColor = TFT_YELLOW;
				}
				else
				{
					textColor = TFT_GREEN;
				}
			}
			else if (strcmp(label, "IAT") == 0)
			{
				if (value > 60)
				{
					textColor = TFT_RED;
				}
				else if (value > 40)
				{
					textColor = TFT_YELLOW;
				}
				else
				{
					textColor = TFT_GREEN;
				}
			}
			else if (strcmp(label, "TPS") == 0)
			{
				if (value > 100)
				{
					textColor = TFT_RED;
				}
				else if (value > 80)
				{
					textColor = TFT_YELLOW;
				}
				else
				{
					textColor = TFT_GREEN;
				}
			}
			else if (strcmp(label, "MAP") == 0)
			{
				if (value > 100)
				{
					textColor = TFT_RED;
				}
				else if (value > 80)
				{
					textColor = TFT_YELLOW;
				}
				else
				{
					textColor = TFT_GREEN;
				}
			}
			else if (strcmp(label, "ADV") == 0)
			{
				if (value < 0)
				{
					textColor = TFT_RED;
				}
				else
				{
					textColor = TFT_GREEN;
				}
			}
			else if (strcmp(label, "FP") == 0)
			{
				if (value < 28)
				{
					textColor = TFT_RED;
				}
				else if (value < 30)
				{
					textColor = TFT_YELLOW;
				}
				else
				{
					textColor = TFT_GREEN;
				}
			}
		}
		lablDraw(x, y, label, 0);
		// spr.loadFont(AA_FONT_LARGE);
		spr.setFreeFont(AA_FONT_FREE_MEDIUM);
		spr.createSprite(50, 38);
		spr.setTextDatum(TC_DATUM);
		spr_width = spr.textWidth("0");
		spr.setTextColor(textColor, TFT_BLACK, true);
		spr.drawNumber(value, 30, 3);
		if (strcmp(label, "ADV") == 0)
		{
			spr.pushSprite(x, y + 35 - 15);
		}
		else
		{
			spr.pushSprite(x, y + 35 - 15);
		}
		spr.deleteSprite();
		lastValue = value;
	}
}
void itemDrawDataFloat(float value, float &lastValue, int x, int y, const char *label)
{
	if (lastValue != value || forceRefresh)
	{
		uint16_t textColor = TFT_ORANGE;
		if (isColorFull)
		{
			if (strcmp(label, "AFR") == 0)
			{
				if ((value > 15.2) && (value < 20))
				{
					textColor = TFT_RED;
				}
				else if (value < 13)
				{
					textColor = TFT_YELLOW;
				}
				else
				{
					textColor = TFT_GREEN;
				}
			}
			else if (strcmp(label, "BAT") == 0)
			{
				if (value < 12.0)
				{
					textColor = TFT_RED;
				}
				else if (value < 12.5)
				{
					textColor = TFT_YELLOW;
				}
				else
				{
					textColor = TFT_GREEN;
				}
			}
		}
		lablDraw(x, y, label, 1);
		// spr.loadFont(AA_FONT_LARGE);
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

void itemDraw(bool setup)
{
	itemDrawDataUsignedInt(rpm, lastRpm, 0, 90, "RPM");
	itemDrawDataInt(clt, lastClt, 0, 10, "CLT");
	itemDrawDataInt(iat, lastIat, 75, 10, "IAT");
	itemDrawDataFloat(afr, lastAfr, 165, 10, "AFR");
	itemDrawDataFloat(bat, lastBat, 250, 10, "BAT");
	itemDrawDataInt(fp, lastFp, 95, 90, "FP");
	itemDrawDataInt(tps, lastTps, 150, 90, "TPS");
	itemDrawDataInt(mapData, lastMapData, 205, 90, "MAP");
	itemDrawDataInt(adv, lastAdv, 265, 90, "ADV");
}


const char *uploadPage PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <title>MAZDUINO Display OTA Update</title>
    <style>
      body {
        display: flex;
        justify-content: center;
        align-items: center;
        flex-direction: column;
        height: 100vh;
        background-color: black;
        color: white;
      }
      .btn {
        padding: 10px 20px;
        font-size: 16px;
        background-color: #007BFF;
        color: white;
        border: none;
        border-radius: 5px;
        cursor: pointer;
      }
      .toggle-btn.off {
        background-color: #FF0000;
      }
    </style>
    <script>
      function toggleState(button) {
        if (button.classList.contains('off')) {
          button.classList.remove('off');
          button.textContent = "Display 2";
          // Kirim status ON ke server
          fetch('/toggle', { method: 'POST', body: 'on' });
        } else {
          button.classList.add('off');
          button.textContent = "Display 1";
          // Kirim status OFF ke server
          fetch('/toggle', { method: 'POST', body: 'off' });
        }
      }
    </script>
  </head>
  <body>
    <h1>MAZDUINO Display OTA Update</h1>
    <form method="POST" action="/update" enctype="multipart/form-data">
      <input type="file" name="firmware">
      <input type="submit" class="btn" value="Upload">
    </form>
    <hr>
    <h2>Display Control</h2>
    <p>Sabar ya nanti diupdate :) </p>
    <!-- <button class="btn toggle-btn" onclick="toggleState(this)">Display 1</button> -->

  </body>
</html>
)rawliteral";

void handleRoot()
{
  server.send(200, "text/html", uploadPage);
}

void handleUpdate()
{
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START)
  {
    Serial.printf("Memulai update: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN))
    {
      Update.printError(Serial);
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize)
    {
      Update.printError(Serial);
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (Update.end(true))
    {
      // Serial.printf("Update selesai: %u bytes\n", upload.totalSize);
    }
    else
    {
      Update.printError(Serial);
    }
  }
}

void handleToggle()
{
  if (server.method() == HTTP_POST)
  {
    bool toggleState = EEPROM.read(0) || false;
    String body = server.arg("plain"); // Ambil isi body POST
    if (body == "on")
    {
      toggleState = 1;
      Serial.println("Toggle: ON");
    }
    else if (body == "off")
    {
      toggleState = 0;
      Serial.println("Toggle: OFF");
    }
    EEPROM.write(0, toggleState);
    EEPROM.commit();

    server.send(200, "text/plain", "OK");
    delay(1000);
    ESP.restart();
  }
}