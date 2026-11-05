/* Probe: which header GPIO line is physically wired to pin 7 (line 144)?
 * Also readback test: does driving a line actually change its pad level? */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <gpiod.h>

struct { int chip, off, phys; const char *n; } pins[] = {
  {0,144,7,"GPIO09_PAC.06"}, {0,112,11,"UART1_RTS_PR.04"}, {0,50,12,"I2S0_SCLK_PH.07"},
  {0,122,13,"SPI1_SCK_PY.00"}, {0,85,15,"GPIO12_PN.01"}, {0,126,16,"SPI1_CS1_PY.04"},
  {0,125,18,"SPI1_CS0_PY.03"}, {0,135,19,"SPI0_MOSI_PZ.05"}, {0,134,21,"SPI0_MISO_PZ.04"},
  {0,123,22,"SPI1_MISO_PY.01"}, {0,133,23,"SPI0_SCK_PZ.03"}, {0,136,24,"SPI0_CS0_PZ.06"},
  {0,137,26,"SPI0_CS1_PZ.07"}, {0,105,29,"GPIO01_PQ.05"}, {0,106,31,"GPIO11_PQ.06"},
  {0,41,32,"GPIO07_PG.06"}, {0,43,33,"GPIO13_PH.00"}, {0,53,35,"I2S0_FS_PI.02"},
  {0,113,36,"UART1_CTS_PR.05"}, {0,124,37,"SPI1_MOSI_PY.02"}, {0,52,38,"I2S0_SDIN_PI.01"},
  {0,51,40,"I2S0_SDOUT_PI.00"},
};

int main(void) {
  struct gpiod_chip *c0 = gpiod_chip_open_by_name("gpiochip0");
  if (!c0) { printf("open gpiochip0: %s\n", strerror(errno)); return 1; }
  int n = (int)(sizeof(pins)/sizeof(pins[0]));

  struct gpiod_line *watch = gpiod_chip_get_line(c0, 144);
  if (!watch || gpiod_line_request_input(watch, "probe") < 0) {
    printf("request 144 failed: %s\n", strerror(errno)); return 1;
  }
  printf("idle level of line144 (pin7): %d\n", gpiod_line_get_value(watch));

  for (int i = 0; i < n; i++) {
    if (pins[i].off == 144) continue;
    struct gpiod_line *l = gpiod_chip_get_line(c0, pins[i].off);
    if (!l) { printf("phys%-3d %-22s get_line failed\n", pins[i].phys, pins[i].n); continue; }
    if (gpiod_line_request_output(l, "probe", 1) < 0) {
      printf("phys%-3d %-22s BUSY/ERR: %s\n", pins[i].phys, pins[i].n, strerror(errno));
      continue;
    }
    usleep(30000);
    int r1 = gpiod_line_get_value(watch);
    int own1 = gpiod_line_get_value(l);
    gpiod_line_set_value(l, 0);
    usleep(30000);
    int r0 = gpiod_line_get_value(watch);
    int own0 = gpiod_line_get_value(l);
    printf("phys%-3d %-22s pin7: %d->%d %-10s own: %d->%d\n",
           pins[i].phys, pins[i].n, r1, r0,
           (r1 != r0) ? "<<<< MOVES" : "", own1, own0);
    gpiod_line_release(l);
  }
  return 0;
}
