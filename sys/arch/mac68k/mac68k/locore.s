/*	$NetBSD: locore.s,v 1.52 1995/12/11 02:38:08 thorpej Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990 The Regents of the University of California.
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
 */
/*-
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
 * All rights reserved.
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * from: Utah $Hdr: locore.s 1.58 91/04/22$
 *
 *	@(#)locore.s	7.11 (Berkeley) 5/9/91
 */

#include "assym.s"
#include "vectors.s"
#include "macglobals.s"

	.text

/*
 * This is where we wind up if the kernel jumps to location 0.
 * (i.e. a bogus PC)  This is known to immediately follow the vector
 * table and is hence at 0x400 (see reset vector in vectors.s).
 */
	.globl	_panic, _panicstr
	.globl	_jmp0panic

_jmp0panic:
	tstl	_panicstr
	jeq	jmp0panic
	stop	#0x2700
jmp0panic:
	pea	Ljmp0panic
	jbsr	_panic
	/* NOTREACHED */
Ljmp0panic:
	.asciz	"kernel jump to zero"
	.even


/*
 * Trap/interrupt vector routines
 */ 

	.globl	_trap, _nofault, _longjmp, _mac68k_buserr_addr
_buserr:
	tstl	_nofault		| device probe?
	jeq	Lberr			| no, handle as usual
	movl	sp@(0x10),_mac68k_buserr_addr
	movl	_nofault,sp@-		| yes,
	jbsr	_longjmp		|  longjmp(nofault)
Lberr:
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	_addrerr		| no, skip
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	lea	sp@(FR_HW),a1		| grab base of HW berr frame
	moveq	#0,d0
	movw	a1@(12),d0		| grab SSW
	movl	a1@(20),d1		| and fault VA
	btst	#11,d0			| check for mis-aligned access
	jeq	Lberr2			| no, skip
	addl	#3,d1			| yes, get into next page
	andl	#PG_FRAME,d1		| and truncate
Lberr2:
	movl	d1,sp@-			| push fault VA
	movl	d0,sp@-			| and padded SSW
	btst	#10,d0			| ATC bit set?
	jeq	Lisberr			| no, must be a real bus error
	movc	dfc,d1			| yes, get MMU fault
	movc	d0,dfc			| store faulting function code
	movl	sp@(4),a0		| get faulting address
	.word	0xf568			| ptestr a0@
	movc	d1,dfc
	.long	0x4e7a0805		| movc mmusr,d0
	movw	d0,sp@			| save (ONLY LOW 16 BITS!)
	jra	Lismerr
#endif
_addrerr:
	clrl	sp@-			| pad SR to longword
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	lea	sp@(FR_HW),a1		| grab base of HW berr frame
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lbenot040		| no, skip
	movl	a1@(8),sp@-		| yes, push fault address
	clrl	sp@-			| no SSW for address fault
	jra	Lisaerr			| go deal with it
Lbenot040:
#endif
	moveq	#0,d0
	movw	a1@(10),d0		| grab SSW for fault processing
	btst	#12,d0			| RB set?
	jeq	LbeX0			| no, test RC
	bset	#14,d0			| yes, must set FB
	movw	d0,a1@(10)		| for hardware, too
LbeX0:
	btst	#13,d0			| RC set?
	jeq	LbeX1			| no, skip
	bset	#15,d0			| yes, must set FC
	movw	d0,a1@(10)		| for hardware, too
LbeX1:
	btst	#8,d0			| data fault?
	jeq	Lbe0			| no, check for hard cases
	movl	a1@(16),d1		| fault address is as given in frame
	jra	Lbe10			| that's it!
Lbe0:
	btst	#4,a1@(6)		| long (type B) stack frame?
	jne	Lbe4			| yes, go handle
	movl	a1@(2),d1		| no, can use save PC
	btst	#14,d0			| FB set?
	jeq	Lbe3			| no, try FC
	addql	#4,d1			| yes, adjust address
	jra	Lbe10			| done
Lbe3:
	btst	#15,d0			| FC set?
	jeq	Lbe10			| no, done
	addql	#2,d1			| yes, adjust address
	jra	Lbe10			| done
Lbe4:
	movl	a1@(36),d1		| long format, use stage B address
	btst	#15,d0			| FC set?
	jeq	Lbe10			| no, all done
	subql	#2,d1			| yes, adjust address
Lbe10:
	movl	d1,sp@-			| push fault VA
	movl	d0,sp@-			| and padded SSW
	movw	a1@(6),d0		| get frame format/vector offset
	andw	#0x0FFF,d0		| clear out frame format
	cmpw	#12,d0			| address error vector?
	jeq	Lisaerr			| yes, go to it
	movl	d1,a0			| fault address
	ptestr	#1,a0@,#7		| do a table search
	pmove	psr,sp@			| save result
	btst	#7,sp@			| bus error bit set?
	jeq	Lismerr			| no, must be MMU fault
	clrw	sp@			| yes, re-clear pad word
	jra	Lisberr			| and process as normal bus error
Lismerr:
	movl	#T_MMUFLT,sp@-		| show that we are an MMU fault
	jra	Ltrapnstkadj		| and deal with it
Lisaerr:
	movl	#T_ADDRERR,sp@-		| mark address error
	jra	Ltrapnstkadj		| and deal with it
Lisberr:
	movl	#T_BUSERR,sp@-		| mark bus error
Ltrapnstkadj:
	jbsr	_trap			| handle the error
	lea	sp@(12),sp		| pop value args
	movl	sp@(FR_SP),a0		| restore user SP
	movl	a0,usp			|   from save area
	movw	sp@(FR_ADJ),d0		| need to adjust stack?
	jne	Lstkadj			| yes, go to it
	moveml	sp@+,#0x7FFF		| no, restore most user regs
	addql	#8,sp			| toss SSP and pad
	jra	rei			| all done
Lstkadj:
	lea	sp@(FR_HW),a1		| pointer to HW frame
	addql	#8,a1			| source pointer
	movl	a1,a0			| source
	addw	d0,a0			|  + hole size = dest pointer
	movl	a1@-,a0@-		| copy
	movl	a1@-,a0@-		|  8 bytes
	movl	a0,sp@(FR_SP)		| new SSP
	moveml	sp@+,#0x7FFF		| restore user registers
	movl	sp@,sp			| and our SP
	jra	rei			| all done

/*
 * FP exceptions.
 */
_fpfline:
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lfp_unimp		| no, skip FPSP
	cmpw	#0x202c,sp@(6)		| format type 2?
	jne	_illinst		| no, treat as illinst
Ldofp_unimp:
#ifdef FPSP
	.globl	fpsp_unimp
	jmp	fpsp_unimp		| go handle in fpsp
#endif
Lfp_unimp:
#endif
#ifdef FPU_EMULATE
	clrl	sp@-		| pad SR to longword
	moveml	#0xFFFF,sp@-	| save user registers
	moveq	#T_FPEMULI,d0	| denote it as an FP emulation trap.
	jra	fault		| do it.
#else
	jra	_illinst
#endif

_fpunsupp:
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lfp_unsupp		| no, treat as illinst
#ifdef FPSP
	.globl	fpsp_unsupp
	jmp	fpsp_unsupp		| yes, go handle it
#endif
Lfp_unsupp:
#endif
#ifdef FPU_EMULATE
	clrl	sp@-			| pad SR to longword
	moveml	#0xFFFF,sp@-		| save user registers
	moveq	#T_FPEMULD,d0		| denote it as an FP emulation trap.
	jra	fault			| do it.
#else
	jra	_illinst
#endif

/*
 * Handles all other FP coprocessor exceptions.
 * Note that since some FP exceptions generate mid-instruction frames
 * and may cause signal delivery, we need to test for stack adjustment
 * after the trap call.
 */
