/**
 * ESP32-C6 BLE Central for Power Consumption Testing
 * 
 * Works with Power Profiler Kit 2 for current measurement.
 */

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <esp_sleep.h>

#define TARGET_DEVICE_NAME "ESP32-C6_BLE_TEST"
#define TEST_SERVICE_UUID           "180D"
#define CONTROL_CHAR_UUID           "2A37"
#define DATA_CHAR_UUID              "2A38"
#define SCENARIO_DURATION           60000
#define NUM_SCENARIOS               8
#define SCAN_DURATION               5

BLEScan* pBLEScan = NULL;
BLEClient* pClient = NULL;
BLERemoteService* pRemoteService = NULL;
BLERemoteCharacteristic* pControlCharacteristic = NULL;
BLERemoteCharacteristic* pDataCharacteristic = NULL;
bool deviceConnected = false;
BLEAddress* targetAddress = NULL;
unsigned long scanStartTime = 0;
unsigned long scenarioStartTime = 0;
unsigned long lastDataTime = 0;
uint32_t packetsReceived = 0;
uint32_t totalBytesReceived = 0;
int currentScenario = 0;
bool scanInProgress = false;
bool connectionRequested = false;

struct TestScenario {
  bool activeScanning;
  uint16_t scanInterval;
  uint16_t scanWindow;
  uint16_t connInterval;
  bool useSleepMode;
  uint16_t sleepDuration;
  const char* description;
};

TestScenario scenarios[NUM_SCENARIOS] = {
  // Scenario 1: Active scanning, fast scan rate
  {
    .activeScanning = true,
    .scanInterval = 100,
    .scanWindow = 50,
    .connInterval = 6,
    .useSleepMode = false,
    .sleepDuration = 0,
    .description = "Active scanning, fast scan rate"
  },
  
  // Scenario 2: Passive scanning, fast scan rate
  {
    .activeScanning = false,
    .scanInterval = 100,
    .scanWindow = 50,
    .connInterval = 6,
    .useSleepMode = false,
    .sleepDuration = 0,
    .description = "Passive scanning, fast scan rate"
  },
  
  // Scenario 3: Active scanning, slow scan rate
  {
    .activeScanning = true,
    .scanInterval = 1000,
    .scanWindow = 100,
    .connInterval = 6,
    .useSleepMode = false,
    .sleepDuration = 0,
    .description = "Active scanning, slow scan rate"
  },

    // Scenario 3.1: Passive scanning, slow scan rate
  {
    .activeScanning = false,
    .scanInterval = 1000,
    .scanWindow = 100,
    .connInterval = 6,
    .useSleepMode = false,
    .sleepDuration = 0,
    .description = "Passive scanning, slow scan rate"
  },
  
  // Scenario 4: Fast connection interval
  {
    .activeScanning = true,
    .scanInterval = 100,
    .scanWindow = 50,
    .connInterval = 6,
    .useSleepMode = false,
    .sleepDuration = 0,
    .description = "Fast connection interval (7.5ms)"
  },
  
  // Scenario 5: Slow connection interval
  {
    .activeScanning = true,
    .scanInterval = 100,
    .scanWindow = 50,
    .connInterval = 800,
    .useSleepMode = false,
    .sleepDuration = 0,
    .description = "Slow connection interval (1000ms)"
  },
  
  
  // Scenario 6: Short sleep between scans
  {
    .activeScanning = true,
    .scanInterval = 500,
    .scanWindow = 100,
    .connInterval = 80,
    .useSleepMode = true,
    .sleepDuration = 1000,
    .description = "Short sleep between scans (1s)"
  },
  
  // Scenario 7: Long sleep between scans
  {
    .activeScanning = true,
    .scanInterval = 500,
    .scanWindow = 100,
    .connInterval = 80,
    .useSleepMode = true,
    .sleepDuration = 5000,
    .description = "Long sleep between scans (5s)"
  }
};

class MyScanCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("Found device: ");
    Serial.println(advertisedDevice.toString().c_str());
    
    if (advertisedDevice.getName() == TARGET_DEVICE_NAME) {
      Serial.print("Found target device: ");
      Serial.println(advertisedDevice.toString().c_str());
      
      advertisedDevice.getScan()->stop();
      scanInProgress = false;
      
      if (targetAddress != NULL) {
        delete targetAddress;
      }
      targetAddress = new BLEAddress(advertisedDevice.getAddress());
      
      connectionRequested = true;
      
      unsigned long scanTime = millis() - scanStartTime;
      Serial.printf("Scan completed in %lu ms\n", scanTime);
    }
  }
};

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    deviceConnected = true;
    Serial.println("Connected to peripheral");
    
    TestScenario& scenario = scenarios[currentScenario];
    
    esp_ble_conn_update_params_t connParams;
    connParams.min_int = scenario.connInterval;
    connParams.max_int = scenario.connInterval;
    connParams.latency = 0;
    connParams.timeout = 400;
    
    esp_ble_gap_update_conn_params(&connParams);
    
    Serial.printf("Requested connection interval: %.2f ms\n", scenario.connInterval * 1.25);
  }

  void onDisconnect(BLEClient* pclient) {
    deviceConnected = false;
    Serial.println("Disconnected from peripheral");
  }
};

