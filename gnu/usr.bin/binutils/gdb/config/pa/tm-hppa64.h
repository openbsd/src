/* Parameters for execution on any Hewlett-Packard PA-RISC machine.
   Copyright 1986, 1987, 1989, 1990, 1991, 1992, 1993, 1995, 1999, 2000
   Free Software Foundation, Inc.

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

/* PA 64-bit specific definitions.  Override those which are in
   tm-hppa.h */

struct frame_info;

/* jimb: this must go.  I'm just using it to disable code I haven't
   gotten working yet.  */
#define GDB_TARGET_IS_HPPA_20W

/* NOTE: cagney/2003-07-27: Using CC='cc +DA2.0W -Ae' configure
   hppa64-hp-hpux11.00; GDB managed to build / start / break main /
   run with multi-arch enabled.  Not sure about much else as there
   appears to be an unrelated problem in the SOM symbol table reader
   causing GDB to lose line number information.  Since prior to this
   switch and a other recent tweaks, 64 bit PA hadn't been building
   for some months, this is probably the lesser of several evils.  */

#include "pa/tm-hppah.h"

#undef FP4_REGNUM
#define FP4_REGNUM 68
#define AP_REGNUM 29  /* Argument Pointer Register */
#define DP_REGNUM 27
#define FP5_REGNUM 70
#define SR5_REGNUM 48


/* For a number of horrible reasons we may have to adjust the location
   of variables on the stack.  Ugh.  jimb: why? */
#define HPREAD_ADJUST_STACK_ADDRESS(ADDR) hpread_adjust_stack_address(ADDR)

extern int hpread_adjust_stack_address (CORE_ADDR);


/* jimb: omitted dynamic linking stuff here */

/* This sequence of words is the instructions

; Call stack frame has already been built by gdb. Since we could be calling
; a varargs function, and we do not have the benefit of a stub to put things in
; the right place, we load the first 8 word of arguments into both the general
; and fp registers.
call_dummy
	nop
        copy %r4,%r29
        copy %r5,%r22
        copy %r6,%r27
        fldd -64(0,%r29),%fr4
        fldd -56(0,%r29),%fr5
        fldd -48(0,%r29),%fr6
        fldd -40(0,%r29),%fr7
        fldd -32(0,%r29),%fr8
        fldd -24(0,%r29),%fr9
        fldd -16(0,%r29),%fr10
        fldd -8(0,%r29),%fr11
        copy %r22,%r1
        ldd -64(%r29), %r26
        ldd -56(%r29), %r25
        ldd -48(%r29), %r24
        ldd -40(%r29), %r23
        ldd -32(%r29), %r22
        ldd -24(%r29), %r21
        ldd -16(%r29), %r20
        bve,l (%r1),%r2
        ldd -8(%r29), %r19
        break 4, 8
	mtsp %r21, %sr0
	ble 0(%sr0, %r22)
        nop
*/

/* Call dummys are sized and written out in word sized hunks.  So we have
   to pack the instructions into words.  Ugh.  */
#undef CALL_DUMMY
#define CALL_DUMMY {0x08000240349d0000LL, 0x34b6000034db0000LL, \
                    0x53a43f8353a53f93LL, 0x53a63fa353a73fb3LL,\
                    0x53a83fc353a93fd3LL, 0x2fa1100a2fb1100bLL,\
                    0x36c1000053ba3f81LL, 0x53b93f9153b83fa1LL,\
                    0x53b73fb153b63fc1LL, 0x53b53fd10fa110d4LL,\
                    0xe820f0000fb110d3LL, 0x0001000400151820LL,\
                    0xe6c0000008000240LL}

/* The PA64 ABI reserves 64 bytes of stack space for outgoing register
   parameters.  */
#undef REG_PARM_STACK_SPACE
#define REG_PARM_STACK_SPACE 64

/* Use the 64-bit calling conventions designed for the PA2.0 in wide mode.  */
#define PA20W_CALLING_CONVENTIONS

#undef FUNC_LDIL_OFFSET
#undef FUNC_LDO_OFFSET
#undef SR4EXPORT_LDIL_OFFSET
#undef SR4EXPORT_LDO_OFFSET

#undef FRAME_SAVED_PC_IN_SIGTRAMP
extern void hppa64_hpux_frame_saved_pc_in_sigtramp (struct frame_info *fi,
                                                    CORE_ADDR *tmp);
#define FRAME_SAVED_PC_IN_SIGTRAMP(FRAME, TMP) \
  hppa64_hpux_frame_saved_pc_in_sigtramp (FRAME, TMP)

#undef FRAME_BASE_BEFORE_SIGTRAMP
extern void hppa64_hpux_frame_base_before_sigtramp (struct frame_info *fi,
                                                    CORE_ADDR *tmp);
#define FRAME_BASE_BEFORE_SIGTRAMP(FRAME, TMP) \
  hppa64_hpux_frame_base_before_sigtramp (FRAME, TMP)

#undef FRAME_FIND_SAVED_REGS_IN_SIGTRAMP
extern void hppa64_hpux_frame_find_saved_regs_in_sigtramp
              (struct frame_info *fi, CORE_ADDR *fsr);
#define FRAME_FIND_SAVED_REGS_IN_SIGTRAMP(FRAME, FSR) \
  hppa64_hpux_frame_find_saved_regs_in_sigtramp (FRAME, FSR)

/* jimb: omitted purify call support */
