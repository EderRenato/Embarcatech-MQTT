#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/i2c.h"
#include "lwip/apps/mqtt.h"
#include "lwip/apps/mqtt_priv.h"
#include "lwip/dns.h"
#include "lwip/altcp_tls.h"
#include "lib/ssd1306.h"
#include "lib/bh1750.h"
#include "lib/dht22.h"
#include "lib/rain_sensor.h"
#include "lib/buzzer.h"

#define WIFI_SSID "SeuSSID"
#define WIFI_PASSWORD "SuaSenha"
#define MQTT_SERVER "endereco_ip_do_broker" // Ex: "192.168.1.100"
#define MQTT_USERNAME "seu_usuario_mqtt"
#define MQTT_PASSWORD "sua_senha_mqtt"

// Pin configurations
#define I2C_OLED_PORT i2c1
#define I2C_OLED_SDA 15
#define I2C_OLED_SCL 14

#define I2C_BH1750_PORT i2c0
#define I2C_BH1750_SDA 8
#define I2C_BH1750_SCL 9

#define DHT22_PIN 16
#define LED_PIN CYW43_WL_GPIO_LED_PIN
#define RAIN_SENSOR_PIN 17

#define GREEN_LED 11
#define BLUE_LED 12
#define RED_LED 13

#define MIN_CRITICAL_TEMP 10.0
#define MAX_CRITICAL_TEMP 35.0
#define MIN_CRITICAL_HUMIDITY 30.0
#define MAX_CRITICAL_HUMIDITY 70.0
#define ALARM_CRITICAL_INTERVAL_MS 250
#define RAIN_ALARM_INTERVAL_MS 600

// Device instances
ssd1306_t oled;
bh1750_t light_sensor;
float temperature = 0;
float humidity = 0;
float luminosity = 0;
bool raining = false;
rain_sensor_t rain_sensor;

absolute_time_t last_alarm_time;
absolute_time_t last_rain_alarm_time;

#ifndef MQTT_SERVER
#error "MQTT_SERVER must be defined"
#endif

#ifdef MQTT_CERT_INC
#include MQTT_CERT_INC
#endif

#ifndef MQTT_TOPIC_LEN
#define MQTT_TOPIC_LEN 100
#endif

typedef struct {
    mqtt_client_t* mqtt_client_inst;
    struct mqtt_connect_client_info_t mqtt_client_info;
    char data[MQTT_OUTPUT_RINGBUF_SIZE];
    char topic[MQTT_TOPIC_LEN];
    uint32_t len;
    ip_addr_t mqtt_server_address;
    bool connect_done;
    int subscribe_count;
    bool stop_client;
} MQTT_CLIENT_DATA_T;

#ifndef DEBUG_printf
#ifndef NDEBUG
#define DEBUG_printf printf
#else
#define DEBUG_printf(...)
#endif
#endif

#ifndef INFO_printf
#define INFO_printf printf
#endif

#ifndef ERROR_printf
#define ERROR_printf printf
#endif

#define MQTT_KEEP_ALIVE_S 60
#define MQTT_SUBSCRIBE_QOS 1
#define MQTT_PUBLISH_QOS 1
#define MQTT_PUBLISH_RETAIN 0
#define MQTT_WILL_TOPIC "/online"
#define MQTT_WILL_MSG "0"
#define MQTT_WILL_QOS 1
#define MQTT_DEVICE_NAME "pico"
#define MQTT_UNIQUE_TOPIC 0

// Function prototypes
static void pub_request_cb(__unused void *arg, err_t err);
static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name);
static void sub_request_cb(void *arg, err_t err);
static void unsub_request_cb(void *arg, err_t err);
static void sub_unsub_topics(MQTT_CLIENT_DATA_T* state, bool sub);
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);
static void start_client(MQTT_CLIENT_DATA_T *state);
static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg);
static bool check_critical_conditions(void);
void init_all_devices();
void update_display(float lux, float temp, float hum, bool rain);
void publish_sensor_data(MQTT_CLIENT_DATA_T *state, float temp, float hum, float lux, bool rain);

