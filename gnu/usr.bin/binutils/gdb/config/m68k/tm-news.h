/* Parameters for execution on a Sony/NEWS, for GDB, the GNU debugger.
   Copyright 1987, 1989, 1991, 1993 Free Software Foundation, Inc.

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

/* See following cpu type determination macro to get the machine type.

Here is an m-news.h file for gdb.  It supports the 68881 registers.
                                           by hikichi@srava.sra.junet

* Ptrace for handling floating register has a bug(before NEWS OS version 2.2),
* After NEWS OS version 3.2, some of ptrace's bug is fixed.
  But we cannot change the floating register(see adb(1) in OS 3.2) yet.  */

/* Extract from an array REGBUF containing the (raw) register state
   a function return value of type TYPE, and copy that, in virtual format,
   into VALBUF.  */

/* when it return the floating value, use the FP0 in NEWS.  */
#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF) \
  { if (TYPE_CODE (TYPE) == TYPE_CODE_FLT) \
      { \
	REGISTER_CONVERT_TO_VIRTUAL (FP0_REGNUM, \
				     REGISTER_VIRTUAL_TYPE (FP0_REGNUM), \
			             &REGBUF[REGISTER_BYTE (FP0_REGNUM)], \
				     VALBUF); \
      } \
    else \
      memcpy (VALBUF, REGBUF, TYPE_LENGTH (TYPE)); }

/* Write into appropriate registers a function return value
   of type TYPE, given in virtual format.  */

/* when it return the floating value, use the FP0 in NEWS.  */
#define STORE_RETURN_VALUE(TYPE,VALBUF) \
  { if (TYPE_CODE (TYPE) == TYPE_CODE_FLT) \
      { \
	char raw_buf[REGISTER_RAW_SIZE (FP0_REGNUM)]; \
	REGISTER_CONVERT_TO_RAW (REGISTER_VIRTUAL_TYPE (FP0_REGNUM), \
				 FP0_REGNUM, VALBUF, raw_buf); \
	write_register_bytes (FP0_REGNUM, \
			      raw_buf, REGISTER_RAW_SIZE (FP0_REGNUM)); \
      } \
    else \
      write_register_bytes (0, VALBUF, TYPE_LENGTH (TYPE)); }

/* Return number of args passed to a frame.
   Can return -1, meaning no way to tell.  */

#define FRAME_NUM_ARGS(val, fi)  \
{ register CORE_ADDR pc = FRAME_SAVED_PC (fi);			\
  register int insn = 0177777 & read_memory_integer (pc, 2);	\
  val = 0;							\
  if (insn == 0047757 || insn == 0157374)  /* lea W(sp),sp or addaw #W,sp */ \
    val = read_memory_integer (pc + 2, 2);			\
  else if ((insn & 0170777) == 0050217 /* addql #N, sp */	\
	   || (insn & 0170777) == 0050117)  /* addqw */		\
    { val = (insn >> 9) & 7; if (val == 0) val = 8; }		\
  else if (insn == 0157774) /* addal #WW, sp */			\
    val = read_memory_integer (pc + 2, 4);			\
  val >>= 2; }

#include "m68k/tm-m68k.h"
