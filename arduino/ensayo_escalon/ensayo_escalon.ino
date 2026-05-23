/*
 * ============================================================
 *  ENSAYO EN LAZO ABIERTO — Respuesta al Escalón
 *  TPI 2026 — Tecnologías para la Automatización
 *  UTN FRRo — Ingeniería en Sistemas de Información
 * ============================================================
 *
 *  PROPÓSITO:
 *  Aplicar un escalón de PWM al LED y registrar la respuesta
 *  del LDR para estimar el modelo de la planta G(s) = K/(τs+1)
 *
 *  IMPORTANTE: Este sketch es SOLO para el ensayo de modelado.
 *  No es el código de control PID del proyecto final.
 *
 *  PINS:
 *    LED  → Pin 9  (PWM)
 *    LDR  → Pin A0 (Analógico)
 *
 *  CÓMO USARLO:
 *  1. Subir este sketch al Arduino (desconectar TX del ESP32 antes)
 *  2. Abrir el Serial Monitor a 9600 baudios
 *  3. Asegurarse de que la caja está cerrada (sin luz externa)
 *  4. El ensayo arranca automáticamente al energizar
 *  5. Copiar la salida del Serial Monitor a un archivo .csv
 *
 *  FORMATO DE SALIDA (CSV):
 *  tiempo_ms,ldr_raw,pwm
 *  0,312,0
 *  100,313,0
 *  ...
 *  3000,315,180     ← acá se aplica el escalón
 *  3100,341,180
 *  ...
 * ============================================================
 */

// ── Configuración de pines ───────────────────────────────────
#define PIN_LED   9    // Pin PWM para el LED
#define PIN_LDR   A0   // Pin analógico para el LDR

// ── Parámetros del ensayo ────────────────────────────────────
#define PWM_INICIAL       0    // Valor PWM antes del escalón (0 = LED apagado)
#define PWM_ESCALON     180    // Valor PWM del escalón (0–255). 180 ≈ 70% de potencia.
                               // Ajustar si el sistema satura muy rápido o es muy lento.

#define TIEMPO_PRE_MS   3000   // Tiempo con LED apagado antes del escalón (ms)
                               // Sirve para ver la línea base del LDR.

#define TIEMPO_POST_MS  15000  // Tiempo de registro después del escalón (ms)
                               // Debe ser al menos 5τ para ver la respuesta completa.
                               // Si no sabés τ, dejar en 15 seg y ajustar después.

#define TS_MS            100   // Período de muestreo (ms).
                               // 100ms = 10 muestras/seg, suficiente para este sistema.

// ── Variables del ensayo ─────────────────────────────────────
unsigned long t_inicio     = 0;
unsigned long t_ultimo     = 0;
unsigned long t_escalon    = 0;
bool          escalon_aplicado = false;
bool          ensayo_terminado = false;
int           pwm_actual   = PWM_INICIAL;

// ============================================================
void setup() {
  Serial.begin(9600);
  pinMode(PIN_LED, OUTPUT);
  analogWrite(PIN_LED, PWM_INICIAL);

  // Esperar un momento para que el Serial Monitor se conecte
  delay(1500);

  // Encabezado del CSV — copiar todo desde acá en el Serial Monitor
  Serial.println("# ENSAYO LAZO ABIERTO — TPI IoT 2026");
  Serial.println("# Escalon PWM: " + String(PWM_ESCALON) + " / 255");
  Serial.println("# Ts = " + String(TS_MS) + " ms");
  Serial.println("# t=0 es el inicio del registro; el escalon se aplica en t=" + String(TIEMPO_PRE_MS) + " ms");
  Serial.println("tiempo_ms,ldr_raw,pwm");

  t_inicio  = millis();
  t_ultimo  = t_inicio;
  t_escalon = t_inicio + TIEMPO_PRE_MS;
}

// ============================================================
void loop() {
  if (ensayo_terminado) return;   // No hacer nada al terminar

  unsigned long ahora = millis();

  // Aplicar escalón cuando llegue el momento
  if (!escalon_aplicado && ahora >= t_escalon) {
    pwm_actual = PWM_ESCALON;
    analogWrite(PIN_LED, pwm_actual);
    escalon_aplicado = true;
  }

  // Muestrear cada TS_MS milisegundos
  if (ahora - t_ultimo >= TS_MS) {
    t_ultimo = ahora;

    unsigned long t_relativo = ahora - t_inicio;
    int ldr = analogRead(PIN_LDR);

    // Formato: tiempo_ms,ldr_raw,pwm
    Serial.print(t_relativo);
    Serial.print(",");
    Serial.print(ldr);
    Serial.print(",");
    Serial.println(pwm_actual);

    // Finalizar ensayo después del tiempo post-escalón
    if (escalon_aplicado && (ahora - t_escalon) >= TIEMPO_POST_MS) {
      analogWrite(PIN_LED, 0);   // Apagar LED al terminar
      Serial.println("# FIN DEL ENSAYO");
      ensayo_terminado = true;
    }
  }
}
