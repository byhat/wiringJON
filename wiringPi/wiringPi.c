/*
 * wiringPi.c:
 *	wiringPi-compatible GPIO core for the NVIDIA Jetson Orin Nano / NX.
 *	Backend: libgpiod (Linux GPIO character device).
 *
 *	This file is part of wiringJON:
 *	  https://github.com/WiringPi/WiringPi  (API contract, LGPLv3)
 *	Ported backend (c) 2026 wiringJON contributors.
 ***********************************************************************
 * The public API and wiringPi.h are taken from the wiringPi 3.20 fork
 * so that programs written for wiringPi compile and link unchanged.
 * Everything below talks to the kernel through libgpiod instead of
 * mapping BCM peripheral registers.
 *
 * Deliberate differences from the original (see README.md):
 *  - digitalWrite/digitalRead auto-request the line if pinMode() was not
 *    called first (gpiod requires an explicit request per direction);
 *  - PWM/clock functions print a diagnostic until PWM support lands;
 *  - pullUpDnControl() asks the kernel for a bias change; on Tegra most
 *    header pulls are fixed by pinmux, so failures are reported once and
 *    ignored;
 *  - wiringPiSetupSys() is unavailable: this kernel (6.8) ships without
 *    the deprecated /sys/class/gpio interface.
 ***********************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sched.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <gpiod.h>

#include "wiringPi.h"
#include "pinmap.h"
#include "version.h"
#include "pwmJetson.h"

#define WPI_CONSUMER	"wiringPi"
#define MAX_CHIPS	4
#define MAX_PHYS	64		/* physical header pins 1..40 (+air) */
#define DIR_PWM		200		/* physDir marker: line driven by PWM */

/* ------------------------------------------------------------------ */
/* state                                                               */

static volatile int wpiMode = WPI_MODE_UNINITIALISED ;

int wiringPiDebug = 0 ;			/* gpio tool sets this */

static struct gpiod_chip *chips [MAX_CHIPS] ;
static char chipNames [MAX_CHIPS][32] ;
static int numChips = 0 ;

static struct gpiod_line *physLine [MAX_PHYS] ;   /* per-physical-pin handle */
static int physDir [MAX_PHYS] ;                   /* -1 free, INPUT, OUTPUT, 100+edge */
static int physPullWarned [MAX_PHYS] ;

struct wiringPiNodeStruct *wiringPiNodes = NULL ;

/* export stubs required by wiringPi.h (no /dev/mem on Jetson) */
volatile unsigned int *_wiringPiGpio        = NULL ;
volatile unsigned int *_wiringPiPwm         = NULL ;
volatile unsigned int *_wiringPiClk         = NULL ;
volatile unsigned int *_wiringPiPads        = NULL ;
volatile unsigned int *_wiringPiTimer       = NULL ;
volatile unsigned int *_wiringPiTimerIrqRaw = NULL ;

static struct timespec epochTs ;

static int wjonDebug (void)
{
  static int envChecked = 0, debug = 0 ;
  if (!envChecked) { envChecked = 1 ; const char *e = getenv ("WIRINGPI_DEBUG") ; debug = (e && *e && *e != '0') ; }
  return debug ;
}

/* ------------------------------------------------------------------ */
/* data tables the header declares                                     */

const char *piModelNames [PI_MODELS_MAX] =
{
  "Model A", "Model B", "Model A+", "Model B+", "Pi 2", "Alpha", "CM",
  "Unknown08", "3B", "Zero", "CM3", "Unknown11", "Zero W", "3B+", "3A+",
  "Unknown15", "CM3+", "4B", "Zero 2W", "400", "CM4", "CM4S", "Unknown22",
  "5", "CM5", "500", "CM5L", "CM0",
  "Jetson Orin (wiringJON)", "Unknown29", "Unknown30", "Unknown31"
} ;

const char *piProcessor [5] = { "Tegra234", "Tegra234", "Tegra234", "Tegra234", "Tegra234" } ;

const char *piRevisionNames [16] =
{
  "00", "01", "02", "03", "04", "05", "06", "07",
  "08", "09", "10", "11", "12", "13", "14", "15"
} ;

const char *piMakerNames [16] =
{
  "Sony", "Egoman", "Embest", "Unknown", "NVIDIA", "Unknown05", "Unknown06",
  "Unknown07", "Unknown08", "Unknown09", "Unknown10", "Unknown11",
  "Unknown12", "Unknown13", "Unknown14", "Unknown15"
} ;

int piMemorySize [8] = { 0, 4096, 8192, 16384, 32768, 65536, 0, 0 } ;

/* ------------------------------------------------------------------ */
/* helpers                                                             */

void piGpioLayoutOops (const char *why)
{
  fprintf (stderr, "wiringJON: piGpioLayoutOops: %s\n", why) ;
  exit (EXIT_FAILURE) ;
}

