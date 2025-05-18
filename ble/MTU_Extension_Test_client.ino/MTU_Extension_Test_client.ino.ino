#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>

#define DEVICE_NAME     "MTU_TEST"
#define SERVICE_UUID    "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define CHAR_UUID       "BEB5483E-36E1-4688-B7F5-EA07361B26A8"
#define DEFAULT_MTU     23
#define EXTENDED_MTU    185

BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
BLEAdvertisedDevice* myDevice = nullptr;
bool connected = false;
unsigned long lastStatusTime = 0;
unsigned long statusInterval = 5000;
unsigned long totalBytesReceived = 0;
unsigned long packetsReceived = 0;
uint16_t currentMtu = DEFAULT_MTU;
bool extendedMtuActive = false;

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    connected = true;
    Serial.println("Connected");
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("Disconnected");
  }
};

static void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                          uint8_t* pData, size_t length, bool isNotify) {
  if (length >= 6 && pData[0] == 'M' && pData[1] == 'T' && pData[2] == 'U') {
    if (pData[3] == 'E' && pData[4] == 'X' && pData[5] == 'T') {
      Serial.println("MTU extend request");
      
      if (!extendedMtuActive) {
        Serial.println("Setting extended MTU");
        
        if (pClient != nullptr && pClient->isConnected()) {
          if (esp_ble_gattc_send_mtu_req(pClient->getGattcIf(), pClient->getConnId()) != ESP_OK) {
            Serial.println("MTU request failed, trying lib method");
            
            bool success = pClient->setMTU(EXTENDED_MTU);
            if (!success) {
              Serial.println("Failed to set MTU");
            } else {
              Serial.printf("Requested MTU: %d\n", EXTENDED_MTU);
            }
          } else {
            Serial.println("MTU request sent");
          }
          
          extendedMtuActive = true;
        }
        
        if (pRemoteCharacteristic != nullptr) {
          uint8_t response[] = {'M', 'T', 'U', 'O', 'K'};
          pRemoteCharacteristic->writeValue(response, 5);
          Serial.println("Sent ACK");
        }
        
        totalBytesReceived = 0;
        packetsReceived = 0;
      }
    } 
    else if (pData[3] == 'D' && pData[4] == 'F' && pData[5] == 'T') {
      Serial.println("MTU default request");
      
      if (extendedMtuActive) {
        Serial.println("Reverting to default MTU");
        
        if (pClient != nullptr && pClient->isConnected()) {
          if (pRemoteCharacteristic != nullptr) {
            uint8_t response[] = {'M', 'T', 'U', 'O', 'K'};
            pRemoteCharacteristic->writeValue(response, 5);
            Serial.println("Sent ACK");
          }
          
          Serial.println("Disconnecting to reset MTU");
          pClient->disconnect();
          
          extendedMtuActive = false;
        }
        
        totalBytesReceived = 0;
        packetsReceived = 0;
      }
    }
  } else {
    totalBytesReceived += length;
    packetsReceived++;
    
    if (packetsReceived % 20 == 0) {
      Serial.printf("Pkt #%lu: %d bytes (MTU:%d, Total:%lu)\n", 
                   packetsReceived, length, currentMtu, totalBytesReceived);
    }
  }
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() && 
        advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
      Serial.printf("Found device with service: %s\n", SERVICE_UUID);
      advertisedDevice.getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
    } else if (advertisedDevice.haveName() && 
               advertisedDevice.getName() == DEVICE_NAME) {
      Serial.printf("Found device: %s\n", advertisedDevice.getName().c_str());
      advertisedDevice.getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
    }
  }
};

bool connectToServer() {
  Serial.printf("Connecting to %s\n", DEVICE_NAME);
  
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
  
  pClient->connect(myDevice);
  if (!pClient->isConnected()) {
    Serial.println("Connection failed");
    return false;
  }
  
  uint16_t mtuSize = extendedMtuActive ? EXTENDED_MTU : DEFAULT_MTU;
  bool success = pClient->setMTU(mtuSize);
  if (success) {
    Serial.printf("MTU requested: %d\n", mtuSize);
  } else {
    Serial.println("MTU request failed");
  }
  
  BLERemoteService* pRemoteService = pClient->getService(BLEUUID(SERVICE_UUID));
  if (pRemoteService == nullptr) {
    Serial.printf("Service not found: %s\n", SERVICE_UUID);
    pClient->disconnect();
    return false;
  }
  
  pRemoteCharacteristic = pRemoteService->getCharacteristic(BLEUUID(CHAR_UUID));
  if (pRemoteCharacteristic == nullptr) {
    Serial.printf("Characteristic not found: %s\n", CHAR_UUID);
    pClient->disconnect();
    return false;
  }
  
  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
  }
  
  connected = true;
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("BLE MTU Client");
  
  BLEDevice::init("");
  
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  
  Serial.println("Scanning...");
  pBLEScan->start(30, false);
}

void loop() {
  if (myDevice != nullptr && !connected) {
    if (connectToServer()) {
      Serial.println("Connected successfully");
    } else {
      Serial.println("Connection failed, retrying...");
      delay(5000);
    }
  }
  
  if (!connected && pClient != nullptr && !pClient->isConnected()) {
    Serial.println("Connection lost, reconnecting...");
    
    delete pClient;
    pClient = nullptr;
    pRemoteCharacteristic = nullptr;
    
    if (connectToServer()) {
      Serial.println("Reconnected");
    } else {
      Serial.println("Reconnection failed");
      delay(5000);
    }
  }
  
  if (connected && millis() - lastStatusTime > statusInterval) {
    lastStatusTime = millis();
    
    if (pClient != nullptr && pClient->isConnected()) {
      uint16_t newMtu = pClient->getMTU();
      if (newMtu != currentMtu && newMtu > 0) {
        Serial.printf("MTU changed: %d\n", newMtu);
        currentMtu = newMtu;
        
        extendedMtuActive = (currentMtu > DEFAULT_MTU);
        
        totalBytesReceived = 0;
        packetsReceived = 0;
      }
    }
    
    Serial.println("--- STATUS ---");
    Serial.printf("Connected: %s\n", connected ? "Yes" : "No");
    Serial.printf("MTU: %d (%s)\n", currentMtu, extendedMtuActive ? "Extended" : "Default");
    Serial.printf("Packets: %lu\n", packetsReceived);
    Serial.printf("Bytes: %lu\n", totalBytesReceived);
    Serial.println("-------------");
  }
  
  delay(10);
}