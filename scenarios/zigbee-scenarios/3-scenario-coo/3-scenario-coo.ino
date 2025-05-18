// Device B (Coordinator) - Receives temperature data

#include "Zigbee.h"
#include "DHT.h"

#define THERMOSTAT_ENDPOINT_NUMBER 10
#define DHTPIN 4
#define DHTTYPE DHT22

ZigbeeThermostat zbThermostat = ZigbeeThermostat(THERMOSTAT_ENDPOINT_NUMBER);
DHT dht(DHTPIN, DHTTYPE);

float sensor_min_temp = -40.0f;
float sensor_max_temp = 80.0f;
float sensor_tolerance = 0.5f;
float sensor_temp = 0.0f;
float sensor_humidity = 0.0f;
unsigned long last_temp_received = 0;
unsigned long temp_receive_count = 0;
bool new_temperature_received = false;

#define VALID_MIN_TEMP -50.0f
#define VALID_MAX_TEMP 100.0f

void readLocalSensor() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  if (!isnan(temperature) && !isnan(humidity)) {
    Serial.printf("Local Sensor Reading: %.2f°C, %.2f%%\n", temperature, humidity);
  }
}

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
  
  Serial.println("Sending pseudo-ACK using getSensorSettings()");
  zbThermostat.getSensorSettings();
}

void printTemperatureReport() {
  Serial.printf("Temp reading #%lu: %.2f°C\n",
               temp_receive_count, sensor_temp);
  new_temperature_received = false;
}

void receiveSensorConfig(float min_temp, float max_temp, float tolerance) {
  Serial.printf("Sensor config received: min %.2f°C, max %.2f°C, tol %.2f°C\n",
                min_temp, max_temp, tolerance);
  sensor_min_temp = min_temp;
  sensor_max_temp = max_temp;
  sensor_tolerance = tolerance;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  
  Serial.println("\n\n----- COORDINATOR STARTUP -----");
  Serial.println("Device B (Coordinator)");
  
  dht.begin();
  
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
  
  Serial.println("----- COORDINATOR STARTUP COMPLETE -----\n");
}

void loop() {
  static uint32_t last_heartbeat = 0;
  static uint32_t last_local_read = 0;
  
  if (millis() - last_heartbeat > 60000) {
    last_heartbeat = millis();
    if (zbThermostat.bound()) {
      Serial.printf("Heartbeat: %lu readings received\n", temp_receive_count);
    } else {
      Serial.println("Coordinator running - no sensor bound");
    }
  }
  
  if (millis() - last_local_read > 300000) {
    last_local_read = millis();
    readLocalSensor();
  }
}