int wiringPiFailure (int fatal, const char *message, ...)
{
  va_list ap ;
  va_start (ap, message) ;
  vfprintf (stderr, message, ap) ;
  va_end (ap) ;
  if (fatal)
    exit (EXIT_FAILURE) ;
  return -1 ;
}

static uint64_t nowNS (void)
{
  struct timespec ts ;
  clock_gettime (CLOCK_MONOTONIC, &ts) ;
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec ;
}

static struct gpiod_chip *chipFor (const char *name)
{
  for (int i = 0 ; i < numChips ; ++i)
    if (strcmp (chipNames [i], name) == 0)
      return chips [i] ;
  if (numChips >= MAX_CHIPS)
    return NULL ;
  struct gpiod_chip *c = gpiod_chip_open_by_name (name) ;
  if (!c)
    return NULL ;
  chips [numChips] = c ;
  strncpy (chipNames [numChips], name, sizeof (chipNames [0]) - 1) ;
  return chips [numChips++] ;
}

/* resolve a user-visible pin number to the pinmap row for this setup mode */
static const wjonPinDescr *pinDescr (int pin)
{
  if (wpiMode == WPI_MODE_UNINITIALISED)
    (void) wiringPiFailure (WPI_FATAL,
      "wiringPi: you must call one of the wiringPiSetup functions first.\n") ;

  const wjonPinDescr *pd = NULL ;
  switch (wpiMode)
  {
    case WPI_MODE_PINS:
    case WPI_MODE_GPIO_DEVICE_WPI:
      pd = wjonByWpi (pin) ;
      if (!pd)
	(void) wiringPiFailure (WPI_FATAL, "wiringPi: %d is not a valid wiringPi pin on this board\n", pin) ;
      break ;

    case WPI_MODE_GPIO:
    case WPI_MODE_GPIO_DEVICE_BCM:
      pd = wjonByBcm (pin) ;
      if (!pd)
	(void) wiringPiFailure (WPI_FATAL, "wiringPi: %d is not a valid GPIO (BCM-style) pin on this board\n", pin) ;
      break ;

    case WPI_MODE_PHYS:
    case WPI_MODE_GPIO_DEVICE_PHYS:
      pd = wjonByPhys (pin) ;
      if (!pd)
	(void) wiringPiFailure (WPI_FATAL, "wiringPi: %d is not a valid physical pin (use 1-40)\n", pin) ;
      break ;

    default:
      pd = wjonByBcm (pin) ;
      if (!pd)
	(void) wiringPiFailure (WPI_FATAL, "wiringPi: pin %d is not valid in this mode\n", pin) ;
      break ;
  }

  if (pd->type != WJON_PIN_GPIO)
    (void) wiringPiFailure (WPI_FATAL,
      "wiringPi: physical pin %d (%s) has no GPIO function on this board\n",
      pd->phys, pd->name) ;
  return pd ;
}

static struct gpiod_line *lineFor (const wjonPinDescr *pd)
{
  struct gpiod_chip *c = chipFor (pd->chip) ;
  if (!c)
  {
    fprintf (stderr, "wiringJON: cannot open GPIO chip %s: %s\n",
             pd->chip, strerror (errno)) ;
    return NULL ;
  }
  struct gpiod_line *l = gpiod_chip_get_line (c, pd->line) ;
  if (!l)
    fprintf (stderr, "wiringJON: cannot get %s line %d: %s\n",
             pd->chip, pd->line, strerror (errno)) ;
  return l ;
}

/* ------------------------------------------------------------------ */
/* node system (expanders etc.)                                        */

static void pinModeDummy         (UNU struct wiringPiNodeStruct *node, UNU int pin, UNU int mode)  { return ; }
static void pullUpDnControlDummy (UNU struct wiringPiNodeStruct *node, UNU int pin, UNU int pud)   { return ; }
static  int digitalReadDummy     (UNU struct wiringPiNodeStruct *node, UNU int pin)                { return LOW ; }
static void digitalWriteDummy    (UNU struct wiringPiNodeStruct *node, UNU int pin, UNU int value) { return ; }
static void pwmWriteDummy        (UNU struct wiringPiNodeStruct *node, UNU int pin, UNU int value) { return ; }
static  int analogReadDummy      (UNU struct wiringPiNodeStruct *node, UNU int pin)                { return 0 ; }
static void analogWriteDummy     (UNU struct wiringPiNodeStruct *node, UNU int pin, UNU int value) { return ; }

struct wiringPiNodeStruct *wiringPiFindNode (int pin)
{
  struct wiringPiNodeStruct *node = wiringPiNodes ;
  while (node)
  {
    if ((pin >= node->pinBase) && (pin <= node->pinMax))
      return node ;
    node = node->next ;
  }
  return NULL ;
}

