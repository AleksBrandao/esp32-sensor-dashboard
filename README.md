# ESP32 Sensor Web Dashboard

Este projeto utiliza um ESP32 para ler sensores de turbidez, pH e temperatura (DS18B20), enviar os dados para o InfluxDB Cloud e servir uma página web local mostrando as últimas leituras, status de envio e informações de calibração. A configuração de rede Wi-Fi é feita via portal interativo usando a biblioteca WiFiManager e o acesso à página é facilitado por mDNS.

---

## Índice

1. [Características](#características)
2. [Requisitos de Hardware](#requisitos-de-hardware)
3. [Requisitos de Software](#requisitos-de-software)
4. [Estrutura de Arquivos](#estrutura-de-arquivos)
5. [Configuração de Credenciais (`secrets.h`)](#configuração-de-credenciais-secretsh)
6. [Sketch Principal (`.ino`)](#sketch-principal-ino)
7. [Configuração de Wi-Fi com WiFiManager](#configuração-de-wi-fi-com-wifimanager)
8. [Sincronização de Horário (NTP)](#sincronização-de-horário-ntp)
9. [Acesso à Página Web](#acesso-à-página-web)
10. [Resolução via mDNS](#resolução-via-mdns)
11. [Envio de Dados para InfluxDB](#envio-de-dados-para-influxdb)
12. [Calibração dos Sensores](#calibração-dos-sensores)
13. [Customizações](#customizações)
14. [Licença](#licença)

---

## Características

* Leitura de sensores:

  * Turbidez (ADC)
  * pH (ADC)
  * Temperatura DS18B20 (OneWire)
* Portal de configuração Wi-Fi via **WiFiManager** (AP e captive portal)
* Sincronização de hora local (UTC−3) por NTP
* Servidor HTTP embutido que exibe as últimas leituras, hora, status e calibrações
* Resolução de nome local via **mDNS** (ex.: `esp32-sensor.local`)
* Envio periódico (a cada 5s) de dados para **InfluxDB Cloud**

## Requisitos de Hardware

* **Placa ESP32** ESP-WROOM-32 WiFi Bluetooth (Wemos Lolin D32 V1 ou similar)

* [Link do módulo](https://www.saravati.com.br/placa-esp32-esp-wroom-32-wifi-bluetooth-wemos-lolin-d32-v1.html)
* **Módulo Sensor de Turbidez (Partículas Suspensas na Água)** conectado ao pino ADC1\_CH6 (GPIO34)

  * [Link do módulo](https://www.saravati.com.br/modulo-sensor-de-turbidez-particulas-suspensas-na-agua.html)
* **Sensor PH (Módulo PH4502C + Eletrodo)** conectado ao pino ADC1\_CH7 (GPIO35)

  * [Link do módulo](https://www.saravati.com.br/sensor-ph-modulo-ph4502c-ph-eletrodo.html)
* **Sensor de Temperatura Digital à Prova D'água DS18B20** conectado ao GPIO4

  * [Link do módulo](https://www.saravati.com.br/sensor-de-temperatura-digital-a-prova-dagua-ds18b20-3m.html)
* **Cabo micro-USB** para alimentação e programação

## Requisitos de Software

* Arduino IDE (versão compatível com ESP32)
* Biblioteca **WiFiManager** by tzapu
* Biblioteca **ESPmDNS**
* Biblioteca **WebServer**
* Biblioteca **HTTPClient**
* Biblioteca **OneWire**
* Biblioteca **DallasTemperature**

> Instale tudo pelo **Library Manager** do Arduino IDE.

## Estrutura de Arquivos

```
/MeuProjetoESP32/
│
├─ codigo_ph_e_turbidez_temperatura_db_secrets_webpage.ino  # Sketch principal
└─ secrets.h                                              # Credenciais (gitignored)
```

## Configuração de Credenciais (`secrets.h`)

```cpp
#ifndef SECRETS_H
#define SECRETS_H

// InfluxDB Cloud\ #define INFLUXDB_URL       "https://us-east-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN     "Token _SEU_TOKEN_AQUI_"
#define INFLUXDB_ORG       "SEU_ORG_ID"
#define INFLUXDB_BUCKET    "SEU_BUCKET"
#define INFLUXDB_PRECISION "ns"

#endif // SECRETS_H
```

> **Importante:** adicione este arquivo ao `.gitignore` para proteger suas credenciais.

## Sketch Principal (`.ino`)

Veja no arquivo `codigo_ph_e_turbidez_temperatura_db_secrets_webpage.ino`:

* Inclusão de bibliotecas e definição de pinos/calibrações
* Variáveis globais para últimas leituras, status de InfluxDB e horário
* `setup()`:

  1. Portal de configuração Wi-Fi via WiFiManager
  2. Sincronização NTP para hora local (UTC−3)
  3. Inicialização de mDNS
  4. Configuração de ADC e sensores OneWire
  5. Inicialização do WebServer e rota `/`
* `loop()`:

  1. `server.handleClient()` para atender requisições
  2. Leitura dos sensores (turbidez, pH, temperatura)
  3. Envio para InfluxDB e atualização de `lastInfluxStatus`
  4. Delay de 5 segundos
* `handleRoot()`:

  * Gera página HTML com:

    * Hostname (`MDNS_NAME`) e IP
    * Últimas leituras de turbidez, pH e temperatura
    * Status do envio ao InfluxDB
    * Hora local formatada (`nowString()`)
    * Valores de calibração atuais

## Configuração de Wi-Fi com WiFiManager

* Na primeira inicialização, o ESP32 cria o AP **ConfigPortal**.
* Conecte-se no smartphone/PC ao Wi-Fi `ConfigPortal`.
* Acesse `http://192.168.4.1/` (ou abra qualquer página para captive portal).
* Selecione sua rede, insira a senha e clique em **Save**.
* As credenciais são salvas internamente (NVS) e o ESP32 reinicia.
* Em inicializações subsequentes, conecta-se automaticamente sem portal.

## Sincronização de Horário (NTP)

```cpp
configTime(-3 * 3600, 0, "pool.ntp.org", "a.st1.ntp.br");
```

* Ajusta o relógio interno para GMT−3 (horário de Brasília).
* Aguarda até que o timestamp seja válido (>1 dia em segundos).
* `nowString()` usa `localtime()` para retornar `HH:MM:SS`.

## Acesso à Página Web

* Após o setup, o Serial Monitor mostrará:

  ```
  >> WebServer em: http://<IP_DO_ESP32>/
  ```
* Abra no navegador (PC ou celular):

  * Por IP: `http://192.168.x.y/`
  * Ou, via mDNS (se suportado): `http://esp32-sensor.local/`
* A página faz **auto-refresh** a cada 5 segundos e exibe as medições.

## Resolução via mDNS

```cpp
ESPmDNS.begin("esp32-sensor");
MDNS.addService("http", "tcp", 80);
```

* Permite usar `http://esp32-sensor.local/` em dispositivos que suportem mDNS.
* No Windows, instale o Bonjour Print Services; em celulares, use apps de descoberta.

## Envio de Dados para InfluxDB

* Formato de *Line Protocol*:

  ```plaintext
  medidas ntu=XX.X,ph=YY.YY,temp=ZZ.ZZ
  ```
* HTTP POST para:

  ```
  <INFLUXDB_URL>/api/v2/write?org=<ORG>&bucket=<BUCKET>&precision=ns
  ```
* Cabeçalhos:

  * `Content-Type: text/plain; charset=utf-8`
  * `Authorization: Token <TOKEN>`
* `lastInfluxStatus` mostra sucesso ou código de erro.

## Calibração dos Sensores

* **Turbidez**:

  * 0 NTU em **água potável** (V0 = 0.850)
  * 800 NTU em **café 2 g/L** (V800 = 0.582)
* **pH**:

  * Calibração com **solução padrão pH 7** e **pH 4**
  * Coeficientes usados: `PH_SLOPE = -3.88391`, `PH_INTERCEPT = 16.43099`

## Customizações

* **Valores de calibração**: ajuste `V0`, `V800`, `PH_SLOPE` e `PH_INTERCEPT` conforme seus ensaios.
* **Intervalo de leitura**: altere `delay(5000)` e `meta-refresh` no HTML.
* **Hostname mDNS**: modifique `MDNS_NAME` para outro identificador.
* **Estilo da página**: edite o CSS em `handleRoot()`.

## Licença

Este projeto está disponível sob a licença MIT. Use, modifique e distribua livremente.
