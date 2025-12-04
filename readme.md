# UCQ-Bot - Robot Controlado por ESP32-CAM

Este proyecto implementa un robot controlado remotamente a través de WiFi utilizando un módulo ESP32-CAM. El robot genera su propia red WiFi (Punto de Acceso) y ofrece una interfaz web moderna para visualizar la cámara en tiempo real y controlar los movimientos.

## Descripción General

El **UCQ-Bot** es un vehículo robotizado que permite:
*   Ver video en tiempo real desde la cámara del ESP32.
*   Controlar el movimiento del robot (Avanzar, Retroceder, Girar) desde cualquier navegador web (celular o PC).
*   Encender y apagar el flash LED para iluminar entornos oscuros.
*   Funcionar de manera autónoma sin necesidad de un router externo, ya que crea su propia red WiFi.

## Guía de Conexión Paso a Paso

Sigue estos pasos para conectar y controlar tu robot:

1.  **Enciende el Robot:** Conecta la batería o fuente de alimentación a tu ESP32-CAM.
2.  **Busca la Red WiFi:**
    *   Abre los ajustes de WiFi en tu celular, tablet o computadora.
    *   Busca una red llamada **`UCQ-Bot`**.
3.  **Conéctate:**
    *   Selecciona la red e introduce la contraseña: **`12345678`**.
    *   *Nota: Es posible que tu celular te diga que "no hay conexión a internet". Esto es normal, mantén la conexión de todos modos.*
4.  **Abre el Navegador:**
    *   Entra a tu navegador web favorito (Chrome, Safari, etc.).
5.  **Ingresa la Dirección:**
    *   En la barra de direcciones escribe: **`192.168.4.1`** y presiona "Ir" o "Enter".
6.  **¡Listo!**
    *   Verás la interfaz de control. Presiona "Iniciar Cámara" para ver el video y usa los botones para moverte.

## Archivos del Proyecto y Funciones

A continuación se describen los archivos principales y las funciones que contienen:

### 1. `ESP32_CAM_Robot_Car.ino`
Es el archivo principal de Arduino. Se encarga de la configuración inicial y el bucle principal.

| Función | Descripción |
| :--- | :--- |
| `setup()` | Configura la velocidad Serial, inicializa la cámara, establece el WiFi en modo AP y configura los pines del robot. |
| `loop()` | Bucle infinito que vigila el tiempo de inactividad para detener el robot automáticamente por seguridad. |

### 2. `app_httpd.cpp`
Contiene la lógica del servidor web, el manejo de la cámara y el control de los motores.

| Función | Descripción |
| :--- | :--- |
| `startCameraServer()` | Inicia el servidor web y define las rutas (URL) para el control y el video. |
| `index_handler()` | Envía el código HTML, CSS y JavaScript al navegador cuando entras a la página. |
| `stream_handler()` | Captura frames de la cámara continuamente y los envía como un video MJPEG. |
| `cmd_handler()` | Recibe los comandos de la web (mover, luz, velocidad) y ejecuta la acción correspondiente. |
| `configurar_robot()` | Configura los canales PWM y asigna los pines para controlar los motores. |
| `parar_robot()` | Detiene todos los motores poniendo sus pines a 0. |
| `avanzar_robot()` | Activa los motores para mover el robot hacia adelante. |
| `retroceder_robot()` | Activa los motores para mover el robot hacia atrás. |
| `girar_izquierda()` | Hace girar el robot a la izquierda (motores en direcciones opuestas). |
| `girar_derecha()` | Hace girar el robot a la derecha (motores en direcciones opuestas). |
| `obtener_velocidad()` | Convierte el valor de porcentaje (0-100) a valor PWM (0-255). |

## Funciones y Controles Web

La interfaz web (`http://192.168.4.1`) ofrece las siguientes funcionalidades:

*   **Video en Vivo:** Visualización directa de lo que ve el robot.
*   **Iniciar Cámara:** Botón para arrancar o detener la transmisión de video.
*   **Flash:** Botón para encender o apagar el LED de alta potencia del ESP32-CAM.
*   **Controles de Movimiento:**
    *   **▲ Avanzar:** Mueve el robot hacia adelante.
    *   **▼ Retroceder:** Mueve el robot hacia atrás.
    *   **◄ Izquierda:** Gira el robot a la izquierda sobre su propio eje.
    *   **► Derecha:** Gira el robot a la derecha sobre su propio eje.
*   **Control de Velocidad:** Un deslizador (slider) para ajustar la velocidad de los motores (PWM).

## Configuración de Red
*   **Nombre de Red (SSID):** `UCQ-Bot`
*   **Contraseña:** `12345678`
