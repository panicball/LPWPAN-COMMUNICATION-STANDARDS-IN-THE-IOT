// Complete channel‐by‐channel Zigbee scanner for ESP32
// Based on: https://github.com/espressif/arduino-esp32/blob/master/libraries/Zigbee/examples/Zigbee_Scan_Networks/Zigbee_Scan_Networks.ino

#include "Zigbee.h"

#ifdef ZIGBEE_MODE_ZCZR
zigbee_role_t role = ZIGBEE_ROUTER;    // or ZIGBEE_COORDINATOR
#else
zigbee_role_t role = ZIGBEE_END_DEVICE;
#endif

const uint8_t scanTime = 5;
uint8_t nextChan   = 11;

void printScannedNetworks(uint16_t networksFound) {
  if (networksFound == 0) {
    Serial.println("No networks found");
  } else {
    zigbee_scan_result_t *scan_result = Zigbee.getScanResult();
    Serial.println("\nScan done");
    Serial.print(networksFound);
    Serial.println(" networks found:");
    Serial.println("Nr | PAN ID | CH | Permit Joining | Router Capacity | End Device Capacity | Extended PAN ID");
    for (int i = 0; i < networksFound; ++i) {
      Serial.printf("%2d | 0x%04hx | %2d | %-14.14s | %-15.15s | %-19.19s | %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
        i+1,
        scan_result[i].short_pan_id,
        scan_result[i].logic_channel,
        scan_result[i].permit_joining      ? "Yes" : "No",
        scan_result[i].router_capacity     ? "Yes" : "No",
        scan_result[i].end_device_capacity ? "Yes" : "No",
        scan_result[i].extended_pan_id[7],
        scan_result[i].extended_pan_id[6],
        scan_result[i].extended_pan_id[5],
        scan_result[i].extended_pan_id[4],
        scan_result[i].extended_pan_id[3],
        scan_result[i].extended_pan_id[2],
        scan_result[i].extended_pan_id[1],
        scan_result[i].extended_pan_id[0]
      );
      delay(10);
    }
    Serial.println();
    Zigbee.scanDelete();
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  if (!Zigbee.begin(role)) {
    Serial.println("Zigbee failed to start! Rebooting...");
    ESP.restart();
  }

  Serial.println("Setup done — starting channel scan...");
  Zigbee.scanNetworks(1UL << nextChan, scanTime);
}

void loop() {
  int16_t status = Zigbee.scanComplete();

  if (status < 0) {
    if (status == ZB_SCAN_FAILED) {
      Serial.printf("Channel %d scan failed — retrying\n", nextChan);
      Zigbee.scanNetworks(1UL << nextChan, scanTime);
    } else {
      // ZB_SCAN_RUNNING
      delay(0);
    }
  } else {
    // scan done: status == number of networks found
    Serial.printf("\n=== Channel %d Results ===\n", nextChan);
    printScannedNetworks(status);

    // move to next channel
    nextChan++;
    if (nextChan > 26) {
      Serial.println(">>> All channels scanned. Waiting 10 s before restart.");
      nextChan = 11;
      delay(10000);
    }

    // start next channel
    Zigbee.scanNetworks(1UL << nextChan, scanTime);
  }
}
