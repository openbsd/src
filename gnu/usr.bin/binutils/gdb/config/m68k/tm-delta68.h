// OBSOLETE /* Target definitions for delta68.
// OBSOLETE    Copyright 1993, 1994, 1998, 1999, 2000 Free Software Foundation, Inc.
// OBSOLETE 
// OBSOLETE    This file is part of GDB.
// OBSOLETE 
// OBSOLETE    This program is free software; you can redistribute it and/or modify
// OBSOLETE    it under the terms of the GNU General Public License as published by
// OBSOLETE    the Free Software Foundation; either version 2 of the License, or
// OBSOLETE    (at your option) any later version.
// OBSOLETE 
// OBSOLETE    This program is distributed in the hope that it will be useful,
// OBSOLETE    but WITHOUT ANY WARRANTY; without even the implied warranty of
// OBSOLETE    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// OBSOLETE    GNU General Public License for more details.
// OBSOLETE 
// OBSOLETE    You should have received a copy of the GNU General Public License
// OBSOLETE    along with this program; if not, write to the Free Software
// OBSOLETE    Foundation, Inc., 59 Temple Place - Suite 330,
// OBSOLETE    Boston, MA 02111-1307, USA.  */
// OBSOLETE 
// OBSOLETE struct frame_info;
// OBSOLETE 
// OBSOLETE #include "regcache.h"
// OBSOLETE 
// OBSOLETE /* Define BPT_VECTOR if it is different than the default.
// OBSOLETE    This is the vector number used by traps to indicate a breakpoint. */
// OBSOLETE 
// OBSOLETE #define BPT_VECTOR 0x1
// OBSOLETE 
// OBSOLETE #define GCC_COMPILED_FLAG_SYMBOL "gcc_compiled%"
// OBSOLETE #define GCC2_COMPILED_FLAG_SYMBOL "gcc2_compiled%"
// OBSOLETE 
// OBSOLETE /* Amount PC must be decremented by after a breakpoint.
// OBSOLETE    On the Delta, the kernel decrements it for us.  */
// OBSOLETE 
// OBSOLETE #define DECR_PC_AFTER_BREAK 0
// OBSOLETE 
// OBSOLETE /* Not sure what happens if we try to store this register, but
// OBSOLETE    phdm@info.ucl.ac.be says we need this define.  */
// OBSOLETE 
// OBSOLETE #define CANNOT_STORE_REGISTER(regno)	(regno == FPI_REGNUM)
// OBSOLETE 
// OBSOLETE /* Extract from an array REGBUF containing the (raw) register state
// OBSOLETE    a function return value of type TYPE, and copy that, in virtual format,
// OBSOLETE    into VALBUF.  */
// OBSOLETE 
// OBSOLETE /* When it returns a float/double value, use fp0 in sysV68.  */
// OBSOLETE /* When it returns a pointer value, use a0 in sysV68.  */
// OBSOLETE 
// OBSOLETE #define DEPRECATED_EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF)		\
// OBSOLETE   if (TYPE_CODE (TYPE) == TYPE_CODE_FLT)				\
// OBSOLETE     DEPRECATED_REGISTER_CONVERT_TO_VIRTUAL (FP0_REGNUM, TYPE,			\
// OBSOLETE 				 &REGBUF[DEPRECATED_REGISTER_BYTE (FP0_REGNUM)],	\
// OBSOLETE 				 VALBUF);				\
// OBSOLETE   else									\
// OBSOLETE     memcpy ((VALBUF),							\
// OBSOLETE 	    (char *) ((REGBUF) +					\
// OBSOLETE 		      (TYPE_CODE(TYPE) == TYPE_CODE_PTR ? 8 * 4 :	\
// OBSOLETE 		       (TYPE_LENGTH(TYPE) >= 4 ? 0 : 4 - TYPE_LENGTH(TYPE)))), \
// OBSOLETE 	    TYPE_LENGTH(TYPE))
// OBSOLETE 
// OBSOLETE /* Write into appropriate registers a function return value
// OBSOLETE    of type TYPE, given in virtual format.  */
// OBSOLETE 
// OBSOLETE /* When it returns a float/double value, use fp0 in sysV68.  */
// OBSOLETE /* When it returns a pointer value, use a0 in sysV68.  */
// OBSOLETE 
// OBSOLETE #define DEPRECATED_STORE_RETURN_VALUE(TYPE,VALBUF) \
// OBSOLETE   if (TYPE_CODE (TYPE) == TYPE_CODE_FLT)				\
// OBSOLETE       {									\
// OBSOLETE 	char raw_buf[DEPRECATED_REGISTER_RAW_SIZE (FP0_REGNUM)];			\
// OBSOLETE 	DEPRECATED_REGISTER_CONVERT_TO_RAW (TYPE, FP0_REGNUM, VALBUF, raw_buf);	\
// OBSOLETE 	deprecated_write_register_bytes (DEPRECATED_REGISTER_BYTE (FP0_REGNUM),		\
// OBSOLETE 			      raw_buf, DEPRECATED_REGISTER_RAW_SIZE (FP0_REGNUM)); \
// OBSOLETE       }									\
// OBSOLETE   else									\
// OBSOLETE     deprecated_write_register_bytes ((TYPE_CODE(TYPE) == TYPE_CODE_PTR ? 8 * 4 : 0), \
// OBSOLETE 			  VALBUF, TYPE_LENGTH (TYPE))
// OBSOLETE 
// OBSOLETE /* Return number of args passed to a frame.
// OBSOLETE    Can return -1, meaning no way to tell.  */
// OBSOLETE 
// OBSOLETE extern int delta68_frame_num_args (struct frame_info *fi);
// OBSOLETE #define FRAME_NUM_ARGS(fi) (delta68_frame_num_args ((fi)))
// OBSOLETE 
// OBSOLETE /* On M68040 versions of sysV68 R3V7.1, ptrace(PT_WRITE_I) does not clear
// OBSOLETE    the processor's instruction cache as it should.  */
// OBSOLETE #define CLEAR_INSN_CACHE()	clear_insn_cache()
// OBSOLETE 
// OBSOLETE #include "m68k/tm-m68k.h"
// OBSOLETE 
// OBSOLETE /* Extract from an array REGBUF containing the (raw) register state
// OBSOLETE    the address in which a function should return its structure value,
// OBSOLETE    as a CORE_ADDR (or an expression that can be used as one).  */
// OBSOLETE 
// OBSOLETE #undef DEPRECATED_EXTRACT_STRUCT_VALUE_ADDRESS
// OBSOLETE #define DEPRECATED_EXTRACT_STRUCT_VALUE_ADDRESS(REGBUF)\
// OBSOLETE 	(*(CORE_ADDR *)((char*)(REGBUF) + 8 * 4))
// OBSOLETE 
// OBSOLETE extern int delta68_in_sigtramp (CORE_ADDR pc, char *name);
// OBSOLETE #define IN_SIGTRAMP(pc,name) delta68_in_sigtramp (pc, name)
// OBSOLETE 
// OBSOLETE extern CORE_ADDR delta68_frame_saved_pc (struct frame_info *fi);
// OBSOLETE #undef DEPRECATED_FRAME_SAVED_PC
// OBSOLETE #define DEPRECATED_FRAME_SAVED_PC(fi) delta68_frame_saved_pc (fi)
// OBSOLETE 
// OBSOLETE extern CORE_ADDR delta68_frame_args_address (struct frame_info *fi);
// OBSOLETE #undef DEPRECATED_FRAME_ARGS_ADDRESS
// OBSOLETE #define DEPRECATED_FRAME_ARGS_ADDRESS(fi) delta68_frame_args_address (fi)
