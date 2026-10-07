#include "baro.h"
#include "pico/stdlib.h"

//  Register map (MPL3115A2 datasheet)
#define REG_DR_STATUS    0x06
#define REG_OUT_P_MSB    0x01  // auto-increments through OUT_T_LSB (0x05)
#define REG_WHO_AM_I     0x0C
#define REG_PT_DATA_CFG  0x13
#define REG_BAR_IN_MSB   0x14
#define REG_BAR_IN_LSB   0x15
#define REG_CTRL_REG1    0x26
#define REG_CTRL_REG2    0x27

#define WHO_AM_I_VALUE   0xC4  // fixed device ID [4]

//  CTRL_REG1 bit fields [4]
#define CTRL1_ALT      0x80  // altimeter mode
#define CTRL1_RAW      0x40  // raw (unprocessed) output
#define CTRL1_OSR_MASK 0x38  // OS[2:0], oversample ratio 2^OS
#define CTRL1_OSR_SHIFT 3
#define CTRL1_RST      0x04  // software reset
#define CTRL1_OST      0x02  // one-shot trigger
#define CTRL1_SBYB     0x01  // 1 = ACTIVE, 0 = STANDBY

//  DR_STATUS bits [4]
#define DR_PDR   0x04  // pressure/altitude new data
#define DR_PTDR  0x08  // pressure OR temperature new data

#define STANDBY_SETTLE_MS 10

struct mpl3115a2 {
    i2c_inst_t *i2c;
    uint8_t addr;
    mpl3115a2_mode_t mode;
};

static mpl3115a2_error_t wr(struct mpl3115a2 *d, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    if (i2c_write_blocking(d->i2c, d->addr, buf, 2, false) == PICO_ERROR_GENERIC)
        return MPL3115A2_ERR_BUS;
    return MPL3115A2_OK;
}

static mpl3115a2_error_t rd(struct mpl3115a2 *d, uint8_t reg,
                            uint8_t *dst, size_t len)
{
    if (i2c_write_blocking(d->i2c, d->addr, &reg, 1, true) == PICO_ERROR_GENERIC)
        return MPL3115A2_ERR_BUS;
    if (i2c_read_blocking(d->i2c, d->addr, dst, len, false) == PICO_ERROR_GENERIC)
        return MPL3115A2_ERR_BUS;
    return MPL3115A2_OK;
}

static mpl3115a2_error_t standby(struct mpl3115a2 *d)
{
    uint8_t ctrl1;
    mpl3115a2_error_t err = rd(d, REG_CTRL_REG1, &ctrl1, 1);
    if (err != MPL3115A2_OK) return err;
    if (!(ctrl1 & CTRL1_SBYB)) return MPL3115A2_OK;  // already standby
    return wr(d, REG_CTRL_REG1, ctrl1 & ~CTRL1_SBYB);
}

static mpl3115a2_error_t active(struct mpl3115a2 *d)
{
    uint8_t ctrl1;
    mpl3115a2_error_t err = rd(d, REG_CTRL_REG1, &ctrl1, 1);
    if (err != MPL3115A2_OK) return err;
    return wr(d, REG_CTRL_REG1, ctrl1 | CTRL1_SBYB);
}

mpl3115a2_t *mpl3115a2_create(i2c_inst_t *i2c, uint8_t i2c_addr)
{
    if (i2c == NULL || i2c_addr != 0x60)  // fixed address, 0x60 [4]
        return NULL;

    static struct mpl3115a2 storage;
    storage.i2c = i2c;
    storage.addr = i2c_addr;
    storage.mode = MPL3115A2_MODE_BAROMETER;
    return (mpl3115a2_t *)&storage;
}

