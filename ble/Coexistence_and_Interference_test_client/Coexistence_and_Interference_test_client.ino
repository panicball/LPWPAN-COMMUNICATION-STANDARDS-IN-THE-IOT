#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define DEVICE_NAME         "COEX_TEST"
#define SERVICE_UUID        "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define CHAR_UUID           "BEB5483E-36E1-4688-B7F5-EA07361B26A8"
#define PACKET_SIZE         20

// Test statistics
uint32_t packetsReceived = 0;
uint32_t lastSequence = 0;
uint32_t missedPackets = 0;
uint32_t outOfOrderPackets = 0;
uint32_t corruptedPackets = 0;
unsigned long lastStatsTime = 0;
unsigned long connectionTime = 0;

// Connection status
bool connected = false;
bool doScan = true;
BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;

// Forward declarations
void connectToServer(BLEAdvertisedDevice* device);
void scanForServer();

// Handle notifications from server
void notifyCallback(BLERemoteCharacteristic* pCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
  uint8_t sequence = pData[0];
  packetsReceived++;
  
  if (length != PACKET_SIZE) {
    corruptedPackets++;
    Serial.printf("Corrupted: expected %d bytes, got %d\n", PACKET_SIZE, length);
  }
  else if (packetsReceived > 1) {
    uint8_t expectedSequence = (lastSequence + 1) % 256;
    
    if (sequence != expectedSequence) {
      int missed = (sequence - expectedSequence) & 0xFF;
      
      if (missed > 0) {
        missedPackets += missed;
        Serial.printf("Missed %d packet(s): expected %d, got %d\n", 
                     missed, expectedSequence, sequence);
      } else {
        outOfOrderPackets++;
        Serial.printf("Out-of-order: expected %d, got %d\n", 
                     expectedSequence, sequence);
      }
    }
  }
  
  lastSequence = sequence;
  
  // Print stats every 50 packets
  if (packetsReceived % 50 == 0) {
    unsigned long now = millis();
    float packetLoss = (float)missedPackets / (packetsReceived + missedPackets) * 100;
    float packetsPerSecond = 0;
    
    if (lastStatsTime > 0)
      packetsPerSecond = (50.0 * 1000) / (now - lastStatsTime);
    
    Serial.printf("Stats: Rcvd %u, Missed %u (%.1f%%), OOO %u, Corrupt %u, Rate: %.1f pkt/s\n",
                 packetsReceived, missedPackets, packetLoss, outOfOrderPackets, 
                 corruptedPackets, packetsPerSecond);
                 
    lastStatsTime = now;
  }
}

// Scan callback
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice device) {
    if (device.getName() == DEVICE_NAME) {
      Serial.printf("Found: %s\n", device.getName().c_str());
      
      BLEDevice::getScan()->stop();
      
      BLEAdvertisedDevice* myDevice = new BLEAdvertisedDevice(device);
      
      xTaskCreate(
        [](void* parameter) {
          BLEAdvertisedDevice* myDevice = (BLEAdvertisedDevice*)parameter;
          connectToServer(myDevice);
          vTaskDelete(NULL);
        },
        "ConnectTask",
        4096,
        myDevice,
        1,
        NULL
      );
      
      doScan = false;
    }
  }
};

// Connect to the server
void connectToServer(BLEAdvertisedDevice* myDevice) {
  Serial.println("Creating client");
  pClient = BLEDevice::createClient();
  
  class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pClient) {
      connected = true;
      connectionTime = millis();
      Serial.println("Connected!");
    }

    void onDisconnect(BLEClient* pClient) {
      connected = false;
      Serial.println("Disconnected");
      
      // Reset statistics
      packetsReceived = 0;
      lastSequence = 0;
      missedPackets = 0;
      outOfOrderPackets = 0;
      corruptedPackets = 0;
      lastStatsTime = 0;
      
      doScan = true;
    }
  };
  
  pClient->setClientCallbacks(new MyClientCallback());
  
  Serial.printf("Connecting to %s\n", myDevice->getAddress().toString().c_str());
  
  pClient->connect(myDevice);
  
  Serial.println("Finding service...");
  BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
  
  if (pRemoteService == nullptr) {
    Serial.println("Service not found");
    pClient->disconnect();
    return;
  }
  
  Serial.println("Finding characteristic...");
  pRemoteCharacteristic = pRemoteService->getCharacteristic(CHAR_UUID);
  
  if (pRemoteCharacteristic == nullptr) {
    Serial.println("Characteristic not found");
    pClient->disconnect();
    return;
  }
  
  if (pRemoteCharacteristic->canRead()) {
    String value = pRemoteCharacteristic->readValue();
    Serial.print("Initial value: ");
    for (int i = 0; i < value.length(); i++) {
      Serial.printf("%02X ", (uint8_t)value[i]);
    }
    Serial.println();
  }
  
  if (pRemoteCharacteristic->canNotify()) {
    Serial.println("Setting up notifications");
    pRemoteCharacteristic->registerForNotify(notifyCallback);
  } else {
    Serial.println("Can't notify!");
  }
}

// Scan for server
void scanForServer() {
  Serial.println("Scanning...");
  
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  pBLEScan->start(5, false);
}

void setup() {
  Serial.begin(115200);
  Serial.println("BLE Coexistence Test Client");
  
  BLEDevice::init("");
  scanForServer();
}

void loop() {
  if (doScan) {
    scanForServer();
    doScan = false;
    delay(2000);
  }
  
  if (connected) {
    static unsigned long lastTimeCheck = 0;
    if (millis() - lastTimeCheck >= 10000) {  // Every 10 seconds
      lastTimeCheck = millis();
      unsigned long seconds = (millis() - connectionTime) / 1000;
      
      Serial.printf("Connected for %02u:%02u\n", 
                   (seconds % 3600) / 60, seconds % 60);
    }
  }
  
  delay(20);
}