#define BLYNK_TEMPLATE_ID "TMPL2AllzeEEG"
#define BLYNK_TEMPLATE_NAME "externo"
#define BLYNK_AUTH_TOKEN "zbvxEsyGWD92KVMDYJFpy0_8Invth9XG"

#include <WiFi.h>
#include <SPI.h>
#include <LoRa.h>
#include <ThingSpeak.h>
#include <BlynkSimpleEsp32.h>

#define LORA_SS   5
#define LORA_RST  32
#define LORA_DIO0 33

const char* ssid = "IE. RAMON ALVARADO 2025";
const char* password = "RamonAlvarado2025.";

WiFiClient client;
unsigned long myChannelNumber = 2645204;
const char * myWriteAPIKey = "0I78ZAWCIPZ92VJW";

BlynkTimer timer;

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

float tInt=0, hInt=0, caudalManual=0, caudalGoteo=0;
float tExt=0, hExt=0, sueloPct=0;
bool valveConfirmed = false;
bool desiredValveState = false;
bool autoMode = true;

unsigned long lastInternal = 0;
unsigned long lastExternal = 0;
unsigned long lastBridge = 0;
unsigned long lastAck = 0;
unsigned long lastAutoDecision = 0;
uint32_t rectoriaCounter = 0;

bool statusInternal = false;
bool statusExternal = false;
bool statusBridge = false;
bool statusRectoria = false;

void connectWiFiForever() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado");
}

void connectBlynk() {
  Blynk.config(BLYNK_AUTH_TOKEN);
  while (!Blynk.connected()) {
    if (Blynk.connect(3000)) break;
    delay(1000);
  }
}

void sendLoRaCommand(uint8_t targetNode, uint8_t cmd) {
  MessagePacket pkt;
  pkt.sourceId = 4;
  pkt.targetId = targetNode;
  pkt.msgType = 2;
  pkt.command = cmd;
  pkt.counter = ++rectoriaCounter;
  pkt.value1 = 0;
  pkt.value2 = 0;
  pkt.value3 = 0;
  pkt.value4 = 0;
  pkt.status1 = 1;

  LoRa.beginPacket();
  LoRa.write((uint8_t*)&pkt, sizeof(pkt));
  LoRa.endPacket();

  Serial.print("Comando enviado por LoRa: ");
  Serial.println(cmd == 2 ? "ENCENDER" : "APAGAR");
}

void updateStatuses() {
  unsigned long now = millis();
  statusInternal = (now - lastInternal < 20000);
  statusExternal = (now - lastExternal < 20000);
  statusBridge   = (now - lastBridge < 20000);
  statusRectoria = (WiFi.status() == WL_CONNECTED);

  Blynk.virtualWrite(V10, statusInternal ? 1 : 0);
  Blynk.virtualWrite(V11, statusExternal ? 1 : 0);
  Blynk.virtualWrite(V12, statusBridge ? 1 : 0);
  Blynk.virtualWrite(V13, statusRectoria ? 1 : 0);
  Blynk.virtualWrite(V14, valveConfirmed ? 1 : 0);
  Blynk.virtualWrite(V15, autoMode ? 1 : 0);
}

void updateBlynkData() {
  Blynk.virtualWrite(V1, tInt);
  Blynk.virtualWrite(V2, hInt);
  Blynk.virtualWrite(V3, caudalManual);
  Blynk.virtualWrite(V4, caudalGoteo);
  Blynk.virtualWrite(V5, tExt);
  Blynk.virtualWrite(V6, hExt);
  Blynk.virtualWrite(V7, sueloPct);
}

void sendThingSpeak() {
  ThingSpeak.setField(1, tInt);
  ThingSpeak.setField(2, hInt);
  ThingSpeak.setField(3, caudalManual);
  ThingSpeak.setField(4, caudalGoteo);
  ThingSpeak.setField(5, tExt);
  ThingSpeak.setField(6, hExt);
  ThingSpeak.setField(7, sueloPct);
  ThingSpeak.setField(8, valveConfirmed ? 1 : 0);

  int result = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  Serial.print("ThingSpeak: ");
  Serial.println(result);
}

