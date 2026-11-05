/*
 * pwmJetson.c:
 *	Hardware PWM for wiringJON via the kernel sysfs PWM interface
 *	(/sys/class/pwm/pwmchipX/pwm0).
 *
 *	Jetson Orin Nano devkit PWM-capable header pins and their PWM
 *	controllers (from the Jetson.GPIO pin data):
 *	  pin 15 GPIO12 PN.01 -> 3280000.pwm
 *	  pin 32 GPIO07 PG.06 -> 32e0000.pwm
 *	  pin 33 GPIO13 PH.00 -> 32c0000.pwm
 *
 *	wiringPi semantics are emulated:
 *	  frequency = base / (pwmClock * pwmRange)   [base default 19.2 MHz,
 *	  like the Pi, so RPi tutorial values behave the same; override with
 *	  WIRINGPI_PWM_BASE_HZ],  duty  = pwmWrite value / pwmRange.
 *	Balanced mode is treated as mark-space (no hardware equivalent here).
 *
 *	NOTE: the PWM function must also be enabled for the pin in the device
 *	tree (sudo /opt/nvidia/jetson-io/jetson-io.py), otherwise the
 *	controller runs but the signal does not reach the header.
 ***********************************************************************
 * LGPLv3, see COPYING.LESSER.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <dirent.h>

#include "wiringPi.h"
#include "pinmap.h"
#include "pwmJetson.h"

#define PWM_CHANNEL	0
#define MAX_PHYS	64

typedef struct
{
  int    active ;
  char   pwmDir [96] ;		/* /sys/class/pwm/pwmchipN/pwm0 */
  unsigned int range ;		/* pwmSetRange, default 1024      */
  unsigned int clock ;		/* pwmSetClock, default 32        */
  int    mode ;			/* PWM_MODE_MS / PWM_MODE_BAL     */
}
pwmStateT ;

static pwmStateT pwmTab [MAX_PHYS] ;

static unsigned long long pwmBaseHz (void)
{
  static unsigned long long base = 0 ;
  if (!base)
  {
    base = 19200000ull ;
    const char *e = getenv ("WIRINGPI_PWM_BASE_HZ") ;
    if (e && *e)
    {
      unsigned long long v = strtoull (e, NULL, 10) ;
      if (v > 0)
	base = v ;
    }
  }
  return base ;
}

static unsigned long long pwmPeriodNs (const pwmStateT *p)
{
  unsigned long long freq = pwmBaseHz () / ((unsigned long long)p->clock * p->range) ;
  if (freq == 0)
    freq = 1 ;
  return 1000000000ull / freq ;
}

static int pwmWriteFile (const char *dir, const char *leaf, const char *value)
{
  char path [128] ;
  snprintf (path, sizeof (path), "%s/%s", dir, leaf) ;
  FILE *f = fopen (path, "w") ;
  if (!f)
  {
    fprintf (stderr, "wiringJON: PWM: cannot open %s: %s\n", path, strerror (errno)) ;
    return -1 ;
  }
  fprintf (f, "%s", value) ;
  fclose (f) ;
  return 0 ;
}

/* find /sys/class/pwm/pwmchipN whose device node name contains dtAddr */
static int findChipDir (const char *dtAddr, char *out, size_t outLen)
{
  const char *root = "/sys/class/pwm" ;
  DIR *d = opendir (root) ;
  if (!d)
    return -1 ;

  struct dirent *e ;
  while ((e = readdir (d)) != NULL)
  {
    if (strncmp (e->d_name, "pwmchip", 7) != 0)
      continue ;
    char chipPath [PATH_MAX], link [PATH_MAX] ;
    snprintf (chipPath, sizeof (chipPath), "%s/%s", root, e->d_name) ;
    snprintf (link, sizeof (link), "%s/device", chipPath) ;
    ssize_t n = readlink (link, link, sizeof (link) - 1) ;
    const char *name = NULL ;
    if (n > 0) { link [n] = '\0' ; name = strrchr (link, '/') ; name = name ? name + 1 : link ; }
    else
    {
      /* device symlink may be absent; try the of_node name */
      snprintf (link, sizeof (link), "%s/device/of_node", chipPath) ;
      char real [PATH_MAX] ;
      if (realpath (link, real))
	name = strrchr (real, '/') ;
    }
    if (name && strstr (name, dtAddr))
    {
      snprintf (out, outLen, "%s", chipPath) ;
      closedir (d) ;
      return 0 ;
    }
  }
  closedir (d) ;
  return -1 ;
}

