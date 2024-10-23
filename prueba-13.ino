#define BLYNK_TEMPLATE_ID "TMPL2SVO84cJh"
#define BLYNK_TEMPLATE_NAME "Paraiso Verde"
#define BLYNK_AUTH_TOKEN "-Ao0offnzfwniUF4Mwkz3K2rMVc_Ypsp"



#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <ThingSpeak.h>
#include <DHT.h>

// Pines
const int sensor1Pin = 15;  // Sensor YF-S201
const int sensor2Pin = 14;  // Sensor YF-S401
const int relayPin = 5;     // Relé para la electroválvula
#define DHTPIN 4            // DHT11
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Variables del flujo
volatile int pulseCount1 = 0;
volatile int pulseCount2 = 0;
float flowRate1 = 0.0;
float flowRate2 = 0.0;



const char* ssid[] = {"Elsacamialeja", "moto g54 5G_7085"};
const char* password[] = {"141024cafesita","12345612"};

WiFiClient client;
unsigned long myChannelNumber = 2645204;
const char * myWriteAPIKey = "0I78ZAWCIPZ92VJW";
bool systemOn = true;
BlynkTimer timer;
BlynkTimer valveTimer;  // Temporizador para la actualización del tiempo de la válvula en Blynk

// Variables para la lógica del temporizador
unsigned long valveRemoteStartTime = 0;
unsigned long valveAutoStartTime = 0;
bool remoteValveActive = false;
bool autoValveActive = false;
bool autoMode = false;
unsigned long lastAutoToggleTime = 0;

// Variables para la cuenta regresiva en Blynk
unsigned long valveRemainingTime = 0;  // Tiempo restante para el apagado remoto en milisegundos

// Variables de temperatura y humedad
float humidity = 0.0;
float temperature = 0.0;
bool dhtReadSuccess = false;  // Controlar si la lectura fue exitosa

// Configuración para WiFi: intentamos conectarnos a múltiples redes
void connectWiFi() {
  int wifiAttempt = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempt < 6) {  // Cambié el número máximo de intentos a 6 para probar las 6 redes
    WiFi.begin(ssid[wifiAttempt], password[wifiAttempt]);
    Serial.print("Intentando conectar a ");
    Serial.println(ssid[wifiAttempt]);


  
    
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {  // Esperamos 20 intentos
      delay(500);
      Serial.print(".");
      timeout++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("\nConectado a ");
      Serial.println(ssid[wifiAttempt]);
      
      // **Ajustar la potencia máxima de transmisión WiFi**
      WiFi.setTxPower(WIFI_POWER_19_5dBm);  // ALta potencia de transmisión
      return;
    }
    wifiAttempt++;
  }
  Serial.println("\nNo se pudo conectar a ninguna red.");
}

// Función para leer los sensores de flujo de agua
void IRAM_ATTR pulseCounter1() {
  pulseCount1++;
}

void IRAM_ATTR pulseCounter2() {
  pulseCount2++;
}

// Lectura del sensor DHT11
void readDHTSensor() {
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Error leyendo el sensor DHT11");
    dhtReadSuccess = false;
  } else {
    dhtReadSuccess = true;
    Serial.print("Humedad: ");
    Serial.print(humidity);
    Serial.print(" %\t");
    Serial.print("Temperatura: ");
    Serial.println(temperature);
  }
}

// Enviar datos a ThingSpeak
void sendDataToThingSpeak() {
  flowRate1 = pulseCount1 / 7.5;  // Conversión para YF-S201
  flowRate2 = pulseCount2 / (5880.0 / 60.0);  // Conversión para YF-S401

  ThingSpeak.setField(1, flowRate1);
  ThingSpeak.setField(2, flowRate2);
  ThingSpeak.setField(3, systemOn);
  ThingSpeak.setField(4, humidity);
  ThingSpeak.setField(5, temperature);

  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  
  if (x == 200) {
    Serial.println("Datos enviados correctamente a ThingSpeak.");
  } else {
    Serial.println("Error al enviar los datos a ThingSpeak.");
  }

  pulseCount1 = 0;
  pulseCount2 = 0;
}

// Enviar datos a Blynk
void sendDataToBlynk() {
  Blynk.virtualWrite(V1, temperature);  // Temperatura en Virtual Pin 1
  Blynk.virtualWrite(V2, humidity);     // Humedad en Virtual Pin 2
  Blynk.virtualWrite(V3, flowRate1);    // Caudal del sensor YF-S201 en Virtual Pin 3
  Blynk.virtualWrite(V4, flowRate2);    // Caudal del sensor YF-S401 en Virtual Pin 4
  Blynk.virtualWrite(V5, systemOn ? "ON" : "OFF");  // Estado de la electroválvula en Virtual Pin 5
}

