/*******************************************************
   PROYECTO: PARAÍSO VERDE
   NODO INTERNO - PIE DEL TANQUE

   ¿Qué hace este programa?
   - Mide temperatura y humedad del aire con un AM2302
   - Mide dos caudales de agua:
       1. Caudal del sistema manual
       2. Caudal del sistema de goteo
   - Controla una electroválvula con un relé
   - Envía datos a Blynk
   - Envía datos a ThingSpeak

   Pensado para ESP32
*******************************************************/


/*------------------------------------------------------
  1. DATOS DE BLYNK
  Estas líneas permiten conectar el ESP32 con Blynk.
------------------------------------------------------*/
#define BLYNK_TEMPLATE_ID "TMPL2BUGZ5Xkj"
#define BLYNK_TEMPLATE_NAME "Interno"
#define BLYNK_AUTH_TOKEN "DtvKpvMD0AaDeWZJym4QFEY9TpfAZ7Ew"


/*------------------------------------------------------
  2. LIBRERÍAS
  Son herramientas listas que nos ayudan a no hacer
  todo desde cero.
------------------------------------------------------*/
#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <ThingSpeak.h>
#include <DHT.h>


/*------------------------------------------------------
  3. PINES DEL ESP32
  Aquí le decimos al ESP32 en qué pin está conectado
  cada sensor o actuador.
------------------------------------------------------*/
const int sensorManualPin = 15;   // Sensor de flujo del sistema manual
const int sensorGoteoPin  = 14;   // Sensor de flujo del sistema de goteo
const int relayPin        = 5;    // Relé de la electroválvula

#define DHTPIN 4                  // Pin de datos del AM2302
#define DHTTYPE DHT22             // El AM2302 trabaja como DHT22


/*------------------------------------------------------
  4. CREAR EL OBJETO DEL SENSOR AM2302
  Esto permite usar el sensor de temperatura y humedad.
------------------------------------------------------*/
DHT dht(DHTPIN, DHTTYPE);


/*------------------------------------------------------
  5. VARIABLES DE LOS SENSORES DE FLUJO
  Los sensores de flujo cuentan pulsos.
  Más pulsos = más agua pasando.
------------------------------------------------------*/
volatile int pulseCountManual = 0;   // Pulsos del sistema manual
volatile int pulseCountGoteo  = 0;   // Pulsos del sistema de goteo

float flowRateManual = 0.0;          // Caudal calculado del sistema manual
float flowRateGoteo  = 0.0;          // Caudal calculado del sistema de goteo


/*------------------------------------------------------
  6. VARIABLES DE TEMPERATURA Y HUMEDAD
------------------------------------------------------*/
float humidity = 0.0;                // Humedad del aire
float temperature = 0.0;             // Temperatura del aire
bool am2302ReadSuccess = false;      // Indica si la lectura salió bien


/*------------------------------------------------------
  7. DATOS DE WIFI
  Aquí va el nombre de la red y su contraseña.
------------------------------------------------------*/
const char* ssid[] = {
  "ParaisoVerde2026"
};

const char* password[] = {
  "RAS.2026"
};

const int wifiNetworks = sizeof(ssid) / sizeof(ssid[0]);


/*------------------------------------------------------
  8. DATOS DE THINGSPEAK
------------------------------------------------------*/
WiFiClient client;
unsigned long myChannelNumber = 2645204;
const char * myWriteAPIKey = "0I78ZAWCIPZ92VJW";


/*------------------------------------------------------
  9. TEMPORIZADORES DE BLYNK
  Sirven para ejecutar funciones cada cierto tiempo.
------------------------------------------------------*/
BlynkTimer timer;
BlynkTimer valveTimer;


/*------------------------------------------------------
  10. VARIABLES DE CONTROL DEL SISTEMA
------------------------------------------------------*/
bool systemOn = true;                 // Estado general de la válvula

unsigned long valveRemoteStartTime = 0;
unsigned long valveAutoStartTime = 0;

bool remoteValveActive = false;
bool autoValveActive = false;
bool autoMode = false;

unsigned long lastAutoToggleTime = 0;
unsigned long valveRemainingTime = 0;


