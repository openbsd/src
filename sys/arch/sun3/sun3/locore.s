/*	$NetBSD: locore.s,v 1.34 1995/12/11 02:38:13 thorpej Exp $	*/

/*
 * Copyright (c) 1994, 1995 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1980, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	from: Utah $Hdr: locore.s 1.66 92/12/22$
 *	@(#)locore.s	8.6 (Berkeley) 5/27/94
 */

#include "assym.s"
#include <machine/trap.h>

| Remember this is a fun project.  (Thanks, Adam.  I try! 8^)

| This is for kvm_mkdb, and should be the address of the beginning
| of the kernel text segment (not necessarily the same as kernbase).
	.text
	.globl	_kernel_text
_kernel_text:

| This is the entry point, as well as the end of the temporary stack
| used during process switch (one 8K page ending at start)
	.globl tmpstk
tmpstk:
	.globl start
start:
| First we need to set it up so we can access the sun MMU, and be otherwise
| undisturbed.  Until otherwise noted, all code must be position independent
| as the boot loader put us low in memory, but we are linked high.
	movw	#PSL_HIGHIPL, sr	| no interrupts
	moveq	#FC_CONTROL, d0		| make movs access "control"
	movc	d0, sfc			| space where the sun3 designers
	movc	d0, dfc			| put all the "useful" stuff

| Set context zero and stay there until pmap_bootstrap.
	moveq	#0, d0
	movsb	d0, CONTEXT_REG

| In order to "move" the kernel to high memory, we are going to copy the
| first 4 Mb of pmegs such that we will be mapped at the linked address.
| This is all done by copying in the segment map (top-level MMU table).
| We will unscramble which PMEGs we actually need later.

	movl	#(SEGMAP_BASE+0), a0		| src
	movl	#(SEGMAP_BASE+KERNBASE), a1	| dst
	movl	#(0x400000/NBSG), d0		| count

L_per_pmeg:
	movsb	a0@, d1			| copy segmap entry
	movsb	d1, a1@
	addl	#NBSG, a0		| increment pointers
	addl	#NBSG, a1
	subql	#1, d0			| decrement count
	bgt	L_per_pmeg

| Kernel is now double mapped at zero and KERNBASE.
| Force a long jump to the relocated code (high VA).

	movl	#IC_CLEAR, d0		| Flush the I-cache
	movc	d0, cacr
	jmp L_high_code:l		| long jump

L_high_code:
| We are now running in the correctly relocated kernel, so
| we are no longer restricted to position-independent code.

| Do bootstrap stuff needed before main() gets called.
| Our boot loader leaves a copy of the kernel's exec header
| just before the start of the kernel text segment, so the
| kernel can sanity-check the DDB symbols at [end...esym].
| Pass the struct exec at tmpstk-32 to sun3_bootstrap().
	lea	tmpstk-32, sp
	jsr	_sun3_bootstrap

| Now that sun3_bootstrap is done using the PROM setcxsegmap
| we can safely set the sfc/dfc to something != FC_CONTROL
	moveq	#FC_USERD, d0		| make movs access "user data"
	movc	d0, sfc			| space for copyin/copyout
	movc	d0, dfc

| Setup process zero user/kernel stacks.
	movl	_proc0paddr,a1		| get proc0 pcb addr
	lea	a1@(USPACE-4),sp	| set SSP to last word
	movl	#USRSTACK-4,a2
	movl	a2,usp			| init user SP

| Note curpcb was already set in sun3_bootstrap.
| Will do fpu initialization during autoconfig (see fpu.c)
| The interrupt vector table and stack are now ready.
	movw	#PSL_LOWIPL,sr		| lower SPL

