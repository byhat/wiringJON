/*
 * mock_gpiod.c — in-memory fake of the libgpiod v1.6 API subset used by
 * wiringJON, for host-side (x86) unit tests without hardware.
 *
 * Two chips are emulated: "gpiochip0" (164 lines, tegra234-gpio) and
 * "gpiochip1" (32 lines, tegra234-gpio-aon). Line 41 and line 144 of
 * gpiochip0 are hard-wired together — the same loopback wire as on the
 * test bench (header pin 32 <-> pin 7): driving either line changes the
 * value of the other and generates edge events.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>

/* ---- minimal ABI-compatible decls (wire-compatible with gpiod.h 1.6) ---- */

struct gpiod_chip ;
struct gpiod_line ;
struct gpiod_line_event { struct timespec ts ; int event_type ; } ;

#ifndef GPIOD_LINE_EVENT_RISING_EDGE
#define GPIOD_LINE_EVENT_RISING_EDGE  1
#define GPIOD_LINE_EVENT_FALLING_EDGE 2
#endif

/* ---- mock state ---- */

#define NCHIP0 164
#define NCHIP1 32

typedef struct
{
  int   value ;
  int   dir ;		/* 0 free, 1 in, 2 out, 3 events */
  int   used ;		/* kernel-consumed flag */
  int   pending ;	/* edge event pending for event_wait */
  int   pendingType ;
  int   lastValue ;
  int   wire ;		/* index of the wired partner, or -1 */
  char  name [16] ;
}
mockLine ;

static mockLine chip0 [NCHIP0], chip1 [NCHIP1] ;
static pthread_mutex_t mlock = PTHREAD_MUTEX_INITIALIZER ;
static int initialised = 0 ;

static struct { const char *name ; mockLine *lines ; unsigned int n ; } chips [2] ;

static void initOnce (void)
{
  if (initialised) return ;
  initialised = 1 ;
  chips [0].name = "gpiochip0" ; chips [0].lines = chip0 ; chips [0].n = NCHIP0 ;
  chips [1].name = "gpiochip1" ; chips [1].lines = chip1 ; chips [1].n = NCHIP1 ;

  /* names for the header pins we care about */
  static const struct { int off ; const char *nm ; } names [] = {
    {144, "PAC.06"}, {112, "PR.04"}, {50, "PH.07"}, {122, "PY.00"},
    {85,  "PN.01"},  {126, "PY.04"}, {125, "PY.03"}, {135, "PZ.05"},
    {134, "PZ.04"},  {123, "PY.01"}, {133, "PZ.03"}, {136, "PZ.06"},
    {137, "PZ.07"},  {105, "PQ.05"}, {106, "PQ.06"}, {41,  "PG.06"},
    {43,  "PH.00"},  {53,  "PI.02"}, {113, "PR.05"}, {124, "PY.02"},
    {52,  "PI.01"},  {51,  "PI.00"},
  } ;
  for (unsigned i = 0 ; i < sizeof (names) / sizeof (names [0]) ; ++i)
    snprintf (chip0 [names [i].off].name, sizeof (chip0 [0].name), "%s", names [i].nm) ;

  /* the test-bench wire: pin 32 (line 41) <-> pin 7 (line 144) */
  chip0 [41].wire  = 144 ;
  chip0 [144].wire = 41 ;
}

/* opaque handles: pointer encodes chip index + line offset */
struct gpiod_chip  { int idx ; } ;
struct gpiod_line  { int chipIdx ; unsigned int off ; } ;

static struct gpiod_chip  chipHandles [2] = { {0}, {1} } ;
static struct gpiod_line  lineHandles [2][NCHIP0] ;

static mockLine *lineOf (struct gpiod_line *l)
{
  return chips [l->chipIdx].lines + l->off ;
}

/* ---- API ---- */

struct gpiod_chip *gpiod_chip_open_by_name (const char *name)
{
  initOnce () ;
  for (int i = 0 ; i < 2 ; ++i)
    if (strcmp (chips [i].name, name) == 0)
      return &chipHandles [i] ;
  errno = ENOENT ;
  return NULL ;
}

unsigned int gpiod_chip_num_lines (struct gpiod_chip *chip)
{
  return chips [chip->idx].n ;
}

struct gpiod_line *gpiod_chip_get_line (struct gpiod_chip *chip, unsigned int offset)
{
  if (!chip || offset >= chips [chip->idx].n)
  {
    errno = EINVAL ;
    return NULL ;
  }
  lineHandles [chip->idx][offset].chipIdx = chip->idx ;
  lineHandles [chip->idx][offset].off = offset ;
  return &lineHandles [chip->idx][offset] ;
}

