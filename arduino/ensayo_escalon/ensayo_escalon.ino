/*
 * Ensayo en lazo abierto para identificar la planta.
 * Aplica un escalón PWM a los dos LEDs y registra la lectura del LDR.
 * Salida CSV por Serial a 115200 baudios.
 */

#define PIN_LED_1   9
#define PIN_LED_2  10
#define PIN_LDR    A0

#define PWM_INICIAL       0
#define PWM_ESCALON     180

#define TIEMPO_PRE_MS   3000
#define TIEMPO_POST_MS  15000
#define TS_MS            10

unsigned long t_inicio     = 0;
unsigned long t_ultimo     = 0;
unsigned long t_escalon    = 0;
bool          escalon_aplicado = false;
bool          ensayo_terminado = false;
int           pwm_actual   = PWM_INICIAL;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_1, OUTPUT);
  pinMode(PIN_LED_2, OUTPUT);
  analogWrite(PIN_LED_1, PWM_INICIAL);
  analogWrite(PIN_LED_2, PWM_INICIAL);

  delay(1500);

  Serial.println("# ENSAYO LAZO ABIERTO — TPI IoT 2026");
  Serial.println("# Escalon PWM: " + String(PWM_ESCALON) + " / 255");
  Serial.println("# Ts = " + String(TS_MS) + " ms");
  Serial.println("# t=0 es el inicio del registro; el escalon se aplica en t=" + String(TIEMPO_PRE_MS) + " ms");
  Serial.println("tiempo_ms,ldr_raw,pwm");

  t_inicio  = millis();
  t_ultimo  = t_inicio;
  t_escalon = t_inicio + TIEMPO_PRE_MS;
}

void loop() {
  if (ensayo_terminado) return;

  unsigned long ahora = millis();

  if (!escalon_aplicado && ahora >= t_escalon) {
    pwm_actual = PWM_ESCALON;
    analogWrite(PIN_LED_1, pwm_actual);
    analogWrite(PIN_LED_2, pwm_actual);
    escalon_aplicado = true;
  }

  if (ahora - t_ultimo >= TS_MS) {
    t_ultimo = ahora;

    unsigned long t_relativo = ahora - t_inicio;
    int ldr = analogRead(PIN_LDR);

    Serial.print(t_relativo);
    Serial.print(",");
    Serial.print(ldr);
    Serial.print(",");
    Serial.println(pwm_actual);

    if (escalon_aplicado && (ahora - t_escalon) >= TIEMPO_POST_MS) {
      analogWrite(PIN_LED_1, 0);
      analogWrite(PIN_LED_2, 0);
      Serial.println("# FIN DEL ENSAYO");
      ensayo_terminado = true;
    }
  }
}