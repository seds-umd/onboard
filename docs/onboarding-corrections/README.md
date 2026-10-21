# Avionics onboarding corrections

Updated 21 October 2026. Open [Start here](start-here.html) for the five-stage project or [The sessions](sessions.html) for the shorter build, flash and IMU walkthrough. These are corrected saved guides. The older shared artifact URLs have not been republished.

Use a **non-wireless Raspberry Pi Pico 2** and the **LSM6DSOX breakout** for these instructions. A compile is not a successful flash or a working sensor; show those separately at the meeting.

## Short route for tonight

1. Build and flash blink; show the actual LED blinking.
2. Follow the diagram for the program being built. The standalone IMU example uses **GP4 SDA / GP5 SCL**. The `onboard` repository uses **GP0 SDA / GP1 SCL**. Both use I2C0, but wiring must match the selected program.
3. Power the tutorial breakout from 3V3 OUT and GND. Read the breakout labels and use a pin marked GND; ground pins are not evenly spaced.
4. Open USB serial for the `onboard` demo. It intentionally waits for a terminal before initialization.
5. Show a valid sensor identity and changing measurements, plus the branch or patch that produced them. These are tutorial programs, not the motherboard flight firmware.

## Build the corrected repository example

The supplied `onboard-source/` directory contains the upstream source at commit `01c49fb275a91b4513ef9af124164a642fa63353` with the small main-program and gyro-range patch applied. No generated upstream build cache is included. After completing compiler and SDK setup, run these commands from this bundle's directory:

```sh
cmake -S onboard-source -B build-onboard-pico2 -DPICO_BOARD=pico2
cmake --build build-onboard-pico2 -j4
```

Flash `build-onboard-pico2/pico_sensors.uf2`, then open its serial port. Use a new build directory if you change machines, SDKs or boards.

Alternatively, apply [onboard-main.patch](onboard-main.patch) once to a clean checkout at the commit above, as shown before the build commands in the sessions guide. Run `git apply --check` first. Do not force a rejected patch or discard other work to apply it. The existing local checkout and GitHub main have not been modified.

## Build the standalone examples

```sh
cmake -S examples -B build-examples-pico2 -DPICO_BOARD=pico2
cmake --build build-examples-pico2 -j4
```

The outputs are `blink.uf2`, `sos.uf2` and `imu.uf2` in `build-examples-pico2/`. The exact C snippets displayed in Start here were extracted to `examples/` and built. The SOS circuit uses an external LED on **GP15 through 330 ohms**, plus an active-low GP14 button. The onboard LED is used only for blink.

## What changed

- Repaired malformed SOS HTML/C and made the LED pin match the wiring diagram.
- Made button release cancel during dashes and gaps within about 5 ms. Added a 20 ms stable press before starting, and corrected Morse gaps.
- Selected Pico 2 explicitly, corrected its BOOTSEL drive name and pin-map chip label, removed leftover MicroPython syntax, and replaced destructive build-cache deletion with a fresh directory.
- Clarified wireless board support and the two different tutorial I2C pin pairs.
- Corrected the repository's first acceleration format, checked initialization only once, and skipped failed/not-ready reads instead of printing stale or uninitialized sample data.
- Corrected all four gyroscope full-scale encodings to match the hardware register, keeping conversion sensitivity consistent with the selected range.
- Made the standalone IMU stop on an incorrect identity/configuration failure, skip incomplete reads, and enable block-data update with address increment for coherent byte pairs. See the [LSM6DSOX datasheet, CTRL3_C](https://www.st.com/resource/en/datasheet/lsm6dsox.pdf).

## Verification and limits

On 21 October: clean Pico 2 target builds passed for blink, SOS, standalone IMU, and the patched repository sensor demo. Host checks passed for the entire SOS waveform and cancellation at each of 6,800 possible millisecond release times; allocation/init/read-error, no-data and formatted-success paths passed for the patched demo. Four-range gyro register-byte and raw-count conversion checks also passed. The patch applies cleanly to its stated upstream source. The detailed local receipt is `verification.json`.

No board was flashed and no physical sensor/button demonstration was performed in this correction pass. The repository's lower-level drivers were not broadly refactored or qualified as flight firmware. Next student contributions can cover setup documentation, a demonstrated range change, or another connected breakout with error handling rather than duplicating the supplied fix.

Board-target and LED references: [Raspberry Pi C/C++ SDK guide](https://www.raspberrypi.com/documentation/microcontrollers/c_sdk.html).