/*
 * Final preparation for calling main.
 *
 * Create a fake exception frame that returns to user mode,
 * and save its address in p->p_md.md_regs for cpu_fork().
 * The new frames for process 1 and 2 will be adjusted by
 * cpu_set_kpc() to arrange for a call to a kernel function
 * before the new process does its rte out to user mode.
 */
  	clrw	sp@-			| vector offset/frame type
	clrl	sp@-			| PC - filled in by "execve"
  	movw	#PSL_USER,sp@-		| in user mode
	clrl	sp@-			| stack adjust count and padding
	lea	sp@(-64),sp		| construct space for D0-D7/A0-A7
	lea	_proc0,a0		| proc0 in a0
	movl	sp,a0@(P_MDREGS)	| save frame for proc0
	movl	usp,a1
	movl	a1,sp@(FR_SP)		| save user stack pointer in frame
	jbsr	_main			| main()
	trap	#15			| should not get here

| This is used by cpu_fork() to return to user mode.
| It is called with SP pointing to a struct trapframe.
	.globl	_proc_do_uret
_proc_do_uret:
	movl	sp@(FR_SP),a0		| grab and load
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| load most registers (all but SSP)
	addql	#8,sp			| pop SSP and stack adjust count
  	rte

/*
 * proc_trampoline:
 * This is used by cpu_set_kpc() to "push" a function call onto the
 * kernel stack of some process, very much like a signal delivery.
 * When we get here, the stack has:
 *
 * SP+8:	switchframe from before cpu_set_kpc
 * SP+4:	void *proc;
 * SP:  	u_long func;
 *
 * On entry, the switchframe pushed by cpu_set_kpc has already been
 * popped off the stack, so all this needs to do is pop the function
 * pointer into a register, call it, then pop the arg, and finally
 * return using the switchframe that remains on the stack.
 */
	.globl	_proc_trampoline
_proc_trampoline:
	movl	sp@+,a0			| function pointer
	jbsr	a0@			| (*func)(procp)
	addql	#4,sp			| toss the arg
	rts				| as cpu_switch would do

| That is all the assembly startup code we need on the sun3!
| The rest of this is like the hp300/locore.s where possible.

/*
 * Trap/interrupt vector routines
 */

	.globl _buserr, _addrerr, _illinst, _zerodiv, _chkinst
	.globl _trapvinst, _privinst, _trace, _badtrap, _fmterr
	.globl _trap0, _trap1, _trap2, _trap12, _trap15
	.globl _coperr, _fpfline, _fpunsupp

	.globl	_trap, _nofault, _longjmp
_buserr:
	tstl	_nofault		| device probe?
	jeq	_addrerr		| no, handle as usual
	movl	_nofault,sp@-		| yes,
	jbsr	_longjmp		|  longjmp(nofault)
_addrerr:
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	lea	sp@(FR_HW),a1		| grab base of HW berr frame
	moveq	#0,d0
	movw	a1@(10),d0		| grab SSW for fault processing
	btst	#12,d0			| RB set?
	jeq	LbeX0			| no, test RC
	bset	#14,d0			| yes, must set FB
	movw	d0,a1@(10)		| for hardware too
LbeX0:
	btst	#13,d0			| RC set?
	jeq	LbeX1			| no, skip
	bset	#15,d0			| yes, must set FC
	movw	d0,a1@(10)		| for hardware too
LbeX1:
	btst	#8,d0			| data fault?
	jeq	Lbe0			| no, check for hard cases
	movl	a1@(16),d1		| fault address is as given in frame
	jra	Lbe10			| thats it
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

/*
 * the sun3 specific code
 *	
 * our mission: figure out whether what we are looking at is
 *              bus error in the UNIX sense, or
 *	        a memory error i.e a page fault
 *
 * [this code replaces similarly mmu specific code in the hp300 code]	
 */
sun3_mmu_specific:
	clrl d0				| make sure top bits are cleard too
	movl d1, sp@-			| save d1
	movc sfc, d1			| save sfc to d1
	moveq #FC_CONTROL, d0		| sfc = FC_CONTROL
	movc d0, sfc
	movsb BUSERR_REG, d0		| get value of bus error register
	movc d1, sfc			| restore sfc
	movl sp@+, d1			| restore d1
	andb #BUSERR_MMU, d0 		| is this an MMU fault?
	jeq Lisberr			| non-MMU bus error
