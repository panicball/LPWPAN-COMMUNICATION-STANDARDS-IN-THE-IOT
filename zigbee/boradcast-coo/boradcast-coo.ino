// Zigbee_Scan_Networks Example
// Tests broadcast at ZDO layer (Mgmt_NWK_Scan)

#include <Zigbee.h>

// router role to enable network scanning
zigbee_role_t role = ZIGBEE_ROUTER;

// Helper to print scan results
template<typename T>
void printScannedNetworks(T *scan_result, uint16_t networksFound) {
  if (networksFound == 0) {
    Serial.println("No networks found");
  } else {
    Serial.println("\nScan complete");
    Serial.print(networksFound);
    Serial.println(" networks found:");
    Serial.println("Nr | PAN ID | CH | Permit Join | Router Cap | ED Cap | Extended PAN ID");
    for (uint16_t i = 0; i < networksFound; ++i) {
      Serial.printf("%2d | 0x%04hx | %2d | %-11s | %-10s | %-7s | %02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X\n",
        i + 1,
        scan_result[i].short_pan_id,
        scan_result[i].logic_channel,
        scan_result[i].permit_joining ? "Yes" : "No",
        scan_result[i].router_capacity ? "Yes" : "No",
        scan_result[i].end_device_capacity ? "Yes" : "No",
        scan_result[i].extended_pan_id[7], scan_result[i].extended_pan_id[6],
        scan_result[i].extended_pan_id[5], scan_result[i].extended_pan_id[4],
        scan_result[i].extended_pan_id[3], scan_result[i].extended_pan_id[2],
        scan_result[i].extended_pan_id[1], scan_result[i].extended_pan_id[0]
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

  // Initialize Zigbee stack for scanning as router
  if (!Zigbee.begin(role, true)) {
    Serial.println("Zigbee failed to start! Rebooting...");
    ESP.restart();
  }

  Serial.println("Setup done, starting network scan...");
  // Broadcast a Mgmt_NWK_Scan request
  Zigbee.scanNetworks();
}

void loop() {
  int16_t status = Zigbee.scanComplete();

  if (status < 0) {
    if (status == ZB_SCAN_FAILED) {
      Serial.println("Scan failed, retrying...");
      Zigbee.scanNetworks();
    }
  } else {
    zigbee_scan_result_t *results = Zigbee.getScanResult();
    printScannedNetworks(results, status);

    // uint8_t fastScanDuration = 3;
    // Zigbee.scanNetworks(/*mask=*/0x07FFF800, fastScanDuration);

    // uint8_t slowScanDuration = 7;
    // Zigbee.scanNetworks(/*mask=*/0x07FFF800, slowScanDuration);


    // Zigbee.scanNetworks();

    // Build a mask for channels 15,16,17,18,19,20
    uint32_t mask = 0;
    for (int ch = 15; ch <= 20; ++ch) {
      mask |= (1UL << ch);
    }
    Zigbee.scanNetworks(mask, 4);

  }
  delay(500);
}
