/* Parameters for HP 9000 model 320 hosting, for GDB, the GNU debugger.
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

#define HOST_BYTE_ORDER BIG_ENDIAN

/* Define this to indicate problems with traps after continuing.  */
#define HP_OS_BUG

/* Set flag to indicate whether HP's assembler is in use. */
#ifdef __GNUC__
#ifdef __HPUX_ASM__
#define HPUX_ASM
#endif
#else /* not GNU C.  */
#define HPUX_ASM
#endif /* not GNU C.  */

/* Define this for versions of hp-ux older than 6.0 */
/* #define HPUX_VERSION_5 */

/* define USG if you are using sys5 /usr/include's */
#undef USG	/* In case it was defined in the Makefile for cplus-dem.c */
#define USG

#define HAVE_TERMIOS

#define REGISTER_ADDR(u_ar0, regno)					\
  (unsigned int)							\
  (((regno) < PS_REGNUM)						\
   ? (&((struct exception_stack *) (u_ar0))->e_regs[(regno + R0)])	\
   : (((regno) == PS_REGNUM)						\
      ? ((int *) (&((struct exception_stack *) (u_ar0))->e_PS))		\
      : (&((struct exception_stack *) (u_ar0))->e_PC)))

#define FP_REGISTER_ADDR(u, regno)					\
  (((char *)								\
    (((regno) < FPC_REGNUM)						\
     ? (&u.u_pcb.pcb_mc68881[FMC68881_R0 + (((regno) - FP0_REGNUM) * 3)]) \
     : (&u.u_pcb.pcb_mc68881[FMC68881_C + ((regno) - FPC_REGNUM)])))	\
   - ((char *) (& u)))

/* Interface definitions for kernel debugger KDB.  */

/* Map machine fault codes into signal numbers.
   First subtract 0, divide by 4, then index in a table.
   Faults for which the entry in this table is 0
   are not handled by KDB; the program's own trap handler
   gets to handle then.  */

#define FAULT_CODE_ORIGIN 0
#define FAULT_CODE_UNITS 4
#define FAULT_TABLE    \
{ 0, 0, 0, 0, SIGTRAP, 0, 0, 0, \
  0, SIGTRAP, 0, 0, 0, 0, 0, SIGKILL, \
  0, 0, 0, 0, 0, 0, 0, 0, \
  SIGILL }

#ifndef HPUX_ASM

/* Start running with a stack stretching from BEG to END.
   BEG and END should be symbols meaningful to the assembler.
   This is used only for kdb.  */

#define INIT_STACK(beg, end)  \
{ asm (".globl end");         \
  asm ("movel $ end, sp");      \
  asm ("clrl fp"); }

/* Push the frame pointer register on the stack.  */
#define PUSH_FRAME_PTR        \
  asm ("movel fp, -(sp)");

/* Copy the top-of-stack to the frame pointer register.  */
#define POP_FRAME_PTR  \
  asm ("movl (sp), fp");

/* After KDB is entered by a fault, push all registers
   that GDB thinks about (all NUM_REGS of them),
   so that they appear in order of ascending GDB register number.
   The fault code will be on the stack beyond the last register.  */

#define PUSH_REGISTERS        \
{ asm ("clrw -(sp)");	      \
  asm ("pea 10(sp)");	      \
  asm ("movem $ 0xfffe,-(sp)"); }

/* Assuming the registers (including processor status) have been
   pushed on the stack in order of ascending GDB register number,
   restore them and return to the address in the saved PC register.  */

#define POP_REGISTERS          \
{ asm ("subil $8,28(sp)");     \
  asm ("movem (sp),$ 0xffff"); \
  asm ("rte"); }

#else /* HPUX_ASM */

/* Start running with a stack stretching from BEG to END.
   BEG and END should be symbols meaningful to the assembler.
   This is used only for kdb.  */

#define INIT_STACK(beg, end)						\
{ asm ("global end");							\
  asm ("mov.l &end,%sp");						\
  asm ("clr.l %a6"); }

/* Push the frame pointer register on the stack.  */
#define PUSH_FRAME_PTR							\
  asm ("mov.l %fp,-(%sp)");

/* Copy the top-of-stack to the frame pointer register.  */
#define POP_FRAME_PTR							\
  asm ("mov.l (%sp),%fp");

/* After KDB is entered by a fault, push all registers
   that GDB thinks about (all NUM_REGS of them),
   so that they appear in order of ascending GDB register number.
   The fault code will be on the stack beyond the last register.  */

#define PUSH_REGISTERS							\
{ asm ("clr.w -(%sp)");							\
  asm ("pea 10(%sp)");							\
  asm ("movm.l &0xfffe,-(%sp)"); }

/* Assuming the registers (including processor status) have been
   pushed on the stack in order of ascending GDB register number,
   restore them and return to the address in the saved PC register.  */

#define POP_REGISTERS							\
{ asm ("subi.l &8,28(%sp)");						\
  asm ("mov.m (%sp),&0xffff");						\
  asm ("rte"); }

#endif /* HPUX_ASM */
