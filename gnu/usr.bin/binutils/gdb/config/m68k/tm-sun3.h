// OBSOLETE /* Parameters for execution on a Sun, for GDB, the GNU debugger.
// OBSOLETE    Copyright 1986, 1987, 1989, 1992, 1993, 1994, 1996, 2000
// OBSOLETE    Free Software Foundation, Inc.
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
// OBSOLETE #ifndef TM_SUN3_H
// OBSOLETE #define TM_SUN3_H
// OBSOLETE 
// OBSOLETE /* Sun3 status includes fpflags, which shows whether the FPU has been used
// OBSOLETE    by the process, and whether the FPU was done with an instruction or 
// OBSOLETE    was interrupted in the middle of a long instruction.  See
// OBSOLETE    <machine/reg.h>.  */
// OBSOLETE /*                      a&d, pc,sr, fp, fpstat, fpflags   */
// OBSOLETE 
// OBSOLETE #define DEPRECATED_REGISTER_BYTES (16*4 + 8 + 8*12 + 3*4 + 4)
// OBSOLETE 
// OBSOLETE #define NUM_REGS 31
// OBSOLETE 
// OBSOLETE #define REGISTER_BYTES_OK(b) \
// OBSOLETE      ((b) == DEPRECATED_REGISTER_BYTES \
// OBSOLETE       || (b) == REGISTER_BYTES_FP \
// OBSOLETE       || (b) == REGISTER_BYTES_NOFP)
// OBSOLETE 
// OBSOLETE /* If PC contains this instruction, then we know what we are in a system
// OBSOLETE    call stub, and the return PC is is at SP+4, instead of SP. */
// OBSOLETE 
// OBSOLETE #define SYSCALL_TRAP 0x4e40	/* trap #0 */
// OBSOLETE #define SYSCALL_TRAP_OFFSET 0	/* PC points at trap instruction */
// OBSOLETE 
// OBSOLETE #include "m68k/tm-m68k.h"
// OBSOLETE 
// OBSOLETE /* Disable alternate breakpoint mechanism needed by 68k stub. */
// OBSOLETE #undef DEPRECATED_REMOTE_BREAKPOINT
// OBSOLETE 
// OBSOLETE /* Offsets (in target ints) into jmp_buf.  Not defined by Sun, but at least
// OBSOLETE    documented in a comment in <machine/setjmp.h>! */
// OBSOLETE 
// OBSOLETE #define JB_ELEMENT_SIZE 4
// OBSOLETE 
// OBSOLETE #define JB_ONSSTACK 0
// OBSOLETE #define JB_SIGMASK 1
// OBSOLETE #define JB_SP 2
// OBSOLETE #define JB_PC 3
// OBSOLETE #define JB_PSL 4
// OBSOLETE #define JB_D2 5
// OBSOLETE #define JB_D3 6
// OBSOLETE #define JB_D4 7
// OBSOLETE #define JB_D5 8
// OBSOLETE #define JB_D6 9
// OBSOLETE #define JB_D7 10
// OBSOLETE #define JB_A2 11
// OBSOLETE #define JB_A3 12
// OBSOLETE #define JB_A4 13
// OBSOLETE #define JB_A5 14
// OBSOLETE #define JB_A6 15
// OBSOLETE 
// OBSOLETE /* Figure out where the longjmp will land.  Slurp the args out of the stack.
// OBSOLETE    We expect the first arg to be a pointer to the jmp_buf structure from which
// OBSOLETE    we extract the pc (JB_PC) that we will land at.  The pc is copied into ADDR.
// OBSOLETE    This routine returns true on success */
// OBSOLETE 
// OBSOLETE #define GET_LONGJMP_TARGET(ADDR) m68k_get_longjmp_target(ADDR)
// OBSOLETE 
// OBSOLETE /* If sun3 pcc says that a parameter is a short, it's a short.  */
// OBSOLETE #define BELIEVE_PCC_PROMOTION_TYPE 1
// OBSOLETE 
// OBSOLETE /* Can't define BELIEVE_PCC_PROMOTION for SunOS /bin/cc of SunOS 4.1.1.
// OBSOLETE    Apparently Sun fixed this for the sparc but not the sun3.  */
// OBSOLETE 
// OBSOLETE /* The code which tries to deal with this bug is never harmful on a sun3.  */
// OBSOLETE #define SUN_FIXED_LBRAC_BUG (0)
// OBSOLETE 
// OBSOLETE #endif /* TM_SUN3_H */