/* End of sun3 specific code. */

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
	addql	#8,sp			| toss SSP and stkadj
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
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save registers
	moveq	#T_FPEMULI,d0		| denote as FP emulation trap
	jra	fault			| do it

_fpunsupp:
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save registers
	moveq	#T_FPEMULD,d0		| denote as FP emulation trap
	jra	fault			| do it

/*
 * Handles all other FP coprocessor exceptions.
 * Note that since some FP exceptions generate mid-instruction frames
 * and may cause signal delivery, we need to test for stack adjustment
 * after the trap call.
 */
	.globl	_fpfault
_fpfault:
	clrl	sp@-		| stack adjust count
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
	fmovem	fpsr,sp@-	| push fpsr as code argument
	frestore a0@		| restore state
	movl	#T_FPERR,sp@-	| push type arg
	jra	Ltrapnstkadj	| call trap and deal with stack cleanup

/*
 * Coprocessor and format errors can generate mid-instruction stack
 * frames and cause signal delivery hence we need to check for potential
 * stack adjustment.
 */
_coperr:
	clrl	sp@-		| stack adjust count
	moveml	#0xFFFF,sp@-
	movl	usp,a0		| get and save
	movl	a0,sp@(FR_SP)	|   the user stack pointer
	clrl	sp@-		| no VA arg
	clrl	sp@-		| or code arg
	movl	#T_COPERR,sp@-	| push trap type
	jra	Ltrapnstkadj	| call trap and deal with stack adjustments

_fmterr:
	clrl	sp@-		| stack adjust count
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
	addql	#8,sp			| pop SP and stack adjust
	jra	rei			| all done

	.globl	_straytrap
_badtrap:
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save std frame regs
	jbsr	_straytrap		| report
	moveml	sp@+,#0xFFFF		| restore regs
	addql	#4, sp			| stack adjust count
	jra	rei			| all done

	.globl	_syscall
_trap0:
	clrl	sp@-			| stack adjust count
	moveml	#0xFFFF,sp@-		| save user registers
	movl	usp,a0			| save the user SP
	movl	a0,sp@(FR_SP)		|   in the savearea
	movl	d0,sp@-			| push syscall number
	jbsr	_syscall		| handle it
	addql	#4,sp			| pop syscall arg
	movl	sp@(FR_SP),a0		| grab and restore
	movl	a0,usp			|   user SP
	moveml	sp@+,#0x7FFF		| restore most registers
	addql	#8,sp			| pop SP and stack adjust
	jra	rei			| all done

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
	jeq	fault			| no, regular fault
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
 * Interrupt handlers.  Most are auto-vectored,
 * and hard-wired the same way on all sun3 models.
 */

#define INTERRUPT_SAVEREG \
	moveml	#0xC0C0,sp@-

#define INTERRUPT_RESTORE \
	moveml	sp@+,#0x0303

.align 4
/*
 * This is the common auto-vector interrupt handler,
 * for which the CPU provides the vector=0x18+level.
 * These are installed in the interrupt vector table.
 */
	.globl	__isr_autovec
__isr_autovec:
	INTERRUPT_SAVEREG
	movw	sp@(22),sp@-		| push exception vector info
	clrw	sp@-
	jbsr	_isr_autovec		| C dispatcher
	addql	#4,sp
	INTERRUPT_RESTORE
	jra rei			/* XXX - Just do rte here? */

/* clock: see clock.c */
.globl __isr_clock, _interrupt_reg, _clock_intr, _clock_va
.align 4
__isr_clock:
	INTERRUPT_SAVEREG 	| save a0, a1, d0, d1
	movl	_clock_va, a0
	movl	_interrupt_reg, a1
	tstb a0@(INTERSIL_INTR_OFFSET)
	andb #~IREG_CLOCK_ENAB_5, a1@
	orb #IREG_CLOCK_ENAB_5, a1@
	tstb a0@(INTERSIL_INTR_OFFSET)
