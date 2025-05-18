#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_gap_ble_api.h>
#include <esp_sleep.h>

#define TEST_DURATION   60000     // 60 seconds per mode
#define DEVICE_NAME     "PHY_TEST"
#define SERVICE_UUID    "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define CHAR_UUID       "BEB5483E-36E1-4688-B7F5-EA07361B26A8"
#define PACKET_SIZE     100
#define SEND_INTERVAL   100

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
uint8_t testData[PACKET_SIZE];
unsigned long lastSendTime = 0;
unsigned long phyChangeTime = 0;
bool using2MPHY = false;
bool phyChangeRequested = false;

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Connected");
    
    esp_ble_gap_set_preferred_default_phy(ESP_BLE_GAP_PHY_1M, ESP_BLE_GAP_PHY_1M);
    using2MPHY = false;
    phyChangeTime = millis();
    Serial.println("Using 1M PHY");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Disconnected");
    pServer->startAdvertising();
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("BLE PHY Test");

  BLEDevice::init(DEVICE_NAME);
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  pCharacteristic = pService->createCharacteristic(
                      CHAR_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );
  pCharacteristic->addDescriptor(new BLE2902());
  
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
  
  phyChangeTime = millis();
}

void loop() {
  if (deviceConnected && (millis() - phyChangeTime >= TEST_DURATION) && !phyChangeRequested) {
    using2MPHY = !using2MPHY;
    
    if (using2MPHY) {
      Serial.println("Switch to 2M PHY");
      esp_ble_gap_set_preferred_default_phy(ESP_BLE_GAP_PHY_2M, ESP_BLE_GAP_PHY_2M);
    } else {
      Serial.println("Switch to 1M PHY");
      esp_ble_gap_set_preferred_default_phy(ESP_BLE_GAP_PHY_1M, ESP_BLE_GAP_PHY_1M);
    }
    
    phyChangeRequested = true;
    
    esp_ble_gap_register_callback([](esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
      if (event == ESP_GAP_BLE_PHY_UPDATE_COMPLETE_EVT) {
        Serial.printf("PHY updated: TX %d, RX %d\n", 
                    param->phy_update.tx_phy, param->phy_update.rx_phy);
        phyChangeRequested = false;
        phyChangeTime = millis();
      }
    });
  }
  
  if (deviceConnected && (millis() - lastSendTime >= SEND_INTERVAL)) {
    lastSendTime = millis();
    
    testData[0]++;
    
    pCharacteristic->setValue(testData, PACKET_SIZE);
    pCharacteristic->notify();
    
    Serial.printf("Sent %d bytes (%s PHY)\n", 
                 PACKET_SIZE, using2MPHY ? "2M" : "1M");
  }
  
  delay(20);
}