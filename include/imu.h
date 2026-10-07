#ifndef LSM6DSOX_H
#define LSM6DSOX_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/i2c.h"

//  Errors
typedef enum {
    LSM6DSOX_OK = 0,
    LSM6DSOX_ERR_BUS,        // I2C transaction failed
    LSM6DSOX_ERR_NOT_FOUND,  // no device at address
    LSM6DSOX_ERR_BAD_ID,     // wrong WHO_AM_I value
    LSM6DSOX_ERR_PARAM,      // invalid argument
    LSM6DSOX_ERR_NO_DATA,    // no new sample ready yet
} lsm6dsox_error_t;

//  Configuration
typedef enum {
    LSM6DSOX_RATE_OFF    = 0x0,  // power down
    LSM6DSOX_RATE_12_5HZ = 0x1,
    LSM6DSOX_RATE_26HZ   = 0x2,
    LSM6DSOX_RATE_52HZ   = 0x3,
    LSM6DSOX_RATE_104HZ  = 0x4,
    LSM6DSOX_RATE_208HZ  = 0x5,
    LSM6DSOX_RATE_417HZ  = 0x6,
    LSM6DSOX_RATE_833HZ  = 0x7,
} lsm6dsox_rate_t;

typedef enum {
    LSM6DSOX_ACCEL_2G  = 0x0,
    LSM6DSOX_ACCEL_4G  = 0x2,
    LSM6DSOX_ACCEL_8G  = 0x3,
    LSM6DSOX_ACCEL_16G = 0x1,
} lsm6dsox_accel_range_t;

typedef enum {
    LSM6DSOX_GYRO_250DPS  = 0x0,
    LSM6DSOX_GYRO_500DPS  = 0x2,
    LSM6DSOX_GYRO_1000DPS = 0x3,
    LSM6DSOX_GYRO_2000DPS = 0x1,
} lsm6dsox_gyro_range_t;

//  Data
typedef struct {
    float ax, ay, az;      // acceleration, g
    float gx, gy, gz;      // rotation, degrees/second
    float temperature_c;   // die temperature, Celsius
} lsm6dsox_data_t;

//  Driver handle (opaque to callers)
typedef struct lsm6dsox lsm6dsox_t;

//  API

// Create a driver bound to an I2C instance.
// Call once per device. Returns NULL on bad parameters.
lsm6dsox_t *lsm6dsox_create(i2c_inst_t *i2c, uint8_t i2c_addr);

// Probe the bus and verify the device's WHO_AM_I. Configures the
// sensor to a sane default (accel + gyro on, BDU enabled).
lsm6dsox_error_t lsm6dsox_init(const lsm6dsox_t *dev);

// Configure accelerometer: output data rate + full-scale range.
lsm6dsox_error_t lsm6dsox_set_accel(const lsm6dsox_t *dev,
                                     lsm6dsox_rate_t rate,
                                     lsm6dsox_accel_range_t range);

// Configure gyroscope: output data rate + full-scale range.
lsm6dsox_error_t lsm6dsox_set_gyro(const lsm6dsox_t *dev,
                                   lsm6dsox_rate_t rate,
                                   lsm6dsox_gyro_range_t range);

// Configure both sensors at once (common case).
lsm6dsox_error_t lsm6dsox_set_range(const lsm6dsox_t *dev,
                                    lsm6dsox_rate_t rate,
                                    lsm6dsox_accel_range_t accel_range,
                                    lsm6dsox_gyro_range_t gyro_range);

// Software reset the device and wait for it to come back.
lsm6dsox_error_t lsm6dsox_reset(const lsm6dsox_t *dev);

// Put both sensors into power-down mode.
lsm6dsox_error_t lsm6dsox_power_down(const lsm6dsox_t *dev);

// True if a new sample is available (checks status register).
lsm6dsox_error_t lsm6dsox_data_ready(const lsm6dsox_t *dev, bool *ready);

// Fetch the latest sample, converted to physical units.
// Returns LSM6DSOX_ERR_NO_DATA if no new sample is ready.
lsm6dsox_error_t lsm6dsox_read(const lsm6dsox_t *dev, lsm6dsox_data_t *data);

// Human-readable error string (useful for logging).
const char *lsm6dsox_strerror(lsm6dsox_error_t err);

#endif // LSM6DSOX_H
