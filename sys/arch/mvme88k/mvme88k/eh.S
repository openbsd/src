/*   $OpenBSD: eh.S,v 1.29 2003/01/14 03:20:16 miod Exp $   */
/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * Copyright (c) 1996 Nivas Madhur
 * Copyright (c) 1998 Steve Murphree, Jr.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 * HISTORY
 * 1. Should get rid of SR0 reference for thread stuff.
 * 2. Make up my mind what is _kstack. I think it
 *       should be p->p_addr+UPAGES. (p_addr
 *         is pointing to user struct and swapin is
 *         making sure it is updated)
 *   Whatever is _kstack, its usage in this file should be
 *   revisited.
 */
 
 /*
 **************************************************************RCS******
 *
 *  -------------------------------------------------------------------
 * ;  In the following discussion, references are made to:             ;
 * ;          MC88100 - RISC MICROPROCESSOR USER'S MANUAL              ;
 * ;  (second edition). Reference in []s refer to section numbers.     ;
 * ;                                                                   ;
 * ;  This discussion assumes that you are at least vaguely familiar   ;
 * ;  with 88100 exception handling (chapter 6), the MACH kernel, and  ;
 * ;  that you have a brain (and use it while reading this).           ;
 * ;                                                                   ;
 * ;  I also assume (and hope) that you're not offended by             ;
 * ;  frequent misspellings.                                           ;
 * ;                                                                   ;
 * ;                       Jeffrey Friedl                              ;
 * ;            jfriedl@rna.ncl.omron.co.jp             ;
 * ;                       December, 1989                              ;
 *  -------------------------------------------------------------------
 *
 * EXCEPTIONS, INTERRUPTS, and TRAPS
 * ---------------------------------
 * This is the machine exception handler.
 * In the MC88100, various "conditions" cause an exception, where
 * processing momentarily jumps here to "service" the exception,
 * and then continues where it left off.
 *
 * There are a number of different types of exceptions.
 * For example, exception #6 is the privilege violation exception which
 * is raised when the user tries to execute a supervisor-only instruction.
 *
 * Exception #1 is the interrupt exception, and is raised when an
 * outside device raises the INT line on the CPU.  This happens,
 * for example, when the clock signals that it is time for a context
 * switch, or perhaps the disk drive signaling that some operation
 * is complete.
 *
 * Traps are also exceptions.  Traps are ways for user programs to request
 * kernel operations.  For example, "tcnd eq0, r0, 128" will raise
 * exception 128, the system call exception.
 *
 *
 * SERVICING AN EXCEPTION
 * -----------------------
 * When an exception occurs, each control register is saved in its
 * respective shadow register and execution continues from the
 * appropriate exception handler.  The exception handler must
 *      - save the context from the time of the exception
 *      - service the exception
 *      - restore the context (registers, etc)
 *      - pick up from where the exception occurred.
 *
 * The context is saved on a stack. Actually, in the user_state area
 * in the PCB if the exception happens in user mode.
 *
 * Servicing the exception is usually straightforward and in fact not dealt
 * with very much here.  Usually a C routine is called to handle it.
 * For example, when a privilege exception is raised, the routine that sends
 * an "illegal instruction" signal to the offending process is called.
 *
 * When the exception has been serviced, the context is restored from the
 * stack and execution resumes from where it left off.
 *
 * In more detail:
 *
 * Saving the exception-time context.
 * ---------------------------------
 *     In saving the exception-time context, we copy the shadow and general
 * purpose registers to memory.  Since one exception may occur while
 * servicing another, the memory used to save the exception-time context may
 * not be static (i.e. the same every time).  Thus, memory on a stack is set
 * aside for the exception frame (area where the exception-time context is
 * saved). The same stack is also used when C routines are called (to
 * service the exception).
 *
 *    Each process has a stack in kernel space (called the "kernel stack",
 * short for "process's kernel stack) as well as the user space stack.  When
 * entering the kernel from user space, the kernel stack is unused.  On this
 * stack we save the exception state and (most likely call a C routine to)
 * service the exception.
 *
 * Before servicing an exception, several issues must be addressed.
 *
 * 1) When an interrupt is recognized by the hardware, the data pipeline is
 *    allowed to clear.  However, if one of these data accesses faults (bad
 *    reference, or a reference to a page which needs to be swapped in), that
 *    reference, as well as any others in the pipeline at the time (at most
 *    three total) are left there, to be taken care of by the exception
 *    handler [6.4.1].  This involves swapping in the proper page and
 *    manually doing the appropriate load or store.
 *
 *    The other (at most, two other) data accesses that might have been in
 *    the pipeline must also be manually completed (even though they may not
 *    be at fault [yes, that's a bad pun, thank you]).
 *
 * 2) If any of the (at most three) uncompleted data access in the pipeline
 *    are loads (from memory to a register), then the bit for the destination
 *    register is set in the SSBR.  Since the hardware will never complete
 *    that load (since we do it manually), the hardware will never clear that
 *    SSBR bit.  Thus, we must clear it manually.  If this isn't done, the
 *    system will hang waiting for a bit to clear that will never.
 *
 * 3) If the exception is the privilege violation exception, the bounds
 *    violation exception, or the misaligned access exception, the
 *    destination register bit in the SSBR may need to be cleared.
 *
 * 4) If the exception is one of the floating exceptions, then the
 *    destination register for that floating process won't be written,
 *    and the SSBR must be cleared explicitly.
 *
 * 5) The FPU must be enabled (as it is disabled by the exception processing
 *    hardware) and allowed to complete actions in progress. This is so
 *    so that it may be used in the servicing of any instruction.
 *    When the FPU is being restarted, operations attempting to complete
 *    may themselves fault (raising another exception).
 *
 * More on Restarting the FPU
 * --------------------------
 *   The manual [section 6.4.3.4] gives only minor mention to this
 * rather complex task.  Before the FPU is restarted all SSBR bits are
 * cleared for actions that the exception handler completes (as mentioned
 * above) so that the SSBR is clear unless there are FPU operations that
 * have not actually been completed (and hence not written to the registers).
 * Also, all control registers (at least all those that we care about) are
 * saved to the stack exception frame before the FPU is restarted (this
 * is important... the reason comes later).
 *
 * The FPU is restarted by doing an rte to a trap-not-taken (the rte
 * actually enables the fpu because we ensure that the EPSR has the
 * FPU-enable bit on; the trap-not-taken ensures anything in the FPU
 * completes by waiting until scoreboard register is clear).
 *
 * At the time the FPU is restarted (the rte to the trap-not-taken) the FPU
 * can write to ANY of the general registers.  Thus, we must make sure that
 * all general registers (r1..r31) are in their pre-exception state so that
 * when saved to the exception frame after the FPU is enabled, they properly
 * reflect any changes made by the FPU in being restarted.
 *
 * Because we can't save the pointer to the exception frame in a general
 * register during the FPU restart (it could get overwritten by the FPU!),
 * we save it in a control register, SR3, during the restart.
 *
 *
 * HOWEVER .....
 *
 * Because other uncompleted actions in the FPU may fault when the FPU is
 * restarted, a new exception may be raised during the restart. This may
 * happen recursively a number of times. Thus, during a restart, ANY register
 * whatsoever may be modified, including control registers.  Because of this
 * we must make sure that the exception handler preserves SR3 throughout
 * servicing an exception so that, if the exception had been raised during
 * an FPU restart, it is returned unmolested when control returns to the FPU
 * restart.
 *
 * Thus: if an exception is from kernel space, we MUST preserve SR3.
 * (if it from user space, no FPU-enable can be in progress and SR3 is
 *  unimportant).
 *
 * Now is a good time to recap SR0..SR3 usage:
 *   SR0 - Used for subroutine returns.  XXX smurph XXX2 not any more...
 *   SR1 - CPU flags (exception handler flags)
 *   SR2 - generally free
 *   SR3 - free only if the exception is from user mode
 *
 * Once the FPU has been restarted, the general registers are saved to the
 * exception frame.  If the exception is not the interrupt exception,
 * interrupts are enabled and any faulted data accesses (see above) are
 * serviced.  In either case, the exception is then serviced (usually by
 * calling a C routine).  After servicing, any faulted data accesses are
 * serviced (if it had been the interrupt exception).  The context is then
 * restored and control returns to where the exception occurred.
 *
 */

#include "assym.h"
#include <machine/asm.h>
#include <machine/board.h>
#include <machine/param.h>          /* CPU_ and BRD_ defines */
#include <machine/trap.h>           /* for T_ defines */
#include <machine/board.h>

/*
 * The exception frame as defined in "machine/pcb.h" (among other places) is
 * a bit outdated and needs to be changed. Until then, we'll define some
 * pseudo-fields there for our needs.
 *
 * EF_SR3    A place to save the exception-time SR3 from just after the
 *         time when an exception is raised until just after the FPU
 *         has been restarted.  This does not necessarly conflict with
 *        the general registers (though it can if you're not careful)
 *         and so we can use a spot later used to save a general register.
 *
 * EF_FLAGS  This is just the old EF_MODE. "EF_MODE" isn't a very good name.
 */
#define EF_SR3         (EF_R0 + 5)
#define EF_FLAGS      EF_MODE

#define INTSTACK	0   /* To make interupts use their own stack */
	data
	align 4
sbadcpupanic:
	string  "eh.S: bad cpu number in FLAGS"
	
	text
	align 8

Lbadcpupanic:
	or.u r2, r0, hi16(sbadcpupanic)
	or   r2, r2, lo16(sbadcpupanic)
	bsr  _C_LABEL(panic)

	align 8

#define OFF_VEC		0
#define OFF_EPSR	4
#define OFF_EXIP	8
#define OFF_ENIP	12
#define OFF_DSR		16
#define OFF_DLAR	20
#define OFF_DPAR	24
#define OFF_ISR		28
#define OFF_ILAR	32
#define OFF_IPAR	36
#define OFF_TMP		40


#define SAVE_CTX				; \
	stcr	r31, SRX			; \
	or.u	r31, r0,  hi16(_save_frame)	; \
	or	r31, r31, lo16(_save_frame)	; \
	/* save old R31 and other R registers */; \
	st.d	r0 , r31, GENREG_OFF(0)		; \
	st.d	r2 , r31, GENREG_OFF(2)		; \
	st.d	r4 , r31, GENREG_OFF(4)		; \
	st.d	r6 , r31, GENREG_OFF(6)		; \
	st.d	r8 , r31, GENREG_OFF(8)		; \
	st.d	r10, r31, GENREG_OFF(10)	; \
	st.d	r12, r31, GENREG_OFF(12)	; \
	st.d	r14, r31, GENREG_OFF(14)	; \
	st.d	r16, r31, GENREG_OFF(16)	; \
	st.d	r18, r31, GENREG_OFF(18)	; \
	st.d	r20, r31, GENREG_OFF(20)	; \
	st.d	r22, r31, GENREG_OFF(22)	; \
	st.d	r24, r31, GENREG_OFF(24)	; \
	st.d	r26, r31, GENREG_OFF(26)	; \
	st.d	r28, r31, GENREG_OFF(28)	; \
	st	r30, r31, GENREG_OFF(30)	; \
	ldcr	r1,  SRX			; \
	st	r1,  r31, GENREG_OFF(31)	; \
	ldcr	r1,  EPSR			; \
	ldcr	r2,  EXIP			; \
	ldcr	r3,  ENIP			; \
	st	r1,  r31, REG_OFF(EF_EPSR)	; \
	st	r2,  r31, REG_OFF(EF_EXIP)	; \
	st	r3,  r31, REG_OFF(EF_ENIP)	; \
	ldcr	r1,  DSR			; \
	ldcr	r2,  DLAR			; \
	ldcr	r3,  DPAR			; \
	st	r1,  r31, REG_OFF(EF_DSR)	; \
	st	r2,  r31, REG_OFF(EF_DLAR)	; \
	st	r3,  r31, REG_OFF(EF_DPAR)	; \
	ldcr	r1,  ISR			; \
	ldcr	r2,  ILAR			; \
	ldcr	r3,  IPAR			; \
	st	r1,  r31, REG_OFF(EF_ISR)	; \
	st	r2,  r31, REG_OFF(EF_ILAR)	; \
	st	r3,  r31, REG_OFF(EF_IPAR)	; \
	ldcr	r1, DSAP			; \
	ldcr	r2, DUAP			; \
	st	r1,  r31, REG_OFF(EF_DSAP)	; \
	st	r2,  r31, REG_OFF(EF_DUAP)	; \
	ldcr	r1, ISAP			; \
	ldcr	r2, IUAP			; \
	st	r1,  r31, REG_OFF(EF_ISAP)	; \
	st	r2,  r31, REG_OFF(EF_IUAP)	; \
	/* Restore r1, r2, r3, and r31	*/	; \
	ld	r1 , r31, GENREG_OFF(1)		; \
	ld.d	r2 , r31, GENREG_OFF(2)		; \
	ld	r31, r31, GENREG_OFF(31)

/***************************************************************************
 ***************************************************************************
 **
 ** #define PREP(NAME, NUM, BIT, SSBR_STUFF, FLAG_CHECK)
 **
 ** This is the "exception processing preparaton" common to all exception
 ** processing.  It is used in the following manor:
 **
 **    ASGLOBAL(foo_handler)
 **           PREP("foo", 11, DEBUG_FOO_BIT, No_SSBR_Stuff, No_Precheck)
 **           CALL(_C_LABEL(trapXXX), T_FOO_FAULT, r31)
 **           DONE(DEBUG_FOO_BIT)
 **
 ** This defines the exception handler for the "foo" exception.
 ** The arguments ro PREP():
 **   NAME -    String for debugging (more info later)
 **   NUM  -    The exception number [see the manual, Table 6-1]
 **   BIT  -    Bit to check in eh_debug for debugging (more info later)
 **   SSBR_STUFF -
 **      If the exception might leave some bits in the SSBR set,
 **      this should indicate how they are cleared.
 **   FLAG_PRECHECK -
 **      This is for the data access exception only. See it for
 **      more info.
 **
 **
 ** What's in between PREP() and DONE() (usually a CALL) is the actual
 ** servicing of the interrupt.  During this time, any register may
 ** be used freely as they've all been saved in the exception frame
 ** (which is pointed-to by r31).
 **/

#ifdef M88100
#define PREP(NAME, NUM, BIT, SSBR_STUFF, FLAG_PRECHECK)	; \
	xcr	FLAGS, FLAGS, SR1			; \
	FLAG_PRECHECK					; \
							; \
	/* the bsr later clobbers r1, so save now */	; \
	stcr	r1, SR2   /* r1 now free */		; \
	/* set or clear the FLAG_FROM_KERNEL bit */	; \
	ldcr	r1, EPSR				; \
	bb0.n	PSR_SUPERVISOR_MODE_BIT, r1, 1f		; \
	clr	FLAGS, FLAGS, 1<FLAG_FROM_KERNEL>	; \
	set	FLAGS, FLAGS, 1<FLAG_FROM_KERNEL>	; \
							; \
	/* get a stack (exception frame) */		; \
1:	bsr	setup_phase_one				; \
							; \
	/* TMP2 now free -- use to set EF_VECTOR */	; \
	or	TMP2, r0, NUM				; \
	st	TMP2, r31, REG_OFF(EF_VECTOR)		; \
	/* TMP3 also free -- use to set last_vector */	; \
	or.u	TMP3, r0,   hi16(_last_vector)		; \
	st	TMP2, TMP3, lo16(_last_vector)		; \
							; \
	/* Clear any bits in the SSBR (held in TMP) */	; \
	/* SSBR_STUFF may be empty, though.         */	; \
	SSBR_STUFF					; \
							; \
	/* call setup_phase_two to restart the FPU  */	; \
	/* and to save all general registers.       */	; \
	bsr	setup_phase_two				; \
							; \
	/* All general regs free -- do any debugging */	; \
	PREP_DEBUG(BIT, NAME)
#endif /* m88100 */

#ifdef M88110
#define PREP2(NAME, NUM, BIT, SSBR_STUFF, FLAG_PRECHECK); \
	SAVE_CTX					; \
	xcr	FLAGS, FLAGS, SR1			; \
	FLAG_PRECHECK					; \
	/* the bsr later clobbers r1, so save now */	; \
	stcr    r1, SR2   /* r1 now free */		; \
	/* set or clear the FLAG_FROM_KERNEL bit */	; \
	ldcr    r1, EPSR 				; \
	bb0.n   PSR_SUPERVISOR_MODE_BIT, r1, 1f 	; \
	clr     FLAGS, FLAGS, 1<FLAG_FROM_KERNEL> 	; \
	set     FLAGS, FLAGS, 1<FLAG_FROM_KERNEL>	; \
							; \
	/* get a stack (exception frame) */		; \