mpl3115a2_error_t mpl3115a2_init(const mpl3115a2_t *dev)
{
    if (dev == NULL) return MPL3115A2_ERR_PARAM;
    struct mpl3115a2 *d = (struct mpl3115a2 *)dev;

    if (i2c_write_blocking(d->i2c, d->addr, NULL, 0, false) == PICO_ERROR_GENERIC)
        return MPL3115A2_ERR_NOT_FOUND;

    uint8_t who = 0;
    mpl3115a2_error_t err = rd(d, REG_WHO_AM_I, &who, 1);
    if (err != MPL3115A2_OK) return err;
    if (who != WHO_AM_I_VALUE) return MPL3115A2_ERR_BAD_ID;  // 0xC4 [4]

    // Must be in STANDBY to change CTRL_REG1 fields (except SBYB/OST/RST) [4]
    err = standby(d);
    if (err != MPL3115A2_OK) return err;
    sleep_ms(STANDBY_SETTLE_MS);

    // Enable data-ready event flags: DREM | PDEFE | TDEFE [4]
    err = wr(d, REG_PT_DATA_CFG, 0x07);
    if (err != MPL3115A2_OK) return err;

    // Default config: barometer, OSR 128, 1 s acquisition step (ST = 0) [4]
    err = wr(d, REG_CTRL_REG1,
             (MPL3115A2_OSR_128 << CTRL1_OSR_SHIFT) & CTRL1_OSR_MASK);
    if (err != MPL3115A2_OK) return err;

    return active(d);
}

mpl3115a2_error_t mpl3115a2_set_mode(const mpl3115a2_t *dev,
                                      mpl3115a2_mode_t mode)
{
    if (dev == NULL) return MPL3115A2_ERR_PARAM;
    struct mpl3115a2 *d = (struct mpl3115a2 *)dev;

    mpl3115a2_error_t err = standby(d);  // config changes need STANDBY [4]
    if (err != MPL3115A2_OK) return err;

    uint8_t ctrl1;
    err = rd(d, REG_CTRL_REG1, &ctrl1, 1);
    if (err != MPL3115A2_OK) return err;

    if (mode == MPL3115A2_MODE_ALTIMETER)
        ctrl1 |= CTRL1_ALT;
    else
        ctrl1 &= ~CTRL1_ALT;

    err = wr(d, REG_CTRL_REG1, ctrl1);
    if (err == MPL3115A2_OK)
        d->mode = mode;

    // Re-activate (STBYB/OST/RST may be written in any mode [4])
    return (err == MPL3115A2_OK) ? active(d) : err;
}

mpl3115a2_error_t mpl3115a2_set_oversample(const mpl3115a2_t *dev,
                                            mpl3115a2_osr_t osr)
{
    if (dev == NULL || (unsigned)osr > MPL3115A2_OSR_128)
        return MPL3115A2_ERR_PARAM;
    struct mpl3115a2 *d = (struct mpl3115a2 *)dev;

    mpl3115a2_error_t err = standby(d);
    if (err != MPL3115A2_OK) return err;

    uint8_t ctrl1;
    err = rd(d, REG_CTRL_REG1, &ctrl1, 1);
    if (err != MPL3115A2_OK) return err;

    ctrl1 = (ctrl1 & ~CTRL1_OSR_MASK) | ((uint8_t)osr << CTRL1_OSR_SHIFT);
    err = wr(d, REG_CTRL_REG1, ctrl1);
    return (err == MPL3115A2_OK) ? active(d) : err;
}

mpl3115a2_error_t mpl3115a2_set_sample_period(const mpl3115a2_t *dev,
                                              uint8_t seconds_pow2)
{
    if (dev == NULL || seconds_pow2 > 15) return MPL3115A2_ERR_PARAM;
    struct mpl3115a2 *d = (struct mpl3115a2 *)dev;

    // Acquisition step = 2^ST seconds, ST is CTRL_REG2 bits [3:0] [4].
    // (The datasheet notes the CTRL_REG2 fields must also be set in standby.)
    mpl3115a2_error_t err = standby(d);
    if (err != MPL3115A2_OK) return err;

    uint8_t ctrl2;
    err = rd(d, REG_CTRL_REG2, &ctrl2, 1);
    if (err != MPL3115A2_OK) return err;

    ctrl2 = (ctrl2 & 0xF0) | seconds_pow2;
    err = wr(d, REG_CTRL_REG2, ctrl2);
    return (err == MPL3115A2_OK) ? active(d) : err;
}