struct wiringPiNodeStruct *wiringPiNewNode (int pinBase, int numPins)
{
  if (pinBase < 64)
    (void) wiringPiFailure (WPI_FATAL, "wiringPiNewNode: pinBase of %d is < 64\n", pinBase) ;

  for (int pin = pinBase ; pin < (pinBase + numPins) ; ++pin)
    if (wiringPiFindNode (pin) != NULL)
      (void) wiringPiFailure (WPI_FATAL, "wiringPiNewNode: Pin %d overlaps with existing definition\n", pin) ;

  struct wiringPiNodeStruct *node =
    (struct wiringPiNodeStruct *)calloc (1, sizeof (struct wiringPiNodeStruct)) ;
  if (!node)
    (void) wiringPiFailure (WPI_FATAL, "wiringPiNewNode: Unable to allocate memory: %s\n", strerror (errno)) ;

  node->pinBase          = pinBase ;
  node->pinMax           = pinBase + numPins - 1 ;
  node->pinMode          = pinModeDummy ;
  node->pullUpDnControl  = pullUpDnControlDummy ;
  node->digitalRead      = digitalReadDummy ;
  node->digitalWrite     = digitalWriteDummy ;
  node->pwmWrite         = pwmWriteDummy ;
  node->analogRead       = analogReadDummy ;
  node->analogWrite      = analogWriteDummy ;
  node->next             = wiringPiNodes ;
  wiringPiNodes = node ;
  return node ;
}

/* ------------------------------------------------------------------ */
/* ISR support                                                         */

typedef struct
{
  struct gpiod_line *line ;
  void (*fn) (void) ;
  void (*fn2) (struct WPIWfiStatus, void *) ;
  void *userdata ;
  unsigned long debounceUs ;
  int edge ;
  pthread_t thread ;
  volatile int running ;
}
isrCtx ;

static isrCtx isrTab [MAX_PHYS] ;

static void *isrThread (void *arg)
{
  isrCtx *c = (isrCtx *)arg ;
  struct timespec ts = { 0, 200 * 1000 * 1000 } ;	/* 200 ms stop ticks */

  while (c->running)
  {
    int r = gpiod_line_event_wait (c->line, &ts) ;
    if (r == 1)
    {
      struct gpiod_line_event ev ;
      gpiod_line_event_read (c->line, &ev) ;
      if (c->fn)
	c->fn () ;
      else if (c->fn2)
      {
	struct WPIWfiStatus st ;
	st.statusOK      = 1 ;
	st.pinBCM        = 0 ;	/* filled by caller side is not possible here */
	st.edge          = (ev.event_type == GPIOD_LINE_EVENT_RISING_EDGE)
	                     ? INT_EDGE_RISING : INT_EDGE_FALLING ;
	st.timeStamp_us  = (long long)ev.ts.tv_sec * 1000000ll
	                 + ev.ts.tv_nsec / 1000 ;
	c->fn2 (st, c->userdata) ;
      }
      if (c->debounceUs)
      {
	ts.tv_sec  = 0 ;
	ts.tv_nsec = 0 ;
	struct timespec db = { c->debounceUs / 1000000,
	                       (c->debounceUs % 1000000) * 1000 } ;
	nanosleep (&db, NULL) ;
	ts.tv_sec  = 0 ;
	ts.tv_nsec = 200 * 1000 * 1000 ;
      }
    }
    else if (r < 0 && errno != EAGAIN)
      break ;
  }
  return NULL ;
}

/* release any request currently held on a physical pin */
static void physRelease (int phys)
{
  if (physLine [phys])
  {
    gpiod_line_release (physLine [phys]) ;
    physLine [phys] = NULL ;
  }
  physDir [phys] = -1 ;
}

static int requestEdges (struct gpiod_line *l, int edge)
{
  switch (edge)
  {
    case INT_EDGE_FALLING: return gpiod_line_request_falling_edge_events (l, WPI_CONSUMER) ;
    case INT_EDGE_RISING : return gpiod_line_request_rising_edge_events  (l, WPI_CONSUMER) ;
    case INT_EDGE_BOTH   : return gpiod_line_request_both_edges_events   (l, WPI_CONSUMER) ;
    default              : return gpiod_line_request_input (l, WPI_CONSUMER) ;
  }
}