1:	bsr	m88110_setup_phase_one			; \
							; \
	/* TMP2 now free -- use to set EF_VECTOR */	; \
	or	TMP2, r0, NUM				; \
	st	TMP2, r31, REG_OFF(EF_VECTOR)		; \
							; \
	/* call setup_phase_two to restart the FPU  */	; \
	/* and to save all general registers.       */	; \
	bsr	m88110_setup_phase_two			; \
							; \
	/* All general regs free -- do any debugging */	; \
	PREP_DEBUG(BIT, NAME)
#endif /* M88110 */

/* Some defines for use with PREP() */
#define No_SSBR_Stuff      /* empty */
#define Clear_SSBR_Dest      bsr clear_dest_ssbr_bit
#define No_Precheck      /* empty */
#define Data_Precheck \
   bb1.n   FLAG_IGNORE_DATA_EXCEPTION, FLAGS, ignore_data_exception
#define M88110_Data_Precheck \
   bb1.n   FLAG_IGNORE_DATA_EXCEPTION, FLAGS, m88110_ignore_data_exception

#ifdef EH_DEBUG
   /*
    * If we allow debugging, there is a variable "eh_debug"
    * in which there is a bit for each exception.  If the bit
    * is set for an exception, debugging information is printed
    * about that exception whenever it occurs.
    *
    * The bits are defined in "asm.h"
    */

GLOBAL(eh_debug)	word 0x00000000
   
   /*
    * additional pre-servicing preparation to be done when
    * debugging... check eh_debug and make the call if
    * need be.
    */
#define PREP_DEBUG(DebugNumber, Name)  \
	or.u	r2, r0, hi16(_eh_debug)		; \
	ld	r3, r2, lo16(_eh_debug)		; \
	bb0	DebugNumber, r3, 4f		; \
	/* call MY_info(ef,thread,flags,kind)*/	; \
	or	r2, r30, r0			; \
	ldcr	r3, SR0				; \
	ldcr	r4, SR1				; \
	or.u	r5, r0, hi16(2f)		; \
	or	r5, r5, lo16(2f)		; \
	bsr.n	_MY_info			; \
	subu	r31, r31, 40			; \
	br.n	4f				; \
	addu	r31, r31, 40			; \
	data					; \
	2: string Name				; \
	byte	0				; \
	align	4				; \
	text					; \
	4:


   /*
    * Post-servicing work to be done.
    * When debugging, check "eh_debug" and call the
    * debug routined if neeed be.
    *
    * Then, return from the interrupt handler.
    */
#define DONE(DebugNumber)  \
	or.u	r2, r0, hi16(_eh_debug)	; \
	ld	r3, r2, lo16(_eh_debug)	; \
	bb0	DebugNumber, r3, 2f	; \
	ldcr	r4, SR1			; \
	CALL(_MY_info_done, r31, r4)	; \
2:	br	return_from_exception_handler
#else
   /*
    * If not debugging, then no debug-prep to do.
    * Also, when you're done, you're done!   (no debug check).
    */
#define PREP_DEBUG(bit, name)
#define DONE(num)      br return_from_exception_handler
#endif

#ifdef M88100
/*#########################################################################*/
/*#### THE ACTUAL EXCEPTION HANDLER ENTRY POINTS for MVME18x ##############*/
/*#########################################################################*/

/* unknown exception handler */
GLOBAL(unknown_handler)
	PREP("unknown", 0, DEBUG_UNKNOWN_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88100_trap), T_UNKNOWNFLT, r30)
	DONE(DEBUG_UNKNOWN_BIT)

/* interrupt exception handler */
GLOBAL(interrupt_handler)
	PREP("interrupt", 1, DEBUG_INTERRUPT_BIT, No_SSBR_Stuff, No_Precheck)
	/* interrupt_func is set in mvme_bootstrap() */
	CALL(_C_LABEL(m88100_trap), T_INT, r30)
	/*CALLP(_interrupt_func, 1, r30) */
	DONE(DEBUG_INTERRUPT_BIT)

/* instruction access exception handler */
GLOBAL(instruction_access_handler)
	PREP("inst", 2, DEBUG_INSTRUCTION_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88100_trap), T_INSTFLT, r30)
	DONE(DEBUG_INSTRUCTION_BIT)

/*
 * data access exception handler --
 *  See badaddr() below for info about Data_Precheck.
 */
GLOBAL(data_exception_handler)
	PREP("data", 3, DEBUG_DATA_BIT, No_SSBR_Stuff, Data_Precheck)
	DONE(DEBUG_DATA_BIT)

/* misaligned access exception handler */
GLOBAL(misaligned_handler)
	PREP("misalign", 4, DEBUG_MISALIGN_BIT, Clear_SSBR_Dest, No_Precheck)
	CALL(_C_LABEL(m88100_trap), T_MISALGNFLT, r30)
	DONE(DEBUG_MISALIGN_BIT)

/* unimplemented opcode exception handler */
GLOBAL(unimplemented_handler)
	PREP("unimp", 5, DEBUG_UNIMPLEMENTED_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88100_trap), T_ILLFLT, r30)
	DONE(DEBUG_UNIMPLEMENTED_BIT)

/*
 * Some versions of the chip have a bug whereby false privilege
 * violation exceptions are raised. If the valid bit in the SXIP is clear,
 * it is false.  If so, just return.  The code before PREP handles this....
 */
GLOBAL(privilege_handler)
	stcr    r1, SR2    /* hold r1 for a moment */
	ldcr    r1, SXIP   /* look at the sxip... valid bit set? */
	bb1.n   RTE_VALID_BIT, r1, 1f /*skip over return if a valid exception*/
	ldcr    r1, SR2    /* restore r1 */
	RTE
1:	PREP("privilege", 6, DEBUG_PRIVILEGE_BIT, Clear_SSBR_Dest, No_Precheck)
	CALL(_C_LABEL(m88100_trap), T_PRIVINFLT, r30) 
	DONE(DEBUG_PRIVILEGE_BIT)

/*
 * I'm not sure what the trap(T_BNDFLT,...) does, but it doesn't send
 * a signal to the process...
 */
GLOBAL(bounds_handler)
	PREP("bounds", 7, DEBUG_BOUNDS_BIT, Clear_SSBR_Dest, No_Precheck)
	CALL(_C_LABEL(m88100_trap), T_BNDFLT, r30)
	DONE(DEBUG_BOUNDS_BIT)

/* integer divide-by-zero exception handler */
GLOBAL(divide_handler)
	PREP("divide", 8, DEBUG_DIVIDE_BIT, Clear_SSBR_Dest, No_Precheck)
	CALL(_C_LABEL(m88100_trap), T_ZERODIV, r30)
	DONE(DEBUG_DIVIDE_BIT)

/* integer overflow exception handler */
GLOBAL(overflow_handler)
	PREP("overflow", 9, DEBUG_OVERFLOW_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88100_trap), T_OVFFLT, r30)
	DONE(DEBUG_OVERFLOW_BIT)

/* Floating-point precise handler */
#define FPp_SSBR_STUFF bsr clear_FPp_ssbr_bit
GLOBAL(fp_precise_handler)
	PREP("FPU precise", 114, DEBUG_FPp_BIT, FPp_SSBR_STUFF, No_Precheck)
	CALL(_m88100_Xfp_precise, r0, r30) /* call fp_precise(??, exception_frame)*/
	DONE(DEBUG_FPp_BIT)

/* Floating-point imprecise handler */
#define FPi_SSBR_STUFF  bsr clear_FPi_ssbr_bit
GLOBAL(fp_imprecise_handler)
	PREP("FPU imprecise", 115, DEBUG_FPi_BIT, FPi_SSBR_STUFF, No_Precheck)
	CALL(_Xfp_imprecise, r0, r30) /*call fp_imprecise(??,exception_frame)*/
	DONE(DEBUG_FPi_BIT)

/* All standard system calls.  */
GLOBAL(syscall_handler)
	PREP("syscall", 128, DEBUG_SYSCALL_BIT, No_SSBR_Stuff, No_Precheck)
	ld   r13, r30, GENREG_OFF(13)
	CALL(_m88100_syscall, r13, r30) /* system call no. is in r13 */
	DONE(DEBUG_SYSCALL_BIT)

/* trap 496 comes here */
GLOBAL(bugtrap)
	PREP("bugsyscall", 496, DEBUG_BUGCALL_BIT, No_SSBR_Stuff, No_Precheck)
	ld   r9,  r30, GENREG_OFF(9)
	CALL(_bugsyscall, r9, r30)   /* system call no. is in r9 */
	DONE(DEBUG_BUGCALL_BIT)

GLOBAL(sigsys)
	PREP("sigsys", 0, DEBUG_SIGSYS_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88100_trap), T_SIGSYS, r30)
	DONE(DEBUG_SIGSYS_BIT)

GLOBAL(sigtrap)
	PREP("sigtrap", 0, DEBUG_SIGTRAP_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88100_trap), T_SIGTRAP, r30)
	DONE(DEBUG_SIGTRAP_BIT)

GLOBAL(stepbpt)
	PREP("stepbpt", 0, DEBUG_SIGTRAP_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88100_trap), T_STEPBPT, r30)
	DONE(DEBUG_SIGTRAP_BIT)

GLOBAL(userbpt)
	PREP("userbpt", 0, DEBUG_SIGTRAP_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88100_trap), T_USERBPT, r30)
	DONE(DEBUG_SIGTRAP_BIT)

#if DDB
GLOBAL(break)
	PREP("break", 130, DEBUG_BREAK_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88100_trap), T_KDB_BREAK, r30)
	DONE(DEBUG_BREAK_BIT)

GLOBAL(trace)
	PREP("trace", 131, DEBUG_TRACE_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88100_trap), T_KDB_TRACE, r30)
	DONE(DEBUG_TRACE_BIT)

GLOBAL(entry)
	PREP("kdb", 132, DEBUG_KDB_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88100_trap), T_KDB_ENTRY, r30)
	DONE(DEBUG_KDB_BIT)

#else /* else not DDB */
GLOBAL(break)
	PREP("break", 130, DEBUG_BREAK_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88100_trap), T_UNKNOWNFLT, r30)
	DONE(DEBUG_BREAK_BIT)

GLOBAL(trace)
	PREP("trace", 131, DEBUG_TRACE_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88100_trap), T_UNKNOWNFLT, r30)
	DONE(DEBUG_TRACE_BIT)

GLOBAL(entry)
	PREP("unknown", 132, DEBUG_UNKNOWN_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88100_trap), T_UNKNOWNFLT, r30)
	DONE(DEBUG_KDB_BIT)
#endif   /* DDB */

/*--------------------------------------------------------------------------*/

/*
 * The error exception handler.
 * The error exception is raised when any other non-trap exception is raised
 * while shadowing is off. This is Bad News.
 *
 * The shadow registers are not valid in this case (shadowing was off, ne).
 * R1-R31 may be interesting though, so we'll save them.
 *
 * We'll not worry about trashing r26-29 here,
 * since they aren't generally used.
 */
GLOBAL(error_handler)
        /* pick up the slavestack */
	or	r26, r0, r31           /* save old stack */
	or.u	r31, r0,  hi16(_intstack_end)
	or	r31, r31, lo16(_intstack_end)

	/* zero the stack, so we'll know what we're lookin' at */
	or.u	r27, r0,  hi16(_intstack)
	or	r27, r27, lo16(_intstack)
1:	cmp	r28, r27, r31
	bb1	ge,  r28, 2f   /* branch if at the end of the stack */
	st	r0,  r0,  r27
	br.n	1b
	addu	r27, r27, 4   /* bump up */
2:   /* stack has been cleared */

   /* ensure that stack is 8-byte aligned */
        clr     r31, r31, 3<0>  /* round down to 8-byte boundary */

   /* create exception frame on stack */
        subu    r31, r31, SIZEOF_EF             /* r31 now our E.F. */

   /* save old R31 and other R registers */
        st.d    r0 , r31, GENREG_OFF(0)
        st.d    r2 , r31, GENREG_OFF(2)
        st.d    r4 , r31, GENREG_OFF(4)
        st.d    r6 , r31, GENREG_OFF(6)
        st.d    r8 , r31, GENREG_OFF(8)
        st.d    r10, r31, GENREG_OFF(10)
        st.d    r12, r31, GENREG_OFF(12)
        st.d    r14, r31, GENREG_OFF(14)
        st.d    r16, r31, GENREG_OFF(16)
        st.d    r18, r31, GENREG_OFF(18)
        st.d    r20, r31, GENREG_OFF(20)
        st.d    r22, r31, GENREG_OFF(22)
        st.d    r24, r31, GENREG_OFF(24)
        st      r30, r31, GENREG_OFF(30)
	st	r26, r31, GENREG_OFF(31)
	
	/* save shadow registers (are OLD if error_handler, though) */
	ldcr	r10, EPSR
	st	r10, r31, REG_OFF(EF_EPSR)
	ldcr	r10, SXIP
	st	r10, r31, REG_OFF(EF_SXIP)
	ldcr	r10, SNIP
	st	r10, r31, REG_OFF(EF_SNIP)
	ldcr	r10, SR1
	st	r10, r31, REG_OFF(EF_MODE)
	ldcr	r10, SFIP
	st	r10, r31, REG_OFF(EF_SFIP)
	ldcr	r10, SSBR
	st	r10, r31, REG_OFF(EF_SSBR)
	stcr	r0,  SSBR  /* won't want shadow bits bothering us later */
	ldcr	r10, DMT0
	st	r10, r31, REG_OFF(EF_DMT0)
	ldcr	r11, DMD0
	st	r11, r31, REG_OFF(EF_DMD0)
	ldcr	r12, DMA0
	st	r12, r31, REG_OFF(EF_DMA0)
	ldcr	r10, DMT1
	st	r10, r31, REG_OFF(EF_DMT1)
	FLUSH_PIPELINE
	ldcr	r11, DMD1
	st 	r11, r31, REG_OFF(EF_DMD1)
	ldcr	r12, DMA1
	st	r12, r31, REG_OFF(EF_DMA1)

	ldcr	r10, DMT2
	st	r10, r31, REG_OFF(EF_DMT2)
	ldcr	r11, DMD2
	st	r11, r31, REG_OFF(EF_DMD2)
	ldcr	r12, DMA2
	st	r12, r31, REG_OFF(EF_DMA2)
	
	/* shove sr2 into EF_FPLS1 */
	ldcr	r10, SR2
	st	r10, r31, REG_OFF(EF_FPLS1)

	/* shove sr3 into EF_FPHS2 */
	ldcr	r10, SR3
	st	r10, r31, REG_OFF(EF_FPHS2)

	/* error vector is 10 */
	or	r10, r0, 10
	st	r10, r31, REG_OFF(EF_VECTOR)

#if 0 /* MVME188 */
#define IST_REG      0xfff84040	/* interrupt status addr */
	/* check if it's a mvme188 */
	or.u	r10,  r0,   hi16(_brdtyp)
	ld	r11,  r10,  lo16(_brdtyp)
	cmp	r10,  r11,  BRD_188
	bb1	ne,   r10,  3f
	or.u	r10,  r0,   hi16(IST_REG) /* interrupt status register */
	ld	r11,  r10,  lo16(IST_REG)
	st	r11, r31,   REG_OFF(EF_MASK) /* put in EF_MASK for regdump */ 
#endif /* MVME188 */ 
	/*
	 * Cheap way to enable FPU and start shadowing again.
	 */
3:      ldcr    r10, PSR
        clr     r10, r10, 1<PSR_FPU_DISABLE_BIT>    /* enable the FPU */
        clr     r10, r10, 1<PSR_SHADOW_FREEZE_BIT>  /* also enable shadowing */
        stcr    r10, PSR  /* bang */
	FLUSH_PIPELINE

	/* put pointer to regs into r30... r31 will become a simple stack */
	or	r30, r31, r0

        subu	r31, r31, 0x10 /* make some breathing space */
        st	r30, r31, 0x0c /* store frame pointer on the st */
        st	r30, r31, 0x08 /* store again for the debugger to recognize */ 
	or.u	r20,  r0, hi16(0x87654321)
	or	r20, r20, lo16(0x87654321)
	st	r20, r31, 0x04
	st	r20, r31, 0x00

        CALL(_C_LABEL(error_fault), r30, r30)

        /* TURN INTERUPTS back on */
        ldcr r1, PSR
	clr  r1, r1, 1<PSR_INTERRUPT_DISABLE_BIT>
        stcr r1, PSR
        FLUSH_PIPELINE

