/*
  ESP32_CAM_Robot_Car
  app_httpd.cpp
  Controlador del Robot y Servidor Web
  
  Este archivo maneja:
  1. La cámara y la transmisión de video.
  2. El servidor web que muestra la página de control.
  3. Los comandos para mover los motores.
*/

#include "dl_lib_matrix3d.h"
#include <esp32-hal-ledc.h>
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "Arduino.h"

// Definición de pines de los motores
#define IZQUIERDA_M0     13
#define IZQUIERDA_M1     12
#define DERECHA_M0       14
#define DERECHA_M1       15

// Variables de velocidad
int velocidad = 255;

// Configuración de PWM
const int frecuencia = 2000;
const int canalPWM = 8;
const int resolucion = 8;

volatile unsigned int  velocidad_motor   = 200;
volatile unsigned long tiempo_anterior = 0;
volatile unsigned long intervalo_movimiento = 250;

// Declaración de funciones
void configurar_robot();
void parar_robot();
void avanzar_robot();
void retroceder_robot();
void girar_izquierda();
void girar_derecha();
uint8_t estado_robot = 0; // 0 = Parado, 1 = Moviéndose

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

// Manejador para el stream de video en vivo
static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  static int64_t last_frame = 0;
  if (!last_frame) {
    last_frame = esp_timer_get_time();
  }

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Fallo al capturar frame de cámara");
      res = ESP_FAIL;
    } else {
      if (fb->format != PIXFORMAT_JPEG) {
        bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
        esp_camera_fb_return(fb);
        fb = NULL;
        if (!jpeg_converted) {
          Serial.println("Fallo compresión JPEG");
          res = ESP_FAIL;
        }
      } else {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
      }
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) {
      break;
    }
    int64_t fr_end = esp_timer_get_time();
    int64_t frame_time = fr_end - last_frame;
    last_frame = fr_end;
    frame_time /= 1000;
  }

  last_frame = 0;
  return res;
}

// Manejador de comandos (Movimiento, Luces)
static esp_err_t cmd_handler(httpd_req_t *req)
{
  char*  buf;
  size_t buf_len;
  char variable[32] = {0,};
  char value[32] = {0,};

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char*)malloc(buf_len);
    if (!buf) {
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
          httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) {
      } else {
        free(buf);
        httpd_resp_send_404(req);
        return ESP_FAIL;
      }
    } else {
      free(buf);
      httpd_resp_send_404(req);
      return ESP_FAIL;
    }
    free(buf);
  } else {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  int val = atoi(value);
  int res = 0;

  // Interpretar comandos
  if (!strcmp(variable, "flash")) {
    ledcWrite(7, val); // Control del LED Flash
  }
  else if (!strcmp(variable, "speed")) {
    if      (val > 255) val = 255;
    else if (val <   0) val = 0;
    velocidad = val;
  }
  else if (!strcmp(variable, "car")) {
    // Lógica de movimiento
    if (val == 1) {
      Serial.println("Avanzar");
      avanzar_robot();
      estado_robot = 1;
    }
    else if (val == 2) {
      Serial.println("Izquierda");
      girar_izquierda();
      estado_robot = 1;
    }
    else if (val == 3) {
      Serial.println("Parar");
      parar_robot();
      estado_robot = 0;
    }
    else if (val == 4) {
      Serial.println("Derecha");
      girar_derecha();
      estado_robot = 1;
    }
    else if (val == 5) {
      Serial.println("Retroceder");
      retroceder_robot();
      estado_robot = 1;
    }
  }
  else {
    res = -1;
  }

  if (res) {
    return httpd_resp_send_500(req);
  }

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_send(req, NULL, 0);
}

