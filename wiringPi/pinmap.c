/*
 * pinmap.c:
 *	Jetson Orin Nano / Orin NX Developer Kit 40-pin header map.
 *	See pinmap.h for provenance and license.
 ***********************************************************************
 * GPIO rows: (phys, wpi, bcm, name, socPin, chip, line) — wpi numbers
 * follow Raspberry Pi physical positions; bcm numbers follow NVIDIA
 * "GPIOxx" documentation names (Jetson.GPIO CVM names).
 *
 * Non-GPIO rows keep documentation names for diagnostics/readall.
 * Pins 3/5 (I2C1), 8/10 (UART1), 27/28 (I2C) have no GPIO mux on this
 * carrier — attempting pinMode() on them fails with a clear message.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pinmap.h"

#define NG(name) {0,-1,-1,(name),"", "", -1, WJON_PIN_NC}

static const wjonPinDescr orinNanoPins[] = {
  /* phys wpi  bcm  name         socPin    chip        line  type */
  {  1, -1, -1, "3V3",         "",        "",          -1, WJON_PIN_3V3 },
  {  2, -1, -1, "5V",          "",        "",          -1, WJON_PIN_5V  },
  {  3, -1, -1, "I2C1_SDA",    "",        "",          -1, WJON_PIN_I2C },
  {  4, -1, -1, "5V",          "",        "",          -1, WJON_PIN_5V  },
  {  5, -1, -1, "I2C1_SCL",    "",        "",          -1, WJON_PIN_I2C },
  {  6, -1, -1, "GND",         "",        "",          -1, WJON_PIN_GND  },
  {  7,  7,  4, "GPIO09",      "PAC.06",  "gpiochip0", 144, WJON_PIN_GPIO },
  {  8, -1, -1, "UART1_TX",    "",        "",          -1, WJON_PIN_UART },
  {  9, -1, -1, "GND",         "",        "",          -1, WJON_PIN_GND  },
  { 10, -1, -1, "UART1_RX",    "",        "",          -1, WJON_PIN_UART },
  { 11,  0, 17, "UART1_RTS",   "PR.04",   "gpiochip0", 112, WJON_PIN_GPIO },
  { 12,  1, 18, "I2S0_SCLK",   "PH.07",   "gpiochip0",  50, WJON_PIN_GPIO },
  { 13,  2, 27, "SPI1_SCK",    "PY.00",   "gpiochip0", 122, WJON_PIN_GPIO },
  { 14, -1, -1, "GND",         "",        "",          -1, WJON_PIN_GND  },
  { 15,  3, 22, "GPIO12",      "PN.01",   "gpiochip0",  85, WJON_PIN_GPIO },
  { 16,  4, 23, "SPI1_CS1",    "PY.04",   "gpiochip0", 126, WJON_PIN_GPIO },
  { 17, -1, -1, "3V3",         "",        "",          -1, WJON_PIN_3V3 },
  { 18,  5, 24, "SPI1_CS0",    "PY.03",   "gpiochip0", 125, WJON_PIN_GPIO },
  { 19, 12, 10, "SPI0_MOSI",   "PZ.05",   "gpiochip0", 135, WJON_PIN_GPIO },
  { 20, -1, -1, "GND",         "",        "",          -1, WJON_PIN_GND  },
  { 21, 13,  9, "SPI0_MISO",   "PZ.04",   "gpiochip0", 134, WJON_PIN_GPIO },
  { 22,  6, 25, "SPI1_MISO",   "PY.01",   "gpiochip0", 123, WJON_PIN_GPIO },
  { 23, 14, 11, "SPI0_SCK",    "PZ.03",   "gpiochip0", 133, WJON_PIN_GPIO },
  { 24, 10,  8, "SPI0_CS0",    "PZ.06",   "gpiochip0", 136, WJON_PIN_GPIO },
  { 25, -1, -1, "GND",         "",        "",          -1, WJON_PIN_GND  },
  { 26, 11,  7, "SPI0_CS1",    "PZ.07",   "gpiochip0", 137, WJON_PIN_GPIO },
  { 27, -1, -1, "I2C_SDA",     "",        "",          -1, WJON_PIN_I2C },
  { 28, -1, -1, "I2C_SCL",     "",        "",          -1, WJON_PIN_I2C },
  { 29, 21,  5, "GPIO01",      "PQ.05",   "gpiochip0", 105, WJON_PIN_GPIO },
  { 30, -1, -1, "GND",         "",        "",          -1, WJON_PIN_GND  },
  { 31, 22,  6, "GPIO11",      "PQ.06",   "gpiochip0", 106, WJON_PIN_GPIO },
  { 32, 26, 12, "GPIO07",      "PG.06",   "gpiochip0",  41, WJON_PIN_GPIO },
  { 33, 23, 13, "GPIO13",      "PH.00",   "gpiochip0",  43, WJON_PIN_GPIO },
  { 34, -1, -1, "GND",         "",        "",          -1, WJON_PIN_GND  },
  { 35, 24, 19, "I2S0_FS",     "PI.02",   "gpiochip0",  53, WJON_PIN_GPIO },
  { 36, 27, 16, "UART1_CTS",   "PR.05",   "gpiochip0", 113, WJON_PIN_GPIO },
  { 37, 25, 26, "SPI1_MOSI",   "PY.02",   "gpiochip0", 124, WJON_PIN_GPIO },
  { 38, 28, 20, "I2S0_SDIN",   "PI.01",   "gpiochip0",  52, WJON_PIN_GPIO },
  { 39, -1, -1, "GND",         "",        "",          -1, WJON_PIN_GND  },
  { 40, 29, 21, "I2S0_SDOUT",  "PI.00",   "gpiochip0",  51, WJON_PIN_GPIO },
  {  0,  0,  0, NULL,          NULL,      NULL,           0, WJON_PIN_NC   }
};

