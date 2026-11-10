
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>

// Configuracion de pines

// Pines del sensor ultrasonico HC-SR04
#define Trig 14
#define Echo 12

// Pines de los LEDs indicadores
#define Led_Bajo 16
#define Led_Medio 4
#define Led_Alto 2
#define Led_Alerta 5

// Configuracion de Wi-Fi

#define WIFI_SSID "Red"
#define WIFI_PASSWORD "contraseña"

HTTPClient httpClient;
WiFiClient wClient;

// Conexion IP

// Direccion IP de la computadora donde se ejecuta la API

// Consultar ultimas 5 lecturas
String URL1 = "http://IP:3000/api/readings?limit=5";

// Enviar un nuevo registro
String URL2 = "http://IP:3000/api/readings";

// Token del dispositivo
// Debe coincidir con DEVICE_TOKEN del archivo .env
String DEVICE_TOKEN = "Codigo privado";

String deviceID = "ESP8266_01";

// Caracteristicas del contenedor

// Altura fisica del contenedor
float alturaContenedor = 11.0;

// Distancia del sensor al agua cuando esta lleno
// Calibrar fisicamente
float distanciaLleno = 1.5;

// Variables de monitoreo del agua
float distancia = 0;
float nivelAgua = 0;
float porcentaje = 0;

float nivelAnterior = -1;
float velocidad = 0;

float DxFuga = 1.0;

bool posibleFuga = false;

// Contador de los datos para la deteccion de fugas
int contadorFuga = 0;

bool lecturaValida = false;

String estadoAgua = "";

unsigned long tiempoAnterior = 0;

unsigned long tiempoLectura = 0;
unsigned long tiempoEnvio = 0;

unsigned long intervaloLectura = 1000;
unsigned long intervaloEnvio = 30000;


// Establecer conexion Wi-Fi
void conectarWiFi() {

  WiFi.mode(WIFI_STA);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Conectandose a red WiFi");

  unsigned long inicio = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - inicio < 20000) {

    delay(500);

    Serial.print(".");

  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println("\nConectado!");

    Serial.print("IP ESP8266: ");

    Serial.println(WiFi.localIP());

  }

  else {

    Serial.println("\nNo se pudo conectar al WiFi.");

  }

}


// Verificar si se establecio conexion Wi-Fi
void verificarWiFi() {

  static unsigned long ultimoIntento = 0;

  if (WiFi.status() != WL_CONNECTED) {

    if (millis() - ultimoIntento >= 10000) {

      ultimoIntento = millis();

      Serial.println("Intentando reconectar WiFi...");

      WiFi.disconnect();

      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    }

  }

}


// Configuracion del sensor ultrasonico
float leerDistancia() {

  digitalWrite(Trig, LOW);

  delayMicroseconds(2);

  digitalWrite(Trig, HIGH);

  delayMicroseconds(10);

  digitalWrite(Trig, LOW);

  long duracion = pulseIn(Echo, HIGH, 30000);

  if (duracion == 0) {

    return -1;

  }

  float distancia = duracion * 0.0343 / 2.0;

  return distancia;

}


// Calcular el nivel del agua actual del contenedor
void calcularNivel() {

  nivelAgua = alturaContenedor -
              (distancia - distanciaLleno);

  nivelAgua = constrain(nivelAgua, 0, alturaContenedor);

  porcentaje = (nivelAgua / alturaContenedor) * 100.0;

}


// Niveles de llenado del contenedor
void clasificarNivel() {

  if (porcentaje < 33.33) {

    estadoAgua = "bajo";

  }

  else if (porcentaje < 66.67) {

    estadoAgua = "medio";

  }

  else {

    estadoAgua = "alto";

  }

}


// Calculo de las velocidades y posible fuga
void calcularVelocidad() {

  unsigned long tiempoActual = millis();

  // Primera medicion: establecer referencia
  if (nivelAnterior < 0) {

    nivelAnterior = nivelAgua;

    tiempoAnterior = tiempoActual;

    velocidad = 0;

    posibleFuga = false;

    contadorFuga = 0;

    return;

  }

  // Calcular velocidad cada 30 segundos
  if (tiempoActual - tiempoAnterior >= 30000) {

    float minutos =
      (tiempoActual - tiempoAnterior) / 60000.0;

    if (minutos > 0) {

      velocidad =
        (nivelAnterior - nivelAgua) / minutos;

    }

    // Actualizar referencia
    nivelAnterior = nivelAgua;

    tiempoAnterior = tiempoActual;

    // Detectar posible fuga
    if (velocidad > DxFuga) {

      contadorFuga++;

    }

    else {

      contadorFuga = 0;

    }

    // Activar alerta despues de dos detecciones consecutivas
    posibleFuga = contadorFuga >= 2;

  }

}


// Control de actuadores (LEDs)
void controlarLEDs() {

  digitalWrite(Led_Bajo, LOW);

  digitalWrite(Led_Medio, LOW);

  digitalWrite(Led_Alto, LOW);

  if (estadoAgua == "bajo") {

    digitalWrite(Led_Bajo, HIGH);

  }

  else if (estadoAgua == "medio") {

    digitalWrite(Led_Medio, HIGH);

  }

  else if (estadoAgua == "alto") {

    digitalWrite(Led_Alto, HIGH);

  }

  digitalWrite(Led_Alerta, posibleFuga ? HIGH : LOW);

}


