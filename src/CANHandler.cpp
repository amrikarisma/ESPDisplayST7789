#include "CANHandler.h"
#include "DisplayConfig.h"
#include "Config.h"
#include "DataTypes.h"
#include <esp32_can.h>
#include "Arduino.h"

void setupCAN() {
  Serial.println("[CAN] Starting CAN initialization...");

  // Set CAN pins - GPIO 17 (RX) and GPIO 16 (TX)
  CAN0.setCANPins(GPIO_NUM_17, GPIO_NUM_16); // RX, TX (fixed order)
  Serial.printf("[CAN] CAN pins set: RX=GPIO17, TX=GPIO16\n");

  uint32_t canSpeed = getCanSpeed();
  Serial.printf("[CAN] Attempting to start CAN at %u bps\n", canSpeed);
  
  // Try to initialize CAN with configured speed
  bool canInitialized = false;
  if (CAN0.begin(canSpeed)) {
    Serial.printf("[CAN] ✓ CAN initialization SUCCESS at %u bps\n", canSpeed);
    canInitialized = true;
  } else {
    Serial.printf("[CAN] ✗ CAN initialization FAILED at %u bps\n", canSpeed);
    
    // Try common Speeduino speeds
    uint32_t speedsToTry[] = {1000000, 500000, 250000, 125000};
    for (int i = 0; i < 4; i++) {
      if (speedsToTry[i] == canSpeed) continue; // Skip already tried speed
      Serial.printf("[CAN] Trying %u bps...\n", speedsToTry[i]);
      if (CAN0.begin(speedsToTry[i])) {
        Serial.printf("[CAN] ✓ CAN started successfully at %u bps\n", speedsToTry[i]);
        setCanSpeed(speedsToTry[i]);  // Save working speed
        canInitialized = true;
        break;
      }
    }
    
    if (!canInitialized) {
      Serial.println("[CAN] ✗ CAN initialization failed at all speeds!");
      Serial.println("[CAN] Check hardware: CAN transceiver, wiring, termination resistors");
      return;
    }
  }
  
  // Set up CAN message filters
  CAN0.watchFor(0x360);  // RPM, MAP, TPS
  CAN0.watchFor(0x361);  // Fuel Pressure
  CAN0.watchFor(0x362);  // Ignition Angle (Leading)
  CAN0.watchFor(0x368);  // AFR 01
  CAN0.watchFor(0x369);  // Trigger System Error Count
  CAN0.watchFor(0x370);  // VSS
  CAN0.watchFor(0x372);  // Voltage
  CAN0.watchFor(0x3E0);  // CLT, IAT
  CAN0.watchFor(0x3E4);  // Indicator
  
  Serial.println("[CAN] CAN message filters configured");
  Serial.println("[CAN] Watching for CAN IDs: 0x360, 0x361, 0x362, 0x368, 0x369, 0x370, 0x372, 0x3E0, 0x3E4");

  isCANMode = true;
  Serial.println("[CAN] CAN setup completed successfully!");
  
  // Send test message to verify CAN is working
  CAN_FRAME testFrame;
  testFrame.id = 0x123;
  testFrame.length = 8;
  for (int i = 0; i < 8; i++) {
    testFrame.data.byte[i] = i;
  }
  
  if (CAN0.sendFrame(testFrame)) {
    Serial.println("[CAN] ✓ Test message sent successfully");
  } else {
    Serial.println("[CAN] ✗ Failed to send test message - check CAN transceiver!");
  }
}

void canTask(void *pvParameters) {
  while (1) {
    handleCANCommunication();
    vTaskDelay(1);
  }
}

