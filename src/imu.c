#include "imu.h"
#include "pico/stdlib.h"

// ================== Register map (internal) ==================
#define REG_WHO_AM_I     0x0F
#define REG_CTRL1_XL     0x10
#define REG_CTRL2_G      0x11
#define REG_CTRL3_C      0x12
#define REG_STATUS       0x1E
#define REG_OUTX_L_G     0x22
#define REG_OUT_TEMP_L   0x20
#define REG_OUTX_L_A     0x28

#define WHO_AM_I_EXPECTED  0x6C
#define DEVICE_READY_MS    100   // wait after soft reset

//  Sensitivities (datasheet)
// LSB per g and LSB per dps for each full-scale setting.
#define ACCEL_SENS_LB_PER_G(range) \
    ((range) == LSM6DSOX_ACCEL_2G  ? 0.0000610f : \
     (range) == LSM6DSOX_ACCEL_4G  ? 0.0001220f : \
     (range) == LSM6DSOX_ACCEL_8G  ? 0.0002440f : \
                                     0.0004880f)

#define GYRO_SENS_LB_PER_DPS(range) \
    ((range) == LSM6DSOX_GYRO_250DPS  ? 0.00875f : \
     (range) == LSM6DSOX_GYRO_500DPS  ? 0.01750f : \
     (range) == LSM6DSOX_GYRO_1000DPS ? 0.03500f : \
                                        0.07000f)

#define TEMP_LSB_PER_C  256.0f
#define TEMP_OFFSET_C   25.0f

//  Handle
struct lsm6dsox {
    i2c_inst_t *i2c;
    uint8_t addr;
    lsm6dsox_accel_range_t accel_range;
    lsm6dsox_gyro_range_t  gyro_range;
};

// ================== Low-level bus access (static/private) ==================

static lsm6dsox_error_t bus_write(struct lsm6dsox *d, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    if (i2c_write_blocking(d->i2c, d->addr, buf, 2, false) == PICO_ERROR_GENERIC)
        return LSM6DSOX_ERR_BUS;
    return LSM6DSOX_OK;
}

static lsm6dsox_error_t bus_write_mask(struct lsm6dsox *d, uint8_t reg,
                                       uint8_t mask, uint8_t val)
{
    uint8_t cur;
    // read-modify-write
    uint8_t cur_out;
    if (i2c_write_blocking(d->i2c, d->addr, &reg, 1, true) == PICO_ERROR_GENERIC)
        return LSM6DSOX_ERR_BUS;
    if (i2c_read_blocking(d->i2c, d->addr, &cur_out, 1, false) == PICO_ERROR_GENERIC)
        return LSM6DSOX_ERR_BUS;
    cur = (cur_out & ~mask) | (val & mask);
    return bus_write(d, reg, cur);
}

static lsm6dsox_error_t bus_read(struct lsm6dsox *d, uint8_t reg,
                                 uint8_t *dst, size_t len)
{
    if (i2c_write_blocking(d->i2c, d->addr, &reg, 1, true) == PICO_ERROR_GENERIC)
        return LSM6DSOX_ERR_BUS;
    if (i2c_read_blocking(d->i2c, d->addr, dst, len, false) == PICO_ERROR_GENERIC)
        return LSM6DSOX_ERR_BUS;
    return LSM6DSOX_OK;
}

//  Public API

lsm6dsox_t *lsm6dsox_create(i2c_inst_t *i2c, uint8_t i2c_addr)
{
    if (i2c == NULL || (i2c_addr != 0x6A && i2c_addr != 0x6B))
        return NULL;

    static struct lsm6dsox storage;
    storage.i2c = i2c;
    storage.addr = i2c_addr;
    storage.accel_range = LSM6DSOX_ACCEL_4G;
    storage.gyro_range = LSM6DSOX_GYRO_500DPS;
    return (lsm6dsox_t *)&storage;
}

