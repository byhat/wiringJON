#
# Makefile:
#	wiringJON wiringPiD daemon (drcNet server), ported from wiringPi
#################################################################################

DEBUG	= -O2
CC	?= gcc
INCLUDE	= -I../wiringPi
CFLAGS	= $(DEBUG) -Wall -Wextra $(INCLUDE) -D_GNU_SOURCE
LDFLAGS	= -L../wiringPi
LIBS	= -lwiringPi -lpthread -lm

SRC	= daemonise.c network.c runRemote.c wiringpid.c
OBJ	= $(SRC:.c=.o)

all:		wiringpid

wiringpid:	$(OBJ)
		$(CC) -o $@ $(OBJ) $(LDFLAGS) $(LIBS)

%.o:		%.c
		$(CC) -c $(CFLAGS) -o $@ $<

clean:
		rm -f $(OBJ) wiringpid *~

install:	wiringpid
		install -m 0755 -d $(DESTDIR)$(PREFIX)/bin
		install -m 0755 wiringpid $(DESTDIR)$(PREFIX)/bin/wiringpid

uninstall:
		rm -f $(DESTDIR)$(PREFIX)/bin/wiringpid

.PHONY:		all clean install uninstall