_fpfault:
	clrl	sp@-		| pad SR to longword
	moveml	#0xFFFF,sp@-	| save user registers
	movl	usp,a0		| and save
	movl	a0,sp@(FR_SP)	|   the user stack pointer
	clrl	sp@-		| no VA arg
	movl	_curpcb,a0	| current pcb
	lea	a0@(PCB_FPCTX),a0 | address of FP savearea
	fsave	a0@		| save state
	tstb	a0@		| null state frame?
	jeq	Lfptnull	| yes, safe
	clrw	d0		| no, need to tweak BIU
	movb	a0@(1),d0	| get frame size
	bset	#3,a0@(0,d0:w)	| set exc_pend bit of BIU
Lfptnull:
	fmovem	fpsr,sp@-	| push fpsr!as code argument
	frestore a0@		| restore state
	movl	#T_FPERR,sp@-	| push type arg
	jra	Ltrapnstkadj	| call trap and deal with stack cleanup

/*
 * Coprocessor and format errors can generate mid-instruction stack
 * frames and cause signal delivery hence we need to check for potential
 * stack adjustment.
 */
_coperr:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	movl	usp,a0		| get and save
	movl	a0,sp@(FR_SP)	|   the user stack pointer
	clrl	sp@-		| no VA arg
	clrl	sp@-		| or code arg
	movl	#T_COPERR,sp@-	| push trap type
	jra	Ltrapnstkadj	| call trap and deal with stack adjustments

_fmterr:
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	movl	usp,a0		| get and save
	movl	a0,sp@(FR_SP)	|   the user stack pointer
	clrl	sp@-		| no VA arg
	clrl	sp@-		| or code arg
	movl	#T_FMTERR,sp@-	| push trap type
	jra	Ltrapnstkadj	| call trap and deal with stack adjustments

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
	movl	usp,a0			| get and save
	movl	a0,sp@(FR_SP)		|   the user stack pointer
	clrl	sp@-			| no VA arg
	clrl	sp@-			| or code arg
	movl	d0,sp@-			| push trap type
	jbsr	_trap			| handle trap
	lea	sp@(12),sp		| pop value args
	movl	sp@(FR_SP),a0		| restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| restore most user regs
	addql	#8,sp			| pop SP and pad word
	jra	rei			| all done

	.globl	_straytrap
_badtrap:
	moveml	#0xC0C0,sp@-		| save scratch regs
	movw	sp@(22),sp@-		| push exception vector info
	clrw	sp@-			| and pad
	movl	sp@(22),sp@-		| and PC
	jbsr	_straytrap		| report
	addql	#8,sp			| pop args
	moveml	sp@+, #0x0303		| restore regs
	jra	rei

	.globl	_syscall
_trap0:
	clrl	sp@-			| pad SR to longword
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	movl	d0,sp@-			| push syscall number
	jbsr	_syscall		| handle it
	addql	#4,sp			| pop syscall arg
	tstl	_astpending
	jne	Lrei2
	tstb	_ssir
	jeq	Ltrap1
	movw	#SPL1,sr
	tstb	_ssir
	jne	Lsir1
Ltrap1:
	movl	sp@(FR_SP),a0		| grab and restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| restore most registers
	addql	#8,sp			| pop SSP and align word
	rte

/*
 * Our native 4.3 implementation uses trap 1 as sigreturn() and trap 2
 * as a breakpoint trap.
 */
_trap1:
	jra	sigreturn

_trap2:
	jra	_trace

/*
 * Trap 12 is the entry point for the cachectl "syscall" (both HPUX & BSD)
 *	cachectl(command, addr, length)
 * command in d0, addr in a1, length in d1
 */
	.globl	_cachectl
_trap12:
	movl	d1,sp@-			| push length
	movl	a1,sp@-			| push addr
	movl	d0,sp@-			| push command
	jbsr	_cachectl		| do it
	lea	sp@(12),sp		| pop args
	jra	rei			| all done

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
	movw	sp@(FR_HW),d1		| get PSW
	andw	#PSL_S,d1		| from user mode?
	jeq	fault			| yes, just a regular fault
	movl	d0,sp@-
	.globl	_kgdb_trap_glue
	jbsr	_kgdb_trap_glue		| returns if no debugger
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
	movw	sp@(FR_HW),d1		| get SSW
	andw	#PSL_S,d1		| from user mode?
	jeq	fault			| yes, just a regular fault
	movl	d0,sp@-
	jbsr	_kgdb_trap_glue		| returns if no debugger
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
	lea	sp@(-84),sp		| leave enough space for largest frame
	movl	sp@(84),sp@		| move up current 8 byte frame
	movl	sp@(88),sp@(4)
	movl	#84,sp@-		| default: adjust by 84 bytes
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	movl	#SYS_sigreturn,sp@-	| push syscall number
	jbsr	_syscall		| handle it
	addql	#4,sp			| pop syscall#
	movl	sp@(FR_SP),a0		| grab and restore
	movl	a0,usp			|   user SP
	lea	sp@(FR_HW),a1		| pointer to HW frame
	movw	sp@(FR_ADJ),d0		| do we need to adjust the stack?
	jeq	Lsigr1			| no, just continue
	moveq	#92,d1			| total size
	subw	d0,d1			|  - hole size = frame size
	lea	a1@(92),a0		| destination
	addw	d1,a1			| source
	lsrw	#1,d1			| convert to word count
	subqw	#1,d1			| minus 1 for dbf
Lsigrlp:
	movw	a1@-,a0@-		| copy a word
	dbf	d1,Lsigrlp		| continue
	movl	a0,a1			| new HW frame base
Lsigr1:
	movl	a1,sp@(FR_SP)		| new SP value
	moveml	sp@+,#0x7FFF		| restore user registers
	movl	sp@,sp			| and our SP
	jra	rei			| all done

/*
 * Interrupt handlers.
 *
 *	Level 0:	Spurious: ignored.
 *	Level 1:	HIL
 *	Level 2:
 *	Level 3:	Internal HP-IB
 *	Level 4:	"Fast" HP-IBs, SCSI
 *	Level 5:	DMA, Ethernet, Built-in RS232
 *	Level 6:	Clock
 *	Level 7:	Non-maskable: parity errors, RESET key
 *
 * ALICE: Here are our assignments:
 *
 *      Level 0:        Spurious: ignored
 *      Level 1:        VIA1 (clock, ADB)
 *      Level 2:        VIA2 (NuBus, SCSI)
 *      Level 3:
 *      Level 4:        Serial (SCC)
 *      Level 5:
 *      Level 6:
 *      Level 7:        Non-maskable: parity errors, RESET button, FOO key
 *
 * On the Q700, at least, in "A/UX mode," this should become:
 *
 *	Level 0:        Spurious: ignored
 *	Level 1:        Software
 *	Level 2:        VIA2 (except ethernet, sound)
 *	Level 3:        Ethernet
 *	Level 4:        Serial (SCC)
 *	Level 5:        Sound
 *	Level 6:        VIA1
 *	Level 7:        NMIs: parity errors, RESET button, YANCC error
 */
/* BARF We must re-configure this. */
	.globl	_hardclock, _nmihand

_spurintr:
_lev3intr:
_lev5intr:
_lev6intr:
	addql	#1,_intrcnt+0
	addql	#1,_cnt+V_INTR
	jra	rei

_lev1intr:
	addql	#1,_intrcnt+4
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	movl	sp, sp@-
	jbsr	_via1_intr
	addql	#4,sp
	moveml	sp@+,#0xFFFF
	addql	#4,sp
	addql	#1,_cnt+V_INTR
	jra	rei

	.globl	_real_via2_intr