lsm6dsox_error_t lsm6dsox_reset(const lsm6dsox_t *dev)
{
    if (dev == NULL) return LSM6DSOX_ERR_PARAM;
    struct lsm6dsox *d = (struct lsm6dsox *)dev;

    // SW_RESET bit (bit 0 of CTRL3_C)
    lsm6dsox_error_t err = bus_write(d, REG_CTRL3_C, 0x01);
    if (err != LSM6DSOX_OK) return err;

    sleep_ms(DEVICE_READY_MS);

    // Wait for reset to self-clear
    uint8_t ctrl3;
    for (int i = 0; i < 10; i++) {
        err = bus_read(d, REG_CTRL3_C, &ctrl3, 1);
        if (err != LSM6DSOX_OK) return err;
        if (!(ctrl3 & 0x01)) return LSM6DSOX_OK;
        sleep_ms(1);
    }
    return LSM6DSOX_ERR_BUS;
}

lsm6dsox_error_t lsm6dsox_init(const lsm6dsox_t *dev)
{
    if (dev == NULL) return LSM6DSOX_ERR_PARAM;
    struct lsm6dsox *d = (struct lsm6dsox *)dev;

    // Verify something is on the bus
    if (i2c_write_blocking(d->i2c, d->addr, NULL, 0, false) == PICO_ERROR_GENERIC)
        return LSM6DSOX_ERR_NOT_FOUND;

    // Verify identity
    uint8_t who = 0;
    lsm6dsox_error_t err = bus_read(d, REG_WHO_AM_I, &who, 1);
    if (err != LSM6DSOX_OK) return err;
    if (who != WHO_AM_I_EXPECTED) return LSM6DSOX_ERR_BAD_ID;

    // Soft reset, then configure defaults
    err = lsm6dsox_reset(dev);
    if (err != LSM6DSOX_OK) return err;

    // CTRL3_C: BDU=1 (bit 6), IF_INC=1 (bit 2) => block data update
    //          + auto-increment on multi-byte reads
    err = bus_write(d, REG_CTRL3_C, 0x44);
    if (err != LSM6DSOX_OK) return err;

    // Default config: accel + gyro at 104 Hz, +/-4g, 500 dps
    return lsm6dsox_set_range(dev, LSM6DSOX_RATE_104HZ,
                               LSM6DSOX_ACCEL_4G, LSM6DSOX_GYRO_500DPS);
}

lsm6dsox_error_t lsm6dsox_set_accel(const lsm6dsox_t *dev,
                                    lsm6dsox_rate_t rate,
                                    lsm6dsox_accel_range_t range)
{
    if (dev == NULL) return LSM6DSOX_ERR_PARAM;
    if ((unsigned)rate > LSM6DSOX_RATE_833HZ) return LSM6DSOX_ERR_PARAM;
    if ((range >> 2) > 0x1) return LSM6DSOX_ERR_PARAM;  // only 2-bit values

    struct lsm6dsox *d = (struct lsm6dsox *)dev;
    // CTRL1_XL: ODR in bits [7:4], FS in bits [3:2]
    uint8_t val = ((uint8_t)rate << 4) | ((uint8_t)range << 2);
    lsm6dsox_error_t err = bus_write(d, REG_CTRL1_XL, val);
    if (err == LSM6DSOX_OK)
        d->accel_range = range;
    return err;
}

lsm6dsox_error_t lsm6dsox_set_gyro(const lsm6dsox_t *dev,
                                   lsm6dsox_rate_t rate,
                                   lsm6dsox_gyro_range_t range)
{
    if (dev == NULL) return LSM6DSOX_ERR_PARAM;
    if ((unsigned)rate > LSM6DSOX_RATE_833HZ) return LSM6DSOX_ERR_PARAM;
    if ((range >> 2) > 0x1) return LSM6DSOX_ERR_PARAM;

    struct lsm6dsox *d = (struct lsm6dsox *)dev;
    // CTRL2_G: ODR in bits [7:4], FS in bits [3:2]
    uint8_t val = ((uint8_t)rate << 4) | ((uint8_t)range << 2);
    lsm6dsox_error_t err = bus_write(d, REG_CTRL2_G, val);
    if (err == LSM6DSOX_OK)
        d->gyro_range = range;
    return err;
}

