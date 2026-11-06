# wiringJON — wiringPi for the Jetson Orin Nano

A **drop-in wiringPi replacement** for the NVIDIA Jetson Orin Nano / Orin NX
Developer Kit (carrier P3768/P3509, module P3767, Tegra234). Same headers,
same `-lwiringPi`, same program behaviour — different backend: the Linux
GPIO character device via **libgpiod** instead of Broadcom `/dev/mem`
registers.

## Hardware verification log (2026-11-06, Orin Nano Super devkit, R39)

Verified against a physical wire between header pins 32 (GPIO07) and 7
(GPIO09):

- `test_gpio phys/gpio/pins`: **25/25 checks PASS** — static levels,
  self-readback, 200 toggle cycles (0 mismatches), pull report, ISR
  (200 interrupts from 100 toggles), `waitForInterrupt2`, timing
- `digitalWrite` latency ≈ 3.2 µs/op through the chardev backend
- `softPwm` 50% duty → 99 rising edges/s counted through the wire
- `gpio -v`, `gpio readall` (live table), `gpio -1 mode/write/read`
  end-to-end loopback through the CLI
- `gpioinfo` shows consumer `wiringPi` on lines requested by the library
- mock CI on the host: 21/21 logic tests + readall render

### The padctl tristate quirk (important for this board)

The Engineering Reference Developer Kit (R39-era L4T) leaves all 40-pin
header pads with the **padctl TRISTATE bit set** — inputs read, but no
pad can drive anything. There is no pinmux DT node to fix it, so
wiringJON handles it itself:

- `wiringPiSetup*()` fixes all 22 header pads when running as root;
- `./build` installs a `wiringjon-padfix` systemd oneshot that does the
  same at every boot, so normal (gpio-group) users work out of the box.

Bits cleared/set per pad: SFIO-select=0 (GPIO), tristate=0, e_input=1;
pull/mux bits preserved (addresses from the Jetson.GPIO pin data,
cross-checked with `pinctrl-tegra234.c`).

    # your existing code compiles and runs unchanged:
    #include <wiringPi.h>
    int main (void) {
      wiringPiSetup () ;
      pinMode (0, OUTPUT) ;
      for (;;) { digitalWrite (0, HIGH); delay (500); digitalWrite (0, LOW); delay (500); }
    }
    gcc blink.c -o blink -lwiringPi

## Status

Core (milestone 1) — written, hardware verification pending board return:

- `wiringPiSetup`, `wiringPiSetupGpio`, `wiringPiSetupPhys`,
  `wiringPiSetupPinType`, `wiringPiSetupGpioDevice` — all numbering modes
- `pinMode` (INPUT/OUTPUT/PM_OFF/PWM_OUTPUT), `digitalRead`, `digitalWrite`
- `pullUpDnControl` (best-effort, see limitations)
- `wiringPiISR` / `wiringPiISR2` / `wiringPiISRStop` / `waitForInterrupt2`
  on gpiod edge events (threaded callbacks, like the original)
- `delay`, `delayMicroseconds`, `millis`, `micros`, `piMicros64`
- `pulseIn`/`pulseInNS`, `frequencyIn`
- threads: `piThreadCreate`, `piLock`/`piUnlock`, `piHiPri`
- expander node system (`wiringPiNewNode`/`wiringPiFindNode`) + 25 drivers
  from wiringPi 3.20 (`mcp23017Setup`, `pcf8574Setup`, `ads1115Setup`, …)

Full stack (written, compiles, on-board check pending):

- **devLib** (`libwiringPiDev`): lcd, lcd128x64, ds1302, maxdetect, piNes,
  gertboard, piFace, piGlow, scrollPhat
- **`gpio` CLI**: `readall` (Orin Nano 40-pin table), `mode/read/write/
  toggle/blink/pwm/pwmTone/wfi/qmode/i2cd/...`; sysfs commands
  (`export/edge/...`) report deprecated, like wiringPi 3.2+ on modern
  kernels
- **hardware PWM** via `/sys/class/pwm` on pins 15/32/33 with wiringPi
  `pwmSetRange/pwmSetClock/pwmWrite` semantics
  (`WIRINGPI_PWM_BASE_HZ` env for exact frequency control)
- **serial/I2C/SPI**: `wiringSerial`, `wiringPiI2C` (+ classic
  `wiringI2C.h` alias), `wiringPiSPI` (+ `wiringSPI.h` alias);
  defaults verified on the devkit: header UART pins 8/10 =
  `/dev/ttyTHS1` (uarta@3100000), header I2C pins 3/5 = `/dev/i2c-1`
  (override `WIRINGPI_I2C_BUS`, check `i2cdetect -l`); SPI needs the
  pin function enabled (`jetson-io.py`) → `/dev/spidev0.x`
  (override `WIRINGPI_SPI_BUS`)