int wiringPiISR (int pin, int mode, void (*fn) (void))
{
  struct wiringPiNodeStruct *node = wiringPiFindNode (pin) ;
  if (node)
    return -1 ;

  const wjonPinDescr *pd = pinDescr (pin) ;
  int phys = pd->phys ;
  isrCtx *c = &isrTab [phys] ;

  if (c->running)
    wiringPiISRStop (pin) ;

  struct gpiod_line *l = lineFor (pd) ;
  if (!l)
    return -1 ;

  physRelease (phys) ;
  if (requestEdges (l, mode) < 0)
  {
    fprintf (stderr, "wiringJON: wiringPiISR(%d): cannot request %s events: %s\n",
             pin, pd->socPin, strerror (errno)) ;
    physDir [phys] = -1 ;
    return -1 ;
  }
  physLine [phys] = l ;
  physDir  [phys] = 100 + mode ;

  if (fn)
  {
    memset (c, 0, sizeof (*c)) ;
    c->line    = l ;
    c->fn      = fn ;
    c->running = 1 ;
    if (pthread_create (&c->thread, NULL, isrThread, c) != 0)
    {
      c->running = 0 ;
      physRelease (phys) ;
      return -1 ;
    }
  }
  return 0 ;
}

int wiringPiISR2 (int pin, int edgeMode,
                  void (*fn) (struct WPIWfiStatus, void *),
                  unsigned long debounce_period_us, void *userdata)
{
  struct wiringPiNodeStruct *node = wiringPiFindNode (pin) ;
  if (node)
    return -1 ;

  const wjonPinDescr *pd = pinDescr (pin) ;
  int phys = pd->phys ;
  isrCtx *c = &isrTab [phys] ;

  if (c->running)
    wiringPiISRStop (pin) ;

  struct gpiod_line *l = lineFor (pd) ;
  if (!l)
    return -1 ;

  physRelease (phys) ;
  if (requestEdges (l, edgeMode) < 0)
  {
    fprintf (stderr, "wiringJON: wiringPiISR2(%d): %s\n", pin, strerror (errno)) ;
    return -1 ;
  }
  physLine [phys] = l ;
  physDir  [phys] = 100 + edgeMode ;

  memset (c, 0, sizeof (*c)) ;
  c->line       = l ;
  c->fn2        = fn ;
  c->userdata   = userdata ;
  c->debounceUs = debounce_period_us ;
  c->running    = 1 ;
  if (pthread_create (&c->thread, NULL, isrThread, c) != 0)
  {
    c->running = 0 ;
    physRelease (phys) ;
    return -1 ;
  }
  return 0 ;
}

int wiringPiISRStop (int pin)
{
  struct wiringPiNodeStruct *node = wiringPiFindNode (pin) ;
  if (node)
    return -1 ;

  const wjonPinDescr *pd = pinDescr (pin) ;
  int phys = pd->phys ;
  isrCtx *c = &isrTab [phys] ;

  if (c->running)
  {
    c->running = 0 ;
    pthread_join (c->thread, NULL) ;
    c->fn = NULL ;
    c->fn2 = NULL ;
  }
  if (physDir [phys] >= 100)
    physRelease (phys) ;
  return 0 ;
}

int waitForInterruptClose (int pin)
{
  return wiringPiISRStop (pin) ;
}

struct WPIWfiStatus waitForInterrupt2 (int pin, int edgeMode, int ms, unsigned long debounceUs)
{
  struct WPIWfiStatus st ;
  memset (&st, 0, sizeof (st)) ;
  st.statusOK = -1 ;

  struct wiringPiNodeStruct *node = wiringPiFindNode (pin) ;
  if (node)
    return st ;

  const wjonPinDescr *pd = pinDescr (pin) ;
  int phys = pd->phys ;
  isrCtx *c = &isrTab [phys] ;
  int wasRunning = c->running ;
  if (wasRunning)
    wiringPiISRStop (pin) ;

  struct gpiod_line *l = lineFor (pd) ;
  if (!l)
    return st ;

  int hadRequest = (physDir [phys] != -1) ;
  physRelease (phys) ;
  if (requestEdges (l, edgeMode) < 0)
  {
    fprintf (stderr, "wiringJON: waitForInterrupt2(%d): %s\n", pin, strerror (errno)) ;
    return st ;
  }
  physLine [phys] = l ;
  physDir  [phys] = 100 + edgeMode ;

  if (debounceUs)
    usleep ((useconds_t)debounceUs) ;

  struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L } ;
  int r = gpiod_line_event_wait (l, &ts) ;
  if (r == 1)
  {
    struct gpiod_line_event ev ;
    gpiod_line_event_read (l, &ev) ;
    st.statusOK     = 1 ;
    st.pinBCM       = pd->bcm ;
    st.edge         = (ev.event_type == GPIOD_LINE_EVENT_RISING_EDGE)
                        ? INT_EDGE_RISING : INT_EDGE_FALLING ;
    st.timeStamp_us = (long long)ev.ts.tv_sec * 1000000ll + ev.ts.tv_nsec / 1000 ;
  }
  else if (r == 0)
    st.statusOK = 0 ;

  physRelease (phys) ;
  if (hadRequest)				/* leave the pin as we found it */
    (void) requestEdges (l, INT_EDGE_BOTH) ;
  return st ;
}