lsm6dsox_error_t lsm6dsox_set_range(const lsm6dsox_t *dev,
                                    lsm6dsox_rate_t rate,
                                    lsm6dsox_accel_range_t accel_range,
                                    lsm6dsox_gyro_range_t gyro_range)
{
    lsm6dsox_error_t err = lsm6dsox_set_accel(dev, rate, accel_range);
    if (err != LSM6DSOX_OK) return err;
    return lsm6dsox_set_gyro(dev, rate, gyro_range);
}

lsm6dsox_error_t lsm6dsox_power_down(const lsm6dsox_t *dev)
{
    return lsm6dsox_set_range(dev, LSM6DSOX_RATE_OFF,
                               LSM6DSOX_ACCEL_2G, LSM6DSOX_GYRO_250DPS);
}

lsm6dsox_error_t lsm6dsox_data_ready(const lsm6dsox_t *dev, bool *ready)
{
    if (dev == NULL || ready == NULL) return LSM6DSOX_ERR_PARAM;
    struct lsm6dsox *d = (struct lsm6dsox *)dev;

    uint8_t status;
    lsm6dsox_error_t err = bus_read(d, REG_STATUS, &status, 1);
    if (err != LSM6DSOX_OK) return err;

    *ready = (status & 0x03) == 0x03;  // XLDA + GDA both set
    return LSM6DSOX_OK;
}

lsm6dsox_error_t lsm6dsox_read(const lsm6dsox_t *dev, lsm6dsox_data_t *data)
{
    if (dev == NULL || data == NULL) return LSM6DSOX_ERR_PARAM;
    struct lsm6dsox *d = (struct lsm6dsox *)dev;

    bool ready;
    lsm6dsox_error_t err = lsm6dsox_data_ready(dev, &ready);
    if (err != LSM6DSOX_OK) return err;
    if (!ready) return LSM6DSOX_ERR_NO_DATA;

    // One burst read of gyro (0x22..0x27) + accel (0x28..0x2D) is possible
    // but temperature sits at 0x20. Keep it simple: two bursts.
    uint8_t gyro_raw[6], accel_raw[6], temp_raw[2];

    err = bus_read(d, REG_OUTX_L_G, gyro_raw, 6);
    if (err != LSM6DSOX_OK) return err;
    err = bus_read(d, REG_OUTX_L_A, accel_raw, 6);
    if (err != LSM6DSOX_OK) return err;
    err = bus_read(d, REG_OUT_TEMP_L, temp_raw, 2);
    if (err != LSM6DSOX_OK) return err;

    int16_t gx = (int16_t)(gyro_raw[1] << 8 | gyro_raw[0]);
    int16_t gy = (int16_t)(gyro_raw[3] << 8 | gyro_raw[2]);
    int16_t gz = (int16_t)(gyro_raw[5] << 8 | gyro_raw[4]);
    int16_t ax = (int16_t)(accel_raw[1] << 8 | accel_raw[0]);
    int16_t ay = (int16_t)(accel_raw[3] << 8 | accel_raw[2]);
    int16_t az = (int16_t)(accel_raw[5] << 8 | accel_raw[4]);
    int16_t temp = (int16_t)(temp_raw[1] << 8 | temp_raw[0]);

    float a_sens = ACCEL_SENS_LB_PER_G(d->accel_range);   // g per LSB
    float g_sens = GYRO_SENS_LB_PER_DPS(d->gyro_range);   // dps per LSB

    data->ax = ax * a_sens;
    data->ay = ay * a_sens;
    data->az = az * a_sens;
    data->gx = gx * g_sens;
    data->gy = gy * g_sens;
    data->gz = gz * g_sens;
    data->temperature_c = (temp / TEMP_LSB_PER_C) + TEMP_OFFSET_C;

    return LSM6DSOX_OK;
}

const char *lsm6dsox_strerror(lsm6dsox_error_t err)
{
    switch (err) {
        case LSM6DSOX_OK:        return "OK";
        case LSM6DSOX_ERR_BUS:   return "I2C bus error";
        case LSM6DSOX_ERR_NOT_FOUND: return "device not found on bus";
        case LSM6DSOX_ERR_BAD_ID:    return "unexpected WHO_AM_I value";
        case LSM6DSOX_ERR_PARAM:     return "invalid parameter";
        case LSM6DSOX_ERR_NO_DATA:   return "no new data ready";
        default:                return "unknown error";
    }
}
