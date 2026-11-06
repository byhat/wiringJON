/*
 * padctlJetson.c — see padctlJetson.h for the story.
 *
 * Pad-control register addresses come from the same Jetson.GPIO pin data
 * as the line offsets (PADCTL column), cross-checked against
 * drivers/pinctrl/tegra/pinctrl-tegra234.c (PINGROUP(..., 0x4080, ...)).
 ***********************************************************************
 * LGPLv3, see COPYING.LESSER.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "padctlJetson.h"

#define PADCTL_BASE	0x02430000ul
#define PADCTL_WINDOW	0x00002000ul	/* cover 0x2430000..0x245FFFF? no: two windows below */

/* phys -> padctl register offset (from P3768+P3767 pin data) */
static const struct { int phys ; unsigned off ; } padOffs [] =
{
  {  7, 0x18030 }, { 11, 0x0098 }, { 12, 0x4088 }, { 13, 0xD030 },
  { 15, 0x0020 }, { 16, 0xD020 }, { 18, 0xD010 }, { 19, 0xD040 },
  { 21, 0xD018 }, { 22, 0xD000 }, { 23, 0xD028 }, { 24, 0xD008 },
  { 26, 0xD038 }, { 29, 0x0068 }, { 31, 0x0070 }, { 32, 0x4080 },
  { 33, 0x4040 }, { 35, 0x40A0 }, { 36, 0x0090 }, { 37, 0xD048 },
  { 38, 0x4098 }, { 40, 0x4090 },
} ;

/* bit positions per drivers/pinctrl/tegra/pinctrl-tegra234.c
 * PIN_PINGROUP_ENTRY_Y: mux [1:0], pupd [3:2], tri 4, e_input 6, sfsel 10 */
#define BIT_TRISTATE	(1u << 4)
#define BIT_E_INPUT	(1u << 6)
#define BIT_SFSEL	(1u << 10)

static unsigned mapRead (unsigned char *map, unsigned off)
{
  return *(volatile unsigned *)(map + off) ;
}

static void mapWrite (unsigned char *map, unsigned off, unsigned v)
{
  *(volatile unsigned *)(map + off) = v ;
}

int wjonPadctlFixup (void)
{
  int fd = open ("/dev/mem", O_RDWR | O_SYNC) ;
  if (fd < 0)
    return -1 ;

  /* offsets used: 0x0068..0x40A0 (main 0x243xxxx) and 0x18030 (0x2448xxx)
   * -> two 0x1000 pages cover everything (0x0000-0x0FFF and the page with
   * 0x40xx; 0x18030 lives in page 0x18000 which also maps fine) */
  unsigned char *m1 = mmap (NULL, 0x18040, PROT_READ | PROT_WRITE,
                            MAP_SHARED, fd, PADCTL_BASE) ;
  if (m1 == MAP_FAILED)
  {
    close (fd) ;
    return -1 ;
  }

  int fixed = 0 ;
  for (unsigned i = 0 ; i < sizeof (padOffs) / sizeof (padOffs [0]) ; ++i)
  {
    unsigned off = padOffs [i].off ;
    unsigned v = mapRead (m1, off) ;
    unsigned want = (v | BIT_E_INPUT) & ~(BIT_TRISTATE | BIT_SFSEL) ;
    if (v != want)
    {
      mapWrite (m1, off, want) ;
      ++fixed ;
    }
  }

  munmap (m1, 0x18040) ;
  close (fd) ;
  return fixed ;
}
