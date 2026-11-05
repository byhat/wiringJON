/* findwire.c: brute-force wire finder.
 * Request every unused line on gpiochip0/gpiochip1 as input; then for each
 * unused line drive it 1 then 0 and snapshot all watchers. Any watcher that
 * follows a driver = physically connected. */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <gpiod.h>

#define MAXL 256

static struct gpiod_chip *chips[2];
static struct gpiod_line *lines[2][MAXL];
static int nlines[2];

int main(void) {
  const char *names[2] = {"gpiochip0", "gpiochip1"};
  for (int c = 0; c < 2; c++) {
    chips[c] = gpiod_chip_open_by_name(names[c]);
    if (!chips[c]) { printf("open %s: %s\n", names[c], strerror(errno)); return 1; }
    nlines[c] = gpiod_chip_num_lines(chips[c]);
    if (nlines[c] > MAXL) nlines[c] = MAXL;
  }

  /* request all unused lines as input watchers */
  for (int c = 0; c < 2; c++)
    for (int i = 0; i < nlines[c]; i++) {
      struct gpiod_line *l = gpiod_chip_get_line(chips[c], i);
      if (!l) continue;
      if (gpiod_line_is_used(l)) continue;             /* claimed by kernel/other */
      if (gpiod_line_request_input(l, "findwire") < 0) continue;
      lines[c][i] = l;
    }
  printf("watching: chip0=%d chip1=%d lines\n",
         gpiod_chip_num_lines(chips[0]), gpiod_chip_num_lines(chips[1]));

  for (int c = 0; c < 2; c++)
    for (int i = 0; i < nlines[c]; i++) {
      if (!lines[c][i]) continue;
      /* move line from input to output driver */
      gpiod_line_release(lines[c][i]);
      if (gpiod_line_request_output(lines[c][i], "findwire", 1) < 0) {
        lines[c][i] = NULL;
        continue;
      }
      usleep(30000);
      int v1[2][MAXL];
      for (int cc = 0; cc < 2; cc++)
        for (int ii = 0; ii < nlines[cc]; ii++)
          v1[cc][ii] = lines[cc][ii] ? gpiod_line_get_value(lines[cc][ii]) : -1;
      gpiod_line_set_value(lines[c][i], 0);
      usleep(30000);
      for (int cc = 0; cc < 2; cc++)
        for (int ii = 0; ii < nlines[cc]; ii++) {
          if (!lines[cc][ii] || (cc == c && ii == i)) continue;
          int v0 = gpiod_line_get_value(lines[cc][ii]);
          if (v0 >= 0 && v1[cc][ii] >= 0 && v0 != v1[cc][ii])
            printf("WIRE: %s line %d (%s) drives %s line %d (%s): %d->%d\n",
                   names[c], i, gpiod_line_name(lines[c][i]) ?: "?",
                   names[cc], ii, gpiod_line_name(lines[cc][ii]) ?: "?",
                   v1[cc][ii], v0);
        }
      /* back to input watcher */
      gpiod_line_release(lines[c][i]);
      if (gpiod_line_request_input(lines[c][i], "findwire") < 0)
        lines[c][i] = NULL;
    }
  printf("scan done\n");
  return 0;
}
