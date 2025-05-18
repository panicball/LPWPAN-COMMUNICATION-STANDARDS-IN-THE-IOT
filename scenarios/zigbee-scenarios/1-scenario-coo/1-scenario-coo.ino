#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee coordinator mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

#define THERMOSTAT_ENDPOINT_NUMBER 10
uint8_t button = BOOT_PIN;

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

void receiveSensorTemp(float temperature) {
  if (temperature < VALID_MIN_TEMP || temperature > VALID_MAX_TEMP) {
    Serial.printf("Invalid temp received: %.2f°C - ignored\n", temperature);
    return;
  }

  last_temp_received = millis();
  temp_receive_count++;
  sensor_temp = temperature;
  new_temperature_received = true;
  printTemperatureReport();

  // === Send pseudo-ACK ===
  Serial.println("Sending pseudo-ACK using getSensorSettings()");
  zbThermostat.getSensorSettings();  // this sends a request the End Device can detect
}


void printTemperatureReport() {
  Serial.printf("Temp reading #%lu: %.2f°C, Humidity: %.1f%%\n", 
               temp_receive_count, sensor_temp, sensor_humidity);
  new_temperature_received = false;
}

void receiveSensorConfig(float min_temp, float max_temp, float tolerance) {
  Serial.printf("Sensor config: min %.2f°C, max %.2f°C, tol %.2f°C\n", 
                min_temp, max_temp, tolerance);
  sensor_min_temp = min_temp;
  sensor_max_temp = max_temp;
  sensor_tolerance = tolerance;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.println("Zigbee Coordinator - Standard Cycle");

  pinMode(button, INPUT_PULLUP);
  zbThermostat.onTempRecieve(receiveSensorTemp);
  zbThermostat.onConfigRecieve(receiveSensorConfig);
  zbThermostat.setManufacturerAndModel("ESP32-C6", "ZigbeeCoordinator");

  Zigbee.addEndpoint(&zbThermostat);
  Zigbee.setRebootOpenNetwork(180);
  Serial.println("Starting ZigBee Coordinator...");
  if (!Zigbee.begin(ZIGBEE_COORDINATOR)) {
    Serial.println("Zigbee failed to start. Rebooting...");
    ESP.restart();
  }

  Serial.println("ZigBee started. Waiting for sensor binding...");
  unsigned long startTime = millis();
  while (!zbThermostat.bound() && (millis() - startTime < 60000)) {
    Serial.print(".");
    delay(500);
  }

  if (zbThermostat.bound()) {
    Serial.println("\nSensor successfully bound.");
    zbThermostat.getSensorSettings();
    zbThermostat.setTemperatureReporting(0, 10, 0);
    Serial.println("Coordinator ready.");
  } else {
    Serial.println("\nSensor not bound. Network still open.");
  }
}

void loop() {
  if (digitalRead(button) == LOW) {
    delay(100);
    unsigned long startTime = millis();
    while (digitalRead(button) == LOW) {
      delay(50);
      if ((millis() - startTime) > 3000) {
        Serial.println("Factory reset triggered.");
        delay(1000);
        Zigbee.factoryReset();
      }
    }
    zbThermostat.getSensorSettings();
  }

  static uint32_t last_heartbeat = 0;
  if (millis() - last_heartbeat > 60000) {
    last_heartbeat = millis();
    if (zbThermostat.bound()) {
      Serial.printf("Heartbeat: %lu readings received\n", temp_receive_count);
    } else {
      Serial.println("Coordinator running - no sensor bound");
    }
  }
}
