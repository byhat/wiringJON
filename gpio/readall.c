/*
 * readall.c:
 *	The readall functions - getting a bit big, so split them out.
 *	Copyright (c) 2012–2019 Gordon Henderson; 2019–2026 Contributors
 ***********************************************************************
 * This file is part of wiringPi:
 *	https://github.com/WiringPi/WiringPi/
 *
 *    wiringPi is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    wiringPi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public License
 *    along with wiringPi.  If not, see <http://www.gnu.org/licenses/>.
 ***********************************************************************
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <wiringPi.h>

extern int wpMode ;

/*
 * doReadallExternal:
 *	A relatively crude way to read the pins on an external device.
 *	We don't know the input/output mode of pins, but we can tell
 *	if it's an analog pin or a digital one...
 *********************************************************************************
 */

static void doReadallExternal (void)
{
  int pin ;

  printf ("+------+---------+--------+\n") ;
  printf ("|  Pin | Digital | Analog |\n") ;
  printf ("+------+---------+--------+\n") ;

  for (pin = wiringPiNodes->pinBase ; pin <= wiringPiNodes->pinMax ; ++pin)
    printf ("| %4d |  %4d   |  %4d  |\n", pin, digitalRead (pin), analogRead (pin)) ;

  printf ("+------+---------+--------+\n") ;
}


/*
 * doReadall:
 *	Read all the GPIO pins
 *	We also want to use this to read the state of pins on an externally
 *	connected device, so we need to do some fiddling with the internal
 *	wiringPi node structures - since the gpio command can only use
 *	one external device at a time, we'll use that to our advantage...
 *********************************************************************************
 */

#define MAX_ALTS 11
static const char unknown_alt[] = " - ";
static const char *alts [MAX_ALTS+1] =
{
  "IN", "OUT", "ALT5", "ALT4", "ALT0", "ALT1", "ALT2", "ALT3", "ALT6", "ALT7", "ALT8", "ALT9"
} ;


static const char* GetAltString(int alt) {

  if (alt>=0 && alt<=MAX_ALTS) {
    return alts[alt];
  }

  return unknown_alt;
}


/* wiringJON: Orin Nano DevKit 40-pin header — wiringPi numbers are assigned
 * by the same physical positions as the Raspberry Pi so unmodified RPi
 * tutorials address the same header pins. Non-GPIO pins are -1. */
static int physToWpi [64] =
{
  -1,           // 0
  -1, -1,       // 1, 2  (3.3v, 5v)
  -1, -1,       // 3, 4  (I2C1_SDA, 5v)
  -1, -1,       // 5, 6  (I2C1_SCL, 0v)
   7, -1,       // 7, 8  (GPIO09, UART1_TX)
  -1, -1,       // 9, 10 (0v, UART1_RX)
   0,  1,       // 11,12 (UART1_RTS, I2S0_SCLK)
   2, -1,       // 13,14 (SPI1_SCK, 0v)
   3,  4,       // 15,16 (GPIO12, SPI1_CS1)
  -1,  5,       // 17,18 (3.3v, SPI1_CS0)
  12, -1,       // 19,20 (SPI0_MOSI, 0v)
  13,  6,       // 21,22 (SPI0_MISO, SPI1_MISO)
  14, 10,       // 23,24 (SPI0_SCK, SPI0_CS0)
  -1, 11,       // 25,26 (0v, SPI0_CS1)
  -1, -1,       // 27,28 (I2C, no GPIO)
  21, -1,       // 29,30 (GPIO01, 0v)
  22, 26,       // 31,32 (GPIO11, GPIO07)
  23, -1,       // 33,34 (GPIO13, 0v)
  24, 27,       // 35,36 (I2S0_FS, UART1_CTS)
  25, 28,       // 37,38 (SPI1_MOSI, I2S0_SDIN)
  -1, 29,       // 39,40 (0v, I2S0_SDOUT)
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1
} ;

static char *physNames [64] =
{
  NULL,

  "    3.3v", "5v      ",
  "I2C1_SDA", "5v      ",
  "I2C1_SCL", "0v      ",
  "GPIO09  ", "UART1_TX",
  "0v      ", "UART1_RX",
  "UART1RTS", "I2S0SCLK",
  "SPI1_SCK", "0v      ",
  "GPIO12  ", "SPI1_CS1",
  "3.3v    ", "SPI1_CS0",
  "SPI0MOSI", "0v      ",
  "SPI0MISO", "SPI1MISO",
  "SPI0_SCK", "SPI0_CS0",
  "0v      ", "SPI0_CS1",
  "I2C_SDA ", "I2C_SCL ",
  "GPIO01  ", "0v      ",
  "GPIO11  ", "GPIO07  ",
  "GPIO13  ", "0v      ",
  "I2S0_FS ", "U1_CTS  ",
  "SPI1MOSI", "I2S0SDIN",
  "0v      ", "I2S0SDO ",
       NULL, NULL,
       NULL, NULL,
       NULL, NULL,
       NULL, NULL,
       NULL, NULL,
  NULL, NULL,
   NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,
} ;


