/*******************************************************
   PROYECTO: PARAÍSO VERDE
   NODO EXTERNO - MONITOREO AMBIENTAL Y SUELO

   ¿Qué hace este programa?
   - Lee temperatura del aire con AM2302
   - Lee humedad del aire con AM2302
   - Lee humedad del suelo por entrada analógica
   - Envía datos a Blynk
   - Envía datos a ThingSpeak
   - Se reconecta si pierde WiFi o Blynk

   Pensado para ESP32
*******************************************************/

// ===============================
// 1. DATOS DE BLYNK
// ===============================

#define BLYNK_TEMPLATE_ID "TMPL2BUGZ5Xkj"
#define BLYNK_TEMPLATE_NAME "externo"
#define BLYNK_AUTH_TOKEN "ZezgFRitNKySNN4yAMXHA5MvtjGtOE13"

// ===============================
// 2. LIBRERÍAS
// ===============================
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <ThingSpeak.h>
#include <DHT.h>

// ===============================
// 3. PINES DEL ESP32
// ===============================
#define DHTPIN 4
#define DHTTYPE DHT22          // AM2302 = DHT22

const int soilSensorPin = 33;  // Pin analógico para humedad del suelo

// Crear objeto del sensor AM2302
DHT dht(DHTPIN, DHTTYPE);

// ===============================
// 4. VARIABLES DE MEDICIÓN
// ===============================
float humidity = 0.0;          // Humedad del aire
float temperature = 0.0;       // Temperatura del aire
int soilMoistureRaw = 0;       // Valor crudo del sensor
int soilMoisturePct = 0;       // Porcentaje estimado de humedad del suelo
bool am2302ReadSuccess = false;

// ===============================
// 5. DATOS DE WIFI
// ===============================
const char* ssid = "ParaisoVerde2026";
const char* password = "RAS.2026";

// ===============================
// 6. DATOS DE THINGSPEAK
// ===============================
// OJO:
// El nodo interno ya usa varios campos.
// Para evitar enredos, aquí dejo:
// Field 6 = humedad del aire
// Field 7 = temperatura
// Field 8 = humedad del suelo (%)
//
// Si prefieres, mejor usa OTRO canal distinto para el nodo externo.
WiFiClient client;
unsigned long myChannelNumber = 2645204;
const char * myWriteAPIKey = "0I78ZAWCIPZ92VJW";

// ===============================
// 7. TEMPORIZADORES
// ===============================
BlynkTimer timer;

// =====================================================
// 8. CONECTAR WIFI INSISTIENDO
// =====================================================
void connectWiFiForever() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);

  Serial.println("======================================");
  Serial.println("Nodo externo - Conexión WiFi");
  Serial.print("Intentando conectar a: ");
  Serial.println(ssid);
  Serial.println("======================================");

  unsigned long lastReconnectAttempt = 0;

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    if (millis() - lastReconnectAttempt > 15000) {
      lastReconnectAttempt = millis();
      Serial.println("\nReintentando conexión WiFi...");
      WiFi.disconnect(true, true);
      delay(1000);
      WiFi.begin(ssid, password);
    }
  }

  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  Serial.println("\n======================================");
  Serial.println("WiFi conectado correctamente.");
  Serial.print("IP local: ");
  Serial.println(WiFi.localIP());
  Serial.print("Señal RSSI: ");
  Serial.println(WiFi.RSSI());
  Serial.println("======================================");
}

// =====================================================
// 9. REVISAR WIFI
// =====================================================
void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado. Reconectando...");
    connectWiFiForever();
  }
}

// =====================================================
// 10. CONECTAR BLYNK
// =====================================================
void connectBlynk() {
  Serial.println("Conectando a Blynk...");
  Blynk.config(BLYNK_AUTH_TOKEN);

  int intentos = 0;
  while (!Blynk.connected() && intentos < 10) {
    if (Blynk.connect(3000)) {
      Serial.println("Blynk conectado correctamente.");
      return;
    } else {
      Serial.println("No se pudo conectar a Blynk. Reintentando...");
    }
    intentos++;
    delay(1000);
  }

  Serial.println("Blynk no conectó por ahora.");
}

