#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define TARGET_DEVICE_NAME    "DUTY_TEST"
#define SERVICE_UUID          "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define DATA_CHAR_UUID        "BEB5483E-36E1-4688-B7F5-EA07361B26A8"
#define SENSOR_CHAR_UUID      "BEB5483E-36E1-4688-B7F5-EA07361B26A9"

#define MAX_SAMPLES           1000
#define MODE_DETECTION_SAMPLES 20
#define RECONNECT_DELAY       5000

unsigned long packetTimes[MAX_SAMPLES];
uint8_t packetValues[MAX_SAMPLES];
unsigned long sensorTimes[MAX_SAMPLES];
uint16_t sensorValues[MAX_SAMPLES];
int packetCount = 0;
int sensorCount = 0;
int currentMode = -1;
unsigned long lastModeChangeTime = 0;
unsigned long connectionStartTime = 0;

bool isConnected = false;
bool scanComplete = false;

BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pDataCharacteristic = nullptr;
BLERemoteCharacteristic* pSensorCharacteristic = nullptr;
BLEAdvertisedDevice* myDevice = nullptr;
BLEScan* pBLEScan = nullptr;

const char* modeNames[] = {
  "CONTINUOUS",
  "BATCHING",
  "POLLING"
};

void detectTransmissionMode();
void updateTransmissionMode(int newMode);
void printDataSummary();

static void dataNotifyCallback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic, 
    uint8_t* pData, 
    size_t length, 
    bool isNotify) {
      
  unsigned long timestamp = millis();
  
  bool isTestCaseEnd = true;
  for (int i = 2; i < length; i++) {
    if (pData[i] != 0xFE) {
      isTestCaseEnd = false;
      break;
    }
  }
  
  bool isTestComplete = true;
  for (int i = 0; i < length; i++) {
    if (pData[i] != 0xFF) {
      isTestComplete = false;
      break;
    }
  }
  
  if (isTestCaseEnd) {
    uint8_t finishedMode = pData[0];
    uint8_t modeCount = pData[1];
    
    Serial.println("\n====================");
    Serial.printf("TEST COMPLETE: Mode %d (%s) - Test %d\n", 
                 finishedMode, modeNames[finishedMode], modeCount);
    Serial.println("====================\n");
    
    printDataSummary();
    
    packetCount = 0;
    sensorCount = 0;
    return;
  }
  
  if (isTestComplete) {
    Serial.println("\n********************");
    Serial.println("ALL TESTS COMPLETE");
    Serial.println("********************\n");
    
    printDataSummary();
    return;
  }
  
  uint8_t seqNum = pData[0];
  
  if (packetCount < MAX_SAMPLES) {
    packetTimes[packetCount] = timestamp;
    packetValues[packetCount] = seqNum;
    packetCount++;
  }
  
  unsigned long interval = 0;
  if (packetCount > 1) {
    interval = packetTimes[packetCount-1] - packetTimes[packetCount-2];
  }
  
  Serial.printf("DATA: %u, %d bytes, %lu ms\n", seqNum, length, interval);
  
  detectTransmissionMode();
}

static void sensorNotifyCallback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic, 
    uint8_t* pData, 
    size_t length, 
    bool isNotify) {
      
  unsigned long timestamp = millis();
  
  uint16_t sensorValue = (pData[0] << 8) | pData[1];
  
  if (sensorCount < MAX_SAMPLES) {
    sensorTimes[sensorCount] = timestamp;
    sensorValues[sensorCount] = sensorValue;
    sensorCount++;
  }
  
  unsigned long interval = 0;
  if (sensorCount > 1) {
    interval = sensorTimes[sensorCount-1] - sensorTimes[sensorCount-2];
  }
  
  Serial.printf("SENSOR: %u, %lu ms\n", sensorValue, interval);
  
  updateTransmissionMode(2);
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    isConnected = true;
    connectionStartTime = millis();
    Serial.println("Connected");
  }

  void onDisconnect(BLEClient* pclient) {
    isConnected = false;
    Serial.println("Disconnected");
    
    printDataSummary();
    
    packetCount = 0;
    sensorCount = 0;
    currentMode = -1;
  }
};

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == TARGET_DEVICE_NAME) {
      Serial.printf("Found target: %s\n", advertisedDevice.toString().c_str());
      
      advertisedDevice.getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      scanComplete = true;
    }
  }
};

void detectTransmissionMode() {
  if (packetCount < MODE_DETECTION_SAMPLES) {
    return;
  }
  
  unsigned long intervals[MODE_DETECTION_SAMPLES-1];
  for (int i = 0; i < MODE_DETECTION_SAMPLES-1; i++) {
    intervals[i] = packetTimes[packetCount-i-1] - packetTimes[packetCount-i-2];
  }
  
  unsigned long maxInterval = 0;
  unsigned long minInterval = UINT32_MAX;
  unsigned long sumIntervals = 0;
  
  for (int i = 0; i < MODE_DETECTION_SAMPLES-1; i++) {
    if (intervals[i] > maxInterval) maxInterval = intervals[i];
    if (intervals[i] < minInterval) minInterval = intervals[i];
    sumIntervals += intervals[i];
  }
  
  unsigned long avgInterval = sumIntervals / (MODE_DETECTION_SAMPLES-1);
  
  int detectedMode = 0;
  
  if (maxInterval > 1000 && minInterval < 50) {
    detectedMode = 1; // BATCHING
  }
  else if (avgInterval > 10000) {
    detectedMode = 2; // POLLING
  }
  
  updateTransmissionMode(detectedMode);
}

