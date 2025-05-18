#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <esp_bt.h>
#include <esp_gap_ble_api.h>

#define TEST_DURATION       60000     // 60 seconds per mode
#define DEVICE_NAME         "COEX_TEST"
#define SERVICE_UUID        "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define CHAR_UUID           "BEB5483E-36E1-4688-B7F5-EA07361B26A8"
#define PACKET_SIZE         20
#define SEND_INTERVAL       200

// Wi-Fi config
#define WIFI_SSID           "test"
#define WIFI_PASSWORD       "test"

enum InterferenceMode {
  // BLE_ONLY,
  // BLE_WITH_WIFI_SCAN,
  BLE_WITH_WIFI_ACTIVE
};

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
uint8_t testData[PACKET_SIZE];
unsigned long lastSendTime = 0;
unsigned long modeChangeTime = 0;
InterferenceMode currentMode = BLE_WITH_WIFI_ACTIVE;
uint8_t modeCount = 0;
uint32_t channelMapUpdateCount = 0;
uint32_t packetsSent = 0;
uint32_t packetErrors = 0;

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

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
  void onStatus(BLECharacteristic* pCharacteristic, Status status, uint32_t code) {
    Serial.printf("BLE status: %d, code=%u\n", (int)status, code);
    
    if ((int)status == 0 || ((int)status == 1 && code == 0)) {
      Serial.println("Indication succeeded");
    } 
    else {
      packetErrors++;
      Serial.println("Indication failed");
    }
  }
};

void startWiFi(bool fullConnection) {
  WiFi.mode(WIFI_MODE_STA);
  
  if (fullConnection) {
    Serial.println("Starting WiFi");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
      delay(500);
      Serial.print(".");
      timeout++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConnected");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nConnection failed - using scan instead");
      startWiFiScan();
    }
  } else {
    startWiFiScan();
  }
}

void startWiFiScan() {
  Serial.println("Starting WiFi scanning");
  
  xTaskCreate(
    [](void* parameter) {
      while(true) {
        Serial.println("WiFi scan...");
        int networks = WiFi.scanNetworks();
        Serial.printf("Found %d networks\n", networks);
        
        WiFi.scanDelete();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
      }
    },
    "WiFiScan",
    4096,
    NULL,
    1,
    NULL
  );
}

void stopWiFi() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi off");
}

void updateInterferenceMode() {
  currentMode = (InterferenceMode)(modeCount % 3);
  
  channelMapUpdateCount = 0;
  packetsSent = 0;
  packetErrors = 0;
  
  switch (currentMode) {
    // case BLE_ONLY:
    //   delay(5000);
    //   Serial.println("\n--- BLE ONLY ---");
    //   stopWiFi();
    //   break;
      
    // case BLE_WITH_WIFI_SCAN:
    //   delay(5000);
    //   Serial.println("\n--- BLE + WIFI SCAN ---");
    //   startWiFi(false);
    //   break;
      
    case BLE_WITH_WIFI_ACTIVE:
      delay(5000);
      Serial.println("\n--- BLE + ACTIVE WIFI ---");
      startWiFi(true);
      break;
  }
  
  modeChangeTime = millis();
}

void registerChannelMapCallback() {
  esp_ble_gap_register_callback([](esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    if (event == ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT) {
      Serial.println("Conn params updated");
    }
    else if (event == ESP_GAP_BLE_UPDATE_WHITELIST_COMPLETE_EVT) {
      channelMapUpdateCount++;
      Serial.printf("Channel map event (%u total)\n", channelMapUpdateCount);
    }
  });
}

void setup() {
  Serial.begin(115200);
  Serial.println("BLE Coexistence Test");

  Serial.println("ESP32 Arduino version:");
  Serial.println(ESP_ARDUINO_VERSION_MAJOR);
  Serial.println(ESP_ARDUINO_VERSION_MINOR);
  
  BLEDevice::init(DEVICE_NAME);
  
  registerChannelMapCallback();
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  pCharacteristic = pService->createCharacteristic(
                      CHAR_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
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
  
  for (int i = 0; i < PACKET_SIZE; i++) {
    testData[i] = i % 256;
  }
  
  updateInterferenceMode();
}

void loop() {
  if (millis() - modeChangeTime >= TEST_DURATION) {
    Serial.printf("Mode %d stats:\n", currentMode);
    Serial.printf("  Packets: %u\n", packetsSent);
    Serial.printf("  Errors: %u (%.2f%%)\n", 
                 packetErrors, (packetErrors * 100.0) / (packetsSent > 0 ? packetsSent : 1));
    Serial.printf("  Channel updates: %u\n", channelMapUpdateCount);
    
    modeCount++;
    updateInterferenceMode();
  }
  
  if (deviceConnected && (millis() - lastSendTime >= SEND_INTERVAL)) {
    lastSendTime = millis();
    
    testData[0]++;
    
    bool useIndication = (packetsSent % 5 == 0);
    
    if (useIndication) {
      pCharacteristic->setValue(testData, PACKET_SIZE);
      pCharacteristic->indicate();
      packetsSent++;
    } else {
      pCharacteristic->setValue(testData, PACKET_SIZE);
      pCharacteristic->notify();
      packetsSent++;
    }
    
    if (packetsSent % 50 == 0) {
      Serial.printf("Stats: %u pkts, %u errs, %u chan updates, Mode: %d\n", 
                   packetsSent, packetErrors, channelMapUpdateCount, currentMode);
    }
  }
  
  if (currentMode == BLE_WITH_WIFI_ACTIVE && WiFi.status() == WL_CONNECTED) {
    static unsigned long lastWifiPing = 0;
    
    if (millis() - lastWifiPing >= 1000) {
      lastWifiPing = millis();
      
      WiFiClient client;
      if (client.connect("8.8.8.8", 80)) {
        client.println("GET / HTTP/1.1");
        client.println("Host: 8.8.8.8");
        client.println("Connection: close");
        client.println();
        client.stop();
      }
    }
  }
  
  delay(20);
}