/* ------------------------------------------------------------------ */
/* core setup                                                          */

static int doSetup (int mode)
{
  if (wpiMode != WPI_MODE_UNINITIALISED)
  {
    wpiMode = mode ;
    return 0 ;
  }

  clock_gettime (CLOCK_MONOTONIC, &epochTs) ;
  for (int i = 0 ; i < MAX_PHYS ; ++i) physDir [i] = -1 ;

  if (wjonBoardInit () != 0)
    return wiringPiFailure (WPI_FATAL,
      "wiringPi: unsupported board (see messages above)\n") ;

  /* open every chip referenced by the pin table, fail loudly */
  for (const wjonPinDescr *pd = wjonPins ; pd->phys ; ++pd)
  {
    if (pd->type != WJON_PIN_GPIO)
      continue ;
    int known = 0 ;
    for (int i = 0 ; i < numChips ; ++i)
      if (strcmp (chipNames [i], pd->chip) == 0) { known = 1 ; break ; }
    if (known)
      continue ;
    if (!chipFor (pd->chip))
      return wiringPiFailure (WPI_FATAL,
        "wiringPi: cannot open %s (%s) — are you in the 'gpio' group / root?\n",
        pd->chip, strerror (errno)) ;
  }

  if (wjonDebug ())
    fprintf (stderr, "wiringJON: board '%s', %d chip(s), mode %d\n",
             wjonBoardName (), numChips, mode) ;

  wpiMode = mode ;
  return 0 ;
}

int wiringPiSetup (void)
{
  return doSetup (WPI_MODE_PINS) ;
}

int wiringPiSetupGpio (void)
{
  return doSetup (WPI_MODE_GPIO) ;
}

int wiringPiSetupPhys (void)
{
  return doSetup (WPI_MODE_PHYS) ;
}

int wiringPiSetupPinType (enum WPIPinType pinType)
{
  switch (pinType)
  {
    case WPI_PIN_BCM : return doSetup (WPI_MODE_GPIO_DEVICE_BCM) ;
    case WPI_PIN_WPI : return doSetup (WPI_MODE_GPIO_DEVICE_WPI) ;
    case WPI_PIN_PHYS: return doSetup (WPI_MODE_GPIO_DEVICE_PHYS) ;
  }
  return wiringPiFailure (WPI_ALMOST, "wiringPiSetupPinType: bad pin type\n") ;
}

int wiringPiSetupGpioDevice (enum WPIPinType pinType)
{
  return wiringPiSetupPinType (pinType) ;
}

int wiringPiSetupSys (void)
{
  /* The sysfs GPIO interface (/sys/class/gpio) is gone from this kernel.
   * To keep programs written for wiringPiSetupSys() running, we alias it
   * to the chardev backend with BCM numbering (sysfs used BCM numbers). */
  static int warned = 0 ;
  if (!warned++)
    fprintf (stderr,
      "wiringJON: wiringPiSetupSys(): /sys/class/gpio no longer exists on this kernel;\n"
      "           running in GPIO (BCM-style) mode instead — no action needed.\n") ;
  return doSetup (WPI_MODE_GPIO) ;
}

int wiringPiSetupPiFace (void)
{
  return wiringPiFailure (WPI_ALMOST, "wiringPiSetupPiFace: not supported on wiringJON\n") ;
}

int wiringPiSetupPiFaceForGpioProg (void)
{
  return wiringPiFailure (WPI_ALMOST, "wiringPiSetupPiFaceForGpioProg: not supported on wiringJON\n") ;
}

void wiringPiVersion (int *major, int *minor)
{
  *major = VERSION_MAJOR ;
  *minor = VERSION_MINOR ;
}

/* ------------------------------------------------------------------ */
/* pin control                                                         */

