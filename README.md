# Pico 2 onboarding

Use a non-wireless Raspberry Pi Pico 2 and the LSM6DSOX tutorial breakout. The repository demo uses I2C0 GP0 (SDA), GP1 (SCL), 3V3 OUT and GND. It waits for a USB serial terminal before initializing the sensor.

See [corrected guides and standalone examples](docs/onboarding-corrections/README.md) for setup, blink, button/SOS and IMU exercises. The source in this branch fixes the gyroscope range encoding, formatted acceleration output and failed-sample handling. Tutorial code is separate from flight firmware.

Configure with a complete Pico SDK and a fresh build directory:

```sh
cmake -S . -B build-pico2-fresh -DPICO_BOARD=pico2
cmake --build build-pico2-fresh -j4
```

Flash the resulting `pico_sensors.uf2` and open its serial port. Do not reuse or delete the tracked `build/` cache. Use a different fresh directory if the one above already belongs to another SDK or target.

Portable behavior checks:

```sh
sh tests/onboarding/run.sh
```

These check SOS timing/release, main error handling and all gyro range encodings/scales. Compilation and host checks do not demonstrate a flashed board or physical sensor operation.