GLOBAL(error_loop) bsr _error_loop
   /* never returns*/

/*
 * The reset exception handler.
 * The reset exception is raised when the RST signal is asserted (machine 
 * is reset), the value of VBR is changed after exceptions are enabled, 
 * or when a jmp, br/bsr to addr 0 (accidents do happen :-)
 *
 * To tell the difference, you should check the value of r1 and the valid
 * bit of SXIP.
 *
 * Upon a real reset, VBR is set to zero (0), so code must be at addr 0 
 * to handle it!!! 
 *
 * This is totaly different than _error_handler.  Shadowing might or 
 * might not be on.
 * R1-R31 could tell u alot about what happend, so we'll save them.
 *
 * We'll not worry about trashing r26-29 here,
 * since they aren't generally used.
 */
GLOBAL(reset_handler)
        /* pick up the slavestack */
	or	r26, r0, r31           /* save old stack */
	or.u	r31, r0,  hi16(_intstack_end)
	or	r31, r31, lo16(_intstack_end)

	/* zero the stack, so we'll know what we're lookin' at */
	or.u	r27, r0,  hi16(_intstack)
	or	r27, r27, lo16(_intstack)
1:	cmp	r28, r27, r31
	bb1	ge,  r28, 2f   /* branch if at the end of the stack */
	st	r0,  r0,  r27
	br.n	1b
	addu	r27, r27, 4   /* bump up */
2:	/* stack has been cleared */

	/* ensure that stack is 8-byte aligned */
	clr	r31, r31, 3<0>  /* round down to 8-byte boundary */

	/* create exception frame on stack */
	subu	r31, r31, SIZEOF_EF             /* r31 now our E.F. */

	/* save old R31 and other R registers */
	st.d	r0 , r31, GENREG_OFF(0)
	st.d	r2 , r31, GENREG_OFF(2)
	st.d	r4 , r31, GENREG_OFF(4)
	st.d	r6 , r31, GENREG_OFF(6)
	st.d	r8 , r31, GENREG_OFF(8)
	st.d	r10, r31, GENREG_OFF(10)
	st.d	r12, r31, GENREG_OFF(12)
	st.d	r14, r31, GENREG_OFF(14)
	st.d	r16, r31, GENREG_OFF(16)
	st.d	r18, r31, GENREG_OFF(18)
	st.d	r20, r31, GENREG_OFF(20)
	st.d	r22, r31, GENREG_OFF(22)
	st.d	r24, r31, GENREG_OFF(24)
	st	r30, r31, GENREG_OFF(30)
	st	r26, r31, GENREG_OFF(31)

	/* save shadow registers */
	ldcr	r10, EPSR
	st	r10, r31, REG_OFF(EF_EPSR)
	ldcr	r10, SXIP
	st	r10, r31, REG_OFF(EF_SXIP)
	ldcr	r10, SNIP
	st	r10, r31, REG_OFF(EF_SNIP)
	ldcr	r10, SR1
	st	r10, r31, REG_OFF(EF_MODE)
	ldcr	r10, SFIP
	st	r10, r31, REG_OFF(EF_SFIP)
	ldcr	r10, SSBR
	st	r10, r31, REG_OFF(EF_SSBR)
	stcr	r0,  SSBR  /* won't want shadow bits bothering us later */

	ldcr	r10, DMT0
	st	r10, r31, REG_OFF(EF_DMT0)
	ldcr	r11, DMD0
	st	r11, r31, REG_OFF(EF_DMD0)
	ldcr	r12, DMA0
	st	r12, r31, REG_OFF(EF_DMA0)

	ldcr	r10, DMT1
	st	r10, r31, REG_OFF(EF_DMT1)
	FLUSH_PIPELINE
	ldcr	r11, DMD1
	st 	r11, r31, REG_OFF(EF_DMD1)
	ldcr	r12, DMA1
	st	r12, r31, REG_OFF(EF_DMA1)

	ldcr	r10, DMT2
	st	r10, r31, REG_OFF(EF_DMT2)
	ldcr	r11, DMD2
	st	r11, r31, REG_OFF(EF_DMD2)
	ldcr	r12, DMA2
	st	r12, r31, REG_OFF(EF_DMA2)

	/* shove sr2 into EF_FPLS1 */
	ldcr	r10, SR2
	st	r10, r31, REG_OFF(EF_FPLS1)

	/* shove sr3 into EF_FPHS2 */
	ldcr	r10, SR3
	st	r10, r31, REG_OFF(EF_FPHS2)

	/* error vector is zippo numero el'zeroooo */
	st	r0,  r31, REG_OFF(EF_VECTOR)

	/*
	 * Cheap way to enable FPU and start shadowing again.
	 */
        ldcr    r10, PSR
        clr     r10, r10, 1<PSR_FPU_DISABLE_BIT>    /* enable the FPU */
        clr     r10, r10, 1<PSR_SHADOW_FREEZE_BIT>  /* also enable shadowing */
   
        stcr    r10, PSR  /* bang */
	FLUSH_PIPELINE

	/* put pointer to regs into r30... r31 will become a simple stack */
	or	r30, r31, r0

	subu	r31, r31, 0x10 /* make some breathing space */
	st 	r30, r31, 0x0c /* store frame pointer on the st */
	st 	r30, r31, 0x08 /* store again for the debugger to recognize */ 
	or.u	r20,  r0, hi16(0x87654321)
	or	r20, r20, lo16(0x87654321)
	st	r20, r31, 0x04
	st	r20, r31, 0x00

	CALL(_C_LABEL(error_reset), r30, r30)

	/* TURN INTERUPTS back on */
	ldcr r1, PSR
	clr  r1, r1, 1<PSR_INTERRUPT_DISABLE_BIT>
	stcr r1, PSR
	FLUSH_PIPELINE

GLOBAL(error_loop2) bsr _error_loop2
/* never returns*/

/*
 * This is part of baddadr (below).
 */
ASGLOBAL(ignore_data_exception)
	/******************************************************\
	*  SR0: pointer to the current thread structure        *
	*  SR1: previous FLAGS reg                *
	*  SR2: free                                           *
	*  SR3: must presere                                   *
	*  FLAGS:  CPU status flags                            *
	\******************************************************/
	xcr   FLAGS, FLAGS, SR1   /* replace SR1, FLAGS */

	/*
	 * For more info, see badaddr() below.
	 *
	 * We just want to jump to "badaddr__return_nonzero" below.
	 *
	 * We don't worry about trashing R2 here because we're
	 * jumping back to the function badaddr() where we're allowd
	 * to blast r2..r9 as we see fit.
	 */

	/* the "+2" below is to set the VALID bit. */
	or.u	r2, r0, hi16(badaddr__return_nonzero + 2)
	or	r2, r2, lo16(badaddr__return_nonzero + 2)
	stcr	r2, SNIP   /* Make it the next instruction to execute */
	
	addu	r2, r2, 4
	stcr	r2, SFIP   /* and the next one after that, too. */
	stcr	r0, SSBR   /* make the scoreboard happy. */
1:   

	/* the following jumps to "badaddr__return_nonzero" in below */
	RTE
#endif /* m88100 */

#ifdef M88110
/*
 * This is part of baddadr (below).
 */
ASGLOBAL(m88110_ignore_data_exception)
	/******************************************************\
	*  SR0: pointer to the current thread structure        *
	*  SR1: previous FLAGS reg                             *
	*  SR2: free                                           *
	*  SR3: must preserve                                  *
	*  FLAGS:  CPU status flags                            *
	\******************************************************/
	xcr   FLAGS, FLAGS, SR1   /* replace SR1, FLAGS */

	/*
	 * For more info, see badaddr() below.
	 *
	 * We just want to jump to "m88110_badaddr__return_nonzero" below.
	 *
	 * We don't worry about trashing R2 here because we're
	 * jumping back to the function badaddr() where we're allowd
	 * to blast r2..r9 as we see fit.
	 */

	or.u	r2, r0, hi16(m88110_badaddr__return_nonzero)
	or	r2, r2, lo16(m88110_badaddr__return_nonzero)
	stcr	r2, EXIP   /* Make it the next instruction to execute */
	
	/* the following jumps to "m88110_badaddr__return_nonzero" below */ 
	NOP
	RTE
#endif /* M88110 */
	

/*
 * extern boolean_t badaddr(unsigned addr, unsigned len)
 *
 * Returns true (non-zero) if the given LEN bytes starting at ADDR are
 * not all currently accessable by the kernel.
 *
 * If all LEN bytes starting at ADDR are accessable, zero is returned.
 *
 * Len may be be 1, 2, or 4.
 *
 * This is implementd by setting a special flag in SR1 before trying to access
 * the given address. If a data access exception is raised, the address
 * is inaccessable. The exception handler will notice the special CPU flag
 * and not try to swap the address in. Rather, it will return to
 * "badaddr__return_nonzero" in this routine so that we may return non-zero
 * to the calling routine.
 *
 * If no fault is raised, we continue to where we return zero to the calling
 * routine (after removing the special CPU flag).
 */

GLOBAL(badaddr)
	/*
	 * Disable interrupts ... don't want a context switch while we're
	 * doing this! Also, save the old PSR in R8 to restore later.
	 */
	ldcr	r8, PSR
	set	r4, r8, 1<PSR_INTERRUPT_DISABLE_BIT>
	FLUSH_PIPELINE
	stcr	r4, PSR

	ldcr	r5, SR1
	set	r5, r5, 1<FLAG_IGNORE_DATA_EXCEPTION>
	/* resetting r5 to SR1 done in the delay slot below. */

	/*
	 * If it's a word we're doing, do that here. Otherwise,
	 * see if it's a halfword.....
	 */
	sub	r6,  r3, 4
	bcnd.n	ne0, r6, badaddr__maybe_halfword
	stcr	r5,  SR1
	FLUSH_PIPELINE

	/*
	 * It's a bad address if it's misaligned.
	 */
	bb1   0, r2, badaddr__return_nonzero
	bb1   1, r2, badaddr__return_nonzero
	/*
	 * The next line will either fault or not. If it faults, execution
	 * will go to:  data_access_handler (see above)
	 * and then to: ignore_data_exception (see above)
	 * and then to: badaddr__return_nonzero (see below)
	 * which will return to the calling function.
	 *
	 * If there is no fault, execution just continues as normal.
	 */
	ld   r5, r2, 0
	FLUSH_PIPELINE
	br.n   badaddr__return
	or   r2, r0, r0   /* indicate a zero (address not bad) return.*/

badaddr__maybe_halfword:
	/* More or less like the code for checking a word above */
	sub   r6, r3, 2
	bcnd   ne0, r6, badaddr__maybe_byte

	/* it's bad if it's misaligned */
	bb1   0, r2, badaddr__return_nonzero

	FLUSH_PIPELINE
	ld.h   r5, r2, 0
	FLUSH_PIPELINE
	br.n   badaddr__return
	or   r2, r0, r0

badaddr__maybe_byte:
	/* More or less like the code for checking a word above */
	sub   r6, r3, 1
	bcnd   ne0, r6, badaddr__unknown_size
	FLUSH_PIPELINE
	ld.b   r5, r2, 0
	FLUSH_PIPELINE
	br.n   badaddr__return
	or   r2, r0, r0
badaddr__unknown_size:
#ifndef NDEBUG
	data
1: 	string "bad length (%d) to badaddr() from 0x%x"
	text
	or.u   r2, r0, hi16(1b)
	or     r2, r2, lo16(1b)
	or   r4, r0, r1
	bsr   _C_LABEL(printf)
	or.u   r2, r0, hi16(1b)
	or     r2, r2, lo16(1b)
	bsr   _C_LABEL(panic)
	/*NOTREACHED*/
#endif

ASGLOBAL(badaddr__return_nonzero)
	or   r2, r0, 1
	/* fall through to badaddr__return */

ASGLOBAL(badaddr__return)
	ldcr   r4, SR1
	clr   r4, r4, 1<FLAG_IGNORE_DATA_EXCEPTION>
	stcr   r4, SR1

	/*
	 * Restore the PSR to what it was before.
	 * The only difference is that we might be enabling interrupts
	 * (which we turned off above).  If interrupts were already off,
	 * we do not want to turn them on now, so we just restore from
	 * where we saved it.
	 */
	FLUSH_PIPELINE
	stcr   r8, PSR
	jmp     r1

ASGLOBAL(m88110_badaddr__return_nonzero)
	/* 
	 * On mc88110, we possibly took an exception 
	 * and we have to clear DSR after the rte
	 * instruction clears the EFRZ bit in 
	 * the PSR. 
	 */
	stcr r0, DSR	/* Clear DSR reg on mc88110 */
	stcr r0, DLAR	/* Clear DLAR reg on mc88110 */
	br.n badaddr__return_nonzero
	stcr r0, DPAR	/* Clear DPAR reg on mc88110 */

/*
******************************************************************************
******************************************************************************
******************************************************************************
*/

#ifdef M88100 

ASGLOBAL(setup_phase_one)
	/***************** REGISTER STATUS BLOCK ***********************\
	* SR0: current thread (if any, null if not)                     *
	* SR1: saved copy of exception-time register now holding FLAGS  *
	* SR2: saved copy of exception-time r1                          *
	* SR3: must be preserved .. may be the exception-time stack     *
	* r1: return address to calling exception handler               *
	* FLAGS: CPU status flags                                       *
	***************************************************             *
	* immediate goal:                                               *
	*    Decide where we're going to put the exception frame.       *
	*   Might be at the end of R31, SR3, or the thread's            *
	*   pcb.                                                        *
	\***************************************************************/
	
	/* Check if we are coming in from a FPU restart exception.
	   If so, the pcb will be in SR3 */
	NOP
	xcr	r1,   r1,   SR2
	NOP
	NOP
	NOP

	bb1	FLAG_ENABLING_FPU, FLAGS, use_SR3_pcb
	/* are we coming in from user mode? If so, pick up thread pcb */
	bb0	FLAG_FROM_KERNEL, FLAGS, pickup_stack

	/* Interrupt in kernel mode, not FPU restart */
ASGLOBAL(already_on_kernel_stack)
	/***************** REGISTER STATUS BLOCK ***********************\
	* SR0: current thread (if any, null if not)                     *
	* SR1: saved copy of exception-time register now holding FLAGS  *
	* SR2: return address to the calling exception handler          *
	* SR3: must be preserved; may be important for other exceptions *
	* FLAGS: CPU status flags                                       *
	***************************************************             *
	* immediate goal:                                               *
	*   We're already on the kernel stack, but not having           *
	*   needed to use SR3. We can just make room on the             *
	*    stack (r31) for our exception frame.                       *
	\***************************************************************/
	subu	r31,  r31,  SIZEOF_EF            /* r31 now our E.F. */
	st	FLAGS,r31,  REG_OFF(EF_FLAGS)    /* save flags */
	st	r1,   r31,  GENREG_OFF(1)        /* save prev. r1 (now r1 free)*/

	ldcr	r1,   SR3                        /* save previous SR3 */
	st	r1,   r31,  REG_OFF(EF_SR3)

	addu	r1,   r31,  SIZEOF_EF            /* save previous r31 */
	br.n	have_pcb
	st 	r1,   r31,  GENREG_OFF(31)