void pinMode (int pin, int mode)
{
  struct wiringPiNodeStruct *node = wiringPiFindNode (pin) ;
  if (node) { node->pinMode (node, pin, mode) ; return ; }

  const wjonPinDescr *pd = pinDescr (pin) ;
  int phys = pd->phys ;

  switch (mode)
  {
    case OUTPUT:
    case SOFT_PWM_OUTPUT:
    case SOFT_TONE_OUTPUT:
    {
      if (physDir [phys] == DIR_PWM)
	wjonPwmStop (phys) ;
      struct gpiod_line *l = lineFor (pd) ;
      if (!l)
	return ;
      if (physDir [phys] >= 100)
	wiringPiISRStop (pin) ;
      if (physLine [phys] && physDir [phys] == OUTPUT)
	return ;					/* already an output */
      physRelease (phys) ;
      if (gpiod_line_request_output (l, WPI_CONSUMER, 0) < 0)
      {
	fprintf (stderr, "wiringJON: pinMode(%d,%s): cannot request %s as output: %s\n",
	         pin, "OUTPUT", pd->socPin, strerror (errno)) ;
	return ;
      }
      physLine [phys] = l ;
      physDir  [phys] = OUTPUT ;
      break ;
    }

    case INPUT:
    {
      if (physDir [phys] == DIR_PWM)
	wjonPwmStop (phys) ;
      struct gpiod_line *l = lineFor (pd) ;
      if (!l)
	return ;
      if (physDir [phys] >= 100)
	wiringPiISRStop (pin) ;
      if (physLine [phys] && physDir [phys] == INPUT)
	return ;					/* already an input */
      physRelease (phys) ;
      if (gpiod_line_request_input (l, WPI_CONSUMER) < 0)
      {
	fprintf (stderr, "wiringJON: pinMode(%d,%s): cannot request %s as input: %s\n",
	         pin, "INPUT", pd->socPin, strerror (errno)) ;
	return ;
      }
      physLine [phys] = l ;
      physDir  [phys] = INPUT ;
      break ;
    }

    case PM_OFF:					/* release to floating input */
      wiringPiISRStop (pin) ;
      if (physDir [phys] == DIR_PWM)
	wjonPwmStop (phys) ;
      physRelease (phys) ;
      break ;

    case PWM_OUTPUT:
    case PWM_MS_OUTPUT:
    case PWM_BAL_OUTPUT:
    {
      if (!pd->pwmDev)
      {
	fprintf (stderr,
	  "wiringJON: pinMode(%d,PWM): pin %s (physical %d) has no PWM function; "
	  "PWM capable pins: 15, 32, 33\n", pin, pd->name, pd->phys) ;
	break ;
      }
      wiringPiISRStop (pin) ;
      physRelease (phys) ;
      if (wjonPwmStart (pd->phys, pd->pwmDev) == 0)
	physDir [phys] = DIR_PWM ;
      break ;
    }

    case GPIO_CLOCK:
      fprintf (stderr, "wiringJON: pinMode(%d,GPIO_CLOCK): no GPIO clock function on Jetson\n", pin) ;
      break ;

    default:
      fprintf (stderr, "wiringJON: pinMode: unknown mode %d\n", mode) ;
      break ;
  }
}

void pullUpDnControl (int pin, int pud)
{
  struct wiringPiNodeStruct *node = wiringPiFindNode (pin) ;
  if (node) { node->pullUpDnControl (node, pin, pud) ; return ; }

  const wjonPinDescr *pd = pinDescr (pin) ;
  int phys = pd->phys ;

#ifdef GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP
  int flags = 0 ;
  switch (pud)
  {
    case PUD_OFF : flags = GPIOD_LINE_REQUEST_FLAG_BIAS_DISABLE  ; break ;
    case PUD_DOWN: flags = GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_DOWN; break ;
    case PUD_UP  : flags = GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP  ; break ;
    default: return ;
  }

  struct gpiod_line *l = physLine [phys] ;
  if (!l)
  {
    pinMode (pin, INPUT) ;
    l = physLine [phys] ;
    if (!l)
      return ;
  }
  if (physDir [phys] >= 100)
  {
    fprintf (stderr, "wiringJON: pullUpDnControl(%d): pin is used by an ISR\n", pin) ;
    return ;
  }

  gpiod_line_release (l) ;
  int r = (physDir [phys] == OUTPUT)
      ? gpiod_line_request_output_flags (l, WPI_CONSUMER, flags, 0)
      : gpiod_line_request_input_flags  (l, WPI_CONSUMER, flags) ;
  if (r < 0)
  {
    if (!physPullWarned [phys]++)
      fprintf (stderr,
        "wiringJON: pullUpDnControl(%d,%s): kernel rejected the bias change (%s).\n"
        "           Tegra header pulls are usually fixed by the pinmux; this call is ignored.\n",
        pin, pd->socPin, strerror (errno)) ;
    /* restore a plain request so the pin keeps working */
    r = (physDir [phys] == OUTPUT)
      ? gpiod_line_request_output (l, WPI_CONSUMER, 0)
      : gpiod_line_request_input  (l, WPI_CONSUMER) ;
    if (r < 0)
      physDir [phys] = -1 ;
  }
#else
  if (!physPullWarned [phys]++)
    fprintf (stderr, "wiringJON: pullUpDnControl needs libgpiod >= 1.6 (bias flags)\n") ;
#endif
}

