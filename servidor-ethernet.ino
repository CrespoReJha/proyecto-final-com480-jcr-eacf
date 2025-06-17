#include <SPI.h>
#include <Ethernet.h>
#include <AccelStepper.h>

// === CONFIGURACIÓN ETHERNET ===
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 0, 40);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
EthernetServer server(80);

// === PINES ===
#define STEP_PIN 3
#define DIR_PIN 2
#define SENSOR_VIBRACION_PIN 8  // KY-02
#define BUZZER_PIN 5           // Buzzer
#define LED_VERDE_PIN 4        // LED verde 
#define LED_ROJO_PIN 6         // LED rojo

// === MOTOR PASO A PASO ===
const long pasosApertura = 1500; // Pasos totales para apertura completa
AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// === VARIABLES DE ESTADO ===
long posicionActual = 0;
bool motorMoviendose = false;
bool obstruccionDetectada = false;
unsigned long tiempoObstruccion = 0;
const unsigned long TIEMPO_BLOQUEO = 3000; // 3 segundos de bloqueo
String comandoPendiente = "";
String estadoSistema = "LISTO";

// === CONTROL BUZZER ===
bool buzzerActivo = false;
int buzzerCiclos = 0;
unsigned long buzzerTiempo = 0;
const unsigned long BUZZER_INTERVALO = 150; // 150ms on/off

