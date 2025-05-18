#define CONFIG_ZB_ENABLED 1

#include <Arduino.h>
#include <Zigbee.h>
#include "ZigbeeCore.h"

#include <esp_zigbee_secur.h>          
#include <esp_zigbee_core.h>            
#include <zdo/esp_zigbee_zdo_common.h> 
#include <esp_err.h>

#define SENSOR_EP          10
#define BATCH_CLUSTER_ID   0x1234
#define PROFILE_HA         ESP_ZB_AF_HA_PROFILE_ID
#define REPORT_INTERVAL_MS 2000
#define BATCH_SIZE         5
#define BATCH_INTERVAL_MS  10000

static const esp_zb_ieee_addr_t downstream_addr = {
  0x40,0x4C,0xCA,0xFF,0xFE,0x4F,0x16,0x00
};

struct Report {
  uint16_t cluster;
  uint8_t  len;
  uint8_t  data[16];
};

static std::vector<Report> buffer;
static unsigned long       lastSend = 0;

void sendBatch() {
  uint8_t payload[BATCH_SIZE*(3+16)] = {0};
  size_t  offset = 0;
  for (auto &r : buffer) {
    payload[offset++] = uint8_t(r.cluster & 0xFF);
    payload[offset++] = uint8_t(r.cluster >> 8);
    payload[offset++] = r.len;
    memcpy(&payload[offset], r.data, r.len);
    offset += r.len;
  }
  esp_zb_apsde_data_req_t req = {};
  req.dst_addr_mode = ESP_ZB_APS_ADDR_MODE_64_ENDP_PRESENT;
  memcpy(req.dst_addr.addr_long, downstream_addr, sizeof(downstream_addr));
  req.dst_endpoint = 1;                  
  req.src_endpoint = SENSOR_EP;
  req.profile_id   = PROFILE_HA;
  req.cluster_id   = BATCH_CLUSTER_ID;
  req.asdu         = payload;
  req.asdu_length  = offset;
  req.tx_options   = ESP_ZB_APSDE_TX_OPT_ACK_TX;
  req.radius       = 0;

  esp_zb_lock_acquire(portMAX_DELAY);
  esp_err_t err = esp_zb_aps_data_request(&req);
  esp_zb_lock_release();

  if (err == ESP_OK) {
    Serial.printf("ED: Sent batch of %u reports (%u bytes)\n", buffer.size(), offset);
    buffer.clear();
    lastSend = millis();
  } else {
    Serial.printf("ED: Batch send failed: %d\n", err);
  }
}

static void temperatureTask(void *arg) {
  for (;;) {
    float ts = temperatureRead();
    Serial.printf("ED: Measured %.2f °C\n", ts);
    Report r{BATCH_CLUSTER_ID, sizeof(ts), {0}};
    memcpy(r.data, &ts, r.len);
    buffer.push_back(r);

    if (buffer.size() >= BATCH_SIZE) sendBatch();
    delay(REPORT_INTERVAL_MS);
  }
}

void setup() {
  Serial.begin(115200);
  if (!Zigbee.begin(ZIGBEE_END_DEVICE)) {
    Serial.println("ED: Zigbee failed to start!");
    while (1) delay(1000);
  }
  Serial.println("ED: Zigbee up");

  // allow join
  esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
  lastSend = millis();

  xTaskCreate(temperatureTask, "tempTask", 2048, nullptr, 1, nullptr);
}

void loop() {
  if (!buffer.empty() && millis() - lastSend >= BATCH_INTERVAL_MS) {
    sendBatch();
  }
}
