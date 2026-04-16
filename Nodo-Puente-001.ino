#include <WiFi.h>
#include <esp_now.h>
#include <SPI.h>
#include <LoRa.h>

#define LORA_SS   5
#define LORA_RST  14
#define LORA_DIO0 26

// CAMBIA ESTA MAC POR LA DEL NODO INTERNO
uint8_t internalMAC[] = {0x24, 0x6F, 0x28, 0x11, 0x22, 0x33};

typedef struct {
  uint8_t sourceId;
  uint8_t targetId;
  uint8_t msgType;
  uint8_t command;
  uint32_t counter;
  float value1;
  float value2;
  float value3;
  float value4;
  uint8_t status1;
} MessagePacket;

unsigned long lastHeartbeat = 0;
uint32_t bridgeCounter = 0;

void onDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  if (len != sizeof(MessagePacket)) return;

  MessagePacket pkt;
  memcpy(&pkt, data, sizeof(pkt));

  LoRa.beginPacket();
  LoRa.write((uint8_t*)&pkt, sizeof(pkt));
  LoRa.endPacket();

  Serial.print("ESP-NOW -> LoRa | source: ");
  Serial.println(pkt.sourceId);
}

void sendBridgeHeartbeat() {
  MessagePacket hb;
  hb.sourceId = 3;
  hb.targetId = 4;
  hb.msgType = 4;
  hb.command = 0;
  hb.counter = ++bridgeCounter;
  hb.value1 = 0;
  hb.value2 = 0;
  hb.value3 = 0;
  hb.value4 = 0;
  hb.status1 = 1;

  LoRa.beginPacket();
  LoRa.write((uint8_t*)&hb, sizeof(hb));
  LoRa.endPacket();
}

void checkLoRaCommands() {
  int packetSize = LoRa.parsePacket();
  if (packetSize != sizeof(MessagePacket)) return;

  MessagePacket pkt;
  LoRa.readBytes((uint8_t*)&pkt, sizeof(pkt));

  if (pkt.msgType == 2 && pkt.targetId == 1) {
    esp_now_send(internalMAC, (uint8_t*)&pkt, sizeof(pkt));
    Serial.println("Comando LoRa recibido y reenviado al nodo interno");
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  Serial.print("MAC del puente: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error iniciando ESP-NOW");
    while (true);
  }

  esp_now_register_recv_cb(onDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, internalMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  SPI.begin(18, 19, 23, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("Error iniciando LoRa RA-01");
    while (true);
  }

  LoRa.setSyncWord(0xF3);
  Serial.println("Nodo puente listo");
}

void loop() {
  if (millis() - lastHeartbeat >= 10000) {
    lastHeartbeat = millis();
    sendBridgeHeartbeat();
    Serial.println("Heartbeat puente enviado");
  }

  checkLoRaCommands();
}
