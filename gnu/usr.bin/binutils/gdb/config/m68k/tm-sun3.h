/* Parameters for execution on a Sun, for GDB, the GNU debugger.
   Copyright (C) 1986, 1987, 1989, 1992 Free Software Foundation, Inc.

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

#ifndef TM_SUN3_H
#define TM_SUN3_H

/* Sun3 status includes fpflags, which shows whether the FPU has been used
   by the process, and whether the FPU was done with an instruction or 
   was interrupted in the middle of a long instruction.  See
   <machine/reg.h>.  */
/*                      a&d, pc,sr, fp, fpstat, fpflags   */

#define REGISTER_BYTES (16*4 + 8 + 8*12 + 3*4 + 4)

#define NUM_REGS 31

#define REGISTER_BYTES_OK(b) \
     ((b) == REGISTER_BYTES \
      || (b) == REGISTER_BYTES_FP \
      || (b) == REGISTER_BYTES_NOFP)

/* If PC contains this instruction, then we know what we are in a system
   call stub, and the return PC is is at SP+4, instead of SP. */

#define SYSCALL_TRAP 0x4e40	/* trap #0 */
#define SYSCALL_TRAP_OFFSET 0	/* PC points at trap instruction */

#include "m68k/tm-m68k.h"

/* Disable alternate breakpoint mechanism needed by 68k stub. */
#undef REMOTE_BREAKPOINT

/* Offsets (in target ints) into jmp_buf.  Not defined by Sun, but at least
   documented in a comment in <machine/setjmp.h>! */

#define JB_ELEMENT_SIZE 4

#define JB_ONSSTACK 0
#define JB_SIGMASK 1
#define JB_SP 2
#define JB_PC 3
#define JB_PSL 4
#define JB_D2 5
#define JB_D3 6
#define JB_D4 7
#define JB_D5 8
#define JB_D6 9
#define JB_D7 10
#define JB_A2 11
#define JB_A3 12
#define JB_A4 13
#define JB_A5 14
#define JB_A6 15

/* Figure out where the longjmp will land.  Slurp the args out of the stack.
   We expect the first arg to be a pointer to the jmp_buf structure from which
   we extract the pc (JB_PC) that we will land at.  The pc is copied into ADDR.
   This routine returns true on success */

#define GET_LONGJMP_TARGET(ADDR) get_longjmp_target(ADDR)
extern int get_longjmp_target PARAMS ((CORE_ADDR *));

/* If sun3 pcc says that a parameter is a short, it's a short.  */
#define BELIEVE_PCC_PROMOTION_TYPE

/* Can't define BELIEVE_PCC_PROMOTION for SunOS /bin/cc of SunOS 4.1.1.
   Apparently Sun fixed this for the sparc but not the sun3.  */

/* The code which tries to deal with this bug is never harmful on a sun3.  */
#define SUN_FIXED_LBRAC_BUG (0)

/* On the sun3 the kernel pushes a sigcontext on the user stack and then
   `calls' _sigtramp in user code. _sigtramp saves the floating point status
   on the stack and calls the signal handler function. The stack does not
   contain enough information to allow a normal backtrace, but sigcontext
   contains the saved user pc/sp. FRAME_CHAIN and friends in tm-m68k.h and
   m68k_find_saved_regs deal with this situation by manufacturing a fake frame
   for _sigtramp.
   SIG_PC_FP_OFFSET is the offset from the signal handler frame to the
   saved pc in sigcontext.
   SIG_SP_FP_OFFSET is the offset from the signal handler frame to the end
   of sigcontext which is identical to the saved sp at SIG_PC_FP_OFFSET - 4.

   Please note that it is impossible to correctly backtrace from a breakpoint
   in _sigtramp as _sigtramp modifies the stack pointer a few times.  */

#undef SIG_PC_FP_OFFSET
#define SIG_PC_FP_OFFSET 324
#define SIG_SP_FP_OFFSET 332

#endif /* TM_SUN3_H */
