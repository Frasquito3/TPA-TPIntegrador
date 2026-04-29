# 💡 TP IoT — Monitor de Luz con Arduino + ESP32

Sistema embebido que mide la luminosidad ambiental con un sensor LDR, controla la intensidad de un LED de forma proporcional, y envía los datos en tiempo real a la plataforma **ThingsBoard** a través de un ESP32 conectado por WiFi.

---

## 🏗️ Arquitectura del sistema

```mermaid
flowchart LR
    LDR[Sensor LDR] -->|lectura analógica| Arduino[Arduino Uno]
    Arduino -->|UART TX → RX| ESP32[ESP32]
    Arduino -->|PWM| LED[LED]
    ESP32 -->|WiFi| ThingsBoard[ThingsBoard]
```

**Flujo de datos:**

1. El **Arduino** lee el valor del sensor LDR por entrada analógica
2. En función del valor leído, ajusta el brillo del LED vía PWM
3. Envía el valor de luz al **ESP32** por comunicación serial (UART)
4. El **ESP32** recibe el dato y lo publica en **ThingsBoard** vía MQTT/WiFi

---

## 📁 Estructura del proyecto

```
tp-iot-luz/
│
├── arduino/
│   └── sensor_luz/
│       └── sensor_luz.ino           ← Sketch principal: lectura LDR + control LED
│
├── esp32/
│   └── thingsboard_uploader/
│       ├── thingsboard_uploader.ino ← Recepción UART + envío a ThingsBoard
│       ├── config.example.h         ← Plantilla de credenciales (subir al repo ✅)
│       └── config.h                 ← Credenciales reales (NO subir al repo ❌)
│
├── docs/
│   ├── diagrama_circuito.png        ← Esquemático del circuito completo
│   ├── diagrama_flujo.png           ← Diagrama de flujo del sistema
│   └── informe.md                   ← Informe técnico del TP
│
├── .gitignore
└── README.md
```

> 📌 **¿Por qué el `.ino` está dentro de una subcarpeta con el mismo nombre?** Arduino IDE lo requiere obligatoriamente: el archivo `sensor_luz.ino` debe estar dentro de una carpeta llamada exactamente `sensor_luz`, de lo contrario el IDE no lo reconoce ni lo abre correctamente.

---

## ⚙️ Configuración inicial del entorno (Windows)

Seguir estos pasos **una sola vez** por máquina antes de empezar a trabajar en el proyecto.

### Paso 1 — Instalar Arduino IDE

