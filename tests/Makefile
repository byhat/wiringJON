#
# Makefile:
#	wiringJON hardware tests
#################################################################################

CC	?= gcc
CFLAGS	= -O2 -Wall -Wextra -I../wiringPi
LDFLAGS	= -L../wiringPi
LIBS	= -lwiringPi

PROGS	= test_gpio test_serial test_spi test_i2c

all:		$(PROGS)

test_gpio:	test_gpio.c ../wiringPi/libwiringPi.so
		$(CC) $(CFLAGS) -o $@ test_gpio.c $(LDFLAGS) $(LIBS)

test_serial:	test_serial.c ../wiringPi/libwiringPi.so
		$(CC) $(CFLAGS) -o $@ test_serial.c $(LDFLAGS) $(LIBS)

test_spi:	test_spi.c ../wiringPi/libwiringPi.so
		$(CC) $(CFLAGS) -o $@ test_spi.c $(LDFLAGS) $(LIBS)

test_i2c:	test_i2c.c ../wiringPi/libwiringPi.so
		$(CC) $(CFLAGS) -o $@ test_i2c.c $(LDFLAGS) $(LIBS)

clean:
		rm -f $(PROGS) *~

.PHONY:		all clean
