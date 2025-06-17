# Proyecto Final COM480
## Integrantes
1. Crespo Rejas Jhamil - Ing. Ciencias de la Computación
2. Calderón Flores Enrique Antonio - Ing. Ciencias de la Computación

# Software del Sistema de control de Ekran

## 1. Introducción al Sistema de Control

En esta sección se documenta el funcionamiento del software tanto a nivel del código del Arduino como de la interfaz web, detallando su lógica, organización y flujo de trabajo. La documentación está orientada a permitir que cualquier persona con conocimientos técnicos pueda entender, modificar o escalar el proyecto en el futuro.

A continuación se describen brevemente los dos componentes clave del sistema:

* **Servidor embebido en Arduino:**
La placa Arduino actúa como servidor HTTP que escucha peticiones desde la red local. Está encargado de ejecutar las acciones solicitadas (abrir o cerrar el motor), detectar posibles obstrucciones mediante un sensor de vibración, gestionar el buzzer y los LEDs, y mantener actualizada la información del estado general del sistema.

* **Interfaz gráfica web:**
Se trata de una aplicación HTML + CSS + JavaScript ligera, que permite al usuario visualizar el estado del sistema y enviar comandos de forma intuitiva. Esta interfaz se comunica con el Arduino a través de peticiones HTTP, interpreta las respuestas en formato JSON y actualiza dinámicamente su contenido para reflejar el estado del sistema.

![Texto alternativo](images/comunicacion-arduino-cliente-web.png)

## 2. Descripción General del Funcionamiento

El sistema inicia con la configuración del hardware y la inicialización de la red Ethernet. A partir de ese momento, entra en un ciclo de ejecución continuo que sigue una secuencia priorizada de tareas, permitiendo el control del motor paso a paso, la detección de obstrucciones mediante un sensor de vibración, y la atención a comandos enviados desde la interfaz web.

### Flujo de operación básico

1. **Inicio del sistema:**

   * Configuración de pines, motor, sensores y LEDs.
   * Inicialización de la conexión Ethernet.
   * Prueba de arranque con señales visuales.

2. **Ciclo continuo (`loop()`):**

   * **Prioridad 1:** Ejecutar movimiento del motor si hay una operación pendiente.
   * **Prioridad 2:** Verificar el estado del sensor de vibración en busca de obstrucciones.
   * **Prioridad 3:** Procesar comandos pendientes enviados por el usuario.
   * **Prioridad 4:** Atender solicitudes HTTP de la interfaz (solo si el motor no se está moviendo).

3. **Gestión de estados:**

   * Si se detecta una obstrucción, se detiene el motor, se activa el buzzer y se muestra el estado de bloqueo durante 3 segundos antes de reanudar la operación.
   * Si el motor completa su movimiento correctamente, se actualiza el estado del sistema y se apagan señales de advertencia.

### Subsistemas del sistema

* **Gestión del motor paso a paso:**
  Controlado por la librería `AccelStepper`, permite abrir y cerrar un mecanismo mediante comandos remotos o locales. Se realiza un seguimiento preciso de la posición.

* **Sensor de vibración:**
  Se utiliza para detectar obstrucciones o interferencias físicas. Si se detecta una vibración mientras el motor se mueve, el sistema se bloquea temporalmente por seguridad.

* **Sistema de bloqueo automático:**
  En caso de obstrucción, se activa el buzzer y los LEDs de advertencia. Luego de 3 segundos, el sistema se reinicia automáticamente si no se ha retirado manualmente.

* **Comunicación por Ethernet:**
  El Arduino actúa como servidor HTTP y responde a peticiones provenientes de la interfaz gráfica. Esto permite controlar el sistema sin necesidad de botones físicos.

* **API HTTP:**
  Proporciona endpoints sencillos:

  * `/abrir` → Inicia la apertura.
  * `/cerrar` → Inicia el cierre.
  * `/reset` → Restaura el sistema tras un bloqueo.
  * `/status` → Devuelve el estado actual en formato JSON.

## 3. Código del Arduino (Servidor embebido)

