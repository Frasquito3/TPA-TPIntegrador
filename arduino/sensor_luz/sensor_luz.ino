/*
 * ============================================================
 *  ARCHIVO DE PRUEBA — NO ES EL CÓDIGO FINAL DEL PROYECTO
 * ============================================================
 * Envía un JSON estático por UART al ESP32 cada 5 segundos.
 *
 * El código definitivo del proyecto está en:
 *   arduino/sensor_luz/sensor_luz.ino
 * ============================================================
 *
 * IMPORTANTE: desconectar el cable TX (pin 1) antes de subir
 * el código, y volver a conectarlo una vez terminado el upload.
 */

#define INTERVALO_MS 5000

unsigned long ultima = 0;

void setup() {
  Serial.begin(9600);   // misma velocidad que el ESP32
}

void loop() {
  if (millis() - ultima >= INTERVALO_MS) {
    ultima = millis();

    String json = "{\"luminosidad\": 42}";
    Serial.println(json);
  }
}