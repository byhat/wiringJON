/*
 * pinmap.h:
 *	40-pin header pin map for the NVIDIA Jetson Orin Nano / Orin NX
 *	Developer Kit (carrier P3768 / P3509, module P3767).
 *
 *	This file is part of wiringJON — a wiringPi-compatible GPIO library
 *	for the Jetson Orin Nano.  LGPLv3, see COPYING.LESSER.
 ***********************************************************************
 *
 * Sources for this table (cross-verified):
 *   - NVIDIA Jetson.GPIO python package, gpio_pin_data.py
 *     (JETSON_ORIN_NX_PIN_DEFS, also used for JETSON_ORIN_NANO)
 *   - Linux kernel drivers/gpio/gpio-tegra186.c tegra234_main_ports[]
 *     (cumulative pin counts give the per-port line offsets)
 *   - Line offsets verified live on a P3768+P3767-0005-super (Orin Nano
 *     Super, kernel 6.8.12-1021-tegra): gpiochip0 = tegra234-gpio (164
 *     lines) and line names ("PG.06", "PAC.06", ...) match.
 *
 * Numbering schemes:
 *   phys — physical pin on the 40-pin header (1..40)
 *   wpi  — wiringPi numbers, assigned by the SAME physical positions as
 *          the Raspberry Pi 40-pin header, so that unmodified RPi
 *          tutorials address the same header positions
 *   bcm  — BCM-style numbers, matching the "GPIOxx" names used in
 *          NVIDIA's header documentation and Jetson.GPIO (BOARD=phys)
 ***********************************************************************
 */

#ifndef __WIRINGJON_PINMAP_H__
#define __WIRINGJON_PINMAP_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  WJON_PIN_GPIO = 0,   /* usable as GPIO via libgpiod          */
  WJON_PIN_3V3,        /* 3.3V power rail                      */
  WJON_PIN_5V,         /* 5V power rail                        */
  WJON_PIN_GND,        /* ground                               */
  WJON_PIN_I2C,        /* I2C function, no GPIO mux on this SoC */
  WJON_PIN_UART,       /* UART function, no GPIO mux           */
  WJON_PIN_NC          /* not connected / other                */
} wjonPinType;

typedef struct {
  int           phys;      /* 1..40                                   */
  int           wpi;       /* wiringPi number or -1                   */
  int           bcm;       /* BCM-style number or -1                  */
  const char   *name;      /* documentation name, e.g. "GPIO09"       */
  const char   *socPin;    /* Tegra234 pad, e.g. "PAC.06"             */
  const char   *chip;      /* libgpiod chip name, e.g. "gpiochip0"    */
  int           line;      /* line offset inside chip, -1 if not GPIO */
  const char   *pwmDev;    /* DT node of the PWM controller or NULL   */
  wjonPinType   type;
} wjonPinDescr;

/* Board detection: reads /proc/device-tree, fills pin table.
 * Returns 0 on success, -1 if the board is not supported. */
extern int        wjonBoardInit        (void);
extern const char *wjonBoardName       (void);

/* Lookups return NULL when the pin is not a GPIO-capable header pin. */
extern const wjonPinDescr *wjonByPhys (int phys);
extern const wjonPinDescr *wjonByWpi  (int wpi);
extern const wjonPinDescr *wjonByBcm  (int bcm);

/* All 40 pins, phys order; terminated by an entry with phys == 0.
 * (writable copy filled by wjonBoardInit for the gpio tool / readall) */
extern wjonPinDescr wjonPins[];

#ifdef __cplusplus
}
#endif

#endif