- **wiringPiD** daemon (drcNet), classic **examples**, udev rule, .deb
  template (`./build debian`)

## Numbering schemes

| Function | numbers | pin 32 (wire) | pin 7 (wire) |
|---|---|---|---|
| `wiringPiSetup()` | wiringPi, same physical positions as RPi | 26 | 7 |
| `wiringPiSetupGpio()` | BCM-style = NVIDIA `GPIOxx` names | 12 (`GPIO07`) | 4 (`GPIO09`) |
| `wiringPiSetupPhys()` | physical pin 1–40 | 32 | 7 |

The `wpi` numbers are assigned so that Raspberry Pi tutorials using
wiringPi numbers hit the same physical header positions. The `gpio`
(BCM-style) numbers match NVIDIA documentation and the Python
Jetson.GPIO library.

## Jetson limitations (documented, not hidden)

- **`wiringPiSetupSys()`** — this kernel (6.8-tegra) has no
  `/sys/class/gpio`; the call works as an alias for GPIO (BCM-style)
  numbering with a one-time notice, so old scripts keep running.
- **Pulls**: Tegra header pull resistors are usually fixed by the pinmux;
  `pullUpDnControl()` forwards the request to the kernel and reports
  (once) when the hardware refuses.
- **PWM/clock**: hardware PWM only on pins 15/32/33 and requires the PWM
  pin function enabled in the device tree
  (`sudo /opt/nvidia/jetson-io/jetson-io.py`); `gpioClockSet` has no
  Jetson equivalent. Balanced PWM mode is treated as mark-space.
- Pins 3/5 (I2C1), 8/10 (UART1), 27/28 (I2C) have **no GPIO mux** on this
  carrier: `pinMode()` on them fails with a clear message.
- `digitalWrite`/`digitalRead` auto-request the line if `pinMode()` was
  not called first (the gpiod API requires a request; the original wrote
  hardware registers regardless).

## Build & install (on the Jetson)

    sudo apt install -y libgpiod-dev build-essential
    ./build                # builds + installs to /usr/local, runs ldconfig

Run the hardware loopback test (needs a wire between physical pins 32
and 7 on the 40-pin header):

    make -C tests
    tests/test_gpio phys
    tests/test_gpio gpio
    tests/test_gpio pins

Access without sudo: user must be in the `gpio` group (default on L4T).

## Run the hardware tests

    make -C tests

| Test | Wire on the header | Prerequisites |
|---|---|---|
| `test_gpio phys\|gpio\|pins` | pin 32 ↔ pin 7 | none |
| `test_serial` | pin 8 (TX) ↔ pin 10 (RX) | UART enabled on 8/10 (`jetson-io.py`); device `/dev/ttyTHS1`, override `WIRINGPI_SERIAL_DEV` |
| `test_spi` | pin 19 (MOSI) ↔ pin 21 (MISO) | SPI enabled via `jetson-io.py` (creates `/dev/spidev0.0`; pins 19–26 stop being GPIO), override `WIRINGPI_SPI_BUS` |
| `test_i2c` | any I2C device on pins 3/5 (informational) | bus default `/dev/i2c-1`, override `WIRINGPI_I2C_BUS`; check `i2cdetect -l` |

The SPI test exploits full duplex: with MOSI tied to MISO the received
stream is the transmitted stream shifted by one byte.

## Repository layout

    wiringPi/       core library (wiringPi.c = gpiod backend, pinmap.c,
                    pwmJetson.c = sysfs PWM, I2C/SPI/serial/soft*, 25 expanders)
    devLib/         libwiringPiDev (lcd, ds1302, gertboard, ...)
    gpio/           gpio CLI (readall with the Orin Nano table)
    wiringPiD/      drcNet daemon
    tests/          hardware loopback tests
    examples/       classic wiringPi examples, unchanged
    udev/           user-level access rules
    debian-template/ .deb packaging (./build debian)
    scripts/        test-bench helpers (SSH runner, GPIO probes)
    third_party/    vendored upstream wiringPi 3.20 (API contract)

## Pin map provenance

Cross-verified between NVIDIA's Jetson.GPIO `gpio_pin_data.py`
(`JETSON_ORIN_NX_PIN_DEFS`, also used for Orin Nano), the kernel driver
`drivers/gpio/gpio-tegra186.c` `tegra234_main_ports[]` cumulative pin
count, and live `gpioinfo` on a P3768+P3767-0005-super
(`gpiochip0` = tegra234-gpio, 164 lines; line names `PG.06`, `PAC.06`, …).

## License

LGPLv3 — same as wiringPi (see COPYING.LESSER). API headers from
https://github.com/WiringPi/WiringPi (Gordon Henderson & contributors).