// =====================================================
// 11. REVISAR CONEXIONES
// =====================================================
void checkConnections() {
  checkWiFiConnection();

  if (WiFi.status() == WL_CONNECTED && !Blynk.connected()) {
    Serial.println("Blynk desconectado. Reconectando...");
    connectBlynk();
  }
}

// =====================================================
// 12. LEER SENSOR AM2302
// =====================================================
void readAM2302Sensor() {
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Error leyendo el sensor AM2302");
    am2302ReadSuccess = false;
  } else {
    am2302ReadSuccess = true;

    Serial.print("Humedad del aire: ");
    Serial.print(humidity);
    Serial.print(" % | Temperatura: ");
    Serial.print(temperature);
    Serial.println(" °C");
  }
}

// =====================================================
// 13. LEER HUMEDAD DEL SUELO
// =====================================================
// Este valor depende del sensor y debe calibrarse.
// En muchos sensores:
// - valor alto = suelo seco
// - valor bajo = suelo húmedo
//
// Aquí convertimos el valor analógico a un porcentaje estimado.
// Tendrás que ajustar los límites en pruebas reales.
void readSoilMoisture() {
  soilMoistureRaw = analogRead(soilSensorPin);

  // Ajusta estos valores según tu sensor real
  int valorSeco = 3200;    // ejemplo de suelo seco
  int valorHumedo = 1400;  // ejemplo de suelo húmedo

  soilMoisturePct = map(soilMoistureRaw, valorSeco, valorHumedo, 0, 100);

  // Limitar el valor entre 0 y 100
  if (soilMoisturePct < 0) soilMoisturePct = 0;
  if (soilMoisturePct > 100) soilMoisturePct = 100;

  Serial.print("Humedad del suelo cruda: ");
  Serial.print(soilMoistureRaw);
  Serial.print(" | Humedad del suelo estimada: ");
  Serial.print(soilMoisturePct);
  Serial.println(" %");
}

// =====================================================
// 14. ENVIAR DATOS A THINGSPEAK
// =====================================================
void sendDataToThingSpeak() {
  if (WiFi.status() != WL_CONNECTED) return;

  ThingSpeak.setField(6, humidity);         // Humedad del aire
  ThingSpeak.setField(7, temperature);      // Temperatura
  ThingSpeak.setField(8, soilMoisturePct);  // Humedad del suelo en %

  int result = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

  if (result == 200) {
    Serial.println("Datos enviados correctamente a ThingSpeak.");
  } else {
    Serial.print("Error al enviar a ThingSpeak. Código: ");
    Serial.println(result);
  }
}

// =====================================================
// 15. ENVIAR DATOS A BLYNK
// =====================================================
// Puedes organizar los pines virtuales como quieras.
// Aquí dejo:
// V1 = temperatura
// V2 = humedad del aire
// V3 = humedad del suelo
void sendDataToBlynk() {
  if (!Blynk.connected()) return;

  Blynk.virtualWrite(V1, temperature);
  Blynk.virtualWrite(V2, humidity);
  Blynk.virtualWrite(V3, soilMoisturePct);
}

// =====================================================
// 16. SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("======================================");
  Serial.println("Iniciando NODO EXTERNO - Paraíso Verde");
  Serial.println("======================================");

  pinMode(soilSensorPin, INPUT);

  // Configurar resolución ADC del ESP32
  analogReadResolution(12); // valores de 0 a 4095

  // Iniciar sensor AM2302
  dht.begin();

  // Conectar WiFi
  connectWiFiForever();

  // Iniciar ThingSpeak
  ThingSpeak.begin(client);

  // Iniciar Blynk
  connectBlynk();

  // Temporizadores
  timer.setInterval(2000L, readAM2302Sensor);       // leer aire cada 2 s
  timer.setInterval(2000L, readSoilMoisture);       // leer suelo cada 2 s
  timer.setInterval(1000L, sendDataToBlynk);        // enviar a Blynk cada 1 s
  timer.setInterval(15000L, sendDataToThingSpeak);  // enviar a ThingSpeak cada 15 s
  timer.setInterval(10000L, checkConnections);      // revisar conexiones cada 10 s

  Serial.println("Nodo externo iniciado correctamente.");
}

// =====================================================
// 17. LOOP
// =====================================================
void loop() {
  if (Blynk.connected()) {
    Blynk.run();
  }

  timer.run();

  // Pequeña pausa para estabilidad
  delay(50);
}