/*------------------------------------------------------
  11. FUNCIÓN PARA CONECTARSE AL WIFI
  El ESP32 intenta conectarse a la red disponible.
------------------------------------------------------*/
void connectWiFi() {
  int wifiAttempt = 0;

  while (WiFi.status() != WL_CONNECTED && wifiAttempt < wifiNetworks) {
    WiFi.begin(ssid[wifiAttempt], password[wifiAttempt]);

    Serial.print("Intentando conectar a: ");
    Serial.println(ssid[wifiAttempt]);

    int timeout = 0;
    while (WiFi.status() != WL_CONNECTED && timeout < 20) {
      delay(500);
      Serial.print(".");
      timeout++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("\nConectado a: ");
      Serial.println(ssid[wifiAttempt]);

      WiFi.setTxPower(WIFI_POWER_19_5dBm); // Potencia alta de transmisión
      return;
    }

    Serial.println("\nNo conectó esa red. Probando otra...");
    wifiAttempt++;
  }

  Serial.println("\nNo se pudo conectar a ninguna red WiFi.");
}


/*------------------------------------------------------
  12. INTERRUPCIONES
  Cada vez que pasa agua, el sensor genera pulsos.
  Estas funciones cuentan esos pulsos.

  IRAM_ATTR se usa en ESP32 para interrupciones.
------------------------------------------------------*/
void IRAM_ATTR pulseCounterManual() {
  pulseCountManual++;
}

void IRAM_ATTR pulseCounterGoteo() {
  pulseCountGoteo++;
}


/*------------------------------------------------------
  13. LEER EL SENSOR AM2302
  Este sensor nos da:
  - temperatura del aire
  - humedad del aire

  Ojo:
  No mide humedad del suelo.
------------------------------------------------------*/
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
    Serial.print(" %");

    Serial.print(" | Temperatura: ");
    Serial.print(temperature);
    Serial.println(" °C");
  }
}


/*------------------------------------------------------
  14. CALCULAR LOS CAUDALES
  Aquí transformamos pulsos en una medida de caudal.

  IMPORTANTE:
  La fórmula depende del tipo de sensor.
  Estas fórmulas son aproximadas y deben calibrarse.
------------------------------------------------------*/
void calculateFlowRates() {
  // Sensor del sistema manual (ejemplo YF-S201)
  flowRateManual = pulseCountManual / 7.5;

  // Sensor del sistema de goteo (ejemplo YF-S401)
  flowRateGoteo = pulseCountGoteo / (5880.0 / 60.0);
}


/*------------------------------------------------------
  15. ENVIAR DATOS A THINGSPEAK
  Campos propuestos:
  Field 1 = caudal manual
  Field 2 = caudal goteo
  Field 3 = estado del sistema
  Field 4 = humedad del aire
  Field 5 = temperatura
------------------------------------------------------*/
void sendDataToThingSpeak() {
  calculateFlowRates();

  ThingSpeak.setField(1, flowRateManual);
  ThingSpeak.setField(2, flowRateGoteo);
  ThingSpeak.setField(3, systemOn ? 1 : 0);
  ThingSpeak.setField(4, humidity);
  ThingSpeak.setField(5, temperature);

  int result = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

  if (result == 200) {
    Serial.println("Datos enviados correctamente a ThingSpeak.");
  } else {
    Serial.print("Error al enviar datos a ThingSpeak. Código: ");
    Serial.println(result);
  }

  // Reiniciamos los contadores después del envío
  pulseCountManual = 0;
  pulseCountGoteo = 0;
}


/*------------------------------------------------------
  16. ENVIAR DATOS A BLYNK
  Puedes cambiar los pines virtuales si lo deseas.
------------------------------------------------------*/
void sendDataToBlynk() {
  Blynk.virtualWrite(V1, temperature);               // Temperatura
  Blynk.virtualWrite(V2, humidity);                  // Humedad del aire
  Blynk.virtualWrite(V3, flowRateManual);            // Caudal manual
  Blynk.virtualWrite(V4, flowRateGoteo);             // Caudal goteo
  Blynk.virtualWrite(V5, systemOn ? "ON" : "OFF");   // Estado de válvula
}


/*------------------------------------------------------
  17. CONTROLAR LA ELECTROVÁLVULA
  Encender = abrir paso del agua
  Apagar   = cerrar paso del agua
------------------------------------------------------*/
void controlValve(bool state) {
  digitalWrite(relayPin, state ? HIGH : LOW);
  systemOn = state;

  if (state) {
    Serial.println("Electroválvula ENCENDIDA");
  } else {
    Serial.println("Electroválvula APAGADA");
  }
}


