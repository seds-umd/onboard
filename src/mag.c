#include "mag.h"
#include "pico/stdlib.h"

//  Register map (per Adafruit MMC56x3 register enum)
#define REG_OUT_X_L      0x00  // 9-byte burst: XYZ [19:8] then XYZ [3:0]
#define REG_OUT_TEMP     0x09
#define REG_STATUS       0x18
#define REG_ODR          0x1A
#define REG_CTRL0        0x1B
#define REG_CTRL1        0x1C
#define REG_CTRL2        0x1D
#define REG_PRODUCT_ID   0x39

#define CHIP_ID_MMC5603  0x10  // MMC5603 product ID
#define CHIP_ID_MMC5613  0xFC  // pin-compatible MMC5613 variant

//  CTRL0 bits [1]
#define CTRL0_TM_M        0x01  // take magnetometer measurement
#define CTRL0_TM_T        0x02  // take temperature measurement
#define CTRL0_SET         0x08  // set pulse
#define CTRL0_RESET       0x10  // reset pulse
#define CTRL0_CMM_FREQ_EN 0x80  // ODR register sets cmm frequency

//  CTRL1 bits [1]
#define CTRL1_SW_RESET    0x80

//  CTRL2 bits [1]
#define CTRL2_CMM_EN      0x10  // continuous measurement mode enable
#define CTRL2_HPOWER      0x80  // 1000 Hz high-power mode

//  STATUS bits [1]
#define STATUS_MEAS_DONE  0x40  // magnetometer measurement done (bit 6)
#define STATUS_TEMP_DONE  0x80  // temperature measurement done (bit 7)

#define LSB_TO_UT   0.00625f  // µT per LSB, 20-bit output
#define TEMP_LSB_C  0.8f      // °C per LSB
#define TEMP_OFFSET 75.0f     // zero count corresponds to -75 °C

#define MEAS_TIMEOUT_MS 100

struct mmc5603 {
    i2c_inst_t *i2c;
    uint8_t addr;
    bool continuous;
};

//  private bus helpers
static mmc5603_error_t wr(struct mmc5603 *d, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    if (i2c_write_blocking(d->i2c, d->addr, buf, 2, false) == PICO_ERROR_GENERIC)
        return MMC5603_ERR_BUS;
    return MMC5603_OK;
}

static mmc5603_error_t rd(struct mmc5603 *d, uint8_t reg,
                          uint8_t *dst, size_t len)
{
    if (i2c_write_blocking(d->i2c, d->addr, &reg, 1, true) == PICO_ERROR_GENERIC)
        return MMC5603_ERR_BUS;
    if (i2c_read_blocking(d->i2c, d->addr, dst, len, false) == PICO_ERROR_GENERIC)
        return MMC5603_ERR_BUS;
    return MMC5603_OK;
}

//  public API

mmc5603_t *mmc5603_create(i2c_inst_t *i2c, uint8_t i2c_addr)
{
    if (i2c == NULL || (i2c_addr != 0x30 && i2c_addr != 0x31))
        return NULL;

    static struct mmc5603 storage;
    storage.i2c = i2c;
    storage.addr = i2c_addr;
    storage.continuous = false;
    return (mmc5603_t *)&storage;
}

mmc5603_error_t mmc5603_init(const mmc5603_t *dev)
{
    if (dev == NULL) return MMC5603_ERR_PARAM;
    struct mmc5603 *d = (struct mmc5603 *)dev;

    if (i2c_write_blocking(d->i2c, d->addr, NULL, 0, false) == PICO_ERROR_GENERIC)
        return MMC5603_ERR_NOT_FOUND;

    uint8_t id = 0;
    mmc5603_error_t err = rd(d, REG_PRODUCT_ID, &id, 1);
    if (err != MMC5603_OK) return err;
    if (id != CHIP_ID_MMC5603 && id != CHIP_ID_MMC5613)
        return MMC5603_ERR_BAD_ID;

    // Software reset (CTRL1 bit 7), wait for boot
    err = wr(d, REG_CTRL1, CTRL1_SW_RESET);
    if (err != MMC5603_OK) return err;
    sleep_ms(20);

    // Set pulse then reset pulse to clear residual magnetization
    err = wr(d, REG_CTRL0, CTRL0_SET);
    if (err != MMC5603_OK) return err;
    sleep_ms(1);
    err = wr(d, REG_CTRL0, CTRL0_RESET);
    if (err != MMC5603_OK) return err;
    sleep_ms(1);

    // Make sure continuous mode is off (one-shot default)
    d->continuous = false;
    return wr(d, REG_CTRL2, 0x00);
}

mmc5603_error_t mmc5603_set_rate(const mmc5603_t *dev, uint16_t hz)
{
    if (dev == NULL) return MMC5603_ERR_PARAM;
    struct mmc5603 *d = (struct mmc5603 *)dev;

    uint8_t ctrl2, odr;
    mmc5603_error_t err = rd(d, REG_CTRL2, &ctrl2, 1);
    if (err != MMC5603_OK) return err;

    if (hz == 1000) {
        // 1000 Hz: ODR = 255 plus high-power bit
        odr = 255;
        ctrl2 |= CTRL2_HPOWER;
    } else if (hz >= 1 && hz <= 255) {
        odr = (uint8_t)hz;
        ctrl2 &= ~CTRL2_HPOWER;
    } else {
        return MMC5603_ERR_PARAM;
    }

    err = wr(d, REG_ODR, odr);
    if (err != MMC5603_OK) return err;

    if (d->continuous) {
        // cmm_freq_en: ODR register controls the continuous-mode rate
        err = wr(d, REG_CTRL0, CTRL0_CMM_FREQ_EN);
        if (err != MMC5603_OK) return err;
    }
    return wr(d, REG_CTRL2, ctrl2);
}

