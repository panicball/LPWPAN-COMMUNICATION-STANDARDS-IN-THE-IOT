// Scenario 1: Standard BLE cycle between two ESP32-C6 devices
// Device A (Client) - Temperature measurement and sending

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include "DHT.h"

#define DHTPIN 5 
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

static BLEAddress serverAddress("40:4c:ca:4f:16:02");

// Timing variables
const unsigned long cycleTime = 3 * 60 * 1000; // 3 minutes in milliseconds
unsigned long lastWakeupTime = 0;
unsigned long attemptTimeout = 10000; // 10 seconds timeout for each attempt

#define uS_TO_S_FACTOR 1000000ULL
#define DEEP_SLEEP_DURATION 180     // Sleep duration in seconds (3 minutes)

// Class for monitoring notifications
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.println("***CLIENT: Connected to server***");
  }

  void onDisconnect(BLEClient* pclient) {
    Serial.println("***CLIENT: Disconnected from server***");
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n----- CLIENT STARTUP -----");
  Serial.println("Device A (Client) - Standard BLE cycle");
  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_DURATION * uS_TO_S_FACTOR);

  dht.begin();
  Serial.println("DHT sensor initialized");

  BLEDevice::init("ESP32-C6_ClientA");
  Serial.println("BLE device initialized");
  Serial.println("----- CLIENT STARTUP COMPLETE -----\n");
  
  lastWakeupTime = millis();
  executeCycle();
}

void loop() {
  if (millis() - lastWakeupTime >= cycleTime) {
    lastWakeupTime = millis();
    executeCycle();
  }
  delay(100); 
}

void executeCycle() {
  Serial.println("\n--- Starting cycle ---");
  Serial.println("1. Waking up from deep sleep (2 sec)");
  delay(2000);

  Serial.println("2. Temperature sensor initialization and measurement (3 sec)");
  delay(1000); // Shorter wait because DHT is already initialized
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read data from DHT sensor!");
    // Use test data so we can continue
    temperature = 22.5;
    humidity = 45.0;
    Serial.println("Using test data for further testing");
  }
  
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" °C, Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");
  
  Serial.println("3. BLE connection initiation with device B (3 sec)");
  BLEClient* pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback()); // Set callbacks

  bool connected = false;
  unsigned long startTime;
  
  for (int i = 0; i < 3; i++) {  // 3 attempts
    Serial.print("Attempting to connect to server (attempt ");
    Serial.print(i + 1);
    Serial.println("/3)...");
    
    Serial.print("Connecting to MAC address: ");
    Serial.println(serverAddress.toString().c_str());
    
    startTime = millis();
    connected = pClient->connect(serverAddress, BLE_ADDR_TYPE_PUBLIC, 10000);
    
    if (connected) {
      Serial.println("Connected successfully!");
      break;
    } else {
      Serial.println("Connection failed, trying again...");
      // Check if stuck for too long
      if (millis() - startTime >= attemptTimeout) {
        Serial.println("Attempt timed out, aborting");
        break;
      }
      delay(1000);
    }
  }
  
  if (!connected) {
    Serial.println("Failed to connect to server. Aborting cycle.");
    pClient->disconnect();
    Serial.println("Will try in the next cycle");
    return;
  }
  
  // 4. Data transmission from device A to device B
  Serial.println("4. Data transmission (1 sec)");
  
  // Get reference to the service
  BLERemoteService* pRemoteService = pClient->getService(BLEUUID(SERVICE_UUID));
  if (pRemoteService == nullptr) {
    Serial.println("ERROR: Failed to find service UUID: " + String(SERVICE_UUID));
    Serial.println("Cannot list all services in this ESP32-C6 version");
    
    pClient->disconnect();
    return;
  }
  
  // Get reference to the characteristic
  BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID));
  if (pRemoteCharacteristic == nullptr) {
    Serial.println("ERROR: Failed to find characteristic UUID: " + String(CHARACTERISTIC_UUID));
    Serial.println("Cannot list all characteristics in this ESP32-C6 version");
    
    pClient->disconnect();
    return;
  }
  
  // Send data
  String data = String(temperature) + "," + String(humidity);
  
  // Additional information
  Serial.println("Sending data: " + data);
  Serial.print("Data length: ");
  Serial.println(data.length());
  
  if (pRemoteCharacteristic->canWrite()) {
    Serial.println("Characteristic can be written");
  } else {
    Serial.println("ERROR: Characteristic cannot be written!");
    pClient->disconnect();
    return;
  }
  
  Serial.println("Sending data via writeValue...");
  pRemoteCharacteristic->writeValue((uint8_t*)data.c_str(), data.length());
  Serial.println("Data sent!");
  
  if (pRemoteCharacteristic->canRead()) {
    Serial.print("Received data from server: ");
  
    uint8_t buffer[100];
    size_t length = 0;
    
    try {
      String value = pRemoteCharacteristic->toString();
      Serial.println(value);
    } catch (...) {
      Serial.println("Error reading data");
    }
  }
  
  delay(1000);
  
  // 5. BLE connection termination
  Serial.println("5. BLE connection termination (1 sec)");
  pClient->disconnect();
  Serial.println("Disconnected from server");
  delay(1000);
  
  // 6. Return to deep sleep
  Serial.println("6. Returning to deep sleep (1 sec)");
  Serial.println("Sleeping for " + String(DEEP_SLEEP_DURATION) + " seconds...");
  Serial.flush(); // Ensure all data is sent before sleep
  delay(1000);
  
  // Go to deep sleep
  esp_deep_sleep_start();
}