#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
 
// ================= WIFI =================
const char* ssid = "Wokwi-GUEST";
const char* password = "";
 
// ================= MQTT =================
const char* mqttServer = "test.mosquitto.org";
const int mqttPort = 1883;
 
WiFiClient espClient;
PubSubClient client(espClient);
 
// ================= PINOS (ESP32-C3) =================
#define TRIG_PIN     4
#define ECHO_PIN     5
 
#define SOIL_PIN     0   // Potenciômetro
#define RAIN_PIN     1   // Potenciômetro
 
#define BUZZER_PIN   3
 
#define LED_GREEN    6
#define LED_YELLOW   7
#define LED_RED      2
 
#define RELAY_PIN    8   // Controla o relé (e a luz azul)
 
// ===== LIMITES =====
float LIMITE_CRITICO = 5.0;
float LIMITE_ATENCAO = 15.0;
float LIMITE_DESLIGA = 20.0;
 
// ===== MODO AUTOMÁTICO/MANUAL =====
bool modoAutomatico = true;
bool bombaLigadaManual = false;
 
unsigned long lastPublish = 0;
unsigned long lastBuzzerToggle = 0;
bool buzzerState = false;
int ultimoEstadoBomba = LOW;  // Para evitar repetição
 
// =========================================
 
void connectWiFi() {
  Serial.print("Conectando WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}
 
void connectMQTT() {
  while (!client.connected()) {
    Serial.print("Conectando MQTT...");
    String clientId = "ESP32FloodMonitor-";
    clientId += String(random(10000));
    if (client.connect(clientId.c_str())) {
      Serial.println("OK");
      client.subscribe("monitoramento/comando/bomba");
      client.subscribe("monitoramento/comando/modo");
      client.subscribe("monitoramento/comando/limites");
    } else {
      Serial.print("Erro MQTT: ");
      Serial.println(client.state());
      delay(2000);
    }
  }
}
 
float readWaterDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) {
    return 400.0;
  }
  return duration * 0.034 / 2.0;
}
 
void controlarBomba(float distancia, bool critico) {
  int estadoAtual = digitalRead(RELAY_PIN);
  int novoEstado = estadoAtual;
  if (modoAutomatico) {
    if (critico || distancia < LIMITE_CRITICO) {
      novoEstado = HIGH;  // Liga bomba
    } 
    else if (distancia > LIMITE_DESLIGA) {
      novoEstado = LOW;   // Desliga bomba
    }
  } else {
    novoEstado = bombaLigadaManual ? HIGH : LOW;
  }
  // Só muda se necessário e publica UMA vez
  if (novoEstado != estadoAtual) {
    digitalWrite(RELAY_PIN, novoEstado);
    if (novoEstado == HIGH) {
      Serial.println(">> BOMBA LIGADA (LED azul deve acender)");
    } else {
      Serial.println(">> BOMBA DESLIGADA (LED azul deve apagar)");
    }
  }
}
 
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  String topico = String(topic);
  if (topico == "monitoramento/comando/bomba") {
    if (message == "ON") {
      bombaLigadaManual = true;
      modoAutomatico = false;
      Serial.println(">> Modo manual: bomba LIGADA");
    } 
    else if (message == "OFF") {
      bombaLigadaManual = false;
      modoAutomatico = false;
      Serial.println(">> Modo manual: bomba DESLIGADA");
    }
  }
  else if (topico == "monitoramento/comando/modo") {
    if (message == "AUTO") {
      modoAutomatico = true;
      Serial.println(">> Modo automático ativado");
    } 
    else if (message == "MANUAL") {
      modoAutomatico = false;
      Serial.println(">> Modo manual ativado");
    }
  }
  else if (topico == "monitoramento/comando/limites") {
    int primeiro = message.indexOf(',');
    int segundo = message.indexOf(',', primeiro + 1);
    if (primeiro > 0 && segundo > 0) {
      LIMITE_CRITICO = message.substring(0, primeiro).toFloat();
      LIMITE_ATENCAO = message.substring(primeiro + 1, segundo).toFloat();
      LIMITE_DESLIGA = message.substring(segundo + 1).toFloat();
      Serial.printf(">> Limites: CRITICO=%.1f ATENCAO=%.1f DESLIGA=%.1f\n", 
                    LIMITE_CRITICO, LIMITE_ATENCAO, LIMITE_DESLIGA);
    }
  }
}
 
