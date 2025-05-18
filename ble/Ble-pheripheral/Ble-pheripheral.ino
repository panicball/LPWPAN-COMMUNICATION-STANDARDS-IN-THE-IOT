#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_sleep.h>
#include <esp_gap_ble_api.h>

#define DEVICE_NAME "ESP32-C6_BLE_TEST"
#define TEST_SERVICE_UUID           "180D"
#define CONTROL_CHAR_UUID           "2A37"
#define DATA_CHAR_UUID              "2A38"
#define SCENARIO_DURATION           60000
#define NUM_SCENARIOS               2

BLEServer* pServer = NULL;
BLEService* pTestService = NULL;
BLECharacteristic* pControlCharacteristic = NULL;
BLECharacteristic* pDataCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long lastDataSent = 0;
unsigned long scenarioStartTime = 0;
uint8_t testData[512];
int currentScenario = 0;

void startAdvertisingWithCurrentSettings();

struct TestScenario {
  uint16_t advInterval;
  uint16_t connInterval;
  uint16_t slaveLatency;
  uint16_t dataSize;
  uint16_t dataInterval;
  bool useSleepMode;
  const char* description;
};

TestScenario scenarios[NUM_SCENARIOS] = {
  // Scenario 1: Fast advertising, no sleep
  // {
  //   .advInterval = 20,       // 20ms advertising interval
  //   .connInterval = 6,       // 7.5ms connection interval
  //   .slaveLatency = 0,       // No slave latency
  //   .dataSize = 20,          // Small data size
  //   .dataInterval = 100,     // Fast data transmission
  //   .useSleepMode = false,   // No sleep
  //   .description = "Fast advertising, fast connection, no sleep"
  // },
  
  // // Scenario 2: Slow advertising, no sleep
  // {
  //   .advInterval = 1000,     // 1000ms advertising interval
  //   .connInterval = 6,       // 7.5ms connection interval
  //   .slaveLatency = 0,       // No slave latency
  //   .dataSize = 20,          // Small data size
  //   .dataInterval = 100,     // Fast data transmission
  //   .useSleepMode = false,   // No sleep
  //   .description = "Slow advertising, fast connection, no sleep"
  // },
  
  // // Scenario 3: Fast advertising, single channel
  // {
  //   .advInterval = 20,       // 20ms advertising interval
  //   .connInterval = 6,       // 7.5ms connection interval
  //   .slaveLatency = 0,       // No slave latency
  //   .dataSize = 20,          // Small data size
  //   .dataInterval = 100,     // Fast data transmission
  //   .useSleepMode = false,   // No sleep
  //   .description = "Fast advertising on primary channel only"
  // },

  //   // Scenario 3.1: Fast advertising, all channels
  // {
  //   .advInterval = 20,       // 20ms advertising interval
  //   .connInterval = 6,       // 7.5ms connection interval
  //   .slaveLatency = 0,       // No slave latency
  //   .dataSize = 20,          // Small data size
  //   .dataInterval = 100,     // Fast data transmission
  //   .useSleepMode = false,   // No sleep
  //   .description = "Fast advertising on primary channel only"
  // },
  
  // // Scenario 4: Fast connection, slave latency enabled
  // {
  //   .advInterval = 100,      // 100ms advertising interval
  //   .connInterval = 6,       // 7.5ms connection interval
  //   .slaveLatency = 10,      // Slave latency of 10
  //   .dataSize = 20,          // Small data size
  //   .dataInterval = 100,     // Fast data transmission
  //   .useSleepMode = false,   // No sleep
  //   .description = "Fast connection with slave latency"
  // },
  
  // // Scenario 5: Slow connection, no slave latency
  // {
  //   .advInterval = 100,      // 100ms advertising interval
  //   .connInterval = 800,     // 1000ms connection interval
  //   .slaveLatency = 0,       // No slave latency
  //   .dataSize = 20,          // Small data size
  //   .dataInterval = 1000,    // Slow data transmission
  //   .useSleepMode = false,   // No sleep
  //   .description = "Slow connection, no slave latency"
  // },
  
  // // Scenario 6: Large data packets
  // {
  //   .advInterval = 100,      // 100ms advertising interval
  //   .connInterval = 80,      // 100ms connection interval
  //   .slaveLatency = 0,       // No slave latency
  //   .dataSize = 200,         // Large data size
  //   .dataInterval = 1000,    // Slow data transmission
  //   .useSleepMode = false,   // No sleep
  //   .description = "Large data packets"
  // },

  // // Scenario 6.1: Large data packets
  // {
  //   .advInterval = 100,      // 100ms advertising interval
  //   .connInterval = 80,      // 100ms connection interval
  //   .slaveLatency = 0,       // No slave latency
  //   .dataSize = 20,          // Small data size
  //   .dataInterval = 1000,    // Slow data transmission
  //   .useSleepMode = false,   // No sleep
  //   .description = "Small data packets"
  // },
  
  // // Scenario 7: Sleep mode between activities
  // {
  //   .advInterval = 100,      // 100ms advertising interval
  //   .connInterval = 80,      // 100ms connection interval
  //   .slaveLatency = 4,       // Slave latency of 4
  //   .dataSize = 20,          // Small data size
  //   .dataInterval = 5000,    // Very slow data transmission
  //   .useSleepMode = true,    // Use sleep mode
  //   .description = "Sleep mode between activities"
  // },

  // // Scenario 7.1: Sleep mode between activities
  // {
  //   .advInterval = 100,      // 100ms advertising interval
  //   .connInterval = 80,      // 100ms connection interval
  //   .slaveLatency = 4,       // Slave latency of 4
  //   .dataSize = 20,          // Small data size
  //   .dataInterval = 5000,    // Very slow data transmission
  //   .useSleepMode = false,    // Use sleep mode
  //   .description = "No Sleep mode between activities"
  // },

  {
    .advInterval = 100,
    .connInterval = 80,
    .slaveLatency = 0,
    .dataSize = 20,
    .dataInterval = 5000,
    .useSleepMode = false,
    .description = "No Slave latency"
  },
  {
    .advInterval = 100,
    .connInterval = 80,
    .slaveLatency = 10,
    .dataSize = 20,
    .dataInterval = 5000,
    .useSleepMode = false,
    .description = "Slave latency"
  },
};

