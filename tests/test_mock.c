/*
 * test_mock.c — host-side (no hardware) logic tests for wiringJON,
 * linked against tests/mock_gpiod.c which emulates the Jetson gpiochips
 * and the bench wire (header pin 32 <-> pin 7, lines 41 <-> 144).
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wiringPi.h>
#include <pinmap.h>

static int passed = 0, failed = 0 ;

static void check (const char *what, int ok)
{
  printf ("[%s] %s\n", ok ? "PASS" : "FAIL", what) ;
  if (ok) ++passed ; else ++failed ;
}

static volatile int isrHits = 0 ;
static void isrCb (void) { ++isrHits ; }

static int loopTest (int (*setupFn) (void), const char *mode,
                     int driver, int reader)
{
  if (setupFn () != 0)
  {
    printf ("[FAIL] setup %s\n", mode) ;
    ++failed ;
    return 1 ;
  }

  char what [96] ;
  pinMode (reader, INPUT) ;
  pinMode (driver, OUTPUT) ;

  digitalWrite (driver, HIGH) ;
  snprintf (what, sizeof (what), "%s: digitalWrite HIGH crosses the mock wire", mode) ;
  check (what, digitalRead (reader) == 1) ;

  digitalWrite (driver, LOW) ;
  snprintf (what, sizeof (what), "%s: digitalWrite LOW crosses the mock wire", mode) ;
  check (what, digitalRead (reader) == 0) ;

  int mism = 0 ;
  for (int i = 0 ; i < 100 ; ++i)
  {
    digitalWrite (driver, i & 1) ;
    if (digitalRead (reader) != (i & 1))
      ++mism ;
  }
  snprintf (what, sizeof (what), "%s: 100 toggle cycles, %d mismatches", mode, mism) ;
  check (what, mism == 0) ;
  return 0 ;
}

int main (void)
{
  setenv ("WIRINGJON_FORCE_BOARD", "1", 1) ;	/* no device tree on x86 */
  wjonBoardInit () ;				/* pinmap lookups need this first */

  /* pinmap sanity: the three numbering modes agree on the two bench pins */
  const wjonPinDescr *p7 = wjonByPhys (7), *p32 = wjonByPhys (32) ;
  check ("pinmap: pin7  = PAC.06 line 144 (wpi 7,  bcm 4)",
         p7  && p7->line == 144 && p7->wpi == 7  && p7->bcm == 4 &&
         strcmp (p7->socPin, "PAC.06") == 0) ;
  check ("pinmap: pin32 = PG.06 line 41 (wpi 26, bcm 12)",
         p32 && p32->line == 41 && p32->wpi == 26 && p32->bcm == 12 &&
         strcmp (p32->socPin, "PG.06") == 0) ;
  check ("pinmap: lookups by wpi/bcm/phys agree",
         wjonByWpi (26) == p32 && wjonByBcm (12) == p32 && wjonByPhys (32) == p32) ;
  check ("pinmap: power pins are not GPIO",
         wjonByPhys (1)->type == WJON_PIN_3V3 && wjonByPhys (6)->type == WJON_PIN_GND &&
         wjonByPhys (3)->type == WJON_PIN_I2C) ;

  /* core loopback in all numbering modes (mock wire) */
  loopTest (wiringPiSetup,     "wPi",  26, 7) ;
  loopTest (wiringPiSetupGpio, "BCM",  12, 4) ;
  loopTest (wiringPiSetupPhys, "phys", 32, 7) ;

  /* back to phys numbering for the rest */
  wiringPiSetupPhys () ;
  pinMode (7, INPUT) ;
  pinMode (32, OUTPUT) ;

  /* ISR through the mock wire */
  isrHits = 0 ;
  int r = wiringPiISR (7, INT_EDGE_BOTH, isrCb) ;
  check ("wiringPiISR registers", r == 0) ;
  for (int i = 0 ; i < 10 ; ++i)
  {
    digitalWrite (32, HIGH) ;
    delay (2) ;
    digitalWrite (32, LOW) ;
    delay (2) ;
  }
  delay (50) ;
  check ("ISR callbacks fired on both edges", isrHits >= 18) ;
  wiringPiISRStop (7) ;

  /* waitForInterrupt2 */
  struct WPIWfiStatus st = waitForInterrupt2 (7, INT_EDGE_RISING, 100, 0) ;
  check ("waitForInterrupt2 timeout path (no driver)", st.statusOK == 0) ;
  waitForInterruptClose (7) ;
  pinMode (32, OUTPUT) ;

  /* node system: dummy node routes without touching hardware */
  struct wiringPiNodeStruct *node = wiringPiNewNode (64, 8) ;
  digitalWrite (64, HIGH) ;
  check ("node digitalWrite routed (no crash, LOW readback)",
         node && digitalRead (64) == LOW) ;
  check ("wiringPiFindNode finds pin 68", wiringPiFindNode (68) == node) ;

  /* timing */
  unsigned m0 = millis () ;
  delay (100) ;
  unsigned dm = millis () - m0 ;
  check ("delay(100) within 90..500 ms", dm >= 90 && dm <= 500) ;
  unsigned u0 = micros () ;
  delayMicroseconds (1000) ;
  unsigned du = micros () - u0 ;
  check ("delayMicroseconds(1000) within 900..5000 us", du >= 900 && du <= 5000) ;

  /* version */
  int vMaj = 0, vMin = 0 ;
  wiringPiVersion (&vMaj, &vMin) ;
  check ("wiringPiVersion reports 3.20", vMaj == 3 && vMin == 20) ;

  printf ("=== mock tests: %d passed, %d failed ===\n", passed, failed) ;
  return failed ? 1 : 0 ;
}
