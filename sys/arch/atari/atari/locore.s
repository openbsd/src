/*	$NetBSD: locore.s,v 1.14 1995/12/18 20:40:59 leo Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1980, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: locore.s 1.58 91/04/22$
 *
 *	@(#)locore.s	7.11 (Berkeley) 5/9/91
 */

/*
 *
 * Original (hp300) Author: unknown, maybe Mike Hibler?
 * Amiga author: Markus Wild
 * Atari Modifications: Leo Weppelman
 */

#include "assym.s"
#include <atari/atari/vectors.s>

	.text
_C_LABEL(kernel_text):

/*
 * This is where we wind up if the kernel jumps to location 0.
 * (i.e. a bogus PC)  This is known to immediately follow the vector
 * table and is hence at 0x400 (see reset vector in vectors.s).
 */
	.globl	_panic
	pea	Ljmp0panic
	jbsr	_panic
	/* NOTREACHED */
Ljmp0panic:
	.asciz	"kernel jump to zero"
	.even

/*
 * Do a dump.
 * Called by auto-restart.
 */
	.globl	_dumpsys
	.globl	_doadump
_doadump:
	jbsr	_dumpsys
	jbsr	_doboot
	/*NOTREACHED*/

/*
 * Trap/interrupt vector routines
 */ 

	.globl	_trap, _nofault, _longjmp
_buserr:
	tstl	_nofault	|  device probe?
	jeq	_addrerr	|  no, handle as usual
	movl	_nofault,sp@-	|  yes,
	jbsr	_longjmp	|   longjmp(nofault)
_addrerr:
	clrl	sp@-		|  stack adjust count
	moveml	#0xFFFF,sp@-	|  save user registers
	movl	usp,a0		|  save the user SP
	movl	a0,sp@(FR_SP)	|    in the savearea
	lea	sp@(FR_HW),a1	|  grab base of HW berr frame
	tstl	_cpu040
	jeq	Lbe030
	movl	a1@(8),sp@-	|  V = exception address
	clrl	sp@-		|  dummy code
	moveq	#0,d0
	movw	a1@(6),d0	|  get vector offset
	andw	#0x0fff,d0
	cmpw	#12,d0		|  is it address error
	jeq	Lisaerr
	movl	a1@(20),d1	|  get fault address
	moveq	#0,d0
	movw	a1@(12),d0	|  get SSW
	btst	#11,d0		|  check for mis-aligned
	jeq	Lbe1stpg	|  no skip
	addl	#3,d1		|  get into next page
	andl	#PG_FRAME,d1	|  and truncate
Lbe1stpg:
	movl	d1,sp@(4)	|  pass fault address.
	movl	d0,sp@		|  pass SSW as code
	btst	#10,d0		|  test ATC
	jeq	Lisberr		| it's a bus error
	jra	Lismerr
Lbe030:
	moveq	#0,d0
	movw	a1@(10),d0	|  grab SSW for fault processing
	btst	#12,d0		|  RB set?
	jeq	LbeX0		|  no, test RC
	bset	#14,d0		|  yes, must set FB
	movw	d0,a1@(10)	|  for hardware too
LbeX0:
	btst	#13,d0		|  RC set?
	jeq	LbeX1		|  no, skip
	bset	#15,d0		|  yes, must set FC
	movw	d0,a1@(10)	|  for hardware too
LbeX1:
	btst	#8,d0		|  data fault?
	jeq	Lbe0		|  no, check for hard cases
	movl	a1@(16),d1	|  fault address is as given in frame
	jra	Lbe10		|  thats it
Lbe0:
	btst	#4,a1@(6)	|  long (type B) stack frame?
	jne	Lbe4		|  yes, go handle
	movl	a1@(2),d1	|  no, can use save PC
	btst	#14,d0		|  FB set?
	jeq	Lbe3		|  no, try FC
	addql	#4,d1		|  yes, adjust address
	jra	Lbe10		|  done
Lbe3:
	btst	#15,d0		|  FC set?
	jeq	Lbe10		|  no, done
	addql	#2,d1		|  yes, adjust address
	jra	Lbe10		|  done
Lbe4:
	movl	a1@(36),d1	|  long format, use stage B address
	btst	#15,d0		|  FC set?
	jeq	Lbe10		|  no, all done
	subql	#2,d1		|  yes, adjust address
Lbe10:
	movl	d1,sp@-		|  push fault VA
	movl	d0,sp@-		|  and padded SSW
	movw	a1@(6),d0	|  get frame format/vector offset
	andw	#0x0FFF,d0	|  clear out frame format
	cmpw	#12,d0		|  address error vector?
	jeq	Lisaerr		|  yes, go to it
	movl	d1,a0		|  fault address
	ptestr	#1,a0@,#7	|  do a table search
	pmove	psr,sp@		|  save result
	btst	#7,sp@		|  bus error bit set?
	jeq	Lismerr		|  no, must be MMU fault
	clrw	sp@		|  yes, re-clear pad word
	jra	Lisberr		|  and process as normal bus error
Lismerr:
	movl	#T_MMUFLT,sp@-	|  show that we are an MMU fault
	jra	Ltrapnstkadj	|  and deal with it
Lisaerr:
	movl	#T_ADDRERR,sp@-	|  mark address error
	jra	Ltrapnstkadj	|  and deal with it
Lisberr:
	movl	#T_BUSERR,sp@-	|  mark bus error
Ltrapnstkadj:
	jbsr	_trap		|  handle the error
	lea	sp@(12),sp	|  pop value args
	movl	sp@(FR_SP),a0	|  restore user SP
	movl	a0,usp		|    from save area
	movw	sp@(FR_ADJ),d0	|  need to adjust stack?
	jne	Lstkadj		|  yes, go to it
	moveml	sp@+,#0x7FFF	|  no, restore most user regs
	addql	#8,sp		|  toss SSP and stkadj
	jra	rei		|  all done
Lstkadj:
	lea	sp@(FR_HW),a1	|  pointer to HW frame
	addql	#8,a1		|  source pointer
	movl	a1,a0		|  source
	addw	d0,a0		|   + hole size = dest pointer
	movl	a1@-,a0@-	|  copy
	movl	a1@-,a0@-	|   8 bytes
	movl	a0,sp@(FR_SP)	|  new SSP
	moveml	sp@+,#0x7FFF	|  restore user registers
	movl	sp@,sp		|  and our SP
	jra	rei		|  all done

/*
 * FP exceptions.
 */
_fpfline:
	tstl	_cpu040		|  an 040 FPU
	jeq	fpfline_not40	|  no, do 6888? emulation
	cmpw	#0x202c,sp@(6)	|  format type 2?
	jne	_illinst	|  no, not an FP emulation
#ifdef FPSP
	.globl fpsp_unimp
	jmp	fpsp_unimp	|  yes, go handle it
#endif
fpfline_not40:
	clrl	sp@-		|  stack adjust count
	moveml	#0xFFFF,sp@-	|  save registers
	moveq	#T_FPEMULI,d0	|  denote as FP emulation trap
	jra	fault		|  do it

_fpunsupp:
	tstl	_cpu040		|  an 040 FPU?
	jeq	fpunsupp_not40
#ifdef FPSP
	.globl	fpsp_unsupp
	jmp	fpsp_unsupp	|  yes, go handle it
#endif
fpunsupp_not40:
	clrl	sp@-		|  stack adjust count
	moveml	#0xFFFF,sp@-	|  save registers
	moveq	#T_FPEMULD,d0	|  denote as FP emulation trap
	jra	fault		|  do it

/*
 * Handles all other FP coprocessor exceptions.
 * Note that since some FP exceptions generate mid-instruction frames
 * and may cause signal delivery, we need to test for stack adjustment
 * after the trap call.
 */
	.globl	_fpfault
_fpfault:
	clrl	sp@-		|  stack adjust count
	moveml	#0xFFFF,sp@-	|  save user registers
	movl	usp,a0		|  and save
	movl	a0,sp@(FR_SP)	|    the user stack pointer
	clrl	sp@-		|  no VA arg
	movl	_curpcb,a0	|  current pcb
	lea	a0@(PCB_FPCTX),a0 |  address of FP savearea
	fsave	a0@		|  save state
	tstb	a0@		|  null state frame?
	jeq	Lfptnull	|  yes, safe
	clrw	d0		|  no, need to tweak BIU
	movb	a0@(1),d0	|  get frame size
	bset	#3,a0@(0,d0:w)	|  set exc_pend bit of BIU