mmc5603_error_t mmc5603_set_continuous(const mmc5603_t *dev, bool enable)
{
    if (dev == NULL) return MMC5603_ERR_PARAM;
    struct mmc5603 *d = (struct mmc5603 *)dev;

    uint8_t ctrl2;
    mmc5603_error_t err = rd(d, REG_CTRL2, &ctrl2, 1);
    if (err != MMC5603_OK) return err;

    if (enable) {
        // Route the ODR register to the continuous-mode frequency
        err = wr(d, REG_CTRL0, CTRL0_CMM_FREQ_EN);
        if (err != MMC5603_OK) return err;
        ctrl2 |= CTRL2_CMM_EN;
    } else {
        ctrl2 &= ~CTRL2_CMM_EN;
    }

    err = wr(d, REG_CTRL2, ctrl2);
    if (err == MMC5603_OK)
        d->continuous = enable;
    return err;
}

bool mmc5603_is_continuous(const mmc5603_t *dev)
{
    return dev ? ((struct mmc5603 *)dev)->continuous : false;
}

mmc5603_error_t mmc5603_read(const mmc5603_t *dev, mmc5603_data_t *data)
{
    if (dev == NULL || data == NULL) return MMC5603_ERR_PARAM;
    struct mmc5603 *d = (struct mmc5603 *)dev;

    if (!d->continuous) {
        // One-shot: trigger a measurement and wait for the done bit [1]
        mmc5603_error_t err = wr(d, REG_CTRL0, CTRL0_TM_M);
        if (err != MMC5603_OK) return err;

        uint8_t status = 0;
        for (int i = 0; i < MEAS_TIMEOUT_MS; i++) {
            err = rd(d, REG_STATUS, &status, 1);
            if (err != MMC5603_OK) return err;
            if (status & STATUS_MEAS_DONE) break;
            sleep_ms(1);
        }
        if (!(status & STATUS_MEAS_DONE)) return MMC5603_ERR_TIMEOUT;
    }

    // 20-bit XYZ: [19:8] of each axis in bytes 0-5, low nibbles in 6-8
    uint8_t b[9];
    mmc5603_error_t err = rd(d, REG_OUT_X_L, b, 9);
    if (err != MMC5603_OK) return err;

    int32_t x = (int32_t)((uint32_t)b[0] << 12 | (uint32_t)b[1] << 4 |
                          (uint32_t)b[6] >> 4);
    int32_t y = (int32_t)((uint32_t)b[2] << 12 | (uint32_t)b[3] << 4 |
                          (uint32_t)b[7] >> 4);
    int32_t z = (int32_t)((uint32_t)b[4] << 12 | (uint32_t)b[5] << 4 |
                          (uint32_t)b[8] >> 4);

    // Offset binary: mid-scale (1<<19) is zero field
    x -= 1 << 19;
    y -= 1 << 19;
    z -= 1 << 19;

    data->mx = x * LSB_TO_UT;
    data->my = y * LSB_TO_UT;
    data->mz = z * LSB_TO_UT;
    return MMC5603_OK;
}

mmc5603_error_t mmc5603_read_temperature(const mmc5603_t *dev,
                                         float *temperature_c)
{
    if (dev == NULL || temperature_c == NULL) return MMC5603_ERR_PARAM;
    struct mmc5603 *d = (struct mmc5603 *)dev;

    if (d->continuous) return MMC5603_ERR_MODE;  // not supported in cmm

    mmc5603_error_t err = wr(d, REG_CTRL0, CTRL0_TM_T);
    if (err != MMC5603_OK) return err;

    uint8_t status = 0;
    for (int i = 0; i < MEAS_TIMEOUT_MS; i++) {
        err = rd(d, REG_STATUS, &status, 1);
        if (err != MMC5603_OK) return err;
        if (status & STATUS_TEMP_DONE) break;
        sleep_ms(1);
    }
    if (!(status & STATUS_TEMP_DONE)) return MMC5603_ERR_TIMEOUT;

    uint8_t raw;
    err = rd(d, REG_OUT_TEMP, &raw, 1);
    if (err != MMC5603_OK) return err;

    *temperature_c = raw * TEMP_LSB_C - TEMP_OFFSET;  // 0.8 °C/LSB, -75 °C
    return MMC5603_OK;
}

const char *mmc5603_strerror(mmc5603_error_t err)
{
    switch (err) {
        case MMC5603_OK:           return "OK";
        case MMC5603_ERR_BUS:      return "I2C bus error";
        case MMC5603_ERR_NOT_FOUND:return "device not found on bus";
        case MMC5603_ERR_BAD_ID:   return "unexpected product ID";
        case MMC5603_ERR_PARAM:    return "invalid parameter";
        case MMC5603_ERR_TIMEOUT:  return "measurement timeout";
        case MMC5603_ERR_MODE:     return "not available in current mode";
        default:                   return "unknown error";
    }
}
