/* Target definitions for delta68.
   Copyright 1993, 1994 Free Software Foundation, Inc.

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

#define BPT_VECTOR 0x1

#undef CPLUS_MARKER
#define CPLUS_MARKER '%'

#define GCC_COMPILED_FLAG_SYMBOL "gcc_compiled%"
#define GCC2_COMPILED_FLAG_SYMBOL "gcc2_compiled%"

/* Amount PC must be decremented by after a breakpoint.
   On the Delta, the kernel decrements it for us.  */

#define DECR_PC_AFTER_BREAK 0

/* Not sure what happens if we try to store this register, but
   phdm@info.ucl.ac.be says we need this define.  */

#define CANNOT_STORE_REGISTER(regno)	(regno == FPI_REGNUM)

/* Extract from an array REGBUF containing the (raw) register state
   a function return value of type TYPE, and copy that, in virtual format,
   into VALBUF.  */

/* When it returns a pointer value, use a0 in sysV68.  */

#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF) \
  memcpy ((VALBUF),							\
	  (char *) ((REGBUF) +						\
		    (TYPE_CODE(TYPE) == TYPE_CODE_PTR ? 8 * 4 :		\
		     (TYPE_LENGTH(TYPE) >= 4 ? 0 : 4 - TYPE_LENGTH(TYPE)))), \
	  TYPE_LENGTH(TYPE))

/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format.  */

/* When it returns a pointer value, use a0 in sysV68.  */

#define STORE_RETURN_VALUE(TYPE,VALBUF) \
  write_register_bytes ((TYPE_CODE(TYPE) == TYPE_CODE_PTR ? 8 * 4 : 0),	\
			VALBUF, TYPE_LENGTH (TYPE))


#include "m68k/tm-m68k.h"