_lev2intr:
	addql	#1,_intrcnt+8
	clrl	sp@-
	moveml	#0xFFFF,sp@-
	movl	sp, sp@-
	movl	_real_via2_intr,a2
	jbsr	a2@
	addql	#4,sp
	moveml	sp@+,#0xFFFF
	addql	#4,sp
	addql	#1,_cnt+V_INTR
	jra	rei

	.globl _zshard

_lev4intr:
	/* handle level 4 (SCC) interrupt special... */
	addql	#1,_intrcnt+12
	clrl	sp@-
	moveml	#0xFFFF,sp@-	| save registers
	clrl	sp@-		| push 0
	jsr	_zshard		| call C routine to deal with it (ser.c/zs.c)
	addl	#4,sp		| throw away arg
	moveml	sp@+, #0xFFFF	| restore registers
	addql	#4,sp
	rte			| return from exception

	.globl _rtclock_intr

/* MAJORBARF: Fix this routine to be like Mac clocks */
_rtclock_intr:
	movl	a6@(8),a1		| get pointer to frame in via1_intr
	movl	a1@(64), sp@-		| push ps
	movl	a1@(68), sp@-		| push pc
	movl	sp, sp@-		| push pointer to ps, pc
	jbsr	_hardclock		| call generic clock int routine
	lea	sp@(12), sp		| pop params
	addql	#1,_intrcnt+20		| add another system clock interrupt

	addql	#1,_cnt+V_INTR		| chalk up another interrupt

	movl	#1, d0			| clock taken care of
	rts				| go back from whence we came

_lev7intr:
	addql	#1, _intrcnt+16
	clrl	sp@-			| pad SR to longword
	moveml	#0xFFFF,sp@-		| save registers
	movl	usp,a0			| and save
	movl	a0,sp@(FR_SP)		|   the user stack pointer
	jbsr	_nmihand		| call handler
	movl	sp@(FR_SP),a0		| restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| and remaining registers
	addql	#8,sp			| pop SSP and align word
	jra	rei			| all done

/*
 * Emulation of VAX REI instruction.
 *
 * This code deals with checking for and servicing ASTs
 * (profiling, scheduling) and software interrupts (network, softclock).
 * We check for ASTs first, just like the VAX.  To avoid excess overhead
 * the T_ASTFLT handling code will also check for software interrupts so we
 * do not have to do it here.  After identifying that we need an AST we
 * drop the IPL to allow device interrupts.
 *
 * This code is complicated by the fact that sendsig may have been called
 * necessitating a stack cleanup.
 */
	.comm	_ssir,1
	.globl	_astpending
	.globl	rei
rei:
#undef STACKCHECK
#ifdef STACKCHECK
	tstl	_panicstr		| have we paniced?
	jne	Ldorte1			| yes, do not make matters worse
#endif
	tstl	_astpending		| AST pending?
	jeq	Lchksir			| no, go check for SIR
Lrei1:
	btst	#5,sp@			| yes, are we returning to user mode?
	jne	Lchksir			| no, go check for SIR
	movw	#PSL_LOWIPL,sr		| lower SPL
	clrl	sp@-			| stack adjust
	moveml	#0xFFFF,sp@-		| save all registers
	movl	usp,a1			| including
	movl	a1,sp@(FR_SP)		|    the users SP
Lrei2:
	clrl	sp@-			| VA == none
	clrl	sp@-			| code == none
	movl	#T_ASTFLT,sp@-		| type == async system trap
	jbsr	_trap			| go handle it
	lea	sp@(12),sp		| pop value args
	movl	sp@(FR_SP),a0		| restore user SP
	movl	a0,usp			|   from save area
	movw	sp@(FR_ADJ),d0		| need to adjust stack?
	jne	Laststkadj		| yes, go to it
	moveml	sp@+,#0x7FFF		| no, restore most user regs
	addql	#8,sp			| toss SP and stack adjust
#ifdef STACKCHECK
	jra	Ldorte
#else
	rte				| and do real RTE
#endif
Laststkadj:
	lea	sp@(FR_HW),a1		| pointer to HW frame
	addql	#8,a1			| source pointer
	movl	a1,a0			| source
	addw	d0,a0			|  + hole size = dest pointer
	movl	a1@-,a0@-		| copy
	movl	a1@-,a0@-		|  8 bytes
	movl	a0,sp@(FR_SP)		| new SSP
	moveml	sp@+,#0x7FFF		| restore user registers
	movl	sp@,sp			| and our SP
#ifdef STACKCHECK
	jra	Ldorte
#else
	rte				| and do real RTE
#endif
Lchksir:
	tstb	_ssir			| SIR pending?
	jeq	Ldorte			| no, all done
	movl	d0,sp@-			| need a scratch register
	movw	sp@(4),d0		| get SR
	andw	#PSL_IPL7,d0		| mask all but IPL
	jne	Lnosir			| came from interrupt, no can do
	movl	sp@+,d0			| restore scratch register
Lgotsir:
	movw	#SPL1,sr		| prevent others from servicing int
	tstb	_ssir			| too late?
	jeq	Ldorte			| yes, oh well...
	clrl	sp@-			| stack adjust
	moveml	#0xFFFF,sp@-		| save all registers
	movl	usp,a1			| including
	movl	a1,sp@(FR_SP)		|    the users SP
Lsir1:
	clrl	sp@-			| VA == none
	clrl	sp@-			| code == none
	movl	#T_SSIR,sp@-		| type == software interrupt
	jbsr	_trap			| go handle it
	lea	sp@(12),sp		| pop value args
	movl	sp@(FR_SP),a0		| restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| and all remaining registers
	addql	#8,sp			| pop SP and stack adjust
#ifdef STACKCHECK
	jra	Ldorte
#else
	rte
#endif
Lnosir:
	movl	sp@+,d0			| restore scratch register
Ldorte:
#ifdef STACKCHECK
	movw	#SPL6,sr		| avoid trouble
	btst	#5,sp@			| are we returning to user mode?
	jne	Ldorte1			| no, skip it
	movl	a6,tmpstk-20
	movl	d0,tmpstk-76
	moveq	#0,d0
	movb	sp@(6),d0		| get format/vector
	lsrl	#3,d0			| convert to index
	lea	_exframesize,a6		|  into exframesize
	addl	d0,a6			|  to get pointer to correct entry
	movw	a6@,d0			| get size for this frame
	addql	#8,d0			| adjust for unaccounted for bytes
	lea	_kstackatbase,a6	| desired stack base
	subl	d0,a6			|   - frame size == our stack
	cmpl	a6,sp			| are we where we think?
	jeq	Ldorte2			| yes, skip it
	lea	tmpstk,a6		| will be using tmpstk
	movl	sp@(4),a6@-		| copy common
	movl	sp@,a6@-		|   frame info
	clrl	a6@-
	movl	sp,a6@-			| save sp
	subql	#4,a6			| skip over already saved a6
	moveml	#0x7FFC,a6@-		| push remaining regs (d0/a6/a7 done)
	lea	a6@(-4),sp		| switch to tmpstk (skip saved d0)
	clrl	sp@-			| is an underflow
	jbsr	_badkstack		| badkstack(0, frame)
	addql	#4,sp
	moveml	sp@+,#0x7FFF		| restore most registers
	movl	sp@,sp			| and SP
	rte
Ldorte2:
	movl	tmpstk-76,d0
	movl	tmpstk-20,a6
Ldorte1:
#endif
	rte				| real return

/*
 * Kernel access to the current processes kernel stack is via a fixed
 * virtual address.  It is at the same address as in the users VA space.
 */
		.data
