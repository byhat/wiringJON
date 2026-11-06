/*
 * padctlJetson.h — internal wiringJON pad-control (pinmux) fixup.
 *
 * On the Orin Nano "Engineering Reference Developer Kit" (R39-era L4T)
 * the bootloader leaves the 40-pin header pads with the padctl TRISTATE
 * bit set, so the pads can neither drive outputs nor be re-driven until
 * the bit is cleared. Stock devkit images program this from the pinmux
 * DT; this board has no such DT node. wjonPadctlFixup() writes the pad
 * registers directly (needs /dev/mem, i.e. root) to put every header
 * pad into a sane GPIO state:
 *   - SFIO select (bit 10) = 0  (GPIO mode)
 *   - tristate   (bit 4)  = 0  (pad driver enabled)
 *   - e_input    (bit 6)  = 1  (input path enabled)
 * all other bits (pull, mux, schmitt, ...) preserved.
 *
 * Not part of the public wiringPi API.
 ***********************************************************************
 * LGPLv3, see COPYING.LESSER.
 */

#ifndef __WIRINGJON_PADCTL_H__
#define __WIRINGJON_PADCTL_H__

/* Returns the number of pads fixed, or -1 if /dev/mem is not accessible
 * (non-root): callers should then hint at the wiringjon-padfix service. */
extern int wjonPadctlFixup (void);

#endif
