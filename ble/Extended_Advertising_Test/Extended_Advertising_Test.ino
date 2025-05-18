#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_bt.h>
#include <esp_gap_ble_api.h>
#include <esp_sleep.h>

#define TEST_DURATION   60000
#define DEVICE_NAME     "EXT_ADV_TEST"
#define SERVICE_UUID    "4FAFC201-1FB5-459E-8FCC-C5C9C331914B"
#define CHAR_UUID       "BEB5483E-36E1-4688-B7F5-EA07361B26A8"
#define ADV_INSTANCE_ID 0

enum AdvertisingMode {
  LEGACY_ADV,
  EXTENDED_ADV_SMALL,
  EXTENDED_ADV_LARGE
};

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
BLEAdvertising* pAdvertising = NULL;
bool deviceConnected = false;
unsigned long modeChangeTime = 0;
AdvertisingMode currentMode = LEGACY_ADV;
uint8_t modeCount = 0;
bool extAdvConfigured = false;

void startAdvertisingWithCurrentMode();
void startLegacyAdvertising();
void startExtendedAdvertising(bool largePDU);
void stopAdvertising();

void my_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_EXT_ADV_SET_RAND_ADDR_COMPLETE_EVT:
      Serial.printf("Ext adv addr set: %d\n", param->ext_adv_set_rand_addr.status);
      break;
      
    case ESP_GAP_BLE_EXT_ADV_SET_PARAMS_COMPLETE_EVT:
      Serial.printf("Ext adv params: %d\n", param->ext_adv_set_params.status);
      break;
    
    case ESP_GAP_BLE_EXT_ADV_DATA_SET_COMPLETE_EVT:
      Serial.printf("Ext adv data: %d\n", param->ext_adv_data_set.status);
      break;
    
    case ESP_GAP_BLE_EXT_SCAN_RSP_DATA_SET_COMPLETE_EVT:
      Serial.printf("Ext scan rsp: %d\n", param->scan_rsp_set.status);
      break;
      
    case ESP_GAP_BLE_EXT_ADV_START_COMPLETE_EVT:
      Serial.printf("Ext adv start: %d\n", param->ext_adv_start.status);
      break;
      
    case ESP_GAP_BLE_EXT_ADV_STOP_COMPLETE_EVT:
      Serial.printf("Ext adv stop: %d\n", param->ext_adv_stop.status);
      break;
      
    default:
      break;
  }
}

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    Serial.println("Connected");
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    Serial.println("Disconnected");
    startAdvertisingWithCurrentMode();
  }
};

void startAdvertisingWithCurrentMode() {
  stopAdvertising();
  delay(200);
  
  switch (currentMode) {
    case LEGACY_ADV:
      startLegacyAdvertising();
      break;
      
    case EXTENDED_ADV_SMALL:
      startExtendedAdvertising(false);
      break;
      
    case EXTENDED_ADV_LARGE:
      startExtendedAdvertising(true);
      break;
  }
}

void stopAdvertising() {
  BLEDevice::getAdvertising()->stop();
  
  if (currentMode != LEGACY_ADV && extAdvConfigured) {
    uint8_t inst_id = ADV_INSTANCE_ID;
    esp_ble_gap_ext_adv_stop(1, &inst_id);
  }
  
  delay(200);
}

void startLegacyAdvertising() {
  Serial.println("Starting legacy adv");
  
  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  
  pAdvertising->setMinInterval(0x20);
  pAdvertising->setMaxInterval(0x40);
  
  BLEDevice::startAdvertising();
}

