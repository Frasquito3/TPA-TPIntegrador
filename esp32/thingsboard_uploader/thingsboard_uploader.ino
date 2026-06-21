/*
 * Gateway ESP32 entre el Arduino UNO y ThingsBoard.
 * Recibe telemetría por UART, publica mediante MQTT
 * y reenvía las solicitudes RPC al Arduino.
 *
 * Las credenciales se almacenan en config.h.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

// UART Arduino–ESP32

constexpr uint8_t ESP32_RX2 = 16;
constexpr uint8_t ESP32_TX2 = 17;
constexpr uint32_t ARDUINO_BAUD = 115200;

HardwareSerial ArduinoPort(2);

// Topics MQTT

constexpr char TB_TELEMETRY_TOPIC[] =
    "v1/devices/me/telemetry";

constexpr char TB_RPC_REQUEST_TOPIC[] =
    "v1/devices/me/rpc/request/+";

constexpr char TB_RPC_RESPONSE_PREFIX[] =
    "v1/devices/me/rpc/response/";

// Conexión MQTT

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

constexpr uint32_t WIFI_RETRY_MS = 10000;
constexpr uint32_t MQTT_RETRY_MS = 5000;

// La telemetría se publica en ThingsBoard cada cuatro segundos.
constexpr uint32_t CLOUD_TELEMETRY_PERIOD_MS = 4000;

uint32_t lastCloudTelemetryMs = 0;
uint32_t lastWifiAttemptMs = 0;
uint32_t lastMqttAttemptMs = 0;

// Estado del gateway

char serialLine[128];
size_t serialLineLength = 0;

uint32_t lastArduinoTelemetryMs = 0;
bool lastReportedArduinoOnline = false;
bool lastSentCloudState = false;

// Funciones auxiliares

const char* obtenerTopicTelemetria() {
  return USE_UTN_CUSTOM_TELEMETRY_TOPIC
      ? UTN_TELEMETRY_TOPIC
      : TB_TELEMETRY_TOPIC;
}

void enviarComandoArduino(const String& comando) {
  ArduinoPort.print(comando);
  ArduinoPort.print('\n');
}

// Informa al Arduino si el gateway está conectado a ThingsBoard.
void actualizarIndicadorThingsBoardArduino() {
  const bool conectado =
      WiFi.status() == WL_CONNECTED &&
      mqtt.connected();

  if (conectado != lastSentCloudState) {
    enviarComandoArduino(
        conectado ? "SET_NET:1" : "SET_NET:0"
    );

    lastSentCloudState = conectado;
  }
}

void publicarEstadoGateway(bool arduinoOnline) {
  if (!mqtt.connected()) {
    return;
  }

  StaticJsonDocument<192> doc;

  doc["online"] = true;
  doc["arduinoOnline"] = arduinoOnline;
  doc["wifiRssi"] = WiFi.RSSI();

  char payload[192];

  const size_t length =
      serializeJson(doc, payload, sizeof(payload));

  mqtt.publish(
      obtenerTopicTelemetria(),
      reinterpret_cast<const uint8_t*>(payload),
      length,
      false
  );
}

// Telemetría recibida desde el Arduino

void procesarTelemetriaArduino(const char* linea) {
  // Formato: T,y100,r,e100,u,kp10000,ki10000,kd10000,modo,saturado

  if (strncmp(linea, "T,", 2) != 0) {
    Serial.print(F("[Arduino] "));
    Serial.println(linea);
    return;
  }

  long y100 = 0;
  int setpoint = 0;
  long error100 = 0;
  int pwm = 0;
  long kp10000 = 0;
  long ki10000 = 0;
  long kd10000 = 0;
  char modo = 'A';
  int saturado = 0;

  const int campos = sscanf(
      linea,
      "T,%ld,%d,%ld,%d,%ld,%ld,%ld,%c,%d",
      &y100,
      &setpoint,
      &error100,
      &pwm,
      &kp10000,
      &ki10000,
      &kd10000,
      &modo,
      &saturado
  );

  if (campos != 9) {
    Serial.print(F("Telemetria invalida: "));
    Serial.println(linea);
    return;
  }

  lastArduinoTelemetryMs = millis();

  if (!mqtt.connected()) {
    return;
  }

  // Limita la frecuencia de publicación sin descartar la recepción UART.
  const uint32_t ahora = millis();

  if (
      lastCloudTelemetryMs != 0 &&
      static_cast<uint32_t>(
          ahora - lastCloudTelemetryMs
      ) < CLOUD_TELEMETRY_PERIOD_MS
  ) {
    return;
  }

  lastCloudTelemetryMs = ahora;

  StaticJsonDocument<384> doc;

  doc["y"] = y100 / 100.0;
  doc["r"] = setpoint;
  doc["e"] = error100 / 100.0;
  doc["u"] = pwm;

  doc["kp"] = kp10000 / 10000.0;
  doc["ki"] = ki10000 / 10000.0;
  doc["kd"] = kd10000 / 10000.0;

  doc["mode"] = modo == 'A' ? "AUTO" : "MANUAL";
  doc["saturated"] = saturado != 0;

  doc["online"] = true;
  doc["arduinoOnline"] = true;
  doc["wifiRssi"] = WiFi.RSSI();

  char payload[384];

  const size_t length =
      serializeJson(doc, payload, sizeof(payload));

  const bool publicado = mqtt.publish(
      obtenerTopicTelemetria(),
      reinterpret_cast<const uint8_t*>(payload),
      length,
      false
  );

  if (!publicado) {
    Serial.println(F("No se pudo publicar la telemetria."));
  }
}

void leerPuertoArduino() {
  while (ArduinoPort.available() > 0) {
    const char recibido =
        static_cast<char>(ArduinoPort.read());

    if (recibido == '\n' || recibido == '\r') {
      if (serialLineLength > 0) {
        serialLine[serialLineLength] = '\0';
        procesarTelemetriaArduino(serialLine);
        serialLineLength = 0;
      }

    } else if (
        serialLineLength <
        sizeof(serialLine) - 1
    ) {
      serialLine[serialLineLength++] = recibido;

    } else {
      // Descarta una línea demasiado larga.
      serialLineLength = 0;
    }
  }
}

// Parámetros RPC

bool extraerParametroNumerico(
    JsonVariantConst params,
    float& valor
) {
  if (
      params.is<float>() ||
      params.is<double>() ||
      params.is<int>() ||
      params.is<long>()
  ) {
    valor = params.as<float>();
    return true;
  }

  if (params.is<JsonObjectConst>()) {
    JsonObjectConst objeto =
        params.as<JsonObjectConst>();

    if (objeto.containsKey("value")) {
      valor = objeto["value"].as<float>();
      return true;
    }
  }

  return false;
}

bool extraerParametroTexto(
    JsonVariantConst params,
    String& valor
) {
  if (params.is<const char*>()) {
    valor = params.as<const char*>();
    return true;
  }

  if (params.is<JsonObjectConst>()) {
    JsonObjectConst objeto =
        params.as<JsonObjectConst>();

    if (objeto.containsKey("value")) {
      valor = objeto["value"].as<const char*>();
      return true;
    }
  }

  return false;
}

// Confirma que el gateway recibió y procesó la solicitud.
void responderRpc(
    const char* requestId,
    bool ok,
    const char* mensaje
) {
  if (!mqtt.connected()) {
    return;
  }

  StaticJsonDocument<192> respuesta;

  respuesta["ok"] = ok;
  respuesta["message"] = mensaje;

  char payload[192];
  serializeJson(respuesta, payload, sizeof(payload));

  char topicRespuesta[96];

  snprintf(
      topicRespuesta,
      sizeof(topicRespuesta),
      "%s%s",
      TB_RPC_RESPONSE_PREFIX,
      requestId
  );

  mqtt.publish(topicRespuesta, payload);
}

// Recepción de RPC

void callbackMqtt(
    char* topic,
    uint8_t* payload,
    unsigned int length
) {
  char requestPayload[256];

  if (length >= sizeof(requestPayload)) {
    return;
  }

  memcpy(requestPayload, payload, length);
  requestPayload[length] = '\0';

  const char* requestId = strrchr(topic, '/');

  if (
      requestId == nullptr ||
      *(requestId + 1) == '\0'
  ) {
    return;
  }

  ++requestId;

  StaticJsonDocument<256> request;

  const DeserializationError error =
      deserializeJson(request, requestPayload);

  if (error) {
    responderRpc(
        requestId,
        false,
        "JSON invalido"
    );
    return;
  }

  const char* metodo =
      request["method"] | "";

  JsonVariantConst params =
      request["params"];

  float valorNumerico = 0.0f;
  String valorTexto;
  String comando;

  bool aceptado = true;

  if (
      strcmp(metodo, "setSetpoint") == 0 &&
      extraerParametroNumerico(
          params,
          valorNumerico
      )
  ) {
    const int nuevoSetpoint = constrain(
        static_cast<int>(lround(valorNumerico)),
        0,
        1023
    );

    comando =
        "SET_R:" +
        String(nuevoSetpoint);

  } else if (
      strcmp(metodo, "setKp") == 0 &&
      extraerParametroNumerico(
          params,
          valorNumerico
      )
  ) {
    comando =
        "SET_KP:" +
        String(valorNumerico, 6);

  } else if (
      strcmp(metodo, "setKi") == 0 &&
      extraerParametroNumerico(
          params,
          valorNumerico
      )
  ) {
    comando =
        "SET_KI:" +
        String(valorNumerico, 6);

  } else if (
      strcmp(metodo, "setKd") == 0 &&
      extraerParametroNumerico(
          params,
          valorNumerico
      )
  ) {
    comando =
        "SET_KD:" +
        String(valorNumerico, 6);

  } else if (
      strcmp(metodo, "setManualPwm") == 0 &&
      extraerParametroNumerico(
          params,
          valorNumerico
      )
  ) {
    const int nuevoPwm = constrain(
        static_cast<int>(lround(valorNumerico)),
        0,
        255
    );

    comando =
        "SET_U:" +
        String(nuevoPwm);

  } else if (
      strcmp(metodo, "setMode") == 0 &&
      extraerParametroTexto(
          params,
          valorTexto
      )
  ) {
    valorTexto.toUpperCase();

    if (
        valorTexto == "AUTO" ||
        valorTexto == "MANUAL"
    ) {
      comando =
          "SET_MODE:" +
          valorTexto;
    } else {
      aceptado = false;
    }

  } else if (
      strcmp(metodo, "loadP") == 0
  ) {
    comando = "LOAD_P";

  } else if (
      strcmp(metodo, "loadPI") == 0
  ) {
    comando = "LOAD_PI";

  } else if (
      strcmp(metodo, "loadPID") == 0
  ) {
    comando = "LOAD_PID";

  } else if (
      strcmp(metodo, "ping") == 0
  ) {
    responderRpc(
        requestId,
        true,
        "pong"
    );
    return;

  } else {
    aceptado = false;
  }

  if (!aceptado) {
    responderRpc(
        requestId,
        false,
        "Metodo o parametro no valido"
    );
    return;
  }

  enviarComandoArduino(comando);

  Serial.print(F("RPC reenviado al Arduino: "));
  Serial.println(comando);

  responderRpc(
      requestId,
      true,
      "Comando reenviado"
  );
}

// Conexión WiFi

void mantenerWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  const uint32_t ahora = millis();

  if (
      static_cast<uint32_t>(
          ahora - lastWifiAttemptMs
      ) < WIFI_RETRY_MS
  ) {
    return;
  }

  lastWifiAttemptMs = ahora;

  Serial.print(F("Conectando a WiFi: "));
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(
      WIFI_SSID,
      WIFI_PASSWORD
  );
}

// Conexión MQTT

void mantenerMqtt() {
  if (
      WiFi.status() != WL_CONNECTED ||
      mqtt.connected()
  ) {
    return;
  }

  const uint32_t ahora = millis();

  if (
      static_cast<uint32_t>(
          ahora - lastMqttAttemptMs
      ) < MQTT_RETRY_MS
  ) {
    return;
  }

  lastMqttAttemptMs = ahora;

  String clientId = "tpi-esp32-";

  const uint64_t chipId =
      ESP.getEfuseMac();

  clientId += String(
      static_cast<uint32_t>(
          chipId & 0xFFFFFFFF
      ),
      HEX
  );

  Serial.print(
      F("Conectando a ThingsBoard... ")
  );

  if (
      mqtt.connect(
          clientId.c_str(),
          TB_TOKEN,
          ""
      )
  ) {
    Serial.println(F("OK"));

    mqtt.subscribe(
        TB_RPC_REQUEST_TOPIC
    );

    const bool arduinoOnline =
        static_cast<uint32_t>(
            millis() -
            lastArduinoTelemetryMs
        ) < 2000UL;

    publicarEstadoGateway(
        arduinoOnline
    );

  } else {
    Serial.print(
        F("fallo, estado MQTT = ")
    );
    Serial.println(
        mqtt.state()
    );
  }
}

// Setup

void setup() {
  Serial.begin(115200);

  ArduinoPort.begin(
      ARDUINO_BAUD,
      SERIAL_8N1,
      ESP32_RX2,
      ESP32_TX2
  );

  mqtt.setServer(
      TB_SERVER,
      TB_PORT
  );

  mqtt.setCallback(
      callbackMqtt
  );

  mqtt.setBufferSize(512);
  mqtt.setKeepAlive(30);

  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  lastWifiAttemptMs =
      millis() - WIFI_RETRY_MS;

  lastMqttAttemptMs =
      millis() - MQTT_RETRY_MS;

  Serial.println(
      F("Gateway ESP32 iniciado.")
  );

  Serial.println(
      F("La configuracion se lee desde config.h.")
  );
}

// Loop

void loop() {
  leerPuertoArduino();

  mantenerWiFi();
  mantenerMqtt();

  if (mqtt.connected()) {
    mqtt.loop();
  }

  actualizarIndicadorThingsBoardArduino();

  const bool arduinoOnline =
      static_cast<uint32_t>(
          millis() -
          lastArduinoTelemetryMs
      ) < 2000UL;

  if (
      arduinoOnline !=
      lastReportedArduinoOnline
  ) {
    lastReportedArduinoOnline =
        arduinoOnline;

    publicarEstadoGateway(
        arduinoOnline
    );
  }
}
