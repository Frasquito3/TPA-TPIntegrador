/*
 * ============================================================
 *  ARCHIVO DE PRUEBA — NO ES EL CÓDIGO FINAL DEL PROYECTO
 * ============================================================
 * Recibe un JSON del Arduino por UART y sube el valor
 * de luminosidad a ThingsBoard.
 *
 * El código definitivo del proyecto está en:
 *   esp32/thingsboard_uploader/thingsboard_uploader.ino
 * ============================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

#define RX_PIN 16
#define TX_PIN 17

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

void conectarWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi conectando");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println(" OK — IP: " + WiFi.localIP().toString());
}

void conectarMQTT() {
  while (!mqtt.connected()) {
    Serial.print("MQTT conectando...");
    String clientId = "ESP32-" + String(random(0xffff), HEX);
    if (mqtt.connect(clientId.c_str(), TB_TOKEN, "")) {
      Serial.println(" OK");
    } else {
      Serial.println(" FALLO rc=" + String(mqtt.state()));
      delay(3000);
    }
  }
}

void setup() {
  Serial.begin(115200);          // Serial monitor
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);  // comunicación con Arduino

  conectarWiFi();
  mqtt.setServer(TB_SERVER, TB_PORT);
  conectarMQTT();

  Serial.println("Esperando datos del Arduino...");
}

void loop() {
  if (!mqtt.connected()) conectarMQTT();
  mqtt.loop();

  // Leer línea completa del Arduino cuando esté disponible
  if (Serial2.available()) {
    String linea = Serial2.readStringUntil('\n');
    linea.trim();

    Serial.println("Recibido: " + linea);

    // Parsear el JSON
    StaticJsonDocument<64> doc;
    DeserializationError error = deserializeJson(doc, linea);

    if (error) {
      Serial.println("Error al parsear JSON: " + String(error.c_str()));
      return;
    }

    int luminosidad = doc["luminosidad"];

    // Publicar en ThingsBoard
    String payload = "{\"luminosidad\": " + String(luminosidad) + "}";
    bool ok = mqtt.publish("v1/devices/me/telemetry", payload.c_str(), true);
    Serial.println(ok ? "Enviado OK: " + payload : "ERROR al enviar");
  }
}