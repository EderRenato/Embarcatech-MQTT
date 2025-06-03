#include "dht22.h"

bool read_dht22(uint gpio, float *temperature, float *humidity) {
    int data[5] = {0};

    // Inicializa o pino
    gpio_init(gpio);
    gpio_set_dir(gpio, GPIO_OUT);
    gpio_put(gpio, 0);
    sleep_ms(2);  // start signal > 1ms
    gpio_put(gpio, 1);
    sleep_us(30);
    gpio_set_dir(gpio, GPIO_IN);

    // Espera resposta do sensor
    int count = 0;
    while (gpio_get(gpio) == 1) {
        if (++count > 10000) return false;
        sleep_us(1);
    }

    count = 0;
    while (gpio_get(gpio) == 0) {
        if (++count > 10000) return false;
        sleep_us(1);
    }

    count = 0;
    while (gpio_get(gpio) == 1) {
        if (++count > 10000) return false;
        sleep_us(1);
    }

    // Lê os 40 bits (5 bytes)
    for (int i = 0; i < 40; i++) {
        // Espera começar o bit
        while (gpio_get(gpio) == 0);

        // Mede o tempo do pulso
        count = 0;
        while (gpio_get(gpio) == 1) {
            count++;
            sleep_us(1);
            if (count > 100) break;
        }

        int byte_idx = i / 8;
        data[byte_idx] <<= 1;
        if (count > 40)  // valor empírico
            data[byte_idx] |= 1;
    }

    // Checa o checksum
    int sum = data[0] + data[1] + data[2] + data[3];
    if ((sum & 0xFF) != data[4]) return false;

    // Converte
    *humidity = ((data[0] << 8) + data[1]) / 10.0;
    *temperature = ((data[2] & 0x7F) << 8 | data[3]) / 10.0;
    if (data[2] & 0x80) *temperature = -*temperature;

    return true;
}
