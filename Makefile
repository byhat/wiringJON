#
# Makefile:
#	wiringJON top level
#################################################################################

SUBDIRS	= wiringPi devLib gpio wiringPiD tests examples

all:
		for d in wiringPi devLib gpio wiringPiD tests ; do $(MAKE) -C $$d || exit 1 ; done

clean:
		for d in $(SUBDIRS) ; do $(MAKE) -C $$d clean ; done

install:
		for d in wiringPi devLib gpio wiringPiD ; do $(MAKE) -C $$d install || exit 1 ; done

uninstall:
		for d in wiringPi devLib gpio wiringPiD ; do $(MAKE) -C $$d uninstall || exit 1 ; done

.PHONY:		all clean install uninstall
