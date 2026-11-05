/* findwire2.c — SAFE wire finder.
 *
 * Drives ONLY the 22 documented 40-pin header GPIO lines (one at a time),
 * while watching every otherwise-unused line on gpiochip0/gpiochip1 as an
 * input. Any watcher that follows a driver is physically connected to it.
 *
 * Unlike the first findwire (which blindly toggled every SoC line and
 * knocked the board's network out), no system GPIO is ever driven here.
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <gpiod.h>

#define MAXL 256

/* phys, chip, line — from the verified pin table */
static const int drv[][2] = {
  {0,144},{0,112},{0,50},{0,122},{0,85},{0,126},{0,125},{0,135},{0,134},
  {0,123},{0,133},{0,136},{0,137},{0,105},{0,106},{0,41},{0,43},{0,53},
  {0,113},{0,124},{0,52},{0,51},
};

static struct gpiod_chip *chips[2];
static struct gpiod_line *watch[2][MAXL];
static int nlines[2];

int main(void)
{
  const char *names[2] = {"gpiochip0", "gpiochip1"};
  for (int c = 0; c < 2; c++) {
    chips[c] = gpiod_chip_open_by_name(names[c]);
    if (!chips[c]) { printf("open %s: %s\n", names[c], strerror(errno)); return 1; }
    nlines[c] = gpiod_chip_num_lines(chips[c]);
    if (nlines[c] > MAXL) nlines[c] = MAXL;
  }

  for (int c = 0; c < 2; c++)
    for (int i = 0; i < nlines[c]; i++) {
      struct gpiod_line *l = gpiod_chip_get_line(chips[c], i);
      if (!l || gpiod_line_is_used(l)) continue;
      if (gpiod_line_request_input(l, "findwire2") < 0) continue;
      watch[c][i] = l;
    }

  int ndrv = (int)(sizeof(drv)/sizeof(drv[0]));
  for (int d = 0; d < ndrv; d++) {
    int c = drv[d][0], i = drv[d][1];
    struct gpiod_line *l = watch[c][i];
    if (!l) { printf("hdr pin line %s %d: not watchable\n", names[c], i); continue; }
    gpiod_line_release(l);
    if (gpiod_line_request_output(l, "findwire2", 1) < 0) {
      printf("hdr pin line %s %d: drive refused: %s\n", names[c], i, strerror(errno));
      watch[c][i] = NULL;
      continue;
    }
    usleep(40000);
    int v1[2][MAXL];
    for (int cc = 0; cc < 2; cc++)
      for (int ii = 0; ii < nlines[cc]; ii++)
        v1[cc][ii] = watch[cc][ii] ? gpiod_line_get_value(watch[cc][ii]) : -1;
    gpiod_line_set_value(l, 0);
    usleep(40000);
    for (int cc = 0; cc < 2; cc++)
      for (int ii = 0; ii < nlines[cc]; ii++) {
        if (!watch[cc][ii] || (cc == c && ii == i)) continue;
        int v0 = gpiod_line_get_value(watch[cc][ii]);
        if (v0 >= 0 && v1[cc][ii] >= 0 && v0 != v1[cc][ii])
          printf("WIRE: hdr %s line %d (%s) drives %s line %d (%s): %d->%d\n",
                 names[c], i, gpiod_line_name(l) ?: "?",
                 names[cc], ii, gpiod_line_name(watch[cc][ii]) ?: "?",
                 v1[cc][ii], v0);
      }
    gpiod_line_release(l);
    gpiod_line_request_input(l, "findwire2");
  }
  printf("safe scan done\n");
  return 0;
}
