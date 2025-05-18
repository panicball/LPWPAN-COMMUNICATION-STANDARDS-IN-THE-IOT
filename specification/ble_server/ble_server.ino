#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <esp_wifi.h>
#include <driver/adc.h>
#include <math.h>

#define DEVICE_NAME "ESP32_Power_Test"
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

const int TEST_PAYLOAD_SIZES[] = {20, 100, 200, 400, 512};
const int TEST_DURATION = 10000;
const int SEND_INTERVAL = 100;

const int VOLTAGE_BUFFER_SIZE = 100;
struct VoltageReading {
    unsigned long timestamp;
    float voltage;
    int payloadSize;
};
VoltageReading voltageReadings[VOLTAGE_BUFFER_SIZE];
int readIndex = 0;

struct TestMetrics {
    unsigned long packetsSent = 0;
    int currentPayloadSize = 0;
    unsigned long testStartTime = 0;
    int txPower = -1;
    float avgTxCurrent = 0.0;
    unsigned long bytesSent = 0;
    float throughput = 0.0;
    uint16_t mtu = 23;
    unsigned long lastSendTime = 0;
};

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
TestMetrics currentTest;

int getTxPowerdBm(esp_power_level_t level) {
  switch (level) {
    case ESP_PWR_LVL_N12: return -12;
    case ESP_PWR_LVL_N9:  return -9;
    case ESP_PWR_LVL_N6:  return -6;
    case ESP_PWR_LVL_N3:  return -3;
    case ESP_PWR_LVL_N0:  return  0;
    case ESP_PWR_LVL_P3:  return  3;
    case ESP_PWR_LVL_P6:  return  6;
    case ESP_PWR_LVL_P9:  return  9;
    case ESP_PWR_LVL_P12: return 12;
    case ESP_PWR_LVL_P15: return 15;
    case ESP_PWR_LVL_P18: return 18;
    case ESP_PWR_LVL_P20: return 20;
    default:              return 0;
  }
}

void printTestMetrics() {
    Serial.println("\n--- Test Metrics ---");
    Serial.printf("Payload: %d bytes\n", currentTest.currentPayloadSize);
    Serial.printf("MTU: %u bytes\n", currentTest.mtu);
    Serial.printf("Packets: %lu\n", currentTest.packetsSent);
    Serial.printf("Total: %lu bytes\n", currentTest.bytesSent);
    Serial.printf("Throughput: %.2f B/s\n", currentTest.throughput);
    Serial.printf("TX Power: %d dBm\n", currentTest.txPower);
    Serial.println("--------------------\n");
}

const esp_power_level_t POWER_CONSTANTS[] = {
    ESP_PWR_LVL_N12,
    ESP_PWR_LVL_N9,
    ESP_PWR_LVL_N6,
    ESP_PWR_LVL_N3,
    ESP_PWR_LVL_N0,
    ESP_PWR_LVL_P3,
    ESP_PWR_LVL_P6,
    ESP_PWR_LVL_P9,
    ESP_PWR_LVL_P12,
    ESP_PWR_LVL_P15,
    ESP_PWR_LVL_P18,
    ESP_PWR_LVL_P20
};

const int POWER_LEVELS[] = {
    -12, -9, -6, -3, 0, 3, 6, 9, 12, 15, 18, 20
};

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Device connected");

        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_N12);
        esp_power_level_t currentPowerLevel = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT);
        
        currentTest.txPower = getTxPowerdBm(currentPowerLevel);

        Serial.println("Power Level Investigation:");
        for (int i = 0; i < (int)(sizeof(POWER_CONSTANTS)/sizeof(POWER_CONSTANTS[0])); i++) {
            esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, POWER_CONSTANTS[i]);
            esp_power_level_t level = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT);
            Serial.printf("Req: %d dBm, Act: %d dBm\n", 
                POWER_LEVELS[i], getTxPowerdBm(level));
        }

        esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_N12);
        currentPowerLevel = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT);
        currentTest.txPower = getTxPowerdBm(currentPowerLevel);

        Serial.printf("Final TX Power: %d dBm\n", currentTest.txPower);

        if (pServer->getConnId() != 0) {
            uint16_t mtu = 512;
            pServer->updatePeerMTU(pServer->getConnId(), mtu);
            Serial.printf("MTU request: %u\n", mtu);
        }
    }

    void onMtuChanged(BLEServer* pServer, uint16_t MTU) {
        currentTest.mtu = MTU;
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected");
        currentTest = TestMetrics();
    }
};

void setup() {
  Serial.begin(115200);
  
  esp_wifi_stop();
  
  BLEDevice::init(DEVICE_NAME);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  
  Serial.println("BLE device ready");
  Serial.println("Waiting for client connection...");
}

void runTest(int payloadSize) {
    if (!deviceConnected) {
        Serial.println("Error: No connection");
        return;
    }

    if (currentTest.mtu == 0) {
        Serial.println("Warning: Using default MTU (23)");
        currentTest.mtu = 23;
    }

    if (currentTest.txPower == -1) {
        Serial.println("Warning: Checking TX Power");
        esp_power_level_t currentPowerLevel = esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT);
        currentTest.txPower = getTxPowerdBm(currentPowerLevel);
    }

    int savedTxPower = currentTest.txPower;
    uint16_t savedMTU = currentTest.mtu;
    currentTest = TestMetrics();
    currentTest.txPower = savedTxPower;
    currentTest.mtu = savedMTU;
    
    currentTest.currentPayloadSize = payloadSize;
    currentTest.testStartTime = millis();
    
    uint8_t* testData = new uint8_t[payloadSize];
    unsigned long startTime = millis();
    unsigned long lastSendTime = 0;
    
    Serial.printf("\nTest: %d bytes (MTU: %d, TX: %d dBm)\n", 
        payloadSize, currentTest.mtu, currentTest.txPower);
    
    while ((millis() - startTime < TEST_DURATION) && deviceConnected) {
        if (millis() - lastSendTime >= SEND_INTERVAL) {
            unsigned long currentTime = millis();
            memcpy(testData, &currentTime, sizeof(unsigned long));
            memset(testData + sizeof(unsigned long), 0x55, payloadSize - sizeof(unsigned long));
            
            pCharacteristic->setValue(testData, payloadSize);
            pCharacteristic->notify();
            
            lastSendTime = currentTime;
            currentTest.packetsSent++;
            currentTest.bytesSent += payloadSize;
        }
    }
    
    unsigned long testDuration = millis() - startTime;
    currentTest.throughput = (float)currentTest.bytesSent / (testDuration / 1000.0);
    
    delete[] testData;
    printTestMetrics();
    
    Serial.printf("Test complete: %d bytes\n", payloadSize);
}

void loop() {
  if (deviceConnected) {
    for (int size : TEST_PAYLOAD_SIZES) {
      runTest(size);
      delay(4000);
    }
    
    delay(5000);
    esp_sleep_enable_timer_wakeup(10 * 1000000);
    esp_deep_sleep_start();
  }
}