void updateTransmissionMode(int newMode) {
  unsigned long now = millis();
  
  if (currentMode != newMode) {
    if (currentMode >= 0) {
      unsigned long duration = now - lastModeChangeTime;
      Serial.printf("\n--- Mode %d (%s): %lu s ---\n\n", 
                    currentMode, modeNames[currentMode], duration/1000);
    }
    
    currentMode = newMode;
    lastModeChangeTime = now;
    
    Serial.printf("\n=== MODE CHANGE: %s ===\n\n", modeNames[currentMode]);
  }
}

void printDataSummary() {
  Serial.println("\n=== SUMMARY ===");
  
  if (packetCount > 0) {
    unsigned long totalTime = packetTimes[packetCount-1] - packetTimes[0];
    float packetsPerSecond = (float)packetCount / (totalTime / 1000.0);
    
    Serial.println("\nData Packets:");
    Serial.printf("Total: %d\n", packetCount);
    Serial.printf("Duration: %lu ms\n", totalTime);
    Serial.printf("Rate: %.2f pkt/s\n", packetsPerSecond);
    
    if (packetCount > 1) {
      unsigned long minInterval = UINT32_MAX;
      unsigned long maxInterval = 0;
      unsigned long sumIntervals = 0;
      
      for (int i = 1; i < packetCount; i++) {
        unsigned long interval = packetTimes[i] - packetTimes[i-1];
        if (interval < minInterval) minInterval = interval;
        if (interval > maxInterval) maxInterval = interval;
        sumIntervals += interval;
      }
      
      float avgInterval = (float)sumIntervals / (packetCount - 1);
      
      Serial.printf("Avg interval: %.2f ms\n", avgInterval);
      Serial.printf("Min: %lu ms\n", minInterval);
      Serial.printf("Max: %lu ms\n", maxInterval);
    }
  }
  
  if (sensorCount > 0) {
    Serial.println("\nSensor Readings:");
    Serial.printf("Total: %d\n", sensorCount);
    
    if (sensorCount > 1) {
      unsigned long totalTime = sensorTimes[sensorCount-1] - sensorTimes[0];
      float readingsPerSecond = (float)sensorCount / (totalTime / 1000.0);
      
      Serial.printf("Duration: %lu ms\n", totalTime);
      Serial.printf("Rate: %.2f rdg/s\n", readingsPerSecond);
      
      unsigned long minInterval = UINT32_MAX;
      unsigned long maxInterval = 0;
      unsigned long sumIntervals = 0;
      
      for (int i = 1; i < sensorCount; i++) {
        unsigned long interval = sensorTimes[i] - sensorTimes[i-1];
        if (interval < minInterval) minInterval = interval;
        if (interval > maxInterval) maxInterval = interval;
        sumIntervals += interval;
      }
      
      float avgInterval = (float)sumIntervals / (sensorCount - 1);
      
      Serial.printf("Avg interval: %.2f ms\n", avgInterval);
      Serial.printf("Min: %lu ms\n", minInterval);
      Serial.printf("Max: %lu ms\n", maxInterval);
    }
  }
  
  Serial.println();
}

bool connectToServer() {
  Serial.printf("Connecting to %s\n", myDevice->getAddress().toString().c_str());
  
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
  
  if (!pClient->connect(myDevice)) {
    Serial.println("Connection failed");
    return false;
  }
  
  BLERemoteService* pRemoteService = pClient->getService(BLEUUID(SERVICE_UUID));
  if (pRemoteService == nullptr) {
    Serial.println("Service not found");
    pClient->disconnect();
    return false;
  }
  
  pDataCharacteristic = pRemoteService->getCharacteristic(BLEUUID(DATA_CHAR_UUID));
  if (pDataCharacteristic == nullptr) {
    Serial.println("Data char not found");
    pClient->disconnect();
    return false;
  }
  
  pSensorCharacteristic = pRemoteService->getCharacteristic(BLEUUID(SENSOR_CHAR_UUID));
  if (pSensorCharacteristic == nullptr) {
    Serial.println("Sensor char not found");
    pClient->disconnect();
    return false;
  }
  
  if (pDataCharacteristic->canNotify()) {
    pDataCharacteristic->registerForNotify(dataNotifyCallback);
  }
  
  if (pSensorCharacteristic->canNotify()) {
    pSensorCharacteristic->registerForNotify(sensorNotifyCallback);
  }
  
  packetCount = 0;
  sensorCount = 0;
  currentMode = -1;
  lastModeChangeTime = millis();
  
  isConnected = true;
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("BLE Duty Cycle Client");
  
  BLEDevice::init("BLE_DUTY_CLIENT");
  
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  
  Serial.printf("Scanning for: %s\n", TARGET_DEVICE_NAME);
}

void loop() {
  if (!isConnected && !scanComplete) {
    Serial.println("Scanning...");
    pBLEScan->start(5, false);
    delay(5000);
  }
  
  if (scanComplete && !isConnected) {
    static unsigned long lastConnectAttempt = 0;
    unsigned long now = millis();
    
    if (now - lastConnectAttempt > RECONNECT_DELAY) {
      lastConnectAttempt = now;
      connectToServer();
    }
  }
  
  if (isConnected) {
    static unsigned long lastStatusUpdate = 0;
    unsigned long now = millis();
    
    if (now - lastStatusUpdate > 10000) {
      lastStatusUpdate = now;
      
      unsigned long connectedTime = (now - connectionStartTime) / 1000;
      Serial.printf("\nStatus: %lu s connected\n", connectedTime);
      Serial.printf("Data: %d pkts, Sensor: %d rdgs\n", packetCount, sensorCount);
      
      if (currentMode >= 0) {
        Serial.printf("Mode: %s\n\n", modeNames[currentMode]);
      }
    }
  }
  
  delay(100);
}