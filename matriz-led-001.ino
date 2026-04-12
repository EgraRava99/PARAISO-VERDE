#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// Configuración de la matriz LED MAX7219, para el arduino paraiso verde 2026
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4

// Pines Arduino Nano (SPI por hardware)
#define CS_PIN 10

// Objeto de la matriz
MD_Parola display = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

void setup() {
  Serial.begin(9600);

  display.begin();
  display.setIntensity(3);   // brillo 0 a 15
  display.displayClear();

  display.displayText(
    "PARAISO VERDE",
    PA_CENTER, 
    60,     // velocidad
    1000,   // pausa
    PA_SCROLL_LEFT,
    PA_SCROLL_LEFT
  );
}

void loop() {
  if (display.displayAnimate()) {
    display.displayReset();
  }
}
