// Device B (Coordinator) - Receives temperature data

#include "Zigbee.h"
extern "C" {
  #include "esp_zigbee_core.h"
}

#define THERMOSTAT_ENDPOINT_NUMBER 10

ZigbeeThermostat zbThermostat = ZigbeeThermostat(THERMOSTAT_ENDPOINT_NUMBER);

float sensor_min_temp = -40.0f;
float sensor_max_temp = 80.0f;
float sensor_tolerance = 0.5f;
float sensor_temp = 0.0f;
float sensor_humidity = 47.30f;
unsigned long last_temp_received = 0;
unsigned long temp_receive_count = 0;
bool new_temperature_received = false;

#define VALID_MIN_TEMP -50.0f
#define VALID_MAX_TEMP 100.0f

unsigned long ack_sent_count = 0;

void receiveSensorTemp(float temperature) {
  if (temperature < VALID_MIN_TEMP || temperature > VALID_MAX_TEMP) {
    Serial.printf("Invalid temperature reading: %.2f°C - ignoring\n", temperature);
    return;
  }
  
  last_temp_received = millis();
  temp_receive_count++;
  sensor_temp = temperature;
  
  Serial.printf("Temperature received: %.2f°C\n", temperature);
  Serial.printf("Humidity: %.1f%%\n", sensor_humidity);
  
  ack_sent_count++;
  Serial.printf("Data received - ACK #%lu registered\n", ack_sent_count);
}

void receiveSensorConfig(float min_temp, float max_temp, float tolerance) {
  Serial.printf("Sensor settings updated: min %.2f°C, max %.2f°C, tolerance %.2f°C\n", 
                min_temp, max_temp, tolerance);
  sensor_min_temp = min_temp;
  sensor_max_temp = max_temp;
  sensor_tolerance = tolerance;
  
  ack_sent_count++;
  Serial.printf("Config processed - ACK #%lu registered\n", ack_sent_count);
}

void parseSerialData() {
  if (Serial.available()) {
    String data = Serial.readStringUntil('\n');
    
    if (data.indexOf("SENSOR_DATA:") >= 0) {
      int h_pos = data.indexOf("H=");
      if (h_pos >= 0) {
        String h_str = data.substring(h_pos + 2);
        sensor_humidity = h_str.toFloat();
        Serial.printf("Extracted humidity: %.1f%%\n", sensor_humidity);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  
  Serial.println("\n\n----- COORDINATOR STARTUP -----");
  Serial.println("Device B (Coordinator)");

  zbThermostat.onTempRecieve(receiveSensorTemp);
  zbThermostat.onConfigRecieve(receiveSensorConfig);
  
  zbThermostat.setManufacturerAndModel("ESP32-C6", "ZigbeeCoordinator");
  Zigbee.addEndpoint(&zbThermostat);
  Zigbee.setRebootOpenNetwork(180);

  Serial.println("Starting ZigBee Coordinator...");
  if (!Zigbee.begin(ZIGBEE_COORDINATOR)) {
    Serial.println("Zigbee failed to start! Rebooting...");
    ESP.restart();
  }

  Serial.println("ZigBee Coordinator started");
  Serial.println("\nWaiting for sensor binding...");
  
  unsigned long startTime = millis();
  while (!zbThermostat.bound() && (millis() - startTime < 60000)) {
    Serial.print(".");
    delay(500);
  }

  if (zbThermostat.bound()) {
    Serial.println("\nTemperature sensor bound successfully!");
    
    zbThermostat.getSensorSettings();
    zbThermostat.setTemperatureReporting(0, 10, 0);
    
    Serial.println("Coordinator ready to receive data");
  } else {
    Serial.println("\nTimeout waiting for binding.");
    Serial.println("Network still open for joining.");
  }
  
  Serial.println("----- COORDINATOR STARTUP COMPLETE -----\n");
}

void loop() {
  parseSerialData();

  static uint32_t last_heartbeat = 0;
  if (millis() - last_heartbeat > 60000) {
    last_heartbeat = millis();
    
    if (zbThermostat.bound()) {
      Serial.printf("\n[Heartbeat] Status: %lu readings, %lu packets\n", 
                   temp_receive_count, ack_sent_count);
      
      if (last_temp_received > 0) {
        unsigned long time_since = (millis() - last_temp_received) / 1000;
        Serial.printf("[Heartbeat] Last reading: %lu seconds ago\n", time_since);
      }
    } else {
      Serial.println("[Heartbeat] No sensor bound yet");
    }
  }
}