ASGLOBAL(use_SR3_pcb)
	/***************** REGISTER STATUS BLOCK ***********************\
	* SR0: current thread (if any, null if not)                     *
	* SR1: saved copy of exception-time register now holding FLAGS  *
	* SR2: return address to the calling exception handler          *
	* SR3: must be preserved; exception-time stack pointer          *
	* FLAGS: CPU status flags                                       *
	***************************************************             *
	* immediate goal:                                               *
	*   An exception occured while enabling the FPU. Since r31      *
	*   is the user's r31 while enabling the FPU, we had put        *
	*   our pcb pointer into SR3, so make room from                 *
	*   there for our stack pointer.                                *
	*       We need to check if SR3 is the old stack pointer or the *
	*       pointer off to the user pcb. If it pointing to the user *
	*       pcb, we need to pick up the kernel stack. Otherwise     *
	*       we need to allocate a frame upon it.                    *
	*       We look at the EPSR to see if it was from user mode     *
	*       Unfortunately, we have no registers free at the moment  *
	*       But we know register 0 in the pcb frame will always be  *
	*       zero, so we can use it as scratch storage.              *
	*                                                               *
	*                                                               *
	\***************************************************************/
	xcr	r30,  r30,  SR3                  /* r30 = old exception frame */ 
	st	r1,   r30,  GENREG_OFF(0)        /* free up r1 */
	ld	r1,   r30,  REG_OFF(EF_EPSR)     /* get back the epsr */
	bb0.n	PSR_SUPERVISOR_MODE_BIT, r1, 1f  /* if user mode */
	ld	r1,   r30,  GENREG_OFF(0)        /* restore r1 */
	/* we were in kernel mode - dump frame upon the stack */
	st	r0,   r30,  GENREG_OFF(0)        /* repair old frame */
	subu	r30,  r30,  SIZEOF_EF            /* r30 now our E.F. */
	st	FLAGS,r30,  REG_OFF(EF_FLAGS)    /* save flags */
	st	r1,   r30,  GENREG_OFF(1)        /* save prev. r1 (now r1 free) */

	st	r31,  r30,  GENREG_OFF(31)       /* save previous r31 */
	or	r31,  r0,   r30                  /* make r31 our pointer. */ 
	addu	r30,  r30,  SIZEOF_EF            /* r30 now has previous SR3 */ 
	st	r30,  r31,  REG_OFF(EF_SR3)      /* save previous SR3 */
	br.n	have_pcb
	xcr	r30,  r30,  SR3                  /* restore r30 */
1:
	/* we took an exception while restarting the FPU from user space.
	 * Consequently, we never picked up a stack. Do so now.
	 * R1 is currently free (saved in the exception frame pointed at by
	 * r30) */
	or.u	r1,   r0,   hi16(_kstack)
	ld	r1,   r1,   lo16(_kstack)
	addu	r1,   r1,   USIZE-SIZEOF_EF
	st	FLAGS,r1,   REG_OFF(EF_FLAGS)    /* store flags */
	st	r31,  r1,   GENREG_OFF(31)       /* store r31 - now free */ 
	st	r30,  r1,   REG_OFF(EF_SR3)      /* store old SR3 (pcb) */
	or	r31,  r1,   r0                   /* make r31 our exception frame pointer */
	ld	r1,   r30,  GENREG_OFF(0)        /* restore old r1 */
	st	r0,   r30,  GENREG_OFF(0)        /* repair that frame */
	st	r1,   r31,  GENREG_OFF(1)        /* store r1 in its proper place */ 
	br.n	have_pcb
	xcr	r30,  r30,  SR3                  /* restore r30 */

ASGLOBAL(pickup_stack)
	/***************** REGISTER STATUS BLOCK ***********************\
	* SR0: current thread                                          *
	* SR1: saved copy of exception-time register now holding FLAGS *
	* SR2: return address to the calling exception handler         *
	* SR3: free                                                    *
	* FLAGS: CPU status flags                                      *
	***************************************************            *
	* immediate goal:                                              *
	*    Since we're servicing an exception from user mode, we     *
	*   know that SR3 is free.  We use it to free up a temp.       *
	*   register to be used in getting the thread's pcb            *
	\***************************************************************/
	stcr	r31,  SR3      /* save previous r31 */

	/* switch to the thread's kernel stack. */
	or.u	r31,  r0,   hi16(_curpcb)
	ld	r31,  r31,  lo16(_curpcb)
	addu	r31,  r31,  PCB_USER_STATE       /* point to user save area */ 
	st	FLAGS,r31,  REG_OFF(EF_FLAGS)    /* save flags */
	st	r1,   r31,  GENREG_OFF(1)        /* save prev. r1 (now r1 free)*/ 
	ldcr	r1,   SR3                        /* save previous r31 */
	st	r1,   r31,  GENREG_OFF(31)
	/*FALLTHROUGH */

ASGLOBAL(have_pcb)
	/***************** REGISTER STATUS BLOCK ***********************\
	* SR0: current thread                                          *
	* SR1: saved copy of exception-time register now holding FLAGS *
	* SR2: return address to the calling exception handler         *
	* SR3: free                                                    *
	* r1:  free                                                    *
	* FLAGS: CPU status flags                                      *
	* r31: our exception frame                                     *
	* Valid in the exception frame:                                *
	*   Exception-time r1, r31, FLAGS.                             *
	*   Exception SR3, if appropriate.                             *
	***************************************************            *
	* immediate goal:                                              *
	*    Save the shadow registers that need to be saved to        *
	*   the exception frame.                                       *
	\***************************************************************/
	stcr	TMP, SR3   /* free up TMP, TMP2, TMP3 */
	SAVE_TMP2
	SAVE_TMP3

	/* save some exception-time registers to the exception frame */
	ldcr	TMP,  EPSR
	st	TMP,  r31,  REG_OFF(EF_EPSR)
	ldcr	TMP3, SNIP
	st	TMP3, r31,  REG_OFF(EF_SNIP)
	ldcr	TMP2, SFIP
	st	TMP2, r31,  REG_OFF(EF_SFIP)
	/* get and store the cpu number */	
	extu	TMP,  FLAGS,  FLAG_CPU_FIELD_WIDTH<0>  /* TMP = cpu# */
	st	TMP,  r31,  REG_OFF(EF_CPU)
   
	/*
	 * Save Pbus fault status register from data and inst CMMU.
	 */
#ifdef MVME188
	/* check if it's a mvme188 */
	or.u	TMP,  r0,   hi16(_brdtyp)
	ld	TMP2, TMP,  lo16(_brdtyp)
	cmp	TMP,  TMP2, BRD_188
	bb1	ne,   TMP,  5f
	
	extu	TMP,  FLAGS, FLAG_CPU_FIELD_WIDTH<0>	/* TMP = cpu# */
	cmp	TMP2, TMP, 0x0				/* CPU0 ? */     
	bb1	eq,   TMP2, 1f
	cmp	TMP2, TMP, 0x1				/* CPU1 ? */     
	bb1	eq,   TMP2, 2f
	cmp	TMP2, TMP, 0x2				/* CPU2 ? */     
	bb1	eq,   TMP2, 3f
	cmp	TMP2, TMP, 0x3				/* CPU3 ? */     
	bb1	eq,   TMP2, 4f
	/* Arrrrg! bad cpu# */ 
	br	Lbadcpupanic				
1:
	/* must be CPU0 */
	or.u	TMP,  r0,   hi16(VME_CMMU_I0)
	ld	TMP2, TMP,  lo16(VME_CMMU_I0) + 0x108
	st	TMP2, r31,  REG_OFF(EF_IPFSR)
	or.u	TMP,  r0,   hi16(VME_CMMU_D0)
	ld	TMP2, TMP,  lo16(VME_CMMU_D0) + 0x108
	st	TMP2, r31,  REG_OFF(EF_DPFSR)
	br	pfsr_done
2:
	/* must be CPU1 */
	or.u	TMP,  r0,   hi16(VME_CMMU_I1)
	ld	TMP2, TMP,  lo16(VME_CMMU_I1) + 0x108
	st	TMP2, r31,  REG_OFF(EF_IPFSR)
	or.u	TMP,  r0,   hi16(VME_CMMU_D1)
	ld	TMP2, TMP,  lo16(VME_CMMU_D1) + 0x108
	st	TMP2, r31,  REG_OFF(EF_DPFSR)
	br	pfsr_done
3:
	/* must be CPU2 */
	or.u	TMP,  r0,   hi16(VME_CMMU_I2)
	ld	TMP2, TMP,  lo16(VME_CMMU_I2) + 0x108
	st	TMP2, r31,  REG_OFF(EF_IPFSR)
	or.u	TMP,  r0,   hi16(VME_CMMU_D2)
	ld	TMP2, TMP,  lo16(VME_CMMU_D2) + 0x108
	st	TMP2, r31,  REG_OFF(EF_DPFSR)
	br	pfsr_done
4:
	/* must be CPU3 */
	or.u	TMP,  r0,   hi16(VME_CMMU_I3)
	ld	TMP2, TMP,  lo16(VME_CMMU_I3) + 0x108
	st	TMP2, r31,  REG_OFF(EF_IPFSR)
	or.u	TMP,  r0,   hi16(VME_CMMU_D3)
	ld	TMP2, TMP,  lo16(VME_CMMU_D3) + 0x108
	st	TMP2, r31,  REG_OFF(EF_DPFSR)
	br	pfsr_done
5: 
#endif   /* MVME188 */
   /* it's a single processor SBC   */  
	or.u	TMP,  r0,   hi16(SBC_CMMU_I)
	ld	TMP2, TMP,  lo16(SBC_CMMU_I) + 0x108
	st	TMP2, r31,  REG_OFF(EF_IPFSR)
	or.u	TMP,  r0,   hi16(SBC_CMMU_D)
	ld	TMP2, TMP,  lo16(SBC_CMMU_D) + 0x108
	st	TMP2, r31,  REG_OFF(EF_DPFSR)

ASGLOBAL(pfsr_done)
	ldcr	TMP,  SSBR
	ldcr	TMP2, SXIP
	ldcr	TMP3, DMT0
	st	TMP2, r31,  REG_OFF(EF_SXIP)
   
#if 0
   /*
    * The following is a kludge so that
    * a core file will have a copy of
    * DMT0 so that 'sim' can display it
    * correctly.
    * After a data fault has been noticed,
    * the real EF_DTM0 is cleared, so I need
    * to throw this somewhere.
    * There's no special reason I chose this
    * register (FPIT)... it's just one of many
    * for which it causes no pain to do this.
    */
	st	TMP3, r31,  REG_OFF(EF_FPIT)
#endif

   /*
    * The above shadow registers are obligatory for any and all
    * exceptions.  Now, if the data access pipeline is not clear,
    * we must save the DMx shadow registers, as well as clear
    * the appropriate SSBR bits for the destination registers of
    * loads or xmems.
    */
	bb0.n	DMT_VALID_BIT, TMP3, DMT_check_finished
	st	TMP3, r31,  REG_OFF(EF_DMT0)

	ldcr	TMP2, DMT1
	ldcr	TMP3, DMT2
	st	TMP2, r31,  REG_OFF(EF_DMT1)
	st	TMP3, r31,  REG_OFF(EF_DMT2)

	ldcr	TMP2, DMA0
	ldcr	TMP3, DMA1
	st	TMP2, r31,  REG_OFF(EF_DMA0)
	st	TMP3, r31,  REG_OFF(EF_DMA1)

	ldcr	TMP2, DMA2
	ldcr	TMP3, DMD0
	st	TMP2, r31,  REG_OFF(EF_DMA2)
	st	TMP3, r31,  REG_OFF(EF_DMD0)

	tb1	0,    r0,   0
	ldcr	TMP2, DMD1
	ldcr	TMP3, DMD2
	st	TMP2, r31,  REG_OFF(EF_DMD1)
	st	TMP3, r31,  REG_OFF(EF_DMD2)

   /*
    *---------------------------------------------------------------
    * need to clear "appropriate" bits in the SSBR before
    * we restart the FPU
    */

ASGLOBAL(check_DMT0)
	ldcr	TMP2, DMT0
	bb0.n	DMT_VALID_BIT, TMP2, DMT_check_finished
	stcr	r0,   DMT0 /* so an exception at fpu_enable doesn't see our DMT0*/ 
	bb1	DMT_LOCK_BIT,  TMP2, do_DMT0
	bb1	DMT_WRITE_BIT, TMP2, check_DMT1

ASGLOBAL(do_DMT0)
	extu	TMP2, TMP2, DMT_DREG_WIDTH <DMT_DREG_OFFSET>
	set	TMP2, TMP2, 1<5>
	clr	TMP,  TMP,  TMP2

ASGLOBAL(check_DMT1)
	ldcr	TMP2, DMT1
	bb0	DMT_VALID_BIT, TMP2, check_DMT2
	bb1	DMT_LOCK_BIT,  TMP2, do_DMT1
	bb1	DMT_WRITE_BIT, TMP2, check_DMT2

ASGLOBAL(do_DMT1)
	extu	TMP2, TMP2, DMT_DREG_WIDTH <DMT_DREG_OFFSET>
	set	TMP2, TMP2, 1<5>
	clr	TMP,  TMP,  TMP2

ASGLOBAL(check_DMT2)
	ldcr	TMP2, DMT2
	bb0	DMT_VALID_BIT, TMP2, DMT_check_finished
	bb1	DMT_LOCK_BIT,  TMP2, do_DMT2_single
	bb1	DMT_WRITE_BIT, TMP2, DMT_check_finished
	bb1	DMT_DOUBLE_BIT,TMP2, do_DMT2_double

ASGLOBAL(do_DMT2_single)
	extu	TMP2, TMP2, DMT_DREG_WIDTH <DMT_DREG_OFFSET>
	br.n	1f
	set	TMP2, TMP2, 1<5>

ASGLOBAL(do_DMT2_double)
	extu	TMP2, TMP2, DMT_DREG_WIDTH <DMT_DREG_OFFSET>
	set	TMP2, TMP2, 1<6>
1:	clr	TMP,  TMP,  TMP2

ASGLOBAL(DMT_check_finished)
	/***************** REGISTER STATUS BLOCK ***********************\
	* SR0: current thread                                          *
	* SR1: saved copy of exception-time register now holding FLAGS *
	* SR2: return address to the calling exception handler         *
	* SR3: saved TMP                                               *
	* r1:  free                                                    *
	* TMP: possibly revised SSBR                                   *
	* TMP2: free                                                   *
	* TMP3: free                                                   *
	* FLAGS: CPU status flags                                      *
	* r31: exception frame                                         *
	*    Valid in the exception frame:                             *
	*   Exception-time r1, r31, FLAGS.                             *
	*   Exception-time TMP2, TMP3.                                 *
	*   Exception-time espr, sfip, snip, sxip.                     *
	*   Dmt0.                                                      *
	*   Other data pipeline control registers, if appropriate.     *
	*   Exception SR3, if appropriate.                             *
	\***************************************************************/
	ldcr	r1,   SR2
	jmp	r1 /* return to allow the handler to clear more SSBR bits */

#endif /* m88100 */

/************************************************************************/
/************************************************************************/

ASGLOBAL(clear_FPi_ssbr_bit)
	/*
	 * Clear floatingpont-imprecise ssbr bits.
	 * Also, save appropriate FPU control registers to the E.F.
	 *
	 *  r1:  return address to calling exception handler
	 *  TMP : (possibly) revised ssbr
	 *  TMP2 : free
	 *  TMP3 : free
	 */
	fldcr	TMP2, FPSR
	fldcr	TMP3, FPCR
	st	TMP2, r31,  REG_OFF(EF_FPSR)
	st	TMP3, r31,  REG_OFF(EF_FPCR)

	fldcr	TMP2, FPECR
	fldcr	TMP3, FPRH
	st	TMP2, r31,  REG_OFF(EF_FPECR)
	st	TMP3, r31,  REG_OFF(EF_FPRH)

	fldcr 	TMP2, FPIT
	fldcr	TMP3, FPRL
	st	TMP2, r31,  REG_OFF(EF_FPIT)
	st	TMP3, r31,  REG_OFF(EF_FPRL)

	/*
	 * We only need clear the bit in the SSBR for the
	 * 2nd reg of a double result [see section 6.8.5]
	 */
#define FPIT_SIZE_BIT   10
	bb0	FPIT_SIZE_BIT, TMP2, not_double_fpi
	extu	TMP2, TMP2, 5<0>  /* get the reg. */
	set	TMP2, TMP2, 1<6>  /* set width (width=2 will clear two bits) */ 
	clr   	TMP,  TMP,  TMP2

ASGLOBAL(not_double_fpi)
	jmp   r1


/************************************************************************/
/************************************************************************/


ASGLOBAL(clear_FPp_ssbr_bit)
	/*
	 * Clear floating pont precise ssbr bits.
	 * Also, save appropriate FPU control registers to the E.F.
	 *
	 *  r1:  return address to calling exception handler
	 *  TMP : (possibly) revised ssbr
	 *  TMP2 : free
	 *  TMP3 : free
	 */
	fldcr TMP2, FPSR
	fldcr TMP3, FPCR
	st    TMP2, r31,  REG_OFF(EF_FPSR)
	st    TMP3, r31,  REG_OFF(EF_FPCR)
	
	fldcr TMP3, FPECR
	st    TMP3, r31,  REG_OFF(EF_FPECR)
	fldcr TMP2, FPHS1
	fldcr TMP3, FPHS2
	st    TMP2, r31,  REG_OFF(EF_FPHS1)
	st    TMP3, r31,  REG_OFF(EF_FPHS2)
	
	fldcr TMP2, FPLS1
	fldcr TMP3, FPLS2
	st    TMP2, r31,  REG_OFF(EF_FPLS1)
	st    TMP3, r31,  REG_OFF(EF_FPLS2)
	
	fldcr TMP2, FPPT
	st    TMP2, r31,  REG_OFF(EF_FPPT)
