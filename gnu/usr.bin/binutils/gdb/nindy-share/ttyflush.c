/* This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* This started out life as code shared between the nindy monitor and
   GDB.  For various reasons, this is no longer true.  Eventually, it
   probably should be merged into remote-nindy.c.  */

#include <stdio.h>
#include "defs.h"
#include "serial.h"

/* Flush all pending input and output for SERIAL, wait for a second, and
   then if there is a character pending, discard it and flush again.  */

int
tty_flush (serial)
     serial_t serial;
{
  while (1)
    {
      SERIAL_FLUSH_INPUT (serial);
      SERIAL_FLUSH_OUTPUT (serial);
      sleep(1);
      switch (SERIAL_READCHAR (serial, 0))
	{
	case SERIAL_TIMEOUT:
	case SERIAL_ERROR:
	case SERIAL_EOF:
	  return 0;
	default:
	  /* We read something.  Eeek.  Try again.  */
	  break;
	}
    }
}
