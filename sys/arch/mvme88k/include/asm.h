/*	$OpenBSD: asm.h,v 1.21 2003/01/02 21:40:46 miod Exp $	*/

/*
 * Mach Operating System
 * Copyright (c) 1993-1992 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
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

#ifndef __MACHINE_M88K_ASM_H__
#define __MACHINE_M88K_ASM_H__

#ifdef __STDC__
#define	_C_LABEL(name)		_ ## name
#else
#define	_C_LABEL(name)		_/**/name
#endif

#define	_ASM_LABEL(name)	name

#define	_ENTRY(name) \
	.text; .align 8; .globl name; name:

#define	ENTRY(name)		_ENTRY(_C_LABEL(name))
#define	ASENTRY(name)		_ENTRY(_ASM_LABEL(name))

#define	GLOBAL(name) \
	.globl _C_LABEL(name); _C_LABEL(name):

#define ASGLOBAL(name) \
	.globl _ASM_LABEL(name); _ASM_LABEL(name):

#define	LOCAL(name) \
	_C_LABEL(name):

#define	ASLOCAL(name) \
	_ASM_LABEL(name):

#define	BSS(name, size) \
	.comm	_C_LABEL(name), size

#define	ASBSS(name, size) \
	.comm	_ASM_LABEL(name), size

#ifdef	__ELF__
#define	WEAK_ALIAS(alias,sym)						\
	.weak alias;							\
	alias = sym
#else
#ifdef	__STDC__
#define	WEAK_ALIAS(alias,sym)						\
	.weak _##alias;							\
	_##alias = _##sym
#else
#define	WEAK_ALIAS(alias,sym)						\
	.weak _/**/alias;						\
	_/**/alias = _/**/sym
#endif
#endif

#ifdef _KERNEL

/*
 * Control register symbolic names
 */

#define	PID	cr0
#define	PSR	cr1
#define	EPSR	cr2
#define	SSBR	cr3
#define	SXIP	cr4
#define	SNIP	cr5
#define	SFIP	cr6
#define	VBR	cr7
#define	DMT0	cr8
#define	DMD0	cr9
#define	DMA0	cr10
#define	DMT1	cr11
#define	DMD1	cr12
#define	DMA1	cr13
#define	DMT2	cr14
#define	DMD2	cr15
#define	DMA2	cr16
#define	SRX	cr16 
#define	SR0	cr17
#define	SR1	cr18
#define	SR2	cr19
#define	SR3	cr20

/* MVME197 only */
#define	SRX	cr16 
#define	EXIP	cr4
#define	ENIP	cr5
#define	ICMD	cr25
#define	ICTL	cr26
#define	ISAR	cr27
#define	ISAP	cr28
#define	IUAP	cr29
#define	IIR	cr30
#define	IBP	cr31
#define	IPPU	cr32
#define	IPPL	cr33
#define	ISR	cr34
#define	ILAR	cr35
#define	IPAR	cr36
#define	DCMD	cr40
#define	DCTL	cr41
#define	DSAR	cr42
#define	DSAP	cr43
#define	DUAP	cr44
#define	DIR	cr45
#define	DBP	cr46
#define	DPPU	cr47
#define	DPPL	cr48
#define	DSR	cr49
#define	DLAR	cr50
#define	DPAR	cr51
/* end MVME197 only */

#define	FPECR	fcr0
#define	FPHS1	fcr1
#define	FPLS1	fcr2
#define	FPHS2	fcr3
#define	FPLS2	fcr4
#define	FPPT	fcr5
#define	FPRH	fcr6
#define	FPRL	fcr7
#define	FPIT	fcr8
#define	FPSR	fcr62
#define	FPCR	fcr63

/*
 * At various times, there is the need to clear the pipeline (i.e.
 * synchronize).  A "tb1 0, r0, foo" will do that (because a trap
 * instruction always synchronizes, and this particular instruction
 * will never actually take the trap).
 */
#if 0
#define	FLUSH_PIPELINE		tcnd	ne0, r0, 0
#define	FLUSH_PIPELINE_STRING	"tcnd	ne0, r0, 0"
#else
#define	FLUSH_PIPELINE		tb1	0, r0, 0
#define	FLUSH_PIPELINE_STRING	"tb1	0, r0, 0"
#endif
#define	NOP			or	r0, r0, r0
#define	NOP_STRING		"or	r0, r0, r0"

#define RTE	NOP ; rte

/*
 * Useful in some situations.
 * NOTE: If ARG1 or ARG2 are r2 or r3, strange things may happen.  Watch out!
 */
#define CALL(NAME, ARG1, ARG2) \
	subu	r31, r31, 32; \
	or	r2, r0, ARG1; \
	bsr.n	NAME; \
	or	r3, r0, ARG2; \
	addu	r31, r31, 32

/* This define is similar to CALL, but accepts a function pointer XXX smurph */
#define CALLP(NAME, ARG1, ARG2) \
	subu	r31, r31, 32; \
	or.u	r5, r0, hi16(NAME); \
	ld	r4, r5, lo16(NAME); \
	or	r2, r0, ARG1; \
	jsr.n	r4; \
	or	r3, r0, ARG2; \
	addu	r31, r31, 32