// ==========================================
// PÁGINA WEB (HTML + CSS + JS)
// ==========================================
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!doctype html>
<html lang="es">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
    <title>UCQ-Bot</title>
    <style>
        :root {
            --primary-color: #00d2ff;
            --secondary-color: #3a7bd5;
            --bg-color: #121212;
            --glass-bg: rgba(255, 255, 255, 0.05);
            --glass-border: rgba(255, 255, 255, 0.1);
            --text-color: #ffffff;
        }

        * { box-sizing: border-box; }

        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: var(--bg-color);
            background: linear-gradient(135deg, #121212 0%, #1a1a1a 100%);
            color: var(--text-color);
            margin: 0;
            padding: 0;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            height: 100vh; /* Ocupa exactamente el alto de la pantalla */
            overflow: hidden; /* Evita scroll */
            touch-action: none; /* Previene gestos del navegador */
        }

        /* Contenedor Principal Compacto */
        .main-container {
            background: var(--glass-bg);
            backdrop-filter: blur(10px);
            -webkit-backdrop-filter: blur(10px);
            border: 1px solid var(--glass-border);
            border-radius: 15px;
            padding: 10px;
            box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.37);
            width: 95%;
            max-width: 400px; /* Ancho máximo para móviles */
            max-height: 98vh;
            display: flex;
            flex-direction: column;
            align-items: center;
        }

        h1 {
            margin: 5px 0;
            font-size: 1.5rem;
            font-weight: 600;
            text-transform: uppercase;
            background: linear-gradient(to right, var(--primary-color), var(--secondary-color));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }

        /* Video Stream Flexible */
        .video-container {
            width: 100%;
            flex-grow: 1; /* Ocupa el espacio disponible */
            border-radius: 10px;
            overflow: hidden;
            background: #000;
            position: relative;
            display: flex;
            align-items: center;
            justify-content: center;
            margin-bottom: 10px;
            min-height: 150px;
        }

        img#stream {
            max-width: 100%;
            max-height: 100%;
            object-fit: contain;
            transform: rotate(0deg); 
        }

        .top-controls {
            display: flex;
            gap: 10px;
            margin-bottom: 10px;
            width: 100%;
            justify-content: center;
        }

        /* Botones de Control */
        .controls-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 8px;
            margin-bottom: 10px;
            width: 100%;
            max-width: 280px; /* Limita el ancho de los botones */
        }

        .btn {
            background: rgba(255, 255, 255, 0.1);
            border: 1px solid var(--glass-border);
            border-radius: 10px;
            color: white;
            padding: 15px 0; /* Padding vertical */
            font-size: 20px;
            cursor: pointer;
            transition: all 0.2s ease;
            display: flex;
            align-items: center;
            justify-content: center;
            user-select: none;
            -webkit-user-select: none;
            -webkit-tap-highlight-color: transparent;
        }

        .btn:active, .btn.active {
            background: var(--primary-color);
            transform: scale(0.95);
        }

        .btn-action {
            font-size: 12px;
            padding: 8px 15px;
            border-radius: 8px;
            background: linear-gradient(45deg, var(--secondary-color), var(--primary-color));
            font-weight: bold;
            border: none;
            color: white;
            cursor: pointer;
        }
        
        .btn-toggle {
            background: rgba(255,255,255,0.1);
            border: 1px solid var(--glass-border);
        }
        
        .btn-toggle.on {
            background: #ffeb3b;
            color: #000;
            box-shadow: 0 0 10px #ffeb3b;
        }

        /* Sliders Compactos */
        .slider-container {
            width: 100%;
            margin: 5px 0;
            display: flex;
            align-items: center;
            justify-content: space-between;
            font-size: 12px;
        }

        .slider-label {
            margin-right: 10px;
            min-width: 60px;
        }

        input[type="range"] {
            -webkit-appearance: none;
            flex-grow: 1;
            height: 4px;
            background: rgba(255,255,255,0.2);
            border-radius: 2px;
            outline: none;
        }

        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            width: 16px;
            height: 16px;
            border-radius: 50%;
            background: var(--primary-color);
            cursor: pointer;
        }

    </style>