int main() {
    sleep_ms(1000);
    stdio_init_all();
    printf("Iniciando o sistema...\n");

    init_all_devices();
    printf("Dispositivos inicializados.\n");

    // Create client state
    static MQTT_CLIENT_DATA_T state = {0};

    // Initialize cyw43 architecture
    if (cyw43_arch_init()) {
        panic("Failed to initialize CYW43");
    }

    // Get unique board ID
    char unique_id_buf[5];
    pico_get_unique_board_id_string(unique_id_buf, sizeof(unique_id_buf));
    for(int i=0; i < sizeof(unique_id_buf) - 1; i++) {
        unique_id_buf[i] = tolower(unique_id_buf[i]);
    }

    // Generate unique client ID
    char client_id_buf[sizeof(MQTT_DEVICE_NAME) + sizeof(unique_id_buf) - 1];
    snprintf(client_id_buf, sizeof(client_id_buf), "%s%s", MQTT_DEVICE_NAME, unique_id_buf);
    INFO_printf("Device name %s\n", client_id_buf);

    // Configure MQTT client
    state.mqtt_client_info.client_id = client_id_buf;
    state.mqtt_client_info.keep_alive = MQTT_KEEP_ALIVE_S;
    state.mqtt_client_info.client_user = MQTT_USERNAME;
    state.mqtt_client_info.client_pass = MQTT_PASSWORD;
    
    char will_topic[MQTT_TOPIC_LEN];
    snprintf(will_topic, sizeof(will_topic), "%s%s", full_topic(&state, MQTT_WILL_TOPIC));
    state.mqtt_client_info.will_topic = will_topic;
    state.mqtt_client_info.will_msg = MQTT_WILL_MSG;
    state.mqtt_client_info.will_qos = MQTT_WILL_QOS;
    state.mqtt_client_info.will_retain = true;

    // Connect to WiFi
    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        panic("Failed to connect");
    }
    INFO_printf("\nConnected to Wifi\n");

    // DNS lookup for MQTT server
    cyw43_arch_lwip_begin();
    int err = dns_gethostbyname(MQTT_SERVER, &state.mqtt_server_address, dns_found, &state);
    cyw43_arch_lwip_end();

    if (err == ERR_OK) {
        start_client(&state);
    } else if (err != ERR_INPROGRESS) {
        panic("dns request failed");
    }

    // Sensor timing variables
    absolute_time_t last_bh1750_read = get_absolute_time();
    absolute_time_t last_dht22_read = get_absolute_time();
    absolute_time_t last_rain_sensor_read = get_absolute_time();
    absolute_time_t last_mqtt_publish = get_absolute_time();
    last_alarm_time = nil_time;
    last_rain_alarm_time = nil_time;

    while (true) {
        // Read BH1750 light sensor
        if (absolute_time_diff_us(last_bh1750_read, get_absolute_time()) > 2000000) {
            bh1750_start_measurement(&light_sensor);
            sleep_ms(180);
            if (bh1750_read_result(&light_sensor, &luminosity)) {
                printf("Luminosity: %.2f lux\n", luminosity);
            } else {
                printf("BH1750 read failed\n");
                luminosity = -1;
            }
            last_bh1750_read = get_absolute_time();
        }

        // Read DHT22 sensor
        if (absolute_time_diff_us(last_dht22_read, get_absolute_time()) > 3000000) {
            if (read_dht22(DHT22_PIN, &temperature, &humidity)) {
                printf("Temperature: %.1fC, Humidity: %.1f%%\n", temperature, humidity);
            } else {
                printf("DHT22 read failed\n");
            }
            last_dht22_read = get_absolute_time();
        }

        // Read rain sensor
        if (absolute_time_diff_us(last_rain_sensor_read, get_absolute_time()) > 2000000) {
            raining = rain_sensor_read_digital(&rain_sensor);
            printf("Rain status: %s\n", raining ? "Raining" : "Not raining");
            last_rain_sensor_read = get_absolute_time();
        }

        // Check critical conditions and trigger alarms
        if (check_critical_conditions()) {
            absolute_time_t now = get_absolute_time();
            if (is_nil_time(last_alarm_time) || 
                absolute_time_diff_us(last_alarm_time, now) >= ALARM_CRITICAL_INTERVAL_MS * 1000) {
                play_alarm_critic();
                last_alarm_time = now;
                gpio_put(RED_LED, true);
            }
        } else {
            last_alarm_time = nil_time;
            gpio_put(RED_LED, false);
        }

        // Rain alarm
        if (raining) {
            absolute_time_t now = get_absolute_time();
            if (is_nil_time(last_rain_alarm_time) || 
                absolute_time_diff_us(last_rain_alarm_time, now) >= RAIN_ALARM_INTERVAL_MS * 1000) {
                play_alarm_rain();
                last_rain_alarm_time = now;
                gpio_put(BLUE_LED, true);
                gpio_put(GREEN_LED, false);
            }
        } else {
            last_rain_alarm_time = nil_time;
            gpio_put(BLUE_LED, false);
        }

        // Normal condition LED
        if (check_critical_conditions() || raining) {
            gpio_put(GREEN_LED, false);
        } else {
            gpio_put(GREEN_LED, true);
        }

        // Update display
        update_display(luminosity, temperature, humidity, raining);

        // Publish sensor data every 10 seconds if connected
        if (state.connect_done && mqtt_client_is_connected(state.mqtt_client_inst)) {
            if (absolute_time_diff_us(last_mqtt_publish, get_absolute_time()) > 10000000) {
                publish_sensor_data(&state, temperature, humidity, luminosity, raining);
                last_mqtt_publish = get_absolute_time();
            }
        }

        // Handle MQTT events
        cyw43_arch_poll();
        sleep_ms(100);
    }
}

