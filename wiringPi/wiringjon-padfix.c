/*
 * wiringjon-padfix — one-shot root helper: puts every 40-pin header pad
 * into a sane GPIO state (clears padctl tristate/SFIO, enables inputs).
 * Installed as a systemd oneshot service by ./build so the fix survives
 * reboots on boards whose bootloader leaves the pads tristated.
 *
 * Usage: sudo wiringjon-padfix [-v]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "padctlJetson.h"

int main (int argc, char **argv)
{
  int n = wjonPadctlFixup () ;
  if (n < 0)
  {
    fprintf (stderr, "wiringjon-padfix: cannot open /dev/mem: %s (run as root)\n",
             strerror (errno)) ;
    return 1 ;
  }
  if (argc > 1 && argv [1][0] == '-' && argv [1][1] == 'v')
    printf ("wiringjon-padfix: %d header pad(s) fixed\n", n) ;
  return 0 ;
}
