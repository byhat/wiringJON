#
# Makefile:
#	wiringJON — wiringPi-compatible core for the Jetson Orin Nano
#	(libgpiod backend)
#
#	LGPLv3, see COPYING.LESSER. API contract: wiringPi 3.20.
#################################################################################

VERSION=$(shell cat ../VERSION)
DESTDIR?=/usr
PREFIX?=/local
LDCONFIG?=ldconfig

SONAME=libwiringPi.so.3
STATIC=libwiringPi.a
DYNAMIC=libwiringPi.so.$(VERSION)

DEBUG	= -O2
CC	?= gcc
INCLUDE	= -I.
CFLAGS	= $(DEBUG) -Wall -Wextra $(INCLUDE) -fPIC -D_GNU_SOURCE
LIBS	= -lgpiod -lpthread

SRC	= wiringPi.c pinmap.c
OBJ	= $(SRC:.c=.o)
HEADERS	= wiringPi.h pinmap.h

all:		$(DYNAMIC) $(STATIC)

$(DYNAMIC):	$(OBJ)
		$(CC) -shared -Wl,-soname,$(SONAME) -o $(DYNAMIC) $(OBJ) $(LIBS)

$(STATIC):	$(OBJ)
		$(AR) rcs $(STATIC) $(OBJ)

%.o:		%.c
		$(CC) -c $(CFLAGS) -o $@ $<

clean:
		rm -f $(OBJ) $(DYNAMIC) $(STATIC) *~

install:	$(DYNAMIC) $(STATIC)
		install -m 0755 -d						$(DESTDIR)$(PREFIX)/include
		install -m 0644 $(HEADERS)					$(DESTDIR)$(PREFIX)/include
		install -m 0755 -d						$(DESTDIR)$(PREFIX)/lib
		install -m 0755 libwiringPi.so.$(VERSION)			$(DESTDIR)$(PREFIX)/lib/libwiringPi.so.$(VERSION)
		ln -sf libwiringPi.so.$(VERSION)				$(DESTDIR)$(PREFIX)/lib/$(SONAME)
		ln -sf libwiringPi.so.$(VERSION)				$(DESTDIR)$(PREFIX)/lib/libwiringPi.so
		install -m 0755 libwiringPi.a					$(DESTDIR)$(PREFIX)/lib/libwiringPi.a
		$(LDCONFIG)

uninstall:
		rm -f $(DESTDIR)$(PREFIX)/lib/libwiringPi.so*
		rm -f $(DESTDIR)$(PREFIX)/lib/libwiringPi.a
		rm -f $(DESTDIR)$(PREFIX)/include/wiringPi.h
		rm -f $(DESTDIR)$(PREFIX)/include/pinmap.h
		$(LDCONFIG)

.PHONY:		all clean install uninstall