| used to have "timebomb" check here...
	lea	sp@(16),a1		| a1 = &clockframe
	movl	a1,sp@-
	jbsr	_clock_intr
	addql	#4,sp
	INTERRUPT_RESTORE
	jra	rei

| Handler for all vectored interrupts (i.e. VME interrupts)
	.globl	_isr_vectored
	.globl	__isr_vectored
__isr_vectored:
	INTERRUPT_SAVEREG
	movw	sp@(22),sp@-		| push exception vector info
	clrw	sp@-
	jbsr	_isr_vectored		| C dispatcher
	addql	#4,sp			|
	INTERRUPT_RESTORE
	jra	rei			| all done


#undef	INTERRUPT_SAVEREG
#undef	INTERRUPT_RESTORE

/* interrupt counters (needed by vmstat) */
	.globl	_intrcnt,_eintrcnt,_intrnames,_eintrnames
_intrnames:
	.asciz	"spur"	| 0
	.asciz	"lev1"	| 1
	.asciz	"lev2"	| 2
	.asciz	"lev3"	| 3
	.asciz	"lev4"	| 4
	.asciz	"clock"	| 5
	.asciz	"lev6"	| 6
	.asciz	"nmi"	| 7
_eintrnames:

	.data
	.even
_intrcnt:
	.long	0,0,0,0,0,0,0,0,0,0
_eintrcnt:
	.text

/*
 * Emulation of VAX REI instruction.
 *
 * This code is (mostly) un-altered from the hp300 code,
 * except that sun machines do not need a simulated SIR
 * because they have a real software interrupt register.
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

	.globl	_astpending
	.globl	rei
rei:
#ifdef	DIAGNOSTIC
	tstl	_panicstr		| have we paniced?
	jne	Ldorte			| yes, do not make matters worse
#endif
	tstl	_astpending		| AST pending?
	jeq	Ldorte			| no, done
Lrei1:
	btst	#5,sp@			| yes, are we returning to user mode?
	jne	Ldorte			| no, done
	movw	#PSL_LOWIPL,sr		| lower SPL
	clrl	sp@-			| stack adjust
	moveml	#0xFFFF,sp@-		| save all registers
	movl	usp,a1			| including
	movl	a1,sp@(FR_SP)		|    the users SP
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
	rte				| and do real RTE
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
Ldorte:
	rte				| real return

/*
 * Initialization is at the beginning of this file, because the
 * kernel entry point needs to be at zero for compatibility with
 * the Sun boot loader.  This works on Sun machines because the
 * interrupt vector table for reset is NOT at address zero.
 * (The MMU has a "boot" bit that forces access to the PROM)
 */

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
_sigcode:	/* Found at address: 0x0DFFffdc */
	movl	sp@(12),a0		| signal handler addr	(4 bytes)
	jsr	a0@			| call signal handler	(2 bytes)
	addql	#4,sp			| pop signo		(2 bytes)
	trap	#1			| special syscall entry	(2 bytes)
	movl	d0,sp@(4)		| save errno		(4 bytes)
	moveq	#1,d0			| syscall == exit	(2 bytes)
	trap	#0			| exit(errno)		(2 bytes)
	.align	2
_esigcode:
	.text

/* XXX - hp300 still has icode here... */

/*
 * Primitives
 */
#include <machine/asm.h>

/* XXX copypage(fromaddr, toaddr) */

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
 * Setrunqueue puts processes into queues, Remrunqueue removes them
 * from queues.  The running process is on no queue, other processes
 * are on a queue related to p->p_priority, divided by 4 actually to
 * shrink the 0-127 range of priorities into the 32 available queues.
 */

	.globl	_whichqs,_qs,_cnt,_panic
	.globl	_curproc
	.comm	_want_resched,4

/*
 * setrunqueue(p)
 *
 * Call should be made at splclock(), and p->p_stat should be SRUN
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
 * remrq(p)
 *
 * Call should be made at splclock().
 */