void startExtendedAdvertising(bool largePDU) {
  Serial.printf("Starting ext adv (%s payload)\n", largePDU ? "large" : "small");

  esp_ble_gap_ext_adv_params_t ext_adv_params = {};
  
  ext_adv_params.type = ESP_BLE_GAP_SET_EXT_ADV_PROP_CONNECTABLE;
  ext_adv_params.interval_min = 0x30;
  ext_adv_params.interval_max = 0x60;
  ext_adv_params.channel_map = ADV_CHNL_ALL;
  ext_adv_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
  ext_adv_params.filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;
  ext_adv_params.primary_phy = ESP_BLE_GAP_PHY_1M;
  ext_adv_params.secondary_phy = ESP_BLE_GAP_PHY_1M;
  ext_adv_params.sid = 0;
  ext_adv_params.scan_req_notif = false;

  esp_err_t status = esp_ble_gap_ext_adv_set_params(ADV_INSTANCE_ID, &ext_adv_params);
  if (status != ESP_OK) {
    Serial.printf("Ext adv params failed: %d\n", status);
    startLegacyAdvertising();
    return;
  }
  
  delay(200);

  uint8_t advData[250] = {0};
  uint16_t advDataLen = 0;
  
  // Add flags
  advData[advDataLen++] = 2;
  advData[advDataLen++] = ESP_BLE_AD_TYPE_FLAG;
  advData[advDataLen++] = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT;
  
  // Add service UUID
  advData[advDataLen++] = 17;
  advData[advDataLen++] = ESP_BLE_AD_TYPE_128SRV_CMPL;
  
  const uint8_t uuid[16] = {0x4B, 0x91, 0x31, 0xC3, 0xC9, 0xC5, 0xCC, 0x8F, 
                         0x9E, 0x45, 0xB5, 0x1F, 0x01, 0xC2, 0xAF, 0x4F};
  memcpy(&advData[advDataLen], uuid, 16);
  advDataLen += 16;
  
  // Add device name
  const char* name = DEVICE_NAME;
  uint8_t nameLen = strlen(name);
  advData[advDataLen++] = nameLen + 1;
  advData[advDataLen++] = ESP_BLE_AD_TYPE_NAME_CMPL;
  memcpy(&advData[advDataLen], name, nameLen);
  advDataLen += nameLen;
  
  // If large payload, add more data
  if (largePDU) {
    const uint16_t mfgDataSize = 190;
    advData[advDataLen++] = mfgDataSize + 3;
    advData[advDataLen++] = ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE;
    
    advData[advDataLen++] = 0xFF;
    advData[advDataLen++] = 0xFF;
    
    for (int i = 0; i < mfgDataSize - 2; i++) {
      advData[advDataLen++] = i % 256;
    }
  }
  
  status = esp_ble_gap_config_ext_adv_data_raw(ADV_INSTANCE_ID, advDataLen, advData);
  if (status != ESP_OK) {
    Serial.printf("Ext adv data failed: %d\n", status);
    startLegacyAdvertising();
    return;
  }
  
  delay(200);
  
  esp_ble_gap_ext_adv_t ext_adv = {};
  ext_adv.instance = ADV_INSTANCE_ID;
  status = esp_ble_gap_ext_adv_start(1, &ext_adv);
  if (status != ESP_OK) {
    Serial.printf("Ext adv start failed: %d\n", status);
    startLegacyAdvertising();
    return;
  }
  
  extAdvConfigured = true;
}

void updateAdvertisingMode() {
  currentMode = (AdvertisingMode)(modeCount % 3);
  
  switch (currentMode) {
    case LEGACY_ADV:
      Serial.println("\n--- LEGACY ADV ---");
      break;
      
    case EXTENDED_ADV_SMALL:
      Serial.println("\n--- EXT ADV (Small) ---");
      break;
      
    case EXTENDED_ADV_LARGE:
      Serial.println("\n--- EXT ADV (Large) ---");
      break;
  }
  
  startAdvertisingWithCurrentMode();
  modeChangeTime = millis();
}

void setup() {
  Serial.begin(115200);
  Serial.println("BLE Ext Adv Test");

  BLEDevice::init(DEVICE_NAME);
  esp_ble_gap_register_callback(my_gap_event_handler);
  
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  BLEService *pService = pServer->createService(SERVICE_UUID);
  
  pCharacteristic = pService->createCharacteristic(
                      CHAR_UUID,
                      BLECharacteristic::PROPERTY_READ
                    );
  
  pService->start();
  
  updateAdvertisingMode();
  
  Serial.println("Ready");
}

void loop() {
  if (millis() - modeChangeTime >= TEST_DURATION) {
    modeCount++;
    updateAdvertisingMode();
  }
  
  delay(100);
}