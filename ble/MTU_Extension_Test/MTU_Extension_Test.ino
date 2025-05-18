#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_sleep.h>

#define TEST_DURATION   60000     // 60 seconds per MTU size
#define DEVICE_NAME     "MTU_TEST"
#define SERVICE_UUID    "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define CHAR_UUID       "BEB5483E-36E1-4688-B7F5-EA07361B26A8"
#define DEFAULT_MTU     23
#define EXTENDED_MTU    185
#define TOTAL_DATA_SIZE 1024
#define SEND_INTERVAL   200

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
uint8_t* testData;
unsigned long lastSendTime = 0;
unsigned long mtuChangeTime = 0;
uint16_t currentMtu = DEFAULT_MTU;
unsigned long totalBytesSent = 0;
unsigned long packetsSent = 0;

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Connected");
    
    currentMtu = DEFAULT_MTU;
    mtuChangeTime = millis();
    Serial.printf("MTU: %d bytes\n", currentMtu);
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Disconnected");
    
    pServer->startAdvertising();
    
    totalBytesSent = 0;
    packetsSent = 0;
  }
  
  void onMtuChanged(BLEServer* pServer, uint16_t mtu) {
    Serial.printf("MTU changed: %d\n", mtu);
    currentMtu = mtu;
    mtuChangeTime = millis();
    
    totalBytesSent = 0;
    packetsSent = 0;
  }
};

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    String value = pCharacteristic->getValue();
    
    if(value.length() > 0) {
      Serial.print("Received: ");
      for(int i = 0; i < value.length(); i++) {
        Serial.print(value[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
      
      if (value.length() >= 3 && value[0] == 'M' && value[1] == 'T' && value[2] == 'U') {
        if (value[3] == 'O' && value[4] == 'K') {
          Serial.println("MTU change acknowledged");
        }
      }
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("BLE MTU Test");

  testData = (uint8_t*)malloc(TOTAL_DATA_SIZE);
  if (testData == NULL) {
    Serial.println("Memory allocation failed");
    while(1);
  }
  
  for (int i = 0; i < TOTAL_DATA_SIZE; i++) {
    testData[i] = i % 256;
  }

  BLEDevice::init(DEVICE_NAME);
  BLEDevice::setMTU(DEFAULT_MTU);
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  pCharacteristic = pService->createCharacteristic(
                      CHAR_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_WRITE
                    );
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  
  pService->start();
  
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  
  Serial.println("Ready");
  
  mtuChangeTime = millis();
}

void loop() {
  if (deviceConnected && (millis() - mtuChangeTime >= TEST_DURATION)) {
    Serial.println("Test complete. Switching MTU...");
    
    if (currentMtu <= DEFAULT_MTU) {
      Serial.println("Requesting extended MTU");
      BLEDevice::setMTU(EXTENDED_MTU);
      
      uint8_t requestMtu[] = {'M', 'T', 'U', 'E', 'X', 'T'};
      pCharacteristic->setValue(requestMtu, 6);
      pCharacteristic->notify();
    } else {
      Serial.println("Requesting default MTU");
      BLEDevice::setMTU(DEFAULT_MTU);
      
      uint8_t requestMtu[] = {'M', 'T', 'U', 'D', 'F', 'T'};
      pCharacteristic->setValue(requestMtu, 6);
      pCharacteristic->notify();
    }
    
    mtuChangeTime = millis();
  }
  
  if (deviceConnected && (millis() - lastSendTime >= SEND_INTERVAL)) {
    lastSendTime = millis();
    
    uint16_t packetSize = currentMtu - 3;
    
    if (packetSize > TOTAL_DATA_SIZE) {
      packetSize = TOTAL_DATA_SIZE;
    }
    
    testData[0] = packetsSent % 256;
    
    pCharacteristic->setValue(testData, packetSize);
    pCharacteristic->notify();
    
    totalBytesSent += packetSize;
    packetsSent++;
    
    Serial.printf("Sent: %d bytes (MTU:%d, total:%lu, pkts:%lu)\n", 
                 packetSize, currentMtu, totalBytesSent, packetsSent);
  }
  
  delay(20);
}

void cleanup() {
  if (testData != NULL) {
    free(testData);
    testData = NULL;
  }
}