/* Modules whose carrier wiring uses the table above (see
 * compats_jetson_orins_nx / compats_jetson_orins_nano in Jetson.GPIO). */
static const char *supportedModules[] = {
  "p3767-0000", "p3767-0001",          /* Orin NX  8/16GB        */
  "p3767-0003", "p3767-0004", "p3767-0005",  /* Orin Nano 4/8GB  */
  NULL
};

static int  boardOk    = 0;
static char boardNameBuf[128] = "unknown";

wjonPinDescr wjonPins[sizeof(orinNanoPins) / sizeof(orinNanoPins[0])];

const char *wjonBoardName(void) { return boardOk ? boardNameBuf : NULL; }

const wjonPinDescr *wjonByPhys(int phys)
{
  if (!boardOk) return NULL;
  for (const wjonPinDescr *p = orinNanoPins; p->phys; ++p)
    if (p->phys == phys) return p;
  return NULL;
}

const wjonPinDescr *wjonByWpi(int wpi)
{
  if (!boardOk) return NULL;
  for (const wjonPinDescr *p = orinNanoPins; p->phys; ++p)
    if (p->type == WJON_PIN_GPIO && p->wpi == wpi) return p;
  return NULL;
}

const wjonPinDescr *wjonByBcm(int bcm)
{
  if (!boardOk) return NULL;
  for (const wjonPinDescr *p = orinNanoPins; p->phys; ++p)
    if (p->type == WJON_PIN_GPIO && p->bcm == bcm) return p;
  return NULL;
}

/* read a whole device-tree property style file with NUL-separated entries */
static int dtRead(const char *path, char *buf, int len)
{
  FILE *f = fopen(path, "rb");
  if (!f) return -1;
  int n = (int)fread(buf, 1, len - 1, f);
  fclose(f);
  if (n <= 0) return -1;
  buf[n] = '\0';
  return n;
}

int wjonBoardInit(void)
{
  if (boardOk) return 0;

  char compat[2048];
  int n = dtRead("/proc/device-tree/compatible", compat, sizeof(compat));
  if (n <= 0) {
    fprintf(stderr, "wiringJON: cannot read /proc/device-tree/compatible\n");
    return -1;
  }
  for (int i = 0; i < n; i++)       /* NUL -> newline for strstr matching */
    if (compat[i] == '\0') compat[i] = '\n';
  compat[n] = '\0';

  int found = 0;
  for (const char **m = supportedModules; *m; ++m)
    if (strstr(compat, *m)) { found = 1; break; }
  if (!found) {
    fprintf(stderr,
      "wiringJON: board is not a supported Jetson Orin NX/Nano devkit.\n"
      "           /proc/device-tree/compatible first entry: %s\n", compat);
    return -1;
  }

  if (dtRead("/proc/device-tree/model", boardNameBuf, sizeof(boardNameBuf)) <= 0)
    strncpy(boardNameBuf, "Jetson Orin (unknown model)", sizeof(boardNameBuf) - 1);

  /* publish the live table for the gpio tool / readall */
  memcpy((void *)wjonPins, orinNanoPins, sizeof(orinNanoPins));

  boardOk = 1;
  return 0;
}
