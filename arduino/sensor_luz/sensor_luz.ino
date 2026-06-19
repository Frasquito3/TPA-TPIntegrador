/*
 * TPI 2026 - CONTROL FINAL EN ARDUINO UNO
 *
 * Control PI por defecto, con posibilidad de modificar:
 * - setpoint
 * - Kp, Ki, Kd
 * - modo AUTO / MANUAL
 * - PWM manual
 *
 * También permite cargar los presets finales P y PI.
 *
 * Hardware:
 *   LED 1: pin 9
 *   LED 2: pin 10
 *   LDR: A0
 *   LCD 16x2 I2C:
 *     SDA: A4
 *     SCL: A5
 *   Comunicación con ESP32:
 *     Arduino RX0 (pin 0) <- ESP32 TX2 (GPIO 17)
 *     Arduino TX1 (pin 1) -> divisor de tensión -> ESP32 RX2 (GPIO 16)
 *     GND Arduino y GND ESP32 unidos
 *
 * IMPORTANTE:
 * Desconectar los cables de los pines 0 y 1 mientras se carga el programa
 * al Arduino. Reconectarlos después de finalizar la carga.
 */

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ============================================================
// PINES Y HARDWARE
// ============================================================

constexpr uint8_t PIN_LED_1 = 9;
constexpr uint8_t PIN_LED_2 = 10;
constexpr uint8_t PIN_LDR   = A0;

constexpr uint8_t LCD_ADDRESS = 0x27;
constexpr uint8_t LCD_COLUMNS = 16;
constexpr uint8_t LCD_ROWS    = 2;

LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLUMNS, LCD_ROWS);

// ============================================================
// CONTROL Y TEMPORIZACIÓN
// ============================================================

constexpr uint32_t CONTROL_PERIOD_US = 10000UL;
constexpr float TS_S = 0.010f;

constexpr uint16_t TELEMETRY_PERIOD_MS = 200;
constexpr uint16_t DISPLAY_PERIOD_MS   = 250;

constexpr uint8_t N_ADC_READINGS = 20;

constexpr int PWM_MIN  = 0;
constexpr int PWM_MAX  = 255;
constexpr int PWM_BIAS = 123;

constexpr float ALPHA_Y = 0.25f;
constexpr float ALPHA_D = 0.20f;

// Presets finales obtenidos experimentalmente.
constexpr float KP_P_FINAL  = 2.743903f;
constexpr float KI_P_FINAL  = 0.0f;
constexpr float KD_P_FINAL  = 0.0f;

constexpr float KP_PI_FINAL = 2.469512f;
constexpr float KI_PI_FINAL = 10.513469f;
constexpr float KD_PI_FINAL = 0.0f;

constexpr float KP_PID_FINAL = 2.592988f;
constexpr float KI_PID_FINAL = 11.039142f;
constexpr float KD_PID_FINAL = 0.029032f;

// Límites de seguridad para parámetros recibidos remotamente.
constexpr float KP_LIMIT = 50.0f;
constexpr float KI_LIMIT = 500.0f;
constexpr float KD_LIMIT = 20.0f;

// ============================================================
// ESTADO DEL CONTROLADOR
// ============================================================

enum class ControlMode : uint8_t {
  AUTO,
  MANUAL
};

ControlMode controlMode = ControlMode::AUTO;

// Arranque conservador: primero regula alrededor del punto de operación.
// Desde ThingsBoard se puede cambiar luego el setpoint a 187.
int setpoint = 177;
int manualPwm = PWM_BIAS;

float kp = KP_PI_FINAL;
float ki = KI_PI_FINAL;
float kd = KD_PI_FINAL;

float integralState = 0.0f;
float yFiltered = 0.0f;
float yPrevious = 0.0f;
float derivativeFiltered = 0.0f;
float currentError = 0.0f;

int currentPwm = PWM_BIAS;
bool outputSaturated = false;
bool cloudOnline = false;

uint32_t nextControlUs = 0;
uint32_t lastTelemetryMs = 0;
uint32_t lastDisplayMs = 0;

// Buffer de recepción de comandos.
char commandBuffer[64];
uint8_t commandLength = 0;

// ============================================================
// FUNCIONES DE HARDWARE
// ============================================================

int readLdrAverage() {
  long sum = 0;

  for (uint8_t i = 0; i < N_ADC_READINGS; ++i) {
    sum += analogRead(PIN_LDR);
  }

  return static_cast<int>(sum / N_ADC_READINGS);
}

void applyPwm(int pwm) {
  currentPwm = constrain(pwm, PWM_MIN, PWM_MAX);
  analogWrite(PIN_LED_1, currentPwm);
  analogWrite(PIN_LED_2, currentPwm);
}

// ============================================================
// CONTROL
// ============================================================

void prepareBumplessIntegral() {
  if (ki <= 0.000001f) {
    integralState = 0.0f;
    return;
  }

  const float pTerm = kp * currentError;
  const float dTerm = kd * derivativeFiltered;

  integralState =
      (static_cast<float>(currentPwm) -
       static_cast<float>(PWM_BIAS) -
       pTerm -
       dTerm) / ki;
}

