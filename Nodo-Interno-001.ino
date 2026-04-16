#include <WiFi.h>
#include <esp_now.h>
#include <DHT.h>

#define DHTPIN 4
#define DHTTYPE DHT22

// -----------------------------
// Pines del nodo interno
// -----------------------------
const int sensorManualPin = 15;   // Sensor de flujo manual
const int sensorGoteoPin  = 14;   // Sensor de flujo goteo
const int relayPin        = 5;    // Relé / electroválvula

DHT dht(DHTPIN, DHTTYPE);

// -----------------------------
// Contadores de pulsos
// -----------------------------
volatile int pulseCountManual = 0;
volatile int pulseCountGoteo  = 0;

// -----------------------------
// Variables medidas
// -----------------------------
float temperature = 0.0;
float humidity = 0.0;
float flowManual = 0.0;
float flowGoteo = 0.0;

// -----------------------------
// Estado de válvula
// -----------------------------
bool valveState = false;

// -----------------------------
// Temporizadores
// -----------------------------
unsigned long lastTelemetry = 0;
unsigned long lastHeartbeat = 0;
unsigned long lastCommandTime = 0;

// Contador de mensajes
uint32_t messageCounter = 0;

// -----------------------------
// MAC real del nodo puente
// -----------------------------
uint8_t bridgeMAC[] = {0x78, 0x1C, 0x3C, 0x2C, 0x63, 0xB8};

// -----------------------------
// Estructura del mensaje
// -----------------------------
typedef struct {
  uint8_t sourceId;   // quién envía
  uint8_t targetId;   // quién debe recibir
  uint8_t msgType;    // 1 telemetría, 2 comando, 3 confirmación, 4 heartbeat
  uint8_t command;    // 0 ninguno, 1 apagar válvula, 2 encender válvula
  uint32_t counter;   // consecutivo de mensaje
  float value1;
  float value2;
  float value3;
  float value4;
  uint8_t status1;    // estado adicional
} MessagePacket;

MessagePacket pkt;

// =====================================================
// Interrupciones de flujo
// =====================================================
void IRAM_ATTR pulseCounterManual() {
  pulseCountManual = pulseCountManual + 1;
}

void IRAM_ATTR pulseCounterGoteo() {
  pulseCountGoteo = pulseCountGoteo + 1;
}

// =====================================================
// Callback de envío ESP-NOW
// Ajustado para ESP32 core 3.3.7
// =====================================================
void onDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("ESP-NOW envío: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "OK" : "FALLÓ");
}

// =====================================================
// Enviar confirmación de ejecución de comando
// =====================================================
void sendAck(uint8_t commandExecuted) {
  MessagePacket ack;

  ack.sourceId = 1;   // nodo interno
  ack.targetId = 4;   // rectoría
  ack.msgType = 3;    // confirmación
  ack.command = commandExecuted;
  ack.counter = ++messageCounter;
  ack.value1 = valveState ? 1 : 0; // estado real de la válvula
  ack.value2 = flowManual;
  ack.value3 = flowGoteo;
  ack.value4 = 0;
  ack.status1 = 1;

  esp_now_send(bridgeMAC, (uint8_t*)&ack, sizeof(ack));
}

// =====================================================
// Recibir comandos desde el puente
// =====================================================
void onDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
  if (len != sizeof(MessagePacket)) return;

  MessagePacket incoming;
  memcpy(&incoming, data, sizeof(incoming));

  // Solo atender mensajes dirigidos al nodo interno
  if (incoming.targetId != 1) return;

  // Solo atender comandos
  if (incoming.msgType != 2) return;

  lastCommandTime = millis();

  if (incoming.command == 2) {
    digitalWrite(relayPin, HIGH);
    valveState = true;
    Serial.println("Orden recibida: ENCENDER válvula");
    sendAck(2);
  }
  else if (incoming.command == 1) {
    digitalWrite(relayPin, LOW);
    valveState = false;
    Serial.println("Orden recibida: APAGAR válvula");
    sendAck(1);
  }
}

// =====================================================
// Enviar telemetría
// =====================================================
void sendTelemetry() {
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Error leyendo AM2302 del nodo interno");
    return;
  }

  // Conversión aproximada de caudal
  flowManual = pulseCountManual / 7.5;
  flowGoteo  = pulseCountGoteo / (5880.0 / 60.0);

  // Reiniciar contadores después del cálculo
  pulseCountManual = 0;
  pulseCountGoteo  = 0;

  pkt.sourceId = 1;
  pkt.targetId = 4;
  pkt.msgType = 1; // telemetría
  pkt.command = 0;
  pkt.counter = ++messageCounter;
  pkt.value1 = temperature;
  pkt.value2 = humidity;
  pkt.value3 = flowManual;
  pkt.value4 = flowGoteo;
  pkt.status1 = valveState ? 1 : 0;

  esp_now_send(bridgeMAC, (uint8_t*)&pkt, sizeof(pkt));

  Serial.println("----- TELEMETRÍA NODO INTERNO -----");
  Serial.print("Temperatura: ");
  Serial.println(temperature);
  Serial.print("Humedad aire: ");
  Serial.println(humidity);
  Serial.print("Caudal manual: ");
  Serial.println(flowManual);
  Serial.print("Caudal goteo: ");
  Serial.println(flowGoteo);
  Serial.print("Válvula: ");
  Serial.println(valveState ? "ENCENDIDA" : "APAGADA");
}

// =====================================================
// Enviar heartbeat
// =====================================================
void sendHeartbeat() {
  MessagePacket hb;

  hb.sourceId = 1;
  hb.targetId = 4;
  hb.msgType = 4; // heartbeat
  hb.command = 0;
  hb.counter = ++messageCounter;
  hb.value1 = valveState ? 1 : 0;
  hb.value2 = 0;
  hb.value3 = 0;
  hb.value4 = 0;
  hb.status1 = 1;

  esp_now_send(bridgeMAC, (uint8_t*)&hb, sizeof(hb));
  Serial.println("Heartbeat del nodo interno enviado");
}

// =====================================================
// Setup
// =====================================================
void setup() {
  Serial.begin(115200);

  pinMode(sensorManualPin, INPUT_PULLUP);
  pinMode(sensorGoteoPin, INPUT_PULLUP);
  pinMode(relayPin, OUTPUT);

  // Seguridad: al arrancar, válvula apagada
  digitalWrite(relayPin, LOW);
  valveState = false;

  attachInterrupt(digitalPinToInterrupt(sensorManualPin), pulseCounterManual, FALLING);
  attachInterrupt(digitalPinToInterrupt(sensorGoteoPin), pulseCounterGoteo, FALLING);

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error iniciando ESP-NOW");
    while (true);
  }

  esp_now_register_send_cb(onDataSent);
  esp_now_register_recv_cb(onDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, bridgeMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Error agregando el puente como peer");
    while (true);
  }

  lastCommandTime = millis();

  Serial.println("Nodo interno listo");
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

  // Seguridad:
  // si pasan 30 segundos sin comandos/confirmación de control,
  // la válvula se apaga
  if ((millis() - lastCommandTime > 30000) && valveState) {
    digitalWrite(relayPin, LOW);
    valveState = false;
    Serial.println("Seguridad: sin comunicación reciente, válvula apagada");
    sendAck(1);
  }
}