int wjonPwmStart (int phys, const char *dtAddr)
{
  if (phys < 0 || phys >= MAX_PHYS || !dtAddr)
    return -1 ;
  pwmStateT *p = &pwmTab [phys] ;
  if (p->active)
    return 0 ;

  char chipDir [96] ;
  if (findChipDir (dtAddr, chipDir, sizeof (chipDir)) != 0)
  {
    fprintf (stderr,
      "wiringJON: PWM controller %s not found under /sys/class/pwm.\n"
      "           If it is disabled, enable the PWM pin function with:\n"
      "             sudo /opt/nvidia/jetson-io/jetson-io.py\n", dtAddr) ;
    return -1 ;
  }

  /* export channel 0 (EBUSY = already exported, e.g. by an earlier run) */
  char chan [8] ;
  snprintf (chan, sizeof (chan), "%d", PWM_CHANNEL) ;
  if (pwmWriteFile (chipDir, "export", chan) != 0 && errno != EBUSY)
    return -1 ;

  snprintf (p->pwmDir, sizeof (p->pwmDir), "%s/pwm%d", chipDir, PWM_CHANNEL) ;

  /* fresh defaults each start */
  p->range = 1024 ;
  p->clock = 32 ;
  p->mode  = PWM_MODE_MS ;

  char ns [32] ;
  pwmWriteFile (p->pwmDir, "duty_cycle", "0") ;
  snprintf (ns, sizeof (ns), "%llu", pwmPeriodNs (p)) ;
  if (pwmWriteFile (p->pwmDir, "period", ns) != 0)
  {
    pwmWriteFile (chipDir, "unexport", chan) ;
    p->pwmDir [0] = '\0' ;
    return -1 ;
  }
  pwmWriteFile (p->pwmDir, "polarity", "normal") ;
  if (pwmWriteFile (p->pwmDir, "enable", "1") != 0)
    return -1 ;

  p->active = 1 ;
  return 0 ;
}

void wjonPwmStop (int phys)
{
  if (phys < 0 || phys >= MAX_PHYS)
    return ;
  pwmStateT *p = &pwmTab [phys] ;
  if (!p->active)
    return ;
  pwmWriteFile (p->pwmDir, "enable", "0") ;
  char chan [8] ;
  snprintf (chan, sizeof (chan), "%d", PWM_CHANNEL) ;
  char chipDir [96] ;
  /* pwmDir = chipDir/pwm0 -> strip the last component */
  strncpy (chipDir, p->pwmDir, sizeof (chipDir) - 1) ;
  char *slash = strrchr (chipDir, '/') ;
  if (slash)
    *slash = '\0' ;
  pwmWriteFile (chipDir, "unexport", chan) ;
  p->active = 0 ;
  p->pwmDir [0] = '\0' ;
}

int wjonPwmWrite (int phys, int value)
{
  if (phys < 0 || phys >= MAX_PHYS)
    return -1 ;
  pwmStateT *p = &pwmTab [phys] ;
  if (!p->active)
    return -1 ;
  if (value < 0)
    value = 0 ;
  if ((unsigned int)value > p->range)
    value = (int)p->range ;
  unsigned long long duty = pwmPeriodNs (p) * (unsigned long long)value / p->range ;
  char ns [32] ;
  snprintf (ns, sizeof (ns), "%llu", duty) ;
  return pwmWriteFile (p->pwmDir, "duty_cycle", ns) ;
}

int wjonPwmTone (int phys, int freq)
{
  if (phys < 0 || phys >= MAX_PHYS || freq <= 0)
    return -1 ;
  pwmStateT *p = &pwmTab [phys] ;
  if (!p->active)
    return -1 ;
  char ns [32] ;
  unsigned long long period = 1000000000ull / (unsigned long long)freq ;
  pwmWriteFile (p->pwmDir, "duty_cycle", "0") ;
  snprintf (ns, sizeof (ns), "%llu", period) ;
  if (pwmWriteFile (p->pwmDir, "period", ns) != 0)
    return -1 ;
  snprintf (ns, sizeof (ns), "%llu", period / 2) ;
  return pwmWriteFile (p->pwmDir, "duty_cycle", ns) ;
}

static void pwmRetune (void)
{
  for (int phys = 0 ; phys < MAX_PHYS ; ++phys)
  {
    pwmStateT *p = &pwmTab [phys] ;
    if (!p->active)
      continue ;
    char ns [32] ;
    pwmWriteFile (p->pwmDir, "duty_cycle", "0") ;
    snprintf (ns, sizeof (ns), "%llu", pwmPeriodNs (p)) ;
    pwmWriteFile (p->pwmDir, "period", ns) ;
  }
}

void wjonPwmSetMode  (int mode)          { (void)mode ; /* MS/BAL same here */ }
void wjonPwmSetRange (unsigned int range)
{
  if (range == 0)
    return ;
  for (int phys = 0 ; phys < MAX_PHYS ; ++phys)
    if (pwmTab [phys].active)
      pwmTab [phys].range = range ;
  pwmRetune () ;
}

void wjonPwmSetClock (unsigned int divisor)
{
  if (divisor == 0)
    return ;
  for (int phys = 0 ; phys < MAX_PHYS ; ++phys)
    if (pwmTab [phys].active)
      pwmTab [phys].clock = divisor ;
  pwmRetune () ;
}
