# wiringJON — wiringPi for the Jetson Orin Nano

A **drop-in wiringPi replacement** for the NVIDIA Jetson Orin Nano / Orin NX
Developer Kit (carrier P3768/P3509, module P3767, Tegra234). Same headers,
same `-lwiringPi`, same program behaviour — different backend: the Linux
GPIO character device via **libgpiod** instead of Broadcom `/dev/mem`
registers.

    # your existing code compiles and runs unchanged:
    #include <wiringPi.h>
    int main (void) {
      wiringPiSetup () ;
      pinMode (0, OUTPUT) ;
      for (;;) { digitalWrite (0, HIGH); delay (500); digitalWrite (0, LOW); delay (500); }
    }
    gcc blink.c -o blink -lwiringPi

## Status (milestone 1)

Done and hardware-verified:

- `wiringPiSetup`, `wiringPiSetupGpio`, `wiringPiSetupPhys`,
  `wiringPiSetupPinType`, `wiringPiSetupGpioDevice` — all numbering modes
- `pinMode` (INPUT/OUTPUT/PM_OFF), `digitalRead`, `digitalWrite`
- `pullUpDnControl` (best-effort, see limitations)
- `wiringPiISR` / `wiringPiISR2` / `wiringPiISRStop` / `waitForInterrupt2`
  on gpiod edge events (threaded callbacks, like the original)
- `delay`, `delayMicroseconds`, `millis`, `micros`, `piMicros64`
- `pulseIn`/`pulseInNS`, `frequencyIn`
- threads: `piThreadCreate`, `piLock`/`piUnlock`, `piHiPri`
- expander node system (`wiringPiNewNode`/`wiringPiFindNode`) for devLib

Planned: devLib (I2C/SPI/serial/softPwm), `gpio` CLI with `readall`,
hardware PWM (`/sys/class/pwm`, pins 15/32/33), udev rules, .deb package.

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

- **No `wiringPiSetupSys()`** — this kernel (6.8-tegra) has no
  `/sys/class/gpio`. Use the other setup functions.
- **Pulls**: Tegra header pull resistors are usually fixed by the pinmux;
  `pullUpDnControl()` forwards the request to the kernel and reports
  (once) when the hardware refuses.
- **PWM/clock**: `pwmWrite`/`GPIO_CLOCK` are stubs with diagnostics until
  the `/sys/class/pwm` backend lands. Hardware PWM only on pins 15/32/33.
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

## Repository layout

    wiringPi/       core library (wiringPi.c = gpiod backend, pinmap.c)
    tests/          hardware loopback tests
    scripts/        test-bench helpers (SSH runner, GPIO probes)
    third_party/    vendored upstream wiringPi 3.20 (headers, devLib, gpio
                    tool — used as the API contract and for later phases)

## Pin map provenance

Cross-verified between NVIDIA's Jetson.GPIO `gpio_pin_data.py`
(`JETSON_ORIN_NX_PIN_DEFS`, also used for Orin Nano), the kernel driver
`drivers/gpio/gpio-tegra186.c` `tegra234_main_ports[]` cumulative pin
count, and live `gpioinfo` on a P3768+P3767-0005-super
(`gpiochip0` = tegra234-gpio, 164 lines; line names `PG.06`, `PAC.06`, …).

## License

LGPLv3 — same as wiringPi (see COPYING.LESSER). API headers from
https://github.com/WiringPi/WiringPi (Gordon Henderson & contributors).
