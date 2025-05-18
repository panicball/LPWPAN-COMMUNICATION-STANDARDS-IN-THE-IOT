// Device A (Client) - Temperature measurement and sending via notifications

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include "DHT.h"

#define DHTPIN 5
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

static BLEAddress serverAddress("40:4c:ca:4f:16:02");

unsigned long lastWakeupTime = 0;
unsigned long attemptTimeout = 3000;

#define uS_TO_S_FACTOR 1000000ULL
#define DEEP_SLEEP_DURATION 180

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
  Serial.println("\n\n----- CLIENT STARTUP (SCENARIO 4) -----");
  Serial.println("Device A (Client) - BLE with GATT notifications");
  
  esp_sleep_enable_timer_wakeup(DEEP_SLEEP_DURATION * uS_TO_S_FACTOR);
  
  dht.begin();
  Serial.println("DHT sensor initialized");

  BLEDevice::init("ESP32-C6_Client_S4");
  Serial.println("BLE device initialized");
  Serial.println("----- CLIENT STARTUP COMPLETE -----\n");
  
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
  delay(2000);
  
  Serial.println("3. BLE connection initiation with device B (3 sec)");
  
  BLEClient* pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
  
  bool connected = false;
  unsigned long startTime = millis();
  
  while (millis() - startTime < attemptTimeout && !connected) {
    Serial.println("Attempting to connect to server...");
    
    Serial.print("Connecting to MAC address: ");
    Serial.println(serverAddress.toString().c_str());
    
    connected = pClient->connect(serverAddress);
    
    if (connected) {
      Serial.println("Successfully connected!");
      break;
    } else {
      Serial.println("Connection failed, trying again...");
      delay(500);
    }
  }
  
  if (!connected) {
    Serial.println("Failed to connect to server. Aborting cycle.");
    delete pClient;
    
    Serial.println("Will try in the next cycle");
    goToSleep();
    return;
  }
  
  BLERemoteService* pRemoteService = pClient->getService(BLEUUID(SERVICE_UUID));
  if (pRemoteService == nullptr) {
    Serial.println("ERROR: Failed to find service UUID: " + String(SERVICE_UUID));
    pClient->disconnect();
    delete pClient;
    
    Serial.println("Will try in the next cycle");
    goToSleep();
    return;
  }
  
  BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(BLEUUID(CHARACTERISTIC_UUID));
  if (pRemoteCharacteristic == nullptr) {
    Serial.println("ERROR: Failed to find characteristic UUID: " + String(CHARACTERISTIC_UUID));
    pClient->disconnect();
    delete pClient;
    
    Serial.println("Will try in the next cycle");
    goToSleep();
    return;
  }
  
  Serial.println("4. Characteristic value update (1 sec)");
  
  String data = String(temperature) + "," + String(humidity);
  
  Serial.println("Sending data: " + data);
  
  if (pRemoteCharacteristic->canWrite()) {
    pRemoteCharacteristic->writeValue((uint8_t*)data.c_str(), data.length());
    Serial.println("Data successfully updated!");
    delay(1000);
    
    Serial.println("5. Sending notification to device B (1 sec)");
    
    if (pRemoteCharacteristic->canNotify()) {
      Serial.println("Registering for notifications...");
      
      if (pRemoteCharacteristic->canIndicate()) {
        Serial.println("Characteristic can send indication, sending...");
        pRemoteCharacteristic->registerForNotify([](BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
          Serial.println("Notification/indication received from server.");
        });
      } else {
        Serial.println("Characteristic cannot send indication.");
      }
      
      delay(1000);
    } else {
      Serial.println("ERROR: Characteristic cannot send notifications!");
      delay(1000);
    }
  } else {
    Serial.println("ERROR: Characteristic cannot be written!");
    delay(1000);
  }
  
  Serial.println("6. BLE connection termination (1 sec)");
  pClient->disconnect();
  delete pClient;
  Serial.println("Disconnected from server");
  delay(1000);
  
  goToSleep();
}

void goToSleep() {
  Serial.println("7. Returning to deep sleep (1 sec)");
  Serial.println("Sleeping for " + String(DEEP_SLEEP_DURATION) + " seconds...");
  Serial.flush();
  delay(1000);
  
  esp_deep_sleep_start();
}