#include <WiFi.h>
#include <esp_wifi.h>
#include <WiFiUdp.h>

const char* ssid = "test";
const char* password = "test";

const unsigned int TX_UDP_PORT = 4210;
unsigned int packetSize = 1024;
unsigned int packetsPerSecond = 100;
unsigned int burstSize = 10;

IPAddress rxDeviceIP;
bool hasTargetIP = false;

enum TrafficMode {
  MODE_HT20,
  MODE_HT40,
  MODE_HE20,
  MODE_CUSTOM
};

TrafficMode currentMode = MODE_HT20;
WiFiUDP udp;

unsigned long packetsSent = 0;
unsigned long totalBytesSent = 0;
unsigned long lastStatTime = 0;
const int STAT_INTERVAL_MS = 1000;

void setupWiFi();
void configureTrafficMode(TrafficMode mode);
void printCurrentMode();
void sendTraffic();
void readSerialCommand();

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\nESP32 Wi-Fi TX Sender");
  Serial.println("===========================================");
  
  setupWiFi();
  lastStatTime = millis();
}

void loop() {
  readSerialCommand();
  
  if (hasTargetIP) {
    sendTraffic();
    
    if (millis() - lastStatTime >= STAT_INTERVAL_MS) {
      Serial.printf("TX: Mode: %d, Pkts: %lu, Bytes: %lu, Rate: %lu pps\n", 
                    currentMode, 
                    packetsSent,
                    totalBytesSent,
                    packetsSent / (STAT_INTERVAL_MS / 1000));
                    
      packetsSent = 0;
      totalBytesSent = 0;
      lastStatTime = millis();
    }
  }
  
  delay(1);
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  Serial.println("Connecting to Wi-Fi...");
  
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    esp_wifi_set_ps(WIFI_PS_NONE);
  } else {
    Serial.println("\nFailed to connect to Wi-Fi.");
  }
}

void configureTrafficMode(TrafficMode mode) {
  currentMode = mode;
  
  switch (mode) {
    case MODE_HT20:
      packetSize = 1024;
      packetsPerSecond = 100;
      burstSize = 5;
      
      esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
      esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
      break;
      
    case MODE_HT40:
      packetSize = 1460;
      packetsPerSecond = 150;
      burstSize = 8;
      
      esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
      esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT40);
      break;
      
    case MODE_HE20:
      packetSize = 1024;
      packetsPerSecond = 200;
      burstSize = 10;
      
      #ifdef WIFI_PROTOCOL_11AX
      esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_11AX);
      esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
      #else
      Serial.println("Warning: 802.11ax not supported, using 802.11n");
      esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
      esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
      #endif
      break;
      
    case MODE_CUSTOM:
      break;
  }
  
  printCurrentMode();
}

void printCurrentMode() {
  Serial.println("\nCurrent Traffic Mode:");
  
  switch (currentMode) {
    case MODE_HT20:
      Serial.println("Mode: HT20 (802.11n, 20MHz)");
      break;
    case MODE_HT40:
      Serial.println("Mode: HT40 (802.11n, 40MHz)");
      break;
    case MODE_HE20:
      Serial.println("Mode: HE20 (802.11ax, 20MHz)");
      break;
    case MODE_CUSTOM:
      Serial.println("Mode: Custom");
      break;
  }
  
  Serial.printf("Packet: %u bytes, Rate: %u pps, Burst: %u pkts\n", 
                packetSize, packetsPerSecond, burstSize);
  Serial.printf("Target: %s:%u\n", rxDeviceIP.toString().c_str(), TX_UDP_PORT);
}

void sendTraffic() {
  static unsigned long lastSendTime = 0;
  static uint8_t *buffer = NULL;
  static size_t bufferSize = 0;
  
  unsigned long sendInterval = 1000 / (packetsPerSecond / burstSize);
  
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();
    
    if (buffer == NULL || bufferSize != packetSize) {
      if (buffer != NULL) {
        free(buffer);
      }
      buffer = (uint8_t*)malloc(packetSize);
      bufferSize = packetSize;
      
      for (unsigned int i = 0; i < packetSize; i++) {
        buffer[i] = i & 0xFF;
      }
    }
    
    for (unsigned int i = 0; i < burstSize; i++) {
      buffer[0] = (packetsSent >> 24) & 0xFF;
      buffer[1] = (packetsSent >> 16) & 0xFF;
      buffer[2] = (packetsSent >> 8) & 0xFF;
      buffer[3] = packetsSent & 0xFF;
      
      udp.beginPacket(rxDeviceIP, TX_UDP_PORT);
      udp.write(buffer, packetSize);
      udp.endPacket();
      
      packetsSent++;
      totalBytesSent += packetSize;
      
      delayMicroseconds(100);
    }
  }
}

void readSerialCommand() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.startsWith("ip ")) {
      String ipString = command.substring(3);
      if (rxDeviceIP.fromString(ipString)) {
        hasTargetIP = true;
        Serial.printf("Target IP: %s\n", rxDeviceIP.toString().c_str());
      } else {
        Serial.println("Invalid IP format. Use: ip 192.168.1.100");
      }
    }
    else if (command == "mode 1" || command == "mode ht20") {
      configureTrafficMode(MODE_HT20);
    }
    else if (command == "mode 2" || command == "mode ht40") {
      configureTrafficMode(MODE_HT40);
    }
    else if (command == "mode 3" || command == "mode he20") {
      configureTrafficMode(MODE_HE20);
    }
    else if (command.startsWith("size ")) {
      int size = command.substring(5).toInt();
      if (size >= 64 && size <= 1472) {
        packetSize = size;
        Serial.printf("Packet size: %u bytes\n", packetSize);
      } else {
        Serial.println("Invalid size. Use 64-1472 bytes.");
      }
    }
    else if (command.startsWith("rate ")) {
      int rate = command.substring(5).toInt();
      if (rate > 0 && rate <= 1000) {
        packetsPerSecond = rate;
        Serial.printf("Rate: %u pps\n", packetsPerSecond);
      } else {
        Serial.println("Invalid rate. Use 1-1000 pps.");
      }
    }
    else if (command.startsWith("burst ")) {
      int burst = command.substring(6).toInt();
      if (burst > 0 && burst <= 50) {
        burstSize = burst;
        Serial.printf("Burst: %u packets\n", burstSize);
      } else {
        Serial.println("Invalid burst. Use 1-50 packets.");
      }
    }
    else if (command == "start") {
      if (hasTargetIP) {
        Serial.println("Traffic started.");
      } else {
        Serial.println("Set target IP first with 'ip' command.");
      }
    }
    else if (command == "stop") {
      hasTargetIP = false;
      Serial.println("Traffic stopped.");
    }
    else if (command == "status") {
      printCurrentMode();
    }
    else {
      Serial.println("Unknown command. Type 'help' for commands.");
    }
  }
}