void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  connectWiFi();
  client.setServer(mqttServer, mqttPort);
  client.setCallback(mqttCallback);
  randomSeed(micros());
  Serial.println("========================================");
  Serial.println(" MONITORAMENTO DE ENCHENTES");
  Serial.println("========================================");
  Serial.printf("Limites: CRITICO=%.1fcm | ATENCAO=%.1fcm | DESLIGA=%.1fcm\n", 
                LIMITE_CRITICO, LIMITE_ATENCAO, LIMITE_DESLIGA);
  // Teste do relé e LED azul
  Serial.println("\n--- TESTE DO RELÉ ---");
  Serial.println("Ligando relé por 2 segundos...");
  digitalWrite(RELAY_PIN, HIGH);
  delay(2000);
  Serial.println("Desligando relé...");
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("--- FIM DO TESTE ---\n");
}
 
void loop() {
  if (!client.connected()) {
    connectMQTT();
  }
  client.loop();
  float waterDistance = readWaterDistance();
  int soilValue = analogRead(SOIL_PIN);
  int rainValue = analogRead(RAIN_PIN);
  bool waterCritical = waterDistance < LIMITE_CRITICO;
  bool waterWarning  = waterDistance >= LIMITE_CRITICO && waterDistance < LIMITE_ATENCAO;
  bool soilCritical = soilValue > 3500;
  bool soilWarning  = soilValue > 2500;
  bool rainCritical = rainValue > 3500;
  bool rainWarning  = rainValue > 2500;
  controlarBomba(waterDistance, waterCritical);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_YELLOW, LOW);
  digitalWrite(LED_RED, LOW);
  String statusRisk = "NORMAL";
  if (waterCritical || soilCritical || rainCritical) {
    statusRisk = "CRITICO";
    digitalWrite(LED_RED, HIGH);
    if (millis() - lastBuzzerToggle > 300) {
      lastBuzzerToggle = millis();
      buzzerState = !buzzerState;
      if (buzzerState) {
        tone(BUZZER_PIN, 1000);
      } else {
        noTone(BUZZER_PIN);
      }
    }
  } 
  else if (waterWarning || soilWarning || rainWarning) {
    statusRisk = "ATENCAO";
    digitalWrite(LED_YELLOW, HIGH);
    noTone(BUZZER_PIN);
  } 
  else {
    statusRisk = "NORMAL";
    digitalWrite(LED_GREEN, HIGH);
    noTone(BUZZER_PIN);
  }
  if (millis() - lastPublish > 3000) {
    lastPublish = millis();
    StaticJsonDocument<256> doc;
    doc["nivel_agua"] = waterDistance;
    doc["umidade_solo"] = soilValue;
    doc["chuva"] = rainValue;
    doc["status"] = statusRisk;
    doc["bomba_ligada"] = digitalRead(RELAY_PIN);
    doc["modo"] = modoAutomatico ? "AUTO" : "MANUAL";
    doc["limite_critico"] = LIMITE_CRITICO;
    doc["limite_atencao"] = LIMITE_ATENCAO;
    doc["limite_desliga"] = LIMITE_DESLIGA;
    char buffer[256];
    serializeJson(doc, buffer);
    client.publish("monitoramento/dados", buffer);
    client.publish("monitoramento/nivel_agua", String(waterDistance, 2).c_str());
    client.publish("monitoramento/status", statusRisk.c_str());
    client.publish("monitoramento/bomba", digitalRead(RELAY_PIN) ? "ON" : "OFF");
    client.publish("monitoramento/modo", modoAutomatico ? "AUTO" : "MANUAL");
    Serial.println();
    Serial.println("================================");
    Serial.print("Agua: "); Serial.print(waterDistance); Serial.println(" cm");
    Serial.print("Solo: "); Serial.println(soilValue);
    Serial.print("Chuva: "); Serial.println(rainValue);
    Serial.print("Status: "); Serial.println(statusRisk);
    Serial.print("Bomba: "); Serial.println(digitalRead(RELAY_PIN) ? "LIGADA" : "DESLIGADA");
    Serial.print("Modo: "); Serial.println(modoAutomatico ? "AUTO" : "MANUAL");
    Serial.println("================================");
  }
  delay(100);
}