// Imprimir los datos de cada medicion
void mostrarDatos() {

  Serial.println("\n---------------------");

  Serial.print("Distancia: ");

  Serial.print(distancia);

  Serial.println(" cm");

  Serial.print("Nivel de agua: ");

  Serial.print(nivelAgua);

  Serial.println(" cm");

  Serial.print("Porcentaje: ");

  Serial.print(porcentaje);

  Serial.println(" %");

  Serial.print("Estado: ");

  Serial.println(estadoAgua);

  Serial.print("Velocidad: ");

  Serial.print(velocidad);

  Serial.println(" cm/min");

  Serial.print("Contador de fuga: ");

  Serial.println(contadorFuga);

  Serial.print("Posible fuga: ");

  Serial.println(posibleFuga ? "SI" : "NO");

  Serial.println("---------------------");

}


// Metodo GET para consultar el historial de mediciones
void GET(String deviceID) {

  Serial.println("Consultando datos historicos...");

  if (WiFi.status() == WL_CONNECTED) {

    httpClient.begin(wClient, URL1.c_str());

    int httpResponseCode = httpClient.GET();

    Serial.print("Respuesta GET: ");

    Serial.println(httpResponseCode);

    if (httpResponseCode > 0) {

      Serial.println(httpClient.getString());

    }

    else {

      Serial.println("Error en la consulta GET");

    }

    httpClient.end();

  }

  return;

}


// Metodo POST para enviar los registros de datos
void POST(
  String deviceID,
  float distancia,
  float nivel,
  float porcentaje,
  float velocidad,
  String estado,
  bool fuga
) {

  // Construir mensaje en formato JSON
  JsonDocument doc;

  // Agregar las mediciones al documento
  doc["device_id"] = deviceID;
  doc["distance_cm"] = distancia;
  doc["water_level_cm"] = nivel;
  doc["level_percent"] = porcentaje;
  doc["level_change_rate_cm_min"] = velocidad;
  doc["water_status"] = estado;
  doc["possible_leak"] = fuga;

  // Convertir el documento JSON a texto
  String data;

  serializeJson(doc, data);

  Serial.println("Enviando medicion:");

  Serial.println(data);

  if (WiFi.status() == WL_CONNECTED) {

    httpClient.begin(wClient, URL2.c_str());

    httpClient.addHeader("Content-Type", "application/json");

    httpClient.addHeader("x-device-token", DEVICE_TOKEN);

    int httpResponseCode = httpClient.POST(data);

    Serial.print("Respuesta POST: ");

    Serial.println(httpResponseCode);

    if (httpResponseCode > 0) {

      Serial.println(httpClient.getString());

    }

    else {

      Serial.println("Error al enviar la medicion");

    }

    httpClient.end();

  }

  else {

    Serial.println("No hay conexion WiFi. No se envio la medicion.");

  }

  return;

}


// Configuracion inicial del sensor y LEDs
void setup() {

  Serial.begin(115200);

  // Configuracion del sensor
  pinMode(Trig, OUTPUT);

  pinMode(Echo, INPUT);

  digitalWrite(Trig, LOW);

  // Configuracion de LEDs
  pinMode(Led_Bajo, OUTPUT);

  pinMode(Led_Medio, OUTPUT);

  pinMode(Led_Alto, OUTPUT);

  pinMode(Led_Alerta, OUTPUT);

  digitalWrite(Led_Bajo, LOW);

  digitalWrite(Led_Medio, LOW);

  digitalWrite(Led_Alto, LOW);

  digitalWrite(Led_Alerta, LOW);

  // Conectar con Wi-Fi
  conectarWiFi();

}


// Ejecucion del programa
void loop() {

  unsigned long tiempoActual = millis();

  // Verificar conexion Wi-Fi
  verificarWiFi();


  // Mediciones cada segundo
  if (tiempoActual - tiempoLectura >= intervaloLectura) {

    tiempoLectura = tiempoActual;

    distancia = leerDistancia();

    lecturaValida = (distancia >= 0);

    if (lecturaValida) {

      // Calcular nivel y porcentaje
      calcularNivel();

      // Clasificar nivel
      clasificarNivel();

      // Calcular velocidad y posible fuga
      calcularVelocidad();

      // Controlar LEDs
      controlarLEDs();

      // Mostrar datos
      mostrarDatos();

    }

    else {

      Serial.println("Error obteniendo datos del HC-SR04");

    }

  }


  // Mandar datos cada 30 segundos
  if (tiempoActual - tiempoEnvio >= intervaloEnvio) {

    tiempoEnvio = tiempoActual;

    if (lecturaValida && distancia >= 0) {

      // Enviar medicion mediante API
      POST(
        deviceID,
        distancia,
        nivelAgua,
        porcentaje,
        velocidad,
        estadoAgua,
        posibleFuga
      );

    }

    else {

      Serial.println("No se envia: lectura del sensor no valida.");

    }

  }

}
