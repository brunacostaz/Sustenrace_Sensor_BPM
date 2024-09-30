#include<WiFi.h>
#include<time.h>
#include<ArduinoJson.h>
#include<PubSubClient.h>
#include<U8g2lib.h>

// Definindo as portas e constantes
const char *SSID = "Wokwi-GUEST";
const char *PASSWORD = "";

const char *BROKER_MQTT = "broker.hivemq.com";
const int BROKER_PORT = 1883;
const char *ID_MQTT = "sustenrace-iot";
const char *TOPIC_PUB_BPM = "sustenrace/iot/bpm";
#define PUBLISH_DELAY 2000

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long publishUpdate = 0;

const char* device_id = "pulseira1";
#define  POT_PIN 34 //pino do potenciômetro
#define LARGURA_MAX_OLED 128 // largura máxima do display

//Criação de um buffer
int bufferBatimento[LARGURA_MAX_OLED];

// Criação do objeto para o display oled
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, /* reset=*/U8X8_PIN_NONE); 

void initWiFi() {
  Serial.printf("Conectando com a rede: %s\n", SSID);
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) delay(100);
  Serial.printf("Conectado com sucesso: %s\nIP: %s\n", SSID, WiFi.localIP().toString().c_str());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Tentando conectar ao MQTT...");
    if (client.connect(ID_MQTT)) {
      Serial.println("conectado");
    } else {
      Serial.print("falhou, rc=");
      Serial.print(client.state());
      Serial.println(" tentando novamente em 5 segundos");
      delay(5000);
    }
  }
}

String obterDataHora() {
  struct tm dataHora;
  while (!getLocalTime(&dataHora)) Serial.println("Erro ao obter tempo");
  char bufferTime[30];
  strftime(bufferTime, sizeof(bufferTime), "%Y-%m-%d %H:%M:%S", &dataHora);
  Serial.println(bufferTime);
  return String(bufferTime);
}

void enviarDadosMQTT(const char* id, int bpm, String timestamp) {
  DynamicJsonDocument json(200);
  json["device_id"] = id;
  json["timestamp"] = timestamp;
  json["bpm"] = bpm;
  char jsonBuffer[256];
  serializeJson(json, jsonBuffer);
  client.publish(TOPIC_PUB_BPM, jsonBuffer);
  Serial.println(jsonBuffer);
}

//Função para atualizar os valores do buffer 
void updateBuffer(int novoValor){
  //realiza o deslocamento dos valores dentro do buffer para a esquerda
  //dessa forma, criaremos a animação do batimento cardíaco
  for(int i = 0; i < LARGURA_MAX_OLED - 1; i++){
    bufferBatimento[i] = bufferBatimento[i + 1];
  }
  //adiciona o novo valor no final do buffer 
  // e mapeia o valor do batimento cardíaco para o tamanho da altura do display(64 pixels)
  bufferBatimento[LARGURA_MAX_OLED - 1] = map(novoValor,60,150,60,0);

}

//Função para desenhar o batimento cardíaco no display OLED
void desenharBatimento(){
  for(int i=0; i < LARGURA_MAX_OLED - 1; i++){
    oled.drawLine(i, bufferBatimento[i], i+1, bufferBatimento[i+1]);
  }
}

void setup() {
  Serial.begin(115200);
  configTime(-3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  initWiFi();
  client.setServer(BROKER_MQTT, BROKER_PORT);

  //Inicialização do buffer de batimento cardíaco
  for(int i = 0; i < LARGURA_MAX_OLED; i++) {
    bufferBatimento[i] = 30;
    //configura o inicio de todos os valores do buffer para 30
    //esse valor corresponde ao posicionamento em Y, colocando-os ao meio
  }

  // inicialização do display oled
  oled.begin();
  // Definindo uma fonte suportada
  oled.setFont(u8g2_font_ncenB08_tr);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) initWiFi();
  if (!client.connected()) reconnect(); 
  client.loop();

  int potBPM = analogRead(POT_PIN);
  String tempoBPM = obterDataHora();
  int mapBPM = map(potBPM, 0, 4095, 60, 150);
  Serial.println(mapBPM);

  if (millis() - publishUpdate >= PUBLISH_DELAY) {
    publishUpdate = millis();
    enviarDadosMQTT(device_id, mapBPM, tempoBPM);
  }

  //chama a função para atualizar o buffer
  updateBuffer(mapBPM);
  
  // Atualizar o display OLED

  // limpa o buffer do display
  oled.clearBuffer();
  //desenha o gráfico do bpm
  desenharBatimento();
  //define a posição do texto
  oled.setCursor(0, 63);
  oled.print("BPM: ");
  oled.print(mapBPM);
  //envia os dados ao display
  oled.sendBuffer();


  delay(1000);
}
