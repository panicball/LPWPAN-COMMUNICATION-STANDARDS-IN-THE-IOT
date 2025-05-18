#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <vector>

#define SERVER_NAME "ESP32_Power_Test"
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

struct TestMetrics {
    unsigned long totalPacketsExpected = 0;
    unsigned long packetsReceived = 0;
    unsigned long packetsLost = 0;
    int currentPayloadSize = 0;
    unsigned long testStartTime = 0;
    unsigned long lastPacketTime = 0;
    int rssi = -127;
    unsigned long bytesReceived = 0;
    float throughput = 0.0;
    uint16_t mtu = 23;
    unsigned long totalRTT = 0;
    unsigned long rttCount = 0;
    float avgRTT = 0.0;
    float jitter = 0.0;
    unsigned long lastLatency = 0;
    float sumJitterSquared = 0.0;
    unsigned long firstPacketTime = 0;
    std::vector<unsigned long> latencies;
};

static TestMetrics currentTest;
static BLEAddress* serverAddress = nullptr;
static boolean doConnect = false;
static boolean connected = false;
static BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
static BLEClient* pClient = nullptr;
unsigned long lastDataReceived = 0;
size_t lastPacketSize = 0;
bool waitingForReconnect = false;
const unsigned long RECONNECT_DELAY = 5000;
unsigned long testStartTime = 0;
const unsigned long TEST_DURATION = 10000;
bool testActive = false;

class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
        connected = true;
        uint16_t mtu = 512;
        if (pclient->setMTU(mtu)) {
            Serial.printf("MTU requested: %u\n", mtu);
        }
    }
    void onDisconnect(BLEClient* pclient) {
        connected = false;
        Serial.println("Disconnected");
    }
};

float calculateStandardDeviation(const std::vector<unsigned long>& values) {
    if (values.size() < 2) return 0.0;
    
    float mean = 0.0;
    for (auto value : values) {
        mean += value;
    }
    mean /= values.size();
    
    float variance = 0.0;
    for (auto value : values) {
        float diff = value - mean;
        variance += diff * diff;
    }
    variance /= (values.size() - 1);
    
    return sqrt(variance);
}

void resetTestMetrics() {
    currentTest = TestMetrics();
    currentTest.latencies.clear();
}

void calculateFinalMetrics() {
    unsigned long testDuration = millis() - currentTest.testStartTime;
    if (testDuration == 0) return;
    
    currentTest.throughput = (float)currentTest.bytesReceived * 1000.0 / testDuration;
    
    currentTest.packetsLost = (currentTest.packetsReceived < currentTest.totalPacketsExpected) ? 
        currentTest.totalPacketsExpected - currentTest.packetsReceived : 0;
    
    if (!currentTest.latencies.empty()) {
        currentTest.jitter = calculateStandardDeviation(currentTest.latencies);
    }
    
    if (currentTest.rttCount > 0) {
        currentTest.avgRTT = (float)currentTest.totalRTT / currentTest.rttCount;
    }
}

void printTestMetrics() {
    if (currentTest.packetsReceived > 0) {
        float packetSuccessRate = (float)currentTest.packetsReceived / currentTest.totalPacketsExpected * 100.0;
        
        Serial.println("\n--- Test Metrics ---");
        Serial.printf("Payload: %d bytes\n", currentTest.currentPayloadSize);
        Serial.printf("MTU: %u bytes\n", currentTest.mtu);
        Serial.printf("RSSI: %d dBm\n", currentTest.rssi);
        Serial.printf("Expected: %lu pkts\n", currentTest.totalPacketsExpected);
        Serial.printf("Received: %lu pkts\n", currentTest.packetsReceived);
        Serial.printf("Lost: %lu pkts\n", currentTest.packetsLost);
        Serial.printf("Success: %.2f%%\n", packetSuccessRate);
        Serial.printf("Total: %lu bytes\n", currentTest.bytesReceived);
        Serial.printf("Duration: %.2f s\n", (millis() - currentTest.testStartTime) / 1000.0);
        Serial.printf("Throughput: %.2f B/s\n", currentTest.throughput);
        
        if (!currentTest.latencies.empty()) {
            float avgLatency = 0;
            for (auto latency : currentTest.latencies) {
                avgLatency += latency;
            }
            avgLatency /= currentTest.latencies.size();
            Serial.printf("Avg latency: %.2f ms\n", avgLatency);
        }
        
        if (currentTest.rttCount > 0) {
            Serial.printf("Avg RTT: %.2f ms\n", currentTest.avgRTT);
        }
        
        if (currentTest.jitter > 0) {
            Serial.printf("Jitter: %.2f ms\n", currentTest.jitter);
        }
        
        Serial.println("--------------------\n");
    }
}

void checkAndEndTest(unsigned long currentTime) {
    if (testActive && (currentTime - testStartTime >= TEST_DURATION)) {
        calculateFinalMetrics();
        printTestMetrics();
        resetTestMetrics();
        testActive = false;
    }
}

