/*
 * test_spi.c — SPI full-duplex loopback test for wiringJON.
 *
 *	Requires a physical wire on the 40-pin header:
 *	  physical pin 19 (SPI0_MOSI) <-> physical pin 21 (SPI0_MISO)
 *
 *	With MOSI tied to MISO, in full duplex the MISO byte received during
 *	clock N is the MOSI byte from clock N-1, so the transfer returns the
 *	pattern shifted by one byte (first byte is undefined -> ignored).
 *
 *	SPI must be enabled in the device tree first:
 *	  sudo /opt/nvidia/jetson-io/jetson-io.py   (configure header, SPI1)
 *	after reboot /dev/spidev0.0 appears. Bus override: WIRINGPI_SPI_BUS.
 *
 *	Exit code 0 = all checks passed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <wiringPiSPI.h>

static int passed = 0, failed = 0 ;

static void check (const char *what, int ok, const char *detail)
{
  printf ("[%s] %-52s %s\n", ok ? "PASS" : "FAIL", what, detail ? detail : "") ;
  if (ok) ++passed ; else ++failed ;
}

/* returns 0 when rx is tx shifted by one byte (MOSI<->MISO wire) */
static int checkShifted (const unsigned char *tx, const unsigned char *rx, int len)
{
  for (int i = 1 ; i < len ; ++i)
    if (rx [i] != tx [i - 1])
      return -1 ;
  return 0 ;
}

static int runSpeed (int speed)
{
  char what [96], detail [64] ;
  unsigned char tx [16], rx [16] ;
  int okAll = 1 ;

  for (int round = 0 ; round < 2 ; ++round)
  {
    for (int i = 0 ; i < 16 ; ++i)
      tx [i] = (unsigned char) (round ? 0xFF - i * 3 : 0xA5 ^ (i * 17)) ;
    memset (rx, 0, sizeof (rx)) ;

    int n = wiringPiSPIDataRW (0, (unsigned char *)tx, 16) ;
    /* wiringPiSPIDataRW reuses the buffer: on return it holds the RX data */
    memcpy (rx, tx, 16) ;

    snprintf (what, sizeof (what), "%d Hz, round %d: DataRW 16 bytes", speed, round) ;
    int ok = (n == 16) ;
    snprintf (detail, sizeof (detail), ok ? NULL : "n=%d errno=%d", n, errno) ;
    check (what, ok, detail) ;
    if (!ok) { okAll = 0 ; continue ; }

    snprintf (what, sizeof (what), "%d Hz, round %d: MOSI->MISO shift pattern", speed, round) ;
    ok = (checkShifted (tx, rx, 16) == 0) ;
    check (what, ok, ok ? NULL : "unexpected rx (wire 19<->21 missing?)") ;
    if (!ok) okAll = 0 ;
  }
  return okAll ;
}

int main (void)
{
  int speeds [] = { 100000, 1000000, 8000000 } ;

  int fd = wiringPiSPISetup (0, speeds [0]) ;
  if (fd < 0)
  {
    printf ("[FAIL] wiringPiSPISetup: %s\n", strerror (errno)) ;
    printf ("  hint: /dev/spidev0.0 does not exist -> enable SPI on the header:\n"
            "          sudo /opt/nvidia/jetson-io/jetson-io.py  (configure SPI1), reboot\n"
            "        and connect the wire between physical pins 19 <-> 21.\n") ;
    return 1 ;
  }
  check ("wiringPiSPISetup channel 0", 1, NULL) ;

  for (unsigned s = 0 ; s < sizeof (speeds) / sizeof (speeds [0]) ; ++s)
    runSpeed (speeds [s]) ;

  printf ("=== %d passed, %d failed ===\n", passed, failed) ;
  return failed ? 1 : 0 ;
}
