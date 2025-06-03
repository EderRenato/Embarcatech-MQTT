# Pico W - Estação de Monitoramento Ambiental com MQTT

[![GitHub](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
![FreeRTOS](https://img.shields.io/badge/FreeRTOS-v10.4.3-green)
![Platform](https://img.shields.io/badge/Platform-Raspberry%20Pi%20Pico-red)
[![Google Drive](https://img.shields.io/badge/Demo-Google%20Drive-blue?logo=google-drive)](https://drive.google.com/file/d/1z4WgWUNK8RuMyGJe0-kG4ZnRtUBp1WBp/view?usp=sharing)

## Descrição do Projeto

Este projeto utiliza um **Raspberry Pi Pico W** para coletar dados de diversos sensores ambientais, exibi-los em um display OLED e publicá-los em um broker MQTT. Inclui alertas sonoros e visuais para condições críticas e detecção de chuva.

---

## ✨ Funcionalidades

- 🌡️ **Leitura de temperatura e umidade** (Sensor DHT22)
- 💡 **Leitura de luminosidade ambiente** (Sensor BH1750)
- 💧 **Detecção de chuva** (Sensor de Chuva)
- 📺 **Exibição dos dados** dos sensores em um display OLED SSD1306
- 🌐 **Conexão Wi-Fi** para comunicação MQTT
- 🚀 **Publicação dos dados** dos sensores para um broker MQTT
- 🔔 **Subscrição a tópicos MQTT** para comandos remotos (ex: `/ping`, `/print`, `/exit`)
- 🚨 **Alertas locais** (LEDs e buzzer) para:
    - Condições críticas de temperatura e umidade
    - Detecção de chuva
- 🟢 **LED de status** para indicar operação normal, alerta crítico ou chuva

---

## 🛠️ Hardware Necessário

- Raspberry Pi Pico W
- Sensor de Temperatura e Umidade DHT22
- Sensor de Luminosidade BH1750 (I2C)
- Display OLED SSD1306 128x64 (I2C)
- Sensor de Chuva (Digital)
- Buzzer (controlado por PWM)
- LEDs:
    - 1x LED Verde
    - 1x LED Azul
    - 1x LED Vermelho
- Protoboard e fios de jumper

---

## 🔧 Configuração (Firmware Pico W)

Antes de compilar e executar o firmware no Pico W, configure no código-fonte (`main.c` ou arquivo de configuração):

**Credenciais Wi-Fi:**
```c
#define WIFI_SSID "SeuSSID"
#define WIFI_PASSWORD "SuaSenha"
```

**Detalhes do Servidor MQTT:**
```c
#define MQTT_SERVER "endereco_ip_do_broker" // Ex: "192.168.0.XX"
#define MQTT_USERNAME "seu_usuario_mqtt"
#define MQTT_PASSWORD "sua_senha_mqtt"
```

**Limites Críticos (opcional):**
```c
#define MIN_CRITICAL_TEMP 10.0
#define MAX_CRITICAL_TEMP 35.0
#define MIN_CRITICAL_HUMIDITY 30.0
#define MAX_CRITICAL_HUMIDITY 70.0
```

---

## 🦟 Broker MQTT (Mosquitto no Termux Android) - Configuração

Você precisará de um broker MQTT para receber os dados do Pico W. O Mosquitto pode ser instalado e executado no Termux em seu dispositivo Android.

### 1. Instalação no Termux

```sh
pkg update && pkg upgrade -y
pkg install mosquitto -y
```

### 2. Configuração de Usuário e Senha

Crie o diretório de configuração do Mosquitto (se necessário):

```sh
mkdir -p $PREFIX/etc/mosquitto
```

Crie um arquivo de senha:

```sh
mosquitto_passwd -c $PREFIX/etc/mosquitto/passwd seu_usuario_mqtt
```

Edite o arquivo de configuração:

```sh
nano $PREFIX/etc/mosquitto/mosquitto.conf
```

Adicione:

```
allow_anonymous false
password_file $PREFIX/etc/mosquitto/passwd
listener 1883 0.0.0.0
```

### 3. Executando o Mosquitto

```sh
mosquitto -c $PREFIX/etc/mosquitto/mosquitto.conf
# Para rodar em segundo plano:
mosquitto -c $PREFIX/etc/mosquitto/mosquitto.conf -d
# Para logs verbosos:
mosquitto -c $PREFIX/etc/mosquitto/mosquitto.conf -v
```

> **Dica:** Use `termux-wake-lock` para evitar que o Android feche o processo.

### 4. Descobrindo o IP do Celular

No Termux:

```sh
ifconfig
```
Procure pela interface `wlan0` e anote o endereço `inet`.

### 5. Teste (Opcional)

Em outra sessão do Termux:

```sh
mosquitto_sub -h localhost -t "#" -u "seu_usuario_mqtt" -P "sua_senha_mqtt" -v
```

---

## 📱 Painel de Monitoramento (IoT MQTT Panel)

O **IoT MQTT Panel** é um aplicativo móvel (Android/iOS) para visualizar dados MQTT e enviar comandos.

### 1. Instalação

Baixe o "IoT MQTT Panel" na Google Play Store ou Apple App Store.

### 2. Conexão com o Broker

- Name: Nome da conexão (ex: "PicoW Estação")
- Address: IP do celular Android (igual ao `MQTT_SERVER`)
- Port: 1883
- Client ID: Padrão ou personalizado
- Username: `seu_usuario_mqtt`
- Password: `sua_senha_mqtt`

### 3. Adicionando Painéis (Widgets)

Configure widgets para cada dado do sensor:

- **Temperatura**
    - Type: Gauge, Text Output ou Line Graph
    - Topic: `/sensor/temperature`
    - Unit: °C

- **Umidade**
    - Type: Gauge, Text Output ou Line Graph
    - Topic: `/sensor/humidity`
    - Unit: %

- **Luminosidade**
    - Type: Text Output ou Line Graph
    - Topic: `/sensor/luminosity`
    - Unit: lux

- **Chuva**
    - Type: Light, Switch ou Text Output
    - Topic: `/sensor/rain`
    - On value: 1
    - Off value: 0

- **Comandos**
    - Exemplo `/ping`:
        - Type: Button
        - Topic (Publish): `/ping`
        - Payload: 1

    - Para resposta `/uptime`:
        - Type: Text Output
        - Topic (Subscribe): `/uptime`

---

## 📌 Pinos Utilizados (Pico W)

| Componente                | Pino Pico W         | Observação                  |
|---------------------------|---------------------|-----------------------------|
| Display OLED (SSD1306)    | SDA: GP15, SCL: GP14| I2C1                        |
| Sensor BH1750             | SDA: GP8, SCL: GP9  | I2C0                        |
| Sensor DHT22              | DATA: GP16          |                             |
| Sensor de Chuva           | DO: GP17            |                             |
| LED Verde                 | GP11                |                             |
| LED Azul                  | GP12                |                             |
| LED Vermelho              | GP13                |                             |
| LED Onboard               | CYW43_WL_GPIO_LED_PIN| Controlado via cyw43_arch   |
| Buzzer                    | Definido em lib/buzzer.h | Verifique a biblioteca |

---

## 📚 Bibliotecas (Firmware Pico W)

- `pico/stdlib.h`
- `pico/cyw43_arch.h`
- `pico/unique_id.h`
- `hardware/gpio.h`
- `hardware/irq.h`
- `hardware/i2c.h`
- `lwip/apps/mqtt.h`
- `lwip/dns.h`
- `lwip/altcp_tls.h`

**Bibliotecas personalizadas (em `lib/`):**

- `lib/ssd1306.h`
- `lib/bh1750.h`
- `lib/dht22.h`
- `lib/rain_sensor.h`
- `lib/buzzer.h`

Certifique-se de incluir e configurar corretamente no `CMakeLists.txt`.

---

## MQTT (Firmware Pico W)

O dispositivo interage com o broker MQTT publicando e subscrevendo tópicos para os dados dos sensores e comandos remotos.

---

## 📄 Licença

Este projeto está licenciado sob a licença MIT. Consulte o arquivo [LICENSE](LICENSE) para mais detalhes.
