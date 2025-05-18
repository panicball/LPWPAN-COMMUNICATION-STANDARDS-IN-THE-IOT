// Device B (Receiver) - BLE broadcast scanning

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define SCAN_TIME 5
#define TARGET_DEVICE_NAME "ESP32-TempSensor"

unsigned long lastScanStart = 0;
int devicesFoundCount = 0;
bool devicesFound = false;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == TARGET_DEVICE_NAME) {
      devicesFound = true;
      Serial.println("\n-------------------------------------------");
      Serial.println("* TARGET DEVICE FOUND *");
      Serial.print("Device name: ");
      Serial.println(advertisedDevice.getName().c_str());
      Serial.print("Device address: ");
      Serial.println(advertisedDevice.getAddress().toString().c_str());
      Serial.print("RSSI: ");
      Serial.println(advertisedDevice.getRSSI());
      
      if (advertisedDevice.haveManufacturerData()) {
        String manufData = advertisedDevice.getManufacturerData();
        
        Serial.print("Manufacturer data (hex): ");
        for (int i = 0; i < manufData.length(); i++) {
          Serial.printf("%02X ", (uint8_t)manufData[i]);
        }
        Serial.println();
        
        if (manufData.length() >= 6) {
          for (int i = 0; i < manufData.length() - 2; i++) {
            if ((uint8_t)manufData[i] == 0xFE) {
              uint8_t tempInt = (uint8_t)manufData[i+1];
              uint8_t tempFrac = (uint8_t)manufData[i+2];
              float temperature = tempInt + (float)tempFrac/100.0;
              
              Serial.print("Temperature: ");
              Serial.print(temperature);
              Serial.println(" °C");
            }
            
            if ((uint8_t)manufData[i] == 0xFF) {
              uint8_t humInt = (uint8_t)manufData[i+1];
              uint8_t humFrac = (uint8_t)manufData[i+2];
              float humidity = humInt + (float)humFrac/100.0;
              
              Serial.print("Humidity: ");
              Serial.print(humidity);
              Serial.println(" %");
            }
          }
        } else {
          Serial.println("Manufacturer data received without temperature/humidity information");
        }
      } else {
        Serial.println("Device has no manufacturer data");
      }
      
      Serial.println("-------------------------------------------");
    }
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n----- DEVICE B STARTUP -----");
  Serial.println("Device B (Receiver) - BLE scanning mode");
  
  BLEDevice::init("ESP32-C6_ScannerB");
  Serial.println("BLE device initialized");
  
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  
  Serial.println("BLE scanning configured");
  Serial.println("----- DEVICE B STARTUP COMPLETE -----\n");
  
  lastScanStart = millis();
  start_scanning();
}

void loop() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastScanStart >= SCAN_TIME * 1000) {
    Serial.println("\n--- Starting new scanning cycle ---");
    devicesFound = false;
    start_scanning();
  }
  
  delay(100);
}

void start_scanning() {
  lastScanStart = millis();
  
  Serial.println("Scanning for BLE devices...");
  
  BLEScan* pBLEScan = BLEDevice::getScan();
  
  pBLEScan->start(SCAN_TIME, false);
  
  Serial.println("Scanning complete.");
  
  if (!devicesFound) {
    Serial.println("Target device \"" + String(TARGET_DEVICE_NAME) + "\" not found.");
  }
  
  pBLEScan->clearResults();
}