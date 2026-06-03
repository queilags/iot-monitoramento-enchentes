#include <ESP8266WiFi.h>
#include <PubSubClient.h>

const char* ssid = "NOME_DA_REDE_WIFI";
const char* password = "SENHA_DO_WIFI";
const char* mqtt_server = "broker.hivemq.com";

WiFiClient espClient;
PubSubClient client(espClient);

const int sensorAgua = A0;
const int buzzer = D1;
const int ledVerde = D5;
const int ledAmarelo = D6;
const int ledVermelho = D7;

void setup() {
  Serial.begin(115200);

  pinMode(buzzer, OUTPUT);
  pinMode(ledVerde, OUTPUT);
  pinMode(ledAmarelo, OUTPUT);
  pinMode(ledVermelho, OUTPUT);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  client.setServer(mqtt_server, 1883);
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("NodeMCU_Monitoramento")) {
      Serial.println("Conectado ao broker MQTT");
    } else {
      delay(2000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }

  client.loop();

  int nivelAgua = analogRead(sensorAgua);

  if (nivelAgua < 400) {
    digitalWrite(ledVerde, HIGH);
    digitalWrite(ledAmarelo, LOW);
    digitalWrite(ledVermelho, LOW);
    digitalWrite(buzzer, LOW);
    client.publish("monitoramento/nivel_agua", "normal");
  } 
  else if (nivelAgua >= 400 && nivelAgua < 700) {
    digitalWrite(ledVerde, LOW);
    digitalWrite(ledAmarelo, HIGH);
    digitalWrite(ledVermelho, LOW);
    digitalWrite(buzzer, LOW);
    client.publish("monitoramento/nivel_agua", "atencao");
  } 
  else {
    digitalWrite(ledVerde, LOW);
    digitalWrite(ledAmarelo, LOW);
    digitalWrite(ledVermelho, HIGH);
    digitalWrite(buzzer, HIGH);
    client.publish("monitoramento/nivel_agua", "critico");
  }

  delay(2000);
}