void requestConnectionUpdate(uint16_t connInterval, uint16_t slaveLatency) {
  Serial.printf("Connection update requested: interval=%d, latency=%d\n", 
                connInterval, slaveLatency);
}

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Device connected");
    
    TestScenario& scenario = scenarios[currentScenario];
    requestConnectionUpdate(scenario.connInterval, scenario.slaveLatency);
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Device disconnected");
    
    startAdvertisingWithCurrentSettings();
  }
};

void startAdvertisingWithCurrentSettings() {
  TestScenario& scenario = scenarios[currentScenario];
  
  BLEDevice::getAdvertising()->stop();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(TEST_SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  
  pAdvertising->setMinInterval((uint16_t)(scenario.advInterval * 1.6));
  pAdvertising->setMaxInterval((uint16_t)(scenario.advInterval * 1.6));
  
  BLEDevice::startAdvertising();
  Serial.printf("Advertising started with interval: %d ms\n", 
               scenario.advInterval);
}

void moveToNextScenario() {
  delay(5000);
  currentScenario = (currentScenario + 1) % NUM_SCENARIOS;
  Serial.printf("\n--- Switching to Scenario %d ---\n", currentScenario + 1);
  Serial.printf("Description: %s\n", scenarios[currentScenario].description);
  
  scenarioStartTime = millis();
  lastDataSent = millis();
  
  startAdvertisingWithCurrentSettings();
  
  if (deviceConnected) {
    TestScenario& scenario = scenarios[currentScenario];
    requestConnectionUpdate(scenario.connInterval, scenario.slaveLatency);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("ESP32-C6 BLE Peripheral Test Starting");
  
  BLEDevice::init(DEVICE_NAME);
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  pTestService = pServer->createService(TEST_SERVICE_UUID);
  
  pControlCharacteristic = pTestService->createCharacteristic(
                             CONTROL_CHAR_UUID,
                             BLECharacteristic::PROPERTY_WRITE |
                             BLECharacteristic::PROPERTY_READ
                           );
  
  pDataCharacteristic = pTestService->createCharacteristic(
                          DATA_CHAR_UUID,
                          BLECharacteristic::PROPERTY_NOTIFY
                        );
  pDataCharacteristic->addDescriptor(new BLE2902());
  
  pTestService->start();
  
  for (int i = 0; i < sizeof(testData); i++) {
    testData[i] = i % 256;
  }
  
  scenarioStartTime = millis();
  Serial.printf("--- Starting Scenario 1 ---\n");
  Serial.printf("Description: %s\n", scenarios[currentScenario].description);
  startAdvertisingWithCurrentSettings();
  
  Serial.println("BLE peripheral ready - running automatic test sequence");
  Serial.println("Each scenario will run for 60 seconds.");
}

void loop() {
  if (millis() - scenarioStartTime >= SCENARIO_DURATION) {
    moveToNextScenario();
  }
  
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
  
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    oldDeviceConnected = deviceConnected;
  }
  
  if (deviceConnected) {
    TestScenario& scenario = scenarios[currentScenario];
    unsigned long currentMillis = millis();
    
    if (currentMillis - lastDataSent >= scenario.dataInterval) {
      lastDataSent = currentMillis;
      
      uint16_t size = (scenario.dataSize > sizeof(testData)) ? sizeof(testData) : scenario.dataSize;
      
      testData[0]++;
      
      pDataCharacteristic->setValue(testData, size);
      pDataCharacteristic->notify();
      
      Serial.printf("Data sent: %d bytes\n", size);
    }
  }
  
  TestScenario& scenario = scenarios[currentScenario];
  if (scenario.useSleepMode && !deviceConnected) {
    uint32_t sleepTime = min(scenario.dataInterval, (uint16_t)1000);
    Serial.printf("Entering light sleep for %d ms\n", sleepTime);
    delay(10);
    
    esp_sleep_enable_timer_wakeup(sleepTime * 1000);
    esp_light_sleep_start();
    
    Serial.println("Woke up from light sleep");
  }
  
  delay(10);
}