void setGains(float newKp, float newKi, float newKd) {
  newKp = constrain(newKp, 0.0f, KP_LIMIT);
  newKi = constrain(newKi, 0.0f, KI_LIMIT);
  newKd = constrain(newKd, 0.0f, KD_LIMIT);

  // Conserva aproximadamente la contribución integral al cambiar Ki.
  const float previousITerm = ki * integralState;

  kp = newKp;
  ki = newKi;
  kd = newKd;

  if (ki > 0.000001f) {
    integralState = previousITerm / ki;
  } else {
    integralState = 0.0f;
  }
}

void loadPFinal() {
  setGains(KP_P_FINAL, KI_P_FINAL, KD_P_FINAL);
}

void loadPiFinal() {
  setGains(KP_PI_FINAL, KI_PI_FINAL, KD_PI_FINAL);
}

void loadPidFinal() {
  setGains(KP_PID_FINAL, KI_PID_FINAL, KD_PID_FINAL);
}

void runControlStep() {
  const int yRaw = readLdrAverage();

  yFiltered += ALPHA_Y * (static_cast<float>(yRaw) - yFiltered);
  currentError = static_cast<float>(setpoint) - yFiltered;

  const float measuredDerivative =
      -(yFiltered - yPrevious) / TS_S;

  derivativeFiltered +=
      ALPHA_D * (measuredDerivative - derivativeFiltered);

  const float pTerm = kp * currentError;
  const float dTerm = kd * derivativeFiltered;

  if (controlMode == ControlMode::AUTO) {
    float integralCandidate = integralState;

    if (ki > 0.000001f) {
      integralCandidate += currentError * TS_S;
    }

    const float candidateOutput =
        static_cast<float>(PWM_BIAS) +
        pTerm +
        ki * integralCandidate +
        dTerm;

    const bool saturatesHigh = candidateOutput > PWM_MAX;
    const bool saturatesLow  = candidateOutput < PWM_MIN;

    const bool errorPushesFurther =
        (saturatesHigh && currentError > 0.0f) ||
        (saturatesLow  && currentError < 0.0f);

    // Anti-windup por integración condicional.
    if (!errorPushesFurther) {
      integralState = integralCandidate;
    }

    const float unsaturatedOutput =
        static_cast<float>(PWM_BIAS) +
        pTerm +
        ki * integralState +
        dTerm;

    outputSaturated =
        unsaturatedOutput > PWM_MAX ||
        unsaturatedOutput < PWM_MIN;

    applyPwm(static_cast<int>(lround(unsaturatedOutput)));

  } else {
    outputSaturated =
        manualPwm <= PWM_MIN ||
        manualPwm >= PWM_MAX;

    applyPwm(manualPwm);

    // Seguimiento interno para un retorno más suave a AUTO.
    prepareBumplessIntegral();
  }

  yPrevious = yFiltered;
}

// ============================================================
// COMANDOS DESDE EL ESP32
// ============================================================

void sendAck(const __FlashStringHelper* name) {
  Serial.print(F("ACK,"));
  Serial.println(name);
}

void processCommand(const char* command) {
  if (strncmp(command, "SET_R:", 6) == 0) {
    setpoint = constrain(atoi(command + 6), 0, 1023);
    sendAck(F("SET_R"));

  } else if (strncmp(command, "SET_KP:", 7) == 0) {
    setGains(atof(command + 7), ki, kd);
    sendAck(F("SET_KP"));

  } else if (strncmp(command, "SET_KI:", 7) == 0) {
    setGains(kp, atof(command + 7), kd);
    sendAck(F("SET_KI"));

  } else if (strncmp(command, "SET_KD:", 7) == 0) {
    setGains(kp, ki, atof(command + 7));
    sendAck(F("SET_KD"));

  } else if (strncmp(command, "SET_U:", 6) == 0) {
    manualPwm = constrain(atoi(command + 6), PWM_MIN, PWM_MAX);
    sendAck(F("SET_U"));

  } else if (strncmp(command, "SET_MODE:", 9) == 0) {
    const char* modeText = command + 9;

    if (strcmp(modeText, "AUTO") == 0) {
      prepareBumplessIntegral();
      controlMode = ControlMode::AUTO;
      sendAck(F("SET_MODE_AUTO"));

    } else if (strcmp(modeText, "MANUAL") == 0) {
      manualPwm = currentPwm;
      controlMode = ControlMode::MANUAL;
      sendAck(F("SET_MODE_MANUAL"));
    }

  } else if (strncmp(command, "SET_NET:", 8) == 0) {
    cloudOnline = atoi(command + 8) != 0;

  } else if (strcmp(command, "LOAD_P") == 0) {
    loadPFinal();
    sendAck(F("LOAD_P"));

  } else if (strcmp(command, "LOAD_PI") == 0) {
    loadPiFinal();
    sendAck(F("LOAD_PI"));
  
  } else if (strcmp(command, "LOAD_PID") == 0) {
    loadPidFinal();
    sendAck(F("LOAD_PID"));

  } else if (strcmp(command, "PING") == 0) {
    Serial.println(F("PONG"));
  }
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    const char received = static_cast<char>(Serial.read());

    if (received == '\n' || received == '\r') {
      if (commandLength > 0) {
        commandBuffer[commandLength] = '\0';
        processCommand(commandBuffer);
        commandLength = 0;
      }

    } else if (commandLength < sizeof(commandBuffer) - 1) {
      commandBuffer[commandLength++] = received;

    } else {
      // Descarta una línea demasiado larga.
      commandLength = 0;
    }
  }
}