Lfptnull:
	fmovem	fpsr,sp@-	|  push fpsr as code argument
	frestore a0@		|  restore state
	movl	#T_FPERR,sp@-	|  push type arg
	jra	Ltrapnstkadj	|  call trap and deal with stack cleanup

/*
 * Coprocessor and format errors can generate mid-instruction stack
 * frames and cause signal delivery hence we need to check for potential
 * stack adjustment.
 */
_coperr:
	clrl	sp@-			|  stack adjust count
	moveml	#0xFFFF,sp@-
	movl	usp,a0			|  get and save
	movl	a0,sp@(FR_SP)		|    the user stack pointer
	clrl	sp@-			|  no VA arg
	clrl	sp@-			|  or code arg
	movl	#T_COPERR,sp@-		|  push trap type
	jra	Ltrapnstkadj		|  call trap and deal with stack adjust

_fmterr:
	clrl	sp@-			|  stack adjust count
	moveml	#0xFFFF,sp@-
	movl	usp,a0			|  get and save
	movl	a0,sp@(FR_SP)		|    the user stack pointer
	clrl	sp@-			|  no VA arg
	clrl	sp@-			|  or code arg
	movl	#T_FMTERR,sp@-		|  push trap type
	jra	Ltrapnstkadj		|  call trap and deal with stack adjust

/*
 * Other exceptions only cause four and six word stack frame and require
 * no post-trap stack adjustment.
 */
_illinst:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_ILLINST,d0
	jra	fault

_zerodiv:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_ZERODIV,d0
	jra	fault

_chkinst:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_CHKINST,d0
	jra	fault

_trapvinst:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_TRAPVINST,d0
	jra	fault

_privinst:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	moveq	#T_PRIVINST,d0
	jra	fault

	.globl	fault
fault:
	movl	usp,a0			|  get and save
	movl	a0,sp@(FR_SP)		|    the user stack pointer
	clrl	sp@-			|  no VA arg
	clrl	sp@-			|  or code arg
	movl	d0,sp@-			|  push trap type
	jbsr	_trap			|  handle trap
	lea	sp@(12),sp		|  pop value args
	movl	sp@(FR_SP),a0		|  restore
	movl	a0,usp			|    user SP
	moveml	sp@+,#0x7FFF		|  restore most user regs
	addql	#8,sp			|  pop SP and stack adjust
	jra	rei			|  all done

	.globl	_straytrap

_lev4intr:				| HBL & VBL interrupts can not
_lev2intr:				|  be turned off on a Falcon, so
	rte				|  just ignore them.

_lev3intr:
_lev5intr:
_lev6intr:
_badtrap:
	moveml	#0xC0C0,sp@-		|  save scratch regs
	movw	sp@(22),sp@-		|  push exception vector info
	clrw	sp@-
	movl	sp@(22),sp@-		|  and PC
	jbsr	_straytrap		|  report
	addql	#8,sp			|  pop args
	moveml	sp@+,#0x0303		|  restore regs
	jra	rei			|  all done

	.globl	_straymfpint
_badmfpint:
	moveml	#0xC0C0,sp@-		|  save scratch regs
	movw	sp@(22),sp@-		|  push exception vector info
	clrw	sp@-
	movl	sp@(22),sp@-		|  and PC
	jbsr	_straymfpint		|  report
	addql	#8,sp			|  pop args
	moveml	sp@+,#0x0303		|  restore regs
	jra	rei			|  all done

	.globl	_syscall
_trap0:
	clrl	sp@-			|  stack adjust count
	moveml	#0xFFFF,sp@-		|  save user registers
	movl	usp,a0			|  save the user SP
	movl	a0,sp@(FR_SP)		|    in the savearea
	movl	d0,sp@-			|  push syscall number
	jbsr	_syscall		|  handle it
	addql	#4,sp			|  pop syscall arg
	movl	sp@(FR_SP),a0		|  grab and restore
	movl	a0,usp			|    user SP
	moveml	sp@+,#0x7FFF		|  restore most registers
	addql	#8,sp			|  pop SP and stack adjust
	jra	rei			|  all done

/*
 * Our native 4.3 implementation uses trap 1 as sigreturn() and trap 2
 * as a breakpoint trap.
 */
_trap1:
	jra	sigreturn

_trap2:
	jra	_trace

/*
 * Trap 12 is the entry point for the cachectl "syscall"
 *	cachectl(command, addr, length)
 * command in d0, addr in a1, length in d1
 */
	.globl	_cachectl
_trap12:
	movl	d1,sp@-			|  push length
	movl	a1,sp@-			|  push addr
	movl	d0,sp@-			|  push command
	jbsr	_cachectl		|  do it
	lea	sp@(12),sp		|  pop args
	jra	rei			|  all done

/*
 * Trap 15 is used for:
 *	- KGDB traps
 *	- trace traps for SUN binaries (not fully supported yet)
 * We just pass it on and let trap() sort it all out
 */
_trap15:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
#ifdef KGDB
	moveq	#T_TRAP15,d0
	movw	sp@(FR_HW),d1		|  get PSW
	andw	#PSL_S,d1		|  from user mode?
	jeq	fault			|  yes, just a regular fault
	movl	d0,sp@-
	.globl	_kgdb_trap_glue
	jbsr	_kgdb_trap_glue		|  returns if no debugger
	addl	#4,sp
#endif
	moveq	#T_TRAP15,d0
	jra	fault

/*
 * Hit a breakpoint (trap 1 or 2) instruction.
 * Push the code and treat as a normal fault.
 */
_trace:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
#ifdef KGDB
	moveq	#T_TRACE,d0
	movw	sp@(FR_HW),d1		|  get SSW
	andw	#PSL_S,d1		|  from user mode?
	jeq	fault			|  no, regular fault
	movl	d0,sp@-
	jbsr	_kgdb_trap_glue		|  returns if no debugger
	addl	#4,sp
#endif
	moveq	#T_TRACE,d0
	jra	fault

/*
 * The sigreturn() syscall comes here.  It requires special handling
 * because we must open a hole in the stack to fill in the (possibly much
 * larger) original stack frame.
 */
sigreturn:
	lea	sp@(-84),sp		|  leave enough space for largest frame
	movl	sp@(84),sp@		|  move up current 8 byte frame
	movl	sp@(88),sp@(4)
	movl	#84,sp@-		|  default: adjust by 84 bytes
	moveml	#0xFFFF,sp@-		|  save user registers 
	movl	usp,a0			|  save the user SP
	movl	a0,sp@(FR_SP)		|    in the savearea
	movl	#SYS_sigreturn,sp@-	|  push syscall number
	jbsr	_syscall		|  handle it
	addql	#4,sp			|  pop syscall#
	movl	sp@(FR_SP),a0		|  grab and restore
	movl	a0,usp			|    user SP
	lea	sp@(FR_HW),a1		|  pointer to HW frame
	movw	sp@(FR_ADJ),d0		|  do we need to adjust the stack?
	jeq	Lsigr1			|  no, just continue
	moveq	#92,d1			|  total size
	subw	d0,d1			|   - hole size = frame size
	lea	a1@(92),a0		|  destination
	addw	d1,a1			|  source
	lsrw	#1,d1			|  convert to word count
	subqw	#1,d1			|  minus 1 for dbf
Lsigrlp:
	movw	a1@-,a0@-		|  copy a word
	dbf	d1,Lsigrlp		|  continue
	movl	a0,a1			|  new HW frame base
Lsigr1:
	movl	a1,sp@(FR_SP)		|  new SP value
	moveml	sp@+,#0x7FFF		|  restore user registers
	movl	sp@,sp			|  and our SP
	jra	rei			|  all done

/*
 * Interrupt handlers.
 *
 *	Level 0:	Spurious: ignored.
 *	Level 1:	softint
 *	Level 2:	HBL
 *	Level 3:	not used
 *	Level 4:	not used
 *	Level 5:	SCC (not used)
 *	Level 6:	MFP1/MFP2 (not used -> autovectored)
 *	Level 7:	Non-maskable: shouldn't be possible. ignore.
 */

/* Provide a generic interrupt dispatcher, only handle hardclock (int6)
 * specially, to improve performance
 */

	.globl	_hardclock

_spurintr:
	addql	#1,_intrcnt+0
	addql	#1,_cnt+V_INTR
	jra	rei

	/* MFP timer A handler --- System clock --- */
