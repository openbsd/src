/* Parameters for targeting to a Bull DPX2.
   Copyright (C) 1986, 1987, 1989, 1991, 1994 Free Software Foundation, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Define BPT_VECTOR if it is different than the default.
   This is the vector number used by traps to indicate a breakpoint. */

#define BPT_VECTOR 0xe

/* Need to get function ends by adding this to epilogue address from .bf
   record, not using x_fsize field.  */
#define FUNCTION_EPILOGUE_SIZE 4

/* The child target can't deal with writing floating registers.  */
#define CANNOT_STORE_REGISTER(regno) ((regno) >= FP0_REGNUM)

#include <sys/types.h>
#include "m68k/tm-m68k.h"