const char *gpiod_line_name (struct gpiod_line *line)
{
  mockLine *l = lineOf (line) ;
  return l->name [0] ? l->name : NULL ;
}

int gpiod_line_is_used (struct gpiod_line *line)
{
  return lineOf (line)->used ;
}

int gpiod_line_request_input (struct gpiod_line *line, const char *consumer)
{
  (void)consumer ;
  mockLine *l = lineOf (line) ;
  pthread_mutex_lock (&mlock) ;
  l->dir = 1 ;
  pthread_mutex_unlock (&mlock) ;
  return 0 ;
}

int gpiod_line_request_output (struct gpiod_line *line, const char *consumer, int default_val)
{
  (void)consumer ;
  mockLine *l = lineOf (line) ;
  pthread_mutex_lock (&mlock) ;
  l->dir = 2 ;
  l->value = default_val ? 1 : 0 ;
  pthread_mutex_unlock (&mlock) ;
  return 0 ;
}

int gpiod_line_request_input_flags (struct gpiod_line *line, const char *consumer, int flags)
{
  (void)flags ;
  return gpiod_line_request_input (line, consumer) ;
}

int gpiod_line_request_output_flags (struct gpiod_line *line, const char *consumer, int flags, int val)
{
  (void)flags ;
  return gpiod_line_request_output (line, consumer, val) ;
}

static int requestEvents (struct gpiod_line *line, const char *consumer)
{
  (void)consumer ;
  mockLine *l = lineOf (line) ;
  pthread_mutex_lock (&mlock) ;
  l->dir = 3 ;
  pthread_mutex_unlock (&mlock) ;
  return 0 ;
}

int gpiod_line_request_rising_edge_events  (struct gpiod_line *l, const char *c) { return requestEvents (l, c) ; }
int gpiod_line_request_falling_edge_events (struct gpiod_line *l, const char *c) { return requestEvents (l, c) ; }
int gpiod_line_request_both_edges_events  (struct gpiod_line *l, const char *c) { return requestEvents (l, c) ; }

void gpiod_line_release (struct gpiod_line *line)
{
  mockLine *l = lineOf (line) ;
  pthread_mutex_lock (&mlock) ;
  l->dir = 0 ;
  pthread_mutex_unlock (&mlock) ;
}

int gpiod_line_set_value (struct gpiod_line *line, int value)
{
  mockLine *l = lineOf (line) ;
  int v = value ? 1 : 0 ;
  pthread_mutex_lock (&mlock) ;
  if (l->dir != 2)
  {
    pthread_mutex_unlock (&mlock) ;
    errno = EPERM ;
    return -1 ;
  }
  l->value = v ;

  mockLine *w = (l->wire >= 0) ? chips [line->chipIdx].lines + l->wire : NULL ;
  if (w)
  {
    int edgeType = v ? GPIOD_LINE_EVENT_RISING_EDGE : GPIOD_LINE_EVENT_FALLING_EDGE ;
    w->value = v ;
    if (w->dir == 3)
    {
      w->pending = 1 ;
      w->pendingType = edgeType ;
    }
  }
  pthread_mutex_unlock (&mlock) ;
  return 0 ;
}

int gpiod_line_get_value (struct gpiod_line *line)
{
  mockLine *l = lineOf (line) ;
  pthread_mutex_lock (&mlock) ;
  int v = l->value ;
  pthread_mutex_unlock (&mlock) ;
  return v ;
}

int gpiod_line_event_wait (struct gpiod_line *line, const struct timespec *timeout)
{
  mockLine *l = lineOf (line) ;
  (void)timeout ;			/* events are delivered instantly */
  pthread_mutex_lock (&mlock) ;
  int pending = l->pending ;
  pthread_mutex_unlock (&mlock) ;
  return pending ? 1 : 0 ;
}

int gpiod_line_event_read (struct gpiod_line *line, struct gpiod_line_event *event)
{
  mockLine *l = lineOf (line) ;
  pthread_mutex_lock (&mlock) ;
  if (!l->pending)
  {
    pthread_mutex_unlock (&mlock) ;
    errno = EAGAIN ;
    return -1 ;
  }
  l->pending = 0 ;
  clock_gettime (CLOCK_REALTIME, &event->ts) ;
  event->event_type = l->pendingType ;
  pthread_mutex_unlock (&mlock) ;
  return 0 ;
}
