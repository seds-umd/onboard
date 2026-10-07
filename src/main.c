#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "imu.h"
#include "mag.h"
#include "baro.h"

#define I2C_SDA_PIN 0
#define I2C_SCL_PIN 1

static void fail(const char *what, const char *why)
{
    printf("%s failed: %s -- halting\n", what, why);
    while (true) { tight_loop_contents(); }
}

int main(void)
{
    stdio_init_all();
    while (!stdio_usb_connected()) { tight_loop_contents(); }
    printf("Pico 2 sensor demo: LSM6DSOX");

    i2c_init(i2c0, 400000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // = IMU =
    lsm6dsox_t *imu = lsm6dsox_create(i2c0, 0x6A);
    if (!imu || lsm6dsox_init(imu) != LSM6DSOX_OK)
        fail("LSM6DSOX init", lsm6dsox_strerror(lsm6dsox_init(imu)));
/*
    //  Magnetometer
    mmc5603_t *mag = mmc5603_create(i2c0, 0x30);
    mmc5603_error_t e = mmc5603_init(mag);
    if (e != MMC5603_OK) fail("MMC5603 init", mmc5603_strerror(e));
    mmc5603_set_continuous(mag, true);      // free-running, no blocking wait
    mmc5603_set_rate(mag, 10);             // 10 Hz

    //  Barometer
    mpl3115a2_t *baro = mpl3115a2_create(i2c0, 0x60);
    mpl3115a2_error_t be = mpl3115a2_init(baro);
    if (be != MPL3115A2_OK) fail("MPL3115A2 init", mpl3115a2_strerror(be));
    mpl3115a2_set_sea_level_pressure(baro, 101326.0f);  // default ref


    mmc5603_data_t  mag_d;
    mpl3115a2_data_t baro_d;*/
    lsm6dsox_data_t imu_d;
    while (true) {
        lsm6dsox_read(imu, &imu_d);
        printf("IMU  A[g] %6C.2f %6.2f %6.2f  G[dps] %7.1f %7.1f %7.1f\n", imu_d.ax, imu_d.ay, imu_d.az, imu_d.gx, imu_d.gy, imu_d.gz);

        printf("---\n");
        sleep_ms(100);
    }
}