mfp_tima:
	moveml	d0-d1/a0-a1,sp@-	|  save scratch registers
	lea	sp@(16),a1		|  get pointer to PS
	movl	a1,sp@-			|  push pointer to PS, PC
	jbsr	_hardclock		|  call generic clock int routine
	addql	#4,sp			|  pop params
	addql	#1,_intrcnt+4		|  add another system clock interrupt
	moveml	sp@+,d0-d1/a0-a1	|  restore scratch regs	
	addql	#1,_cnt+V_INTR		|  chalk up another interrupt
	jra	rei			|  all done

#ifdef STATCLOCK
	/* MFP timer C handler --- Stat/Prof clock --- */
mfp_timc:
	moveml	d0-d1/a0-a1,sp@-	|  save scratch registers
	lea	sp@(16),a1		|  get pointer to PS
	movl	a1,sp@-			|  push pointer to PS, PC
	jbsr	_statintr		|  call statistics clock handler
	addql	#4,sp			|  pop params
	addql	#1,_intrcnt+36		|  add another stat clock interrupt
	moveml	sp@+,d0-d1/a0-a1	|  restore scratch regs	
	addql	#1,_cnt+V_INTR		|  chalk up another interrupt
	jra	rei			|  all done
#endif /* STATCLOCK */

	/* MFP ACIA handler --- keyboard/midi --- */
mfp_kbd:
	addql	#1,_intrcnt+8		|  add another kbd/mouse interrupt

	moveml	d0-d1/a0-a1,sp@-	|  Save scratch registers
	movw	sp@(16),sp@-		|  push previous SR value
	clrw	sp@-			|     padded to longword
	jbsr	_kbdintr		|  handle interrupt
	addql	#4,sp			|  pop SR
	moveml	sp@+,d0-d1/a0-a1
	addql	#1,_cnt+V_INTR		|  chalk up another interrupt
	jra	rei

	/* MFP2 SCSI DMA handler --- NCR5380 --- */
mfp2_5380dm:
	addql	#1,_intrcnt+24		|  add another 5380-DMA interrupt

	moveml	d0-d1/a0-a1,sp@-	|  Save scratch registers
	movw	sp@(16),sp@-		|  push previous SR value
	clrw	sp@-			|     padded to longword
	jbsr	_scsi_dma		|  handle interrupt
	addql	#4,sp			|  pop SR
	moveml	sp@+,d0-d1/a0-a1
	addql	#1,_cnt+V_INTR		|  chalk up another interrupt
	jra	rei

	/* MFP2 SCSI handler --- NCR5380 --- */
mfp2_5380:
	addql	#1,_intrcnt+20		|  add another 5380-SCSI interrupt

	moveml	d0-d1/a0-a1,sp@-	|  Save scratch registers
	movw	sp@(16),sp@-		|  push previous SR value
	clrw	sp@-			|     padded to longword
	jbsr	_scsi_ctrl		|  handle interrupt
	addql	#4,sp			|  pop SR
	moveml	sp@+,d0-d1/a0-a1
	addql	#1,_cnt+V_INTR		|  chalk up another interrupt
	jra	rei

	/* MFP DMA handler --- fdc/acsi --- */
mfp_fd_acsi:
	addql	#1,_intrcnt+12		|  add another fdc/acsi interrupt

	moveml	d0-d1/a0-a1,sp@-	|  Save scratch registers
	movw	sp@(16),sp@-		|  push previous SR value
	clrw	sp@-			|     padded to longword
	jbsr	_cdmaint		|  handle interrupt
	addql	#4,sp			|  pop SR
	moveml	sp@+,d0-d1/a0-a1
	addql	#1,_cnt+V_INTR		|  chalk up another interrupt
	jra	rei

	/* SCC Interrupt --- modem2/serial2 --- */
sccint:
	addql	#1,_intrcnt+32		|  add another SCC interrupt

	moveml	d0-d1/a0-a1,sp@-	|  Save scratch registers
	movw	sp@(16),sp@-		|  push previous SR value
	clrw	sp@-			|     padded to longword
	jbsr	_zshard			|  handle interrupt
	addql	#4,sp			|  pop SR
	moveml	sp@+,d0-d1/a0-a1
	addql	#1,_cnt+V_INTR		|  chalk up another interrupt
	jra	rei

	/* Level 1 (Software) interrupt handler */
_lev1intr:
	moveb	#0, SOFTINT_ADDR	|  Turn off software interrupt
	moveml	d0-d1/a0-a1,sp@-
	addql	#1,_intrcnt+16		|  add another software interrupt
	jbsr	_softint		|  handle software interrupts
	moveml	sp@+,d0-d1/a0-a1
	addql	#1,_cnt+V_INTR		|  chalk up another interrupt
	jra	rei

	/*
	 * Should never occur, except when special hardware modification
	 * is installed. In this case, one expects to be dropped into
	 * the debugger.
	 */
_lev7intr:
#ifdef DDB
	/*
	 * Note that the nmi has to be turned off while handling it because
	 * the hardware modification has no de-bouncing logic....
	 */
	movb	SYSMASK_ADDR, sp@-	|  save current sysmask
	movb	#0, SYSMASK_ADDR	|  disable all interrupts
	trap	#15			|  drop into the debugger
	movb	sp@+, SYSMASK_ADDR	|  restore sysmask
#endif
	addql	#1,_intrcnt+28		|  add another nmi interrupt
	rte				|  all done


/*
 * Emulation of VAX REI instruction.
 *
 * This code deals with checking for and servicing ASTs
 * (profiling, scheduling) and software interrupts (network, softclock).
 * We check for ASTs first, just like the VAX.  To avoid excess overhead
 * the T_ASTFLT handling code will also check for software interrupts so we
 * do not have to do it here.  After identifing that we need an AST we
 * drop the IPL to allow device interrupts.
 *
 * This code is complicated by the fact that sendsig may have been called
 * necessitating a stack cleanup.  A cleanup should only be needed at this
 * point for coprocessor mid-instruction frames (type 9), but we also test
 * for bus error frames (type 10 and 11).
 */
	.comm	_ssir,1
	.globl	_astpending
	.globl	rei
rei:
#ifdef DEBUG
	tstl	_panicstr		|  have we paniced?
	jne	Ldorte			|  yes, do not make matters worse
#endif
	tstl	_astpending		|  AST pending?
	jeq	Lchksir			|  no, go check for SIR
Lrei1:
	btst	#5,sp@			|  yes, are we returning to user mode?
	jne	Lchksir			|  no, go check for SIR
	movw	#PSL_LOWIPL,sr		|  lower SPL
	clrl	sp@-			|  stack adjust
	moveml	#0xFFFF,sp@-		|  save all registers
	movl	usp,a1			|  including
	movl	a1,sp@(FR_SP)		|     the users SP
	clrl	sp@-			|  VA == none
	clrl	sp@-			|  code == none
	movl	#T_ASTFLT,sp@-		|  type == async system trap
	jbsr	_trap			|  go handle it	
	lea	sp@(12),sp		|  pop value args
	movl	sp@(FR_SP),a0		|  restore user SP
	movl	a0,usp			|    from save area
	movw	sp@(FR_ADJ),d0		|  need to adjust stack?
	jne	Laststkadj		|  yes, go to it
	moveml	sp@+,#0x7FFF		|  no, restore most user regs
	addql	#8,sp			|  toss SP and stack adjust
	rte				|  and do real RTE
Laststkadj:
	lea	sp@(FR_HW),a1		|  pointer to HW frame
	addql	#8,a1			|  source pointer
	movl	a1,a0			|  source
	addw	d0,a0			|   + hole size = dest pointer
	movl	a1@-,a0@-		|  copy
	movl	a1@-,a0@-		|   8 bytes
	movl	a0,sp@(FR_SP)		|  new SSP
	moveml	sp@+,#0x7FFF		|  restore user registers
	movl	sp@,sp			|  and our SP
	rte				|  and do real RTE
Lchksir:
	tstb	_ssir			|  SIR pending?
	jeq	Ldorte			|  no, all done
	movl	d0,sp@-			|  need a scratch register
	movw	sp@(4),d0		|  get SR
	andw	#PSL_IPL7,d0		|  mask all but IPL
	jne	Lnosir			|  came from interrupt, no can do
	movl	sp@+,d0			|  restore scratch register