### 3.1 Configuración inicial (`setup()`)

La función `setup()` del código Arduino se ejecuta una sola vez al iniciar el sistema. Su propósito es realizar todas las configuraciones necesarias para que los distintos componentes del hardware y las comunicaciones comiencen en un estado operativo y seguro. En este proyecto, se configuran los pines de entrada/salida, el motor paso a paso, el servidor Ethernet, y se establece el estado inicial del sistema.

#### Configuración de pines

Los pines digitales se configuran para controlar los siguientes componentes:

| Componente          | Pin Digital | Modo de Configuración                      |
| ------------------- | ----------- | ------------------------------------------ |
| Motor - STEP        | 3           | Salida (usado por librería `AccelStepper`) |
| Motor - DIR         | 2           | Salida (usado por librería `AccelStepper`) |
| Sensor de vibración | 8           | Entrada con `INPUT_PULLUP`                 |
| Buzzer              | 5           | Salida                                     |
| LED Verde           | 4           | Salida                                     |
| LED Rojo            | 6           | Salida                                     |

La configuración `INPUT_PULLUP` para el sensor de vibración es esencial, ya que este tipo de sensor trabaja cerrando el circuito a tierra (GND) cuando detecta vibración. El pull-up interno evita lecturas inestables cuando el circuito está abierto.

#### Inicialización del motor paso a paso

Se usa la librería `AccelStepper` para controlar el motor con aceleración y velocidad definidas:

```cpp
stepper.setMaxSpeed(400);
stepper.setAcceleration(600);
stepper.setCurrentPosition(0);
```

Esto garantiza un movimiento suave y controlado, evitando saltos bruscos. Además, se define el número total de pasos necesarios para una apertura completa, permitiendo calcular posiciones relativas más adelante.

#### Estado inicial del sistema

Antes de iniciar el bucle principal, se establecen las siguientes condiciones:

* El LED verde se enciende, indicando que el sistema está listo.
* El LED rojo y el buzzer se apagan.
* Se muestra un mensaje de arranque en el monitor serial para facilitar la depuración.

#### Configuración de red y servidor HTTP

El Arduino se conecta a la red local utilizando una dirección IP fija, definida en el código:

```cpp
IPAddress ip(192, 168, 0, 40);
```

Esto permite saber con certeza la dirección desde la cual se puede acceder al servidor. El método `Ethernet.begin()` configura los parámetros de red, y `server.begin()` habilita el servidor HTTP que escuchará en el puerto 80.

Finalmente, se imprime en el monitor serial la dirección IP del dispositivo, confirmando que la conexión fue exitosa.

### Fragmento de código de referencia

```cpp
Ethernet.begin(mac, ip, gateway, gateway, subnet);
server.begin();

Serial.println(F("=== SISTEMA EKRAN API INICIADO ==="));
Serial.print(F("IP: "));
Serial.println(Ethernet.localIP());
```

### 3.2 Ciclo principal (`loop()`)

La función `loop()` en Arduino representa el núcleo operativo del sistema. Se ejecuta de forma continua e infinita mientras el dispositivo esté encendido. En este proyecto, el ciclo principal está cuidadosamente organizado por **prioridades**, lo que permite garantizar una respuesta eficiente tanto al control de hardware como a la comunicación con la interfaz web.

#### Orden de ejecución en el ciclo

El ciclo se estructura en cinco bloques de ejecución que se ejecutan secuencialmente, con un orden que refleja la criticidad de cada tarea:

1. **`moverMotor()`**
   Controla el desplazamiento del motor paso a paso si hay una orden de movimiento en curso.

2. **`verificarVibracion()`**
   Supervisa el estado del sensor de vibración para detectar obstrucciones y, si es necesario, interrumpe el movimiento.

3. **`procesarComandoPendiente()`**
   Evalúa si existe un comando almacenado (como “ABRIR” o “CERRAR”) y ejecuta la acción correspondiente.

4. **`atenderEthernet()`** *(condicional)*
   Atiende solicitudes HTTP desde la interfaz web. Esto se ejecuta **solo si el motor no está en movimiento**, para evitar conflictos de sincronización.

5. **`Ethernet.maintain()`**
   Mantiene activa la conexión de red DHCP (aunque en este caso se usa IP fija, sigue siendo buena práctica).

#### Lógica y razones del orden

* **Movimiento primero**:
  El motor necesita ser actualizado en cada ciclo a través de `stepper.run()` para que se desplace correctamente. Si no se ejecuta con prioridad, el motor no avanzaría como se espera.

* **Sensor de vibración inmediatamente después**:
  Es crítico detectar obstrucciones rápidamente mientras el motor se mueve. Este control evita daños físicos y activa las señales de advertencia.

* **Procesamiento de comandos en tercer lugar**:
  Esto permite almacenar un comando en espera y ejecutarlo solo cuando sea seguro (cuando el motor esté detenido y no haya obstrucción).

* **Atención a clientes HTTP como última prioridad operativa**:
  Esto evita interrupciones en tareas físicas sensibles como mover el motor o detectar vibraciones. Solo se permite cuando el sistema está estable.

#### Fragmento de código de referencia

```cpp
void loop() {
  moverMotor();                      // Prioridad 1
  verificarVibracion();             // Prioridad 2
  procesarComandoPendiente();       // Prioridad 3

  if (!motorMoviendose) {           // Prioridad 4 (condicional)
    atenderEthernet();
  }

  Ethernet.maintain();              // Mantenimiento de red
}
```
Excelente, avancemos con la **Sección 3.3: Lógica de control del motor paso a paso**. Esta sección está dedicada a explicar cómo se mueve el motor de forma precisa, cómo se interpretan los comandos de apertura/cierre, y cómo se actualiza el estado del sistema con base en la posición actual.


### 3.3 Lógica de control del motor

El movimiento del motor paso a paso es gestionado mediante la función `moverMotor()`, que se ejecuta en cada iteración del ciclo principal. Este subsistema es responsable de realizar los movimientos solicitados, controlar la posición actual y actualizar el estado del sistema tras cada desplazamiento.

#### Control del motor con AccelStepper

Se utiliza la librería `AccelStepper`, que permite controlar motores paso a paso con aceleración y velocidad definidas, evitando movimientos bruscos y asegurando precisión.

En la configuración inicial (`setup()`), ya se definió la velocidad máxima y la aceleración.

Durante la ejecución, la función `stepper.run()` debe llamarse continuamente para que el motor se desplace correctamente hasta su destino. Por eso, `moverMotor()` tiene la **más alta prioridad en el `loop()`**.

#### Estructura de la función `moverMotor()`

```cpp
void moverMotor() {
  if (motorMoviendose && !obstruccionDetectada) {
    stepper.run();
    
    if (stepper.distanceToGo() == 0) {
      motorMoviendose = false;
      posicionActual = stepper.currentPosition();
      // Actualizar estado del sistema
      ...
    }
  }
}
```

La función se activa únicamente si:

* Hay un comando en curso (`motorMoviendose == true`), y
* No hay obstrucción detectada.

Una vez que el motor llega a su destino (`distanceToGo() == 0`), el sistema detiene el movimiento y actualiza la posición actual y el estado textual (`estadoSistema`).

#### Clasificación del estado del sistema

Tras finalizar el movimiento, el sistema evalúa la posición del motor para asignar uno de los siguientes estados:

* **CERRADO:** posición cerca de 0 pasos.
* **ABIERTO:** posición cercana a `pasosApertura`.
* **PARCIAL:** cualquier posición intermedia.

Esto es útil tanto para mostrar información en la interfaz como para decidir si el siguiente comando es válido.

```cpp
if (posicionActual <= 50) {
  estadoSistema = "CERRADO";
} else if (posicionActual >= pasosApertura - 50) {
  estadoSistema = "ABIERTO";
} else {
  estadoSistema = "PARCIAL";
}
```

Además, se reestablece el LED verde y se apaga el LED rojo, indicando que el sistema vuelve a estar listo.

