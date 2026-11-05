/*
 * blink.c — classic wiringPi blink, unchanged from the Raspberry Pi version.
 * Wiring: LED (with resistor) on physical pin 11 = wiringPi pin 0.
 * Build: gcc blink.c -o blink -lwiringPi
 */

#include <wiringPi.h>

int main (void)
{
  wiringPiSetup () ;
  pinMode (0, OUTPUT) ;

  for (;;)
  {
    digitalWrite (0, HIGH) ;
    delay (500) ;
    digitalWrite (0, LOW) ;
    delay (500) ;
  }
  return 0 ;
}
