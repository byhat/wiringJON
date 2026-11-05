#
# Makefile:
#	wiringJON devLib — device-level library (libwiringPiDev), ported from
#	wiringPi; pure /dev & wiringPi-API code, hardware-agnostic.
#################################################################################

VERSION=$(shell cat ../VERSION)
DESTDIR?=/usr
PREFIX?=/local
LDCONFIG?=ldconfig

SONAME=libwiringPiDev.so.3
STATIC=libwiringPiDev.a
DYNAMIC=libwiringPiDev.so.$(VERSION)

DEBUG	= -O2
CC	?= gcc
INCLUDE	= -I. -I../wiringPi
CFLAGS	= $(DEBUG) -Wall -Wextra $(INCLUDE) -fPIC -D_GNU_SOURCE
LIBS	=

SRC	= ds1302.c maxdetect.c piNes.c gertboard.c piFace.c	\
	lcd128x64.c lcd.c scrollPhat.c piGlow.c
OBJ	= $(SRC:.c=.o)
HEADERS	= ds1302.h gertboard.h lcd128x64.h lcd.h maxdetect.h piFace.h	\
	piGlow.h piNes.h scrollPhat.h font.h scrollPhatFont.h

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
		install -m 0755 libwiringPiDev.so.$(VERSION)			$(DESTDIR)$(PREFIX)/lib/libwiringPiDev.so.$(VERSION)
		ln -sf libwiringPiDev.so.$(VERSION)				$(DESTDIR)$(PREFIX)/lib/$(SONAME)
		ln -sf libwiringPiDev.so.$(VERSION)				$(DESTDIR)$(PREFIX)/lib/libwiringPiDev.so
		install -m 0755 libwiringPiDev.a				$(DESTDIR)$(PREFIX)/lib/libwiringPiDev.a
		$(LDCONFIG)

uninstall:
		rm -f $(DESTDIR)$(PREFIX)/lib/libwiringPiDev.so*
		rm -f $(DESTDIR)$(PREFIX)/lib/libwiringPiDev.a
		rm -f $(DESTDIR)$(PREFIX)/include/{ds1302,gertboard,lcd128x64,lcd,maxdetect,piFace,piGlow,piNes,scrollPhat,font,scrollPhatFont}.h
		$(LDCONFIG)

.PHONY:		all clean install uninstall