void publish_sensor_data(MQTT_CLIENT_DATA_T *state, float temp, float hum, float lux, bool rain) {
    char topic[100];
    char payload[20];
    
    // Publish temperature
    snprintf(topic, sizeof(topic), "%s/sensor/temperature", full_topic(state, ""));
    snprintf(payload, sizeof(payload), "%.1f", temp);
    mqtt_publish(state->mqtt_client_inst, topic, payload, strlen(payload), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    
    // Publish humidity
    snprintf(topic, sizeof(topic), "%s/sensor/humidity", full_topic(state, ""));
    snprintf(payload, sizeof(payload), "%.1f", hum);
    mqtt_publish(state->mqtt_client_inst, topic, payload, strlen(payload), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    
    // Publish luminosity
    snprintf(topic, sizeof(topic), "%s/sensor/luminosity", full_topic(state, ""));
    snprintf(payload, sizeof(payload), "%.2f", lux);
    mqtt_publish(state->mqtt_client_inst, topic, payload, strlen(payload), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    
    // Publish rain status
    snprintf(topic, sizeof(topic), "%s/sensor/rain", full_topic(state, ""));
    snprintf(payload, sizeof(payload), "%d", rain ? 1 : 0);
    mqtt_publish(state->mqtt_client_inst, topic, payload, strlen(payload), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
}

// Publish request callback
static void pub_request_cb(__unused void *arg, err_t err) {
    if (err != ERR_OK) {
        ERROR_printf("Publish failed: %d\n", err);
    }
}

// Generate full MQTT topic with client ID prefix if MQTT_UNIQUE_TOPIC is enabled
static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name) {
#if MQTT_UNIQUE_TOPIC
    static char full_topic[MQTT_TOPIC_LEN];
    snprintf(full_topic, sizeof(full_topic), "/%s%s", state->mqtt_client_info.client_id, name);
    return full_topic;
#else
    return name;
#endif
}

// Start MQTT client connection
static void start_client(MQTT_CLIENT_DATA_T *state) {
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    const int port = MQTT_TLS_PORT;
    INFO_printf("Using TLS\n");
#else
    const int port = MQTT_PORT;
    INFO_printf("Warning: Not using TLS\n");
#endif

    state->mqtt_client_inst = mqtt_client_new();
    if (!state->mqtt_client_inst) {
        panic("MQTT client instance creation error");
    }

    INFO_printf("Connecting to MQTT server at %s\n", ipaddr_ntoa(&state->mqtt_server_address));

    cyw43_arch_lwip_begin();
    if (mqtt_client_connect(state->mqtt_client_inst, &state->mqtt_server_address, port, 
                          mqtt_connection_cb, state, &state->mqtt_client_info) != ERR_OK) {
        panic("MQTT broker connection error");
    }
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    // This is important for MBEDTLS_SSL_SERVER_NAME_INDICATION
    mbedtls_ssl_set_hostname(altcp_tls_context(state->mqtt_client_inst->conn), MQTT_SERVER);
#endif
    mqtt_set_inpub_callback(state->mqtt_client_inst, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, state);
    cyw43_arch_lwip_end();
}

// DNS resolution callback
static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T*)arg;
    if (ipaddr) {
        state->mqtt_server_address = *ipaddr;
        start_client(state);
    } else {
        panic("DNS request failed for hostname: %s", hostname);
    }
}

// Check if sensor values are in critical ranges
static bool check_critical_conditions(void) {
    return ((temperature >= MAX_CRITICAL_TEMP || temperature <= MIN_CRITICAL_TEMP) ||
            (humidity >= MAX_CRITICAL_HUMIDITY || humidity <= MIN_CRITICAL_HUMIDITY));
}

void init_all_devices() {
    // Initialize I2C for OLED
    i2c_init(I2C_OLED_PORT, 400000);
    gpio_set_function(I2C_OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_OLED_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_OLED_SDA);
    gpio_pull_up(I2C_OLED_SCL);

    // Initialize I2C for BH1750
    i2c_init(I2C_BH1750_PORT, 400000);
    gpio_set_function(I2C_BH1750_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_BH1750_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_BH1750_SDA);
    gpio_pull_up(I2C_BH1750_SCL);

    // Initialize OLED
    ssd1306_init(&oled, 128, 64, false, 0x3C, I2C_OLED_PORT);
    ssd1306_config(&oled);
    ssd1306_fill(&oled, false);
    ssd1306_send_data(&oled);

    // Initialize BH1750
    bh1750_init(&light_sensor, I2C_BH1750_PORT, BH1750_ADDR_LOW, BH1750_CONT_HIGH_RES_MODE);

    // Initialize Rain Sensor
    rain_sensor_init_digital(&rain_sensor, RAIN_SENSOR_PIN);

    init_buzzer_pwm(BUZZER_A);
    init_buzzer_pwm(BUZZER_B);

    gpio_init(GREEN_LED);
    gpio_init(BLUE_LED);
    gpio_init(RED_LED);
    gpio_set_dir(GREEN_LED, GPIO_OUT);
    gpio_set_dir(BLUE_LED, GPIO_OUT);
    gpio_set_dir(RED_LED, GPIO_OUT);
    gpio_pull_up(GREEN_LED);
    gpio_pull_up(BLUE_LED);
    gpio_pull_up(RED_LED);
}

static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (status == MQTT_CONNECT_ACCEPTED) {
        state->connect_done = true;
        sub_unsub_topics(state, true); // subscribe;

        // indicate online
        if (state->mqtt_client_info.will_topic) {
            mqtt_publish(state->mqtt_client_inst, state->mqtt_client_info.will_topic, "1", 1, 
                        MQTT_WILL_QOS, true, pub_request_cb, state);
        }

        // Publish initial sensor data
        publish_sensor_data(state, temperature, humidity, luminosity, raining);
    } 
    else if (status == MQTT_CONNECT_DISCONNECTED) {
        if (!state->connect_done) {
            panic("Failed to connect to MQTT server");
        }
        ERROR_printf("MQTT connection disconnected\n");
    }
    else {
        panic("Unexpected MQTT connection status: %d", status);
    }
}
// Tópicos de assinatura
static void sub_unsub_topics(MQTT_CLIENT_DATA_T* state, bool sub) {
    mqtt_request_cb_t cb = sub ? sub_request_cb : unsub_request_cb;
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/led"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/print"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/ping"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/exit"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
}