void setup() {
  Serial.begin(9600);
  
  // Configurar pines
  pinMode(SENSOR_VIBRACION_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_VERDE_PIN, OUTPUT);
  pinMode(LED_ROJO_PIN, OUTPUT);
  
  // Estado inicial
  digitalWrite(LED_VERDE_PIN, HIGH);
  digitalWrite(LED_ROJO_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Configuración del motor
  stepper.setMaxSpeed(400);
  stepper.setAcceleration(600);
  stepper.setCurrentPosition(0);
  posicionActual = 0;
  
  // Iniciar Ethernet
  Ethernet.begin(mac, ip, gateway, gateway, subnet);
  server.begin();
  
  Serial.println(F("=== SISTEMA EKRAN API INICIADO ==="));
  Serial.print(F("IP: "));
  Serial.println(Ethernet.localIP());
  
  // Test inicial de LEDs
  digitalWrite(LED_ROJO_PIN, HIGH);
  delay(500);
  digitalWrite(LED_ROJO_PIN, LOW);
  digitalWrite(LED_VERDE_PIN, HIGH);
  Serial.println(F("Sistema listo"));
}

void loop() {
  // Prioridad 1: Mover motor
  moverMotor();
  
  // Prioridad 2: Sensor de vibración y buzzer
  verificarVibracion();
  
  // Prioridad 3: Procesar comandos pendientes
  procesarComandoPendiente();
  
  // Prioridad 4: Ethernet (solo si motor no está en movimiento)
  if (!motorMoviendose) {
    atenderEthernet();
  }
  
  // Mantener conexión Ethernet
  Ethernet.maintain();
}

void verificarVibracion() {
  // Manejar buzzer no bloqueante
  if (buzzerActivo) {
    if (millis() - buzzerTiempo >= BUZZER_INTERVALO) {
      if (digitalRead(BUZZER_PIN) == HIGH) {
        digitalWrite(BUZZER_PIN, LOW);
      } else {
        digitalWrite(BUZZER_PIN, HIGH);
        buzzerCiclos++;
      }
      buzzerTiempo = millis();
      if (buzzerCiclos >= 6) { // 3 ciclos completos (on/off)
        digitalWrite(BUZZER_PIN, LOW);
        buzzerActivo = false;
        buzzerCiclos = 0;
      }
    }
  }

  // Verificar sensor
  bool sensorVibracion = digitalRead(SENSOR_VIBRACION_PIN);
  
  if (!sensorVibracion && motorMoviendose && !obstruccionDetectada) {
    stepper.stop();
    posicionActual = stepper.currentPosition();
    stepper.setCurrentPosition(posicionActual);
    motorMoviendose = false;
    obstruccionDetectada = true;
    tiempoObstruccion = millis();
    
    digitalWrite(LED_VERDE_PIN, LOW);
    digitalWrite(LED_ROJO_PIN, HIGH);
    
    // Iniciar buzzer
    buzzerActivo = true;
    buzzerCiclos = 0;
    buzzerTiempo = millis();
    digitalWrite(BUZZER_PIN, HIGH);
    
    estadoSistema = "BLOQUEADO 3s - PASO " + String(posicionActual);
    Serial.println(F("*** OBSTRUCCION DETECTADA ***"));
    Serial.println(posicionActual);
  }
  
  if (obstruccionDetectada && millis() - tiempoObstruccion >= TIEMPO_BLOQUEO) {
    obstruccionDetectada = false;
    digitalWrite(LED_ROJO_PIN, LOW);
    digitalWrite(LED_VERDE_PIN, HIGH);
    estadoSistema = "LISTO - PASO " + String(posicionActual);
    Serial.println(F("*** AUTO-RESET COMPLETADO ***"));
  }
}

void moverMotor() {
  if (motorMoviendose && !obstruccionDetectada) {
    stepper.run();
    
    if (stepper.distanceToGo() == 0) {
      motorMoviendose = false;
      posicionActual = stepper.currentPosition();
      
      if (posicionActual <= 50) {
        estadoSistema = "CERRADO (pos: " + String(posicionActual) + ")";
      } else if (posicionActual >= pasosApertura - 50) {
        estadoSistema = "ABIERTO (pos: " + String(posicionActual) + ")";
      } else {
        estadoSistema = "PARCIAL (pos: " + String(posicionActual) + ")";
      }
      
      digitalWrite(LED_VERDE_PIN, HIGH);
      digitalWrite(LED_ROJO_PIN, LOW);
      Serial.print(F("Movimiento completado: "));
      Serial.println(posicionActual);
    }
  }
}

void procesarComandoPendiente() {
  if (comandoPendiente != "" && !motorMoviendose && !obstruccionDetectada) {
    if (comandoPendiente == "ABRIR") {
      long pasosRestantes = pasosApertura - posicionActual;
      if (pasosRestantes > 0) {
        stepper.setCurrentPosition(posicionActual);
        stepper.moveTo(pasosApertura);
        motorMoviendose = true;
        estadoSistema = "ABRIENDO " + String(pasosRestantes) + " pasos...";
        digitalWrite(LED_VERDE_PIN, HIGH);
        digitalWrite(LED_ROJO_PIN, LOW);
        Serial.print(F("ABRIENDO: "));
        Serial.println(pasosRestantes);
      } else {
        estadoSistema = "YA ESTA ABIERTO";
        Serial.println(F("Ya está abierto"));
      }
    } else if (comandoPendiente == "CERRAR") {
      if (posicionActual > 0) {
        stepper.setCurrentPosition(posicionActual);
        stepper.moveTo(0);
        motorMoviendose = true;
        estadoSistema = "CERRANDO " + String(posicionActual) + " pasos...";
        digitalWrite(LED_VERDE_PIN, HIGH);
        digitalWrite(LED_ROJO_PIN, LOW);
        Serial.print(F("CERRANDO: "));
        Serial.println(posicionActual);
      } else {
        estadoSistema = "YA ESTA CERRADO";
        Serial.println(F("Ya está cerrado"));
      }
    } else if (comandoPendiente == "RESET") {
      obstruccionDetectada = false;
      digitalWrite(LED_ROJO_PIN, LOW);
      digitalWrite(LED_VERDE_PIN, HIGH);
      digitalWrite(BUZZER_PIN, LOW);
      stepper.setCurrentPosition(posicionActual);
      estadoSistema = "RESET MANUAL - PASO " + String(posicionActual);
      Serial.println(F("RESET MANUAL"));
    }
    comandoPendiente = "";
  }
}

void atenderEthernet() {
  EthernetClient client = server.available();
  if (client) {
    String peticion = "";
    unsigned long startTime = millis();
    
    // Leer petición rápidamente (timeout 50ms)
    while (client.connected() && client.available() && millis() - startTime < 50) {
      char c = client.read();
      peticion += c;
      if (peticion.endsWith("\r\n\r\n")) break;
    }
    
    Serial.println(F("Petición recibida"));
    
    // Procesar comandos
    if (peticion.indexOf("GET /status") != -1) {
      enviarEstado(client);
    } else if (peticion.indexOf("GET /abrir") != -1) {
      comandoPendiente = "ABRIR";
      enviarRespuestaComando(client, "Comando ABRIR recibido");
    } else if (peticion.indexOf("GET /cerrar") != -1) {
      comandoPendiente = "CERRAR";
      enviarRespuestaComando(client, "Comando CERRAR recibido");
    } else if (peticion.indexOf("GET /reset") != -1) {
      comandoPendiente = "RESET";
      enviarRespuestaComando(client, "Comando RESET recibido");
    } else {
      enviarError(client, "Ruta no encontrada");
    }
    
    client.stop();
    Serial.println(F("Cliente desconectado"));
  }
}

void enviarEstado(EthernetClient &client) {
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: application/json"));
  client.println(F("Access-Control-Allow-Origin: *"));
  client.println(F("Connection: close"));
  client.println();
  
  client.print(F("{\"e\":\""));
  client.print(estadoSistema);
  client.print(F("\",\"p\":"));
  client.print(posicionActual);
  client.print(F(",\"t\":"));
  client.print(pasosApertura);
  client.print(F(",\"c\":"));
  client.print((posicionActual * 100) / pasosApertura);
  client.print(F(",\"s\":\""));
  client.print(digitalRead(SENSOR_VIBRACION_PIN) ? F("NORMAL") : F("VIBRACION"));
  client.print(F("\",\"m\":\""));
  client.print(motorMoviendose ? F("MOVIENDOSE") : F("DETENIDO"));
  client.print(F("\",\"v\":"));
  client.print(digitalRead(LED_VERDE_PIN) ? F("1") : F("0"));
  client.print(F(",\"r\":"));
  client.print(digitalRead(LED_ROJO_PIN) ? F("1") : F("0"));
  client.print(F(",\"o\":"));
  client.print(obstruccionDetectada ? F("1") : F("0"));
  client.print(F(",\"tr\":"));
  client.print(obstruccionDetectada ? (TIEMPO_BLOQUEO - (millis() - tiempoObstruccion)) / 1000 : 0);
  client.println(F("}"));
}

void enviarRespuestaComando(EthernetClient &client, String mensaje) {
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: application/json"));
  client.println(F("Access-Control-Allow-Origin: *"));
  client.println(F("Connection: close"));
  client.println();
  
  client.print(F("{\"m\":\""));
  client.print(mensaje);
  client.println(F("\"}"));
}

void enviarError(EthernetClient &client, String mensaje) {
  client.println(F("HTTP/1.1 404 Not Found"));
  client.println(F("Content-Type: application/json"));
  client.println(F("Access-Control-Allow-Origin: *"));
  client.println(F("Connection: close"));
  client.println();
  
  client.print(F("{\"e\":\""));
  client.print(mensaje);
  client.println(F("\"}"));
}

void serialEvent() {
  if (Serial.available()) {
    String comando = Serial.readString();
    comando.trim();
    
    if (comando == "abrir") comandoPendiente = "ABRIR";
    else if (comando == "cerrar") comandoPendiente = "CERRAR";
    else if (comando == "reset") comandoPendiente = "RESET";
    else if (comando == "test") {
      Serial.println(F("=== TEST ==="));
      Serial.print(F("Sensor: "));
      Serial.println(digitalRead(SENSOR_VIBRACION_PIN));
      Serial.print(F("LED Verde: "));
      Serial.println(digitalRead(LED_VERDE_PIN));
      Serial.print(F("LED Rojo: "));
      Serial.println(digitalRead(LED_ROJO_PIN));
      Serial.print(F("Posicion: "));
      Serial.println(stepper.currentPosition());
      Serial.print(F("Motor: "));
      Serial.println(motorMoviendose ? F("MOVIENDOSE") : F("DETENIDO"));
    }
  }
}