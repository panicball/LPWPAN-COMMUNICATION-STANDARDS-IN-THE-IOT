#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include <esp_gap_ble_api.h>

#define SERVER_NAME "PHY_TEST"
#define SERVICE_UUID "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define CHAR_UUID    "BEB5483E-36E1-4688-B7F5-EA07361B26A8"

BLEClient* pClient = NULL;
BLERemoteCharacteristic* pRemoteCharacteristic = NULL;
BLEAdvertisedDevice* myDevice = NULL;
BLEScan* pBLEScan = NULL;

bool connected = false;
unsigned long lastPacketTime = 0;
unsigned long packetCount = 0;
unsigned long bytesReceived = 0;
uint8_t lastSequence = 0;
uint8_t lostPackets = 0;
unsigned long phyStartTime = 0;
bool currentPHY1M = true;

void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, 
                    uint8_t* pData, size_t length, bool isNotify) {
  unsigned long currentTime = millis();
  
  if (lastPacketTime > 0) {
    unsigned long interval = currentTime - lastPacketTime;
    
    uint8_t expectedSequence = (lastSequence + 1) & 0xFF;
    if (pData[0] != expectedSequence) {
      lostPackets++;
      Serial.printf("Lost packet. Exp: %d, Got: %d\n", 
                   expectedSequence, pData[0]);
    }
    
    if (packetCount % 10 == 0) {
      Serial.printf("PHY: %s, Pkts: %lu, Bytes: %lu, Lost: %u, Interval: %.2f ms\n",
                   currentPHY1M ? "1M" : "2M",
                   packetCount,
                   bytesReceived,
                   lostPackets,
                   (float)interval);
    }
  }
  
  lastPacketTime = currentTime;
  packetCount++;
  bytesReceived += length;
  lastSequence = pData[0];
  
  if (currentTime - phyStartTime > 60000) {
    currentPHY1M = !currentPHY1M;
    phyStartTime = currentTime;
    
    Serial.printf("\n----- PHY: %s -----\n\n", 
                 currentPHY1M ? "1M" : "2M");
    packetCount = 0;
    bytesReceived = 0;
    lostPackets = 0;
  }
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == SERVER_NAME) {
      Serial.print("Found: ");
      Serial.println(advertisedDevice.getName().c_str());
      
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      BLEDevice::getScan()->stop();
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("BLE PHY Client");
  
  BLEDevice::init("");
  
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  
  Serial.println("Scanning...");
  pBLEScan->start(30);
}

void loop() {
  if (myDevice != NULL && !connected) {
    connectToServer();
  }
  
  if (connected && !pClient->isConnected()) {
    connected = false;
    Serial.println("Disconnected");
    
    delete pClient;
    pClient = NULL;
    delay(1000);
    connectToServer();
  }
  
  delay(10);
}

bool connectToServer() {
  Serial.print("Connecting to ");
  Serial.println(myDevice->getName().c_str());
  
  pClient = BLEDevice::createClient();
  
  if (!pClient->connect(myDevice)) {
    Serial.println("Connection failed");
    return false;
  }
  
  Serial.println("Connected");
  phyStartTime = millis();
  
  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  if (pRemoteService == nullptr) {
    Serial.println("Service not found");
    pClient->disconnect();
    return false;
  }
  
  pRemoteCharacteristic = pRemoteService->getCharacteristic(CHAR_UUID);
  if (pRemoteCharacteristic == nullptr) {
    Serial.println("Characteristic not found");
    pClient->disconnect();
    return false;
  }
  
  if (pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    Serial.println("Notifications enabled");
  }
  
  connected = true;
  return true;
}