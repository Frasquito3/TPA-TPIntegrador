/*
 * Ensayo automático de control P, PI o PID.
 * Los parámetros de la corrida se configuran al comienzo del archivo.
 *
 * La salida se imprime como una matriz compatible con MATLAB:
 * tiempo, medición, medición filtrada, referencia, PWM, error,
 * términos P, I y D, derivada filtrada, salida calculada y saturación.
 */

#include <Arduino.h>

constexpr uint8_t PIN_LED_1 = 9;
constexpr uint8_t PIN_LED_2 = 10;
constexpr uint8_t PIN_LDR = A0;

enum TipoControl : uint8_t {
  CONTROL_P = 0,
  CONTROL_PI = 1,
  CONTROL_PID = 2
};

// Configuración del ensayo

// Controlador utilizado en la corrida
constexpr TipoControl CONTROLADOR = CONTROL_PID;

// Ganancias utilizadas en la corrida.
constexpr float KP = 2.592988f;
constexpr float KI = 11.039142f;
constexpr float KD = 0.029032f;

// Factor común aplicado a las tres ganancias.
constexpr float FACTOR_AJUSTE = 1.0f;

// Referencias de la prueba.
constexpr int SETPOINT_INICIAL = 177;
constexpr int SETPOINT_FINAL = 187;

// PWM necesario para sostener aproximadamente 177 ADC.
constexpr int PWM_BIAS = 123;

// Temporizacion del ensayo.
constexpr uint16_t TS_MS = 10;
constexpr float TS_S = TS_MS / 1000.0f;
constexpr uint16_t TIEMPO_PRE_MS = 3000;
constexpr uint16_t TIEMPO_POST_MS = 12000;

// Un valor menor de ALPHA_Y aumenta el filtrado y el retardo.
constexpr float ALPHA_Y = 0.25f;

constexpr float ALPHA_D = 0.20f;

constexpr int PWM_MIN = 0;
constexpr int PWM_MAX = 255;
constexpr uint8_t N_LECTURAS_ADC = 20;

float integral = 0.0f;
float yFiltrada = 0.0f;
float yAnterior = 0.0f;
float derivadaFiltrada = 0.0f;

bool filtroInicializado = false;
bool escalonAplicado = false;
bool ensayoFinalizado = false;

unsigned long inicioMs = 0;
unsigned long ultimoControlMs = 0;

int leerLDRPromedio() {
  long suma = 0;

  for (uint8_t i = 0; i < N_LECTURAS_ADC; ++i) {
    suma += analogRead(PIN_LDR);
  }

  return static_cast<int>(suma / N_LECTURAS_ADC);
}

void aplicarPWM(int pwm) {
  const int valor = constrain(pwm, PWM_MIN, PWM_MAX);
  analogWrite(PIN_LED_1, valor);
  analogWrite(PIN_LED_2, valor);
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_LED_1, OUTPUT);
  pinMode(PIN_LED_2, OUTPUT);

  aplicarPWM(PWM_BIAS);
  delay(1000);

  const int y0 = leerLDRPromedio();
  yFiltrada = static_cast<float>(y0);
  yAnterior = yFiltrada;
  filtroInicializado = true;

  inicioMs = millis();
  ultimoControlMs = inicioMs;

  Serial.println(F("datos = ["));
}

void loop() {
  if (ensayoFinalizado) {
    return;
  }

  const unsigned long ahora = millis();

  if ((unsigned long)(ahora - ultimoControlMs) < TS_MS) {
    return;
  }

  ultimoControlMs += TS_MS;

  // Evita ejecutar varios ciclos juntos si el programa quedó atrasado.
  if ((unsigned long)(ahora - ultimoControlMs) > 5UL * TS_MS) {
    ultimoControlMs = ahora;
  }

  const unsigned long t = ahora - inicioMs;

  int setpoint = SETPOINT_INICIAL;

  if (t >= TIEMPO_PRE_MS) {
    setpoint = SETPOINT_FINAL;
    escalonAplicado = true;
  }

  if (t >= (unsigned long)TIEMPO_PRE_MS + TIEMPO_POST_MS) {
    aplicarPWM(0);
    Serial.println(F("];"));
    ensayoFinalizado = true;
    return;
  }

  const int yRaw = leerLDRPromedio();

  if (!filtroInicializado) {
    yFiltrada = static_cast<float>(yRaw);
    yAnterior = yFiltrada;
    filtroInicializado = true;
  } else {
    yFiltrada += ALPHA_Y * (static_cast<float>(yRaw) - yFiltrada);
  }

  const float error = static_cast<float>(setpoint) - yFiltrada;

  const float kp = KP * FACTOR_AJUSTE;
  const float ki = KI * FACTOR_AJUSTE;
  const float kd = KD * FACTOR_AJUSTE;

  const float terminoP = kp * error;

  float integralCandidata = integral;
  float terminoI = 0.0f;

  if (CONTROLADOR == CONTROL_PI || CONTROLADOR == CONTROL_PID) {
    integralCandidata += error * TS_S;
    terminoI = ki * integralCandidata;
  }

  // Derivada negativa de la medición para evitar derivative kick.
  const float derivadaMedicion =
      -(yFiltrada - yAnterior) / TS_S;

  derivadaFiltrada +=
      ALPHA_D * (derivadaMedicion - derivadaFiltrada);

  float terminoD = 0.0f;

  if (CONTROLADOR == CONTROL_PID) {
    terminoD = kd * derivadaFiltrada;
  }

  float salidaNoSaturada =
      static_cast<float>(PWM_BIAS) +
      terminoP +
      terminoI +
      terminoD;

  const bool saturaArriba = salidaNoSaturada > PWM_MAX;
  const bool saturaAbajo = salidaNoSaturada < PWM_MIN;

  const bool errorEmpujaSaturacion =
      (saturaArriba && error > 0.0f) ||
      (saturaAbajo && error < 0.0f);

  // Anti-windup por integración condicional.
  if ((CONTROLADOR == CONTROL_PI || CONTROLADOR == CONTROL_PID) &&
      !errorEmpujaSaturacion) {
    integral = integralCandidata;
  } else {
    terminoI = ki * integral;

    salidaNoSaturada =
        static_cast<float>(PWM_BIAS) +
        terminoP +
        terminoI +
        terminoD;
  }

  const int pwm = constrain(
      static_cast<int>(lround(salidaNoSaturada)),
      PWM_MIN,
      PWM_MAX);

  const bool saturadoFinal = salidaNoSaturada > PWM_MAX || salidaNoSaturada < PWM_MIN;

  aplicarPWM(pwm);
  yAnterior = yFiltrada;
 
  Serial.print(t);
  Serial.print(',');
  Serial.print(yRaw);
  Serial.print(',');
  Serial.print(yFiltrada, 3);
  Serial.print(',');
  Serial.print(setpoint);
  Serial.print(',');
  Serial.print(pwm);
  Serial.print(',');
  Serial.print(error, 3);
  Serial.print(',');
  Serial.print(terminoP, 3);
  Serial.print(',');
  Serial.print(terminoI, 3);
  Serial.print(',');
  Serial.print(terminoD, 3);
  Serial.print(',');
  Serial.print(derivadaFiltrada, 3);
  Serial.print(',');
  Serial.print(salidaNoSaturada, 3);
  Serial.print(',');
  Serial.print(saturadoFinal ? 1 : 0);
  Serial.println(';');
}
