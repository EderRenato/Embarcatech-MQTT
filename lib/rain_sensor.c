// rain_sensor.c - Implementação da biblioteca do sensor de chuva
#include "rain_sensor.h"

void rain_sensor_init_digital(rain_sensor_t *sensor, uint digital_pin) {
    sensor->digital_pin = digital_pin;
    sensor->analog_pin = 0;
    sensor->is_analog_initialized = false;
    sensor->rain_threshold = 0.5f; // Valor padrão
    
    gpio_init(digital_pin);
    gpio_set_dir(digital_pin, GPIO_IN);
    gpio_pull_up(digital_pin); // Normalmente o sensor tem saída em pull-up
}

void rain_sensor_init_analog(rain_sensor_t *sensor, uint analog_pin, float threshold) {
    sensor->digital_pin = 0;
    sensor->analog_pin = analog_pin;
    sensor->is_analog_initialized = true;
    sensor->rain_threshold = threshold;
    
    adc_init();
    adc_gpio_init(analog_pin);
}

void rain_sensor_init_dual(rain_sensor_t *sensor, uint digital_pin, uint analog_pin, float threshold) {
    sensor->digital_pin = digital_pin;
    sensor->analog_pin = analog_pin;
    sensor->is_analog_initialized = true;
    sensor->rain_threshold = threshold;
    
    gpio_init(digital_pin);
    gpio_set_dir(digital_pin, GPIO_IN);
    gpio_pull_up(digital_pin);
    
    adc_init();
    adc_gpio_init(analog_pin);
}

bool rain_sensor_read_digital(rain_sensor_t *sensor) {
    return !gpio_get(sensor->digital_pin); // Invertido porque normalmente HIGH quando seco
}

float rain_sensor_read_analog(rain_sensor_t *sensor) {
    if (!sensor->is_analog_initialized) return 0.0f;
    
    adc_select_input(sensor->analog_pin - 26); // Converte pino GPIO para canal ADC
    uint16_t raw = adc_read();
    return (float)raw / 4095.0f; // Converte para valor entre 0.0 e 1.0
}

bool rain_sensor_is_raining(rain_sensor_t *sensor) {
    if (sensor->is_analog_initialized) {
        float value = rain_sensor_read_analog(sensor);
        return value > sensor->rain_threshold;
    } else if (sensor->digital_pin > 0) {
        return rain_sensor_read_digital(sensor);
    }
    return false;
}

void rain_sensor_set_threshold(rain_sensor_t *sensor, float new_threshold) {
    sensor->rain_threshold = new_threshold;
}