void digitalWrite (int pin, int value)
{
  struct wiringPiNodeStruct *node = wiringPiFindNode (pin) ;
  if (node) { node->digitalWrite (node, pin, value) ; return ; }

  const wjonPinDescr *pd = pinDescr (pin) ;
  int phys = pd->phys ;

  struct gpiod_line *l = physLine [phys] ;
  if (!l || physDir [phys] != OUTPUT)
  {
    pinMode (pin, OUTPUT) ;				/* auto-request, see file header */
    l = physLine [phys] ;
    if (!l)
      return ;
  }
  gpiod_line_set_value (l, value ? 1 : 0) ;
}

int digitalRead (int pin)
{
  struct wiringPiNodeStruct *node = wiringPiFindNode (pin) ;
  if (node)
    return node->digitalRead (node, pin) ;

  const wjonPinDescr *pd = pinDescr (pin) ;
  int phys = pd->phys ;

  if (physDir [phys] == DIR_PWM)		/* line is owned by sysfs PWM */
    return 0 ;

  struct gpiod_line *l = physLine [phys] ;
  if (!l)
  {
    pinMode (pin, INPUT) ;				/* auto-request, see file header */
    l = physLine [phys] ;
    if (!l)
      return 0 ;
  }
  return gpiod_line_get_value (l) ;
}

void pwmWrite (int pin, int value)
{
  struct wiringPiNodeStruct *node = wiringPiFindNode (pin) ;
  if (node) { node->pwmWrite (node, pin, value) ; return ; }

  const wjonPinDescr *pd = pinDescr (pin) ;
  if (physDir [pd->phys] != DIR_PWM)
    pinMode (pin, PWM_OUTPUT) ;			/* auto-start, see file header */
  wjonPwmWrite (pd->phys, value) ;
}

void pwmToneWrite (int pin, int freq)
{
  struct wiringPiNodeStruct *node = wiringPiFindNode (pin) ;
  if (node) { node->pwmWrite (node, pin, 0) ; return ; }
  const wjonPinDescr *pd = pinDescr (pin) ;
  if (physDir [pd->phys] != DIR_PWM)
    pinMode (pin, PWM_OUTPUT) ;
  wjonPwmTone (pd->phys, freq) ;
}
void pwmSetMode (int mode)             { wjonPwmSetMode (mode) ; }
void pwmSetRange (unsigned int range)  { wjonPwmSetRange (range) ; }
void pwmSetChannelRange (unsigned int channel, unsigned int range) { (void)channel ; wjonPwmSetRange (range) ; }
void pwmSetPinRange (int pin, unsigned int range) { (void)pin ; wjonPwmSetRange (range) ; }
void pwmSetClock (int divisor)         { if (divisor > 0) wjonPwmSetClock ((unsigned int)divisor) ; }
void gpioClockSet (int pin, int freq)
{
  (void)freq ;
  fprintf (stderr, "wiringJON: gpioClockSet(%d): no GPIO clock function on Jetson\n", pin) ;
}

int analogRead (int pin)
{
  struct wiringPiNodeStruct *node = wiringPiFindNode (pin) ;
  if (node)
    return node->analogRead (node, pin) ;
  return 0 ;						/* no on-SoC ADC */
}

void analogWrite (int pin, int value)
{
  struct wiringPiNodeStruct *node = wiringPiFindNode (pin) ;
  if (node) { node->analogWrite (node, pin, value) ; return ; }
}

void pinModeAlt (int pin, int mode)
{
  (void)mode ;
  fprintf (stderr, "wiringJON: pinModeAlt(%d): not supported (managed by device tree pinmux)\n", pin) ;
}

enum WPIPinAlt getPinModeAlt (int pin) { return (enum WPIPinAlt)getAlt (pin) ; }

int getAlt (int pin)
{
  struct wiringPiNodeStruct *node = wiringPiFindNode (pin) ;
  if (node)
    return WPI_ALT_UNKNOWN ;
  const wjonPinDescr *pd = pinDescr (pin) ;
  if (physDir [pd->phys] == DIR_PWM)
    return WPI_ALT0 ;				/* PWM alternate, like the Pi */
  if (!physLine [pd->phys])
    return WPI_NONE ;
  return (physDir [pd->phys] == OUTPUT) ? WPI_ALT_OUTPUT : WPI_ALT_INPUT ;
}

void setPadDrive (int group, int value)      { (void)group ; (void)value ; }
void setPadDrivePin (int pin, int value)     { (void)pin ; (void)value ; }
unsigned int digitalReadByte (void)          { return 0 ; }
unsigned int digitalReadByte2 (void)         { return 0 ; }
void digitalWriteByte (int value)            { (void)value ; }
void digitalWriteByte2 (int value)           { (void)value ; }

/* ------------------------------------------------------------------ */
/* board identification                                                */

int piGpioLayout (void) { return GPIO_LAYOUT_DEFAULT ; }
int piBoardRev (void)   { return piGpioLayout () ; }