/*
 * readallPhys:
 *	Given a physical pin output the data on it and the next pin:
 *| BCM | wPi |   Name  | Mode | Val| Physical |Val | Mode | Name    | wPi | BCM |
 *********************************************************************************
 */

static void readallPhys (int physPin)
{
  int pin ;

  if (physPinToGpio (physPin) == -1)
    printf (" |     |    ") ;
  else
    printf (" | %3d | %3d", physPinToGpio (physPin), physToWpi [physPin]) ;

  printf (" | %s", physNames [physPin]) ;

  if (physToWpi [physPin] == -1)
    printf (" |      |  ") ;
  else
  {
    if      (wpMode == WPI_MODE_GPIO)
      pin = physPinToGpio (physPin) ;
    else if (wpMode == WPI_MODE_PHYS)
      pin = physPin ;
    else
      pin = physToWpi [physPin] ;

    printf (" | %4s", GetAltString(getAlt (pin))) ;
    printf (" | %d", digitalRead (pin)) ;
  }

// Pin numbers:

  printf (" | %2d", physPin) ;
  ++physPin ;
  printf (" || %-2d", physPin) ;

// Same, reversed

  if (physToWpi [physPin] == -1)
    printf (" |   |     ") ;
  else
  {
    if      (wpMode == WPI_MODE_GPIO)
      pin = physPinToGpio (physPin) ;
    else if (wpMode == WPI_MODE_PHYS)
      pin = physPin ;
    else
      pin = physToWpi [physPin] ;

    printf (" | %d", digitalRead (pin)) ;
    printf (" | %-4s", GetAltString(getAlt (pin))) ;
  }

  printf (" | %-5s", physNames [physPin]) ;

  if (physToWpi     [physPin] == -1)
    printf (" |     |    ") ;
  else
    printf (" | %-3d | %-3d", physToWpi [physPin], physPinToGpio (physPin)) ;

  printf (" |\n") ;
}


/*
 * allReadall:
 *	Read all the pins regardless of the model. Primarily of use for
 *	the compute module, but handy for other fiddling...
 *********************************************************************************
 */

static void allReadall (void)
{
  int pin ;

  printf ("+-----+------+-------+      +-----+------+-------+\n") ;
  printf ("| Pin | Mode | Value |      | Pin | Mode | Value |\n") ;
  printf ("+-----+------+-------+      +-----+------+-------+\n") ;

  for (pin = 0 ; pin < 27 ; ++pin)
  {
    printf ("| %3d ", pin) ;
    printf ("| %-4s ", GetAltString(getAlt (pin))) ;
    printf ("| %s  ", digitalRead (pin) == HIGH ? "High" : "Low ") ;
    printf ("|      ") ;
    printf ("| %3d ", pin + 27) ;
    printf ("| %-4s ", GetAltString(getAlt (pin + 27))) ;
    printf ("| %s  ", digitalRead (pin + 27) == HIGH ? "High" : "Low ") ;
    printf ("|\n") ;
  }

  printf ("+-----+------+-------+      +-----+------+-------+\n") ;

}


/*
 * abReadall:
 *	Read all the pins on the model A or B.
 *********************************************************************************
 */

void abReadall (int model, int rev)
{
  int pin ;
  char *type ;

  if (model == PI_MODEL_A)
    type = " A" ;
  else
    if (rev == PI_VERSION_2)
      type = "B2" ;
    else
      type = "B1" ;

  printf (" +-----+-----+---------+------+---+-Model %s-+---+------+---------+-----+-----+\n", type) ;
  printf (" | BCM | wPi |   Name  | Mode | V | Physical | V | Mode | Name    | wPi | BCM |\n") ;
  printf (" +-----+-----+---------+------+---+----++----+---+------+---------+-----+-----+\n") ;
  for (pin = 1 ; pin <= 26 ; pin += 2)
    readallPhys (pin) ;

  if (rev == PI_VERSION_2) // B version 2
  {
    printf (" +-----+-----+---------+------+---+----++----+---+------+---------+-----+-----+\n") ;
    for (pin = 51 ; pin <= 54 ; pin += 2)
      readallPhys (pin) ;
  }

  printf (" +-----+-----+---------+------+---+----++----+---+------+---------+-----+-----+\n") ;
  printf (" | BCM | wPi |   Name  | Mode | V | Physical | V | Mode | Name    | wPi | BCM |\n") ;
  printf (" +-----+-----+---------+------+---+-Model %s-+---+------+---------+-----+-----+\n", type) ;
}


