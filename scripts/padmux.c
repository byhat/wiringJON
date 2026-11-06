/* padmux.c — dump (and optionally write) Tegra234 PINMUX pad-control
 * registers for the 40-pin header pads.
 * Usage: padmux                 — dump all header pads
 *        padmux w <hexaddr> <hexval> — write one padctl register (mmap) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

static unsigned padRead(int fd, unsigned long a)
{
  size_t pagesz = 4096;
  unsigned long base = a & ~(pagesz - 1);
  void *m = mmap(NULL, pagesz, PROT_READ, MAP_SHARED, fd, base);
  if (m == MAP_FAILED) { perror("mmap"); exit(1); }
  unsigned v = *(volatile unsigned *)((char *)m + (a - base));
  munmap(m, pagesz);
  return v;
}

static void padWrite(int fd, unsigned long a, unsigned v)
{
  size_t pagesz = 4096;
  unsigned long base = a & ~(pagesz - 1);
  void *m = mmap(NULL, pagesz, PROT_READ | PROT_WRITE, MAP_SHARED, fd, base);
  if (m == MAP_FAILED) { perror("mmap"); exit(1); }
  *(volatile unsigned *)((char *)m + (a - base)) = v;
  munmap(m, pagesz);
}

int main(int argc, char **argv)
{
  int fd = open("/dev/mem", O_RDWR | O_SYNC);
  if (fd < 0) { perror("open /dev/mem (need root)"); return 1; }

  if (argc == 4 && !strcmp(argv[1], "w"))
  {
    unsigned long a = strtoul(argv[2], NULL, 16);
    unsigned v = strtoul(argv[3], NULL, 16);
    printf("before: 0x%08lx = 0x%08x\n", a, padRead(fd, a));
    padWrite(fd, a, v);
    printf("after:  0x%08lx = 0x%08x\n", a, padRead(fd, a));
    return 0;
  }

  struct { int phys; const char *pin; unsigned long addr; } pads[] = {
    { 7,  "PAC.06", 0x2448030 }, { 11, "PR.04",  0x2430098 },
    { 12, "PH.07",  0x2434088 }, { 13, "PY.00",  0x243D030 },
    { 15, "PN.01",  0x2440020 }, { 16, "PY.04",  0x243D020 },
    { 18, "PY.03",  0x243D010 }, { 19, "PZ.05",  0x243D040 },
    { 21, "PZ.04",  0x243D018 }, { 22, "PY.01",  0x243D000 },
    { 23, "PZ.03",  0x243D028 }, { 24, "PZ.06",  0x243D008 },
    { 26, "PZ.07",  0x243D038 }, { 29, "PQ.05",  0x2430068 },
    { 31, "PQ.06",  0x2430070 }, { 32, "PG.06",  0x2434080 },
    { 33, "PH.00",  0x2434040 }, { 35, "PI.02",  0x24340A0 },
    { 36, "PR.05",  0x2430090 }, { 37, "PY.02",  0x243D048 },
    { 38, "PI.01",  0x2434098 }, { 40, "PI.00",  0x2434090 },
  };

  for (unsigned i = 0; i < sizeof(pads)/sizeof(pads[0]); ++i)
  {
    unsigned v = padRead(fd, pads[i].addr);
    printf("phys %-3d %-6s 0x%08lx = 0x%08x  mux=%u pull=%u tri=%u ein=%u sfsel=%u\n",
           pads[i].phys, pads[i].pin, pads[i].addr, v,
           v & 3, (v >> 2) & 3, (v >> 4) & 1, (v >> 6) & 1, (v >> 10) & 1);
  }
  return 0;
}

