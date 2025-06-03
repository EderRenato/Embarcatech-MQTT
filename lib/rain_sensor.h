// rain_sensor.h - Biblioteca para Sensor de Chuva Digital/Analógico
// Autor: Eder Renato
// Data: 17/05/2025

#ifndef RAIN_SENSOR_H
#define RAIN_SENSOR_H

#include "pico/stdlib.h"
#include "hardware/adc.h"

typedef struct {
    uint digital_pin;
    uint analog_pin;
    bool is_analog_initialized;
    float rain_threshold;
} rain_sensor_t;

// Inicializa o sensor no modo digital
void rain_sensor_init_digital(rain_sensor_t *sensor, uint digital_pin);

// Inicializa o sensor no modo analógico
void rain_sensor_init_analog(rain_sensor_t *sensor, uint analog_pin, float threshold);

// Inicializa o sensor para uso em ambos os modos
void rain_sensor_init_dual(rain_sensor_t *sensor, uint digital_pin, uint analog_pin, float threshold);

// Lê o estado digital do sensor (true = chuva detectada)
bool rain_sensor_read_digital(rain_sensor_t *sensor);

// Lê o valor analógico do sensor (0.0 a 1.0, onde 1.0 = máximo de água)
float rain_sensor_read_analog(rain_sensor_t *sensor);

// Verifica se está chovendo com base no limiar analógico
bool rain_sensor_is_raining(rain_sensor_t *sensor);

// Define um novo limiar para detecção de chuva no modo analógico
void rain_sensor_set_threshold(rain_sensor_t *sensor, float new_threshold);

#endif // RAIN_SENSOR_H