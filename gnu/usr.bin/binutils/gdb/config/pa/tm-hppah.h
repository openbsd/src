/* Parameters for execution on an HP PA-RISC machine, running HPUX, for GDB.
   Copyright 1991, 1992 Free Software Foundation, Inc. 

   Contributed by the Center for Software Science at the
   University of Utah (pa-gdb-bugs@cs.utah.edu).

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


/* Actually, for a PA running HPUX the kernel calls the signal handler
   without an intermediate trampoline.  Luckily the kernel always sets
   the return pointer for the signal handler to point to _sigreturn.  */
#define IN_SIGTRAMP(pc, name) (name && STREQ ("_sigreturn", name))

/* For HPUX:

   The signal context structure pointer is always saved at the base
   of the frame which "calls" the signal handler.  We only want to find
   the hardware save state structure, which lives 10 32bit words into
   sigcontext structure.

   Within the hardware save state structure, registers are found in the
   same order as the register numbers in GDB.

   At one time we peeked at %r31 rather than the PC queues to determine
   what instruction took the fault.  This was done on purpose, but I don't
   remember why.  Looking at the PC queues is really the right way, and
   I don't remember why that didn't work when this code was originally
   written.  */

#define FRAME_SAVED_PC_IN_SIGTRAMP(FRAME, TMP) \
{ \
  *(TMP) = read_memory_integer ((FRAME)->frame + (43 * 4) , 4); \
}

#define FRAME_BASE_BEFORE_SIGTRAMP(FRAME, TMP) \
{ \
  *(TMP) = read_memory_integer ((FRAME)->frame + (40 * 4), 4); \
}

#define FRAME_FIND_SAVED_REGS_IN_SIGTRAMP(FRAME, FSR) \
{ \
  int i; \
  CORE_ADDR TMP; \
  TMP = (FRAME)->frame + (10 * 4); \
  for (i = 0; i < NUM_REGS; i++) \
    { \
      if (i == SP_REGNUM) \
	(FSR)->regs[SP_REGNUM] = read_memory_integer (TMP + SP_REGNUM * 4, 4); \
      else \
	(FSR)->regs[i] = TMP + i * 4; \
    } \
}

/* Mostly it's common to all HPPA's.  */
#include "pa/tm-hppa.h"