static void notifyCallback(BLERemoteCharacteristic* pRemoteCharacteristic, 
                           uint8_t* pData, size_t length, bool isNotify) {
  packetsReceived++;
  totalBytesReceived += length;
  lastDataTime = millis();
  
  Serial.printf("Received data: %u bytes (total packets: %u, total bytes: %u)\n", 
                length, packetsReceived, totalBytesReceived);
}

void startScanWithCurrentSettings() {
  if (scanInProgress) {
    return;
  }
  
  TestScenario& scenario = scenarios[currentScenario];
  
  Serial.println("Starting BLE scan...");
  scanStartTime = millis();
  scanInProgress = true;
  
  pBLEScan->setActiveScan(scenario.activeScanning);
  
  pBLEScan->setInterval((uint16_t)(scenario.scanInterval * 1.6));
  pBLEScan->setWindow((uint16_t)(scenario.scanWindow * 1.6));
  
  pBLEScan->start(SCAN_DURATION, false);
  
  Serial.printf("Scanning with: active=%d, interval=%d ms, window=%d ms\n", 
               scenario.activeScanning, scenario.scanInterval, scenario.scanWindow);
}

bool connectToPeripheral() {
  if (targetAddress == NULL) {
    Serial.println("No target address found");
    return false;
  }
  
  Serial.printf("Connecting to %s...\n", targetAddress->toString().c_str());
  
  if (pClient != NULL) {
    delete pClient;
  }
  
  pClient = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());
  
  if (!pClient->connect(*targetAddress)) {
    Serial.println("Connection failed");
    return false;
  }
  
  Serial.println("Connected to server");
  
  pRemoteService = pClient->getService(BLEUUID(TEST_SERVICE_UUID));
  if (pRemoteService == nullptr) {
    Serial.println("Failed to find test service");
    pClient->disconnect();
    return false;
  }
  
  pControlCharacteristic = pRemoteService->getCharacteristic(BLEUUID(CONTROL_CHAR_UUID));
  if (pControlCharacteristic == nullptr) {
    Serial.println("Failed to find control characteristic");
    pClient->disconnect();
    return false;
  }
  
  pDataCharacteristic = pRemoteService->getCharacteristic(BLEUUID(DATA_CHAR_UUID));
  if (pDataCharacteristic == nullptr) {
    Serial.println("Failed to find data characteristic");
    pClient->disconnect();
    return false;
  }
  
  if (pDataCharacteristic->canNotify()) {
    pDataCharacteristic->registerForNotify(notifyCallback);
  }
  
  TestScenario& scenario = scenarios[currentScenario];
  
  esp_ble_conn_update_params_t connParams;
  connParams.min_int = scenario.connInterval;
  connParams.max_int = scenario.connInterval;
  connParams.latency = 0;
  connParams.timeout = 400;
  
  esp_ble_gap_update_conn_params(&connParams);
  
  return true;
}

void sendCommand(uint8_t cmd, uint8_t val) {
  if (deviceConnected && pControlCharacteristic != nullptr) {
    uint8_t cmdData[2] = {cmd, val};
    pControlCharacteristic->writeValue(cmdData, 2);
    Serial.printf("Sent command: %02X %02X\n", cmd, val);
  } else {
    Serial.println("Cannot send command - not connected");
  }
}

void moveToNextScenario() {
  if (deviceConnected && pClient != NULL) {
    pClient->disconnect();
    deviceConnected = false;
    delay(500);
  }
  
  scanInProgress = false;
  connectionRequested = false;
  
  currentScenario = (currentScenario + 1) % NUM_SCENARIOS;
  Serial.printf("\n--- Switching to Scenario %d ---\n", currentScenario + 1);
  Serial.printf("Description: %s\n", scenarios[currentScenario].description);
  
  scenarioStartTime = millis();
  packetsReceived = 0;
  totalBytesReceived = 0;
  
  startScanWithCurrentSettings();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\nESP32-C6 BLE Central Test Starting");
  
  BLEDevice::init("ESP32-C6_BLE_CENTRAL");
  
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyScanCallbacks());
  
  scenarioStartTime = millis();
  Serial.printf("--- Starting Scenario 1 ---\n");
  Serial.printf("Description: %s\n", scenarios[currentScenario].description);
  startScanWithCurrentSettings();
  
  Serial.println("BLE central ready - running automatic test sequence");
  Serial.println("Each scenario will run for 60 seconds.");
}

void loop() {
  if (millis() - scenarioStartTime >= SCENARIO_DURATION) {
    moveToNextScenario();
  }
  
  if (connectionRequested && !deviceConnected && !scanInProgress) {
    connectionRequested = false;
    connectToPeripheral();
  }
  
  if (!scanInProgress && !deviceConnected && (millis() - scanStartTime > SCAN_DURATION * 1000 + 1000)) {
    startScanWithCurrentSettings();
  }
  
  if (deviceConnected && (millis() - lastDataTime > 5000)) {
    sendCommand(0xFF, 0xFF);
    lastDataTime = millis();
  }
  
  TestScenario& scenario = scenarios[currentScenario];
  if (scenario.useSleepMode && !deviceConnected && !scanInProgress) {
    Serial.printf("Entering light sleep for %d ms\n", scenario.sleepDuration);
    delay(10);
    
    esp_sleep_enable_timer_wakeup(scenario.sleepDuration * 1000);
    esp_light_sleep_start();
    
    Serial.println("Woke up from light sleep");
  }
  
  delay(10);
}