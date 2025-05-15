#include <WiFi.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// --- Definições de pinos e parâmetros ADC ---
const int TURBIDITY_PIN   = 34;   // ADC1_CH6, turbidez
const int PH_PIN          = 35;   // ADC1_CH7, pH
const float VREF          = 3.3;  // tensão de referência do ESP32
const int   RAW_MAX       = 4095; // resolução 12 bits

// --- Constantes de calibração turbidez (0–800 NTU) ---
//const float V0            = 1.6606;   // tensão média em 0 NTU (água de torneira)
const float V0            = 0.850;   // tensão média em 0 NTU (água de torneira)
//const float V800          = 1.4479;   // tensão média em 800 NTU (2 g/L café)
const float V800          = 0.582;   // tensão média em 800 NTU (2 g/L café)
const float STANDARD_NTU  = 800.0;    // faixa máxima
const int   N_SAMPLES     = 100;      // amostras para suavizar turbidez

// --- Coeficientes de calibração pH (obtidos em pH7 e pH4) ---
const float PH_SLOPE      = -3.88391;   // seu valor de slope
const float PH_INTERCEPT  = 16.43099;   // seu valor de offset


// --- DS18B20 (OneWire) ---
#define DS18B20_PIN 4               // pino de dados do DS18B20
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

// --- WiFi ---
const char* WIFI_SSID = "INTELBRAS";
const char* WIFI_PASS = "Anaenena";

// --- InfluxDB Cloud ---
#define INFLUXDB_URL    "https://us-east-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN  "Token _eaLR1vnoP34qswOjeToIJE6fSvz4melC0Vpln4jI_YKcK6R1Q_b7cEzQ5BOr6XlAgWP9cAmYFW1kmnJpJjhwg=="
#define INFLUXDB_ORG    "6984e8fff6b2d928"
#define INFLUXDB_BUCKET "sensor_dagua"
// Ajuste precision=ns ou =s conforme seu bucket/configuração
#define INFLUXDB_PRECISION "ns"

void setup() {
  Serial.begin(115200);

  // Configura ADC para leitura até ~3.3 V
  analogSetPinAttenuation(TURBIDITY_PIN, ADC_11db);
  analogSetPinAttenuation(PH_PIN,        ADC_11db);

  // Inicializa sensor de temperatura
  sensors.begin();

  // Conecta no Wi-Fi
  Serial.print("Conectando em ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  Serial.println("Iniciando medições de turbidez, pH e temperatura...");
}

void loop() {
  // --- 1) Medição de Turbidez ---
  long sumRaw = 0;
  for (int i = 0; i < N_SAMPLES; i++) {
    sumRaw += analogRead(TURBIDITY_PIN);
    delay(10);
  }
  float avgRaw = sumRaw / float(N_SAMPLES);
  float vT = avgRaw * (VREF / RAW_MAX);
  float ntu = (vT - V0) / (V800 - V0) * STANDARD_NTU;
  ntu = constrain(ntu, 0.0, STANDARD_NTU);

  // --- 2) Medição de pH ---
  int rawP = analogRead(PH_PIN);
  float vP = rawP * (VREF / RAW_MAX);
  float phValue = PH_SLOPE * vP + PH_INTERCEPT;

  // --- 3) Medição de Temperatura DS18B20 ---
  sensors.requestTemperatures();           // dispara conversão
  float tempC = sensors.getTempCByIndex(0); // lê em °C
  bool tempError = (tempC == DEVICE_DISCONNECTED_C);

  // --- 4) Print no Serial ---
  Serial.printf("Turbidez → V: %.3f V | NTU: %.1f", vT, ntu);
  Serial.printf("   |   pH → V: %.3f V | pH: %.2f", vP, phValue);
  Serial.print("   |   Temp: ");
  if (tempError) {
    Serial.print("Erro!");
  } else {
    Serial.printf("%.2f °C", tempC);
  }
  Serial.println();

  // --- 5) Envio para InfluxDB ---
  sendToInflux(ntu, phValue, tempC);

  // Aguarda antes da próxima rodada
  delay(20000);  // respeita limite de escrita do InfluxDB (~1 chamada/15s)
}

void sendToInflux(float ntu, float ph, float temp) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado, não enviando dados.");
    return;
  }

  HTTPClient http;
  String url = String(INFLUXDB_URL)
             + "/api/v2/write?org=" + INFLUXDB_ORG
             + "&bucket=" + INFLUXDB_BUCKET
             + "&precision=" + INFLUXDB_PRECISION;
  http.begin(url);
  http.addHeader("Authorization", INFLUXDB_TOKEN);
  http.addHeader("Content-Type", "text/plain; charset=utf-8");

  // monta o body usando String(...) para iniciar
  String body = String("medidas ")
              + "ntu="  + String(ntu, 1) + ","
              + "ph="   + String(ph,  2) + ","
              + "temp=" + String(temp,2);

  int code = http.POST(body);
  if (code == 204) {
    Serial.println("  → Enviado com sucesso ao InfluxDB");
  } else {
    Serial.printf("  → Erro HTTP: %d\n", code);
  }
  http.end();
}