Lgotsir:
	movw	#SPL1,sr		|  prevent others from servicing int
	tstb	_ssir			|  too late?
	jeq	Ldorte			|  yes, oh well...
	clrl	sp@-			|  stack adjust
	moveml	#0xFFFF,sp@-		|  save all registers
	movl	usp,a1			|  including
	movl	a1,sp@(FR_SP)		|     the users SP
	clrl	sp@-			|  VA == none
	clrl	sp@-			|  code == none
	movl	#T_SSIR,sp@-		|  type == software interrupt
	jbsr	_trap			|  go handle it
	lea	sp@(12),sp		|  pop value args
	movl	sp@(FR_SP),a0		|  restore	
	movl	a0,usp			|    user SP
	moveml	sp@+,#0x7FFF		|  and all remaining registers
	addql	#8,sp			|  pop SP and stack adjust	
	rte
Lnosir:
	movl	sp@+,d0			|  restore scratch register
Ldorte:
	rte				|  real return	

	.data
_esym:	.long	0
	.globl	_esym


/*
 * Initialization
 *
 * A5 contains physical load point from boot
 * exceptions vector thru our table, that's bad.. just hope nothing exceptional
 * happens till we had time to initialize ourselves..
 */
	.comm	_lowram,4

	.text
	.globl	_edata
	.globl	_etext,_end
	.globl	start
	.globl	_first_8_bytes
_first_8_bytes:				| Save first 8 bytes here
	.long	0
	.long	0

	.word	0x0002			|  XXX: loadbsd version required
					|  2: needs a4 = esym
					|  XXX should be a symbol?
start:
	movw	#PSL_HIGHIPL,sr		| No interrupts

	/*
	 * a0 = start of loaded kernel
	 * a1 = value of esym
	 * d0 = fastmem size
	 * d1 = stmem size
	 * d2 = cputype
	 * d3 = boothowto
	 * d4 = size of loaded kernel
	 */
	movl	a0,	a6		| Save load address in a6
	movl	#8, a5			| Addresses 0-8 are mapped to ROM on the
	addql	#8, a0			|  atari ST. We cannot set these.
	subl	#8, d4

	/*
	 * Copy until end of kernel relocation code.
	 */
Lstart0:
	movl	a0@+,a5@+
	subl	#4, d4
	cmpl	#Lstart3,a5
	jle	Lstart0
	/*
	 * Enter kernel at destination address and continue copy
	 */
	jmp	Lstart1
Lstart1:
	movl	a6@+,_first_8_bytes	|  copy first 8 bytes
	movl	a6@,_first_8_bytes+4
Lstart2:
	movl	a0@+,a5@+		|  copy the rest of the kernel
	subl	#4, d4
	jcc	Lstart2
Lstart3:

	lea	tmpstk,sp		|  give ourselves a temporary stack

	/*
	 *  save the passed parameters. `prepass' them on the stack for
	 *  later catch by _start_c
	 */
	movl	a1,sp@-			|  pass address of _esym
	movl	d1,sp@-			|  pass stmem-size
	movl	d0,sp@-			|  pass fastmem-size
	movl	d5,sp@-			|  pass fastmem_start
	movl	d2,sp@-			|  pass machine id
	movl	d3,_boothowto		|  save reboot flags

	movl	#ATARI_68030,d1		|  68030 type from loader
	andl	d2,d1
	jeq	Ltestfor020		|  Not an 68030, try 68020
	movl	#MMU_68030,_mmutype	|  Use 68030 MMU
	jra	Lsetcpu040		|  skip to init.
Ltestfor020:
	movl	#ATARI_68020,d1		|  68020 type from loader
	andl	d2,d1
	jeq	Lsetcpu040
	movl	#MMU_68851,_mmutype	|  Assume 68851 with 68020
Lsetcpu040:
	movl	#CACHE_OFF,d0		|  68020/030 cache
	movl	#ATARI_68040,d1
	andl	d1,d2
	movl	d2,_cpu040		|  set 68040 CPU flag
	jeq	Lstartnot040		|  it's not 68040
	movl	#MMU_68040,_mmutype	|  same as hp300 for compat
	.word	0xf4f8			|  cpusha bc - push and inval caches
	movl	#CACHE40_OFF,d0		|  68040 cache disable
Lstartnot040:
	movc	d0,cacr			| clear and disable on-chip cache(s)
	moveq	#0,d0			| This assumes load-address 0!!
	movc	d0,vbr

	/*
	 * Initialize source/destination control registers for movs
	 */
	moveq	#FC_USERD,d0		|  user space
	movc	d0,sfc			|    as source
	movc	d0,dfc			|    and destination of transfers

	/*
	 * let the C function initialize everything and enable the MMU
	 */
	jsr	_start_c

	/*
	 * set kernel stack, user SP, and initial pcb
	 */
	movl	_proc0paddr,a1		| proc0 kernel stack
	lea	a1@(USPACE),sp		| set kernel stack to end of area
	movl	#USRSTACK-4,a2
	movl	a2,usp			| init user SP
	movl	a2,a1@(PCB_USP)		| and save it
	movl	a1,_curpcb		| proc0 is running
	clrw	a1@(PCB_FLAGS)		| clear flags
| LWP: The next part can be savely ommitted I think. The fpu probing
|      code resets the m6888? fpu. How about a 68040 fpu?
|
|	clrl	a1@(PCB_FPCTX)		|  ensure null FP context
|	pea	a1@(PCB_FPCTX)
|	jbsr	_m68881_restore		|  restore it (does not kill a1)
|	addql	#4,sp

	/* flush TLB and turn on caches */
	jbsr	_TBIA			|  invalidate TLB
	movl	#CACHE_ON,d0
	tstl	_cpu040
	jeq	Lcacheon
	/*  is this needed? MLH */
	.word	0xf4f8			|  cpusha bc - push & invalidate caches
	movl	#CACHE40_ON,d0
Lcacheon:
	movc	d0,cacr			|  clear cache(s)

	/*
	 * Final setup for C code
	 */
	movw	#PSL_LOWIPL,sr		|  lower SPL

#ifdef notdef
	movl	d6,_bootdev		|    and boot device
#endif

	/*
	 * Create a fake exception frame that returns to user mode,
	 * make space for the rest of a fake saved register set, and
	 * pass a pointer to the register set to "main()".
	 * "main()" will call "icode()", which fakes
	 * an "execve()" system call, which is why we need to do that
	 * ("main()" sets "u.u_ar0" to point to the register set).
	 * When "main()" returns, we're running in process 1 and have
	 * successfully faked the "execve()".  We load up the registers from
	 * that set; the "rte" loads the PC and PSR, which jumps to "init".
 	 */
	.globl	_proc0
	movl	#0,a6			|  make DDB stack_trace() work
  	clrw	sp@-			|  vector offset/frame type
	clrl	sp@-			|  PC - filled in by "execve"
  	movw	#PSL_USER,sp@-		|  in user mode
	clrl	sp@-			|  stack adjust count
	lea	sp@(-64),sp		|  construct space for D0-D7/A0-A7
	lea	_proc0,a0		| proc0 in a0
	movl	sp,a0@(P_MD + MD_REGS)	| save frame for proc0
	movl	usp,a1
	movl	a1,sp@(FR_SP)		| save user stack pointer in frame
	pea	sp@			|  addr of space for D0
	jbsr	_main			|  main(r0)
	addql	#4,sp			|  pop args
	tstl	_cpu040			|  68040?
	jeq	Lnoflush		|  no, skip
	.word	0xf478			|  cpusha dc
	.word	0xf498			|  cinva ic
Lnoflush:
	movl	sp@(FR_SP),a0		|  grab and load
	movl	a0,usp			|    user SP
	moveml	sp@+,#0x7FFF		|  load most registers (all but SSP)
	addql	#8,sp			|  pop SSP and stack adjust count
  	rte

/*
 * proc_trampoline call function in register a2 with a3 as an arg
 * and then rei.
 */
	.globl	_proc_trampoline
_proc_trampoline:
	movl	a3,sp@-			| push function arg (curproc)
	jbsr	a2@			| call function
	addql	#4,sp			| pop arg
	movl	sp@(FR_SP),a0		| usp to a0
	movl	a0,usp			| setup user stack pointer
	moveml	sp@+,#0x7FFF		| restore all but sp
	addql	#8,sp			| pop sp and stack adjust
	jra	rei			| all done

/*
 * Signal "trampoline" code (18 bytes).  Invoked from RTE setup by sendsig().
 *
 * Stack looks like:
 *
 *	sp+0 ->	signal number
 *	sp+4	signal specific code
 *	sp+8	pointer to signal context frame (scp)
 *	sp+12	address of handler
 *	sp+16	saved hardware state
 *			.
 *			.
 *	scp+0->	beginning of signal context frame
 */
	.globl	_sigcode, _esigcode
	.data
