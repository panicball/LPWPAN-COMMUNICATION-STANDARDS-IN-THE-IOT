#define CONFIG_ZB_ENABLED 1

#include <Arduino.h>
#include <Zigbee.h>
#include "ZigbeeCore.h"

#include <aps/esp_zigbee_aps.h>         
#include <esp_zigbee_secur.h>           
#include <esp_zigbee_core.h>            
#include <zdo/esp_zigbee_zdo_common.h>  

#include <vector>
#include <cstring>
#include <esp_err.h>

#define GATEWAY_EP        1
#define BATCH_CLUSTER_ID  0x1234
#define FLUSH_INTERVAL_MS 10000

struct Report {
  uint16_t cluster;
  uint8_t  len;
  uint8_t  data[16];
};

static std::vector<Report> buffer;
static unsigned long       lastFlush = 0;

bool onApsData(esp_zb_apsde_data_ind_t ind) {
  Serial.printf(
    "APS Ind from NWK 0x%04X EP%d→EP%d CL=0x%04X len=%u\n",
    ind.src_short_addr,
    ind.src_endpoint,
    ind.dst_endpoint,
    ind.cluster_id,
    ind.asdu_length
  );

  if (ind.cluster_id != BATCH_CLUSTER_ID) {
    return false;
  }
  const uint8_t *p   = ind.asdu;
  size_t         rem = ind.asdu_length;
  while (rem >= 3) {
    Report r;
    r.cluster = uint16_t(p[0]) | (uint16_t(p[1]) << 8);
    r.len     = p[2];
    if (r.len > sizeof(r.data) || rem < 3 + r.len) break;
    memcpy(r.data, p + 3, r.len);
    buffer.push_back(r);

    p   += 3 + r.len;
    rem -= 3 + r.len;
  }

  Serial.printf("Received batch → buffered %u samples total\n", buffer.size());
  return true;
}

void setup() {
  Serial.begin(115200);
  // 1) start as Zigbee Coordinator
  if (!Zigbee.begin(ZIGBEE_COORDINATOR)) {
    Serial.println("Coordinator start failed!");
    while (1) delay(1000);
  }
  Serial.println("Coordinator up");

  // 2) allow new EDs to join without handshake
  esp_zb_secur_link_key_exchange_required_set(false);

  // 3) register
  esp_zb_aps_data_indication_handler_register(onApsData);

  // 4) form PAN + permit‑join for 60 s
  esp_err_t err = esp_zb_bdb_start_top_level_commissioning(
    ESP_ZB_BDB_MODE_NETWORK_FORMATION |
    ESP_ZB_BDB_MODE_NETWORK_STEERING
  );
  Serial.printf("Commissioning start (err=%d)\n", err);

  lastFlush = millis();
}

void loop() {
  if (!buffer.empty() && millis() - lastFlush >= FLUSH_INTERVAL_MS) {
    Serial.println("Flushing buffer:");
    for (auto &r : buffer) {
      float val = 0;
      memcpy(&val, r.data, r.len);
      Serial.printf("  cluster 0x%04X → %.2f\n", r.cluster, val);
    }
    buffer.clear();
    lastFlush = millis();
  }
  delay(100);
}