| Scratch memory.  Careful when messing with these...
longscratch:	.long	0
longscratch2:	.long	0
pte_tmp:	.long	0  | for get_pte()
_macos_crp1:	.long	0
_macos_crp2:	.long	0
_macos_tc:	.long	0
_macos_tt0:	.long	0
_macos_tt1:	.long	0
_bletch:	.long	0
_esym:		.long	0
		.globl	_esym, _bletch
		.globl	_macos_crp1, _macos_crp2, _macos_tc
		.globl	_macos_tt0, _macos_tt1

/*
 * Initialization
 */
	.even

	.text
	.globl	_edata
	.globl	_etext
	.globl	start
	.globl _videoaddr, _videorowbytes
	.globl _videobitdepth
	.globl _machineid
	.globl _videosize
	.globl _IOBase
	.globl _NuBusBase

	.globl _locore_dodebugmarks

#define DEBUG
#ifdef DEBUG
#define debug_mark(s)			\
	.data	;			\
0:	.asciz	s ;			\
	.text	;			\
	tstl	_locore_dodebugmarks ;	\
	beq	1f ;			\
	movml	#0xC0C0, sp@- ;		\
	pea	0b ;			\
	jbsr	_printf ;		\
	addql	#4, sp ;		\
	movml	sp@+, #0x0303 ;		\
1:	;
#endif

start:
	movw	#PSL_HIGHIPL,sr		| no interrupts.  ever.

| Give ourself a stack
	lea	tmpstk,sp		| give ourselves a temporary stack
	movl	#CACHE_OFF,d0
	movc	d0, cacr

| Some parameters provided by MacOS
|
| LAK: This section is the new way to pass information from the booter
| to the kernel.  At A1 there is an environment variable which has
| a bunch of stuff in ascii format, "VAR=value\0VAR=value\0\0".

	.globl	_initenv, _getenvvars	| in machdep.c
	.globl	_setmachdep		| in machdep.c

	/* Initialize source/destination control registers for movs */
	moveq	#FC_USERD,d0		| user space
	movc	d0,sfc			|   as source
	movc	d0,dfc			|   and destination of transfers

	movl	a1, sp@-		| Address of buffer
	movl	d4, sp@-		| Some flags... (mostly not used)
	jbsr	_initenv
	addql	#8, sp

	jbsr	_getenvvars		| Parse the environment buffer

	jbsr	_setmachdep		| Set some machine-dep stuff

	jbsr	_vm_set_page_size	| Set the vm system page size, now.
	jbsr	_consinit		| XXX Should only be if graybar on

	cmpl	#MMU_68040, _mmutype	| Set in _getenvvars ONLY if 040.
	jne	Lstartnot040		| It's not an '040
	.word	0xf4f8			| cpusha bc - push and invalidate caches

	movl	#CACHE4_OFF,d0		| 68040 cache disable
	movc	d0, cacr

	movel	#0x0, d0
	.word	0x4e7b, 0x0004		| Disable itt0
	.word	0x4e7b, 0x0005		| Disable itt1
	.word	0x4e7b, 0x0006		| Disable dtt0
	.word	0x4e7b, 0x0007		| Disable dtt1
	.word	0x4e7b, 0x0003		| Disable MMU

	movl	#0x0,sp@-		| Fake unenabled MMU
	jra	do_bootstrap

Lstartnot040:

| BG - Figure out our MMU
	movl	#0x200, d0		| data freeze bit (??)
	movc	d0, cacr		| only exists in 68030
	movc	cacr, d0		| on an '851, it'll go away.
	tstl	d0
	jeq	Lisa68020
	movl	#MMU_68030, _mmutype	| 68030 MMU
	jra	Lmmufigured
Lisa68020:
	movl	#MMU_68851, _mmutype	| 68020, implies 68851, or crash.
Lmmufigured:

	lea	_macos_tc,a0
	pmove	tc,a0@
	movl	a0@,sp@-		| Save current TC for bootstrap

/*
 * Figure out MacOS mappings and bootstrap NetBSD
 */
do_bootstrap:
	jbsr	_bootstrap_mac68k
	addql	#4,sp
	
/*
 * Prepare to enable MMU.
 */
	movl	_Sysseg,a1		| system segment table addr
	addl	_load_addr,a1		| Make it physical addr

	cmpl	#MMU_68040, _mmutype
	jne	Lenablepre040MMU	| if not 040, skip
	movl	a1,d1
	.long	0x4e7b1807		| movc d1,srp
	.word	0xf4d8			| cinva bc
	.word	0xf518			| pflusha
	movl	#0x8000,d0
	.long	0x4e7b0003		| Enable MMU
	movl	#0x80008000,d0
	movc	d0,cacr			| turn on both caches
	jra	Lloaddone

Lenablepre040MMU:
	lea	_protorp,a0
	movl	#0x80000202,a0@		| nolimit + share global + 4 byte PTEs
	movl	a1,a0@(4)		| + segtable address
	pmove	a0@,srp			| load the supervisor root pointer
	movl	#0x80000002,a0@		| reinit upper half for CRP loads

| LAK: Kill the TT0 and TT1 registers so the don't screw us up later.
	tstl	_mmutype		| ttx instructions will break 68851
	jgt	LnokillTT

	lea	longscratch,a0
	movl	#0, a0@
	.long	0xF0100800		| movl a0@,tt0
	.long	0xF0100C00		| movl a0@,tt1

LnokillTT:
	lea	longscratch,a2
	movl	#0x82c0aa00,a2@		| value to load TC with
	pmove	a2@,tc			| load it

Lloaddone:

/*
 * Should be running mapped from this point on
 */

/* init mem sizes */

/* set kernel stack, user SP, and initial pcb */
	movl	_proc0paddr,a1		| get proc0 pcb addr
	lea	a1@(USPACE-4),sp	| set kernel stack to end of area
	movl	#USRSTACK-4,a2
	movl	a2,usp			| init user SP
	movl	a1,_curpcb		| proc0 is running
	jbsr	_TBIA			| invalidate TLB
	cmpl	#MMU_68040,_mmutype	| 68040?
	jeq	Lnocache0		| yes, cache already on
	movl	#CACHE_ON,d0
	movc	d0,cacr			| clear cache(s)
/* XXX Enable external cache here. */

Lnocache0:
/* final setup for C code */
	jbsr	_setmachdep		| Set some machine-dep stuff
	movw	#PSL_LOWIPL,sr		| lower SPL ; enable interrupts

/*
 * Create a fake exception frame so that cpu_fork() can copy it.
 * main() never returns; we exit to user mode from a forked process
 * later on.
 */
	clrw	sp@-			| vector offset/frame type
	clrl	sp@-			| PC - filled in by "execve"
	movw	#PSL_USER,sp@-		| in user mode
	clrl	sp@-			| stack adjust count and padding
	lea	sp@(-64),sp		| construct space for D0-D7/A0-A7
	lea	_proc0,a0		| save pointer to frame
	movl	sp,a0@(P_MD_REGS)	|   in proc0.p_md.md_regs

	jra	_main

/*
 * proc_trampoline
 *	Call function in register a2 with a3 as an arg and then rei.  Note
 * that we restore the stack before calling, thus giving "a2" more stack.
 * (for the case that, e.g., if curproc had a deeply nested call chain...)
 * cpu_fork() also depends on struct frame being a second arg to the
 * function in a2.
 */
	.globl	_proc_trampoline
_proc_trampoline:
	movl	a3,sp@-			| push function arg (curproc)
	jbsr	a2@			| call function
	addql	#4,sp			| pop arg
	movl	sp@(FR_SP),a0		| usp to a0
	movl	a0,usp			| setup user's stack pointer
	movml	sp@+,#0x7fff		| restore all but sp
	addql	#8,sp			| pop sp and stack adjust
	jra	rei			| all done


