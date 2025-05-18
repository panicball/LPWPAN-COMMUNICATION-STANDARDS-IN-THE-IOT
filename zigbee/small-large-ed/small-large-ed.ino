#ifndef ZIGBEE_MODE_ED
  #error "Zigbee end device mode must be selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"
#include <Arduino.h>

#define ED_ENDPOINT 1

const uint8_t analogPin = A0;
const uint8_t button    = BOOT_PIN;

ZigbeeAnalog zbAnalogDevice(ED_ENDPOINT);

void setup() {
  Serial.begin(115200);
  pinMode(button, INPUT_PULLUP);
  analogReadResolution(10);

  zbAnalogDevice.setManufacturerAndModel("Espressif", "ChunkED");
  zbAnalogDevice.addAnalogInput();

  ZigbeeEP::allowMultipleBinding(true);
  Zigbee.addEndpoint(&zbAnalogDevice);


  if (!Zigbee.begin()) {
    Serial.println("Zigbee failed to start, rebooting...");
    ESP.restart();
  }

  while (!Zigbee.connected()) {
    Serial.print('.');
    delay(100);
  }
  Serial.println("\nJoined network");
}

void loop() {
  // // --- 1) Send small
  // float analogValue = (float)analogRead(analogPin);
  // if (zbAnalogDevice.setAnalogInput(analogValue)) {
  //   zbAnalogDevice.reportAnalogInput();
  //   Serial.printf("Small packet: %.1f\n", analogValue);
  // }

  // --- 2) Send buffer in 4-byte chunks
  static uint8_t bigData[16];
  for (uint8_t i = 0; i < sizeof(bigData); i++) bigData[i] = i;

  for (uint8_t offset = 0; offset < sizeof(bigData); offset += 4) {
    uint32_t chunk = 0;
    for (uint8_t b = 0; b < 4 && (offset + b) < sizeof(bigData); b++) {
      chunk |= ((uint32_t)bigData[offset + b]) << (8 * b);
    }
    float f;
    memcpy(&f, &chunk, sizeof(f));

    if (zbAnalogDevice.setAnalogInput(f)) {
      zbAnalogDevice.reportAnalogInput();
      Serial.printf("Big chunk @%d: 0x%08X\n", offset, chunk);
    }
    delay(200);
  }
  delay(5000);
}
