#include <WiFi.h>
#include <PubSubClient.h>
#include "config.h" 

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

unsigned long ultima = 0;

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
  Serial.begin(115200);
  conectarWiFi();
  mqtt.setServer(TB_SERVER, TB_PORT);
  conectarMQTT();
}

void loop() {
  if (!mqtt.connected()) conectarMQTT();
  mqtt.loop();

  if (millis() - ultima >= 5000) {
    ultima = millis();

    String json = "{\"luminosidad\": 42}";
    bool ok = mqtt.publish("v1/devices/me/telemetry", json.c_str(), true);
    Serial.println(ok ? "Enviado OK: " + json : "ERROR al enviar");
  }
}