/*
 * SR1 - CPU FLAGS REGISTER
 * XXX clean this when the trap handler is reworked. Among the things
 * I like to see is having the trap frame on the kernel stack instead
 * of putting in the PCB. If done properly, we don't need SR1 for doing
 * anything special. nivas
 * 
 * SR1 contains flags about the current CPU status.
 *
 * The bit FLAG_IGNORE_DATA_EXCEPTION indicates that any data exceptions
 * 	should be ignored (well, at least treated in a special way).
 * The bit FLAG_INTERRUPT_EXCEPTION indicates that the current exception
 * 	is the interrupt exception.  Such information can be gotten
 * 	in other ways, but having it in the flags makes it easy for the
 *	exception handler to check quickly.
 * The bit FLAG_ENABLING_FPU indicates that the exception handler is
 * 	in the process of enabling the FPU (so that an exception can
 * 	be serviced).  This is needed because enabling the FPU can
 *	cause other exceptions to happen, and the whole system is
 *	in a rather precarious state and so special cautions must
 * 	be taken.
 */
#define FLAG_CPU_FIELD_WIDTH		2	/* must be <= 12 */
#define FLAG_IGNORE_DATA_EXCEPTION	5	/* bit number 5  */
#define FLAG_INTERRUPT_EXCEPTION	6	/* bit number 6  */
#define FLAG_ENABLING_FPU		7	/* bit number 7  */
#define FLAG_FROM_KERNEL		8	/* bit number 8  */
#define FLAG_187			9	/* bit number 9  */
#define FLAG_188			10	/* bit number 10 */
#define FLAG_197			11	/* bit number 11 */

/* REGister OFFset into the E.F. (exception frame) */
#define REG_OFF(reg_num)  ((reg_num) * 4) /* (num * sizeof(register int))  */
#define GENREG_OFF(num)	(REG_OFF(EF_R0 + (num))) /* GENeral REGister OFFset */

#define GENERAL_BREATHING_ROOM	/* arbitrarily */ 200
#define KERNEL_STACK_BREATHING_ROOM 	\
	(GENERAL_BREATHING_ROOM + SIZEOF_STRUCT_PCB + SIZEOF_STRUCT_UTHREAD)

/*
 * Some registers used during the setting up of the new exception frame.
 * Don't choose r1, r30, or r31 for any of them.
 *
 * Also, if any are 'r2' or 'r3', be careful using with CALL above!
 */
#define	FLAGS	r2
#define	TMP	r3
#define	TMP2	r10
#define	TMP3	r11
#define	SAVE_TMP2	st	r10, r31, GENREG_OFF(10)
#define	SAVE_TMP3	st	r11, r31, GENREG_OFF(11)
#define	RESTORE_TMP2	ld	r10, r31, GENREG_OFF(10)
#define	RESTORE_TMP3	ld	r11, r31, GENREG_OFF(11)

/*
 * Info about the PSR 
 */
#define	PSR_SHADOW_FREEZE_BIT		0
#define	PSR_INTERRUPT_DISABLE_BIT	1
#define	PSR_FPU_DISABLE_BIT		3
#define	PSR_BIG_ENDIAN_MODE		30
#define	PSR_SUPERVISOR_MODE_BIT		31
/* 
 * mc88110 PSR bit definitions (MVME197) 
 */
#define PSR_GRAPHICS_DISABLE_BIT	4
#define PSR_SERIAL_MODE_BIT		29
#define PSR_CARRY_BIT			28
#define PSR_SERIALIZE_BIT		25

/*
 * Status bits for an SXIP/SNIP/SFIP address.
 */
#define	RTE_VALID_BIT		1
#define	RTE_ERROR_BIT		0

/*
 * Info about DMT0/DMT1/DMT2
 */
#define	DMT_VALID_BIT		0
#define	DMT_WRITE_BIT		1
#define	DMT_LOCK_BIT		12
#define	DMT_DOUBLE_BIT		13
#define	DMT_DAS_BIT		14
#define	DMT_DREG_OFFSET		7
#define	DMT_DREG_WIDTH		5

/*
 * Bits for eh_debug.
 */
#define	DEBUG_INTERRUPT_BIT		0
#define	DEBUG_DATA_BIT			1
#define	DEBUG_INSTRUCTION_BIT		2
#define	DEBUG_MISALIGN_BIT		3
#define	DEBUG_UNIMP_BIT			4
#define	DEBUG_DIVIDE_BIT		5
#define	DEBUG_OF_BIT			6
#define	DEBUG_FPp_BIT			7
#define	DEBUG_FPi_BIT			8
#define	DEBUG_SYSCALL_BIT	 	9
#define	DEBUG_MACHSYSCALL_BIT		10
#define	DEBUG_UNIMPLEMENTED_BIT		11
#define	DEBUG_PRIVILEGE_BIT		12
#define	DEBUG_BOUNDS_BIT		13
#define	DEBUG_OVERFLOW_BIT		14
#define	DEBUG_ERROR_BIT			15
#define	DEBUG_SIGSYS_BIT		16
#define	DEBUG_SIGTRAP_BIT		17
#define	DEBUG_BREAK_BIT			18
#define	DEBUG_TRACE_BIT			19
#define	DEBUG_KDB_BIT			20
#define	DEBUG_JKDB_BIT			21
#define	DEBUG_BUGCALL_BIT		22
/* MVME197 Non-Maskable Interrupt */
#define	DEBUG_NON_MASK_BIT		23
/* MVME197 Data Read Miss (Software Table Searches) */
#define	DEBUG_197_READ_BIT		25
/* MVME197 Data Write Miss (Software Table Searches) */
#define	DEBUG_197_WRITE_BIT		26
/* MVME197 Inst ATC Miss (Software Table Searches) */
#define	DEBUG_197_INST_BIT		27

#define	DEBUG_UNKNOWN_BIT		31

/*
 * These things for locore_c_routines.c and locore.S
 */
#define	PREDEFINED_BY_ROM	0xffffffff
#define	END_OF_VECTOR_LIST	0xfffffffe

#endif	/* _KERNEL */

#endif /* __MACHINE_M88K_ASM_H__ */
