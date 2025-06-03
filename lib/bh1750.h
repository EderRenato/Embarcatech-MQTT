#ifndef BH1750_H
#define BH1750_H

#include "hardware/i2c.h"
#include "pico/stdlib.h"

// Endereços I2C
#define BH1750_ADDR_LOW  0x23 // ADDR pin connected to GND
#define BH1750_ADDR_HIGH 0x5C // ADDR pin connected to VCC

// Modos de operação (datasheet page 5)
typedef enum {
    BH1750_CONT_HIGH_RES_MODE = 0x10,   // Resolution: 1 lx, Measurement Time: 120ms
    BH1750_CONT_HIGH_RES_MODE2 = 0x11,  // Resolution: 0.5 lx, Measurement Time: 120ms
    BH1750_CONT_LOW_RES_MODE = 0x13,    // Resolution: 4 lx, Measurement Time: 16ms
    BH1750_ONE_TIME_HIGH_RES_MODE = 0x20,
    BH1750_ONE_TIME_HIGH_RES_MODE2 = 0x21,
    BH1750_ONE_TIME_LOW_RES_MODE = 0x23
} bh1750_mode_t;

// Estrutura do dispositivo
typedef struct {
    i2c_inst_t *i2c;
    uint8_t addr;
    bh1750_mode_t mode;
    absolute_time_t last_measurement;
} bh1750_t;

/**
 * @brief Inicializa o sensor BH1750
 * @param dev Ponteiro para a estrutura do dispositivo
 * @param i2c Instância I2C (i2c0 ou i2c1)
 * @param addr Endereço I2C (BH1750_ADDR_LOW ou BH1750_ADDR_HIGH)
 * @param mode Modo de operação
 */
void bh1750_init(bh1750_t *dev, i2c_inst_t *i2c, uint8_t addr, bh1750_mode_t mode);

/**
 * @brief Inicia uma medição de luminosidade
 * @param dev Ponteiro para a estrutura do dispositivo
 * @return true se bem sucedido, false em caso de erro
 */
bool bh1750_start_measurement(bh1750_t *dev);

/**
 * @brief Lê o valor de luminosidade
 * @param dev Ponteiro para a estrutura do dispositivo
 * @param lux Ponteiro para armazenar o valor em lux
 * @return true se bem sucedido, false em caso de erro
 * @note Deve ser chamado após bh1750_start_measurement() e tempo de medição adequado
 */
bool bh1750_read_result(bh1750_t *dev, float *lux);

/**
 * @brief Medição única com tratamento completo
 * @param dev Ponteiro para a estrutura do dispositivo
 * @param lux Ponteiro para armazenar o valor em lux
 * @return true se bem sucedido, false em caso de erro
 */
bool bh1750_measure(bh1750_t *dev, float *lux);

#endif