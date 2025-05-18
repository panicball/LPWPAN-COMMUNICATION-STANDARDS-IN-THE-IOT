#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_sleep.h>

#define TEST_DURATION   60000
#define DEVICE_NAME     "DUTY_TEST"
#define SERVICE_UUID    "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define CHAR_UUID       "BEB5483E-36E1-4688-B7F5-EA07361B26A8"
#define SENSOR_CHAR_UUID "BEB5483E-36E1-4688-B7F5-EA07361B26A9"
#define PACKET_SIZE     20
#define TOTAL_TEST_CYCLES 3

enum TransmissionMode {
  // CONTINUOUS_STREAMING,
  // DATA_BATCHING,
  PERIODIC_POLLING
};

BLEServer* pServer = NULL;
BLECharacteristic* pDataCharacteristic = NULL;
BLECharacteristic* pSensorCharacteristic = NULL;
bool deviceConnected = false;
uint8_t testData[PACKET_SIZE];
unsigned long lastTransmissionTime = 0;
unsigned long modeChangeTime = 0;
TransmissionMode currentMode = PERIODIC_POLLING;
uint8_t modeCount = 0;
uint16_t batchCounter = 0;
uint16_t sensorValue = 0;
bool testingComplete = false;

const uint16_t CONTINUOUS_INTERVAL = 100;
const uint16_t BATCH_INTERVAL = 20;
const uint16_t BATCH_SIZE = 20;
const uint16_t BATCH_CYCLE = 5000;
const uint16_t POLLING_INTERVAL = 30000;

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Connected");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Disconnected");
    pServer->startAdvertising();
  }
};

void updateTransmissionMode() {
  if (modeCount >= TOTAL_TEST_CYCLES * 3) {
    if (!testingComplete) {
      Serial.println("\n===================");
      Serial.println("TESTING COMPLETE");
      Serial.println("===================");
      testingComplete = true;
      
      uint8_t endData[PACKET_SIZE];
      memset(endData, 0xFF, PACKET_SIZE);
      
      if (deviceConnected && pDataCharacteristic != NULL) {
        pDataCharacteristic->setValue(endData, PACKET_SIZE);
        pDataCharacteristic->notify();
      }
    }
    return;
  }
  
  currentMode = (TransmissionMode)(modeCount % 3);
  
  int currentCycle = (modeCount / 3) + 1;
  Serial.printf("\n=== CYCLE %d/%d ===\n", currentCycle, TOTAL_TEST_CYCLES);
  
  switch (currentMode) {
    // case CONTINUOUS_STREAMING:
    //   Serial.println("MODE: CONTINUOUS");
    //   Serial.printf("Interval: %dms\n", CONTINUOUS_INTERVAL);
    //   break;
      
    // case DATA_BATCHING:
    //   Serial.println("MODE: BATCHING");
    //   Serial.printf("%d pkts @ %dms, sleep %dms\n", 
    //                BATCH_SIZE, BATCH_INTERVAL, BATCH_CYCLE - (BATCH_SIZE * BATCH_INTERVAL));
    //   batchCounter = 0;
    //   break;
      
    case PERIODIC_POLLING:
      Serial.println("MODE: POLLING");
      Serial.printf("Interval: %dms\n", POLLING_INTERVAL);
      break;
  }
  
  modeChangeTime = millis();
  lastTransmissionTime = millis();
}

uint16_t readSensor() {
  sensorValue = (sensorValue + 1) % 1024;
  return sensorValue;
}

void setup() {
  Serial.begin(115200);
  Serial.println("BLE Duty Cycle Test");

  BLEDevice::init(DEVICE_NAME);
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  pDataCharacteristic = pService->createCharacteristic(
                           CHAR_UUID,
                           BLECharacteristic::PROPERTY_READ |
                           BLECharacteristic::PROPERTY_NOTIFY
                         );
  pDataCharacteristic->addDescriptor(new BLE2902());
  
  pSensorCharacteristic = pService->createCharacteristic(
                             SENSOR_CHAR_UUID,
                             BLECharacteristic::PROPERTY_READ |
                             BLECharacteristic::PROPERTY_NOTIFY
                           );
  pSensorCharacteristic->addDescriptor(new BLE2902());
  
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  Serial.println("Ready");
  
  for (int i = 0; i < PACKET_SIZE; i++) {
    testData[i] = i % 256;
  }
  
  updateTransmissionMode();
}

