/*
 * isr.c — interrupt example: counts edges on a pin using wiringPiISR.
 * With the test bench wire (header pin 32 <-> pin 7) drive pin 26 (wPi)
 * from another program or a button on pin 7.
 * Build: gcc isr.c -o isr -lwiringPi
 */

#include <stdio.h>
#include <wiringPi.h>

static volatile int counter = 0 ;

static void myInterrupt (void)
{
  ++counter ;
}

int main (void)
{
  wiringPiSetup () ;
  wiringPiISR (7, INT_EDGE_RISING, &myInterrupt) ;

  for (;;)
  {
    printf (" %d\n", counter) ;
    delay (1000) ;
  }
  return 0 ;
}
