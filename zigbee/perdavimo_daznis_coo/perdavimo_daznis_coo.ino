#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee coordinator mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

#define THERMOSTAT_ENDPOINT_NUMBER 5

ZigbeeThermostat zbThermostat = ZigbeeThermostat(THERMOSTAT_ENDPOINT_NUMBER);

float sensor_temp;
float sensor_max_temp;
float sensor_min_temp;
float sensor_tolerance;

/****************** Temperature sensor handling *******************/
void recieveSensorTemp(float temperature) {
  Serial.printf("Temperature sensor value: %.2f°C\n", temperature);
  sensor_temp = temperature;
}

void recieveSensorConfig(float min_temp, float max_temp, float tolerance) {
  Serial.printf("Temperature sensor settings: min %.2f°C, max %.2f°C, tolerance %.2f°C\n", min_temp, max_temp, tolerance);
  sensor_min_temp = min_temp;
  sensor_max_temp = max_temp;
  sensor_tolerance = tolerance;
}
/********************* Arduino functions **************************/
void setup() {
  Serial.begin(115200);
  zbThermostat.onTempRecieve(recieveSensorTemp);
  zbThermostat.onConfigRecieve(recieveSensorConfig);

  zbThermostat.setManufacturerAndModel("Espressif", "ZigbeeThermostat");

  Zigbee.addEndpoint(&zbThermostat);
  Zigbee.setRebootOpenNetwork(180);

  if (!Zigbee.begin(ZIGBEE_COORDINATOR)) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  }

  Serial.println("Waiting for Temperature sensor to bound to the thermostat");
  while (!zbThermostat.bound()) {
    Serial.printf(".");
    delay(500);
  }

  Serial.println();

  zbThermostat.getSensorSettings();
}

void loop() {
    // Set reporting interval for temperature sensor
    zbThermostat.setTemperatureReporting(0, 60, 2);
  }

  // Print temperature sensor data
  static uint32_t last_print = 0;
  if (millis() - last_print > 60000) {
    last_print = millis();
    int temp_percent = (int)((sensor_temp - sensor_min_temp) / (sensor_max_temp - sensor_min_temp) * 100);
    Serial.printf("Loop temperature info: %.2f°C (%d %%)\n", sensor_temp, temp_percent);
  }
}