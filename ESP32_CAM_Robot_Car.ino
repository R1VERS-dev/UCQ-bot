/*
  ESP32_CAM_Robot_Car
  Archivo Principal (.ino)
  
  Este archivo configura el ESP32, conecta al WiFi (modo Punto de Acceso),
  inicializa la cámara y configura el bucle principal para detener el robot automáticamente.
*/

#include "esp_wifi.h"
#include "esp_camera.h"
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// Credenciales del Punto de Acceso (AP)

const char* ssid_ap = "UCQ-Bot";
const char* password_ap = "12345678";

// Variables externas definidas en app_httpd.cpp
extern volatile unsigned int  velocidad_motor;
extern void parar_robot();
extern void configurar_robot();
extern uint8_t estado_robot;
extern volatile unsigned long tiempo_anterior;        
extern volatile unsigned long intervalo_movimiento; 

// Definición de pines para el modelo AI THINKER
#define CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

void startCameraServer();

void setup() 
{
  // Deshabilitar detector de brownout (caída de tensión) para evitar reinicios
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  // Configuración de la Cámara
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // Ajustar buffers según si hay PSRAM disponible
  if(psramFound()){
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Inicializar cámara
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Error al iniciar la cámara: 0x%x", err);
    return;
  }

  // Ajustes iniciales del sensor
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QVGA);
  s->set_vflip(s, 1);   // Voltear verticalmente (ajustar si la imagen sale al revés)
  s->set_hmirror(s, 1); // Espejo horizontal

  // Iniciar Punto de Acceso WiFi
  WiFi.softAP(ssid_ap, password_ap);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("Dirección IP del AP: ");
  Serial.println(myIP);
  
  // Iniciar servidor web
  startCameraServer();

  // Configurar LED Flash (Pin 4)
  ledcSetup(7, 5000, 8);
  ledcAttachPin(4, 7);  
  
  // Configurar pines del robot
  configurar_robot();
  
  // Parpadeo inicial del LED para indicar que está listo
  for (int i=0;i<5;i++) 
  {
    ledcWrite(7,10); 
    delay(50);
    ledcWrite(7,0);
    delay(50);    
  }
  digitalWrite(33,LOW); // Apagar LED rojo interno
      
  tiempo_anterior = millis();
}

void loop() {
  // Bucle principal: Verifica si debe detener el robot automáticamente
  if(estado_robot)
  {
    unsigned long tiempoActual = millis();
    if (tiempoActual - tiempo_anterior >= intervalo_movimiento) {
      tiempo_anterior = tiempoActual;
      parar_robot();
      Serial.println("Parada Automática");
      estado_robot = 0;
    }
  }
  delay(1);
  yield();
}
