#ifndef MPL3115A2_H
#define MPL3115A2_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware/i2c.h"

typedef enum {
    MPL3115A2_OK = 0,
    MPL3115A2_ERR_BUS,
    MPL3115A2_ERR_NOT_FOUND,
    MPL3115A2_ERR_BAD_ID,   // WHO_AM_I != 0xC4 [4]
    MPL3115A2_ERR_PARAM,
    MPL3115A2_ERR_NO_DATA,
} mpl3115a2_error_t;

typedef enum {
    MPL3115A2_MODE_BAROMETER,  // pressure output in Pa (CTRL_REG1 ALT = 0)
    MPL3115A2_MODE_ALTIMETER,  // altitude output in m  (CTRL_REG1 ALT = 1)
} mpl3115a2_mode_t;

typedef enum {
    MPL3115A2_OSR_1   = 0,   // oversample ratios 2^OS, per CTRL_REG1 OS bits
    MPL3115A2_OSR_2   = 1,
    MPL3115A2_OSR_4   = 2,
    MPL3115A2_OSR_8   = 3,
    MPL3115A2_OSR_16  = 4,
    MPL3115A2_OSR_32  = 5,
    MPL3115A2_OSR_64  = 6,
    MPL3115A2_OSR_128 = 7,   // highest resolution (1.5 Pa RMS noise)
} mpl3115a2_osr_t;

typedef struct {
    union {
        float pressure_pa;   // valid in barometer mode
        float altitude_m;    // valid in altimeter mode
    };
    float temperature_c;
} mpl3115a2_data_t;

typedef struct mpl3115a2 mpl3115a2_t;

mpl3115a2_t *mpl3115a2_create(i2c_inst_t *i2c, uint8_t i2c_addr);

// Probe, verify WHO_AM_I == 0xC4 [4], enable data-ready event flags
// (PT_DATA_CFG = 0x07 [4]) and go ACTIVE with OSR 128, 1 Hz.
mpl3115a2_error_t mpl3115a2_init(const mpl3115a2_t *dev);

// Barometer (Pa) or altimeter (m) output mode [4].
mpl3115a2_error_t mpl3115a2_set_mode(const mpl3115a2_t *dev,
                                      mpl3115a2_mode_t mode);

// Oversampling ratio 1x..128x [4].
mpl3115a2_error_t mpl3115a2_set_oversample(const mpl3115a2_t *dev,
                                            mpl3115a2_osr_t osr);

// Autonomous acquisition period in seconds (2^ST, 1 s to ~9 h) [4].
mpl3115a2_error_t mpl3115a2_set_sample_period(const mpl3115a2_t *dev,
                                              uint8_t seconds_pow2);

// Sea-level reference pressure for altitude calculations, in Pa.
// Stored in BAR_IN as 2 Pa per LSB [4].
mpl3115a2_error_t mpl3115a2_set_sea_level_pressure(const mpl3115a2_t *dev,
                                                    float pressure_pa);

// True when a new pressure/altitude + temperature sample is ready (PTDR ).
mpl3115a2_error_t mpl3115a2_data_ready(const mpl3115a2_t *dev, bool *ready);

// Read the latest sample, converted to physical units.
mpl3115a2_error_t mpl3115a2_read(const mpl3115a2_t *dev,
                                 mpl3115a2_data_t *data);

const char *mpl3115a2_strerror(mpl3115a2_error_t err);
#endif // MPL3115A2_H