1. Ir a [arduino.cc/en/software](https://www.arduino.cc/en/software)
2. Descargar **Arduino IDE 2** para Windows
3. Ejecutar el instalador y seguir los pasos (siguiente, siguiente, instalar)
4. Al finalizar, abrir Arduino IDE

### Paso 2 — Agregar soporte para ESP32

El Arduino IDE viene preparado solo para placas Arduino. Para poder programar el ESP32 hay que agregar su paquete de soporte manualmente.

1. Ir a `File → Preferences`
2. En el campo **"Additional boards manager URLs"** pegar la siguiente URL:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Hacer click en **OK**
4. Ir a `Tools → Board → Boards Manager`
5. En el buscador escribir `esp32`
6. Instalar el paquete **"esp32 by Espressif Systems"** (puede tardar unos minutos)

> ✅ Una vez instalado, la librería `WiFi.h` queda disponible automáticamente. No hace falta buscarla ni instalarla por separado.

### Paso 3 — Instalar el driver del ESP32 (CP210x)

Windows no reconoce el ESP32 automáticamente: necesita un driver para poder comunicarse con él por USB. El ESP32 WROOM-32 usa el chip **CP210x** de Silicon Labs.

1. Ir a [silabs.com/developers/usb-to-uart-bridge-vcp-drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers?tab=downloads)
2. Descargar **"CP210x Windows Drivers"**
3. Descomprimir el zip e instalar el `.exe` correspondiente
4. Conectar el ESP32 por USB → debería aparecer como `COMx` en el Administrador de dispositivos

> ⚠️ Si después de instalar el driver el ESP32 **sigue sin aparecer** en la lista de puertos COM, puede que la placa use el chip **CH340** en lugar del CP210x (depende del fabricante). En ese caso instalar el driver desde [wch-ic.com/downloads/CH341SER_EXE.html](https://www.wch-ic.com/downloads/CH341SER_EXE.html).

### Paso 4 — Seleccionar la placa correcta en Arduino IDE

Al ir a `Tools → Board` aparece una lista enorme de opciones que puede ser confusa. Esta tabla muestra qué elegir según el modelo:

| Tengo en la mano                 | Seleccionar en Arduino IDE                      |
| -------------------------------- | ----------------------------------------------- |
| ESP32 WROOM-32 / 32D / 32E / 32U | **ESP32 Dev Module** ✅                         |
| ESP32 DevKit V1                  | **ESP32 Dev Module** ✅                         |
| ESP32 DevKitC                    | **ESP32 Dev Module** ✅                         |
| No sé exactamente qué modelo es  | **ESP32 Dev Module** (funciona para la mayoría) |

> 📌 **ESP32 Dev Module** es la opción correcta para el WROOM-32 y para la gran mayoría de las placas de desarrollo ESP32 genéricas.

### Paso 5 — Instalar librerías necesarias

Ir a `Sketch → Include Library → Manage Libraries` e instalar las siguientes:

| Librería       | Autor           | Usado en | Para qué                          |
| -------------- | --------------- | -------- | --------------------------------- |
| `PubSubClient` | Nick O'Leary    | ESP32    | Comunicación MQTT con ThingsBoard |
| `ArduinoJson`  | Benoit Blanchon | ESP32    | Serialización de datos            |

> 📌 **No instalar la librería `ThingsBoard`**. La comunicación con ThingsBoard se hace directamente vía MQTT usando `PubSubClient`, que es más simple y no tiene problemas de compatibilidad de versiones.

> 📌 Las librerías `WiFi` y `PubSubClient` **no requieren instalación separada** — `WiFi` viene incluida con el paquete ESP32 del Paso 2, y `PubSubClient` se instala desde el Library Manager como se indica arriba.

### Paso 6 — Configurar credenciales del ESP32

El archivo `config.h` del ESP32 contiene las credenciales WiFi y el token de ThingsBoard. Como cada integrante tiene su propia red y su propio dispositivo en ThingsBoard, **este archivo no se sube al repositorio** (está en el `.gitignore`). En cambio, se sube `config.example.h` como plantilla.

Copiar la plantilla y renombrarla:

```
esp32/thingsboard_uploader/config.example.h  →  config.h
```

Editar `config.h` con las credenciales personales:

```cpp
#define WIFI_SSID     "nombre_de_tu_red"
#define WIFI_PASSWORD "contraseña_wifi"
#define TB_TOKEN      "token_del_dispositivo"
#define TB_SERVER     "thingsboard.cloud"
```

> 🔒 El archivo `config.h` **nunca se sube al repositorio**. Si alguien lo sube por error, las credenciales quedan expuestas públicamente y hay que cambiarlas de inmediato.

> 📌 El `config.h` del **Arduino** sí se sube al repo sin problema, ya que solo contiene constantes de hardware (pines, umbrales) que son iguales para todos y no son secretas.

### Paso 7 — Cargar los sketches a las placas

1. Abrir `arduino/sensor_luz/sensor_luz.ino` con Arduino IDE
2. Ir a `Tools → Board` → seleccionar **Arduino Uno** (o el modelo que usen)
3. Ir a `Tools → Port` → seleccionar el puerto COM donde está conectado el Arduino
4. Click en **Upload** (→)
5. Repetir para el ESP32: abrir `esp32/thingsboard_uploader/thingsboard_uploader.ino`
6. En `Tools → Board` → seleccionar **ESP32 Dev Module**
7. Seleccionar el puerto COM del ESP32 y hacer **Upload**

---

## 🔧 Configuración de ThingsBoard

1. Crear una cuenta en [thingsboard.cloud](https://thingsboard.cloud) (gratuito)
2. Ir a **Devices** → **Add Device** → Darle un nombre (ej: `sensor-luz-tp`)
3. Abrir el dispositivo → pestaña **"Credentials"** → copiar el **Access Token**
4. Pegarlo en `esp32/thingsboard_uploader/config.h` como valor de `TB_TOKEN`
5. Desde el dashboard de ThingsBoard se puede visualizar la telemetría en tiempo real

---

## 🤝 Flujo de trabajo en equipo (Git)

Usamos **Git + GitHub** para versionar todo el proyecto: código, documentación y diagramas. La herramienta del día a día es **GitHub Desktop**.

### Herramientas por tipo de archivo

| Qué modificar                                 | Con qué programa                              |
| --------------------------------------------- | --------------------------------------------- |
| Sketches `.ino`                               | Arduino IDE                                   |
| `README.md`, `informe.md`, `config.example.h` | VS Code, Notepad++, cualquier editor de texto |
| Diagramas e imágenes                          | La herramienta que usen para los esquemáticos |
| Versionar y sincronizar todo                  | GitHub Desktop                                |

GitHub Desktop detecta automáticamente cualquier archivo modificado, sin importar con qué programa lo editaron.

### Rutina diaria

```
1. Abrir GitHub Desktop → Pull (traerse los últimos cambios)
2. Editar lo que corresponda (Arduino IDE, editor de texto, etc.)
3. GitHub Desktop → escribir mensaje de commit → Commit → Push
```

> ⚠️ Hacer siempre **Pull antes de empezar** a trabajar para evitar conflictos.

### Convención de mensajes de commit

Usar prefijos para que el historial sea claro:

```
feat:     add LDR sensor reading
fix:      fix PWM value mapping
docs:     update circuit diagram
refactor: separate sending logic into a dedicated function
```

---

## Requisitos de hardware

| Componente          | Descripción                     |
| ------------------- | ------------------------------- |
| Arduino Uno / Nano  | Microcontrolador principal      |
| ESP32               | Módulo WiFi para envío de datos |
| Sensor LDR          | Sensor de luz (fotoresistencia) |
| LED                 | LED estándar (cualquier color)  |
| Resistencia 220Ω    | Para el LED                     |
| Resistencia 10kΩ    | Divisor de tensión para el LDR  |
| Cables / protoboard | Conexionado                     |

### Conexiones UART

| Arduino    | ESP32        |
| ---------- | ------------ |
| TX (pin 1) | RX (GPIO 16) |
| GND        | GND          |

> ⚠️ El ESP32 trabaja a **3.3V**. Si el Arduino opera a 5V, usar un divisor de tensión o un level shifter en la línea TX→RX.

---

## 👥 Integrantes

|                                                    Avatar                                                    | Nombre Completo                   |                                                         Perfil de GitHub                                                          |
| :----------------------------------------------------------------------------------------------------------: | :-------------------------------- | :-------------------------------------------------------------------------------------------------------------------------------: |
|  <img src="https://github.com/carlex74.png" width="50" alt="Avatar de Carlos" style="border-radius: 50%;">   | Carlos Ricardo Gugliermino Zuñiga |   [![GitHub: carlex74](https://img.shields.io/badge/GitHub-Profile-blue?style=social&logo=github)](https://github.com/carlex74)   |
| <img src="https://github.com/NiconiKImg.png" width="50" alt="Avatar de Nicolás" style="border-radius: 50%;"> | Nicolás Pedemonte                 | [![GitHub: NiconiKImg](https://img.shields.io/badge/GitHub-Profile-blue?style=social&logo=github)](https://github.com/NiconiKImg) |
|    <img src="https://github.com/LucaTvl.png" width="50" alt="Avatar de Luca" style="border-radius: 50%;">    | Luca Trincavelli                  |    [![GitHub: LucaTvl](https://img.shields.io/badge/GitHub-Profile-blue?style=social&logo=github)](https://github.com/LucaTvl)    |
| <img src="https://github.com/Frasquito3.png" width="50" alt="Avatar de Franco" style="border-radius: 50%;">  | Franco Zariaga                    | [![GitHub: Frasquito3](https://img.shields.io/badge/GitHub-Profile-blue?style=social&logo=github)](https://github.com/Frasquito3) |
