/*
 * test_serial.c — UART loopback test for wiringJON.
 *
 *	Requires a physical wire on the 40-pin header:
 *	  physical pin 8  (UART1_TX) <-> physical pin 10 (UART1_RX)
 *
 *	Device defaults to /dev/ttyTHS1 (header UART on the Orin Nano devkit);
 *	override with WIRINGPI_SERIAL_DEV=<dev>.
 *
 *	NOTE: if the UART function is not enabled on pins 8/10 in the device
 *	tree, run:  sudo /opt/nvidia/jetson-io/jetson-io.py
 *
 *	Exit code 0 = all checks passed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <wiringSerial.h>

static int passed = 0, failed = 0 ;

static void check (const char *what, int ok, const char *detail)
{
  printf ("[%s] %-52s %s\n", ok ? "PASS" : "FAIL", what, detail ? detail : "") ;
  if (ok) ++passed ; else ++failed ;
}

static int drainEcho (int fd, const char *sent, int len, int msTotal)
{
  char got [256] ;
  int n = 0 ;
  for (int t = 0 ; t < msTotal && n < len ; t += 5)
  {
    while (serialDataAvail (fd) > 0 && n < len)
      got [n++] = (char)serialGetchar (fd) ;
    usleep (5000) ;
  }
  if (n != len)
    return -1 ;
  return memcmp (got, sent, len) ;
}

int main (void)
{
  const char *dev = getenv ("WIRINGPI_SERIAL_DEV") ;
  if (!dev || !*dev)
    dev = "/dev/ttyTHS1" ;

  int bauds [] = { 9600, 115200 } ;
  char buf [128] ;

  for (unsigned b = 0 ; b < sizeof (bauds) / sizeof (bauds [0]) ; ++b)
  {
    int fd = serialOpen (dev, bauds [b]) ;
    snprintf (buf, sizeof (buf), "open %s @ %d", dev, bauds [b]) ;
    if (fd < 0)
    {
      check (buf, 0, strerror (errno)) ;
      printf ("  hint: header UART pins 8/10 must be enabled in the device tree\n"
              "        (sudo /opt/nvidia/jetson-io/jetson-io.py) and the wire must be\n"
              "        between physical pins 8 <-> 10; try WIRINGPI_SERIAL_DEV=<dev>\n") ;
      printf ("=== %d passed, %d failed ===\n", passed, failed) ;
      return 1 ;
    }
    check (buf, 1, NULL) ;

    serialFlush (fd) ;
    usleep (10000) ;

    const char *msg = "wiringJON UART loopback 0123456789\r\n" ;
    serialPuts (fd, msg) ;
    int r = drainEcho (fd, msg, (int)strlen (msg), 300) ;
    snprintf (buf, sizeof (buf), "%d baud: serialPuts echoed back", bauds [b]) ;
    check (buf, r == 0, r == -1 ? "timeout" : (r ? "mismatch" : NULL)) ;

    char pattern [32] ;
    for (int i = 0 ; i < 32 ; ++i)
      pattern [i] = (char) (i * 7 + 1) ;			/* incl. non-printables */
    for (int i = 0 ; i < 32 ; ++i)
      serialPutchar (fd, pattern [i]) ;
    r = drainEcho (fd, pattern, 32, 300) ;
    snprintf (buf, sizeof (buf), "%d baud: 32 binary bytes echoed", bauds [b]) ;
    check (buf, r == 0, r == -1 ? "timeout" : (r ? "mismatch" : NULL)) ;

    serialClose (fd) ;
  }

  printf ("=== %d passed, %d failed ===\n", passed, failed) ;
  return failed ? 1 : 0 ;
}