void loop() {
  if (!testingComplete && millis() - modeChangeTime >= TEST_DURATION) {
    Serial.println("\n--- TEST COMPLETE ---");
    
    if (deviceConnected) {
      uint8_t endCaseData[PACKET_SIZE];
      memset(endCaseData, 0xFE, PACKET_SIZE);
      endCaseData[0] = currentMode;
      endCaseData[1] = modeCount;
      
      if (pDataCharacteristic != NULL) {
        pDataCharacteristic->setValue(endCaseData, PACKET_SIZE);
        pDataCharacteristic->notify();
      }
      
      delay(100);
    }
    
    modeCount++;
    updateTransmissionMode();
  }
  
  if (!testingComplete && deviceConnected) {
    unsigned long currentTime = millis();
    
    switch (currentMode) {
      // case CONTINUOUS_STREAMING:
      //   if (currentTime - lastTransmissionTime >= CONTINUOUS_INTERVAL) {
      //     lastTransmissionTime = currentTime;
      //     testData[0]++;
      //     pDataCharacteristic->setValue(testData, PACKET_SIZE);
      //     pDataCharacteristic->notify();
      //     Serial.printf("TX: #%d\n", testData[0]);
      //   }
      //   break;
        
      // case DATA_BATCHING:
      //   if (batchCounter < BATCH_SIZE) {
      //     if (currentTime - lastTransmissionTime >= BATCH_INTERVAL) {
      //       lastTransmissionTime = currentTime;
      //       testData[0]++;
      //       pDataCharacteristic->setValue(testData, PACKET_SIZE);
      //       pDataCharacteristic->notify();
      //       batchCounter++;
      //       Serial.printf("TX: %d/%d\n", batchCounter, BATCH_SIZE);
      //     }
      //   } else {
      //     if (currentTime - lastTransmissionTime >= (BATCH_CYCLE - (BATCH_SIZE * BATCH_INTERVAL))) {
      //       Serial.println("New batch");
      //       batchCounter = 0;
      //       lastTransmissionTime = currentTime;
      //       if (!deviceConnected) {
      //         Serial.printf("Sleep %dms\n", BATCH_CYCLE - (BATCH_SIZE * BATCH_INTERVAL) - 100);
      //         delay(10);
      //         esp_sleep_enable_timer_wakeup((BATCH_CYCLE - (BATCH_SIZE * BATCH_INTERVAL) - 100) * 1000);
      //         esp_light_sleep_start();
      //       }
      //     }
      //   }
      //   break;
        
      case PERIODIC_POLLING:
        if (currentTime - lastTransmissionTime >= POLLING_INTERVAL) {
          lastTransmissionTime = currentTime;
          
          Serial.println("Wakeup");
          
          uint16_t reading = readSensor();
          
          uint8_t sensorData[2];
          sensorData[0] = (reading >> 8) & 0xFF;
          sensorData[1] = reading & 0xFF;
          
          pSensorCharacteristic->setValue(sensorData, 2);
          pSensorCharacteristic->notify();
          
          Serial.printf("Sent: %d\n", reading);
          
          Serial.printf("Sleep %dms\n", POLLING_INTERVAL - 100);
          
          if (!deviceConnected) {
            delay(10);
            esp_sleep_enable_timer_wakeup((POLLING_INTERVAL - 100) * 1000);
            esp_light_sleep_start();
          }
        }
        break;
    }
  }
  
  delay(10);
  
  if (testingComplete && !deviceConnected) {
    static unsigned long lastAdvertisingTime = 0;
    if (millis() - lastAdvertisingTime > 2000) {
      Serial.println("Still advertising...");
      lastAdvertisingTime = millis();
    }
  }
}