#ifndef TEST_I2C_H
#define TEST_I2C_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
typedef struct {int x;} i2c_inst_t;
extern i2c_inst_t *i2c0;
void i2c_init(i2c_inst_t *,unsigned);
#endif
#ifndef TEST_I2C_FUNCS
#define TEST_I2C_FUNCS
int i2c_write_blocking(i2c_inst_t *,uint8_t,const uint8_t *,size_t,bool);
int i2c_read_blocking(i2c_inst_t *,uint8_t,uint8_t *,size_t,bool);
#define PICO_ERROR_GENERIC (-1)
#endif
