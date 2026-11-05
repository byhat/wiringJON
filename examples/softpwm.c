/*
 * softpwm.c — software PWM fade (works on any GPIO pin, no hardware PWM).
 * LED on physical pin 11 = wiringPi pin 0.
 * Build: gcc softpwm.c -o softpwm -lwiringPi
 */

#include <wiringPi.h>
#include <softPwm.h>

int main (void)
{
  wiringPiSetup () ;
  softPwmCreate (0, 0, 100) ;	/* pin, initial value, range */

  for (;;)
  {
    for (int i = 0 ; i <= 100 ; ++i) { softPwmWrite (0, i) ; delay (20) ; }
    for (int i = 100 ; i >= 0 ; --i) { softPwmWrite (0, i) ; delay (20) ; }
  }
  return 0 ;
}