// Dados de entrada MQTT
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
#if MQTT_UNIQUE_TOPIC
    const char *basic_topic = state->topic + strlen(state->mqtt_client_info.client_id) + 1;
#else
    const char *basic_topic = state->topic;
#endif
    strncpy(state->data, (const char *)data, len);
    state->len = len;
    state->data[len] = '\0';

    DEBUG_printf("Topic: %s, Message: %s\n", state->topic, state->data);
    
    if (strcmp(basic_topic, "/print") == 0) {
        INFO_printf("%.*s\n", len, data);
    } 
    else if (strcmp(basic_topic, "/ping") == 0) {
        char buf[11];
        snprintf(buf, sizeof(buf), "%u", to_ms_since_boot(get_absolute_time()) / 1000);
        mqtt_publish(state->mqtt_client_inst, full_topic(state, "/uptime"), buf, strlen(buf), 
                    MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    } 
    else if (strcmp(basic_topic, "/exit") == 0) {
        state->stop_client = true;
        sub_unsub_topics(state, false);
    }
}

// Dados de entrada publicados
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    strncpy(state->topic, topic, sizeof(state->topic));
}

// Requisição de Assinatura - subscribe
static void sub_request_cb(void *arg, err_t err) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (err != 0) {
        panic("subscribe request failed %d", err);
    }
    state->subscribe_count++;
}

// Requisição para encerrar a assinatura
static void unsub_request_cb(void *arg, err_t err) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (err != 0) {
        panic("unsubscribe request failed %d", err);
    }
    state->subscribe_count--;
    assert(state->subscribe_count >= 0);

    // Stop if requested
    if (state->subscribe_count <= 0 && state->stop_client) {
        mqtt_disconnect(state->mqtt_client_inst);
    }
}

void update_display(float lux, float temp, float hum, bool rain) {
    char buffer[32];
    ssd1306_fill(&oled, false);
    
    snprintf(buffer, sizeof(buffer), "Luz: %.2f lux", lux);
    ssd1306_draw_string(&oled, buffer, 1, 10);
    
    snprintf(buffer, sizeof(buffer), "Temp: %.1fC", temp);
    ssd1306_draw_string(&oled, buffer, 1, 25);
    
    snprintf(buffer, sizeof(buffer), "Umidade: %.1f%%", hum);
    ssd1306_draw_string(&oled, buffer, 1, 40);
    
    snprintf(buffer, sizeof(buffer), "Chuva: %s", rain ? "Sim" : "Nao");
    ssd1306_draw_string(&oled, buffer, 1, 55);
    
    ssd1306_send_data(&oled);
}