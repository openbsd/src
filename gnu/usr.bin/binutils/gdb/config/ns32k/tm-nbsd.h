/* Macro definitions for ns32k running under NetBSD.
   Copyright 1986, 1987, 1989, 1991, 1992, 1993 Free Software Foundation, Inc.

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

/* Override number of expected traps from sysv. */
#define START_INFERIOR_TRAPS_EXPECTED 2
#define INVALID_FLOAT(p, s) invalid_float(p, s)

/* Most definitions from umax could be used. */

#include "ns32k/tm-umax.h"

/* Generic NetBSD definitions.  */

#include "tm-nbsd.h"

/* Saved Pc.  Get it from sigcontext if within sigtramp.  */

/* We define our own fetch and store methods. */
#define FETCH_INFERIOR_REGISTERS

/* Offset to saved PC in sigcontext, from <machine/signal.h>.  */
#define SIGCONTEXT_PC_OFFSET 20

#undef FRAME_SAVED_PC(FRAME)
#define FRAME_SAVED_PC(FRAME) \
  (((FRAME)->signal_handler_caller \
    ? sigtramp_saved_pc (FRAME) \
    : read_memory_integer ((FRAME)->frame + 4, 4)) \
   )

#undef FRAME_NUM_ARGS
#define FRAME_NUM_ARGS(numargs, fi) numargs = frame_num_args(fi)

#undef FRAME_CHAIN
#define FRAME_CHAIN(thisframe)  \
  (read_memory_integer ((thisframe)->frame, 4) > (thisframe)->frame ? \
   read_memory_integer ((thisframe)->frame, 4) : 0)

#define FRAME_CHAIN_VALID(chain, thisframe)\
     ((chain) != 0\
   && !inside_main_func ((thisframe) -> pc))

/* tm-umax.h assumes a 32082 fpu. We have a 32382 fpu. */
#undef REGISTER_NAMES
#undef NUM_REGS
#undef REGISTER_BYTES
#undef  REGISTER_BYTE
/* Initializer for an array of names of registers.
   There should be NUM_REGS strings in this initializer.  */

#define REGISTER_NAMES {"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",	\
 			"f0", "f1", "f2", "f3", "f4", "f5", "f6", "f7",	\
			"sp", "fp", "pc", "ps",				\
 			"fsr",						\
			"l0", "l1", "l2", "l3", "l4", "l5", "l6", "l7", "xx",			\
 			}

#define NUM_REGS		29

/* Total amount of space needed to store our copies of the machine's
   register state, the array `registers'.  */
#define REGISTER_BYTES \
  ((NUM_REGS - 4) * REGISTER_RAW_SIZE(R0_REGNUM) \
   + 8            * REGISTER_RAW_SIZE(LP0_REGNUM))

/* Index within `registers' of the first byte of the space for
   register N.  */

/* This is a bit yuck. The even numbered double precision floating
   point long registers occupy the same space as the even:odd numbered
   single precision floating point registers, but the extra 32381 fpu
   registers are at the end. Doing it this way is compatable for both
   32081 and 32381 equiped machines. */

#define REGISTER_BYTE(N) (((N) < LP0_REGNUM? (N)\
			   : ((N) - LP0_REGNUM) & 1? (N) - 1 \
			   : ((N) - LP0_REGNUM + FP0_REGNUM)) * 4)


#undef FRAME_NUM_ARGS
#define FRAME_NUM_ARGS(numargs, fi) numargs = frame_num_args(fi)

#undef FRAME_CHAIN
#define FRAME_CHAIN(thisframe)  \
  (read_memory_integer ((thisframe)->frame, 4) > (thisframe)->frame ? \
   read_memory_integer ((thisframe)->frame, 4) : 0)

#undef FRAME_CHAIN_VALID
#define FRAME_CHAIN_VALID(chain, thisframe)	\
  ((chain) != 0					\
   && !inside_main_func ((thisframe) -> pc))