1:   

#define FPPT_SIZE_BIT   5
	bb1.n FPPT_SIZE_BIT, TMP2, 2f
	extu  TMP3, TMP2, 5<0> /* get FP operation dest reg */
	br.n  3f
	set   TMP3, TMP3, 1<5> /* set size=1 -- clear one bit for "float" */
2:	set   TMP3, TMP3, 1<6> /* set size=2 -- clear two bit for "double" */ 
3:	clr   TMP,  TMP,  TMP3   /* clear bit(s) in ssbr. */
4:	jmp   r1


/************************************************************************/
/************************************************************************/

ASGLOBAL(clear_dest_ssbr_bit)
	/*
	 * There are various cases where an exception can leave the
	 * destination register's bit in the SB set.
	 * Examples:
	 *   misaligned or privilege exception on a LD or XMEM
	 *   DIV or DIVU by zero.
	 *
	 * I think that if the instruction is LD.D, then two bits must
	 * be cleared.
	 *
	 * Even though there are a number of instructions/exception
	 * combinations that could fire this code up, it's only required
	 * to be run for the above cases.  However, I don't think it'll
	 * ever be a problem to run this in other cases (ST instructions,
	 * for example), so I don't bother checking.  If we had to check
	 * for every possible instruction, this code would be much larger.
	 *
	 * The only checking, then, is to see if it's a LD.D or not.
	 *
	 * At the moment....
	 *  r1:  return address to calling exception handler
	 *  TMP : (possibly) revised ssbr
	 *  TMP2 : free
	 *  TMP3 : free
	 */
	
	ldcr  TMP3, EPSR   /* going to check: user or system memory? */
	ldcr  TMP2, SXIP   /* get the instruction's address */
	bb1.n PSR_SUPERVISOR_MODE_BIT, TMP3, 2f
	clr   TMP2, TMP2, 2<0> /* get rid of valid and error bits. */
	
1:	/* user space load here */
#if ERRATA__XXX_USR
	NOP
	ld.usr TMP2,TMP2, r0      /* get the instruction itself */
	NOP
	NOP
	NOP
	br    3f
#else
	br.n  3f
	ld.usr TMP2,TMP2, r0      /* get the instruction itself */
#endif

2:	/* system space load here */
	ld    TMP2, TMP2, r0      /* get the instruction itself */

3:	/* now have the instruction..... */
	/*
	 * Now see if it's a double load
	 * There are three forms of double load [IMM16, scaled, unscaled],
	 * which can be checked by matching against two templates:
	 *          -- 77776666555544443333222211110000 --
	 *   if (((instruction & 11111100000000000000000000000000) ==
	 *             00010000000000000000000000000000) ;;
	 *       ((instruction & 11111100000000001111110011100000) ==
	 *             11110100000000000001000000000000))
	 *   {
	 *      It's a load double, so
	 *      clear two SSBR bits.
	 *   } else {
	 *      It's not a load double.
	 *      Must be a load single, xmem, or st
	 *      Thus, clear one SSBR bit.
	 *   }
	 */
	/* check the first pattern for ld.d */
	extu  TMP3, TMP2, 16<16>   /* get the upper 16 bits */
	mask  TMP3, TMP3, 0xFC00   /* apply the mask */
	cmp   TMP3, TMP3, 0x1000   /* if this is equal, it's a load double */
	bb1   eq,   TMP3, misaligned_double
	
	/* still could be -- check the second pattern for ld.d */
	/* look at the upper 16 bits first */
	extu  TMP3, TMP2, 16<16>   /* get the upper 16 bits */
	mask  TMP3, TMP3, 0xFC00   /* apply the mask */
	cmp   TMP3, TMP3, 0xF400   /* if equal, it might be a load double */
	bb1   ne,   TMP3, misaligned_single /* not equal, so must be single */
	
	/* now look at the lower 16 bits */
	extu  TMP3, TMP2, 16<0>    /* get the lower 16 bits */
	mask  TMP3, TMP3, 0xFCE0   /* apply the mask */
	cmp   TMP3, TMP3, 0x1000   /* if this is equal, it's a load double */
	bb1   eq,   TMP3, misaligned_double
	
ASGLOBAL(misaligned_single)
	extu  TMP2, TMP2, 5<21>    /* get the destination register */
	br.n  1f
	set   TMP2, TMP2, 1<5>     /* set size=1 */

ASGLOBAL(misaligned_double)
	extu  TMP2, TMP2, 5<21>    /* get the destination register */
	set   TMP2, TMP2, 1<6>     /* set size=2 -- clear two bit for "ld.d" */

1:	jmp.n   r1
	clr   TMP,  TMP,  TMP2     /* clear bit(s) in ssbr. */

/************************************************************************/
/************************************************************************/

#ifdef M88100

ASGLOBAL(setup_phase_two)
	/***************** REGISTER STATUS BLOCK ***********************\
	* SR0: saved return address to calling exception handler        *
	* SR1: saved copy of exception-time register now holding FLAGS  *
	* SR2: free                                                     *
	* SR3: saved TMP                                                *
	* r1:  return address to calling exception handler              *
	* TMP: possibly revised SSBR                                    *
	* TMP2: free                                                    *
	* TMP3: free                                                    *
	* FLAGS: CPU status flags                                       *
	* r31: our exception frame                                      *
	*    Valid in the exception frame:                              *
	*   Exception-time r1, r31, FLAGS.                              *
	*   Exception-time TMP2, TMP3.                                  *
	*   Exception-time espr, sfip, snip, sxip.                      *
	*   Exception number (EF_VECTOR).                               *
	*   Dmt0                                                        *
	*   Other data pipeline control registers, if appropriate.      *
	*   FPU control registers, if appropriate.                      *
	*   Exception SR3, if appropriate.                              *
	***************************************************             *
	* immediate goal:                                               *
	*   restore the system to the exception-time state (except      *
	* SR3 will be OUR stack pointer) so that we may resart the FPU. *
	\***************************************************************/
	/*stcr   r1, SR0*/      /* save return address */
	
	stcr  TMP,  SSBR   /* done with SSBR, TMP now free */
	RESTORE_TMP2      /* done with extra temp regs */
	RESTORE_TMP3      /* done with extra temp regs */
	
	/* Get the current PSR and modify for the rte to enable the FPU */
#if 1
	ldcr  TMP,  PSR
	clr   TMP,  TMP,  1<PSR_FPU_DISABLE_BIT>    /* enable the FPU */
	clr   TMP,  TMP,  1<PSR_SHADOW_FREEZE_BIT>  /* also enable shadowing */
	stcr  TMP,  EPSR
	
	/* the "+2" below is to set the VALID_BIT */
	or.u  TMP,  r0,   hi16(fpu_enable +2)
	or    TMP,  TMP,  lo16(fpu_enable +2)
	stcr  TMP,  SNIP      /* jump to here fpu_enable */
	addu  TMP,  TMP,  4
	stcr  TMP,  SFIP      /* and then continue after that */
#else
	ldcr  TMP,  PSR
	or.u  TMP,  TMP,  0x8000    /* set supervisor mode */
	and   TMP,  TMP,  0xfff7    /* also enable shadowing */
	stcr  TMP,  EPSR
	stcr  r0,   SXIP       /* clear valid bit */
	stcr  r0,   SNIP       /* clear valid bit */
	or.u  TMP,  r0,   hi16(fpu_enable)
	or    TMP,  TMP,  lo16(fpu_enable)
	or    TMP,  TMP,  0x2 /* set the VALID_BIT and clear Exception bit */
	stcr  TMP,  SFIP      /* jump to here fpu_enable */
#endif

setup_phase_two_cont:
	set   FLAGS, FLAGS, 1<FLAG_ENABLING_FPU> /* note what we're doing.*/
	xcr   FLAGS, FLAGS, SR1
	st    r1,   r31,  REG_OFF(EF_RET) /* save the return address */
	ld    r1,   r31,  GENREG_OFF(1)    /* get original r1 */
	
	xcr   TMP,  r31,  SR3   /* TMP now restored. R31 now saved in SR3 */
	ld    r31,  r31,  GENREG_OFF(31) /* get original r31 */
	
	/***************** REGISTER STATUS BLOCK ***********************\
	* SR0: current thread                                           *
	* SR1: CPU flags                                                *
	* SR2: free                                                     *
	* SR3: pointer to our exception frame (our stack pointer)       *
	* r1 through r31: original exception-time values                *
	*                                                               *
	* Valid in the exception frame:                                 *
	*   Exception-time FLAGS.                                       *
	*   Exception-time espr, sfip, snip, sxip.                      *
	*   Exception number (EF_VECTOR).                               *
	*   Dmt0                                                        *
	*   Other data pipeline control registers, if appropriate.      *
	*   FPU control registers, if appropriate.                      *
	*   Exception SR3, if appropriate.                              *
	*   Held temporarly in the exception frame:                     *
	*   Return address to the calling excption handler.             *
	***************************************************             *
	* immediate goal:                                               *
	*   Do an RTE to restart the fpu and jump to "fpu_enable"       *
	*   Another exception (or exceptions) may be raised in          *
	*   this, which is why FLAG_ENABLING_FPU is set in SR1.         *
	\***************************************************************/
	
	RTE   /* jumps to "fpu_enable" on the next line to enable the FPU. */
	
ASGLOBAL(fpu_enable)
	FLUSH_PIPELINE
	xcr   TMP,  TMP,  SR3               /* get E.F. pointer */
	st.d  r30,  TMP,  GENREG_OFF(30)    /* save previous r30, r31 */
	or    r31,  TMP,  r0                /* transfer E.F. pointer to r31 */
	ld    TMP,  r31,  REG_OFF(EF_SR3)   /* get previous SR3; maybe important*/
	
	/* make sure that the FLAG_ENABLING_FPU bit is off */
	xcr   FLAGS,FLAGS,SR1
	clr   FLAGS,FLAGS,1<FLAG_ENABLING_FPU>
	xcr   FLAGS,FLAGS,SR1
	
	xcr   TMP,  TMP,  SR3       /* replace TMP, SR3 */
	
	/* now save all regs to the exception frame. */
	st.d  r0 ,  r31,  GENREG_OFF(0)
	st.d  r2 ,  r31,  GENREG_OFF(2)
	st.d  r4 ,  r31,  GENREG_OFF(4)
	st.d  r6 ,  r31,  GENREG_OFF(6)
	st.d  r8 ,  r31,  GENREG_OFF(8)
	st.d  r10,  r31,  GENREG_OFF(10)
	st.d  r12,  r31,  GENREG_OFF(12)
	st.d  r14,  r31,  GENREG_OFF(14)
	st.d  r16,  r31,  GENREG_OFF(16)
	st.d  r18,  r31,  GENREG_OFF(18)
	st.d  r20,  r31,  GENREG_OFF(20)
	st.d  r22,  r31,  GENREG_OFF(22)
	st.d  r24,  r31,  GENREG_OFF(24)
	st.d  r26,  r31,  GENREG_OFF(26)
	st.d  r28,  r31,  GENREG_OFF(28)
#ifdef JEFF_DEBUG
	/* mark beginning of frame with notable value */
	or.u  r20,  r0,   hi16(0x12345678)
	or    r20,  r20,  lo16(0x12345678)
	st    r20,  r31,  GENREG_OFF(0)
#endif

	/* get and save IPL */
	bsr	_C_LABEL(getipl)
	st	r2, r31, REG_OFF(EF_MASK)
	
	/***************** REGISTER STATUS BLOCK ***********************\
	* SR0: current thread                                           *
	* SR1: free                                                     *
	* SR2: free                                                     *
	* SR3: previous exception-time SR3                              *
	* r1: return address to the calling exception handler           *
	* r2 through r30: free                                          *
	* r31: our exception frame                                      *
	*                                                               *
	* Valid in the exception frame:                                 *
	*   Exception-time r0 through r31.                              *
	*   Exception-time FLAGS.                                       *
	*   Exception-time espr, sfip, snip, sxip.                      *
	*   Exception number (EF_VECTOR).                               *
	*   Dmt0                                                        *
	*   Other data pipeline control registers, if appropriate.      *
	*   FPU control registers, if appropriate.                      *
	*   Exception SR3, if appropriate.                              *
	***************************************************             *
	* immediate goal:                                               *
	*   Pick up a stack if we came in from user mode. Put           *
	*       A copy of the exception frame pointer into r30          *
	*       bump the stack a doubleword and write the exception     *
	*       frame pointer.                                          *
	*   if not an interrupt exception,                              *
	*       Turn on interrupts and service any outstanding          *
	*       data access exceptions.                                 *
	*   Return to calling exception handler to                      *
	*   service the exception.                                      *
	\***************************************************************/
	
	/*
	 * If it's not the interrupt exception, enable interrupts and
	 * take care of any data access exceptions......
	 */
#if INTSTACK
	/*
	 * If interrupt exception, switch to interrupt stack if not
	 * already there. Else, switch to kernel stack.
	 */
#endif
	or    r30,  r0,   r31		/* get a copy of the e.f. pointer */ 
	ld    r2,   r31,  REG_OFF(EF_EPSR)
	bb1   PSR_SUPERVISOR_MODE_BIT, r2, 1f  /* If in kernel mode */

#if INTSTACK
	ld    r3,   r31,  REG_OFF(EF_VECTOR)
	cmp   r3,   r3,   1      /* is interrupt ? */
	bb0   eq,   r3,   2f
	or.u  r31,  r0,   hi16(_intstack_end)   /* swith to int stack */
	or    r31,  r31,  lo16(_intstack_end)
	br    3f
2:
#endif
	or.u  r31,  r0,   hi16(_kstack)
	ld    r31,  r31,  lo16(_kstack)
	addu  r31,  r31,  USIZE        /* point at proper end */
	br    3f
1:
#if INTSTACK
	ld    r3,   r31,  REG_OFF(EF_VECTOR)
	cmp   r3,   r3,   1       /* is interrupt ? */
	bb0   eq,   r3,   3f      /* no, we will stay on kern stack */
	or.u  r31,  r0,   hi16(_intstack_end)   /* swith to int stack */
	or    r31,  r31,  lo16(_intstack_end)
#endif /* INTSTACK */
	/* This label is here for debugging */
exception_handler_has_ksp:  global exception_handler_has_ksp
3:	/*
	 * here - r30 holds a pointer to the exception frame.
	 * r31 is a pointer to the kernel stack/interrupt stack.
	*/
	subu  r31,  r31,  8  /* make some breathing space */
	st    r30,  r31,  0  /* store frame pointer on the stack */
#if DDB
	st    r30,  r31,  4  /* store it again for the debugger to recognize */
#endif /* DDB */

	ld    r2,   r30,  REG_OFF(EF_VECTOR)
	bcnd.n eq0, r2,   return_to_calling_exception_handler  /* is error */
	ld    r14,  r30,  REG_OFF(EF_RET)
	cmp   r3,   r2,   1 /* interrupt is exception #1 ;Is an interrupt? */
	bb1.n eq,   r3,   return_to_calling_exception_handler /* skip if so */

#if DDB
	cmp   r3,   r2,   130 /* DDB break exception */
	bb1.n eq,   r3,   return_to_calling_exception_handler
	cmp   r3,   r2,   132 /* DDB entry exception */
	bb1.n eq,   r3,   return_to_calling_exception_handler
#endif

	ldcr  r2,   PSR
	clr   r2,   r2,   1<PSR_INTERRUPT_DISABLE_BIT>   /* enable interrupts */
	stcr  r2,   PSR
#if     DDB
	FLUSH_PIPELINE
#endif

	/* service any outstanding data pipeline stuff
	   - check dmt0 anything outstanding?*/

	ld    r3,   r30,  REG_OFF(EF_DMT0)
	bb0   DMT_VALID_BIT, r3, return_to_calling_exception_handler

	/*
	  r30 can be clobbered by calls. So stuff its value into a
	  preserved register, say r15. R14 is in use (see return_to_... below). 
	 */
	or    r15,  r0,   r30

	CALL(_C_LABEL(m88100_trap), T_DATAFLT, r15)
	CALL(_data_access_emulation, r15, r0)

	/*   restore it... */
	or    r30,  r0,   r15

	/* clear the dmt0 word in the E.F */
	st    r0,   r30,  REG_OFF(EF_DMT0)