</head>
<body>

    <div class="main-container">
        <h1>UCQ-Bot</h1>
        
        <div class="video-container">
            <img id="stream" src="">
            <div style="position:absolute; color:#555; z-index:-1;">Cámara Off</div>
        </div>

        <div class="top-controls">
            <button id="toggle-stream" class="btn-action">Iniciar Cámara</button>
            <button id="toggle-flash" class="btn-action btn-toggle">Flash</button>
        </div>

        <!-- Controles de Movimiento -->
        <div class="controls-grid">
            <div></div>
            <button class="btn" id="btn-fwd" data-val="1">▲</button>
            <div></div>
            
            <button class="btn" id="btn-left" data-val="2">◄</button>
            <div></div>
            <button class="btn" id="btn-right" data-val="4">►</button>
            
            <div></div>
            <button class="btn" id="btn-back" data-val="5">▼</button>
            <div></div>
        </div>

        <!-- Controles Extra -->
        <div class="slider-container">
            <span class="slider-label">Velocidad</span>
            <input type="range" id="speed" min="0" max="255" value="200">
        </div>

    </div>

    <script>
        document.addEventListener('DOMContentLoaded', function() {
            const basePath = document.location.origin;
            const streamImg = document.getElementById('stream');
            const toggleStreamBtn = document.getElementById('toggle-stream');
            const toggleFlashBtn = document.getElementById('toggle-flash');
            
            let isStreaming = false;
            let isFlashOn = false;
            let moveInterval = null; // Para repetir el comando

            // Función para enviar comandos al ESP32
            function sendCommand(variable, value) {
                fetch(`${basePath}/control?var=${variable}&val=${value}`)
                    .catch(error => console.error('Error:', error));
            }

            // Control de la Cámara
            toggleStreamBtn.onclick = () => {
                if (isStreaming) {
                    window.stop();
                    toggleStreamBtn.innerHTML = 'Iniciar Cámara';
                    toggleStreamBtn.style.background = '';
                    isStreaming = false;
                } else {
                    // Agregamos un timestamp para forzar al navegador a recargar el stream
                    streamImg.src = `${basePath}:81/stream?t=${Date.now()}`;
                    toggleStreamBtn.innerHTML = 'Detener';
                    toggleStreamBtn.style.background = '#ff416c';
                    isStreaming = true;
                }
            };
            
            // Control del Flash (Botón Toggle)
            toggleFlashBtn.onclick = () => {
                isFlashOn = !isFlashOn;
                if (isFlashOn) {
                    toggleFlashBtn.classList.add('on');
                    sendCommand('flash', 255); // Encender al máximo
                } else {
                    toggleFlashBtn.classList.remove('on');
                    sendCommand('flash', 0); // Apagar
                }
            };

            // Sliders
            const speedSlider = document.getElementById('speed');
            speedSlider.onchange = () => sendCommand('speed', speedSlider.value);

            // Lógica de "Mantener para mover" (Hold to move) con repetición
            const moveButtons = document.querySelectorAll('.controls-grid .btn[data-val]');
            
            moveButtons.forEach(btn => {
                const val = btn.getAttribute('data-val');
                
                const startMove = (e) => {
                    if(e.cancelable) e.preventDefault(); // Evitar scroll/zoom
                    if (moveInterval) return; // Ya se está moviendo

                    btn.classList.add('active');
                    sendCommand('car', val); // Primer comando inmediato
                    
                    // Repetir comando cada 200ms para evitar que el robot se detenga
                    moveInterval = setInterval(() => {
                        sendCommand('car', val);
                    }, 200);
                };

                const stopMove = (e) => {
                    if(e.cancelable) e.preventDefault();
                    btn.classList.remove('active');
                    
                    if (moveInterval) {
                        clearInterval(moveInterval);
                        moveInterval = null;
                    }
                    sendCommand('car', 3); // Enviar STOP
                };

                // Eventos Mouse (PC)
                btn.addEventListener('mousedown', startMove);
                btn.addEventListener('mouseup', stopMove);
                btn.addEventListener('mouseleave', stopMove);

                // Eventos Touch (Móvil)
                btn.addEventListener('touchstart', startMove, {passive: false});
                btn.addEventListener('touchend', stopMove, {passive: false});
            });
        });
    </script>