// ============================================================
// TELEMETRÍA Y DISPLAY
// ============================================================

void sendTelemetry() {
  // Se transmiten enteros escalados para no depender de printf con float
  // en el ATmega328P y para reducir el tamaño de cada mensaje.
  const long y100      = lround(yFiltered * 100.0f);
  const long error100  = lround(currentError * 100.0f);
  const long kp10000   = lround(kp * 10000.0f);
  const long ki10000   = lround(ki * 10000.0f);
  const long kd10000   = lround(kd * 10000.0f);
  const char modeChar  =
      controlMode == ControlMode::AUTO ? 'A' : 'M';

  char line[80];

  const int length = snprintf(
      line,
      sizeof(line),
      "T,%ld,%d,%ld,%d,%ld,%ld,%ld,%c,%d\n",
      y100,
      setpoint,
      error100,
      currentPwm,
      kp10000,
      ki10000,
      kd10000,
      modeChar,
      outputSaturated ? 1 : 0
  );

  if (length > 0 && length < static_cast<int>(sizeof(line))) {
    // Si el buffer serial está ocupado, se omite esta muestra de telemetría.
    // El control de 10 ms continúa funcionando.
    if (Serial.availableForWrite() >= length) {
      Serial.write(
          reinterpret_cast<const uint8_t*>(line),
          static_cast<size_t>(length)
      );
    }
  }
}

void printPaddedLine(uint8_t row, const char* text) {
  lcd.setCursor(0, row);

  uint8_t written = 0;

  while (*text != '\0' && written < LCD_COLUMNS) {
    lcd.print(*text++);
    ++written;
  }

  while (written < LCD_COLUMNS) {
    lcd.print(' ');
    ++written;
  }
}

void updateDisplay() {
  char line1[17];
  char line2[17];

  const long y10 = lround(yFiltered * 10.0f);
  const long yInteger = y10 / 10;
  const long yDecimal = labs(y10 % 10);

  snprintf(
      line1,
      sizeof(line1),
      "Y:%3ld.%1ld R:%3d",
      yInteger,
      yDecimal,
      setpoint
  );

  snprintf(
      line2,
      sizeof(line2),
      "%s U:%3d TB:%s",
      controlMode == ControlMode::AUTO ? "AUTO" : "MAN ",
      currentPwm,
      cloudOnline ? "ON" : "--"
  );

  printPaddedLine(0, line1);
  printPaddedLine(1, line2);
}

// ============================================================
// SETUP Y LOOP
// ============================================================

void setup() {
  Serial.begin(115200);

  pinMode(PIN_LED_1, OUTPUT);
  pinMode(PIN_LED_2, OUTPUT);

  Wire.begin();

  lcd.init();
  lcd.backlight();
  printPaddedLine(0, "TPI 2026");
  printPaddedLine(1, "Inicializando...");

  applyPwm(PWM_BIAS);
  delay(1000);

  yFiltered = static_cast<float>(readLdrAverage());
  yPrevious = yFiltered;
  currentError = static_cast<float>(setpoint) - yFiltered;

  nextControlUs = micros() + CONTROL_PERIOD_US;
  lastTelemetryMs = millis();
  lastDisplayMs = millis();

  updateDisplay();
}

void loop() {
  readSerialCommands();

  const uint32_t nowUs = micros();

  if (static_cast<int32_t>(nowUs - nextControlUs) >= 0) {
    nextControlUs += CONTROL_PERIOD_US;

    // Si hubo un retraso grande, se recupera el calendario sin ejecutar
    // varios ciclos juntos.
    if (static_cast<int32_t>(nowUs - nextControlUs) >
        static_cast<int32_t>(5UL * CONTROL_PERIOD_US)) {
      nextControlUs = nowUs + CONTROL_PERIOD_US;
    }

    runControlStep();
  }

  const uint32_t nowMs = millis();

  if (static_cast<uint32_t>(nowMs - lastTelemetryMs) >=
      TELEMETRY_PERIOD_MS) {
    lastTelemetryMs = nowMs;
    sendTelemetry();
  }

  if (static_cast<uint32_t>(nowMs - lastDisplayMs) >=
      DISPLAY_PERIOD_MS) {
    lastDisplayMs = nowMs;
    updateDisplay();
  }
}