void piBoardId (int *model, int *rev, int *mem, int *maker, int *overVolted)
{
  *model      = PI_MODEL_JETSON_ORIN ;
  *rev        = PI_VERSION_2 ;
  *mem        = 2 ;					/* piMemorySize[2] = 8192 */
  *maker      = PI_MAKER_UNKNOWN ;
  *overVolted = 0 ;
}

int piBoard40Pin (void) { return 1 ; }
int piRP1Model (void)   { return 0 ; }			/* no RP1 chip on Jetson */

int wpiPinToGpio (int wpiPin)
{
  const wjonPinDescr *pd = wjonByWpi (wpiPin) ;
  return pd ? pd->bcm : -1 ;
}

int physPinToGpio (int physPin)
{
  const wjonPinDescr *pd = wjonByPhys (physPin) ;
  return (pd && pd->type == WJON_PIN_GPIO) ? pd->bcm : -1 ;
}

int wiringPiUserLevelAccess (void)
{
  return (wpiMode != WPI_MODE_UNINITIALISED) ? 1 : 0 ;
}

int wiringPiGlobalMemoryAccess (void) { return 0 ; }
int wiringPiGpioDeviceGetFd (void)    { return -1 ; }

/* ------------------------------------------------------------------ */
/* threads / scheduler                                                 */

int piThreadCreate (void *(*fn) (void *))
{
  pthread_t myThread ;
  pthread_attr_t attr ;
  pthread_attr_init (&attr) ;
  pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED) ;
  int r = pthread_create (&myThread, &attr, fn, NULL) ;
  pthread_attr_destroy (&attr) ;
  return r ;
}

static pthread_mutex_t piMutexes [8] =
{
  PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
  PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
  PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER,
  PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER
} ;

void piLock (int key)
{
  pthread_mutex_lock (&piMutexes [key % 8]) ;
}

void piUnlock (int key)
{
  pthread_mutex_unlock (&piMutexes [key % 8]) ;
}

int piHiPri (const int pri)
{
  if (pri > 0)
  {
    struct sched_param sched ;
    memset (&sched, 0, sizeof (sched)) ;
    sched.sched_priority = pri ;
    if (sched_setscheduler (0, SCHED_RR, &sched) == 0)
      return 0 ;
  }
  return setpriority (PRIO_PROCESS, 0, pri) == 0 ? 0 : -1 ;
}

/* ------------------------------------------------------------------ */
/* timing                                                              */

void delay (unsigned int ms)
{
  struct timespec req = { ms / 1000, (long)(ms % 1000) * 1000000L }, rem ;
  while (nanosleep (&req, &rem) == -1 && errno == EINTR)
    req = rem ;
}

void delayMicroseconds (unsigned int us)
{
  if (us >= 200)
  {
    struct timespec req = { us / 1000000, (long)(us % 1000000) * 1000L }, rem ;
    while (nanosleep (&req, &rem) == -1 && errno == EINTR)
      req = rem ;
    return ;
  }
  uint64_t target = nowNS () + (uint64_t)us * 1000ull ;
  while (nowNS () < target)
    ;							/* busy wait for <200us */
}

unsigned long long piMicros64 (void)
{
  return (nowNS () - ((uint64_t)epochTs.tv_sec * 1000000000ull
                    + epochTs.tv_nsec)) / 1000ull ;
}

unsigned int millis (void) { return (unsigned int)(piMicros64 () / 1000ull) ; }
unsigned int micros (void) { return (unsigned int)(piMicros64 ()) ; }

/* ------------------------------------------------------------------ */
/* pulse / frequency measurement (v3.20 API)                           */

unsigned long long pulseInNS (int pin, int level, unsigned long long timeoutNS)
{
  uint64_t start = nowNS () ;
  while (digitalRead (pin) != level)
    if (nowNS () - start > timeoutNS)
      return 0 ;
  uint64_t edge = nowNS () ;
  while (digitalRead (pin) == level)
    if (nowNS () - edge > timeoutNS)
      return 0 ;
  return nowNS () - edge ;
}

unsigned int pulseIn (int pin, int level, unsigned int timeoutUs)
{
  return (unsigned int)(pulseInNS (pin, level, (unsigned long long)timeoutUs * 1000ull) / 1000ull) ;
}

unsigned long long frequencyIn (int pin, unsigned long windowMs)
{
  uint64_t start = nowNS () ;
  uint64_t window = (uint64_t)windowMs * 1000000ull ;
  unsigned long long edges = 0 ;
  int last = digitalRead (pin) ;
  while (nowNS () - start < window)
  {
    int now = digitalRead (pin) ;
    if (now != last && now == HIGH)
      ++edges ;
    last = now ;
  }
  return edges * 1000ull / windowMs ;
}
