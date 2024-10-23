#define BLYNK_TEMPLATE_ID "TMPL2MhkdCsN0"
#define BLYNK_TEMPLATE_NAME "Mi primer ledCopy"
#define BLYNK_AUTH_TOKEN "xp-t-BAiCxqiVgRGD7_aWdBaRL97nMKr"


#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <ThingSpeak.h>
#include <DHT.h>

// Pines ajustados para el ESP32 de 30 pines
#define DHTPIN 4             // Pin para el sensor DHT11 (revisar si GPIO 4 está disponible)
#define DHTTYPE DHT11        // Tipo de sensor DHT11
DHT dht(DHTPIN, DHTTYPE);

const int soilSensorPin = 33;  // Cambié el pin al GPIO 33 (en lugar de 32)

// Variables de medición
float humidity = 0.0;
float temperature = 0.0;
int soilMoisture = 0;



const char* ssid[] = {"Elsacamialeja", "moto g54 5G_7085"};
const char* password[] = {"141024cafesita","12345612"};
WiFiClient client;
unsigned long myChannelNumber = 2645204;
const char * myWriteAPIKey = "0I78ZAWCIPZ92VJW";
BlynkTimer timer;

// Función para conectarse al WiFi
void connectWiFi() {
  int wifiAttempt = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempt < 6) {
    WiFi.begin(ssid[wifiAttempt], password[wifiAttempt]);
    Serial.print("Intentando conectar a ");
    Serial.println(ssid[wifiAttempt]);
    
    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
      delay(500);
      Serial.print(".");
      timeout++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("\nConectado a ");
      Serial.println(ssid[wifiAttempt]);

      // Potencia de transmisión ajustada
      WiFi.setTxPower(WIFI_POWER_13dBm);  // Usa la potencia por defecto de 13dBm
      
      return;
    }
    wifiAttempt++;
  }
  Serial.println("\nNo se pudo conectar a ninguna red.");
}

// Lectura del sensor DHT11
void readDHTSensor() {
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Error leyendo el sensor DHT11");
  } else {
    Serial.print("Humedad: ");
    Serial.print(humidity);
    Serial.print(" %\t");
    Serial.print("Temperatura: ");
    Serial.println(temperature);
  }
}

// Lectura del sensor de humedad del suelo (LM393)
void readSoilMoisture() {
  soilMoisture = analogRead(soilSensorPin);  // Lectura analógica
  Serial.print("Humedad del suelo (LM393): ");
  Serial.println(soilMoisture);
}

// Enviar datos a ThingSpeak
void sendDataToThingSpeak() {
  ThingSpeak.setField(3, temperature);   // Field 3: Temperatura
  ThingSpeak.setField(6, humidity);      // Field 6: Humedad
  ThingSpeak.setField(7, soilMoisture);  // Field 7: Humedad del suelo

  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  
  if (x == 200) {
    Serial.println("Datos enviados correctamente a ThingSpeak.");
  } else {
    Serial.println("Error al enviar los datos a ThingSpeak.");
  }
}

// Enviar datos a Blynk
void sendDataToBlynk() {
  Blynk.virtualWrite(V1, temperature);  // Enviar temperatura a Blynk en Virtual Pin 1
  Blynk.virtualWrite(V2, humidity);     // Enviar humedad a Blynk en Virtual Pin 2
  Blynk.virtualWrite(V3, soilMoisture); // Enviar humedad del suelo a Blynk en Virtual Pin 3
}

void setup() {
  Serial.begin(115200);
  dht.begin();  // Inicializamos el DHT11

  pinMode(soilSensorPin, INPUT);

  // Intentamos conectar a Wi-Fi
  connectWiFi();

  // Inicializamos ThingSpeak
  ThingSpeak.begin(client);

  // Configuramos Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, WiFi.SSID().c_str(), WiFi.psk().c_str());

  // Temporizadores para enviar datos a ThingSpeak y Blynk
  timer.setInterval(2000L, readDHTSensor);      // Leer el DHT11 cada 2 segundos
  timer.setInterval(2000L, readSoilMoisture);   // Leer la humedad del suelo cada 2 segundos
  timer.setInterval(15000L, sendDataToThingSpeak);  // Enviar datos a ThingSpeak cada 15 segundos
  timer.setInterval(1000L, sendDataToBlynk);    // Enviar datos a Blynk cada segundo
}

void loop() {
  Blynk.run();  // Ejecutar Blynk
  timer.run();  // Ejecutar el temporizador
}
