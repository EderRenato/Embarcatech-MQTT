#ifndef DHT22_H
#define DHT22_H

#include "pico/stdlib.h"

typedef struct {
    float temp;
    float hum;
} dht_data_t;

bool read_dht22(uint gpio, float *temperature, float *humidity);

#endif