_sigcode:
	movl	sp@(12),a0		|  signal handler addr	 (4 bytes)
	jsr	a0@			|  call signal handler	 (2 bytes)
	addql	#4,sp			|  pop signo		 (2 bytes)
	trap	#1			|  special syscall entry (2 bytes)
	movl	d0,sp@(4)		|  save errno		 (4 bytes)
	moveq	#1,d0			|  syscall == exit	 (2 bytes)
	trap	#0			|  exit(errno)		 (2 bytes)
	.align	2
_esigcode:

/*
 * Primitives
 */
#include <m68k/asm.h>

/*
 * copypage(fromaddr, toaddr)
 *
 * Optimized version of bcopy for a single page-aligned NBPG byte copy.
 * dbra will work better perhaps.
 */
ENTRY(copypage)
	movl	sp@(4),a0		|  source address
	movl	sp@(8),a1		|  destination address
	movl	#NBPG/32,d0		|  number of 32 byte chunks
	tstl	_cpu040
	jeq	Lmlloop			|  no, use movl
Lm16loop:
	.long	0xf6209000		|  move16 a0@+,a1@+
	.long	0xf6209000		|  move16 a0@+,a1@+
	subql	#1,d0
	jne	Lm16loop
	rts
Lmlloop:
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	movl	a0@+,a1@+
	subql	#1,d0
	jne	Lmlloop
	rts
/*
 * update profiling information for the user
 * addupc(pc, &u.u_prof, ticks)
 */
ENTRY(addupc)
	movl	a2,sp@-			|  scratch register
	movl	sp@(12),a2		|  get &u.u_prof
	movl	sp@(8),d0		|  get user pc
	subl	a2@(8),d0		|  pc -= pr->pr_off
	jlt	Lauexit			|  less than 0, skip it
	movl	a2@(12),d1		|  get pr->pr_scale
	lsrl	#1,d0			|  pc /= 2
	lsrl	#1,d1			|  scale /= 2
	mulul	d1,d0			|  pc /= scale
	moveq	#14,d1
	lsrl	d1,d0			|  pc >>= 14
	bclr	#0,d0			|  pc &= ~1
	cmpl	a2@(4),d0		|  too big for buffer?
	jge	Lauexit			|  yes, screw it
	addl	a2@,d0			|  no, add base
	movl	d0,sp@-			|  push address
	jbsr	_fusword		|  grab old value
	movl	sp@+,a0			|  grab address back
	cmpl	#-1,d0			|  access ok
	jeq	Lauerror		|  no, skip out
	addw	sp@(18),d0		|  add tick to current value
	movl	d0,sp@-			|  push value
	movl	a0,sp@-			|  push address
	jbsr	_susword		|  write back new value
	addql	#8,sp			|  pop params
	tstl	d0			|  fault?
	jeq	Lauexit			|  no, all done
Lauerror:
	clrl	a2@(12)			|  clear scale (turn off prof)
Lauexit:
	movl	sp@+,a2			|  restore scratch reg
	rts

/*
 * non-local gotos
 */
ENTRY(setjmp)
	movl	sp@(4),a0		|  savearea pointer
	moveml	#0xFCFC,a0@		|  save d2-d7/a2-a7
	movl	sp@,a0@(48)		|  and return address
	moveq	#0,d0			|  return 0
	rts

ENTRY(qsetjmp)
	movl	sp@(4),a0		|  savearea pointer
	lea	a0@(40),a0		|  skip regs we do not save
	movl	a6,a0@+			|  save FP
	movl	sp,a0@+			|  save SP
	movl	sp@,a0@			|  and return address
	moveq	#0,d0			|  return 0
	rts

ENTRY(longjmp)
	movl	sp@(4),a0
	moveml	a0@+,#0xFCFC
	movl	a0@,sp@
	moveq	#1,d0
	rts

/*
 * The following primitives manipulate the run queues.
 * _whichqs tells which of the 32 queues _qs
 * have processes in them.  Setrunqueue puts processes into queues, Remrq
 * removes them from queues.  The running process is on no queue,
 * other processes are on a queue related to p->p_priority, divided by 4
 * actually to shrink the 0-127 range of priorities into the 32 available
 * queues.
 */

	.globl	_whichqs,_qs,_cnt,_panic
	.globl	_curproc
	.comm	_want_resched,4

/*
 * Setrunqueue(p)
 *
 * Call should be made at spl6(), and p->p_stat should be SRUN
 */
ENTRY(setrunqueue)
	movl	sp@(4),a0
	tstl	a0@(P_BACK)
	jeq	Lset1
	movl	#Lset2,sp@-
	jbsr	_panic
Lset1:
	clrl	d0
	movb	a0@(P_PRIORITY),d0
	lsrb	#2,d0
	movl	_whichqs,d1
	bset	d0,d1
	movl	d1,_whichqs
	lslb	#3,d0
	addl	#_qs,d0
	movl	d0,a0@(P_FORW)
	movl	d0,a1
	movl	a1@(P_BACK),a0@(P_BACK)
	movl	a0,a1@(P_BACK)
	movl	a0@(P_BACK),a1
	movl	a0,a1@(P_FORW)
	rts

Lset2:
	.asciz	"setrunqueue"
	.even

/*
 * Remrq(p)
 *
 * Call should be made at spl6().
 */
ENTRY(remrunqueue)
	movl	sp@(4),a0
	clrl	d0
	movb	a0@(P_PRIORITY),d0
	lsrb	#2,d0
	movl	_whichqs,d1
	bclr	d0,d1
	jne	Lrem1
	movl	#Lrem3,sp@-
	jbsr	_panic
Lrem1:
	movl	d1,_whichqs
	movl	a0@(P_FORW),a1
	movl	a0@(P_BACK),a1@(P_BACK)
	movl	a0@(P_BACK),a1
	movl	a0@(P_FORW),a1@(P_FORW)
	movl	#_qs,a1
	movl	d0,d1
	lslb	#3,d1
	addl	d1,a1
	cmpl	a1@(P_FORW),a1
	jeq	Lrem2
	movl	_whichqs,d1
	bset	d0,d1
	movl	d1,_whichqs
Lrem2:
	clrl	a0@(P_BACK)
	rts

Lrem3:
	.asciz	"remrunqueue"
Lsw0:
	.asciz	"cpu_switch"
	.even

	.globl	_curpcb
	.globl	_masterpaddr		|  XXX compatibility (debuggers)
	.data
_masterpaddr:				|  XXX compatibility (debuggers)
_curpcb:
	.long	0
pcbflag:
	.byte	0			|  copy of pcb_flags low byte
	.align	2
	.comm	nullpcb,SIZEOF_PCB
	.text

/*
 * At exit of a process, do a switch for the last time.
 * Switch to a safe stack and PCB, and deallocate the process's user area.
 */
ENTRY(switch_exit)
	movl	sp@(4),a0
	movl	#nullpcb,_curpcb	| save state into garbage pcb
	lea	tmpstk,sp		| goto a tmp stack

	/* Free old process's user area. */
	movl	#USPACE,sp@-		| size of u-area
	movl	a0@(P_ADDR),sp@-	| address of process's u-area
	movl	_kernel_map,sp@-	| map it was allocated in
	jbsr	_kmem_free		| deallocate it
	lea	sp@(12),sp		| pop args

	jra	_cpu_switch

/*
 * When no processes are on the runq, Swtch branches to idle
 * to wait for something to come ready.
 */
	.globl	Idle
Lidle:
	stop	#PSL_LOWIPL
Idle:
idle:
	movw	#PSL_HIGHIPL,sr
	tstl	_whichqs
	jeq	Lidle
	movw	#PSL_LOWIPL,sr
	jra	Lsw1

Lbadsw:
	movl	#Lsw0,sp@-
	jbsr	_panic
	/*NOTREACHED*/

/*
 * Cpu_switch()
 *
 * NOTE: On the mc68851 (318/319/330) we attempt to avoid flushing the
 * entire ATC.  The effort involved in selective flushing may not be
 * worth it, maybe we should just flush the whole thing?
 *
 * NOTE 2: With the new VM layout we now no longer know if an inactive
 * user's PTEs have been changed (formerly denoted by the SPTECHG p_flag
 * bit).  For now, we just always flush the full ATC.
 */
ENTRY(cpu_switch)
	movl	_curpcb,a0		|  current pcb
	movw	sr,a0@(PCB_PS)		|  save sr before changing ipl
#ifdef notyet
	movl	_curproc,sp@-		|  remember last proc running
#endif
	clrl	_curproc
