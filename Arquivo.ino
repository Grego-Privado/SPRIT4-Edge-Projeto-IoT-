//Autores: Azor Biagioni Tartuce RM:563995
//Victor Altieri RM: 565288
//Pedro Henrique Silva Gregolini RM:563342
//Rafael Falaguasta RM: 561714
//Ben-Hur Iung de Lima Ferreira RM: 561564

#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

// ---------------------- CONFIGURAÇÕES ----------------------
const char* SSID = "Wokwi-GUEST";
const char* PASSWORD = "";
const char* BROKER_MQTT = "20.151.88.70";
const int BROKER_PORT = 1883;

const char* TOPICO_PUBLISH = "/TEF/field001/attrs";
const char* TOPICO_PUBLISH_l = "/TEF/field001/attrs/l";
const char* TOPICO_PUBLISH_t = "/TEF/field001/attrs/t";
const char* TOPICO_PUBLISH_h = "/TEF/field001/attrs/h";
const char* TOPICO_SUBSCRIBE = "/TEF/field001/cmd";
const char* ID_MQTT = "field001";

#define LED_ONBOARD 2      // LED onboard
#define LDR_PIN 34          // Sensor LDR
#define DHT_PIN 4           // Sensor DHT22
#define BUZZER_PIN 25       // Buzzer ativo/passivo

#define DHTTYPE DHT22
DHT dht(DHT_PIN, DHTTYPE);

// ---------------------- OBJETOS ----------------------
WiFiClient espClient;
PubSubClient MQTT(espClient);
char EstadoSaida = '0';

// ---------------------- LIMITES IDEIAIS ----------------------
// Faixas ideais para uma vinheria
const int LUZ_MAX = 40;         // %
const float TEMP_MIN = 12.0;    // °C
const float TEMP_MAX = 18.0;    // °C
const float UMID_MIN = 60.0;    // %
const float UMID_MAX = 80.0;    // %

// ---------------------- VARIÁVEIS DE CONTROLE ----------------------
bool alertaAtivo = false;
unsigned long ultimoAlerta = 0;
unsigned long intervaloPiscaLED = 500;
bool estadoLED = false;

// ---------------------- FUNÇÕES ----------------------
void initSerial() {
  Serial.begin(115200);
  Serial.println("=== CP5 - VINHERIA IoT ===");
  Serial.println("Dispositivo: field001");
  Serial.println("Pinos: DHT=4, LDR=34");
}

void initWiFi() {
  Serial.print("Conectando WiFi");
  WiFi.begin(SSID, PASSWORD);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" OK!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" FALHOU!");
  }
}

void initMQTT() {
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(mqtt_callback);
}

// ---------------------- CALLBACK MQTT ----------------------
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.print("Comando recebido: ");
  Serial.println(msg);

  // Processar comandos do Postman (on/off)
  if (msg.indexOf("on") >= 0) {
    digitalWrite(LED_ONBOARD, HIGH);
    EstadoSaida = '1';
    Serial.println("💡 LED ligado via comando remoto");
  }
  else if (msg.indexOf("off") >= 0) {
    digitalWrite(LED_ONBOARD, LOW);
    EstadoSaida = '0';
    alertaAtivo = false;
    Serial.println("💡 LED desligado via comando remoto");
  }
}

void reconnectMQTT() {
  while (!MQTT.connected()) {
    Serial.print("Conectando MQTT...");
    if (MQTT.connect(ID_MQTT)) {
      Serial.println(" OK!");
      MQTT.subscribe(TOPICO_SUBSCRIBE);
      Serial.println("Subscrito aos comandos");
    } else {
      Serial.print(" FALHA (");
      Serial.print(MQTT.state());
      Serial.println(")");
      delay(2000);
    }
  }
}

// ---------------------- FUNÇÃO DE ALERTA ----------------------
void emitirAlerta(int frequencia, String tipo) {
  Serial.print("⚠️ ALERTA: ");
  Serial.println(tipo);

  alertaAtivo = true;

  // Som de alerta
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, frequencia, 300);
    delay(400);
    noTone(BUZZER_PIN);
    delay(100);
  }
}

void controlarLEDAlerta() {
  if (alertaAtivo) {
    unsigned long agora = millis();
    if (agora - ultimoAlerta > intervaloPiscaLED) {
      estadoLED = !estadoLED;
      digitalWrite(LED_ONBOARD, estadoLED ? HIGH : LOW);
      ultimoAlerta = agora;
    }
  }
}

// ---------------------- SETUP ----------------------
void setup() {
  initSerial();

  // Configurar pinos
  pinMode(LED_ONBOARD, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);  // GPIO 34

  digitalWrite(LED_ONBOARD, LOW);

  // Inicializar DHT no GPIO 4 (INPUT/OUTPUT)
  dht.begin();
  Serial.println("DHT22 inicializado no GPIO 4");

  initWiFi();
  initMQTT();

  Serial.println("Sistema iniciado!");
  Serial.printf("Limites: Temp[%.1f-%.1f]°C Umid[%.1f-%.1f]%% Luz[<%d%%]\n", 
                TEMP_MIN, TEMP_MAX, UMID_MIN, UMID_MAX, LUZ_MAX);
  delay(2000);
}

// ---------------------- LOOP PRINCIPAL ----------------------
void loop() {
  // Manter conexão MQTT
  if (!MQTT.connected()) {
    reconnectMQTT();
  }
  MQTT.loop();

  // Controlar LED durante alertas
  controlarLEDAlerta();

  // ===== LEITURA DOS SENSORES =====
  // LDR no GPIO 34
  int ldrValue = analogRead(LDR_PIN);
  int luminosidade = map(ldrValue, 0, 4095, 100, 0);

  // DHT22 no GPIO 4
  float temperatura = dht.readTemperature();
  float umidade = dht.readHumidity();

  // Verificar se DHT está funcionando
  if (isnan(temperatura) || isnan(umidade)) {
    Serial.println("❌ Erro na leitura do DHT22! Verifique conexões.");
    delay(2000);
    return;
  }

  // ===== MOSTRAR DADOS =====
  Serial.println("===============================");
  Serial.printf("☀️ Luminosidade: %d%% (GPIO 34)\n", luminosidade);
  Serial.printf("🌡️ Temperatura: %.1f°C (GPIO 4)\n", temperatura);
  Serial.printf("💧 Umidade: %.1f%% (GPIO 4)\n", umidade);

  // ===== ENVIAR VIA MQTT =====
  // Formato UltraLight: object_id|valor|object_id|valor
  // Usando os object_id do Postman: l|t|h|s

  String mensagem_luminosidade = String(luminosidade);    // l = luminosity
  String mensagem_temperatura = String(temperatura, 1); // t = temperature
  String mensagem_umidade = String(umidade, 1);     // h = humidity

  if (MQTT.publish(TOPICO_PUBLISH_l, mensagem_luminosidade.c_str())) {
    Serial.println("Mensagem: " + mensagem_luminosidade);
  } else {
    Serial.println("❌ Falha no envio MQTT");
  }

  if (MQTT.publish(TOPICO_PUBLISH_t, mensagem_temperatura.c_str())) {
    Serial.println("Mensagem: " + mensagem_temperatura);
  } else {
    Serial.println("❌ Falha no envio MQTT");
  }

  if (MQTT.publish(TOPICO_PUBLISH_h, mensagem_umidade.c_str())) {
    Serial.println("Mensagem: " + mensagem_umidade);
  } else {
    Serial.println("❌ Falha no envio MQTT");
  }

  // ===== VERIFICAÇÃO DOS LIMITES E ALERTAS =====
  bool parametros_normais = true;

  if (luminosidade < LUZ_MAX) {
    emitirAlerta(1000, "LUMINOSIDADE BAIXA, LIGAR LUZES");
    parametros_normais = false;
  }

  Serial.println("-------------------------------");
  delay(5000); // Intervalo entre leituras
}//Autores: Azor Biagioni Tartuce RM:563995
//Victor Altieri RM: 565288
//Pedro Henrique Silva Gregolini RM:563342
//Rafael Falaguasta RM: 561714
//Ben-Hur Iung de Lima Ferreira RM: 561564

#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

// ---------------------- CONFIGURAÇÕES ----------------------
const char* SSID = "Wokwi-GUEST";
const char* PASSWORD = "";
const char* BROKER_MQTT = "20.151.88.70";
const int BROKER_PORT = 1883;

const char* TOPICO_PUBLISH = "/TEF/field001/attrs";
const char* TOPICO_PUBLISH_l = "/TEF/field001/attrs/l";
const char* TOPICO_PUBLISH_t = "/TEF/field001/attrs/t";
const char* TOPICO_PUBLISH_h = "/TEF/field001/attrs/h";
const char* TOPICO_SUBSCRIBE = "/TEF/field001/cmd";
const char* ID_MQTT = "field001";

#define LED_ONBOARD 2      // LED onboard
#define LDR_PIN 34          // Sensor LDR
#define DHT_PIN 4           // Sensor DHT22
#define BUZZER_PIN 25       // Buzzer ativo/passivo

#define DHTTYPE DHT22
DHT dht(DHT_PIN, DHTTYPE);

// ---------------------- OBJETOS ----------------------
WiFiClient espClient;
PubSubClient MQTT(espClient);
char EstadoSaida = '0';

// ---------------------- LIMITES IDEIAIS ----------------------
// Faixas ideais para uma vinheria
const int LUZ_MAX = 40;         // %
const float TEMP_MIN = 12.0;    // °C
const float TEMP_MAX = 18.0;    // °C
const float UMID_MIN = 60.0;    // %
const float UMID_MAX = 80.0;    // %

// ---------------------- VARIÁVEIS DE CONTROLE ----------------------
bool alertaAtivo = false;
unsigned long ultimoAlerta = 0;
unsigned long intervaloPiscaLED = 500;
bool estadoLED = false;

// ---------------------- FUNÇÕES ----------------------
void initSerial() {
  Serial.begin(115200);
  Serial.println("=== CP5 - VINHERIA IoT ===");
  Serial.println("Dispositivo: field001");
  Serial.println("Pinos: DHT=4, LDR=34");
}

void initWiFi() {
  Serial.print("Conectando WiFi");
  WiFi.begin(SSID, PASSWORD);

  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    Serial.print(".");
    tentativas++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" OK!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" FALHOU!");
  }
}

void initMQTT() {
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(mqtt_callback);
}

// ---------------------- CALLBACK MQTT ----------------------
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.print("Comando recebido: ");
  Serial.println(msg);

  // Processar comandos do Postman (on/off)
  if (msg.indexOf("on") >= 0) {
    digitalWrite(LED_ONBOARD, HIGH);
    EstadoSaida = '1';
    Serial.println("💡 LED ligado via comando remoto");
  }
  else if (msg.indexOf("off") >= 0) {
    digitalWrite(LED_ONBOARD, LOW);
    EstadoSaida = '0';
    alertaAtivo = false;
    Serial.println("💡 LED desligado via comando remoto");
  }
}

void reconnectMQTT() {
  while (!MQTT.connected()) {
    Serial.print("Conectando MQTT...");
    if (MQTT.connect(ID_MQTT)) {
      Serial.println(" OK!");
      MQTT.subscribe(TOPICO_SUBSCRIBE);
      Serial.println("Subscrito aos comandos");
    } else {
      Serial.print(" FALHA (");
      Serial.print(MQTT.state());
      Serial.println(")");
      delay(2000);
    }
  }
}

// ---------------------- FUNÇÃO DE ALERTA ----------------------
void emitirAlerta(int frequencia, String tipo) {
  Serial.print("⚠️ ALERTA: ");
  Serial.println(tipo);

  alertaAtivo = true;

  // Som de alerta
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, frequencia, 300);
    delay(400);
    noTone(BUZZER_PIN);
    delay(100);
  }
}

void controlarLEDAlerta() {
  if (alertaAtivo) {
    unsigned long agora = millis();
    if (agora - ultimoAlerta > intervaloPiscaLED) {
      estadoLED = !estadoLED;
      digitalWrite(LED_ONBOARD, estadoLED ? HIGH : LOW);
      ultimoAlerta = agora;
    }
  }
}

// ---------------------- SETUP ----------------------
void setup() {
  initSerial();

  // Configurar pinos
  pinMode(LED_ONBOARD, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);  // GPIO 34

  digitalWrite(LED_ONBOARD, LOW);

  // Inicializar DHT no GPIO 4 (INPUT/OUTPUT)
  dht.begin();
  Serial.println("DHT22 inicializado no GPIO 4");

  initWiFi();
  initMQTT();

  Serial.println("Sistema iniciado!");
  Serial.printf("Limites: Temp[%.1f-%.1f]°C Umid[%.1f-%.1f]%% Luz[<%d%%]\n", 
                TEMP_MIN, TEMP_MAX, UMID_MIN, UMID_MAX, LUZ_MAX);
  delay(2000);
}

// ---------------------- LOOP PRINCIPAL ----------------------
void loop() {
  // Manter conexão MQTT
  if (!MQTT.connected()) {
    reconnectMQTT();
  }
  MQTT.loop();

  // Controlar LED durante alertas
  controlarLEDAlerta();

  // ===== LEITURA DOS SENSORES =====
  // LDR no GPIO 34
  int ldrValue = analogRead(LDR_PIN);
  int luminosidade = map(ldrValue, 0, 4095, 100, 0);

  // DHT22 no GPIO 4
  float temperatura = dht.readTemperature();
  float umidade = dht.readHumidity();

  // Verificar se DHT está funcionando
  if (isnan(temperatura) || isnan(umidade)) {
    Serial.println("❌ Erro na leitura do DHT22! Verifique conexões.");
    delay(2000);
    return;
  }

  // ===== MOSTRAR DADOS =====
  Serial.println("===============================");
  Serial.printf("☀️ Luminosidade: %d%% (GPIO 34)\n", luminosidade);
  Serial.printf("🌡️ Temperatura: %.1f°C (GPIO 4)\n", temperatura);
  Serial.printf("💧 Umidade: %.1f%% (GPIO 4)\n", umidade);

  // ===== ENVIAR VIA MQTT =====
  // Formato UltraLight: object_id|valor|object_id|valor
  // Usando os object_id do Postman: l|t|h|s

  String mensagem_luminosidade = String(luminosidade);    // l = luminosity
  String mensagem_temperatura = String(temperatura, 1); // t = temperature
  String mensagem_umidade = String(umidade, 1);     // h = humidity

  if (MQTT.publish(TOPICO_PUBLISH_l, mensagem_luminosidade.c_str())) {
    Serial.println("Mensagem: " + mensagem_luminosidade);
  } else {
    Serial.println("❌ Falha no envio MQTT");
  }

  if (MQTT.publish(TOPICO_PUBLISH_t, mensagem_temperatura.c_str())) {
    Serial.println("Mensagem: " + mensagem_temperatura);
  } else {
    Serial.println("❌ Falha no envio MQTT");
  }

  if (MQTT.publish(TOPICO_PUBLISH_h, mensagem_umidade.c_str())) {
    Serial.println("Mensagem: " + mensagem_umidade);
  } else {
    Serial.println("❌ Falha no envio MQTT");
  }

  // ===== VERIFICAÇÃO DOS LIMITES E ALERTAS =====
  bool parametros_normais = true;

  if (luminosidade < LUZ_MAX) {
    emitirAlerta(1000, "LUMINOSIDADE BAIXA, LIGAR LUZES");
    parametros_normais = false;
  }

  Serial.println("-------------------------------");
  delay(5000); // Intervalo entre leituras
}