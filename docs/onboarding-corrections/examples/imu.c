#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define I2C_PORT      i2c0
#define SDA_PIN       4
#define SCL_PIN       5
#define LSM_ADDR      0x6A     // LSM6DSOX, SA0 low
#define REG_WHO_AM_I  0x0F
#define REG_CTRL1_XL  0x10
#define REG_OUTX_L_A  0x28

static int reg_write(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    return i2c_write_blocking(I2C_PORT, LSM_ADDR, buf, 2, false);
}

static int reg_read(uint8_t reg, uint8_t *dst, size_t len) {
    // true on the write keeps control of the bus for the read that follows
    int r = i2c_write_blocking(I2C_PORT, LSM_ADDR, &reg, 1, true);
    if (r != 1) return -1;
    return i2c_read_blocking(I2C_PORT, LSM_ADDR, dst, len, false);
}

int main(void) {
    stdio_init_all();
    while (!stdio_usb_connected()) sleep_ms(10); // open a USB terminal first

    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    uint8_t who = 0;
    if (reg_read(REG_WHO_AM_I, &who, 1) != 1 || who != 0x6c) {
        printf("IMU not found: WHO_AM_I=0x%02x; check wiring/address\n", who);
        return 1;
    }
    printf("WHO_AM_I = 0x%02x, expected 0x6c\n", who);

    if (reg_write(0x12, 0x44) != 2 || // CTRL3_C: BDU and address increment
        reg_write(REG_CTRL1_XL, 0x40) != 2) {
        printf("IMU configuration failed\n");
        return 1;
    } // Accelerometer on, 104 Hz, +/- 2 g

    while (true) {
        uint8_t raw[6];
        if (reg_read(REG_OUTX_L_A, raw, sizeof raw) != sizeof raw) {
            printf("IMU read failed; sample skipped\n");
            sleep_ms(200);
            continue;
        }

        int16_t x = (int16_t)((raw[1] << 8) | raw[0]);
        int16_t y = (int16_t)((raw[3] << 8) | raw[2]);
        int16_t z = (int16_t)((raw[5] << 8) | raw[4]);

        // 0.061 mg per count at +/- 2 g, converted to g
        printf("%.3f  %.3f  %.3f\n", x * 0.061f / 1000.0f,
                                      y * 0.061f / 1000.0f,
                                      z * 0.061f / 1000.0f);
        sleep_ms(200);
    }
}