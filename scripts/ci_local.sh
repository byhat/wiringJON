#!/bin/bash
# ci_local.sh — host-side (x86, no hardware) checks:
#   1. syntax-check every C file
#   2. duplicate global symbol check across the core objects
#   3. full build of devLib (hardware-independent)
#   4. reminder: full link + hardware tests run on the Jetson
set -e
cd "$(dirname "$0")/.."

GPIOD_INC=""
if [ ! -f /tmp/gpiodinc/gpiod.h ]; then
  curl -sL --max-time 20 -o /tmp/gpiodinc/gpiod.h \
    https://raw.githubusercontent.com/brgl/libgpiod/v1.6.x/include/gpiod.h || true
fi
[ -f /tmp/gpiodinc/gpiod.h ] && GPIOD_INC="-I/tmp/gpiodinc"

echo "== syntax check =="
for f in wiringPi/*.c devLib/*.c gpio/*.c wiringPiD/*.c examples/*.c tests/*.c ; do
  gcc -fsyntax-only -Wall -D_GNU_SOURCE -IwiringPi -IdevLib $GPIOD_INC "$f"
done
echo "  all sources OK"

echo "== duplicate symbol check (core) =="
rm -rf /tmp/wjonchk && mkdir -p /tmp/wjonchk
for f in wiringPi/*.c ; do
  gcc -c -O1 -D_GNU_SOURCE -IwiringPi $GPIOD_INC "$f" -o "/tmp/wjonchk/$(basename "${f%.c}").o"
done
dups=$(nm /tmp/wjonchk/*.o | awk '$2=="T"||$2=="D"||$2=="B"{print $3}' | sort | uniq -d)
if [ -n "$dups" ]; then echo "DUPLICATE SYMBOLS: $dups"; exit 1; fi
echo "  no duplicates"

echo "== devLib build =="
make -s -C devLib

echo "== mock unit tests (emulated gpiochips + bench wire) =="
rm -rf /tmp/wjonmock && mkdir -p /tmp/wjonmock
ok=0
for f in wiringPi/*.c ; do
  b=$(basename "${f%.c}")
  gcc -c -O1 -D_GNU_SOURCE -IwiringPi $GPIOD_INC "$f" -o "/tmp/wjonmock/$b.o" || exit 1
  ok=$((ok+1))
done
gcc -O1 -D_GNU_SOURCE -IwiringPi $GPIOD_INC tests/test_mock.c tests/mock_gpiod.c tests/mock_crypt.c \
    /tmp/wjonmock/*.o -o /tmp/test_mock -lpthread -lm
/tmp/test_mock || exit 1

echo "== gpio readall on mock =="
gcc -O1 -D_GNU_SOURCE -IwiringPi -IdevLib $GPIOD_INC gpio/gpio.c gpio/readall.c \
    tests/mock_gpiod.c tests/mock_crypt.c /tmp/wjonmock/*.o -o /tmp/gpio_mock \
    -lpthread -lm -IdevLib 2>/dev/null || \
gcc -O1 -D_GNU_SOURCE -IwiringPi -IdevLib $GPIOD_INC gpio/gpio.c gpio/readall.c \
    devLib/*.o tests/mock_gpiod.c tests/mock_crypt.c /tmp/wjonmock/*.o -o /tmp/gpio_mock \
    -lpthread -lm
WIRINGJON_FORCE_BOARD=1 /tmp/gpio_mock -v | head -8
WIRINGJON_FORCE_BOARD=1 /tmp/gpio_mock readall

echo "== local CI PASS (link + hardware tests run on the Jetson) =="
