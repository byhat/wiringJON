#
# Makefile:
#	wiringJON top level
#################################################################################

all:
		$(MAKE) -C wiringPi
		$(MAKE) -C tests

clean:
		$(MAKE) -C wiringPi clean
		$(MAKE) -C tests clean

install:
		$(MAKE) -C wiringPi install

uninstall:
		$(MAKE) -C wiringPi uninstall

.PHONY:		all clean install uninstall
