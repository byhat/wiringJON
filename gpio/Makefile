#
# Makefile:
#	wiringJON gpio command line utility
#################################################################################

DEBUG	= -O2
CC	?= gcc
INCLUDE	= -I../wiringPi -I../devLib
CFLAGS	= $(DEBUG) -Wall -Wextra $(INCLUDE) -D_GNU_SOURCE
LDFLAGS	= -L../wiringPi -L../devLib
LIBS	= -lwiringPi -lwiringPiDev -lpthread -lm

SRC	= gpio.c readall.c
OBJ	= $(SRC:.c=.o)

all:		gpio

gpio:		$(OBJ)
		$(CC) -o $@ $(OBJ) $(LDFLAGS) $(LIBS)

%.o:		%.c
		$(CC) -c $(CFLAGS) -o $@ $<

clean:
		rm -f $(OBJ) gpio *~

install:	gpio
		install -m 0755 -d $(DESTDIR)$(PREFIX)/bin
		install -m 4755 gpio $(DESTDIR)$(PREFIX)/bin/gpio
		install -m 0755 -d $(DESTDIR)$(PREFIX)/share/man/man1
		install -m 0644 gpio.1 $(DESTDIR)$(PREFIX)/share/man/man1/gpio.1

uninstall:
		rm -f $(DESTDIR)$(PREFIX)/bin/gpio
		rm -f $(DESTDIR)$(PREFIX)/share/man/man1/gpio.1

.PHONY:		all clean install uninstall
