#ifndef ZIGBEE_MODE_ZCZR
  #error "Zigbee coordinator mode is not selected in Tools->Zigbee mode"
#endif

#include "Zigbee.h"
#include <Arduino.h>

#define GATEWAY_ENDPOINT_NUMBER 1
#define GATEWAY_RCP_UART_PORT    UART_NUM_1
#define GATEWAY_RCP_RX_PIN       4
#define GATEWAY_RCP_TX_PIN       5

class MyGateway : public ZigbeeGateway {
public:
  MyGateway(uint8_t ep) : ZigbeeGateway(ep) {}

  void addBoundDevice(zb_device_params_t *device) override {
    ZigbeeGateway::addBoundDevice(device);
    for (int i = 0; i < 8; i++) {
      Serial.printf("%02X", device->ieee_addr[7 - i]);
    }
    Serial.println();
  }

  void zbAttributeSet(const esp_zb_zcl_set_attr_value_message_t *message) override {
    uint8_t ep       = message->info.dst_endpoint;
    uint16_t cluster = message->info.cluster;
    uint16_t attr    = message->attribute.id;
    uint8_t len      = message->attribute.data.size;
    const uint8_t *data = (const uint8_t *)message->attribute.data.value;

    Serial.printf("Report EP %d, Cluster 0x%04X, Attr 0x%04X, %d bytes: ", ep, cluster, attr, len);
    for (uint8_t i = 0; i < len; i++) {
      Serial.printf("%02X ", data[i]);
    }
    Serial.println();

    const uint16_t ANALOG_INPUT_CLUSTER = 0x000C;
    const uint16_t ATTR_PRESENT_VALUE   = 0x0055;
    if (cluster == ANALOG_INPUT_CLUSTER && attr == ATTR_PRESENT_VALUE && len >= 2) {
      int16_t raw = (int16_t)((data[1] << 8) | data[0]);
      Serial.printf("Parsed analog value: %d\r\n", raw);
    }
  }
};

MyGateway zbGateway(GATEWAY_ENDPOINT_NUMBER);

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Zigbee Coordinator (No Wi‑Fi)…");

  Zigbee.addEndpoint(&zbGateway);
  ZigbeeEP::allowMultipleBinding(true);
  Zigbee.setRebootOpenNetwork(180);

  esp_zb_radio_config_t radio_cfg = ZIGBEE_DEFAULT_UART_RCP_RADIO_CONFIG();
  radio_cfg.radio_uart_config.port   = GATEWAY_RCP_UART_PORT;
  radio_cfg.radio_uart_config.rx_pin = (gpio_num_t)GATEWAY_RCP_RX_PIN;
  radio_cfg.radio_uart_config.tx_pin = (gpio_num_t)GATEWAY_RCP_TX_PIN;
  Zigbee.setRadioConfig(radio_cfg);

  if (!Zigbee.begin(ZIGBEE_COORDINATOR)) {
    Serial.println("Zigbee failed to start, rebooting…");
    ESP.restart();
  }
  Serial.println("Zigbee started successfully!");
}

void loop() {
  delay(100);
}
