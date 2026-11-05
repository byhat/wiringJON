/*
 * pwmJetson.h — internal wiringJON PWM backend over /sys/class/pwm.
 * Not part of the public wiringPi API.
 ***********************************************************************
 * LGPLv3, see COPYING.LESSER.
 */

#ifndef __WIRINGJON_PWM_H__
#define __WIRINGJON_PWM_H__

#include "pinmap.h"

/* Start hardware PWM on a physical pin (dtAddr = DT node, e.g. "32e0000.pwm").
 * Returns 0 on success, -1 with a message on stderr. */
extern int  wjonPwmStart      (int phys, const char *dtAddr);

/* Stop (disable + unexport). Safe to call when not running. */
extern void wjonPwmStop       (int phys);

/* Duty in range units (see pwmSetRange). Returns -1 if not started. */
extern int  wjonPwmWrite      (int phys, int value);

/* Frequency in Hz at 50% duty. */
extern int  wjonPwmTone       (int phys, int freq);

/* wiringPi PWM semantics: freq = base / (clock * range). */
extern void wjonPwmSetMode    (int mode);
extern void wjonPwmSetRange   (unsigned int range);
extern void wjonPwmSetClock   (unsigned int divisor);

#endif
