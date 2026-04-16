#include <WiFi.h>
#include <esp_now.h>
#include <DHT.h>

#define DHTPIN 4
#define DHTTYPE DHT22

// -----------------------------
// Pin de humedad del suelo
// -----------------------------
const int soilSensorPin = 33;

DHT dht(DHTPIN, DHTTYPE);

// -----------------------------
// MAC real del puente
// -----------------------------
uint8_t bridgeMAC[] = {0x78, 0x1C, 0x3C, 0x2C, 0x63, 0xB8};

// -----------------------------
// Temporizadores
// -----------------------------
unsigned long lastTelemetry = 0;
unsigned long lastHeartbeat = 0;

// Contador de mensajes
uint32_t messageCounter = 0;

// -----------------------------
// Estructura del mensaje
// -----------------------------
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

// =====================================================
// Callback de envío ESP-NOW
// Ajustado para ESP32 core 3.3.7
// =====================================================
void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("ESP-NOW envío: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FALLÓ");
}

// =====================================================
// Enviar telemetría
// value1 = temperatura
// value2 = humedad del aire
// value3 = humedad del suelo en %
// value4 = humedad del suelo cruda
// =====================================================
void sendTelemetry() {
  float hum = dht.readHumidity();
  float temp = dht.readTemperature();

  if (isnan(hum) || isnan(temp)) {
    Serial.println("Error leyendo AM2302 del nodo externo");
    return;
  }

  int soilRaw = analogRead(soilSensorPin);

  // Ajusta estos valores con pruebas reales
  int valorSeco = 3200;
  int valorHumedo = 1400;

  int soilPct = map(soilRaw, valorSeco, valorHumedo, 0, 100);

  if (soilPct < 0) soilPct = 0;
  if (soilPct > 100) soilPct = 100;

  MessagePacket pkt;
  pkt.sourceId = 2;
  pkt.targetId = 4;
  pkt.msgType = 1; // telemetría
  pkt.command = 0;
  pkt.counter = ++messageCounter;
  pkt.value1 = temp;
  pkt.value2 = hum;
  pkt.value3 = soilPct;
  pkt.value4 = soilRaw;
  pkt.status1 = 1;

  esp_now_send(bridgeMAC, (uint8_t*)&pkt, sizeof(pkt));

  Serial.println("----- TELEMETRÍA NODO EXTERNO -----");
  Serial.print("Temperatura: ");
  Serial.println(temp);
  Serial.print("Humedad aire: ");
  Serial.println(hum);
  Serial.print("Humedad suelo %: ");
  Serial.println(soilPct);
  Serial.print("Humedad suelo raw: ");
  Serial.println(soilRaw);
}

// =====================================================
// Enviar heartbeat
// =====================================================
void sendHeartbeat() {
  MessagePacket hb;

  hb.sourceId = 2;
  hb.targetId = 4;
  hb.msgType = 4; // heartbeat
  hb.command = 0;
  hb.counter = ++messageCounter;
  hb.value1 = 0;
  hb.value2 = 0;
  hb.value3 = 0;
  hb.value4 = 0;
  hb.status1 = 1;

  esp_now_send(bridgeMAC, (uint8_t*)&hb, sizeof(hb));
  Serial.println("Heartbeat del nodo externo enviado");
}

// =====================================================
// Setup
// =====================================================
void setup() {
  Serial.begin(115200);

  pinMode(soilSensorPin, INPUT);
  analogReadResolution(12);

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error iniciando ESP-NOW");
    while (true);
  }

  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, bridgeMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Error agregando el puente como peer");
    while (true);
  }

  Serial.println("Nodo externo listo");
}

// =====================================================
// Loop
// =====================================================
void loop() {
  // Telemetría cada 5 segundos
  if (millis() - lastTelemetry >= 5000) {
    lastTelemetry = millis();
    sendTelemetry();
  }

  // Heartbeat cada 10 segundos
  if (millis() - lastHeartbeat >= 10000) {
    lastHeartbeat = millis();
    sendHeartbeat();
  }
}