/*------------------------------------------------------
  18. CONTROL REMOTO DE LA VÁLVULA
  Si se enciende desde Blynk, puede durar máximo
  15 minutos.
------------------------------------------------------*/
void handleRemoteValve() {
  if (remoteValveActive) {
    valveRemainingTime = 900000 - (millis() - valveRemoteStartTime);

    if (millis() - valveRemoteStartTime >= 900000) {
      controlValve(false);
      remoteValveActive = false;
      valveRemainingTime = 0;

      Serial.println("La válvula se apagó sola después de 15 minutos.");
      Blynk.virtualWrite(V6, 0);
    }
  }
}


/*------------------------------------------------------
  19. MOSTRAR EL TIEMPO RESTANTE EN BLYNK
------------------------------------------------------*/
void updateValveTimerOnBlynk() {
  if (remoteValveActive) {
    Blynk.virtualWrite(V6, valveRemainingTime / 1000);

    Serial.print("Tiempo restante de válvula: ");
    Serial.print(valveRemainingTime / 1000);
    Serial.println(" segundos");
  } else {
    Blynk.virtualWrite(V6, 0);
  }
}


/*------------------------------------------------------
  20. MODO AUTOMÁTICO
  Cada 3 horas abre la válvula 5 minutos.
  Esto es una prueba; luego puedes ajustar tiempos.
------------------------------------------------------*/
void handleAutoValve() {
  if (millis() - lastAutoToggleTime >= 10800000) {   // 3 horas
    controlValve(true);
    autoValveActive = true;
    autoMode = true;
    valveAutoStartTime = millis();
    lastAutoToggleTime = millis();
  }

  if (autoValveActive && millis() - valveAutoStartTime >= 300000) { // 5 min
    controlValve(false);
    autoValveActive = false;
    autoMode = false;
  }
}


/*------------------------------------------------------
  21. SETUP
  Esta parte corre una sola vez al inicio.
------------------------------------------------------*/
void setup() {
  Serial.begin(115200);

  // Configurar pines
  pinMode(sensorManualPin, INPUT_PULLUP);
  pinMode(sensorGoteoPin, INPUT_PULLUP);
  pinMode(relayPin, OUTPUT);

  // Aseguramos que la válvula inicie apagada
  digitalWrite(relayPin, LOW);

  // Iniciar sensor AM2302
  dht.begin();

  // Conectar WiFi
  connectWiFi();

  // Iniciar ThingSpeak
  ThingSpeak.begin(client);

  // Activar interrupciones para contar pulsos de flujo
  attachInterrupt(digitalPinToInterrupt(sensorManualPin), pulseCounterManual, FALLING);
  attachInterrupt(digitalPinToInterrupt(sensorGoteoPin), pulseCounterGoteo, FALLING);

  // Iniciar Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, WiFi.SSID().c_str(), WiFi.psk().c_str());

  // Temporizadores
  timer.setInterval(2000L, sendDataToBlynk);          // cada 2 segundos
  timer.setInterval(15000L, sendDataToThingSpeak);    // cada 15 segundos
  valveTimer.setInterval(1000L, updateValveTimerOnBlynk); // cada 1 segundo

  Serial.println("Sistema iniciado correctamente.");
}


/*------------------------------------------------------
  22. LOOP
  Esta parte corre una y otra vez sin detenerse.
  Es como el corazón del programa.
------------------------------------------------------*/
void loop() {
  Blynk.run();
  timer.run();
  valveTimer.run();

  // Leer el sensor ambiental
  readAM2302Sensor();

  // Calcular caudales en tiempo real
  calculateFlowRates();

  // Revisar lógica de control
  handleAutoValve();
  handleRemoteValve();

  // Esperar un poco para no leer demasiado rápido el AM2302
  delay(2000);
}


/*------------------------------------------------------
  23. CONTROL DESDE BLYNK
  Con V6 se puede prender o apagar la válvula.
------------------------------------------------------*/
BLYNK_WRITE(V6) {
  int value = param.asInt();

  if (value == 1 && !remoteValveActive) {
    controlValve(true);
    remoteValveActive = true;
    valveRemoteStartTime = millis();

    Serial.println("Electroválvula activada desde Blynk.");
  }
  else if (value == 0 && remoteValveActive) {
    controlValve(false);
    remoteValveActive = false;

    Serial.println("Electroválvula desactivada desde Blynk.");
  }
}