/*
 * Icode is copied out to process 1 to exec init.
 * If the exec fails, process 1 exits.
 */
	.globl	_icode,_szicode
	.text
_icode: 
	clrl	sp@-
	pea	pc@((argv-.)-2)
	pea	pc@((init-.)-2)
	clrl	sp@-
	moveq	#SYS_execve,d0
	trap	#0
	moveq	#SYS_exit,d0
	trap	#0
init:
	.asciz	"/sbin/init"
	.even
argv:   
	.long	init+6-_icode		| argv[0] = "init" ("/sbin/init" + 6)
	.long	eicode-_icode		| argv[1] follows icode after copyout
	.long	0
eicode:

_szicode:
	.long	_szicode-_icode


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
	.align	2
_sigcode:
	movl	sp@(12),a0		| signal handler addr	(4 bytes)
	jsr	a0@			| call signal handler	(2 bytes)
	addql	#4,sp			| pop signo		(2 bytes)
	trap	#1			| special syscall entry	(2 bytes)
	movl	d0,sp@(4)		| save errno		(4 bytes)
	moveq	#1,d0			| syscall == exit	(2 bytes)
	trap	#0			| exit(errno)		(2 bytes)
	.align	2
_esigcode:

/*
 * Primitives
 */ 

#include "m68k/asm.h"

/*
 * copypage(fromaddr, toaddr)
 *
 * Optimized version of bcopy for a single page-aligned NBPG byte copy.
 */
ENTRY(copypage)
	movl	sp@(4),a0		| source address
	movl	sp@(8),a1		| destination address
	movl	#NBPG/32,d0		| number of 32 byte chunks
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lmlloop			| no, use movl
Lm16loop:
	.long	0xf6209000		| move16 a0@+,a1@+
	.long	0xf6209000		| move16 a0@+,a1@+
	subql	#1,d0
	jne	Lm16loop
	rts
#endif
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
 * non-local gotos
 */
ENTRY(setjmp)
	movl	sp@(4),a0	| savearea pointer
	moveml	#0xFCFC,a0@	| save d2-d7/a2-a7
	movl	sp@,a0@(48)	| and return address
	moveq	#0,d0		| return 0
	rts

ENTRY(longjmp)
	movl	sp@(4),a0
	moveml	a0@+,#0xFCFC
	movl	a0@,sp@
	moveq	#1,d0
	rts

/*
 * The following primitives manipulate the run queues.
 * _whichqs tells which of the 32 queues _qs have processes in them.
 * Setrunqueue puts processes into queues, Remrq removes them from queues.
 * The running process is on no queue, other processes are on a queue
 * related to p->p_priority, divided by 4 actually to shrink the 0-127
 * range of priorities into the 32 available queues.
 */

	.globl	_whichqs,_qs,_cnt,_panic
	.globl	_curproc,_want_resched

/*
 * setrunqueue(p)
 *
 * Call should be made at spl6(), and p->p_stat should be SRUN
 */
ENTRY(setrunqueue)
	movl	sp@(4),a0
#ifdef DIAGNOSTIC
	tstl	a0@(P_BACK)
	jne	Lset1
	tstl	a0@(P_WCHAN)
	jne	Lset1
	cmpb	#SRUN,a0@(P_STAT)
	jne	Lset1
#endif
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
#ifdef DIAGNOSTIC
Lset1:
	movl	#Lset2,sp@-
	jbsr	_panic
Lset2:
	.asciz	"setrunqueue"
	.even
#endif

/*
 * Remrq(proc *p)
 *
 * Call should be made at spl6().
 */
ENTRY(remrq)
	movl	sp@(4),a0		| proc *p
	movb	a0@(P_PRIORITY),d0	| d0 = processes priority
#ifdef DIAGNOSTIC
	lsrb	#2,d0			| d0 /= 4
	movl	_whichqs,d1		| d1 = whichqs
	bclr	d0,d1			| clear bit in whichqs corresponding to
					|  processes priority
	jeq	Lrem2			| if (d1 & (1 << d0)) == 0
#endif
	movl	a0@(P_BACK),a1
	clrl	a0@(P_BACK)
	movl	a0@(P_FORW),a0
	movl	a0,a1@(P_FORW)
	movl	a1,a0@(P_BACK)
	cmpal	a0,a1
	jne	Lrem1
#ifndef DIAGNOSTIC
	lsrb	#2,d0
	movl	_whichqs,d1
#endif
	bclr	d0,d1
	movl	d1,_whichqs
Lrem1:
	rts
#ifdef DIAGNOSTIC
Lrem2:
	movl	#Lrem3,sp@-
	jbsr	_panic
Lrem3:
	.asciz	"remrq"
	.even
#endif

Lsw0:
	.asciz	"cpu_switch"
	.even

	.globl	_curpcb
	.globl	_masterpaddr	| XXX compatibility (debuggers)
	.data
_masterpaddr:			| XXX compatibility (debuggers)
_curpcb:
	.long	0
pcbflag:
	.byte	0		| copy of pcb_flags low byte
	.align	2
	.comm	nullpcb,SIZEOF_PCB
	.text

/*
 * switch_exit()
 * 	At the exit of a process, do a switch for the last time.
 * Switch to a safe stack and PCB, then deallocate the process's resources
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
Idle:
	stop	#PSL_LOWIPL
	movw	#PSL_HIGHIPL,sr
	movl	_whichqs,d0
	jeq	Idle
	jra	Lsw1

Lbadsw:
	movl	#Lsw0,sp@-
	jbsr	_panic
	/*NOTREACHED*/

/*
 * cpu_switch()
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
	movl	_curpcb,a0		| current pcb
	movw	sr,a0@(PCB_PS)		| save sr before changing ipl
#ifdef notyet
	movl	_curproc,sp@-		| remember last proc running
#endif /* notyet */
	clrl	_curproc

	/*
	 * Find the highest-priority queue that isn't empty,
	 * then take the first proc from that queue.
	 */
	movw	#PSL_HIGHIPL,sr		| lock out interrupts
	movl	_whichqs,d0
	jeq	Idle