static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    
    unsigned long currentTime = millis();
    lastDataReceived = currentTime;
    
    checkAndEndTest(currentTime);
    
    if (!testActive) {
        testStartTime = currentTime;
        currentTest.firstPacketTime = currentTime;
        currentTest.testStartTime = currentTime;
        currentTest.lastLatency = 0;
        currentTest.latencies.clear();
        testActive = true;
        lastPacketSize = length;
        currentTest.currentPayloadSize = length;
        currentTest.totalPacketsExpected = 100;
    }
    
    if (testActive && lastPacketSize != length) {
        calculateFinalMetrics();
        printTestMetrics();
        resetTestMetrics();
        
        testStartTime = currentTime;
        currentTest.currentPayloadSize = length;
        currentTest.testStartTime = currentTime;
        currentTest.firstPacketTime = currentTime;
        currentTest.totalPacketsExpected = 100;
        lastPacketSize = length;
    }
    
    currentTest.packetsReceived++;
    currentTest.bytesReceived += length;
    
    unsigned long packetLatency = currentTime - currentTest.firstPacketTime;
    currentTest.latencies.push_back(packetLatency);
    
    if (length >= sizeof(unsigned long)) {
        unsigned long sendTime;
        memcpy(&sendTime, pData, sizeof(unsigned long));
        if (sendTime > 0 && sendTime < currentTime) {
            unsigned long rtt = currentTime - sendTime;
            currentTest.totalRTT += rtt;
            currentTest.rttCount++;
        }
    }
}

static void cleanupBLE() {
    if (testActive) {
        calculateFinalMetrics();
        printTestMetrics();
        testActive = false;
    }
    
    if (pClient != nullptr) {
        if (connected) {
            currentTest.rssi = pClient->getRssi();
            pClient->disconnect();
        }
        delete pClient;
        pClient = nullptr;
    }
    if (serverAddress != nullptr) {
        delete serverAddress;
        serverAddress = nullptr;
    }
    connected = false;
}


class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        if (advertisedDevice.getName() == SERVER_NAME) {
            currentTest.rssi = advertisedDevice.getRSSI();
            
            advertisedDevice.getScan()->stop();
            serverAddress = new BLEAddress(advertisedDevice.getAddress());
            doConnect = true;
        }
    }
};

bool connectToServer() {
    if (serverAddress == nullptr) {
        Serial.println("Error: Server address is null");
        return false;
    }
    Serial.print("Connecting to ");
    Serial.println(serverAddress->toString().c_str());
    
    pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());
    Serial.println("BLE Client created");
    
    if (!pClient->connect(*serverAddress)) {
        Serial.println("Connection failed");
        return false;
    }
    Serial.println("Connected to server");
    
    uint16_t requestedMTU = 512;
    if (pClient->setMTU(requestedMTU)) {
        Serial.printf("MTU requested: %u\n", requestedMTU);
        delay(100);
        currentTest.mtu = pClient->getMTU();
        Serial.printf("MTU negotiated: %u\n", currentTest.mtu);
    }
    
    currentTest.rssi = pClient->getRssi();
    Serial.printf("RSSI: %d dBm\n", currentTest.rssi);
    
    BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);
    if (pRemoteService == nullptr) {
        Serial.println("Service not found");
        pClient->disconnect();
        return false;
    }
    pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);
    if (pRemoteCharacteristic == nullptr) {
        Serial.println("Characteristic not found");
        pClient->disconnect();
        return false;
    }
    if(pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
    }
    connected = true;
    waitingForReconnect = false;
    return true;
}

void startScan() {
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(30, false);
}

void setup() {
    Serial.begin(115200);
    
    BLEDevice::init("");
    BLEDevice::setMTU(512);
    startScan();
}

void loop() {
    static unsigned long lastRssiUpdate = 0;
    const unsigned long RSSI_UPDATE_INTERVAL = 1000;
    const unsigned long TEST_TIMEOUT = 12000;
    
    if (doConnect) {
        if (connectToServer()) {
            lastDataReceived = millis();
        } else {
            cleanupBLE();
        }
        doConnect = false;
    }
    
    if (connected && currentTest.firstPacketTime != 0) {
        unsigned long currentTime = millis();
        if (currentTime - lastDataReceived > TEST_TIMEOUT) {
            calculateFinalMetrics();
            printTestMetrics();
            resetTestMetrics();
            lastPacketSize = 0;
        }
    }
    
    if (connected && pClient != nullptr) {
        unsigned long currentTime = millis();
        if (currentTime - lastRssiUpdate >= RSSI_UPDATE_INTERVAL) {
            int newRssi = pClient->getRssi();
            if (newRssi != 0) {
                currentTest.rssi = newRssi;
            }
            lastRssiUpdate = currentTime;
        }
    }
    
    if (connected) {
        unsigned long currentTime = millis();
        if (currentTime - lastDataReceived > 10000) {
            cleanupBLE();
            waitingForReconnect = true;
            lastDataReceived = currentTime;
        }
    }
    
    if (waitingForReconnect && !connected && !doConnect) {
        unsigned long currentTime = millis();
        if (currentTime - lastDataReceived > RECONNECT_DELAY) {
            startScan();
            waitingForReconnect = false;
        }
    }
    
    if (!connected && !doConnect && !waitingForReconnect) {
        startScan();
    }
    
    delay(100);
}