#### Variables importantes

| Variable          | Descripción                                      |
| ----------------- | ------------------------------------------------ |
| `pasosApertura`   | Total de pasos necesarios para apertura completa |
| `posicionActual`  | Posición actual del motor (pasos)                |
| `motorMoviendose` | Flag que indica si hay un movimiento en curso    |
| `estadoSistema`   | Texto informativo sobre el estado del motor      |

#### Ejemplo de ejecución completa

1. El usuario envía un comando `ABRIR`.
2. El sistema llama a `stepper.moveTo(pasosApertura)`.
3. En cada ciclo del `loop()`, se ejecuta `stepper.run()` hasta alcanzar el destino.
4. Al finalizar, se actualiza la posición y el estado.
5. El motor se detiene automáticamente y se activan los LEDs correspondientes.

### 3.4 Manejo de obstrucciones y control del buzzer

La función `verificarVibracion()` se encarga de supervisar el **sensor de vibración** en tiempo real y de activar el **buzzer** y los **LEDs de advertencia** en caso de detectar una obstrucción durante el movimiento del motor. Esta lógica de protección impide que el motor siga moviéndose cuando hay algún bloqueo físico, lo que puede evitar daños estructurales.

#### Funcionamiento general

La función tiene dos bloques principales:

  1. **Manejo del buzzer en modo no bloqueante** (funciona como una alarma intermitente mientras el sistema está bloqueado).
  2. **Verificación del sensor de vibración** y lógica de reacción ante una obstrucción.

#### 1. Manejo del buzzer

El buzzer se activa solo si el sistema detecta una obstrucción, y se apaga automáticamente después de sonar tres veces (6 cambios on/off, cada uno cada 150 ms).

**Referencia al codigo:**

```cpp
if (buzzerActivo) {
  if (millis() - buzzerTiempo >= BUZZER_INTERVALO) {
    // Cambia estado del buzzer
    ...
    if (buzzerCiclos >= 6) {
      digitalWrite(BUZZER_PIN, LOW);
      buzzerActivo = false;
    }
  }
}
```

Esta lógica **no bloquea** el flujo del `loop()`, lo que permite que el sistema siga funcionando mientras el buzzer está activo.

#### 2. Verificación del sensor de vibración

El sensor KY-02 está conectado con `INPUT_PULLUP`, por lo que su salida es `LOW` (falsa) cuando detecta vibración.

**Cuando se detecta una vibración mientras el motor se mueve:**

* Se detiene el motor inmediatamente.
* Se guarda la posición actual.
* Se activa el buzzer y el LED rojo.
* Se cambia el estado del sistema a **BLOQUEADO**.
* Se inicia un temporizador de 3 segundos.

```cpp
if (!sensorVibracion && motorMoviendose && !obstruccionDetectada) {
  stepper.stop();
  posicionActual = stepper.currentPosition();
  motorMoviendose = false;
  obstruccionDetectada = true;
  tiempoObstruccion = millis();
  ...
}
```

#### Restauración automática

Después de 3 segundos (`TIEMPO_BLOQUEO`), el sistema se reinicia automáticamente:

* Se apaga el LED rojo.
* Se enciende el LED verde.
* Se actualiza el estado a **LISTO**.
* Se permite nuevamente el movimiento del motor.

```cpp
if (obstruccionDetectada && millis() - tiempoObstruccion >= TIEMPO_BLOQUEO) {
  obstruccionDetectada = false;
  digitalWrite(LED_ROJO_PIN, LOW);
  digitalWrite(LED_VERDE_PIN, HIGH);
  estadoSistema = "LISTO - PASO " + String(posicionActual);
}
```

#### Variables importantes

| Variable               | Descripción                             |
| ---------------------- | --------------------------------------- |
| `buzzerActivo`         | Indica si el buzzer está activo         |
| `buzzerCiclos`         | Número de alternancias del buzzer       |
| `buzzerTiempo`         | Marca de tiempo para alternar el buzzer |
| `obstruccionDetectada` | Bandera que bloquea el sistema          |
| `tiempoObstruccion`    | Tiempo en que se detectó la obstrucción |