ENTRY(remrq)
	movl	sp@(4),a0		| proc *p
	clrl	d0
	movb	a0@(P_PRIORITY),d0
	lsrb	#2,d0
	movl	_whichqs,d1
	bclr	d0,d1			| if ((d1 & (1 << d0)) == 0)
	jeq	Lrem2			|   panic (empty queue)
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
	jeq	Lrem1
	movl	_whichqs,d1
	bset	d0,d1
	movl	d1,_whichqs
Lrem1:
	clrl	a0@(P_BACK)
	rts
Lrem2:
	movl	#Lrem3,sp@-
	jbsr	_panic
Lrem3:
	.asciz	"remrq"


| Message for Lbadsw panic
Lsw0:
	.asciz	"cpu_switch"
	.even

	.globl	_curpcb
	.globl	_masterpaddr	| XXX compatibility (debuggers)
	.data
_masterpaddr:			| XXX compatibility (debuggers)
_curpcb:
	.long	0
mdpflag:
	.byte	0		| copy of proc md_flags low byte
	.align	2
	.comm	nullpcb,SIZEOF_PCB
	.text

/*
 * At exit of a process, do a cpu_switch for the last time.
 * Switch to a safe stack and PCB, and deallocate the process's resources.
 * The ipl is high enough to prevent the memory from being reallocated.
 */
ENTRY(switch_exit)
	movl	sp@(4),a0		| struct proc *p
	movl	#nullpcb,_curpcb	| save state into garbage pcb
	lea	tmpstk,sp		| goto a tmp stack
	movl	a0,sp@-			| pass proc ptr down

	/* Free old process's u-area. */
	movl	#USPACE,sp@-		| size of u-area
	movl	a0@(P_ADDR),sp@-	| address of process's u-area
	movl	_kernel_map,sp@-	| map it was allocated in
	jbsr	_kmem_free		| deallocate it
	lea	sp@(12),sp		| pop args

	jra	_cpu_switch

/*
 * When no processes are on the runq, cpu_switch() branches to idle
 * to wait for something to come ready.
 */
	.data
	.globl _Idle_count
_Idle_count:
	.long   0
	.text

	.globl	Idle
Lidle:
	stop	#PSL_LOWIPL
Idle:
	movw	#PSL_HIGHIPL,sr
	addql   #1, _Idle_count
	tstl	_whichqs
	jeq	Lidle
	movw	#PSL_LOWIPL,sr
	jra	Lsw1

Lbadsw:
	movl	#Lsw0,sp@-
	jbsr	_panic
	/*NOTREACHED*/

/*
 * cpu_switch()
 * Hacked for sun3	
 * XXX - Arg 1 is a proc pointer (curproc) but this doesn't use it.
 * XXX - Sould we use p->p_addr instead of curpcb? -gwr
 */
ENTRY(cpu_switch)
	movl	_curpcb,a1		| current pcb
	movw	sr,a1@(PCB_PS)		| save sr before changing ipl
#ifdef notyet
	movl	_curproc,sp@-		| remember last proc running
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
	jra	Idle