// “IA” de primera versión: basada en reglas
void decisionIA() {
  if (!autoMode) return;

  // Seguridad primero
  if (!(statusInternal && statusExternal && statusBridge && statusRectoria)) {
    desiredValveState = false;
    sendLoRaCommand(1, 1);
    Serial.println("IA: apagado por seguridad de comunicación");
    return;
  }

  // Regla simple: si suelo muy seco y temperatura alta, encender
  if (sueloPct < 40 && tExt > 24) {
    if (!desiredValveState) {
      desiredValveState = true;
      sendLoRaCommand(1, 2);
      Serial.println("IA: orden de encendido");
    }
  }
  // Si el suelo ya está húmedo, apagar
  else if (sueloPct >= 60) {
    if (desiredValveState) {
      desiredValveState = false;
      sendLoRaCommand(1, 1);
      Serial.println("IA: orden de apagado");
    }
  }

  // Si la válvula está encendida pero no hay confirmación reciente, apagar
  if (desiredValveState && millis() - lastAck > 15000) {
    desiredValveState = false;
    sendLoRaCommand(1, 1);
    Serial.println("IA: apagado por falta de confirmación");
  }
}

void receiveLoRa() {
  int packetSize = LoRa.parsePacket();
  if (packetSize != sizeof(MessagePacket)) return;

  MessagePacket pkt;
  LoRa.readBytes((uint8_t*)&pkt, sizeof(pkt));

  if (pkt.sourceId == 1 && pkt.msgType == 1) {
    tInt = pkt.value1;
    hInt = pkt.value2;
    caudalManual = pkt.value3;
    caudalGoteo = pkt.value4;
    valveConfirmed = pkt.status1;
    lastInternal = millis();
    Serial.println("Telemetría interna recibida");
  }
  else if (pkt.sourceId == 2 && pkt.msgType == 1) {
    tExt = pkt.value1;
    hExt = pkt.value2;
    sueloPct = pkt.value3;
    lastExternal = millis();
    Serial.println("Telemetría externa recibida");
  }
  else if (pkt.sourceId == 3 && pkt.msgType == 4) {
    lastBridge = millis();
    Serial.println("Heartbeat del puente");
  }
  else if (pkt.sourceId == 1 && pkt.msgType == 3) {
    valveConfirmed = pkt.value1 > 0.5;
    lastAck = millis();
    lastInternal = millis();
    Serial.print("ACK nodo interno, válvula: ");
    Serial.println(valveConfirmed ? "ENCENDIDA" : "APAGADA");
  }
}

void checkConnections() {
  if (WiFi.status() != WL_CONNECTED) connectWiFiForever();
  if (!Blynk.connected()) connectBlynk();
}

BLYNK_WRITE(V20) {
  int val = param.asInt();
  autoMode = (val == 1);
  Serial.print("Modo automático: ");
  Serial.println(autoMode ? "ON" : "OFF");
}

BLYNK_WRITE(V21) {
  int val = param.asInt();
  if (!autoMode) {
    if (val == 1) {
      desiredValveState = true;
      sendLoRaCommand(1, 2);
    } else {
      desiredValveState = false;
      sendLoRaCommand(1, 1);
    }
  }
}

void setup() {
  Serial.begin(115200);

  connectWiFiForever();
  ThingSpeak.begin(client);
  connectBlynk();

  SPI.begin(18, 19, 23, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("Error iniciando LoRa RA-01");
    while (true);
  }

  LoRa.setSyncWord(0xF3);

  timer.setInterval(2000L, updateBlynkData);
  timer.setInterval(5000L, updateStatuses);
  timer.setInterval(7000L, decisionIA);
  timer.setInterval(15000L, sendThingSpeak);
  timer.setInterval(10000L, checkConnections);

  Serial.println("Nodo rectoría listo");
}

void loop() {
  if (Blynk.connected()) Blynk.run();
  timer.run();
  receiveLoRa();
}