/*
 * piPlusReadall:
 *	Read all the pins on the model A+ or the B+ or actually, all 40-pin Pi's
 *********************************************************************************
 */
const char piModelNamesShort[PI_MODELS_MAX][11] =
{
  "---Pi A---",	//  0
  "---Pi B---",	//  1
  "---Pi A+--",	//  2
  "---Pi B+--",	//  3
  "---Pi 2---",	//  4
  "---Alpha--",	//  5
  "----CM----",	//  6
  "",	// 07
  "---Pi 3B--",	//  8
  "-Pi Zero--",	//  9
  "---CM3----",	// 10
  "",	// 11
  "-Pi ZeroW-",	// 12
  "---Pi 3B+-",	// 13
  "---Pi 3A+-",	// 14
  "",	// 15
  "---CM3+---",	// 16
  "---Pi 4B--",	// 17
  "Pi Zero 2W",	// 18
  "--Pi 400--",	// 19
  "---CM4----",	// 20
  "---CM4S---",	// 21
  "",	// 22
  "---Pi 5---",	// 23
  "---CM5----",	// 24
  "--Pi 500--",	// 25
  "---CM5L---",	// 26
  "---CM0----",	// 27
  "-OrinNano-",	// 28 wiringJON: Jetson Orin Nano DevKit
  "",	// 29
  "",	// 30
  "",	// 31
} ;

static void plus2header (int model)
{
  if (model<PI_MODELS_MAX && piModelNamesShort[model][0]!='\0') {
    printf (" +-----+-----+---------+------+---+%s+---+------+---------+-----+-----+\n", piModelNamesShort[model]);
  } else {
    printf (" +-----+-----+---------+------+---+---- ? ---+---+------+---------+-----+-----+\n");
  }
}


static void piPlusReadall (int model)
{
  int pin ;

  plus2header (model) ;

  printf (" | BCM | wPi |   Name  | Mode | V | Physical | V | Mode | Name    | wPi | BCM |\n") ;
  printf (" +-----+-----+---------+------+---+----++----+---+------+---------+-----+-----+\n") ;
  for (pin = 1 ; pin <= 40 ; pin += 2)
    readallPhys (pin) ;
  printf (" +-----+-----+---------+------+---+----++----+---+------+---------+-----+-----+\n") ;
  printf (" | BCM | wPi |   Name  | Mode | V | Physical | V | Mode | Name    | wPi | BCM |\n") ;

  plus2header (model) ;
}


/*
 * doReadall:
 *	Generic read all pins called from main program. Works out the Pi type
 *	and calls the appropriate function.
 *********************************************************************************
 */

void doReadall (void)
{
  int model, rev, mem, maker, overVolted ;

  if (wiringPiNodes != NULL)	// External readall
  {
    doReadallExternal () ;
    return ;
  }

  piBoardId (&model, &rev, &mem, &maker, &overVolted) ;

  if      ((model == PI_MODEL_A) || (model == PI_MODEL_B))
    abReadall (model, rev) ;
  else if ((model == PI_MODEL_BP) || (model == PI_MODEL_AP) ||
	(model == PI_MODEL_2)    ||
	(model == PI_MODEL_3AP)  ||
	(model == PI_MODEL_3B)   || (model == PI_MODEL_3BP) ||
	(model == PI_MODEL_4B)   || (model == PI_MODEL_400) || (model == PI_MODEL_CM4) || (model == PI_MODEL_CM4S) ||
	(model == PI_MODEL_ZERO) || (model == PI_MODEL_ZERO_W) || (model == PI_MODEL_ZERO_2W) || (model == PI_MODEL_CM0) ||
  (model == PI_MODEL_5)    || (model == PI_MODEL_500) || (model == PI_MODEL_CM5) || (model == PI_MODEL_CM5L) ||
  (model == PI_MODEL_JETSON_ORIN) )		// wiringJON
    piPlusReadall (model) ;
  else if ((model == PI_MODEL_CM) || (model == PI_MODEL_CM3) || (model == PI_MODEL_CM3P) )  //could be Compute Module 1/3/3+ IO Board 120 GPIO pins, legacy code
    allReadall () ;
  else
    printf ("Oops - unable to determine board type... model: %d\n", model) ;
}


/*
 * doAllReadall:
 *	Force reading of all pins regardless of Pi model
 *********************************************************************************
 */

void doAllReadall (void)
{
  allReadall () ;
}


/*
 * doQmode:
 *	Query mode on a pin
 *********************************************************************************
 */

void doQmode (int argc, char *argv [])
{
  int pin ;

  if (argc != 3)
  {
    fprintf (stderr, "Usage: %s qmode pin\n", argv [0]) ;
    exit (EXIT_FAILURE) ;
  }

  pin = atoi (argv [2]) ;
  printf ("%s\n", GetAltString(getAlt (pin))) ;
}