ASGLOBAL(return_to_calling_exception_handler)
	jmp   r14 /* loaded above */
#endif /* m88100 */


/*
 * proc_trampoline.
 * When a process setup by cpu_set_kpc() resumes, it will find itself in
 * proc_trampoline, with r31 pointing to a ksigframe. proc_trampoline will
 * load func and proc values from ksigframe, call the function, and on return
 * pop off the ksigframe. Then, it will load pc from the switchframe and
 * jump there.
 */

ENTRY(proc_trampoline)
	ld	r1,r31,0	/* load func */
	ld	r2,r31,4 	/* load proc pointer */
	jsr.n	r1
	subu	r31,r31,40	/* create stack space for function */
	addu	r31,r31,48	/* stack space above + ksigframe */
	ld	r1, r31,0	/* load pc */
	ld	r2, r31,4	/* & proc pointer from switch frame */ 
	jsr.n	r1
	addu	r31,r31,8
	bsr	_C_LABEL(panic)

/*
 * proc_do_uret 
 * this is called as proc_do_uret(proc) from proc_trampoline(). This function
 * loads r31 with a pointer to the trap frame for the given proc and calls
 * return_from_exception_handler which loads all the registers and does an
 * rte.
 */

ENTRY(proc_do_uret)
	ld	r3,r2,P_ADDR		/* p->p_addr */
	addu	r3,r3,PCB_USER_STATE	/* p->p_addr.u_pcb.user_state */
	st	r3,r31,0		/* put it on the stack */
	br	return_from_exception_handler

ASGLOBAL(return_from_exception_handler)
	/*
	 * Regs r1-r30 are free. R31 is pointing at the word
	 * on the kernel stack where our pointer to the exception frame
	 * it stored. Reload it now.
	 *
	 * At this point, if EF_DMT0 is not zero, then
	 * this must have been an interrupt where the fault didn't
	 * get corrected above.  We'll do that now.
	 *
	 * We load it into r14 since it is preserved across function
	 * calls, and we may have to call some routines from within here.
	 *
	 * control is transfered here from obvious places in this file
	 * and thread_bootstrap in luna88k/locore.c.
	 *
	 */
#ifdef M88110	
	or.u	r2, r0, hi16(_C_LABEL(cputyp))
	ld	r3, r2, lo16(_C_LABEL(cputyp))
	cmp	r2, r3, CPU_88110
	bb1	eq, r2, m88110_return_code        
#endif 
#ifdef M88100
#define FPTR    r14
	ld	FPTR, r31, 0                 /* grab exception frame pointer */ 
	ld	r3, FPTR, REG_OFF(EF_DMT0)
	bb0	DMT_VALID_BIT, r3, check_ast /*[Oh well, nothing to do here] */

#if 1
	/*
	 * This might happen for non-interrupts  If the user sets DMT0
	 * in an exception handler.........
	 */
	ld	r2, FPTR, REG_OFF(EF_VECTOR)
	cmp	r2, r2, 1 /* interrupt is exception #1 ; Is an interrupt? */ 
	bb1	eq, r2, 1f
	or.u	r4, r0, hi16(2f)
	or	r4, r4, lo16(2f)
#if DDB
	CALL(_C_LABEL(db_printf), r4, r0)
	tb0	0, r0, 132
#endif
	br 1f
	data
2:	string "OOPS: DMT0 not zero and not interrupt.\n\000"
	align 8
	text
1:
#endif
	/*
	 * If it's the interrupt exception, enable interrupt.
	 * Take care of any data access exception...... 90/8/15 add by yama
	 */
	
	/*
	 * Is it ever possible to have interrupt exception while EPSR has
	 * it disabled? I don't think so.. XXX nivas
	 */
	ld    r2, FPTR, REG_OFF(EF_VECTOR)
	cmp   r2, r2, 1   /* interrupt is exception #1 ; Is an interrupt? */
        bb1   ne, r2, 1f  /* If not so, skip */

	/* if EPSR has interrupts disabled, skip also */
	ld    r2, FPTR, REG_OFF(EF_EPSR)
        bb1   PSR_INTERRUPT_DISABLE_BIT, r2, 1f		/* skip if disabled */
	ldcr  r2, PSR
	clr   r2, r2, 1<PSR_INTERRUPT_DISABLE_BIT>	/* enable interrupts */
	FLUSH_PIPELINE
        stcr  r2, PSR
1:
	ld    r2, FPTR, REG_OFF(EF_DMT0)
	bb0   DMT_VALID_BIT, r2, 2f

	/*
	 * if there happens to be a data fault that hasn't been serviced yet,
	 * go off and service that...
	 */
	CALL(_C_LABEL(m88100_trap), T_DATAFLT, r30)
	CALL(_C_LABEL(data_access_emulation), r30, r0) /* really only 2 args */
	/* clear the dmt0 word in the E.F. */
	st   r0 , FPTR, REG_OFF(EF_DMT0)
2:
	br   check_ast
#endif /* m88100 */

#ifdef M88110
ASLOCAL(m88110_return_code)
#define FPTR    r14
	ld	FPTR, r31, 0                 /* grab exception frame pointer */ 

	/*
	 * If it's the interrupt exception, enable interrupt.
	 * If it's the data access exception, take care of it.
	 */

	/*
	 * Is it ever possible to have interrupt exception while EPSR has
	 * it disabled? I don't think so.. XXX nivas
	 * 
	 * On mc88110, you can.  The NMI interrupt. aka ABORT. XXX smurph
	 */
	ld	r2, FPTR, REG_OFF(EF_VECTOR)
	cmp	r2, r2, 1   /* Is it an interrupt? */ 
	bb1	ne, r2, 1f  /* If not, skip */

	/* if EPSR has interrupts disabled, skip also */
	ld	r2, FPTR, REG_OFF(EF_EPSR)
	bb1	PSR_INTERRUPT_DISABLE_BIT, r2, 1f  /* skip if disabled */ 
	ldcr	r2, PSR
	clr	r2, r2, 1<PSR_INTERRUPT_DISABLE_BIT>    /* enable interrupts */ 
	FLUSH_PIPELINE
	stcr	r2, PSR
1:
	br 2f	/* temp XXX smurph */
	ld	r2, FPTR, REG_OFF(EF_VECTOR)
	cmp	r2, r2, 3   /* Is it a data access exception? */ 
	bb1	ne, r2, 2f  /* If not, skip */

	/*
	 * if there happens to be a data fault that hasn't been serviced yet,
	 * go off and service that...
	 */
	CALL(_C_LABEL(m88110_trap), T_DATAFLT, r30)

	/* clear the dsr word in the E.F. */
	st	r0, FPTR, REG_OFF(EF_DSR)
	st	r0, FPTR, REG_OFF(EF_DLAR)
	st	r0, FPTR, REG_OFF(EF_DPAR)
2:
#endif /* M88110 */

/*
 *   If the saved ipl is 0, then call dosoftint() to process soft
 *   interrupts.
 *   If returning to user land, look for ASTs
 */
ASLOCAL(check_ast)

        ld      r2, FPTR, REG_OFF(EF_EPSR)   /* get pre-exception PSR */
        bb1     PSR_INTERRUPT_DISABLE_BIT, r2, 1f /* skip if ints off */
        ld      r2, FPTR, REG_OFF(EF_MASK)   /* get pre-exception ipl */
        bcnd    ne0, r2, 1f		/* can't do softint's */
        
	subu	r31, r31, 32
	bsr.n   _C_LABEL(setipl)
        or      r2,r0,1
	/* at ipl 1 now */	
	addu	r31, r31, 32
        bsr     _C_LABEL(dosoftint)
        /* is this needed? we are going to restore the ipl below XXX nivas */
	subu	r31, r31, 32
        bsr.n   _C_LABEL(setipl)
        or      r2,r0,0			/* ints are enabled */
	addu	r31, r31, 32
	/* at ipl 0 now */
1:
        ld      r2, FPTR, REG_OFF(EF_EPSR)   /* get pre-exception PSR */
        bb1     PSR_SUPERVISOR_MODE_BIT, r2, no_ast /*skip if in system mode */

        /* should assert here - not in user mode with ints off XXX nivas */
        /* get and check want_ast */
        or.u	r2, r0, hi16(_C_LABEL(want_ast))
        ld	r3, r2, lo16(_C_LABEL(want_ast))
        bcnd	eq0, r3, no_ast

        /*
         * trap(AST,...) will service ast's.
         */
#if defined(M88110) && defined(M88100)
	or.u	r2, r0, hi16(_C_LABEL(cputyp))
	ld	r3, r2, lo16(_C_LABEL(cputyp))
	cmp	r2, r3, CPU_88110
	bb0	eq, r2, 2f
#endif
#if defined(M88110)
        CALL(_C_LABEL(m88110_trap), T_ASTFLT, FPTR)
#endif
#if defined(M88110) && defined(M88100)
	br	no_ast
2:
#endif
#ifdef M88100
        CALL(_C_LABEL(m88100_trap), T_ASTFLT, FPTR)
#endif

ASLOCAL(no_ast)

        /* disable interrupts */
        ldcr    r1, PSR                 
        set     r1, r1, 1<PSR_INTERRUPT_DISABLE_BIT>
        FLUSH_PIPELINE
        stcr    r1, PSR

        /* now ready to return....*/
	subu	r31, r31, 40
	bsr.n   _C_LABEL(setipl)
	 ld	r2, FPTR, REG_OFF(EF_MASK)   /* get pre-exception ipl */
	addu	r31, r31, 40
        
        /*
         * Transfer the frame pointer to r31, since we no longer need a stack.
         * No page faults here, and interrupts are disabled.
         */
        or      r31, r0,  FPTR
        /* restore r1 later */
        ld.d    r2 , r31, GENREG_OFF(2)
        ld.d    r4 , r31, GENREG_OFF(4)
        ld.d    r6 , r31, GENREG_OFF(6)
        ld.d    r8 , r31, GENREG_OFF(8)
        ld.d    r10, r31, GENREG_OFF(10)
        ld.d    r12, r31, GENREG_OFF(12)
        ld.d    r14, r31, GENREG_OFF(14)
        ld.d    r16, r31, GENREG_OFF(16)
        ld.d    r18, r31, GENREG_OFF(18)
        ld.d    r20, r31, GENREG_OFF(20)
        ld.d    r22, r31, GENREG_OFF(22)
        ld.d    r24, r31, GENREG_OFF(24)
        ld.d    r26, r31, GENREG_OFF(26)
        ld.d    r28, r31, GENREG_OFF(28)
        /* restore r1, r30, r31 later */

        /* disable shadowing */
        ldcr    r1, PSR
        set     r1, r1, 1<PSR_SHADOW_FREEZE_BIT>
        FLUSH_PIPELINE
        stcr    r1, PSR

	/* reload the control regs*/
#ifdef M88110
        or.u	r1, r0, hi16(_C_LABEL(cputyp))
	ld	r30, r1, lo16(_C_LABEL(cputyp))
	cmp	r1, r30, CPU_88110
	bb1	ne, r1, 1f
        /* mc88110 needs the EXIP */
	ld      r30, r31, REG_OFF(EF_ENIP)
        ld      r1,  r31, REG_OFF(EF_EXIP)
        stcr    r30, ENIP
        stcr    r1,  EXIP
	br 	2f      
1:
#endif 
        st      r0, r31, REG_OFF(EF_IPFSR)
        st      r0, r31, REG_OFF(EF_DPFSR)

        /*
         * Note: no need to restore the SXIP.
         * When the "rte" causes execution to continue
         * first with the instruction pointed to by the NIP
         * and then the FIP.
         *
         * See MC88100 Risc Processor User's Manual, 2nd Edition,
         * section 6.4.3.1.2-4
         */
        ld      r30, r31, REG_OFF(EF_SNIP)
        ld      r1,  r31, REG_OFF(EF_SFIP)
        stcr    r0,  SSBR
        stcr    r30, SNIP
        stcr    r1,  SFIP

2:        
	ld      r30, r31, REG_OFF(EF_EPSR)
        ld      r1,  r31, REG_OFF(EF_MODE)
        stcr    r30, EPSR

        /* Now restore r1, r30, and r31 */
        ld      r1,  r31, GENREG_OFF(1)
        ld.d    r30, r31, GENREG_OFF(30)

        RTE

#ifdef M88110 	
/*#########################################################################*/
/*#### THE ACTUAL EXCEPTION HANDLER ENTRY POINTS - MVME197 ################*/
/*#########################################################################*/

/* unknown exception handler */
GLOBAL(m88110_unknown_handler)
	PREP2("unknown", 0, DEBUG_UNKNOWN_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_UNKNOWNFLT, r30)
	DONE(DEBUG_UNKNOWN_BIT)

/* interrupt exception handler */
GLOBAL(m88110_interrupt_handler)
	PREP2("interrupt", 1, DEBUG_INTERRUPT_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_INT, r30)
	DONE(DEBUG_INTERRUPT_BIT)

/* instruction access exception handler */
GLOBAL(m88110_instruction_access_handler)
	PREP2("inst", 2, DEBUG_INSTRUCTION_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_INSTFLT, r30)
	DONE(DEBUG_INSTRUCTION_BIT)
/*
 * data access exception handler --
 *  See badaddr() below for info about Data_Precheck.
 */
GLOBAL(m88110_data_exception_handler)
	PREP2("data", 3, DEBUG_DATA_BIT, No_SSBR_Stuff, M88110_Data_Precheck)
	CALL(_C_LABEL(m88110_trap), T_DATAFLT, r30)
	DONE(DEBUG_DATA_BIT)

/* misaligned access exception handler */
GLOBAL(m88110_misaligned_handler)
	PREP2("misalign", 4, DEBUG_MISALIGN_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_MISALGNFLT, r30)
	DONE(DEBUG_MISALIGN_BIT)

/* unimplemented opcode exception handler */
GLOBAL(m88110_unimplemented_handler)
	PREP2("unimp", 5, DEBUG_UNIMPLEMENTED_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_ILLFLT, r30)
	DONE(DEBUG_UNIMPLEMENTED_BIT)

/* privilege exception handler */
GLOBAL(m88110_privilege_handler)
	PREP2("privilege", 6, DEBUG_PRIVILEGE_BIT, No_SSBR_Stuff, No_Precheck) 
	CALL(_C_LABEL(m88110_trap), T_PRIVINFLT, r30)
	DONE(DEBUG_PRIVILEGE_BIT)

/*
 * I'm not sure what the trap(T_BNDFLT,...) does, but it doesn't send
 * a signal to the process...
 */
GLOBAL(m88110_bounds_handler)
	PREP2("bounds", 7, DEBUG_BOUNDS_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_BNDFLT, r30)
	DONE(DEBUG_BOUNDS_BIT)

/* integer divide-by-zero exception handler */
GLOBAL(m88110_divide_handler)
	PREP2("divide", 8, DEBUG_DIVIDE_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_ZERODIV, r30)
	DONE(DEBUG_DIVIDE_BIT)

/* integer overflow exception handler */
GLOBAL(m88110_overflow_handler)
	PREP2("overflow", 9, DEBUG_OVERFLOW_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_OVFFLT, r30)
	DONE(DEBUG_OVERFLOW_BIT)

/* Floating-point precise handler */
GLOBAL(m88110_fp_precise_handler)
	PREP2("FPU precise", 114, DEBUG_FPp_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_m88110_Xfp_precise, r0, r30) /* call fp_precise(??, exception_frame)*/
	DONE(DEBUG_FPp_BIT)

/* MVME197 non-maskable interrupt handler (ABORT button) */
GLOBAL(m88110_nonmaskable)
	PREP2("MVME197 non-mask", 11, DEBUG_NON_MASK_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_NON_MASK, r30)
	DONE(DEBUG_NON_MASK_BIT)

/* MVME197 data MMU read miss handler */
GLOBAL(m88110_data_read_miss)
	PREP2("MVME197 read miss", 12, DEBUG_197_READ_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_197_READ, r30)
	DONE(DEBUG_197_READ_BIT)