</body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, (const char *)INDEX_HTML, strlen(INDEX_HTML));
}

void startCameraServer()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t cmd_uri = {
        .uri       = "/control",
        .method    = HTTP_GET,
        .handler   = cmd_handler,
        .user_ctx  = NULL
    };

   httpd_uri_t stream_uri = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };
    
    Serial.printf("Iniciando servidor web en puerto: '%d'\n", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &cmd_uri);
    }

    config.server_port += 1;
    config.ctrl_port += 1;
    Serial.printf("Iniciando servidor de stream en puerto: '%d'\n", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}

unsigned int obtener_velocidad(unsigned int sp)
{
  return map(sp, 0, 100, 0, 255);
}

// ==========================================
// FUNCIONES DE CONTROL DEL ROBOT
// ==========================================

void configurar_robot()
{
    // Configurar pines de PWM para los motores
    // Usamos canales 3, 4, 5, 6 para controlar los 4 pines del driver L298N
    ledcSetup(3, 2000, 8); // Canal 3, 2000Hz, 8 bits
    ledcSetup(4, 2000, 8); 
    ledcSetup(5, 2000, 8); 
    ledcSetup(6, 2000, 8); 

    // Asignar pines a los canales PWM
    ledcAttachPin(DERECHA_M0, 3);
    ledcAttachPin(DERECHA_M1, 4);
    ledcAttachPin(IZQUIERDA_M0, 5);
    ledcAttachPin(IZQUIERDA_M1, 6);
    
    pinMode(33, OUTPUT); // LED de estado (rojo en placa)
    
    parar_robot();
}

void parar_robot()
{
  // Poner todos los pines a 0 detiene los motores
  ledcWrite(3, 0);
  ledcWrite(4, 0);
  ledcWrite(5, 0);
  ledcWrite(6, 0);
}

void avanzar_robot()
{
  // Para avanzar:
  // Motor Derecho: M0=0, M1=Velocidad
  // Motor Izquierdo: M0=0, M1=Velocidad
  ledcWrite(3, 0);
  ledcWrite(4, velocidad);
  ledcWrite(5, 0);
  ledcWrite(6, velocidad);
  
  intervalo_movimiento = 350; // Tiempo de seguridad
  tiempo_anterior = millis();  
}

void retroceder_robot()
{
  // Para retroceder:
  // Motor Derecho: M0=Velocidad, M1=0
  // Motor Izquierdo: M0=Velocidad, M1=0
  ledcWrite(3, velocidad);
  ledcWrite(4, 0);
  ledcWrite(5, velocidad);
  ledcWrite(6, 0);
  
  intervalo_movimiento = 350;
  tiempo_anterior = millis();  
}

void girar_derecha()
{
  // INTERCAMBIADO: Ahora esta función usa la lógica que antes era "Izquierda"
  // Para girar a la derecha (sobre su eje):
  // Motor Derecho: Avanza (M0=0, M1=Velocidad)
  // Motor Izquierdo: Retrocede (M0=Velocidad, M1=0)
  ledcWrite(3, 0);
  ledcWrite(4, velocidad);
  ledcWrite(5, velocidad);
  ledcWrite(6, 0);
  
  intervalo_movimiento = 100;
  tiempo_anterior = millis();
}

void girar_izquierda()
{
  // INTERCAMBIADO: Ahora esta función usa la lógica que antes era "Derecha"
  // Para girar a la izquierda (sobre su eje):
  // Motor Derecho: Retrocede (M0=Velocidad, M1=0)
  // Motor Izquierdo: Avanza (M0=0, M1=Velocidad)
  ledcWrite(3, velocidad); 
  ledcWrite(4, 0);
  ledcWrite(5, 0);
  ledcWrite(6, velocidad);

  intervalo_movimiento = 100;
  tiempo_anterior = millis();
}
