/* Target machine definitions for GDB for an embedded SPARC.
   Copyright 1989, 1992, 1993 Free Software Foundation, Inc.

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

#include "sparc/tm-sparc.h"

#define TARGET_SPARCLET 1

/* overrides of tm-sparc.h */

/* Initializer for an array of names of registers.
   There should be NUM_REGS strings in this initializer.  */
/* Sparclet has no fp! */
/* Compiler maps types for floats by number, so can't 
   change the numbers here. */

#undef REGISTER_NAMES
/* g0 removed - Sparclet returns error if attempt to access. */
/* psr removed - Sparclet does not return ": " in response, 
   the monitor is therefore unable to get the expected response delimiter, 
   causing a timeout. */
#define REGISTER_NAMES  \
{ "", "g1", "g2", "g3", "g4", "g5", "g6", "g7",	\
  "o0", "o1", "o2", "o3", "o4", "o5", "o6", "o7",	\
  "l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7",	\
  "i0", "i1", "i2", "i3", "i4", "i5", "i6", "i7",	\
                                                                \
  0, 0, 0, 0, 0, 0, 0, 0,       \
  0, 0, 0, 0, 0, 0, 0, 0, \
  0, 0, 0, 0, 0, 0, 0, 0,       \
  0, 0, 0, 0, 0, 0, 0, 0,       \
                                                                \
  "y", 0, "wim", "tbr", "pc", "npc", 0, 0 }

/* Remove FP dependant code which was defined in tm-sparc.h */
#undef	FP0_REGNUM /* Floating point register 0 */
#undef	FPS_REGNUM /* Floating point status register */
#undef 	CPS_REGNUM /* Coprocessor status register */

#undef EXTRACT_RETURN_VALUE
#define EXTRACT_RETURN_VALUE(TYPE,REGBUF,VALBUF)          \
  {                                                                    \
      memcpy ((VALBUF),                            \
          (char *)(REGBUF) + REGISTER_RAW_SIZE (O0_REGNUM) * 8 +       \
          (TYPE_LENGTH(TYPE) >= REGISTER_RAW_SIZE (O0_REGNUM)      \
           ? 0 : REGISTER_RAW_SIZE (O0_REGNUM) - TYPE_LENGTH(TYPE)),   \
          TYPE_LENGTH(TYPE));                      \
  }
#undef STORE_RETURN_VALUE
#define STORE_RETURN_VALUE(TYPE,VALBUF) \
  {                                                                          \
      /* Other values are returned in register %o0.  */                      \
      write_register_bytes (REGISTER_BYTE (O0_REGNUM), (VALBUF),         \
                TYPE_LENGTH (TYPE));  \
  }
#undef PRINT_REGISTER_HOOK
#define PRINT_REGISTER_HOOK(regno)

/* Override the standard gdb prompt when compiled for this target.  */
    
#define DEFAULT_PROMPT  "(gdbslet) "

/* Offsets into jmp_buf.  Not defined by Sun, but at least documented in a
   comment in <machine/setjmp.h>! */

#define JB_ELEMENT_SIZE 4	/* Size of each element in jmp_buf */

#define JB_ONSSTACK 0
#define JB_SIGMASK 1
#define JB_SP 2
#define JB_PC 3
#define JB_NPC 4
#define JB_PSR 5
#define JB_G1 6
#define JB_O0 7
#define JB_WBCNT 8

/* Figure out where the longjmp will land.  We expect that we have just entered
   longjmp and haven't yet setup the stack frame, so the args are still in the
   output regs.  %o0 (O0_REGNUM) points at the jmp_buf structure from which we
   extract the pc (JB_PC) that we will land at.  The pc is copied into ADDR.
   This routine returns true on success */

extern int
get_longjmp_target PARAMS ((CORE_ADDR *));

#define GET_LONGJMP_TARGET(ADDR) get_longjmp_target(ADDR)