mpl3115a2_error_t mpl3115a2_set_sea_level_pressure(const mpl3115a2_t *dev,
                                                    float pressure_pa)
{
    if (dev == NULL || pressure_pa < 30000.0f || pressure_pa > 120000.0f)
        return MPL3115A2_ERR_PARAM;
    struct mpl3115a2 *d = (struct mpl3115a2 *)dev;

    uint16_t raw = (uint16_t)(pressure_pa / 2.0f);  // BAR_IN is 2 Pa/LSB [4]
    uint8_t vals[2] = { (uint8_t)(raw >> 8), (uint8_t)(raw & 0xFF) };

    mpl3115a2_error_t err = wr(d, REG_BAR_IN_MSB, vals[0]);
    if (err != MPL3115A2_OK) return err;
    return wr(d, REG_BAR_IN_LSB, vals[1]);
}

mpl3115a2_error_t mpl3115a2_data_ready(const mpl3115a2_t *dev, bool *ready)
{
    if (dev == NULL || ready == NULL) return MPL3115A2_ERR_PARAM;
    struct mpl3115a2 *d = (struct mpl3115a2 *)dev;

    uint8_t status;
    mpl3115a2_error_t err = rd(d, REG_DR_STATUS, &status, 1);
    if (err != MPL3115A2_OK) return err;

    *ready = (status & DR_PTDR) != 0;  // new pressure/temp sample [4]
    return MPL3115A2_OK;
}

mpl3115a2_error_t mpl3115a2_read(const mpl3115a2_t *dev,
                                 mpl3115a2_data_t *data)
{
    if (dev == NULL || data == NULL) return MPL3115A2_ERR_PARAM;
    struct mpl3115a2 *d = (struct mpl3115a2 *)dev;

    bool ready;
    mpl3115a2_error_t err = mpl3115a2_data_ready(dev, &ready);
    if (err != MPL3115A2_OK) return err;
    if (!ready) return MPL3115A2_ERR_NO_DATA;

    // One burst: OUT_P_MSB..OUT_T_LSB, auto-increment 0x01 -> 0x05 [4]
    uint8_t raw[5];
    err = rd(d, REG_OUT_P_MSB, raw, 5);
    if (err != MPL3115A2_OK) return err;

    if (d->mode == MPL3115A2_MODE_BAROMETER) {
        // 20-bit unsigned pressure: 18-bit integer Pa + 2 fractional bits
        // (0.25 Pa) in OUT_P_LSB bits 5:4 [4]
        uint32_t whole = ((uint32_t)raw[0] << 10) |
                         ((uint32_t)raw[1] << 2) |
                         ((uint32_t)raw[2] >> 6);
        float frac = (raw[2] >> 4) & 0x03;   // quarter-Pa steps
        data->pressure_pa = whole + frac * 0.25f;
    } else {
        // 20-bit 2's-complement altitude: signed meters in MSB/CSB,
        // unsigned 0.0625 m fractions in OUT_P_LSB bits 7:4 [4]
        int16_t whole = (int16_t)(((uint16_t)raw[0] << 8) | raw[1]);
        float frac = (raw[2] >> 4) & 0x0F;
        data->altitude_m = whole + frac * 0.0625f;
    }

    // 12-bit signed temperature: integer °C in OUT_T_MSB (2's complement),
    // 0.0625 °C fractions in OUT_T_LSB bits 7:4 [4]
    data->temperature_c = (int8_t)raw[3] +
                          ((raw[4] >> 4) & 0x0F) * 0.0625f;

    return MPL3115A2_OK;
}

const char *mpl3115a2_strerror(mpl3115a2_error_t err)
{
    switch (err) {
        case MPL3115A2_OK:          return "OK";
        case MPL3115A2_ERR_BUS:      return "I2C bus error";
        case MPL3115A2_ERR_NOT_FOUND:return "device not found on bus";
        case MPL3115A2_ERR_BAD_ID:   return "unexpected WHO_AM_I";
        case MPL3115A2_ERR_PARAM:     return "invalid parameter";
        case MPL3115A2_ERR_NO_DATA:   return "no new data ready";
        default:                     return "unknown error";
    }
}