Lsw1:
	movl	d0,d1
	negl	d0
	andl	d1,d0
	bfffo	d0{#0:#32},d1
	eorib	#31,d1

	movl	d1,d0
	lslb	#3,d1			| convert queue number to index
	addl	#_qs,d1			| locate queue (q)
	movl	d1,a1
	movl	a1@(P_FORW),a0		| p = q->p_forw
	cmpal	d1,a0			| anyone on queue?
	jeq	Lbadsw			| no, panic
	movl	a0@(P_FORW),a1@(P_FORW)	| q->p_forw = p->p_forw
	movl	a0@(P_FORW),a1		| n = p->p_forw
	movl	d1,a1@(P_BACK)		| n->p_back = q
	cmpal	d1,a1			| anyone left on queue?
	jne	Lsw2			| yes, skip
	movl	_whichqs,d1
	bclr	d0,d1			| no, reset bit
	movl	d1,_whichqs
Lsw2:
	movl	a0,_curproc
	clrl	_want_resched
#ifdef notyet
	movl	sp@+,a1
	cmpl	a0,a1			| switching to same proc?
	jeq	Lswdone			| yes, skip save and restore
#endif /* notyet */
	/*
	 * Save state of previous process in its pcb.
	 */
	movl	_curpcb,a1
	moveml	#0xFCFC,a1@(PCB_REGS)	| save non-scratch registers
	movl	usp,a2			| grab USP (a2 has been saved)
	movl	a2,a1@(PCB_USP)		| and save it

	tstl	_fpu_type		| Do we have an fpu?
	jeq	Lswnofpsave		| No?  Then don't attempt save.
	lea	a1@(PCB_FPCTX),a2	| pointer to FP save area
	fsave	a2@			| save FP state
	tstb	a2@			| null state frame?
	jeq	Lswnofpsave		| yes, all done
	fmovem	fp0-fp7,a2@(216)	| save FP general registers
	fmovem	fpcr/fpsr/fpi,a2@(312)	| save FP control registers
Lswnofpsave:

#ifdef DIAGNOSTIC
	tstl	a0@(P_WCHAN)
	jne	Lbadsw
	cmpb	#SRUN,a0@(P_STAT)
	jne	Lbadsw
#endif /* DIAGNOSTIC */
	clrl	a0@(P_BACK)		| clear back link
	movb	a0@(P_MD_FLAGS+3),pcbflag | low byte of p_md.md_flags
	movl	a0@(P_ADDR),a1		| get p_addr
	movl	a1,_curpcb

	/* see if pmap_activate needs to be called; should remove this */
	movl	a0@(P_VMSPACE),a0	| vmspace = p->p_vmspace
#ifdef DIAGNOSTIC
	tstl	a0			| map == VM_MAP_NULL?
	jeq	Lbadsw			| panic
#endif /* DIAGNOSTIC */
	lea	a0@(VM_PMAP),a0		| pmap = &vmspace.vm_pmap
	tstl	a0@(PM_STCHG)		| pmap->st_changed?
	jeq	Lswnochg		| no, skip
	pea	a1@			| push pcb (at p_addr)
	pea	a0@			| push pmap
	jbsr	_pmap_activate		| pmap_activate(pmap, pcb)
	addql	#8,sp
	movl	_curpcb,a1		| restore p_addr
Lswnochg:

	lea	tmpstk,sp		| now goto a tmp stack for NMI
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lres1a			| no, skip
	.word	0xf518			| yes, pflusha
	movl	a1@(PCB_USTP),d0	| get USTP
	moveq	#PGSHIFT,d1
	lsll	d1,d0			| convert to addr
	.long	0x4e7b0806		| movc d0,urp
	jra	Lcxswdone
Lres1a:
#endif
	movl	#CACHE_CLR,d0
	movc	d0,cacr			| invalidate cache(s)
	pflusha				| flush entire TLB
	movl	a1@(PCB_USTP),d0	| get USTP
	moveq	#PGSHIFT,d1
	lsll	d1,d0			| convert to addr
	lea	_protorp,a0		| CRP prototype
	movl	d0,a0@(4)		| stash USTP
	pmove	a0@,crp			| load new user root pointer
Lcxswdone:
	moveml	a1@(PCB_REGS),#0xFCFC	| and registers
	movl	a1@(PCB_USP),a0
	movl	a0,usp			| and USP

	tstl	_fpu_type		| If we don't have an fpu,
	jeq	Lnofprest		|  don't try to restore it.
	lea	a1@(PCB_FPCTX),a0	| pointer to FP save area
	tstb	a0@			| null state frame?
	jeq	Lresfprest		| yes, easy
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lresnot040		| no, skip
	clrl	sp@-			| yes...
	frestore sp@+			| ...magic!
Lresnot040:
#endif
	fmovem	a0@(312),fpcr/fpsr/fpi	| restore FP control registers
	fmovem	a0@(216),fp0-fp7	| restore FP general registers
Lresfprest:
	frestore a0@			| restore state

Lnofprest:
	movw	a1@(PCB_PS),sr		| no, restore PS
	moveq	#1,d0			| return 1 (for alternate returns)
	rts

/*
 * savectx(pcb)
 *	Update pcb, saving current processor state.
 */
ENTRY(savectx)
	movl	sp@(4),a1
	movw	sr,a1@(PCB_PS)
	movl	usp,a0			| grab USP
	movl	a0,a1@(PCB_USP)		| and save it
	moveml	#0xFCFC,a1@(PCB_REGS)	| save non-scratch registers

	tstl	_fpu_type		| Do we have FPU?
	jeq	Lsavedone		| No?  Then don't save state.
	lea	a1@(PCB_FPCTX),a0	| pointer to FP save area
	fsave	a0@			| save FP state
	tstb	a0@			| null state frame?
	jeq	Lsavedone		| yes, all done
	fmovem	fp0-fp7,a0@(216)	| save FP general registers
	fmovem	fpcr/fpsr/fpi,a0@(312)	| save FP control registers
Lsavedone:
	moveq	#0,d0			| return 0
	rts

#if defined(M68040)
ENTRY(suline)
	movl	sp@(4),a0		| address to write
	movl	_curpcb,a1		| current pcb
	movl	#Lslerr,a1@(PCB_ONFAULT) | where to return to on a fault
	movl	sp@(8),a1		| address of line
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	movl	a1@+,d0			| get lword
	movsl	d0,a0@+			| put lword
	nop				| sync
	moveq	#0,d0			| indicate no fault
	jra	Lsldone
Lslerr:
	moveq	#-1,d0
Lsldone:
	movl	_curpcb,a1		| current pcb
	clrl	a1@(PCB_ONFAULT)	| clear fault address
	rts
#endif

/*
 * Invalidate entire TLB.
 */
ENTRY(TBIA)
__TBIA:
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lmotommu3		| no, skip
	.word	0xf518			| yes, pflusha
	rts
Lmotommu3:
#endif
	pflusha
	tstl	_mmutype
	jgt	Ltbia851
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
Ltbia851:
	rts

/*
 * Invalidate any TLB entry for given VA (TB Invalidate Single)
 */
ENTRY(TBIS)
#ifdef DEBUG
	tstl	fulltflush		| being conservative?
	jne	__TBIA			| yes, flush entire TLB
#endif
	movl	sp@(4),a0
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lmotommu4		| no, skip
	movc	dfc,d1
	moveq	#FC_USERD, d0		| user space
	movc	d0, dfc
	.word	0xf508			| pflush a0@
	moveq	#FC_SUPERD,d0		| supervisor space
	movc	d0, dfc
	.word	0xf508			| pflush a0@
	movc	d1,dfc
	rts
Lmotommu4:
#endif
	tstl	_mmutype
	jgt	Ltbis851
	pflush	#0,#0,a0@		| flush address from both sides
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip data cache
	rts
Ltbis851:
	pflushs	#0,#0,a0@		| flush address from both sides
	rts

/*
 * Invalidate supervisor side of TLB
 */
ENTRY(TBIAS)
#ifdef DEBUG
	tstl	fulltflush		| being conservative?
	jne	__TBIA			| yes, flush everything
#endif
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lmotommu5		| no, skip
	.word	0xf518			| yes, pflusha (for now) XXX
	rts
Lmotommu5:
#endif
	tstl	_mmutype
	jgt	Ltbias851
	pflush	#4,#4			| flush supervisor TLB entries
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts
Ltbias851:
	pflushs	#4,#4			| flush supervisor TLB entries
	rts

/*
 * Invalidate user side of TLB
 */
ENTRY(TBIAU)
#ifdef DEBUG
	tstl	fulltflush		| being conservative?
	jne	__TBIA			| yes, flush everything
#endif
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lmotommu6		| no, skip
	.word	0xf518			| yes, pflusha (for now) XXX
Lmotommu6:
#endif
	tstl	_mmutype
	jgt	Ltbiau851
	pflush	#0,#4			| flush user TLB entries
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts
Ltbiau851:
	pflush	#0,#4			| flush user TLB entries
	rts

/*
 * Invalidate instruction cache
 */
ENTRY(ICIA)
#if defined(M68040)
ENTRY(ICPA)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lmotommu7		| no, skip
	.word	0xf498			| cinva ic
	rts
Lmotommu7:
#endif
	movl	#IC_CLEAR,d0
	movc	d0,cacr			| invalidate i-cache
	rts

/*
 * Invalidate data cache.
 * NOTE: we do not flush 68030 on-chip cache as there are no aliasing
 * problems with DC_WA.  The only cases we have to worry about are context
 * switch and TLB changes, both of which are handled "in-line" in resume
 * and TBI*.
 */
ENTRY(DCIA)
__DCIA:
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lmotommu8		| no, skip
	.word	0xf478			| cpusha dc
Lmotommu8:
#endif
	rts

ENTRY(DCIS)
__DCIS:
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	Lmotommu9		| no, skip
	.word	0xf478			| cpusha dc
Lmotommu9:
#endif
	rts

ENTRY(DCIU)
__DCIU:
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	LmotommuA		| no, skip
	.word	0xf478			| cpusha dc
LmotommuA:
#endif
	rts

#ifdef M68040
ENTRY(ICPL)	/* invalidate instruction physical cache line */
	movl	sp@(4),a0		| address
	.word	0xf488			| cinvl ic,a0@
	rts
ENTRY(ICPP)	/* invalidate instruction physical cache page */
	movl	sp@(4),a0		| address
	.word	0xf490			| cinvp ic,a0@
	rts
ENTRY(DCPL)	/* invalidate data physical cache line */
	movl	sp@(4),a0		| address
	.word	0xf448			| cinvl dc,a0@
	rts
ENTRY(DCPP)	/* invalidate data physical cache page */
	movl	sp@(4),a0		| address
	.word	0xf450			| cinvp dc,a0@
	rts
ENTRY(DCPA)	/* invalidate instruction physical cache line */
	.word	0xf458			| cinva dc
	rts
ENTRY(DCFL)	/* data cache flush line */
	movl	sp@(4),a0		| address
	.word	0xf468			| cpushl dc,a0@
	rts
ENTRY(DCFP)	/* data cache flush page */
	movl	sp@(4),a0		| address
	.word	0xf470			| cpushp dc,a0@
	rts
#endif /* M68040 */

ENTRY(PCIA)
#if defined(M68040)
ENTRY(DCFA)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	LmotommuB		| no, skip
	.word	0xf478			| cpusha dc
	rts
LmotommuB:
#endif
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts

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
	movl	sp,d0			| get current SP
	addql	#4,d0			| compensate for return address
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
	.word	0xf548		| ptestw (a0)
	moveq	#FC_USERD,d0		| restore DFC to user space
	movc	d0,dfc
	.word	0x4e7a,0x0805	| movec  MMUSR,d0
	rts

/*
 * Load a new user segment table pointer.
 */
ENTRY(loadustp)
	movl	sp@(4),d0		| new USTP
	moveq	#PGSHIFT,d1
	lsll	d1,d0			| convert to addr
#if defined(M68040)
	cmpl	#MMU_68040,_mmutype	| 68040?
	jne	LmotommuC		| no, skip
	.long	0x4e7b0806		| movec d0, URP
	rts
LmotommuC:
#endif
	pflusha
	lea	_protorp,a0		| CRP prototype
	movl	d0,a0@(4)		| stash USTP
	pmove	a0@,crp			| load root pointer
	movl	#DC_CLEAR,d0
	movc	d0,cacr			| invalidate on-chip d-cache
	rts				|   since pmove flushes TLB

/*
 * Set processor priority level calls.  Most are implemented with
 * inline asm expansions.  However, spl0 requires special handling
 * as we need to check for our emulated software interrupts.
 */

ALTENTRY(splnone, _spl0)
ENTRY(spl0)
	moveq	#0,d0
	movw	sr,d0			| get old SR for return
	movw	#PSL_LOWIPL,sr		| restore new SR
	tstb	_ssir			| software interrupt pending?
	jeq	Lspldone		| no, all done
	subql	#4,sp			| make room for RTE frame
	movl	sp@(4),sp@(2)		| position return address
	clrw	sp@(6)			| set frame type 0
	movw	#PSL_LOWIPL,sp@		| and new SR
	jra	Lgotsir			| go handle it
Lspldone:
	rts

ENTRY(_insque)
	movw	sr,d0
	movw	#PSL_HIGHIPL,sr		| atomic
	movl	sp@(8),a0		| where to insert (after)
	movl	sp@(4),a1		| element to insert (e)
	movl	a0@,a1@			| e->next = after->next
	movl	a0,a1@(4)		| e->prev = after
	movl	a1,a0@			| after->next = e
	movl	a1@,a0
	movl	a1,a0@(4)		| e->next->prev = e
	movw	d0,sr
	rts

ENTRY(_remque)
	movw	sr,d0
	movw	#PSL_HIGHIPL,sr		| atomic
	movl	sp@(4),a0		| element to remove (e)
	movl	a0@,a1
	movl	a0@(4),a0
	movl	a0,a1@(4)		| e->next->prev = e->prev
	movl	a1,a0@			| e->prev->next = e->next
	movw	d0,sr
	rts

/*
 * Save and restore 68881 state.
 * Pretty awful looking since our assembler does not
 * recognize FP mnemonics.
 */
ENTRY(m68881_save)
	movl	sp@(4),a0		| save area pointer
	fsave	a0@			| save state
	tstb	a0@			| null state frame?
	jeq	Lm68881sdone		| yes, all done
	fmovem fp0-fp7,a0@(216)		| save FP general registers
	fmovem fpcr/fpsr/fpi,a0@(312)	| save FP control registers
Lm68881sdone:
	rts

ENTRY(m68881_restore)
	movl	sp@(4),a0		| save area pointer
	tstb	a0@			| null state frame?
	jeq	Lm68881rdone		| yes, easy
	fmovem	a0@(312),fpcr/fpsr/fpi	| restore FP control registers
	fmovem	a0@(216),fp0-fp7	| restore FP general registers
Lm68881rdone:
	frestore a0@			| restore state
	rts

/*
 * Handle the nitty-gritty of rebooting the machine.
 * Basically we just turn off the MMU and jump to the appropriate ROM routine.
 * Note that we must be running in an address range that is mapped one-to-one
 * logical to physical so that the PC is still valid immediately after the MMU
 * is turned off.  We have conveniently mapped the last page of physical
 * memory this way.
 */
	.globl	_doboot, _ROMBase
_doboot:
	movw	#PSL_HIGHIPL,sr		| no interrupts
	movl	#CACHE_OFF,d0
	movc	d0,cacr			| disable on-chip cache(s)

	movl	_MacOSROMBase, _ROMBase	| Load MacOS ROMBase

	movl	#0x90,a1		| offset of ROM reset routine
	addl	_ROMBase,a1		| add to ROM base
	jra	a1@			| and jump to ROM to reset machine

/*
 * LAK: (7/24/94) This routine was added so that the
 *  C routine that runs at startup can figure out how MacOS
 *  had mapped memory.  We want to keep the same mapping so
 *  that when we set our MMU pointer, the PC doesn't point
 *  in the middle of nowhere.
 *
 * long get_pte(void *addr, unsigned long pte[2], unsigned short *psr)
 *
 *  Takes "addr" and looks it up in the current MMU pages.  Puts
 *  the PTE of that address in "pte" and the result of the
 *  search in "psr".  "pte" should be 2 longs in case it is
 *  a long-format entry.
 *
 *  One possible problem here is that setting the tt register
 *  may screw something up if, say, the address returned by ptest
 *  in a0 has msb of 0.
 *
 *  Returns -1 on error, 0 if pte is a short-format pte, or
 *  1 if pte is a long-format pte.
 *
 *  Be sure to only call this routine if the MMU is enabled.  This
 *  routine is probably more general than it needs to be -- it
 *  could simply return the physical address (replacing
 *  get_physical() in machdep).
 *
 *  "gas" does not understand the tt0 register, so we must hand-
 *  assemble the instructions.
 */
	.globl	_get_pte
_get_pte:
	addl	#-4,sp		| make temporary space
	movl	sp@(8),a0	| logical address to look up
	movl	#0,a1		| clear in case of failure
	ptestr	#1,a0@,#7,a1	| search for logical address
	pmove	psr,sp@		| store processor status register
	movw	sp@,d1
	movl	sp@(16),a0	| where to store the psr
	movw	d1,a0@		| send back to caller
	andw	#0xc400,d1	| if bus error, exceeded limit, or invalid
	jne	get_pte_fail1	| leave now
	tstl	a1		| check address we got back
	jeq	get_pte_fail2	| if 0, then was not set -- fail

	| enable tt0
	movl	a1,d0
	movl	d0,pte_tmp	| save for later
	andl	#0xff000000,d0	| keep msb
	orl	#0x00008707,d0	| enable tt for reading and writing
	movl	d0,longscratch
	lea	longscratch,a0
	.long	0xf0100800	| pmove a0@,tt0

	| send first long back to user
	movl	sp@(12),a0	| address of where to put pte
	movl	a1@,d0		|
	movl	d0,a0@		| first long

	andl	#3,d0		| dt bits of pte
	cmpl	#1,d0		| should be 1 if page descriptor
	jne	get_pte_fail3	| if not, get out now

	movl	sp@(16),a0	| addr of stored psr
	movw	a0@,d0		| get psr again
	andw	#7,d0		| number of levels it found
	addw	#-1,d0		| find previous level
	movl	sp@(8),a0	| logical address to look up
	movl	#0,a1		| clear in case of failure

	cmpl	#0,d0
	jeq	pte_level_zero
	cmpl	#1,d0
	jeq	pte_level_one
	cmpl	#2,d0
	jeq	pte_level_two
	cmpl	#3,d0
	jeq	pte_level_three
	cmpl	#4,d0
	jeq	pte_level_four
	cmpl	#5,d0
	jeq	pte_level_five
	cmpl	#6,d0
	jeq	pte_level_six
	jra	get_pte_fail4	| really should have been one of these...

pte_level_zero:
	| must get CRP to get length of entries at first level
	lea	longscratch,a0	| space for two longs
	pmove	crp,a0@		| save root pointer
	movl	a0@,d0		| load high long
	jra	pte_got_parent
pte_level_one:
	ptestr	#1,a0@,#1,a1	| search for logical address
	pmove	psr,sp@		| store processor status register
	movw	sp@,d1
	jra	pte_got_it
pte_level_two:
	ptestr	#1,a0@,#2,a1	| search for logical address
	pmove	psr,sp@		| store processor status register
	movw	sp@,d1
	jra	pte_got_it
pte_level_three:
	ptestr	#1,a0@,#3,a1	| search for logical address
	pmove	psr,sp@		| store processor status register
	movw	sp@,d1
	jra	pte_got_it
pte_level_four:
	ptestr	#1,a0@,#4,a1	| search for logical address
	pmove	psr,sp@		| store processor status register
	movw	sp@,d1
	jra	pte_got_it
pte_level_five:
	ptestr	#1,a0@,#5,a1	| search for logical address
	pmove	psr,sp@		| store processor status register
	movw	sp@,d1
	jra	pte_got_it
pte_level_six:
	ptestr	#1,a0@,#6,a1	| search for logical address
	pmove	psr,sp@		| store processor status register
	movw	sp@,d1

pte_got_it:
	andw	#0xc400,d1	| if bus error, exceeded limit, or invalid
	jne	get_pte_fail5	| leave now
	tstl	a1		| check address we got back
	jeq	get_pte_fail6	| if 0, then was not set -- fail

	| change tt0
	movl	a1,d0
	andl	#0xff000000,d0	| keep msb
	orl	#0x00008707,d0	| enable tt for reading and writing
	movl	d0,longscratch
	lea	longscratch,a0
	.long	0xF0100800	| pmove a0@,tt0

	movl	a1@,d0		| get pte of parent
	movl	d0,_macos_tt0	| XXX for later analysis (kill this line)
pte_got_parent:
	andl	#3,d0		| dt bits of pte
	cmpl	#2,d0		| child is short-format descriptor
	jeq	short_format
	cmpl	#3,d0		| child is long-format descriptor
	jne	get_pte_fail7

	| long_format -- we must go back, change the tt, and get the
	|  second long.  The reason we didn't do this in the first place
	|  is that the first long might have been the last long of RAM.

	movl	pte_tmp,a1	| get address of our original pte
	addl	#4,a1		| address of ite second long

	| change tt0 back
	movl	a1,d0
	andl	#0xff000000,d0	| keep msb
	orl	#0x00008707,d0	| enable tt for reading and writing
	movl	d0,longscratch
	lea	longscratch,a0
	.long	0xF0100800	| pmove a0@,tt0

	movl	sp@(12),a0	| address of return pte
	movl	a1@,a0@(4)	| write in second long

	movl	#1,d0		| return long-format
	jra	get_pte_success

short_format:
	movl	#0,d0		| return short-format
	jra	get_pte_success

get_pte_fail:
	movl	#-1,d0		| return failure

get_pte_success:
	clrl	d1		| disable tt
	movl	d1,longscratch
	lea	longscratch,a0
	.long	0xF0100800	| pmove a0@,tt0

	addl	#4,sp		| return temporary space
	rts

get_pte_fail1:
	jbsr	_printstar
	jra	get_pte_fail
get_pte_fail2:
	jbsr	_printstar
	jbsr	_printstar
	jra	get_pte_fail
get_pte_fail3:
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jra	get_pte_fail
get_pte_fail4:
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jra	get_pte_fail
get_pte_fail5:
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jra	get_pte_fail
get_pte_fail6:
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jra	get_pte_fail
get_pte_fail7:
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jra	get_pte_fail
get_pte_fail8:
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jra	get_pte_fail
get_pte_fail9:
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jra	get_pte_fail
get_pte_fail10:
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jbsr	_printstar
	jra	get_pte_fail

	.data
	.globl	_sanity_check
_sanity_check:
	.long	0x18621862	| this is our stack overflow checker.

	.space	4 * NBPG
tmpstk:
	.globl	_machineid
_machineid:
	.long	0		| default to 320
	.globl	_mmutype,_protorp
_mmutype:
	.long	0		| Are we running 68851, 68030, or 68040?
_protorp:
	.long	0,0		| prototype root pointer
	.globl	_cold
_cold:
	.long	1		| cold start flag
	.globl	_proc0paddr
_proc0paddr:
	.long	0		| KVA of proc0 u-area
	.globl	_intiolimit
_intiolimit:
	.long	0		| KVA of end of internal IO space
	.globl	_load_addr
_load_addr:
	.long	0		| Physical address of kernel
lastpage:
	.long	0		| LAK: to store the addr of last page in mem
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
	.globl	_intrcnt,_intrnames,_eintrcnt,_eintrnames
_intrnames:
	.asciz	"spur"
	.asciz	"via1"
	.asciz	"via2"
	.asciz	"scc"
	.asciz	"nmi"
	.asciz	"clock"
_eintrnames:
	.even
_intrcnt:
	.long	0,0,0,0,0,0
_eintrcnt:			| Unused, but needed for vmstat
	.long	0
	.globl	_MacOSROMBase
_MacOSROMBase:
	.long	0x40800000
	.globl	_mac68k_vrsrc_cnt, _mac68k_vrsrc_vec
_mac68k_vrsrc_cnt:
	.long	0
_mac68k_vrsrc_vec:
	.word	0, 0, 0, 0, 0, 0
_mac68k_buserr_addr:
	.long	0
_locore_dodebugmarks:
	.long	0
