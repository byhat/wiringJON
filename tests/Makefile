#
# Makefile:
#	wiringJON hardware tests
#################################################################################

CC	?= gcc
CFLAGS	= -O2 -Wall -Wextra -I../wiringPi
LDFLAGS	= -L../wiringPi
LIBS	= -lwiringPi

PROGS	= test_gpio

all:		$(PROGS)

test_gpio:	test_gpio.c ../wiringPi/libwiringPi.so
		$(CC) $(CFLAGS) -o $@ test_gpio.c $(LDFLAGS) $(LIBS)

clean:
		rm -f $(PROGS) *~

.PHONY:		all clean
