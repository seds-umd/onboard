#ifndef MMC5603_H
#define MMC5603_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/i2c.h"

typedef enum {
    MMC5603_OK = 0,
    MMC5603_ERR_BUS,
    MMC5603_ERR_NOT_FOUND,
    MMC5603_ERR_BAD_ID,
    MMC5603_ERR_PARAM,
    MMC5603_ERR_TIMEOUT,     // measurement never completed
    MMC5603_ERR_MODE,        // operation invalid in current mode
} mmc5603_error_t;

typedef struct {
    float mx, my, mz;  // magnetic field, microtesla
} mmc5603_data_t;

// Opaque handle
typedef struct mmc5603 mmc5603_t;

// Create a driver bound to an I2C instance (address is typically 0x30 [3]).
mmc5603_t *mmc5603_create(i2c_inst_t *i2c, uint8_t i2c_addr);

// Probe the bus, verify product ID, soft-reset, and do a set/reset
// pulse to clear residual magnetization [1].
mmc5603_error_t mmc5603_init(const mmc5603_t *dev);

// Output data rate: 1..255 Hz, or 1000 Hz (uses high-power mode) [1].
mmc5603_error_t mmc5603_set_rate(const mmc5603_t *dev, uint16_t hz);

// Enable/disable continuous measurement mode [1].
// One-shot mode (the default after init) takes a measurement per read.
mmc5603_error_t mmc5603_set_continuous(const mmc5603_t *dev, bool enable);

bool mmc5603_is_continuous(const mmc5603_t *dev);

// Read magnetic field. In one-shot mode this triggers a measurement and
// waits for it (blocking). In continuous mode it reads the latest sample [1].
mmc5603_error_t mmc5603_read(const mmc5603_t *dev, mmc5603_data_t *data);

// On-chip temperature. Only available in one-shot mode [1].
mmc5603_error_t mmc5603_read_temperature(const mmc5603_t *dev,
                                        float *temperature_c);

const char *mmc5603_strerror(mmc5603_error_t err);


#endif // MMC5603_H