void handleCANCommunication() {
  static uint32_t lastRefresh = millis();
  static uint32_t messageCount = 0;
  static unsigned long lastDebugPrint = 0;
  static unsigned long lastHealthCheck = 0;
  
  uint32_t elapsed = millis() - lastRefresh;
  refreshRate = (elapsed > 0) ? (1000 / elapsed) : 0;
  lastRefresh = millis();
  unsigned long currentTime = millis();
  
  isCANMode = true;  // We're in CAN mode when this function is called
  
  if (CAN0.available()) {
    messageCount++;
    CAN_FRAME can_message;
    if (CAN0.read(can_message)) {
      // Reduced debug output - only print every 100 messages or for specific debug
      if (messageCount % 100 == 0) {
        Serial.printf("[CAN] Msg #%u - ID:0x%03X, Len:%d\n", messageCount, can_message.id, can_message.length);
      }
      
      // Process data based on ID
      switch (can_message.id) {
        case 0x360: {
          rpm = (can_message.data.byte[0] << 8) | can_message.data.byte[1];
          uint16_t map = (can_message.data.byte[2] << 8) | can_message.data.byte[3];
          uint16_t tps_raw = (can_message.data.byte[4] << 8) | can_message.data.byte[5];
          mapData = map / 10.0;
          tps = tps_raw / 10.0;
          
          // Debug important data every 50 messages
          if (messageCount % 50 == 0) {
            Serial.printf("[CAN] 0x360: RPM=%u, MAP=%d, TPS=%d\n", rpm, mapData, tps);
          }
          break;
        }
        case 0x361: {
          uint16_t fuel_pressure = (can_message.data.byte[0] << 8) | can_message.data.byte[1];
          // uint16_t oil_pressure = (can_message.data.byte[2] << 8) | can_message.data.byte[3];
          fp = fuel_pressure / 10 - 101.3;
          break;
        }
        case 0x368: {
          uint16_t afr_raw = (can_message.data.byte[0] << 8) | can_message.data.byte[1];
          float lambda = afr_raw / 1000.0;
          afrConv = lambda * 14.7;
          
          // Debug AFR data occasionally
          if (messageCount % 100 == 0) {
            Serial.printf("[CAN] 0x368: AFR=%.1f\n", afrConv);
          }
          break;
        }
        case 0x369: {
          uint16_t trigger_raw = (can_message.data.byte[0] << 8) | can_message.data.byte[1];
          triggerError = trigger_raw;
          break;
        }
        case 0x370: {
          uint16_t vss_raw = (can_message.data.byte[0] << 8) | can_message.data.byte[1];
          vss = vss_raw / 10.0;
          break;
        }
        case 0x372: {
          uint16_t voltage = (can_message.data.byte[0] << 8) | can_message.data.byte[1];
          bat = voltage / 10.0;
          
          // Debug voltage data occasionally
          if (messageCount % 100 == 0) {
            Serial.printf("[CAN] 0x372: BAT=%.1fV\n", bat);
          }
          break;
        }
        case 0x3E0: {
          uint16_t clt_raw = (can_message.data.byte[0] << 8) | can_message.data.byte[1];
          uint16_t iat_raw = (can_message.data.byte[2] << 8) | can_message.data.byte[3];
          float clt_k = clt_raw / 10.0;
          float iat_k = iat_raw / 10.0;
          clt = clt_k - 273.15;
          iat = iat_k - 273.15;
          
          // Debug temperature data occasionally
          if (messageCount % 100 == 0) {
            Serial.printf("[CAN] 0x3E0: CLT=%d°C, IAT=%d°C\n", (int)clt, (int)iat);
          }
          break;
        }
        case 0x3E4: {
          // Indicators/Status bits - Haltech CAN protocol format
          // Based on Haltech CAN Protocol documentation
          
          uint8_t byte1 = can_message.data.byte[1];
          uint8_t byte2 = can_message.data.byte[2]; 
          uint8_t byte3 = can_message.data.byte[3];
          uint8_t byte7 = can_message.data.byte[7];
          
          // Byte 1 status bits
          // 1:4 = Decel Cut Active, 1:2 = Brake Pedal Switch, 1:1 = Clutch Switch
          dfco = (byte1 & 0x10) ? true : false;           // Byte 1, bit 4 - Decel Cut Active
          
          // Byte 2 status bits  
          // 2:7 = Launch Control Active, 2:1 = Torque Reduction Active
          launch = (byte2 & 0x80) ? true : false;         // Byte 2, bit 7 - Launch Control Active
          rev = (byte2 & 0x02) ? true : false;            // Byte 2, bit 1 - Torque Reduction Active (REV limiter)
          
          // Byte 3 status bits
          // 3:5 = Air Con Request, 3:4 = Air Con Output, 3:0 = Thermo-fan 1 On
          airCon = (byte3 & 0x10) ? true : false;         // Byte 3, bit 4 - Air Con Output
          fan = (byte3 & 0x01) ? true : false;            // Byte 3, bit 0 - Thermo-fan 1 On
          
          // Byte 7 status bits
          // 7:0 = Traction Control Light
          syncStatus = (byte7 & 0x01) ? true : false;     // Using TC Light as sync indicator
          
          break;
        }
        case 0x362: {
          uint16_t adv_raw = (can_message.data.byte[4] << 8) | can_message.data.byte[5];
          adv = adv_raw / 10.0;
          break;
        }
        default:
          break;
      }
    } else {
      Serial.println("[CAN] Error reading CAN message.");
    }
  }

  // Print periodic summary of all CAN data received (every 5 seconds)
  if (currentTime - lastDebugPrint > 5000) {
    Serial.println("[CAN] === Data Summary ===");
    Serial.printf("[CAN] RPM: %u, MAP: %d kPa, TPS: %d%%\n", rpm, mapData, tps);
    Serial.printf("[CAN] CLT: %d°C, IAT: %d°C, BAT: %.1fV\n", (int)clt, (int)iat, bat);
    Serial.printf("[CAN] AFR: %.1f, FP: %d psi, ADV: %d°\n", afrConv, fp, adv);
    Serial.printf("[CAN] Indicators: SYNC=%s, FAN=%s, REV=%s, LCH=%s, AC=%s, DFCO=%s\n", 
                  syncStatus ? "ON" : "OFF", fan ? "ON" : "OFF", rev ? "ON" : "OFF",
                  launch ? "ON" : "OFF", airCon ? "ON" : "OFF", dfco ? "ON" : "OFF");
    Serial.printf("[CAN] Messages processed: %u, Refresh rate: %u Hz\n", messageCount, refreshRate);
    lastDebugPrint = currentTime;
  }

}
