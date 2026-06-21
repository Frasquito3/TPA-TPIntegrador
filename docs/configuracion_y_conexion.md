# Configuración y conexiones

Esta guía resume el conexionado del sistema final y los pasos necesarios para cargar los programas del Arduino UNO y del ESP32.

## Componentes

- Arduino UNO.
- ESP32 compatible con la configuración `ESP32 Dev Module`.
- Dos LEDs blancos.
- Dos resistencias de 220 Ω.
- Un sensor LDR.
- Una resistencia de 10 kΩ para el divisor del LDR.
- Una resistencia de 1,8 kΩ.
- Una resistencia de 3,3 kΩ.
- Display LCD 16x2 con interfaz I2C.
- Protoboard.
- Cables Dupont.
- Cables USB para la programación y alimentación de las placas.

## Conexiones del Arduino

### Sensor LDR

El LDR y la resistencia de 10 kΩ forman un divisor de tensión conectado a la entrada analógica A0.

```text
5 V ── LDR ──┬── A0
             │
           10 kΩ
             │
            GND
```

Con esta disposición, la tensión sobre A0 y la lectura ADC aumentan cuando aumenta la iluminación recibida por el LDR.

| Elemento | Conexión |
| --- | --- |
| Extremo superior del LDR | 5 V |
| Unión entre LDR y resistencia | A0 |
| Extremo inferior de la resistencia de 10 kΩ | GND |

### LEDs actuadores

Los dos LEDs se controlan mediante salidas PWM independientes, aunque reciben el mismo valor de mando.

```text
Pin 9  ── LED 1 ── 220 Ω ── GND
Pin 10 ── LED 2 ── 220 Ω ── GND
```

| Actuador | Pin del Arduino |
| --- | --- |
| LED 1 | Pin 9 |
| LED 2 | Pin 10 |

Cada LED debe conectarse en serie con una resistencia de 220 Ω para limitar la corriente.

### Display LCD 16x2 I2C

| LCD | Arduino UNO |
| --- | --- |
| VCC | 5 V |
| GND | GND |
| SDA | A4 |
| SCL | A5 |

El firmware utiliza por defecto la dirección I2C:

```text
0x27
```

Si el display no responde, se debe verificar su dirección mediante un escáner I2C.

## Comunicación Arduino–ESP32

La comunicación entre las placas es UART bidireccional a **115200 baudios**, con ocho bits de datos, sin paridad y un bit de parada.

| Origen | Destino | Conexión |
| --- | --- | --- |
| Arduino TX, pin 1 | ESP32 RX2, GPIO 16 | Mediante divisor resistivo |
| ESP32 TX2, GPIO 17 | Arduino RX, pin 0 | Conexión directa |
| GND del Arduino | GND del ESP32 | Masa común |

### Adaptación del nivel lógico

La salida TX del Arduino trabaja con niveles cercanos a 5 V. La entrada del ESP32 admite 3,3 V, por lo que la conexión Arduino TX → ESP32 RX2 debe incluir un divisor resistivo.

```text
Arduino TX ── 1,8 kΩ ──┬── ESP32 RX2, GPIO 16
                       │
                     3,3 kΩ
                       │
                      GND
```

La señal de 3,3 V transmitida por el ESP32 es reconocida correctamente por la entrada RX del Arduino, por lo que la conexión ESP32 TX2 → Arduino RX puede realizarse directamente.

Las dos placas deben compartir GND para utilizar la misma referencia eléctrica.

> Antes de cargar un programa en el Arduino UNO, desconectar temporalmente las conexiones de los pines 0 y 1. Una vez finalizada la carga, volver a conectarlas.

## Entorno de desarrollo

## Arduino UNO

En Arduino IDE seleccionar:

```text
Tools → Board → Arduino AVR Boards → Arduino Uno
```

Para el firmware final se debe instalar la librería correspondiente al display:

```text
LiquidCrystal_I2C
```

## ESP32

Agregar el soporte para ESP32 en Arduino IDE mediante la URL de Espressif:

```text
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Después seleccionar:

```text
Tools → Board → esp32 → ESP32 Dev Module
```

Instalar las librerías utilizadas por el gateway:

- `PubSubClient`.
- `ArduinoJson`.

La librería `WiFi` se incluye con el paquete de placas ESP32.

## Credenciales del gateway

Dentro de la carpeta:

```text
esp32/thingsboard_uploader/
```

copiar el archivo:

```text
config.example.h
```

con el nombre:

```text
config.h
```

Completar los valores reales en `config.h`:

```cpp
#pragma once

constexpr char WIFI_SSID[] = "nombre_de_tu_red";
constexpr char WIFI_PASSWORD[] = "contraseña_wifi";

constexpr char TB_TOKEN[] = "token_del_dispositivo";
constexpr char TB_SERVER[] = "thingsboard.cloud";
constexpr uint16_t TB_PORT = 1883;

constexpr char TB_TELEMETRY_TOPIC[] =
    "utn/2026/c02/gXX/telemetry";
