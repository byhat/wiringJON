/*
 * test_gpio.c:
 *	Hardware loopback test for wiringJON.
 *
 *	Requires a physical wire on the 40-pin header between
 *	  physical pin 32 (GPIO07 / PG.06)  -> driver
 *	  physical pin 7  (GPIO09 / PAC.06) -> reader
 *
 *	Usage: test_gpio <pins|gpio|phys|pinerr|hold>
 *	  pins — wiringPiSetup()    numbering (driver=wPi 26, reader=wPi 7)
 *	  gpio — wiringPiSetupGpio()numbering (driver=12,   reader=4)
 *	  phys — wiringPiSetupPhys()numbering (driver=32,   reader=7)
 *	  pinerr — exercises the fatal path for a non-GPIO pin (expect exit!=0)
 *	  hold — drives the driver pin HIGH for 3 s (for external gpioinfo check)
 *
 *	Exit code 0 = all checks passed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiringPi.h>

static int passed = 0, failed = 0 ;

static void check (const char *what, int ok, const char *detail)
{
  printf ("[%s] %-46s %s\n", ok ? "PASS" : "FAIL", what, detail ? detail : "") ;
  if (ok) ++passed ; else ++failed ;
}

static volatile int isrCount = 0 ;
static void isrCb (void) { ++isrCount ; }

static PI_THREAD (toggler)
{
  for (int i = 0 ; i < 3 ; ++i)
  {
    digitalWrite (26, HIGH) ;		/* wPi numbering used by the caller */
    delay (50) ;
    digitalWrite (26, LOW) ;
    delay (50) ;
  }
  return NULL ;
}

