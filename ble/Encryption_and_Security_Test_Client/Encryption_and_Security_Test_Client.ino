#include <BLEDevice.h>
#include <BLESecurity.h>

#define SERVER_NAME     "SEC_TEST"
#define SERVICE_UUID    "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define CHAR_UUID       "BEB5483E-36E1-4688-B7F5-EA07361B26A8"
#define PASSKEY         123456
#define SCAN_DURATION   5
#define READ_INTERVAL   2000

BLEClient* pClient = nullptr;
BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
BLESecurity* pSecurity = nullptr;
BLEAdvertisedDevice* serverDevice = nullptr;

bool connected = false;
bool doScan = true;
uint32_t lastReadTime = 0;
uint32_t packetsReceived = 0;
uint32_t bytesReceived = 0;

class ClientSecurity : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() {
    Serial.println("PassKey request");
    return PASSKEY;
  }

  void onPassKeyNotify(uint32_t pass_key) {
    Serial.printf("PassKey: %06d\n", pass_key);
  }

  bool onConfirmPIN(uint32_t pass_key) {
    Serial.printf("Confirm PIN: %06d\n", pass_key);
    return true;
  }

  bool onSecurityRequest() {
    Serial.println("Security request");
    return true;
  }

  void onAuthenticationComplete(esp_ble_auth_cmpl_t auth_cmpl) {
    if (auth_cmpl.success) {
      Serial.println("Auth success");
      
      if (auth_cmpl.key_present) {
        Serial.println("Bonding completed");
      }
      
      Serial.printf("Mode: %s\n", 
                  (auth_cmpl.auth_mode == ESP_LE_AUTH_NO_BOND) ? "No Bond" : 
                  (auth_cmpl.auth_mode == ESP_LE_AUTH_BOND) ? "Bond" : 
                  (auth_cmpl.auth_mode == ESP_LE_AUTH_REQ_MITM) ? "MITM" : "Bond+MITM");
    } else {
      Serial.printf("Auth failed: %d\n", auth_cmpl.fail_reason);
    }
  }
};

void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
                    uint8_t* pData, size_t length, bool isNotify) {
  packetsReceived++;
  bytesReceived += length;
  
  Serial.printf("Notify: %d bytes, seq: %d\n", length, pData[0]);
}

class AdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.getName() == SERVER_NAME) {
      Serial.printf("Found server: %s\n", advertisedDevice.toString().c_str());
      
      BLEDevice::getScan()->stop();
      serverDevice = new BLEAdvertisedDevice(advertisedDevice);
      doScan = false;
    }
  }
};

bool connectToServer() {
  Serial.printf("Connecting to %s\n", serverDevice->getAddress().toString().c_str());
  
  pClient = BLEDevice::createClient();
  
  if (!pClient->connect(serverDevice)) {
    Serial.println("Connection failed");
    return false;
  }
  
  Serial.println("Connected");
  
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
  
  if(pRemoteCharacteristic->canRead()) {
    String value = pRemoteCharacteristic->readValue();
    Serial.printf("Initial read: %d bytes\n", value.length());
  }
  
  if(pRemoteCharacteristic->canNotify()) {
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    Serial.println("Notifications enabled");
  }
  
  connected = true;
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("BLE Security Client");
  
  BLEDevice::init("SEC_TEST_CLIENT");
  
  pSecurity = new BLESecurity();
  BLEDevice::setSecurityCallbacks(new ClientSecurity());
  
  pSecurity->setCapability(ESP_IO_CAP_OUT);
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND | ESP_LE_AUTH_REQ_MITM);
  pSecurity->setKeySize(16);
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  pSecurity->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->start(SCAN_DURATION, false);
  
  Serial.println("Scanning...");
}

void loop() {
  if (doScan) {
    BLEDevice::getScan()->start(SCAN_DURATION, false);
    Serial.println("Scanning...");
    delay(SCAN_DURATION * 1000);
  }
  
  if (!connected && serverDevice) {
    if (connectToServer()) {
      Serial.println("Connection successful");
    } else {
      Serial.println("Connection failed, retrying...");
      delay(5000);
    }
  }
  
  if (connected && (millis() - lastReadTime > READ_INTERVAL)) {
    lastReadTime = millis();
    
    if (pRemoteCharacteristic && pRemoteCharacteristic->canRead()) {
      String value = pRemoteCharacteristic->readValue();
      Serial.printf("Read: %d bytes, pkts: %d, total: %d\n", 
                   value.length(), packetsReceived, bytesReceived);
      
      if (pRemoteCharacteristic->canWrite()) {
        uint8_t writeBuf[4] = {0x01, 0x02, 0x03, 0x04};
        pRemoteCharacteristic->writeValue(writeBuf, 4);
        Serial.println("Wrote 4 bytes");
      }
    }
  }
  
  if (!pClient || !pClient->isConnected()) {
    if (connected) {
      connected = false;
      Serial.println("Disconnected");
      
      packetsReceived = 0;
      bytesReceived = 0;
      
      if (pClient) {
        pClient->disconnect();
        delete pClient;
        pClient = nullptr;
      }
      
      if (serverDevice) {
        delete serverDevice;
        serverDevice = nullptr;
      }
      
      pRemoteCharacteristic = nullptr;
      doScan = true;
    }
  }
  
  delay(100);
}