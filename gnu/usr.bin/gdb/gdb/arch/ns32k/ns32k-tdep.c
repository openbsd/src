/* Target dependent functions for ns532 based PC532 board.
   Copyright (C) 1992, 1993 Free Software Foundation, Inc.

This file is part of GDB.

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
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* @@@ isa_NAN should be in ieee generic float routines file */

/* Check for bad floats/doubles in P

   LEN says whether this is a single-precision or double-precision float.
   
   Returns 0 when valid IEEE floating point number
   	   1 when LEN is invalid
	   2 when NAN
	   3 when denormalized
 */

#include <sys/types.h>
#include <machine/reg.h>

#define SINGLE_EXP_BITS  8
#define DOUBLE_EXP_BITS 11
int
isa_NAN(p, len)
     int *p, len;
{
  int exponent;
  if (len == 4)
    {
      exponent = *p;
      exponent = exponent << 1 >> (32 - SINGLE_EXP_BITS - 1);
      if (exponent == -1)
	return 2;		/* NAN */
      else if (!exponent && (*p & 0x7fffffff)) /* Mask sign bit off */
	return 3;		/* Denormalized */
      else
	return 0;
    }
  else if (len == 8)
    {
      exponent = *(p+1);
      exponent = exponent << 1 >> (32 - DOUBLE_EXP_BITS - 1);
      if (exponent == -1)
	return 2;		/* NAN */
      else if (!exponent && ((*p & 0x7fffffff) || *(p+1)))
	return 3;		/* Denormalized */
      else
	return 0;
    }
  else return 1;
}