int main (int argc, char **argv)
{
  const char *mode = argc > 1 ? argv [1] : "phys" ;

  int (*setupFn) (void) = NULL ;
  int driver = 0, reader = 0 ;

  if      (!strcmp (mode, "pins")) { setupFn = wiringPiSetup    ; driver = 26 ; reader = 7  ; }
  else if (!strcmp (mode, "gpio")) { setupFn = wiringPiSetupGpio; driver = 12 ; reader = 4  ; }
  else if (!strcmp (mode, "phys")) { setupFn = wiringPiSetupPhys; driver = 32 ; reader = 7  ; }
  else if (!strcmp (mode, "pinerr"))
  {
    wiringPiSetupPhys () ;
    printf ("expecting fatal error for physical pin 3 (I2C1_SDA, no GPIO):\n") ;
    pinMode (3, OUTPUT) ;
    printf ("ERROR: pinMode(3) did not fail\n") ;
    return 1 ;
  }
  else if (!strcmp (mode, "hold"))
  {
    wiringPiSetupPhys () ;
    pinMode (32, OUTPUT) ;
    digitalWrite (32, HIGH) ;
    printf ("holding physical pin 32 HIGH for 3 s...\n") ;
    fflush (stdout) ;
    delay (3000) ;
    digitalWrite (32, LOW) ;
    printf ("done\n") ;
    return 0 ;
  }
  else
  {
    fprintf (stderr, "usage: %s <pins|gpio|phys|pinerr|hold>\n", argv [0]) ;
    return 2 ;
  }

  if (setupFn () == -1)
  {
    printf ("setup failed for mode '%s'\n", mode) ;
    return 1 ;
  }
  printf ("=== wiringJON hardware loopback test, numbering mode: %s "
          "(driver pin -> reader pin) ===\n", mode) ;

  /* 1. basic output/input ------------------------------------------------ */
  pinMode (reader, INPUT) ;
  pinMode (driver, OUTPUT) ;

  digitalWrite (driver, HIGH) ; delay (50) ;
  int r1 = digitalRead (reader) ;
  char buf [128] ;
  snprintf (buf, sizeof (buf), "reader reads %d", r1) ;
  check ("digitalWrite HIGH -> digitalRead", r1 == 1, buf) ;

  digitalWrite (driver, LOW) ; delay (50) ;
  int r0 = digitalRead (reader) ;
  snprintf (buf, sizeof (buf), "reader reads %d", r0) ;
  check ("digitalWrite LOW  -> digitalRead", r0 == 0, buf) ;

  /* 2. readback of our own output line ----------------------------------- */
  digitalWrite (driver, HIGH) ; delay (1) ;
  check ("self readback while HIGH", digitalRead (driver) == 1, NULL) ;
  digitalWrite (driver, LOW) ; delay (1) ;
  check ("self readback while LOW", digitalRead (driver) == 0, NULL) ;

  /* 3. fast toggling ------------------------------------------------------ */
  int mism = 0 ;
  for (int i = 0 ; i < 200 ; ++i)
  {
    digitalWrite (driver, i & 1) ;
    delayMicroseconds (200) ;
    if (digitalRead (reader) != (i & 1))
      ++mism ;
  }
  snprintf (buf, sizeof (buf), "%d mismatches over 200 cycles", mism) ;
  check ("200 toggle cycles follow the wire", mism == 0, buf) ;

  /* 4. write speed (informational) ---------------------------------------- */
  unsigned int t0 = micros () ;
  for (int i = 0 ; i < 10000 ; ++i)
    digitalWrite (driver, i & 1) ;
  unsigned int t1 = micros () ;
  printf ("        info: digitalWrite %.2f us/op\n", (t1 - t0) / 10000.0) ;

  /* 5. pull control (informational on Tegra: pulls may be pinmux-fixed) --- */
  pinMode (driver, INPUT) ;			/* release the wire */
  pullUpDnControl (reader, PUD_DOWN) ;
  delay (10) ;
  int pd = digitalRead (reader) ;
  pullUpDnControl (reader, PUD_UP) ;
  delay (10) ;
  int pu = digitalRead (reader) ;
  printf ("        info: idle level with PUD_DOWN=%d, PUD_UP=%d "
          "(0/1 = pull works, constant = fixed by pinmux)\n", pd, pu) ;

  /* 6. interrupts ---------------------------------------------------------- */
  pinMode (driver, OUTPUT) ;
  isrCount = 0 ;
  wiringPiISR (reader, INT_EDGE_BOTH, isrCb) ;
  for (int i = 0 ; i < 100 ; ++i)
  {
    digitalWrite (driver, HIGH) ; delay (5) ;
    digitalWrite (driver, LOW)  ; delay (5) ;
  }
  delay (150) ;
  int c = isrCount ;
  wiringPiISRStop (reader) ;
  snprintf (buf, sizeof (buf), "%d interrupts (expect >=190)", c) ;
  check ("wiringPiISR both edges x100 toggles", c >= 190 && c <= 210, buf) ;

  /* 7. waitForInterrupt2 + piThreadCreate (wPi numbering for the thread) --- */
  if (!strcmp (mode, "pins"))
  {
    pinMode (26, OUTPUT) ;
    struct WPIWfiStatus st = { 0, 0, 0, 0 } ;
    piThreadCreate (toggler) ;
    st = waitForInterrupt2 (7, INT_EDGE_RISING, 2000, 0) ;
    snprintf (buf, sizeof (buf), "statusOK=%d edge=%d", st.statusOK, st.edge) ;
    check ("waitForInterrupt2 rising edge", st.statusOK == 1, buf) ;
    waitForInterruptClose (7) ;
    pinMode (26, OUTPUT) ;
  }

  /* 8. timing -------------------------------------------------------------- */
  unsigned int m0 = millis () ;
  delay (250) ;
  unsigned int dm = millis () - m0 ;
  snprintf (buf, sizeof (buf), "delta %u ms", dm) ;
  check ("delay(250) accuracy", dm >= 240 && dm <= 600, buf) ;

  unsigned int u0 = micros () ;
  for (int i = 0 ; i < 100 ; ++i)
    delayMicroseconds (1000) ;
  unsigned int du = micros () - u0 ;
  snprintf (buf, sizeof (buf), "100x1000us took %u us", du) ;
  check ("delayMicroseconds accuracy", du >= 95000 && du <= 200000, buf) ;

  printf ("=== %s: %d passed, %d failed ===\n", failed ? "RESULT: FAIL" : "RESULT: PASS", passed, failed) ;
  return failed ? 1 : 0 ;
}