### 3.5 Comunicación por Ethernet y API HTTP

El sistema se comunica con la interfaz gráfica a través de peticiones HTTP dentro de una red local. Para ello, el Arduino se comporta como un **servidor HTTP embebido** gracias a la librería `Ethernet.h`. Esta arquitectura permite el control remoto del motor y la lectura del estado del sistema desde un navegador web.

#### Configuración del servidor

En el `setup()`, el servidor se inicializa con una dirección IP fija:

```cpp
Ethernet.begin(mac, ip, gateway, gateway, subnet);
server.begin();
```

Esto permite acceder al Arduino desde la interfaz web mediante una URL como:
`http://192.168.0.40/status` o `http://192.168.0.40/abrir`.

#### Lógica de atención a peticiones: `atenderEthernet()`

La función `atenderEthernet()` se ejecuta en cada iteración del `loop()`, **solo cuando el motor no está en movimiento**. Esta función:

1. Detecta si hay un cliente conectado.
2. Lee la petición HTTP recibida.
3. Identifica la ruta solicitada.
4. Ejecuta la acción correspondiente o responde con un error.

```cpp
if (peticion.indexOf("GET /abrir") != -1) {
  comandoPendiente = "ABRIR";
  enviarRespuestaComando(client, "Comando ABRIR recibido");
}
```

Después de procesar la petición, el cliente se desconecta con `client.stop()`.

#### Endpoints disponibles

La API HTTP del sistema incluye los siguientes **endpoints** accesibles mediante métodos `GET`:

| Endpoint  | Acción que realiza                                     |
| --------- | ------------------------------------------------------ |
| `/status` | Devuelve el estado actual del sistema en JSON          |
| `/abrir`  | Solicita apertura del sistema (motor a posición final) |
| `/cerrar` | Solicita cierre del sistema (motor a posición inicial) |
| `/reset`  | Restablece el sistema si estaba bloqueado              |

> **Nota:** Estos comandos no ejecutan la acción directamente. Solo almacenan el comando en `comandoPendiente`, que luego se procesa en `procesarComandoPendiente()` cuando el sistema esté listo.

#### Formato de respuesta: `/status`

El endpoint `/status` devuelve un objeto JSON con toda la información relevante del sistema:

```json
{
  "e": "LISTO",
  "p": 300,
  "t": 1500,
  "c": 20,
  "s": "NORMAL",
  "m": "DETENIDO",
  "v": "1",
  "r": "0",
  "o": "0",
  "tr": 0
}
```

| Campo | Descripción                                                         |
| ----- | ------------------------------------------------------------------- |
| `e`   | Estado textual del sistema (`LISTO`, `ABRIENDO`, `BLOQUEADO`, etc.) |
| `p`   | Posición actual del motor (en pasos)                                |
| `t`   | Pasos totales para apertura completa                                |
| `c`   | Porcentaje de apertura (0–100%)                                     |
| `s`   | Estado del sensor de vibración (`NORMAL` o `VIBRACION`)             |
| `m`   | Estado del motor (`MOVIENDOSE` o `DETENIDO`)                        |
| `v`   | Estado del LED verde (`1` encendido, `0` apagado)                   |
| `r`   | Estado del LED rojo (`1` encendido, `0` apagado)                    |
| `o`   | Si hay obstrucción detectada (`1` sí, `0` no)                       |
| `tr`  | Tiempo restante del bloqueo (en segundos)                           |

Esto permite a la interfaz actualizar su estado en tiempo real.

#### Funciones auxiliares

* `enviarEstado()`: Construye y envía el JSON de estado.
* `enviarRespuestaComando()`: Responde con un mensaje simple tras recibir un comando.
* `enviarError()`: Devuelve un error 404 si la ruta solicitada no existe.



### 3.6 Comandos por puerto serial(`serialEvent()`)