// Controlar la electroválvula
void controlValve(bool state) {
  digitalWrite(relayPin, state ? HIGH : LOW);
  systemOn = state;
  if (state) {
    Serial.println("Electroválvula encendida");
  } else {
    Serial.println("Electroválvula apagada");
  }
}

// Control remoto de la válvula (máximo 15 minutos)
void handleRemoteValve() {
  if (remoteValveActive) {
    valveRemainingTime = 900000 - (millis() - valveRemoteStartTime);  // Tiempo restante en milisegundos

    if (millis() - valveRemoteStartTime >= 900000) {  // 15 minutos (900,000 milisegundos)
      controlValve(false);  // Apagar la válvula
      remoteValveActive = false;
      valveRemainingTime = 0;  // Restablecer el tiempo restante
      Serial.println("Electroválvula apagada automáticamente después de 15 minutos.");
      Blynk.virtualWrite(V6, 0);  // Asegurar que el valor en Blynk sea 0 al apagar
    }
  }
}

// Función para enviar el tiempo restante a Blynk
void updateValveTimerOnBlynk() {
  if (remoteValveActive) {
    // Enviar el tiempo restante a Blynk (convertido a segundos)
    Blynk.virtualWrite(V6, valveRemainingTime / 1000);
    Serial.print("Tiempo restante: ");
    Serial.println(valveRemainingTime / 1000);  // Mostrar en Serial el tiempo restante en segundos
  } else {
    Blynk.virtualWrite(V6, 0);  // Si no está activa, enviar 0
  }
}

// Control automático de la válvula (cada 3 horas por 5 minutos)
void handleAutoValve() {
  if (millis() - lastAutoToggleTime >= 10800000) {  // 3 horas
    controlValve(true);
    autoValveActive = true;
    autoMode = true;
    valveAutoStartTime = millis();
    lastAutoToggleTime = millis();
  }
  if (autoValveActive && millis() - valveAutoStartTime >= 300000) {  // 5 minutos
    controlValve(false);
    autoValveActive = false;
    autoMode = false;
  }
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);  // Aseguramos que la válvula esté apagada al inicio

  // Intentamos conectar a Wi-Fi
  connectWiFi();

  // Inicializamos ThingSpeak
  ThingSpeak.begin(client);

  // Configuramos los sensores de flujo
  attachInterrupt(digitalPinToInterrupt(sensor1Pin), pulseCounter1, FALLING);
  attachInterrupt(digitalPinToInterrupt(sensor2Pin), pulseCounter2, FALLING);

  // Configuramos Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, WiFi.SSID().c_str(), WiFi.psk().c_str());
  
  // Configuramos los temporizadores de Blynk
  timer.setInterval(1000L, sendDataToBlynk);  // Enviar datos cada segundo
  timer.setInterval(15000L, sendDataToThingSpeak);  // Enviar datos a ThingSpeak cada 15 segundos

  // Temporizador específico para actualizar el tiempo restante de la válvula en Blynk
  valveTimer.setInterval(1000L, updateValveTimerOnBlynk);  // Actualizar cada 1 segundo
}

void loop() {
  // Mantener la conexión con Blynk
  Blynk.run();
  timer.run();
  valveTimer.run();  // Aseguramos que el temporizador de la válvula se ejecute

  // Leer el sensor DHT
  readDHTSensor();

  // Calcular caudal en tiempo real
  flowRate1 = pulseCount1 / 7.5;  // Conversión para YF-S201
  flowRate2 = pulseCount2 ;  // Conversión para YF-S401, modificada

  // Control automático y remoto de la válvula
  handleAutoValve();
  handleRemoteValve();
}

// Control de la electroválvula mediante la aplicación Blynk
BLYNK_WRITE(V6) {
  int value = param.asInt();
  if (value == 1 && !remoteValveActive) {
    controlValve(true);  // Encender la válvula remotamente
    remoteValveActive = true;
    valveRemoteStartTime = millis();  // Iniciar el temporizador
    Serial.println("Electroválvula activada remotamente.");
  } else if (value == 0 && remoteValveActive) {
    controlValve(false);  // Apagar la válvula remotamente
    remoteValveActive = false;
    Serial.println("Electroválvula desactivada remotamente.");
  }
}
