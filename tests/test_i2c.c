/*
 * test_i2c.c — I2C smoke/scan test for wiringJON (informational).
 *
 *	Header I2C on the Orin Nano devkit = pins 3 (SDA) / 5 (SCL).
 *	Bus default: /dev/i2c-1 (override with WIRINGPI_I2C_BUS=<n>;
 *	run `i2cdetect -l` to see the buses on your L4T release).
 *
 *	With any I2C device on pins 3/5 this finds and reads it; without
 *	hardware it still verifies that the bus opens and reports clearly.
 *
 *	Exit code 0 = bus works (scan result is informational).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <wiringPiI2C.h>

int main (void)
{
  int fd = wiringPiI2CSetup (0x00) ;	/* dummy: just opens the bus */
  if (fd < 0)
  {
    printf ("[FAIL] wiringPiI2CSetup: %s\n", strerror (errno)) ;
    printf ("  hint: try WIRINGPI_I2C_BUS=<n> (see `i2cdetect -l`); check that\n"
            "        you are in the 'i2c' group.\n") ;
    return 1 ;
  }
  printf ("[PASS] I2C bus opened (fd %d)\n", fd) ;

  printf ("scanning 0x03..0x77 ...\n") ;
  int found = 0 ;
  for (int addr = 0x03 ; addr <= 0x77 ; ++addr)
  {
    int afd = wiringPiI2CSetup (addr) ;
    if (afd < 0)
      continue ;
    /* functional probe: a read that succeeds means a device ACKed its address */
    errno = 0 ;
    (void)wiringPiI2CRead (afd) ;
    if (errno != ENXIO && errno != EREMOTEIO && errno != EIO && errno != ETIMEDOUT)
    {
      printf ("  device at 0x%02X\n", addr) ;
      ++found ;
    }
    close (afd) ;
  }
  printf ("%s: %d device(s) on the bus%s\n",
          found ? "SCAN" : "SCAN (empty bus ok if nothing wired)",
          found, found ? "" : " — wire something to pins 3/5 to test data ops") ;
  close (fd) ;
  return 0 ;
}