Además de los comandos recibidos por red, el sistema también permite **controlarlo manualmente** desde el monitor serial del entorno de desarrollo Arduino IDE. Esta funcionalidad es muy útil para realizar **pruebas rápidas**, **debuggear comportamientos del sistema** o **activar funciones sin necesidad de interfaz gráfica**.

#### Funcionamiento de `serialEvent()`

La función `serialEvent()` se ejecuta automáticamente cuando hay nuevos datos disponibles en el puerto serial. Su lógica es sencilla:

1. Lee el comando escrito por el usuario en el monitor serial.
2. Lo limpia (`trim()`) para eliminar espacios o saltos de línea.
3. Lo compara con comandos conocidos.
4. Ejecuta o encola el comando correspondiente.

#### Comandos disponibles por puerto serial

| Comando  | Acción                                                             |
| -------- | ------------------------------------------------------------------ |
| `abrir`  | Encola comando `ABRIR` (igual que si se enviara desde la interfaz) |
| `cerrar` | Encola comando `CERRAR`                                            |
| `reset`  | Encola comando `RESET`                                             |
| `test`   | Ejecuta una impresión completa del estado actual del sistema       |

#### Código de referencia

```cpp
void serialEvent() {
  if (Serial.available()) {
    String comando = Serial.readString();
    comando.trim();

    if (comando == "abrir") comandoPendiente = "ABRIR";
    else if (comando == "cerrar") comandoPendiente = "CERRAR";
    else if (comando == "reset") comandoPendiente = "RESET";
    else if (comando == "test") {
      Serial.println(F("=== TEST ==="));
      ...
    }
  }
}
```

#### Salida del comando `test`

El comando especial `test` imprime directamente:

* Estado del sensor de vibración.
* Estado de los LEDs.
* Posición actual del motor.
* Estado del motor (en movimiento o no).

Esto permite realizar una verificación rápida del hardware conectado sin necesidad de interfaz gráfica o conexión de red.

### Utilidad para desarrollo y mantenimiento

El modo debug por serial es ideal para:

* Verificar sensores antes de integrarlos al sistema.
* Calibrar la posición del motor sin arriesgar errores por interfaz.
* Confirmar que los LEDs y buzzer están conectados correctamente.
* Evaluar la estabilidad del sistema en condiciones no controladas.

## 4. Código de la Interfaz Web

La interfaz gráfica del sistema fue desarrollada en **HTML, CSS y JavaScript puro**, sin frameworks externos. Su propósito principal es ofrecer un control remoto simple, intuitivo y visualmente claro del sistema embebido, permitiendo:

* Enviar comandos de apertura, cierre y reset.
* Ver en tiempo real el estado del sistema.
* Recibir alertas de bloqueo por obstrucción.
* Monitorear el progreso del motor.

### 4.1 Estructura general del frontend

El HTML está organizado en las siguientes secciones:

* **Encabezado principal (`<h1>`)**: Título del panel.
* **Panel de estado (`.status-panel`)**: Muestra datos clave como posición, estado del motor, sensor, LEDs.
* **Botones de control (`.controls`)**: Permiten enviar comandos al servidor.
* **Log de eventos (`.log`)**: Muestra una bitácora de eventos importantes.
* **Indicador de conexión (`.connection-status`)**: Informa si hay comunicación activa con el Arduino.

### 4.2 Comunicación con el Arduino

La interfaz se conecta con el Arduino a través de **peticiones HTTP GET** usando `fetch()`. Se hacen dos tipos de peticiones:

* **Consulta de estado:**
  Se hace regularmente cada 200 ms mediante `fetchStatus()` hacia el endpoint `/status`.

* **Envío de comandos:**
  Cuando el usuario presiona un botón, se ejecuta `enviarComando()` que accede a `/abrir`, `/cerrar` o `/reset`.

> **Ejemplo de llamada HTTP:**

```javascript
const response = await fetch(`${API_URL}/abrir`);
```

Las respuestas se procesan en formato JSON y se utilizan para actualizar la interfaz con la información más reciente.

### 4.3 Actualización dinámica de la interfaz

