// Scenario 2: BLE broadcast mode and scanning
// Device A (Sender) - Temperature measurement and broadcasting

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>
#include "DHT.h"

#define DHTPIN 5
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define DEVICE_NAME "ESP32-TempSensor"
BLEAdvertising *pAdvertising = nullptr;
BLEAdvertisementData advData;

const unsigned long broadcastDuration = 5000;
unsigned long lastWakeupTime = 0;

#define uS_TO_S_FACTOR 1000000ULL
#define DEEP_SLEEP_DURATION 180

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n----- DEVICE A STARTUP -----");
  Serial.println("Device A (Sender) - BLE broadcast mode");
  
  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_DURATION * uS_TO_S_FACTOR);
  
  dht.begin();
  Serial.println("DHT sensor initialized");
  
  BLEDevice::init(DEVICE_NAME);
  Serial.println("BLE device initialized");
  Serial.println("----- DEVICE A STARTUP COMPLETE -----\n");
  
  lastWakeupTime = millis();
  executeCycle();
}

void loop() {
  delay(100);
}

void executeCycle() {
  Serial.println("\n--- Starting cycle ---");
  Serial.println("1. Waking up from deep sleep (2 sec)");
  delay(2000);
  
  Serial.println("2. Temperature sensor initialization and measurement (3 sec)");
  delay(1000);
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read data from DHT sensor!");
    temperature = 22.5;
    humidity = 45.0;
    Serial.println("Using test data for further testing");
  }
  
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" °C, Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");
  
  Serial.println("3. Enabling BLE broadcast mode (2 sec)");
  
  pAdvertising = BLEDevice::getAdvertising();
  
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  
  uint8_t tempInt = (uint8_t)temperature;
  uint8_t tempFrac = (uint8_t)((temperature - tempInt) * 100);
  uint8_t humInt = (uint8_t)humidity;
  uint8_t humFrac = (uint8_t)((humidity - humInt) * 100);
  
  // Create data structure
  uint8_t customData[] = {
    0xFE, tempInt, tempFrac,  // Temperature tag and value
    0xFF, humInt, humFrac     // Humidity tag and value
  };
  
  advData.setName(DEVICE_NAME);
  advData.setFlags(0x06);
  advData.setManufacturerData(String((char*)customData, sizeof(customData)));
  
  pAdvertising->setAdvertisementData(advData);
  
  pAdvertising->start();
  Serial.println("BLE broadcast started");
  Serial.println("Sending data:");
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" °C (");
  Serial.print(tempInt);
  Serial.print(".");
  Serial.print(tempFrac);
  Serial.println(")");
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.print(" % (");
  Serial.print(humInt);
  Serial.print(".");
  Serial.print(humFrac);
  Serial.println(")");
  
  delay(2000);
  
  Serial.println("4. Broadcasting temperature data in advertising packets (5 sec)");
  unsigned long startTime = millis();
  while (millis() - startTime < broadcastDuration) {
    delay(100);
  }
  
  Serial.println("5. Disabling BLE broadcast mode (1 sec)");
  pAdvertising->stop();
  Serial.println("BLE broadcast stopped");
  delay(1000);
  
  Serial.println("6. Returning to deep sleep (1 sec)");
  Serial.println("Sleeping for " + String(DEEP_SLEEP_DURATION) + " seconds...");
  Serial.flush();
  delay(1000);
  
  esp_deep_sleep_start();
}