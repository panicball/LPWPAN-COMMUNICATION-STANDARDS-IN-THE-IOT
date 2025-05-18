#if !defined(ZIGBEE_MODE_ED) && !defined(ZIGBEE_MODE_ZCZR)
#error "Zigbee device mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"

#ifdef ZIGBEE_MODE_ZCZR
zigbee_role_t role = ZIGBEE_ROUTER;
#else
zigbee_role_t role = ZIGBEE_END_DEVICE;
#endif

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
      // Print all available info for each network found
      Serial.printf("%2d", i + 1);
      Serial.print(" | ");
      Serial.printf("0x%04hx", scan_result[i].short_pan_id);
      Serial.print(" | ");
      Serial.printf("%2d", scan_result[i].logic_channel);
      Serial.print(" | ");
      Serial.printf("%-14.14s", scan_result[i].permit_joining ? "Yes" : "No");
      Serial.print(" | ");
      Serial.printf("%-15.15s", scan_result[i].router_capacity ? "Yes" : "No");
      Serial.print(" | ");
      Serial.printf("%-19.19s", scan_result[i].end_device_capacity ? "Yes" : "No");
      Serial.print(" | ");
      Serial.printf(
        "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x", scan_result[i].extended_pan_id[7], scan_result[i].extended_pan_id[6], scan_result[i].extended_pan_id[5],
        scan_result[i].extended_pan_id[4], scan_result[i].extended_pan_id[3], scan_result[i].extended_pan_id[2], scan_result[i].extended_pan_id[1],
        scan_result[i].extended_pan_id[0]
      );
      Serial.println();
      delay(10);
    }
    Serial.println("");
    Zigbee.scanDelete();
  }
}

void setup() {
  Serial.begin(115200);

  if (!Zigbee.begin(role)) {
    Serial.println("Zigbee failed to start!");
    Serial.println("Rebooting...");
    ESP.restart();
  }

  Serial.println("Setup done, starting Zigbee network scan...");
  Zigbee.scanNetworks();
}

void loop() {
  int16_t ZigbeeScanStatus = Zigbee.scanComplete();
  if (ZigbeeScanStatus < 0) {
    if (ZigbeeScanStatus == ZB_SCAN_FAILED) {
      Serial.println("Zigbee scan has failed. Starting again.");
      Zigbee.scanNetworks();
    }
  } else {
    printScannedNetworks(ZigbeeScanStatus);
    Zigbee.scanNetworks();
  }
  delay(500);
  Serial.println("Loop running...");
}