# 🔧 Configuración y Conexiones del Hardware

Este documento detalla los pasos para configurar el entorno de desarrollo y las conexiones físicas entre el Arduino, el ESP32 y los componentes.

---

## 🔌 Conexiones de Hardware

### Componentes Necesarios
- Microcontrolador: Arduino Uno / Nano
- Módulo de comunicación: ESP32
- Sensor: LDR (Fotoresistencia)
- Actuador: LED estándar (cualquier color)
- Resistencias: 220Ω (para el LED), 10kΩ (para el LDR y divisor de tensión)
- Cables y protoboard

### Conexión Arduino ↔ ESP32 (Comunicación UART)
El Arduino se comunica con el ESP32 enviando los datos del sensor por el puerto Serial (a 9600 baudios).

| Arduino    | ESP32        |
| ---------- | ------------ |
| TX (Pin 1) | RX (GPIO 16) |
| GND        | GND          |

> ⚠️ **Divisor de Tensión:** El Arduino opera con lógica de 5V, mientras que el ESP32 trabaja a **3.3V**. Para evitar dañar el ESP32, es **OBLIGATORIO** colocar un divisor de tensión (o usar un level shifter) en la línea que va del TX del Arduino al RX del ESP32.

**Esquema de conexión con Divisor de Tensión:**
```text
Arduino TX (Pin 1) ──[1kΩ]──┬──── ESP32 RX (GPIO 16)
                            │
                          [2kΩ]
                            │
                           GND ──── Arduino GND ──── ESP32 GND
```

> ⚠️ **Importante para la carga de código:** Al subir un sketch al Arduino, se debe **desconectar el cable TX (pin 1)**. Una vez finalizada la carga, volver a conectarlo para que se comunique con el ESP32.

### Sensor LDR y LED
- **Sensor LDR:** Debe conectarse formando un divisor de tensión junto con una resistencia de 10kΩ hacia uno de los pines analógicos del Arduino.
- **LED:** Conectarlo a un pin digital con capacidad PWM del Arduino, utilizando en serie una resistencia limitadora de 220Ω.

---

## ⚙️ Configuración inicial del entorno (Windows)

Seguir estos pasos **una sola vez** por máquina antes de empezar a trabajar en el proyecto.

### Paso 1 — Instalar Arduino IDE
1. Ir a [arduino.cc/en/software](https://www.arduino.cc/en/software) y descargar **Arduino IDE 2** para Windows.

### Paso 2 — Agregar soporte para ESP32
1. Ir a `File → Preferences`.
2. En **"Additional boards manager URLs"** pegar: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Ir a `Tools → Board → Boards Manager`, buscar `esp32` e instalar el paquete **"esp32 by Espressif Systems"**.

### Paso 3 — Instalar el driver del ESP32 (CP210x)
1. Descargar **"CP210x Windows Drivers"** desde [silabs.com](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=downloads) y ejecutar el instalador.
2. Si el dispositivo no aparece, puede usar el chip **CH340**. Descargar desde [wch-ic.com](https://www.wch-ic.com/downloads/CH341SER_EXE.html).

### Paso 4 — Seleccionar la placa correcta
| Placa                            | Seleccionar en Arduino IDE                      |
| -------------------------------- | ----------------------------------------------- |
| ESP32 WROOM-32 / DevKit / DevKitC| **ESP32 Dev Module** ✅                         |

### Paso 5 — Instalar librerías necesarias
Ir a `Sketch → Include Library → Manage Libraries` e instalar:
- `PubSubClient` por Nick O'Leary (Para MQTT).
- `ArduinoJson` por Benoit Blanchon (Para serialización).

*(Nota: Las librerías `WiFi` y `PubSubClient` no requieren descargas externas extra).*

### Paso 6 — Configurar credenciales del ESP32
Copiar la plantilla de credenciales y renombrarla:
```
esp32/thingsboard_uploader/config.example.h  →  config.h
```
Editar `config.h` con los datos reales:
```cpp
const char* WIFI_SSID     = "nombre_de_tu_red";
const char* WIFI_PASSWORD = "contraseña_wifi";
const char* TB_TOKEN      = "token_del_dispositivo";
const char* TB_SERVER     = "thingsboard.cloud";
const int   TB_PORT       = 1883;
```
> 🔒 **Nunca** subir `config.h` al repositorio.

### Paso 7 — Cargar los sketches
1. Abrir y cargar `arduino/sensor_luz/sensor_luz.ino` en el **Arduino Uno** (recordar desconectar el Pin TX temporalmente).
2. Abrir y cargar `esp32/thingsboard_uploader/thingsboard_uploader.ino` en el **ESP32 Dev Module**.

---

## ☁️ Configuración de ThingsBoard

1. Crear una cuenta gratuita en [thingsboard.cloud](https://thingsboard.cloud).
2. Ir a **Devices** → **Add Device** y nombrarlo (ej: `sensor-luz-tp`).
3. En la pestaña **"Credentials"** del dispositivo creado, copiar el **Access Token**.
4. Pegarlo en el archivo `config.h` del ESP32 como valor de `TB_TOKEN`.
5. Visualizar los datos entrantes en la pestaña "Latest Telemetry" o en un Dashboard.
