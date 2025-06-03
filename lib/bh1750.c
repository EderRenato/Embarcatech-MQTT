#include "bh1750.h"
#include "hardware/gpio.h"

// Tempos de medição mínimos em ms (datasheet page 5)
static const uint16_t measurement_times[] = {
    [BH1750_CONT_HIGH_RES_MODE] = 120,
    [BH1750_CONT_HIGH_RES_MODE2] = 120,
    [BH1750_CONT_LOW_RES_MODE] = 16,
    [BH1750_ONE_TIME_HIGH_RES_MODE] = 120,
    [BH1750_ONE_TIME_HIGH_RES_MODE2] = 120,
    [BH1750_ONE_TIME_LOW_RES_MODE] = 16
};

void bh1750_init(bh1750_t *dev, i2c_inst_t *i2c, uint8_t addr, bh1750_mode_t mode) {
    dev->i2c = i2c;
    dev->addr = addr;
    dev->mode = mode;
    dev->last_measurement = nil_time;

    // Power On (pode falhar se o sensor não estiver conectado)
    uint8_t power_on = 0x01;
    i2c_write_blocking(dev->i2c, dev->addr, &power_on, 1, false);
    
    // Reset (opcional mas recomendado)
    uint8_t reset = 0x07;
    i2c_write_blocking(dev->i2c, dev->addr, &reset, 1, false);
    
    sleep_ms(10); // Tempo para reset
}

bool bh1750_start_measurement(bh1750_t *dev) {
    uint8_t cmd = dev->mode;
    if (i2c_write_blocking(dev->i2c, dev->addr, &cmd, 1, false) != 1) {
        return false;
    }
    dev->last_measurement = get_absolute_time();
    return true;
}

bool bh1750_read_result(bh1750_t *dev, float *lux) {
    // Verifica se tempo de medição adequado passou
    if (!is_nil_time(dev->last_measurement)) {
        uint32_t elapsed = absolute_time_diff_us(dev->last_measurement, get_absolute_time()) / 1000;
        uint16_t required_time = measurement_times[dev->mode];
        
        if (elapsed < required_time) {
            sleep_ms(required_time - elapsed);
        }
    }

    uint8_t buf[2];
    if (i2c_read_blocking(dev->i2c, dev->addr, buf, 2, false) != 2) {
        return false;
    }

    uint16_t raw = (buf[0] << 8) | buf[1];
    *lux = raw / 1.2f; // Conversão para lux (datasheet page 6)
    return true;
}

bool bh1750_measure(bh1750_t *dev, float *lux) {
    if (!bh1750_start_measurement(dev)) {
        return false;
    }
    return bh1750_read_result(dev, lux);
}