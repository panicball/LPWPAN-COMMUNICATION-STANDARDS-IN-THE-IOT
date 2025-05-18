#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLESecurity.h>
#include <esp_sleep.h>

#define TEST_DURATION   60000
#define DEVICE_NAME     "SEC_TEST"
#define SERVICE_UUID    "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define CHAR_UUID       "BEB5483E-36E1-4688-B7F5-EA07361B26A8"
#define PACKET_SIZE     100
#define SEND_INTERVAL   200
#define PASSKEY         123456

enum SecurityMode {
  NO_SECURITY,
  ENCRYPTION_NO_MITM,
  ENCRYPTION_WITH_MITM,
  BONDED_RECONNECTION
};

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLESecurity* pSecurity = NULL;
bool deviceConnected = false;
uint8_t testData[PACKET_SIZE];
unsigned long lastSendTime = 0;
unsigned long securityChangeTime = 0;
SecurityMode currentMode = NO_SECURITY;
uint8_t securityModeCount = 0;
bool bondingOccurred = false;

void updateSecurityMode();

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Connected");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Disconnected");
    
    pServer->startAdvertising();
    
    if (currentMode == BONDED_RECONNECTION && bondingOccurred) {
      Serial.println("Bonded reconnection complete");
      
      securityModeCount++;
      updateSecurityMode();
    }
  }
};

class MySecurity : public BLESecurityCallbacks {
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
        Serial.println("Bonding complete");
        bondingOccurred = true;
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

void updateSecurityMode() {
  currentMode = (SecurityMode)(securityModeCount % 4);
  
  esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_NO_BOND;
  uint8_t key_size = 16;
  uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  bool initateSecurityRequest = false;
  
  switch (currentMode) {
    case NO_SECURITY:
      Serial.println("\n--- NO SECURITY ---");
      iocap = ESP_IO_CAP_NONE;
      auth_req = ESP_LE_AUTH_NO_BOND;
      initateSecurityRequest = false;
      bondingOccurred = false;
      break;
      
    case ENCRYPTION_NO_MITM:
      Serial.println("\n--- ENCRYPTION (No MITM) ---");
      iocap = ESP_IO_CAP_NONE;
      auth_req = ESP_LE_AUTH_BOND;
      initateSecurityRequest = true;
      bondingOccurred = false;
      break;
      
    case ENCRYPTION_WITH_MITM:
      Serial.println("\n--- ENCRYPTION (MITM) ---");
      iocap = ESP_IO_CAP_OUT;
      auth_req = ESP_LE_AUTH_BOND | ESP_LE_AUTH_REQ_MITM;
      initateSecurityRequest = true;
      bondingOccurred = false;
      break;
      
    case BONDED_RECONNECTION:
      Serial.println("\n--- BONDED RECONNECTION ---");
      iocap = ESP_IO_CAP_OUT;
      auth_req = ESP_LE_AUTH_BOND;
      initateSecurityRequest = true;
      break;
  }
  
  pSecurity->setCapability(iocap);
  pSecurity->setAuthenticationMode(auth_req);
  pSecurity->setKeySize(key_size);
  pSecurity->setInitEncryptionKey(init_key);
  pSecurity->setRespEncryptionKey(rsp_key);
  
  if (initateSecurityRequest && deviceConnected) {
    Serial.println("Security request initiated");
    
    esp_ble_sec_act_t secAct = ESP_BLE_SEC_ENCRYPT;
    if (auth_req & ESP_LE_AUTH_REQ_MITM) {
      secAct = ESP_BLE_SEC_ENCRYPT_MITM;
    }
    
    pSecurity->setAuthenticationMode(auth_req);
    pSecurity->setCapability(iocap);
    
    Serial.printf("Security mode: auth=%d, iocap=%d\n", auth_req, iocap);
  }
  
  securityChangeTime = millis();
}

void setup() {
  Serial.begin(115200);
  Serial.println("BLE Security Test");

  BLEDevice::init(DEVICE_NAME);
  
  pSecurity = new BLESecurity();
  BLEDevice::setSecurityCallbacks(new MySecurity());
  
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
  
  updateSecurityMode();
  
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
}

void loop() {
  if (millis() - securityChangeTime >= TEST_DURATION && currentMode != BONDED_RECONNECTION) {
    securityModeCount++;
    updateSecurityMode();
  }
  
  if (deviceConnected && (millis() - lastSendTime >= SEND_INTERVAL)) {
    lastSendTime = millis();
    
    testData[0]++;
    
    pCharacteristic->setValue(testData, PACKET_SIZE);
    pCharacteristic->notify();
    
    Serial.printf("Sent: %d bytes (Mode: %d)\n", PACKET_SIZE, currentMode);
  }
  
  delay(20);
}