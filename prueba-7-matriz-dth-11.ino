#include <MD_Parola.h>
#include <MD_MAX72XX.h>
#include <SPI.h>

// Definición del hardware de la matriz LED
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW  // Tipo de hardware, FC16_HW es común para MAX7219
#define MAX_DEVICES 4  // Número de módulos 8x8 en cascada
#define DATA_PIN 23    // Pin DIN (Entrada de datos)
#define CLK_PIN 18     // Pin CLK (Reloj)
#define CS_PIN 5       // Pin CS/LOAD (Chip Select)

// Creación del objeto Parola para controlar la matriz LED
MD_Parola display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// Frase a mostrar en la matriz LED
char message[] = "Paraiso Verde usando el agua de forma inteligente";

void setup() {
  Serial.begin(115200);

  // Inicializa la matriz LED
  display.begin();
  display.setIntensity(5);  // Ajusta el brillo (0 a 15)
  display.displayClear();   // Limpia la pantalla de inicio

  // Configura la velocidad y tipo de animación
  display.displayText(message, PA_CENTER, 100, 30, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
}

void loop() {
  // Llama al método displayAnimate para controlar la animación del texto
  if (display.displayAnimate()) {
    display.displayReset();  // Resetea para que el texto siga repitiéndose
  }
}