Lswfnd:
	movw	#PSL_HIGHIPL,sr		| lock out interrupts
	movl	a0@,d1			| and check again...
	bclr	d0,d1
	jeq	Lsw1			| proc moved, rescan
	movl	d1,a0@			| update whichqs
	moveq	#1,d1			| double check for higher priority
	lsll	d0,d1			| process (which may have snuck in
	subql	#1,d1			| while we were finding this one)
	andl	a0@,d1
	jeq	Lswok			| no one got in, continue
	movl	a0@,d1
	bset	d0,d1			| otherwise put this one back
	movl	d1,a0@
	jra	Lsw1			| and rescan
Lswok:
	movl	d0,d1
	lslb	#3,d1			| convert queue number to index
	addl	#_qs,d1			| locate queue (q)
	movl	d1,a1
	cmpl	a1@(P_FORW),a1		| anyone on queue?
	jeq	Lbadsw			| no, panic
	movl	a1@(P_FORW),a0		| p = q->p_forw
	movl	a0@(P_FORW),a1@(P_FORW)	| q->p_forw = p->p_forw
	movl	a0@(P_FORW),a1		| q = p->p_forw
	movl	a0@(P_BACK),a1@(P_BACK)	| q->p_back = p->p_back
	cmpl	a0@(P_FORW),d1		| anyone left on queue?
	jeq	Lsw2			| no, skip
	movl	_whichqs,d1
	bset	d0,d1			| yes, reset bit
	movl	d1,_whichqs
Lsw2:
	movl	a0,_curproc
	clrl	_want_resched
#ifdef notyet
	movl	sp@+,a1
	cmpl	a0,a1			| switching to same proc?
	jeq	Lswdone			| yes, skip save and restore
#endif
	/*
	 * Save state of previous process in its pcb.
	 */
	movl	_curpcb,a1
	moveml	#0xFCFC,a1@(PCB_REGS)	| save non-scratch registers
	movl	usp,a2			| grab USP (a2 has been saved)
	movl	a2,a1@(PCB_USP)		| and save it

	tstl	_fpu_type		| Do we have an fpu?
	jeq	Lswnofpsave		| No?  Then don't try save.
	lea	a1@(PCB_FPCTX),a2	| pointer to FP save area
	fsave	a2@			| save FP state
	tstb	a2@			| null state frame?
	jeq	Lswnofpsave		| yes, all done
	fmovem	fp0-fp7,a2@(FPF_REGS)		| save FP general regs
	fmovem	fpcr/fpsr/fpi,a2@(FPF_FPCR)	| save FP control regs
Lswnofpsave:

#ifdef DIAGNOSTIC
	tstl	a0@(P_WCHAN)
	jne	Lbadsw
	cmpb	#SRUN,a0@(P_STAT)
	jne	Lbadsw
#endif
	clrl	a0@(P_BACK)		| clear back link
	movl	a0@(P_ADDR),a1		| get p_addr
	movl	a1,_curpcb
	movb	a0@(P_MDFLAG+3),mdpflag	| low byte of p_md.md_flags

	/* see if pmap_activate needs to be called; should remove this */
	movl	a0@(P_VMSPACE),a0	| vmspace = p->p_vmspace
#ifdef DIAGNOSTIC
	tstl	a0			| map == VM_MAP_NULL?
	jeq	Lbadsw			| panic
#endif

| Important note:  We MUST call pmap_activate to set the
| MMU context register (like setting a root table pointer).
	lea	a0@(VM_PMAP),a0		| pmap = &vmspace.vm_pmap
	pea	a1@			| push pcb (at p_addr)
	pea	a0@			| push pmap
	jbsr	_pmap_activate		| pmap_activate(pmap, pcb)
	addql	#8,sp
	movl	_curpcb,a1		| restore p_addr

| XXX - Should do this in pmap_activeate only if context reg changed.
	movl	#IC_CLEAR,d0
	movc	d0,cacr

Lcxswdone:
	moveml	a1@(PCB_REGS),#0xFCFC	| reload registers
	movl	a1@(PCB_USP),a0
	movl	a0,usp			| and USP

	tstl	_fpu_type		| If we don't have an fpu,
	jeq	Lres_skip		|  don't try to restore it.
	lea	a1@(PCB_FPCTX),a0	| pointer to FP save area
	tstb	a0@			| null state frame?
	jeq	Lresfprest		| yes, easy
	fmovem	a0@(FPF_FPCR),fpcr/fpsr/fpi	| restore FP control regs
	fmovem	a0@(FPF_REGS),fp0-fp7		| restore FP general regs
Lresfprest:
	frestore a0@			| restore state
Lres_skip:
	movw	a1@(PCB_PS),d0		| no, restore PS
#ifdef DIAGNOSTIC
	btst	#13,d0			| supervisor mode?
	jeq	Lbadsw			| no? panic!
#endif
	movw	d0,sr			| OK, restore PS
	moveq	#1,d0			| return 1 (for alternate returns)
	rts

/*
 * savectx(pcb)
 * Update pcb, saving current processor state.
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
	fmovem	fp0-fp7,a0@(FPF_REGS)		| save FP general regs
	fmovem	fpcr/fpsr/fpi,a0@(FPF_FPCR)	| save FP control regs
Lsavedone:
	moveq	#0,d0			| return 0
	rts

/* suline() */
/* TBIA, TBIS, TBIAS, TBIAU */

/*
 * Invalidate instruction cache
 */
ENTRY(ICIA)
	movl	#IC_CLEAR,d0
	movc	d0,cacr			| invalidate i-cache
	rts

/* DCIA, DCIS */

/*
 * Invalidate data cache.
 */
ENTRY(DCIU)
	rts

/* ICPL, ICPP, DCPL, DCPP, DCPA, DCFL, DCFP, PCIA */
/* ecacheon, ecacheoff */

/*
 * Get callers current SP value.
 * Note that simply taking the address of a local variable in a C function
 * doesn't work because callee saved registers may be outside the stack frame
 * defined by A6 (e.g. GCC generated code).
 *
 * [I don't think the ENTRY() macro will do the right thing with this -- glass]
 */
	.globl	_getsp
_getsp:
	movl	sp,d0			| get current SP
	addql	#4,d0			| compensate for return address
	rts

ENTRY(getsfc)
	movc	sfc,d0
	rts

ENTRY(getdfc)
	movc	dfc,d0
	rts

ENTRY(getvbr)
	movc vbr, d0
	rts

ENTRY(setvbr)
	movl sp@(4), d0
	movc d0, vbr
	rts

/* loadustp */

/*
 * Set processor priority level calls.  Most are implemented with
 * inline asm expansions.  However, we need one instantiation here
 * in case some non-optimized code makes external references.
 * Most places will use the inlined function param.h supplies.
 */

ENTRY(_spl)
	movl	sp@(4),d1
	clrl	d0
	movw	sr,d0
	movw	d1,sr
	rts

ENTRY(getsr)
	moveq	#0, d0
	movw	sr, d0
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
	fmovem fp0-fp7,a0@(FPF_REGS)		| save FP general regs
	fmovem fpcr/fpsr/fpi,a0@(FPF_FPCR)	| save FP control regs
Lm68881sdone:
	rts

ENTRY(m68881_restore)
	movl	sp@(4),a0		| save area pointer
	tstb	a0@			| null state frame?
	jeq	Lm68881rdone		| yes, easy
	fmovem	a0@(FPF_FPCR),fpcr/fpsr/fpi	| restore FP control regs
	fmovem	a0@(FPF_REGS),fp0-fp7		| restore FP general regs
Lm68881rdone:
	frestore a0@			| restore state
	rts

| delay(int usecs)
| Delay for "usec" microseconds.  Minimum delay is about 5 uS.
|
| This routine depends on the variable "cpuspeed"
| which should be set based on the CPU clock rate.
| XXX - Currently this is set in sun3_startup.c based on the
| CPU model but this should be determined at run time...
|
	.globl	_delay
_delay:
	| d0 = (cpuspeed * usecs)
	movel	_cpuspeed,d0
	mulsl	sp@(4),d0
	| subtract some overhead
	moveq	#80,d1
	subl	d1,d0
| This loop takes 8 clocks per cycle.
Ldelay:
	subql	#8,d0
	jgt	Ldelay
	rts


| Define some addresses, mostly so DDB can print useful info.
	.globl	_kernbase
	.set	_kernbase,KERNBASE
	.globl	_dvma_base
	.set	_dvma_base,DVMA_SPACE_START
	.globl	_prom_start
	.set	_prom_start,MONSTART
	.globl	_prom_base
	.set	_prom_base,PROM_BASE

|The end!