/* MVME197 data MMU write miss handler */
GLOBAL(m88110_data_write_miss)
	PREP2("MVME197 write miss", 13, DEBUG_197_WRITE_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_197_WRITE, r30)
	DONE(DEBUG_197_WRITE_BIT)

/* MVME197 inst MMU ATC miss handler */
GLOBAL(m88110_inst_atc_miss)
	PREP2("MVME197 inst miss", 14, DEBUG_197_INST_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_197_INST, r30)
	DONE(DEBUG_197_INST_BIT)

/* All standard system calls.  */
GLOBAL(m88110_syscall_handler)
	PREP2("syscall", 128, DEBUG_SYSCALL_BIT, No_SSBR_Stuff, No_Precheck)
	ld   r13, r30, GENREG_OFF(13)
	CALL(_m88110_syscall, r13, r30) /* system call no. is in r13 */
	DONE(DEBUG_SYSCALL_BIT)

/* trap 496 comes here */
GLOBAL(m88110_bugtrap)
	PREP2("bugsyscall", 496, DEBUG_BUGCALL_BIT, No_SSBR_Stuff, No_Precheck)
	ld   r9,  r30, GENREG_OFF(9)
	CALL(_bugsyscall, r9, r30)   /* system call no. is in r9 */
	DONE(DEBUG_BUGCALL_BIT)

GLOBAL(m88110_sigsys)
	PREP2("sigsys", 0, DEBUG_SIGSYS_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_SIGSYS, r30)
	DONE(DEBUG_SIGSYS_BIT)

GLOBAL(m88110_sigtrap)
	PREP2("sigtrap", 0, DEBUG_SIGTRAP_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_SIGTRAP, r30)
	DONE(DEBUG_SIGTRAP_BIT)

GLOBAL(m88110_stepbpt)
	PREP2("sigtrap", 0, DEBUG_SIGTRAP_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_STEPBPT, r30)
	DONE(DEBUG_SIGTRAP_BIT)

GLOBAL(m88110_userbpt)
	PREP2("sigtrap", 0, DEBUG_SIGTRAP_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_USERBPT, r30)
	DONE(DEBUG_SIGTRAP_BIT)

#if DDB
GLOBAL(m88110_break)
	PREP2("break", 130, DEBUG_BREAK_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_KDB_BREAK, r30)
	DONE(DEBUG_BREAK_BIT)

GLOBAL(m88110_trace)
	PREP2("trace", 131, DEBUG_TRACE_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_KDB_TRACE, r30)
	DONE(DEBUG_TRACE_BIT)

GLOBAL(m88110_entry)
	PREP2("kdb", 132, DEBUG_KDB_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_KDB_ENTRY, r30)
	DONE(DEBUG_KDB_BIT)
#else /* else not DDB */
GLOBAL(m88110_break)
	PREP2("break", 130, DEBUG_BREAK_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_UNKNOWNFLT, r30)
	DONE(DEBUG_BREAK_BIT)

GLOBAL(m88110_trace)
	PREP2("trace", 131, DEBUG_TRACE_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_UNKNOWNFLT, r30)
	DONE(DEBUG_TRACE_BIT)

GLOBAL(m88110_entry)
	PREP2("unknown", 132, DEBUG_UNKNOWN_BIT, No_SSBR_Stuff, No_Precheck)
	CALL(_C_LABEL(m88110_trap), T_UNKNOWNFLT, r30)
	DONE(DEBUG_KDB_BIT)
#endif   /* DDB */


/*--------------------------------------------------------------------------*/
/*
 * The error exception handler.
 * The error exception is raised when any other non-trap exception is raised
 * while shadowing is off. This is Bad News.
 *
 * The shadow registers are not valid in this case (shadowing was off, ne).
 * R1-R31 may be interesting though, so we'll save them.
 *
 * We'll not worry about trashing r26-29 here,
 * since they aren't generally used.
 */
GLOBAL(m88110_error_handler)
	xcr	r2, r2,	SRX
	or	r2, r0, 10
	stcr	r2, 	SR0
	br.n	_C_LABEL(m88110_fatal)
	xcr	r2, r2,	SRX

/*
 * The reset exception handler.
 * The reset exception is raised when the RST signal is asserted (machine 
 * is reset), the value of VBR is changed after exceptions are enabled, 
 * or when a jmp, br/bsr to addr 0 (accidents do happen :-)
 *
 * Upon a real reset, VBR is set to zero (0), so code must be at addr 0 
 * to handle it!!! 
 *
 * This is totaly different than _error_handler.  Shadowing might or 
 * might not be on.
 * R1-R31 could tell u alot about what happend, so we'll save them.
 *
 * We'll not worry about trashing r26-29 here,
 * since they aren't generally used.
 */
GLOBAL(m88110_reset_handler)
	stcr	r0, 	SR0
	/* FALL THROUGH */
GLOBAL(m88110_fatal)
        /* pick up the slavestack */
	or	r26, r0, r31           /* save old stack */
	or.u	r31, r0,  hi16(_intstack_end)
	or	r31, r31, lo16(_intstack_end)

	/* zero the stack, so we'll know what we're lookin' at */
	or.u	r27, r0,  hi16(_intstack)
	or	r27, r27, lo16(_intstack)
1:	cmp	r28, r27, r31
	bb1	ge,  r28, 2f   /* branch if at the end of the stack */
	st	r0,  r0,  r27
	br.n	1b
	addu	r27, r27, 4   /* bump up */
2:	/* stack has been cleared */

	/* ensure that stack is 8-byte aligned */
        clr     r31, r31, 3<0>  /* round down to 8-byte boundary */

	/* create exception frame on stack */
        subu    r31, r31, SIZEOF_EF             /* r31 now our E.F. */

	/* save old R31 and other R registers */
        st.d    r0 , r31, GENREG_OFF(0)
        st.d    r2 , r31, GENREG_OFF(2)
        st.d    r4 , r31, GENREG_OFF(4)
        st.d    r6 , r31, GENREG_OFF(6)
        st.d    r8 , r31, GENREG_OFF(8)
        st.d    r10, r31, GENREG_OFF(10)
        st.d    r12, r31, GENREG_OFF(12)
        st.d    r14, r31, GENREG_OFF(14)
        st.d    r16, r31, GENREG_OFF(16)
        st.d    r18, r31, GENREG_OFF(18)
        st.d    r20, r31, GENREG_OFF(20)
        st.d    r22, r31, GENREG_OFF(22)
        st.d    r24, r31, GENREG_OFF(24)
        st      r30, r31, GENREG_OFF(30)
	st	r26, r31, GENREG_OFF(31)

	/* vector is put in SRO (either 0 or 10 at this point) */
	ldcr	r10, SR0
	st	r10,  r31, REG_OFF(EF_VECTOR)
	cmp	r10, r10, 0	/* Is it the reset exception? */
	bb1.n	ne,  r10, 1f	/* If not, skip. */
	
	/* save shadow registers (are OLD if error_handler, though) */
	ldcr	r10, EPSR
	st	r10, r31, REG_OFF(EF_EPSR)
	ldcr	r10, EXIP
	st	r10, r31, REG_OFF(EF_EXIP)
	ldcr	r10, ENIP
	st	r10, r31, REG_OFF(EF_ENIP)
	ldcr	r10, DSR
	st	r10, r31, REG_OFF(EF_DSR)
	ldcr	r10, DLAR
	st	r10, r31, REG_OFF(EF_DLAR)
	ldcr	r10, DPAR
	st	r10, r31, REG_OFF(EF_DPAR)
	ldcr	r10, ISR
	st	r10, r31, REG_OFF(EF_ISR)
	ldcr	r10, ILAR
	st	r10, r31, REG_OFF(EF_ILAR)
	ldcr	r10, IPAR
	st	r10, r31, REG_OFF(EF_IPAR)
	ldcr	r10, SR1
	br.n	2f
	st	r10, r31, REG_OFF(EF_MODE)

1:	
	/* retrieve saved shadow registers for error_handler, though) */
	or.u	r30, r0,  hi16(_save_frame)
	or	r30, r30, lo16(_save_frame)
	ld	r10, r30, REG_OFF(EF_EPSR)
	st	r10, r31, REG_OFF(EF_EPSR)
	ld	r10, r30, REG_OFF(EF_EXIP)
	st	r10, r31, REG_OFF(EF_ENIP)
	ld	r10, r30, REG_OFF(EF_DSR)
	st	r10, r31, REG_OFF(EF_DSR)
	ld	r10, r30, REG_OFF(EF_DLAR)
	st	r10, r31, REG_OFF(EF_DLAR)
	ld	r10, r30, REG_OFF(EF_DPAR)
	st	r10, r31, REG_OFF(EF_DPAR)
	ld	r10, r30, REG_OFF(EF_ISR)
	st	r10, r31, REG_OFF(EF_ISR)
	ld	r10, r30, REG_OFF(EF_ILAR)
	st	r10, r31, REG_OFF(EF_ILAR)
	ld	r10, r30, REG_OFF(EF_IPAR)
	st	r10, r31, REG_OFF(EF_IPAR)
	ld	r10, r30, REG_OFF(EF_ISAP)
	st	r10, r31, REG_OFF(EF_ISAP)
	ld	r10, r30, REG_OFF(EF_DSAP)
	st	r10, r31, REG_OFF(EF_DSAP)
	ld	r10, r30, REG_OFF(EF_IUAP)
	st	r10, r31, REG_OFF(EF_IUAP)
	ld	r10, r30, REG_OFF(EF_DUAP)
	st	r10, r31, REG_OFF(EF_DUAP)
	ldcr	r10, SR1
	st	r10, r31, REG_OFF(EF_MODE)
2:
	/* shove sr2 into EF_FPLS1 */
	ldcr	r10, SR2
	st	r10, r31, REG_OFF(EF_FPLS1)

	/* shove sr3 into EF_FPHS2 */
	ldcr	r10, SR3
	st	r10, r31, REG_OFF(EF_FPHS2)

	
	/*
	 * Cheap way to enable FPU and start shadowing again.
	 */
        ldcr    r10, PSR
        clr     r10, r10, 1<PSR_FPU_DISABLE_BIT>    /* enable the FPU */
        clr     r10, r10, 1<PSR_SHADOW_FREEZE_BIT>  /* also enable shadowing */
   
        stcr    r10, PSR  /* bang */
	FLUSH_PIPELINE

	/* put pointer to regs into r30... r31 will become a simple stack */
	or	r30, r31, r0

        subu	r31, r31, 0x10 /* make some breathing space */
        st	r30, r31, 0x0c /* store frame pointer on the st */
        st	r30, r31, 0x08 /* store again for the debugger to recognize */ 
	or.u	r20,  r0, hi16(0x87654321)
	or	r20, r20, lo16(0x87654321)
	st	r20, r31, 0x04
	st	r20, r31, 0x00
        
	CALL(_error_fatal, r30, r30)

        /* TURN INTERUPTS back on */
        ldcr r1, PSR
	clr  r1, r1, 1<PSR_INTERRUPT_DISABLE_BIT>
        stcr r1, PSR
        FLUSH_PIPELINE

ASGLOBAL(m88110_error_loop)
	bsr m88110_error_loop
   /* never returns*/

ASGLOBAL(m88110_setup_phase_one)
	/***************** REGISTER STATUS BLOCK ***********************\
	* SR0: current thread (if any, null if not)                     *
	* SR1: saved copy of exception-time register now holding FLAGS  *
	* SR2: saved copy of exception-time r1                          *
	* SR3: must be preserved .. may be the exception-time stack     *
	* r1: return address to calling exception handler               *
	* FLAGS: CPU status flags                                       *
	***************************************************             *
	* immediate goal:                                               *
	*    Decide where we're going to put the exception frame.       *
	*   Might be at the end of R31, SR3, or the thread's            *
	*   pcb.                                                        *
	\***************************************************************/
	
	/* Check if we are coming in from a FPU restart exception.
	   If so, the pcb will be in SR3 */
	NOP
	xcr   r1,   r1,   SR2
	NOP
	NOP
	NOP
	
	bb1   FLAG_ENABLING_FPU, FLAGS, m88110_use_SR3_pcb
	/* are we coming in from user mode? If so, pick up process pcb */
	bb0   FLAG_FROM_KERNEL, FLAGS, m88110_pickup_stack
	
	/* Interrupt in kernel mode, not FPU restart */
ASGLOBAL(m88110_already_on_kernel_stack)
	/***************** REGISTER STATUS BLOCK ***********************\
	* SR0: current thread (if any, null if not)                     *
	* SR1: saved copy of exception-time register now holding FLAGS  *
	* SR2: return address to the calling exception handler          *
	* SR3: must be preserved; may be important for other exceptions *
	* FLAGS: CPU status flags                                       *
	***************************************************             *
	* immediate goal:                                               *
	*   We're already on the kernel stack, but not having           *
	*   needed to use SR3. We can just make room on the             *
	*    stack (r31) for our exception frame.                       *
	\***************************************************************/
	subu  r31,  r31,  SIZEOF_EF            /* r31 now our E.F. */
	st    FLAGS,r31,  REG_OFF(EF_FLAGS)    /* save flags */
	st    r1,   r31,  GENREG_OFF(1)        /* save prev. r1 (now r1 free)*/
	
	ldcr  r1,   SR3                        /* save previous SR3 */
	st    r1,   r31,  REG_OFF(EF_SR3)
	
	addu  r1,   r31,  SIZEOF_EF            /* save previous r31 */
	br.n  m88110_have_pcb
	st    r1,   r31,  GENREG_OFF(31)
	
	
ASGLOBAL(m88110_use_SR3_pcb)
	/***************** REGISTER STATUS BLOCK ***********************\
	* SR0: current thread (if any, null if not)                     *
	* SR1: saved copy of exception-time register now holding FLAGS  *
	* SR2: return address to the calling exception handler          *
	* SR3: must be preserved; exception-time stack pointer          *
	* FLAGS: CPU status flags                                       *
	***************************************************             *
	* immediate goal:                                               *
	*   An exception occured while enabling the FPU. Since r31      *
	*   is the user's r31 while enabling the FPU, we had put        *
	*   our pcb pointer into SR3, so make room from                 *
	*   there for our stack pointer.                                *
	*       We need to check if SR3 is the old stack pointer or the *
	*       pointer off to the user pcb. If it pointing to the user *
	*       pcb, we need to pick up the kernel stack. Otherwise     *
	*       we need to allocate a frame upon it.                    *
	*       We look at the EPSR to see if it was from user mode     *
	*       Unfortunately, we have no registers free at the moment  *
	*       But we know register 0 in the pcb frame will always be  *
	*       zero, so we can use it as scratch storage.              *
	*                                                               *
	*                                                               *
	\***************************************************************/
	xcr	r2, r2,	SRX
	or	r2, r0, 10
	stcr	r2, 	SR0
	br.n	_C_LABEL(m88110_fatal)
	xcr	r2, r2,	SRX
	/* Testing!!! */
	xcr   r30,  r30,  SR3                  /* r30 = old exception frame */
	st    r1,   r30,  GENREG_OFF(0)        /* free up r1 */
	ld    r1,   r30,  REG_OFF(EF_EPSR)     /* get back the epsr */
	bb0.n PSR_SUPERVISOR_MODE_BIT, r1, 1f  /* if user mode */
	ld    r1,   r30,  GENREG_OFF(0)        /* restore r1 */
	/* we were in kernel mode - dump frame upon the stack */
	st    r0,   r30,  GENREG_OFF(0)        /* repair old frame */
	subu  r30,  r30,  SIZEOF_EF            /* r30 now our E.F. */
	st    FLAGS,r30,  REG_OFF(EF_FLAGS)    /* save flags */
	st    r1,   r30,  GENREG_OFF(1)        /* save prev. r1 (now r1 free) */
	
	st    r31,  r30,  GENREG_OFF(31)       /* save previous r31 */
	or    r31,  r0,   r30                  /* make r31 our pointer. */
	addu  r30,  r30,  SIZEOF_EF            /* r30 now has previous SR3 */
	st    r30,  r31,  REG_OFF(EF_SR3)      /* save previous SR3 */
	br.n  m88110_have_pcb
	xcr   r30,  r30,  SR3                  /* restore r30 */