```

El archivo `config.h` contiene información privada y no debe subirse al repositorio.

La plantilla `config.example.h` debe conservarse para indicar qué valores necesita completar cada usuario.

## Carga de los programas

### Ensayo de respuesta al escalón

Archivo:

```text
arduino/ensayo_escalon/ensayo_escalon.ino
```

Este programa aplica un escalón PWM en lazo abierto y registra la evolución de la lectura del LDR.

Procedimiento general:

1. Mantener la caja cerrada.
2. Conectar los LEDs y el LDR.
3. Cargar el sketch en el Arduino.
4. Abrir el monitor serie a 115200 baudios.
5. Reiniciar el Arduino.
6. Guardar los datos generados.

### Corridas experimentales P, PI y PID

Archivo:

```text
arduino/pid_controller/pid_controller.ino
```

Este programa se utiliza para ejecutar corridas automáticas de los controladores y obtener los datos empleados durante la sintonización.

Antes de cargarlo se deben verificar:

- El tipo de controlador seleccionado.
- Los valores de `Kp`, `Ki` y `Kd`.
- La referencia inicial.
- La referencia final.
- El período de muestreo.
- La duración de la corrida.

Los datos generados por el monitor serie pueden copiarse para su análisis en MATLAB.

### Sistema final

Archivos:

```text
arduino/sensor_luz/sensor_luz.ino
esp32/thingsboard_uploader/thingsboard_uploader.ino
```

Orden recomendado:

1. Desconectar temporalmente las líneas UART del Arduino.
2. Cargar `sensor_luz.ino` en el Arduino UNO.
3. Cargar `thingsboard_uploader.ino` en el ESP32.
4. Reconectar las líneas UART.
5. Compartir GND entre ambas placas.
6. Encender las dos placas.
7. Verificar los valores mostrados en el LCD.
8. Comprobar la recepción de telemetría en ThingsBoard.

## Configuración de ThingsBoard

La conexión utiliza los siguientes parámetros:

| Parámetro | Valor |
| --- | --- |
| Servidor | `thingsboard.cloud` |
| Puerto | `1883` |
| Autenticación | Token del dispositivo |
| Formato de telemetría | JSON |
| Topic | `utn/2026/c02/gXX/telemetry` |

La denominación `gXX` se conserva porque la cátedra no asignó un número de grupo específico.

El topic configurado en el ESP32 debe coincidir exactamente con el filtro configurado en el Device Profile de ThingsBoard.

## Telemetría publicada

| Clave | Descripción |
| --- | --- |
| `y` | Salida filtrada utilizada por el controlador |
| `r` | Referencia activa |
| `e` | Error de seguimiento |
| `u` | PWM realmente aplicado |
| `kp` | Ganancia proporcional |
| `ki` | Ganancia integral |
| `kd` | Ganancia derivativa |
| `mode` | Modo `AUTO` o `MANUAL` |
| `saturated` | Estado de saturación |
| `arduinoOnline` | Recepción reciente de datos del Arduino |
| `wifiRssi` | Intensidad de la señal WiFi |
| `online` | Estado declarado por el gateway |

## Métodos RPC

| Método | Acción |
| --- | --- |
| `setSetpoint` | Modifica la referencia |
| `setKp` | Modifica Kp |
| `setKi` | Modifica Ki |
| `setKd` | Modifica Kd |
| `setMode` | Selecciona `AUTO` o `MANUAL` |
| `setManualPwm` | Fija la salida PWM en modo MANUAL |
| `loadP` | Carga el preset P |
| `loadPI` | Carga el preset PI |
| `loadPID` | Carga el preset PID |

La respuesta RPC confirma que el ESP32 recibió e interpretó la solicitud.

La aplicación efectiva debe comprobarse mediante la telemetría posterior enviada por el Arduino.

## Configuraciones disponibles

| Controlador | Kp | Ki | Kd |
| --- | ---: | ---: | ---: |
| P | 2,743903 | 0 | 0 |
| PI | 2,469512 | 10,513469 | 0 |
| PID | 2,592988 | 11,039142 | 0,029032 |

## Períodos de operación

| Proceso | Período |
| --- | ---: |
| Control local del Arduino | 10 ms |
| Telemetría Arduino–ESP32 | 200 ms |
| Publicación en ThingsBoard | 4000 ms |

Las solicitudes RPC se procesan cuando llegan al ESP32 y no dependen del período de publicación de la telemetría.

## Comprobaciones básicas

Antes de ejecutar el sistema verificar:

- El LDR está conectado a A0.
- Los LEDs están conectados a los pines 9 y 10.
- Cada LED posee una resistencia de 220 Ω.
- El divisor de nivel lógico se encuentra en la línea Arduino TX → ESP32 RX2.
- Las placas comparten GND.
- El archivo `config.h` contiene credenciales válidas.
- El topic coincide con el Device Profile.
- El monitor serie utiliza 115200 baudios.
- El LCD utiliza la dirección I2C correcta.
- Las líneas UART fueron reconectadas después de programar el Arduino.

## Seguridad de las credenciales

No deben publicarse:

- La contraseña de la red WiFi.
- El token del dispositivo.
- El archivo `config.h` real.

Sí debe publicarse:

- `config.example.h`.
- La documentación del topic.
- Los nombres de las variables necesarias.
- Las instrucciones para crear la configuración local.