Toda la interfaz se actualiza con la función `updateUI(data)`, que:

* Actualiza los textos de estado.
* Cambia el color de los indicadores LED.
* Muestra alertas si el sistema está bloqueado.
* Ajusta los botones de control según el estado (por ejemplo, desactivando “Abrir” si ya está completamente abierto).

También se incluye una **barra de progreso animada** que simula visualmente el movimiento del motor en función del porcentaje de apertura recibido.

```javascript
document.getElementById('progressBar').style.width = `${data.c}%`;
```

### 4.5 Registro de eventos (`.log`)

Cada evento importante o cambio en el sistema se registra en un panel de log para seguimiento. Esto incluye:

* Conexiones exitosas.
* Comandos enviados.
* Errores de red.
* Cambios de estado del motor o sensores.

Esto facilita la depuración del sistema sin necesidad de acceder al monitor serie.

### 4.6 Indicadores visuales

* **LEDs simulados**:
  Pequeños círculos que cambian dinámicamente de color según el valor de `data.v` y `data.r`.

* **Alerta de obstrucción**:
  Muestra un bloque rojo con el tiempo restante de bloqueo si se detecta una obstrucción (`data.o === '1'`).

![Texto alternativo](images/interfaz-web.png)

## 5. Sincronización entre Arduino e Interfaz

Una de las características clave del sistema es su capacidad para **mostrar en tiempo real el estado del motor, sensores y LEDs**, así como para **responder a eventos inesperados como obstrucciones**. Esto se logra gracias a un proceso de sincronización constante entre la interfaz web y el servidor embebido en Arduino.

### 5.1 Actualización periódica del estado

Desde que se carga la página, se activa un ciclo continuo de actualizaciones usando la función `monitorStatus()`:

```javascript
fetchStatus().then(() => monitorStatus());
```

Cada 200 milisegundos, esta función llama a `fetchStatus()`, que:

* Hace una petición GET al endpoint `/status`.
* Espera una respuesta JSON con todos los datos del sistema.
* Llama a `updateUI(data)` para actualizar la interfaz visual.

Este enfoque, aunque no utiliza WebSockets, es **suficiente y eficiente en una red local** para mantener sincronía sin complicaciones.

### 5.2 Formato de los datos (JSON)

La respuesta del servidor Arduino incluye los siguientes campos:

```json
{
  "e": "ABRIENDO",
  "p": 600,
  "t": 1500,
  "c": 40,
  "s": "NORMAL",
  "m": "MOVIENDOSE",
  "v": "1",
  "r": "0",
  "o": "0",
  "tr": 0
}
```

Estos campos son interpretados por la interfaz para:

* Mostrar el estado textual (`e`)
* Dibujar la barra de progreso (`c`)
* Activar o desactivar botones según el estado del motor o presencia de obstrucción
* Cambiar colores de los LEDs virtuales
* Mostrar alertas si el sistema está bloqueado (`o === '1'`)

### 5.3 Manejo de interrupciones y sincronización forzada

Si el motor se detiene inesperadamente (por ejemplo, por una obstrucción), el sistema lo detecta automáticamente:

```javascript
if (wasMoving && !isNowMoving && animationFrame) {
    cancelAnimationFrame(animationFrame);
    resetMovementState();
}
```

Esto detiene la animación y fuerza una sincronización directa con el estado real, evitando inconsistencias visuales.

### Tabla de sincronización: JSON → interfaz

| Campo JSON    | Componente visual en la interfaz                           |
| ------------- | ---------------------------------------------------------- |
| `e`           | Estado textual (`#sistemaEstado`)                          |
| `p`, `t`, `c` | Posición y barra de progreso (`#progressBar`, `#posicion`) |
| `s`           | Estado del sensor (`#sensor`)                              |
| `m`           | Estado del motor (`#motor`)                                |
| `v`, `r`      | Indicadores LED (`#ledVerde`, `#ledRojo`)                  |
| `o`, `tr`     | Alerta de obstrucción (`#alertContainer`)                  |