1:
	/* we took an exception while restarting the FPU from user space.
	 * Consequently, we never picked up a stack. Do so now.
	 * R1 is currently free (saved in the exception frame pointed at by
	 * r30) */
	or.u  r1,   r0,   hi16(_kstack)
	ld    r1,   r1,   lo16(_kstack)
	addu  r1,   r1,   USIZE-SIZEOF_EF
	st    FLAGS,r1,   REG_OFF(EF_FLAGS)    /* store flags */
	st    r31,  r1,   GENREG_OFF(31)       /* store r31 - now free */
	st    r30,  r1,   REG_OFF(EF_SR3)      /* store old SR3 (pcb) */
	or    r31,  r1,   r0                   /* make r31 our exception frame pointer */
	ld    r1,   r30,  GENREG_OFF(0)        /* restore old r1 */
	st    r0,   r30,  GENREG_OFF(0)        /* repair that frame */
	st    r1,   r31,  GENREG_OFF(1)        /* store r1 in its proper place */
	br.n  m88110_have_pcb
	xcr   r30,  r30,  SR3                  /* restore r30 */
	
ASGLOBAL(m88110_pickup_stack)
	/***************** REGISTER STATUS BLOCK ***********************\
	* SR0: current thread                                          *
	* SR1: saved copy of exception-time register now holding FLAGS *
	* SR2: return address to the calling exception handler         *
	* SR3: free                                                    *
	* FLAGS: CPU status flags                                      *
	***************************************************            *
	* immediate goal:                                              *
	*    Since we're servicing an exception from user mode, we     *
	*   know that SR3 is free.  We use it to free up a temp.       *
	*   register to be used in getting the thread's pcb            *
	\***************************************************************/
	stcr  r31,  SR3      /* save previous r31 */
	
	/* switch to the thread's kernel stack. */
	or.u  r31,  r0,   hi16(_curpcb)
	ld    r31,  r31,  lo16(_curpcb)
	addu  r31,  r31,  PCB_USER_STATE       /* point to user save area */
	st    FLAGS,r31,  REG_OFF(EF_FLAGS)    /* save flags */
	st    r1,   r31,  GENREG_OFF(1)        /* save prev. r1 (now r1 free)*/
	ldcr  r1,   SR3                        /* save previous r31 */
	st    r1,   r31,  GENREG_OFF(31)
	/*FALLTHROUGH */
	
ASGLOBAL(m88110_have_pcb)
	/***************** REGISTER STATUS BLOCK ***********************\
	* SR0: current thread                                          *
	* SR1: saved copy of exception-time register now holding FLAGS *
	* SR2: return address to the calling exception handler         *
	* SR3: free                                                    *
	* r1:  free                                                    *
	* FLAGS: CPU status flags                                      *
	* r31: our exception frame                                     *
	*    Valid in the exception frame:                             *
	*   Exception-time r1, r31, FLAGS.                             *
	*   Exception SR3, if appropriate.                             *
	***************************************************            *
	* immediate goal:                                              *
	*    Save the shadow registers that need to be saved to        *
	*   the exception frame.                                       *
	\***************************************************************/
	stcr   TMP, SR3   /* free up TMP, TMP2, TMP3 */
	SAVE_TMP2
	SAVE_TMP3
	
	/* save some exception-time registers to the exception frame */
	ldcr  TMP,  EPSR
	st    TMP,  r31,  REG_OFF(EF_EPSR)
	ldcr  TMP2, EXIP
	st    TMP2, r31,  REG_OFF(EF_EXIP)
	bb0.n 0,    TMP2, 1f
	/* The instruction was NOT in the delay slot, zero ENIP. */
	st    r0,   r31,  REG_OFF(EF_ENIP)
	/* The instruction was in the delay slot, save ENIP. */
	ldcr  TMP3, ENIP
	st    TMP3, r31,  REG_OFF(EF_ENIP)
1:
	/* NO SFIP on mc88110, zero it */
	st    r0,   r31,  REG_OFF(EF_SFIP)
	
	/* get and store the cpu number */	
	extu  TMP,  FLAGS,  FLAG_CPU_FIELD_WIDTH<0>  /* TMP = cpu# */
	st    TMP,  r31,  REG_OFF(EF_CPU)
	
	/*
	 * Save Pbus fault status register from data and inst CMMU.
	 */
	ldcr  TMP,  ISR
	ldcr  TMP2, ILAR
	ldcr  TMP3, IPAR
	st    TMP,  r31,  REG_OFF(EF_ISR)
	st    TMP2, r31,  REG_OFF(EF_ILAR)
	st    TMP3, r31,  REG_OFF(EF_IPAR)
	ldcr  TMP,  ISAP
	ldcr  TMP2, IUAP
	st    TMP,  r31,  REG_OFF(EF_ISAP)
	st    TMP2, r31,  REG_OFF(EF_IUAP)
	ldcr  TMP,  DSR
	ldcr  TMP2, DLAR
	ldcr  TMP3, DPAR
	st    TMP,  r31,  REG_OFF(EF_DSR)
	st    TMP2, r31,  REG_OFF(EF_DLAR)
	st    TMP3, r31,  REG_OFF(EF_DPAR)
	ldcr  TMP,  DSAP
	ldcr  TMP2, DUAP
	st    TMP,  r31,  REG_OFF(EF_DSAP)
	st    TMP2, r31,  REG_OFF(EF_DUAP)
	
	ldcr  r1,   SR2
	jmp   r1 /* return */
	
/************************************************************************/
/************************************************************************/

ASGLOBAL(m88110_setup_phase_two)
	/***************** REGISTER STATUS BLOCK ***********************\
	* SR0: saved return address to calling exception handler        *
	* SR1: saved copy of exception-time register now holding FLAGS  *
	* SR2: free                                                     *
	* SR3: saved TMP                                                *
	* r1:  return address to calling exception handler              *
	* TMP: possibly revised SSBR                                    *
	* TMP2: free                                                    *
	* TMP3: free                                                    *
	* FLAGS: CPU status flags                                       *
	* r31: our exception frame                                      *
	*    Valid in the exception frame:                              *
	*   Exception-time r1, r31, FLAGS.                              *
	*   Exception-time TMP2, TMP3.                                  *
	*   Exception-time espr, enip, exip.                            *
	*   Exception number (EF_VECTOR).                               *
	*   Dmt0                                                        *
	*   Other data pipeline control registers, if appropriate.      *
	*   FPU control registers, if appropriate.                      *
	*   Exception SR3, if appropriate.                              *
	***************************************************             *
	* immediate goal:                                               *
	*   restore the system to the exception-time state (except      *
	* SR3 will be OUR stack pointer) so that we may resart the FPU. *
	\***************************************************************/
	/*stcr   r1, SR0*/      /* save return address */
	
	RESTORE_TMP2      /* done with extra temp regs */
	RESTORE_TMP3      /* done with extra temp regs */
	
	ldcr  TMP,  PSR
	clr   TMP,  TMP,  1<PSR_FPU_DISABLE_BIT>    /* enable the FPU */
	clr   TMP,  TMP,  1<PSR_SHADOW_FREEZE_BIT>  /* also enable shadowing */
	stcr  TMP,  EPSR
	
	or.u  TMP,  r0,   hi16(m88110_fpu_enable)
	or    TMP,  TMP,  lo16(m88110_fpu_enable)
	stcr  TMP,  EXIP      /* jump to here fpu_enable */
	addu  TMP,  TMP,  4
	stcr  TMP,  ENIP      /* and then continue after that */
	
	set   FLAGS, FLAGS, 1<FLAG_ENABLING_FPU> /* note what we're doing.*/
	xcr   FLAGS, FLAGS, SR1
	st    r1,   r31,  REG_OFF(EF_RET) /* save the return address */
	ld    r1,   r31,  GENREG_OFF(1)    /* get original r1 */
	
	ldcr  TMP, SR3
	stcr  r31, SR3	/* TMP now restored. R31 now saved in SR3 */
	ld    r31,  r31,  GENREG_OFF(31) /* get original r31 */
	
	/***************** REGISTER STATUS BLOCK ***********************\
	* SR0: current thread                                           *
	* SR1: CPU flags                                                *
	* SR2: free                                                     *
	* SR3: pointer to our exception frame (our stack pointer)       *
	* r1 through r31: original exception-time values                *
	*                                                               *
	* Valid in the exception frame:                                 *
	*   Exception-time FLAGS.                                       *
	*   Exception-time espr, sfip, enip, exip.                      *
	*   Exception number (EF_VECTOR).                               *
	*   Dmt0                                                        *
	*   Other data pipeline control registers, if appropriate.      *
	*   FPU control registers, if appropriate.                      *
	*   Exception SR3, if appropriate.                              *
	*   Held temporarly in the exception frame:                     *
	*   Return address to the calling excption handler.             *
	***************************************************             *
	* immediate goal:                                               *
	*   Do an RTE to restart the fpu and jump to "fpu_enable"       *
	*   Another exception (or exceptions) may be raised in          *
	*   this, which is why FLAG_ENABLING_FPU is set in SR1.         *
	\***************************************************************/
	NOP
	RTE   /* jumps to "fpu_enable" on the next line to enable the FPU. */
	
ASGLOBAL(m88110_fpu_enable)
	FLUSH_PIPELINE
	/* Now we can handle another exception!!! */
	/* Now that EFZE is cleared, we can clear these */
	stcr  r0, ISR	/* Clear ISR */
	stcr  r0, ILAR	/* Clear ILAR */
	stcr  r0, IPAR	/* Clear IPAR */
	stcr  r0, DSR	/* Clear DSR */
	stcr  r0, DLAR	/* Clear DLAR */
	stcr  r0, DPAR	/* Clear DPAR */
	xcr   TMP,  TMP,  SR3               /* get E.F. pointer */
	st.d  r30,  TMP,  GENREG_OFF(30)    /* save previous r30, r31 */
	or    r31,  TMP,  r0                /* transfer E.F. pointer to r31 */
	ld    TMP,  r31,  REG_OFF(EF_SR3)   /* get previous SR3; maybe important*/

	/* make sure that the FLAG_ENABLING_FPU bit is off */
	xcr   FLAGS,FLAGS,SR1
	clr   FLAGS,FLAGS,1<FLAG_ENABLING_FPU>
	xcr   FLAGS,FLAGS,SR1

	xcr   TMP,  TMP,  SR3       /* replace TMP, SR3 */

	/* now save all regs to the exception frame. */
	st.d  r0 ,  r31,  GENREG_OFF(0)
	st.d  r2 ,  r31,  GENREG_OFF(2)
	st.d  r4 ,  r31,  GENREG_OFF(4)
	st.d  r6 ,  r31,  GENREG_OFF(6)
	st.d  r8 ,  r31,  GENREG_OFF(8)
	st.d  r10,  r31,  GENREG_OFF(10)
	st.d  r12,  r31,  GENREG_OFF(12)
	st.d  r14,  r31,  GENREG_OFF(14)
	st.d  r16,  r31,  GENREG_OFF(16)
	st.d  r18,  r31,  GENREG_OFF(18)
	st.d  r20,  r31,  GENREG_OFF(20)
	st.d  r22,  r31,  GENREG_OFF(22)
	st.d  r24,  r31,  GENREG_OFF(24)
	st.d  r26,  r31,  GENREG_OFF(26)
	st.d  r28,  r31,  GENREG_OFF(28)
#ifdef JEFF_DEBUG
	/* mark beginning of frame with notable value */
	or.u  r20,  r0,   hi16(0x12345678)
	or    r20,  r20,  lo16(0x12345678)
	st    r20,  r31,  GENREG_OFF(0)
#endif
	
	/***************** REGISTER STATUS BLOCK ***********************\
	* SR0: current thread                                           *
	* SR1: free                                                     *
	* SR2: free                                                     *
	* SR3: previous exception-time SR3                              *
	* r1: return address to the calling exception handler           *
	* r2 through r30: free                                          *
	* r31: our exception frame                                      *
	*                                                               *
	* Valid in the exception frame:                                 *
	*   Exception-time r0 through r31.                              *
	*   Exception-time FLAGS.                                       *
	*   Exception-time espr, enip, exip.                            *
	*   Exception number (EF_VECTOR).                               *
	*   DSR                                                         *
	* Other data pipeline control registers, if appropriate.        *
	*   FPU control registers, if appropriate.                      *
	*   Exception SR3, if appropriate.                              *
	***************************************************             *
	* immediate goal:                                               *
	*   Pick up a stack if we came in from user mode. Put           *
	*       A copy of the exception frame pointer into r30          *
	*       bump the stack a doubleword and write the exception     *
	*       frame pointer.                                          *
	*   if not an interrupt exception,                              *
	*       Turn on interrupts and service any outstanding          *
	*       data access exceptions.                                 *
	*   Return to calling exception handler to                      *
	*   service the exception.                                      *
	\***************************************************************/
	
	/*
	 * If it's not the interrupt exception, enable interrupts and
	 * take care of any data access exceptions......
	 */
	or    r30,  r0,   r31              /* get a copy of the e.f. pointer */
	ld    r2,   r31,  REG_OFF(EF_EPSR)
	bb1   PSR_SUPERVISOR_MODE_BIT, r2, 1f  /* If in kernel mode */

#if INTSTACK
	/*
	 * If interrupt exception, switch to interrupt stack if not
	 * already there. Else, switch to kernel stack.
	 */
	ld    r3,   r31,  REG_OFF(EF_VECTOR)
	cmp   r3,   r3,   1      /* is interrupt ? */
	bb0   eq,   r3,   2f
	or.u  r31,  r0,   hi16(_intstack_end)   /* swith to int stack */
	or    r31,  r31,  lo16(_intstack_end)
	br    3f
2:
#endif
	or.u  r31,  r0,   hi16(_kstack)
	ld    r31,  r31,  lo16(_kstack)
	addu  r31,  r31,  USIZE        /* point at proper end */
	br    3f
1:
#if INTSTACK
	ld    r3,   r31,  REG_OFF(EF_VECTOR)
	cmp   r3,   r3,   1       /* is interrupt ? */
	bb0   eq,   r3,   3f      /* no, we will stay on kern stack */
	or.u  r31,  r0,   hi16(_intstack_end)   /* swith to int stack */
	or    r31,  r31,  lo16(_intstack_end)
#endif	/* INTSTACK */
	/* This label is here for debugging */
m88110_exception_handler_has_ksp:  global m88110_exception_handler_has_ksp
3:	/*
	 * here - r30 holds a pointer to the exception frame.
	 * r31 is a pointer to the kernel stack/interrupt stack.
	 */
	subu  r31,  r31,  8  /* make some breathing space */
	st    r30,  r31,  0  /* store frame pointer on the stack */
#if DDB
	st    r30,  r31,  4  /* store it again for the debugger to recognize */
#endif /* DDB */

	ld    r2,   r30,  REG_OFF(EF_VECTOR)
	bcnd.n eq0, r2,   m88110_return_to_calling_exception_handler  /* is error */
	ld    r14,  r30,  REG_OFF(EF_RET) /* load return value XXX!!! */
	cmp   r3,   r2,   1 /* interrupt is exception #1 ;Is an interrupt? */
	bb1.n   eq,   r3,   m88110_return_to_calling_exception_handler /* skip if so */

#if DDB
	cmp   r3,   r2,   130 /* DDB break exception */
	bb1.n eq,   r3,   m88110_return_to_calling_exception_handler

	cmp   r3,   r2,   132 /* DDB entry exception */
	bb1.n eq,   r3,   m88110_return_to_calling_exception_handler
#endif

	ldcr  r2,   PSR
	clr   r2,   r2,   1<PSR_INTERRUPT_DISABLE_BIT>   /* enable interrupts */
	stcr  r2,   PSR
#if     DDB
	FLUSH_PIPELINE
#endif
#if 1 /* test */
	br  m88110_return_to_calling_exception_handler
#endif
	/* service any outstanding data pipeline stuff
	   - check dsr... anything outstanding?*/

	ld    r3,   r30,  REG_OFF(EF_DSR)
	cmp   r3,   r3,   0
	bb1   eq,   r3,   m88110_return_to_calling_exception_handler

/*
 * r30 can be clobbered by calls. So stuff its value into a
 * preserved register, say r15. R14 is in use (see return_to_... below).
 */
	or    r15,  r0,   r30
#if 0
	CALL(_C_LABEL(test_trap), r15, r0)
#endif   
	CALL(_C_LABEL(m88110_trap), T_DATAFLT, r15)

	/*   restore it... */
	or    r30,  r0,   r15

	/* clear the dsr word in the E.F */
	st    r0,   r30,  REG_OFF(EF_DSR)

ASGLOBAL(m88110_return_to_calling_exception_handler)
	jmp   r14 /* loaded above */
	
	data
	.align 8
GLOBAL(save_frame)
	space SIZEOF_EF	
#endif
