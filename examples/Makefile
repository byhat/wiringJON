#
# Makefile:
#	wiringJON classic examples — same code as on the Raspberry Pi
#################################################################################

CC	?= gcc
INCLUDE	= -I../wiringPi
CFLAGS	= -O2 -Wall $(INCLUDE)
LDFLAGS	= -L../wiringPi -L../devLib
LIBS	= -lwiringPi -lwiringPiDev -lpthread -lm

PROGS	= blink isr softpwm servo

all:		$(PROGS)

%:		%.c
		$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LIBS)

clean:
		rm -f $(PROGS) *~

.PHONY:		all clean
