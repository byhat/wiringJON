/*
 * servo.c — software servo sweep (wiringPi pin 0 = physical pin 11).
 * Build: gcc servo.c -o servo -lwiringPi
 */

#include <wiringPi.h>
#include <softServo.h>

int main (void)
{
  wiringPiSetup () ;
  softServoSetup (0, -1, -1, -1, -1, -1, -1, -1) ;

  for (;;)
  {
    for (int deg = 0 ; deg <= 180 ; deg += 5) { softServoWrite (0, deg) ; delay (50) ; }
    for (int deg = 180 ; deg >= 0 ; deg -= 5) { softServoWrite (0, deg) ; delay (50) ; }
  }
  return 0 ;
}