Lsw1:
	/*
	 * Find the highest-priority queue that isn't empty,
	 * then take the first proc from that queue.
	 */
	clrl	d0
	lea	_whichqs,a0
	movl	a0@,d1
Lswchk:
	btst	d0,d1
	jne	Lswfnd
	addqb	#1,d0
	cmpb	#32,d0
	jne	Lswchk
	jra	idle
Lswfnd:
	movw	#PSL_HIGHIPL,sr		|  lock out interrupts
	movl	a0@,d1			|  and check again...
	bclr	d0,d1
	jeq	Lsw1			|  proc moved, rescan
	movl	d1,a0@			|  update whichqs
	moveq	#1,d1			|  double check for higher priority
	lsll	d0,d1			|  process (which may have snuck in
	subql	#1,d1			|  while we were finding this one)
	andl	a0@,d1
	jeq	Lswok			|  no one got in, continue
	movl	a0@,d1
	bset	d0,d1			|  otherwise put this one back
	movl	d1,a0@
	jra	Lsw1			|  and rescan
Lswok:
	movl	d0,d1
	lslb	#3,d1			|  convert queue number to index
	addl	#_qs,d1			|  locate queue (q)
	movl	d1,a1
	cmpl	a1@(P_FORW),a1		|  anyone on queue?
	jeq		Lbadsw		|  no, panic
	movl	a1@(P_FORW),a0		|  p = q->p_forw
	movl	a0@(P_FORW),a1@(P_FORW)	|  q->p_forw = p->p_forw
	movl	a0@(P_FORW),a1		|  q = p->p_forw
	movl	a0@(P_BACK),a1@(P_BACK)	|  q->p_back = p->p_back
	cmpl	a0@(P_FORW),d1		|  anyone left on queue?
	jeq	Lsw2			|  no, skip
	movl	_whichqs,d1
	bset	d0,d1			|  yes, reset bit
	movl	d1,_whichqs
Lsw2:
	movl	a0,_curproc
	clrl	_want_resched
#ifdef notyet
	movl	sp@+,a1
	cmpl	a0,a1			|  switching to same proc?
	jeq	Lswdone			|  yes, skip save and restore
#endif

	/*
	 * Save state of previous process in its pcb.
	 */
	movl	_curpcb,a1
	moveml	#0xFCFC,a1@(PCB_REGS)	|  save non-scratch registers
	movl	usp,a2			|  grab USP (a2 has been saved)
	movl	a2,a1@(PCB_USP)		|  and save it
	movl	_CMAP2,a1@(PCB_CMAP2)	|  save temporary map PTE
	tstl	_fputype		|  do we have an FPU?
	jeq	Lswnofpsave		|  no? don't attempt to save
	lea	a1@(PCB_FPCTX),a2	|  pointer to FP save area
	fsave	a2@			|  save FP state
	tstb	a2@			|  null state frame?
	jeq	Lswnofpsave		|  yes, all done
	fmovem	fp0-fp7,a2@(216)	|  save FP general registers
	fmovem	fpcr/fpsr/fpi,a2@(312)	|  save FP control registers
Lswnofpsave:

#ifdef DIAGNOSTIC
	tstl	a0@(P_WCHAN)
	jne	Lbadsw
	cmpb	#SRUN,a0@(P_STAT)
	jne	Lbadsw
#endif
	clrl	a0@(P_BACK)		|  clear back link
	movl	a0@(P_ADDR),a1		|  get p_addr
	movl	a1,_curpcb
	movb	a1@(PCB_FLAGS+1),pcbflag |  copy of pcb_flags low byte

	/* see if pmap_activate needs to be called; should remove this */
	movl	a0@(P_VMSPACE),a0	|  vmspace = p->p_vmspace
#ifdef DIAGNOSTIC
	tstl	a0			|  map == VM_MAP_NULL? 
	jeq	Lbadsw			|  panic 
#endif
	lea	a0@(VM_PMAP),a0		|  pmap = &vmspace.vm_pmap 
	tstl	a0@(PM_STCHG)		|  pmap->st_changed? 
	jeq	Lswnochg		|  no, skip 
	pea	a1@			|  push pcb (at p_addr) 
	pea	a0@			|  push pmap 
	jbsr	_pmap_activate		|  pmap_activate(pmap, pcb) 
	addql	#8,sp
	movl	_curpcb,a1		|  restore p_addr 
Lswnochg:
	lea	tmpstk,sp		|  now goto a tmp stack for NMI 
	tstl	_cpu040
	jne	Lres2
	movl	#CACHE_CLR,d0
	movc	d0,cacr			|  invalidate cache(s) 
	pflusha				|  flush entire TLB 
	jra	Lres3
Lres2:
	.word	0xf518			|  pflusha (68040)
	movl	#CACHE40_ON,d0
	movc	d0,cacr			|  invalidate cache(s)
Lres3:
	movl	a1@(PCB_USTP),d0	|  get USTP
	moveq	#PGSHIFT,d1
	lsll	d1,d0			|  convert to addr
	tstl	_cpu040
	jne	Lres4
	lea	_protorp,a0		|  CRP prototype
	movl	d0,a0@(4)		|  stash USTP
	pmove	a0@,crp			|  load new user root pointer
	jra	Lres5
Lres4:
	.word	0x4e7b,0x0806		|  movc d0,URP
Lres5:
	movl	a1@(PCB_CMAP2),_CMAP2	|  reload tmp map
	moveml	a1@(PCB_REGS),#0xFCFC	|  and registers
	movl	a1@(PCB_USP),a0
	movl	a0,usp			|  and USP
	tstl	_fputype		|  do we have an FPU?
	jeq	Lnofprest		|  no, don't attempt to restore
	lea	a1@(PCB_FPCTX),a0	|  pointer to FP save area
	tstb	a0@			|  null state frame?
	jeq	Lresfprest		|  yes, easy
	fmovem	a0@(312),fpcr/fpsr/fpi	|  restore FP control registers
	fmovem	a0@(216),fp0-fp7	|  restore FP general registers
Lresfprest:
	frestore a0@			|  restore state

Lnofprest:
	movw	a1@(PCB_PS),sr		|  no, restore PS
	moveq	#1,d0			|  return 1 (for alternate returns)
	rts

/*
 * savectx(pcb)
 * Update pcb, saving current processor state
 */
ENTRY(savectx)
	movl	sp@(4),a1
	movw	sr,a1@(PCB_PS)
	movl	usp,a0			|  grab USP 
	movl	a0,a1@(PCB_USP)		|  and save it 
	moveml	#0xFCFC,a1@(PCB_REGS)	|  save non-scratch registers 
	movl	_CMAP2,a1@(PCB_CMAP2)	|  save temporary map PTE 
	tstl	_fputype		|  do we have an FPU?
	jeq	Lsavedone		|  no, don't attempt to save
	lea	a1@(PCB_FPCTX),a0	|  pointer to FP save area 
	fsave	a0@			|  save FP state 
	tstb	a0@			|  null state frame? 
	jeq	Lsavedone		|  yes, all done 
	fmovem	fp0-fp7,a0@(216)	|  save FP general registers 
	fmovem	fpcr/fpsr/fpi,a0@(312)	|  save FP control registers 
Lsavedone:
	moveq	#0,d0			|  return 0 
	rts

/*
 * Copy 1 relocation unit (NBPG bytes)
 * from user virtual address to physical address
 */
ENTRY(copyseg)
	movl	_curpcb,a1		|  current pcb 
	movl	#Lcpydone,a1@(PCB_ONFAULT) |  where to return to on a fault 
	movl	sp@(8),d0		|  destination page number 
	moveq	#PGSHIFT,d1
	lsll	d1,d0			|  convert to address 
	orl	#PG_CI+PG_RW+PG_V,d0	|  make sure valid and writable 
	movl	_CMAP2,a0
	movl	_CADDR2,sp@-		|  destination kernel VA 
	movl	d0,a0@			|  load in page table 
	jbsr	_TBIS			|  invalidate any old mapping 
	addql	#4,sp
	movl	_CADDR2,a1		|  destination addr 
	movl	sp@(4),a0		|  source addr 
	movl	#NBPG/4-1,d0		|  count 
Lcpyloop:
	movsl	a0@+,d1			|  read longword 
	movl	d1,a1@+			|  write longword
	dbf	d0,Lcpyloop		|  continue until done
Lcpydone:
	movl	_curpcb,a1		|  current pcb
	clrl	a1@(PCB_ONFAULT)	|  clear error catch
	rts

/*
 * Copy 1 relocation unit (NBPG bytes)
 * from physical address to physical address
 */
ENTRY(physcopyseg)
	movl	sp@(4),d0		|  source page number 
	moveq	#PGSHIFT,d1
	lsll	d1,d0			|  convert to address 
	orl	#PG_CI+PG_RW+PG_V,d0	|  make sure valid and writable 
	movl	_CMAP1,a0
	movl	d0,a0@			|  load in page table 
	movl	_CADDR1,sp@-		|  destination kernel VA 
	jbsr	_TBIS			|  invalidate any old mapping 
	addql	#4,sp

	movl	sp@(8),d0		|  destination page number 
	moveq	#PGSHIFT,d1
	lsll	d1,d0			|  convert to address 
	orl	#PG_CI+PG_RW+PG_V,d0	|  make sure valid and writable 
	movl	_CMAP2,a0
	movl	d0,a0@			|  load in page table 
	movl	_CADDR2,sp@-		|  destination kernel VA 
	jbsr	_TBIS			|  invalidate any old mapping 
	addql	#4,sp

	movl	_CADDR1,a0		|  source addr 
	movl	_CADDR2,a1		|  destination addr 
	movl	#NBPG/4-1,d0		|  count 
Lpcpy:
	movl	a0@+,a1@+		|  copy longword 
	dbf	d0,Lpcpy		|  continue until done 
	rts

/*
 * zero out physical memory
 * specified in relocation units (NBPG bytes)
 */
ENTRY(clearseg)
	movl	sp@(4),d0		|  destination page number
	moveq	#PGSHIFT,d1
	lsll	d1,d0			|  convert to address
	orl	#PG_CI+PG_RW+PG_V,d0	|  make sure valid and writable
	movl	_CMAP1,a0
	movl	_CADDR1,sp@-		|  destination kernel VA
	movl	d0,a0@			|  load in page map
	jbsr	_TBIS			|  invalidate any old mapping
	addql	#4,sp
	movl	_CADDR1,a1		|  destination addr
	movl	#NBPG/4-1,d0		|  count
	/* simple clear loop is fastest on 68020 */
Lclrloop:
	clrl	a1@+			|  clear a longword
	dbf		d0,Lclrloop	|  continue til done
	rts

/*
 * Invalidate entire TLB.
 */
ENTRY(TBIA)
__TBIA:
	tstl	_cpu040
	jne	Ltbia040
	pflusha				|  flush entire TLB
	tstl	_mmutype
	jpl	Lmc68851a		|  68851 implies no d-cache
	movl	#DC_CLEAR,d0
	movc	d0,cacr			|  invalidate on-chip d-cache
Lmc68851a:
	rts
Ltbia040:
	.word	0xf518			|  pflusha
	rts

/*
 * Invalidate any TLB entry for given VA (TB Invalidate Single)
 */
ENTRY(TBIS)
#ifdef DEBUG
	tstl	fulltflush		|  being conservative?
	jne	__TBIA			|  yes, flush entire TLB
#endif
	movl	sp@(4),a0		|  get addr to flush
	tstl	_cpu040
	jne	Ltbis040
	tstl	_mmutype
	jpl	Lmc68851b		|  is 68851?
	pflush	#0,#0,a0@		|  flush address from both sides
	movl	#DC_CLEAR,d0
	movc	d0,cacr			|  invalidate on-chip data cache
	rts
Lmc68851b:
	pflushs	#0,#0,a0@		|  flush address from both sides
	rts
Ltbis040:
	moveq	#FC_SUPERD,d0		|  select supervisor
	movc	d0,dfc
	.word	0xf508			|  pflush a0@
	moveq	#FC_USERD,d0		|  select user
	movc	d0,dfc
	.word	0xf508			|  pflush a0@
	rts

/*
 * Invalidate supervisor side of TLB
 */
ENTRY(TBIAS)
#ifdef DEBUG
	tstl	fulltflush		|  being conservative?
	jne	__TBIA			|  yes, flush everything
#endif
	tstl	_cpu040
	jne	Ltbias040
	tstl	_mmutype
	jpl	Lmc68851c		|  68851?
	pflush	#4,#4			|  flush supervisor TLB entries
	movl	#DC_CLEAR,d0
	movc	d0,cacr			|  invalidate on-chip d-cache
	rts
Lmc68851c:
	pflushs #4,#4			|  flush supervisor TLB entries
	rts
Ltbias040:
/*  68040 can't specify supervisor/user on pflusha, so we flush all */
	.word	0xf518			|  pflusha
	rts

/*
 * Invalidate user side of TLB
 */
ENTRY(TBIAU)
#ifdef DEBUG
	tstl	fulltflush		|  being conservative?
	jne	__TBIA			|  yes, flush everything
#endif
	tstl	_cpu040
	jne	Ltbiau040
	tstl	_mmutype
	jpl	Lmc68851d		|  68851?
	pflush	#0,#4			|  flush user TLB entries
	movl	#DC_CLEAR,d0
	movc	d0,cacr			|  invalidate on-chip d-cache
	rts
Lmc68851d:
	pflushs	#0,#4			|  flush user TLB entries
	rts
Ltbiau040:
	/*  68040 can't specify supervisor/user on pflusha, so we flush all */
	.word	0xf518			|  pflusha
	rts

/*
 * Invalidate instruction cache
 */
ENTRY(ICIA)
ENTRY(ICPA)
#if defined(M68030) || defined(M68020)
#if defined(M68040)
	tstl	_cpu040
	jne	Licia040
#endif
	movl	#IC_CLEAR,d0
	movc	d0,cacr			|  invalidate i-cache
	rts
Licia040:
#endif
#if defined(M68040)
	.word	0xf498			|  cinva ic
	rts
#endif

/*
 * Invalidate data cache.
 * NOTE: we do not flush 68030 on-chip cache as there are no aliasing
 * problems with DC_WA.  The only cases we have to worry about are context
 * switch and TLB changes, both of which are handled "in-line" in resume
 * and TBI*.
 */
ENTRY(DCIA)
__DCIA:
	tstl	_cpu040
	jeq	Ldciax
	.word	0xf478			|  cpusha dc
Ldciax:
	rts

ENTRY(DCIS)
__DCIS:
	tstl	_cpu040
	jeq	Ldcisx
	.word	0xf478			|  cpusha dc
	nop
Ldcisx:
	rts

ENTRY(DCIU)
__DCIU:
	tstl	_cpu040
	jeq	Ldciux
	.word	0xf478			|  cpusha dc
Ldciux:
	rts

/*  Invalid single cache line */
ENTRY(DCIAS)
__DCIAS:
	tstl	_cpu040
	jeq	Ldciasx
	movl	sp@(4),a0
	.word	0xf468			|  cpushl dc,a0@
Ldciasx:
	rts
#ifdef M68040
ENTRY(ICPL)	/* invalidate instruction physical cache line */
	movl    sp@(4),a0		|  address
	.word   0xf488			|  cinvl ic,a0@
	rts
ENTRY(ICPP)	/* invalidate instruction physical cache page */
	movl    sp@(4),a0		|  address
	.word   0xf490			|  cinvp ic,a0@
	rts
ENTRY(DCPL)	/* invalidate data physical cache line */
	movl    sp@(4),a0		|  address
	.word   0xf448			|  cinvl dc,a0@
	rts
ENTRY(DCPP)	/* invalidate data physical cache page */
	movl    sp@(4),a0		|  address
	.word   0xf450			|  cinvp dc,a0@
	rts
ENTRY(DCPA)	/* invalidate data physical all */
	.word   0xf458			|  cinva dc
	rts
ENTRY(DCFL)	/* data cache flush line */
	movl    sp@(4),a0		|  address
	.word   0xf468			|  cpushl dc,a0@
	rts
ENTRY(DCFP)	/* data cache flush page */
	movl    sp@(4),a0		|  address
	.word   0xf470			|  cpushp dc,a0@
	rts
#endif	/* M68040 */

ENTRY(PCIA)
#if defined(M68030) || defined(M68030)
#if defined(M68040)
	tstl	_cpu040
	jne	Lpcia040
#endif
	movl	#DC_CLEAR,d0
	movc	d0,cacr			|  invalidate on-chip d-cache
	rts
#endif
#if defined(M68040)
ENTRY(DCFA)
Lpcia040:
	.word	0xf478			|  cpusha dc
	rts
#endif

ENTRY(ecacheon)
	rts

ENTRY(ecacheoff)
	rts

/*
 * Get callers current SP value.
 * Note that simply taking the address of a local variable in a C function
 * doesn't work because callee saved registers may be outside the stack frame
 * defined by A6 (e.g. GCC generated code).
 */
	.globl	_getsp
_getsp:
	movl	sp,d0			|  get current SP
	addql	#4,d0			|  compensate for return address
	rts

	.globl	_getsfc, _getdfc
_getsfc:
	movc	sfc,d0
	rts
_getdfc:
	movc	dfc,d0
	rts

/*
 * Check out a virtual address to see if it's okay to write to.
 *
 * probeva(va, fc)
 *
 */
ENTRY(probeva)
	movl	sp@(8),d0
	movec	d0,dfc
	movl	sp@(4),a0
	.word	0xf548			|  ptestw (a0)
	moveq	#FC_USERD,d0		|  restore DFC to user space
	movc	d0,dfc
	.word	0x4e7a,0x0805		|  movec  MMUSR,d0
	rts

/*
 * Load a new user segment table pointer.
 */
ENTRY(loadustp)
	movl	sp@(4),d0		|  new USTP
	moveq	#PGSHIFT,d1
	lsll	d1,d0			|  convert to addr
	tstl	_cpu040
	jne	Lldustp040
	lea	_protorp,a0		|  CRP prototype
	movl	d0,a0@(4)		|  stash USTP
	pmove	a0@,crp			|  load root pointer
	movl	#DC_CLEAR,d0
	movc	d0,cacr			|  invalidate on-chip d-cache
	rts				|    since pmove flushes TLB
Lldustp040:
	.word	0x4e7b,0x0806		|  movec d0,URP
	rts

/*
 * Flush any hardware context associated with given USTP.
 * Only does something for HP330 where we must flush RPT
 * and ATC entries in PMMU.
 */
ENTRY(flushustp)
	tstl	_cpu040
	jne	Lnot68851
	tstl	_mmutype		|  68851 PMMU?
	jle	Lnot68851		|  no, nothing to do
	movl	sp@(4),d0		|  get USTP to flush
	moveq	#PGSHIFT,d1
	lsll	d1,d0			|  convert to address
	movl	d0,_protorp+4		|  stash USTP
	pflushr	_protorp		|  flush RPT/TLB entries
Lnot68851:
	rts

ENTRY(ploadw)
	movl	sp@(4),a0		|  address to load
	tstl	_cpu040
	jne	Lploadw040
	ploadw	#1,a0@			|  pre-load translation
Lploadw040:				|  should 68040 do a ptest?
	rts

/*
 * Set processor priority level calls.  Most are implemented with
 * inline asm expansions.  However, spl0 requires special handling
 * as we need to check for our emulated software interrupts.
 */

ENTRY(spl0)
	moveq	#0,d0
	movw	sr,d0			|  get old SR for return
	movw	#PSL_LOWIPL,sr		|  restore new SR
	tstb	_ssir			|  software interrupt pending?
	jeq	Lspldone		|  no, all done
	subql	#4,sp			|  make room for RTE frame
	movl	sp@(4),sp@(2)		|  position return address
	clrw	sp@(6)			|  set frame type 0
	movw	#PSL_LOWIPL,sp@		|  and new SR
	jra	Lgotsir			|  go handle it
Lspldone:
	rts

ENTRY(_insque)
	movw	sr,d0
	movw	#PSL_HIGHIPL,sr		|  atomic
	movl	sp@(8),a0		|  where to insert (after)
	movl	sp@(4),a1		|  element to insert (e)
	movl	a0@,a1@			|  e->next = after->next
	movl	a0,a1@(4)		|  e->prev = after
	movl	a1,a0@			|  after->next = e
	movl	a1@,a0
	movl	a1,a0@(4)		|  e->next->prev = e
	movw	d0,sr
	rts

ENTRY(_remque)
	movw	sr,d0
	movw	#PSL_HIGHIPL,sr		|  atomic
	movl	sp@(4),a0		|  element to remove (e)
	movl	a0@,a1
	movl	a0@(4),a0
	movl	a0,a1@(4)		|  e->next->prev = e->prev
	movl	a1,a0@			|  e->prev->next = e->next
	movw	d0,sr
	rts

/*
 * Save and restore 68881 state.
 * Pretty awful looking since our assembler does not
 * recognize FP mnemonics.
 */
ENTRY(m68881_save)
	movl	sp@(4),a0		|  save area pointer
	fsave	a0@			|  save state
	tstb	a0@			|  null state frame?
	jeq	Lm68881sdone		|  yes, all done
	fmovem	fp0-fp7,a0@(216)	|  save FP general registers
	fmovem	fpcr/fpsr/fpi,a0@(312)	|  save FP control registers
Lm68881sdone:
	rts

ENTRY(m68881_restore)
	movl	sp@(4),a0		|  save area pointer
	tstb	a0@			|  null state frame?
	jeq	Lm68881rdone		|  yes, easy
	fmovem	a0@(312),fpcr/fpsr/fpi	|  restore FP control registers
	fmovem	a0@(216),fp0-fp7	|  restore FP general registers
Lm68881rdone:
	frestore a0@			|  restore state
	rts

/*
 * Handle the nitty-gritty of rebooting the machine.
 *
 */
	.globl	_doboot
_doboot:
	movl	#CACHE_OFF,d0
	movl	_cpu040,d1		|  load 68040 flag
	jeq	Ldoboot0
	.word	0xf4f8			|  cpusha bc - push and inval caches
	nop
	movl	#CACHE40_OFF,d0
Ldoboot0:
	movc	d0,cacr			|  disable on-chip cache(s)

	movw	#0x2700,sr		|  cut off any interrupts

	/*
	 * Clear first 2k of ST-memory. We start clearing at address 0x8
	 * because the lower 8 bytes are mapped to ROM.
	 * This makes sure that the machine will 'cold-boot'.
	 */
	movl	_page_zero,a0
	addl	#0x8,a0
	movl	#512,d0
Ldb1:
	clrl	a0@+
	dbra	d0,Ldb1
	
	lea	Ldoreboot,a1		| a1 = start of copy range
	lea	Ldorebootend,a2		| a2 = end of copy range
	movl	_page_zero,a0		| a0 = virtual base for page zero
	addl	a1,a0			|		+ offset of Ldoreboot
Ldb2:					| Do the copy
	movel	a1@+,a0@+
	cmpl	a1,a2
	jle	Ldb2

	/*
	 * Ok, turn off MMU..
	 */
Ldoreboot:
	tstl	d1			| We saved cpu040 here
	jne	Lmmuoff040		| Go turn off 68040 MMU
	lea	zero,a0
	pmove	a0@,tc			| Turn off MMU
	lea	nullrp,a0
	pmove	a0@,crp			| Invalidate Cpu root pointer
	pmove	a0@,srp			|  and the Supervisor root pointer
	jra	Ldoboot1		| Ok, continue with actual reboot
Lmmuoff040:
	movl	#0,d0
	.word	0x4e7b,0x0003		|  movc d0,TC
	.word	0x4e7b,0x0806		|  movc d0,URP
	.word	0x4e7b,0x0807		|  movc d0,SRP
Ldoboot1:
	movl	0x4,a0			| fetch reset-vector
	jmp	a0@			| jump through it
	/*  NOT REACHED */

/*  A do-nothing MMU root pointer (includes the following long as well) */

nullrp:	.long	0x7fff0001
zero:	.long	0
Ldorebootend:

	.data
	.space	NBPG
tmpstk:
	.globl	_mmutype,_protorp
_protorp:
	.long	0x80000002,0		|  prototype root pointer
	.globl	_cold
_cold:
	.long	1			|  cold start flag
	.globl	_proc0paddr
_proc0paddr:
	.long	0			|  KVA of proc0 u-area
#ifdef DEBUG
	.globl	fulltflush, fullcflush
fulltflush:
	.long	0
fullcflush:
	.long	0
	.globl	timebomb
timebomb:
	.long	0
#endif
/* interrupt counters */
	.globl	_intrcnt,_eintrcnt,_intrnames,_eintrnames
_intrnames:
	.asciz	"spur"
	.asciz	"clock"
	.asciz	"kbd/mouse"
	.asciz	"fdc/acsi"
	.asciz	"soft"
	.asciz	"5380-SCSI"
	.asciz	"5380-DMA"
	.asciz	"nmi"
	.asciz	"8530-SCC"
	.asciz	"statclock"
_eintrnames:
	.even
_intrcnt:
	.long	0,0,0,0,0,0,0,0,0,0
_eintrcnt:
