#include <WiFiManager.h>     // https://github.com/tzapu/WiFiManager
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h>
#include "secrets.h"

// --- Pinos e parâmetros ---
const int TURBIDITY_PIN   = 34;   // ADC1_CH6, turbidez
const int PH_PIN          = 35;   // ADC1_CH7, pH
const float VREF          = 3.3;
const int   RAW_MAX       = 4095;

// --- Calibração turbidez (0–800 NTU) ---
const float V0           = 0.850;
const float V800         = 0.582;
const float STANDARD_NTU = 800.0;
const int   N_SAMPLES    = 100;

// --- Calibração pH ---
const float PH_SLOPE     = -3.88391;
const float PH_INTERCEPT = 16.43099;

// --- DS18B20 (OneWire) ---
#define DS18B20_PIN 4
OneWire oneWire(DS18B20_PIN);
DallasTemperature sensors(&oneWire);

// --- WebServer e mDNS ---
WebServer server(80);
const char* MDNS_NAME = "esp32-sensor";  // acesso em http://esp32-sensor.local/

// Variáveis globais
float lastNtu       = 0.0;
float lastPh        = 0.0;
float lastTemp      = 0.0;
String lastInfluxStatus = "—";

void setup() {
  Serial.begin(115200);
  delay(100);

  // 1) Portal Wi-Fi
  WiFiManager wm;
  Serial.println("\n>> Iniciando ConfigPortal...");
  if (!wm.autoConnect("ConfigPortal")) {
    Serial.println("! Falha no Wi-Fi, reiniciando...");
    delay(3000);
    ESP.restart();
  }
  Serial.println("► Conectado em: " + WiFi.SSID());

  // 2) Sincronização NTP (UTC–3)
  configTime(-3 * 3600, 0, "pool.ntp.org", "a.st1.ntp.br");
  Serial.print("Aguardando NTP");
  while (time(nullptr) < 24 * 3600) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n► NTP sincronizado: " + nowString());

  // 3) mDNS
  if (MDNS.begin(MDNS_NAME)) {
    Serial.printf("mDNS ok: http://%s.local/\n", MDNS_NAME);
    MDNS.addService("http", "tcp", 80);
  } else {
    Serial.println("! Erro ao iniciar mDNS");
  }

  // 4) ADC e sensor de temperatura
  analogSetPinAttenuation(TURBIDITY_PIN, ADC_11db);
  analogSetPinAttenuation(PH_PIN,        ADC_11db);
  sensors.begin();

  // 5) WebServer
  server.on("/", HTTP_GET, handleRoot);
  server.begin();
  Serial.println(">> WebServer em: http://" + WiFi.localIP().toString() + "/");
}

void loop() {
  server.handleClient();

  // Leituras
  lastNtu  = readTurbidity();
  lastPh   = readPH();
  sensors.requestTemperatures();
  lastTemp = sensors.getTempCByIndex(0);

  Serial.printf("Turbidez: %.1f NTU, pH: %.2f, Temp: %.2f °C\n",
                lastNtu, lastPh, lastTemp);

  // Envio ao InfluxDB
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String url = String(INFLUXDB_URL)
               + "/api/v2/write?org=" + INFLUXDB_ORG
               + "&bucket=" + INFLUXDB_BUCKET
               + "&precision=" + INFLUXDB_PRECISION;
    http.begin(url);
    http.addHeader("Content-Type", "text/plain; charset=utf-8");
    http.addHeader("Authorization", INFLUXDB_TOKEN);

    String body = String("medidas ")
                + "ntu="  + String(lastNtu,1)  + ","
                + "ph="   + String(lastPh,2)   + ","
                + "temp=" + String(lastTemp,2);

    int code = http.POST(body);
    if (code == 204) {
      lastInfluxStatus = "Enviado ao InfluxDB";
      Serial.println("→ " + lastInfluxStatus);
    } else {
      lastInfluxStatus = "Erro HTTP: " + String(code);
      Serial.println("→ " + lastInfluxStatus);
    }
    http.end();
  } else {
    lastInfluxStatus = "Wi-Fi desconectado";
  }

  delay(5000);
}

float readTurbidity() {
  long sum = 0;
  for (int i = 0; i < N_SAMPLES; i++) {
    sum += analogRead(TURBIDITY_PIN);
    delay(2);
  }
  float v   = (sum / float(N_SAMPLES)) * VREF / RAW_MAX;
  return constrain((V0 - v) * STANDARD_NTU / (V0 - V800), 0.0, STANDARD_NTU);
}

float readPH() {
  int raw = analogRead(PH_PIN);
  float v = raw * VREF / RAW_MAX;
  return PH_SLOPE * v + PH_INTERCEPT;
}

void handleRoot() {
  String ip = WiFi.localIP().toString();

  String html = "<!DOCTYPE html><html><head>"
                "<meta charset='UTF-8'>"
                "<meta http-equiv='refresh' content='5'>"
                "<title>Sensores ESP32</title>"
                "<style>body{font-family:Arial;padding:16px;}"
                "h1{color:#333;}p{font-size:1.1em;}"
                "</style></head><body>"
                "<h1>Últimas Leituras</h1>"
                "<p><strong>Host:</strong> " + String(MDNS_NAME) + ".local</p>"
                "<p><strong>IP:</strong>   " + ip + "</p>"
                "<p><strong>Turbidez:</strong> " + String(lastNtu,1)  + " NTU</p>"
                "<p><strong>pH:</strong>       " + String(lastPh,2)   + "</p>"
                "<p><strong>Temp:</strong>     " + String(lastTemp,2) + " °C</p>"
                "<p><strong>Status Influx:</strong> " + lastInfluxStatus + "</p>"
                "<p><em>Atualizado em " + nowString() + "</em></p>"
                "</body></html>";

  server.send(200, "text/html", html);
}

String nowString() {
  time_t t = time(nullptr);
  struct tm* tmInfo = localtime(&t);
  char buf[16];
  sprintf(buf, "%02d:%02d:%02d", tmInfo->tm_hour, tmInfo->tm_min, tmInfo->tm_sec);
  return String(buf);
}
