/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
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
/*     "locore.h"		  Omron Corporation
 * This file created by Omron Corporation, 1990.
 * NOTE: Any assembly file that includes this one must define ASSEMBLER first
 */

#ifndef __MACHINE_LOCORE_H__
#define __MACHINE_LOCORE_H__


/*
 **********************************************************************
	SYNTACTICAL AND SEMANTIC DOO-DADS
 **********************************************************************
 */
/*
 * NEWLINE is defined in 'assmy.s' in the object area to be a double
 * backslash, which 'as' interprets the same as '\n'
 */

/*
 * If this has been included in an assembly file, make sure
 * LOCORE is defined.  Always make sure KERNEL is defined.
 */
#if defined(ASSEMBLER) && !defined(LOCORE)
#  define LOCORE
#endif
#if !defined(KERNEL)
#  define KERNEL
#endif

/* Define EH_DEBUG to be non-zero to compile-in various debugging things */
#ifndef	EH_DEBUG
#define EH_DEBUG 0
#endif	EH_DEBUG

/* this gives the offsets into various structures of various elements, etc */
#include "assym.s"

/*
 * LABEL(name)
 * 	Defines the name to be a label visible to the world.
 *
 * _LABEL(name)
 *	Defines one visible only to the file, unless debugging
 * 	is enabled, in which case it's visible to the world (and
 *	hence to debuggers, and such).
 */
#define LABEL(name)	name:	global name NEWLINE

#if EH_DEBUG
#  define _LABEL(name)	name:	global name NEWLINE
#else
#  define _LABEL(name)	name:               NEWLINE
#endif


/*
 * Useful in some situations.
 * NOTE: If ARG1 or ARG2 are r2 or r3, strange things may happen.  Watch out!
 */
#define CALL(NAME, ARG1, ARG2)             \
  		subu	r31, r31, 32	NEWLINE \
                or      r2, r0, ARG1    NEWLINE \
                bsr.n   NAME            NEWLINE \
                or      r3, r0, ARG2	NEWLINE \
		addu	r31, r31, 32

/*
 **********************************************************************
	SYMBOLIC CONSTANTS AND VALUES and other important things
 **********************************************************************
 */

/*
 * SR1 - CPU FLAGS REGISTER
 * 
 * SR1 contains flags about the current CPU status.
 *
 * The lowest FLAG_CPU_FIELD_WIDTH bits hold the cpu number (currently 0-3).
 *
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
#define FLAG_CPU_FIELD_WIDTH		4	/* must be <= 12 */

#define FLAG_IGNORE_DATA_EXCEPTION	5	/* bit number 5 */
#define FLAG_INTERRUPT_EXCEPTION	6	/* bit number 6 */
#define FLAG_ENABLING_FPU		7	/* bit number 7 */


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
#define FLAGS	r2
#define TMP	r3
#define TMP2	r10
#define TMP3	r11
#define SAVE_TMP2	st	r10, r31, GENREG_OFF(10)
#define SAVE_TMP3	st	r11, r31, GENREG_OFF(11)
#define RESTORE_TMP2	ld	r10, r31, GENREG_OFF(10)
#define RESTORE_TMP3	ld	r11, r31, GENREG_OFF(11)


/* alternate CPU control register names */
#define PID	cr0
#define PSR	cr1
#define EPSR	cr2
#define SSBR	cr3
#define SXIP	cr4
#define SNIP	cr5
#define SFIP	cr6
#define VBR	cr7
#define DMT0	cr8
#define DMD0	cr9
#define DMA0	cr10
#define DMT1	cr11
#define DMD1	cr12
#define DMA1	cr13
#define DMT2	cr14
#define DMD2	cr15
#define DMA2	cr16
#define SR0	cr17
#define SR1	cr18
#define SR2	cr19
#define SR3	cr20
#define FPECR	fcr0
#define FPHS1	fcr1
#define FPLS1	fcr2
#define FPHS2	fcr3
#define FPLS2	fcr4
#define FPPT	fcr5
#define FPRH	fcr6
#define FPRL	fcr7
#define FPIT	fcr8
#define FPSR	fcr62
#define FPCR	fcr63

/*
 * Info about the PSR 
 */
#define PSR_SHADOW_FREEZE_BIT		 0
#define PSR_INTERRUPT_DISABLE_BIT	 1
#define PSR_FPU_DISABLE_BIT		 3
#define PSR_BIG_ENDIAN_MODE		30
#define PSR_SUPERVISOR_MODE_BIT		31

/*
 * Status bits for an SXIP/SNIP/SFIP address.
 */
#define RTE_VALID_BIT	 1
#define RTE_ERROR_BIT	 0

/*
 * Info about DMT0/DMT1/DMT2
 */
#define DMT_VALID_BIT	 0
#define DMT_WRITE_BIT 	 1
#define DMT_LOCK_BIT	12
#define DMT_DOUBLE_BIT	13
#define DMT_DAS_BIT     14
#define DMT_DREG_OFFSET	 7
#define DMT_DREG_WIDTH	 5

/*
 * Bits for eh_debug.
 */
#define DEBUG_INTERRUPT_BIT		 0
#define DEBUG_DATA_BIT			 1
#define DEBUG_INSTRUCTION_BIT		 2
#define DEBUG_MISALIGN_BIT		 3
#define DEBUG_UNIMP_BIT			 4
#define DEBUG_DIVIDE_BIT		 5
#define DEBUG_OF_BIT			 6
#define DEBUG_FPp_BIT			 7
#define DEBUG_FPi_BIT			 8
#define DEBUG_SYSCALL_BIT	 	 9
#define DEBUG_MACHSYSCALL_BIT		10
#define DEBUG_UNIMPLEMENTED_BIT		11
#define DEBUG_PRIVILEGE_BIT		12
#define DEBUG_BOUNDS_BIT		13
#define DEBUG_OVERFLOW_BIT		14
#define DEBUG_ERROR_BIT			15
#define DEBUG_SIGSYS_BIT		16
#define DEBUG_SIGTRAP_BIT		17
#define DEBUG_BREAK_BIT			18
#define DEBUG_TRACE_BIT			19
#define DEBUG_KDB_BIT			20
#define DEBUG_JKDB_BIT			21
#define DEBUG_BUGCALL_BIT		22

#define DEBUG_UNKNOWN_BIT		31

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

#define YES		1
#define NO		0

#define	SCSI_INTS	0x10
#define	SCSI_SSTS	0x18
#define SCSI_DREG	0x28

/* change software timer for 8mm device support -- 90/08/21 CEC OKUI */
#define	SCSI_WAIT	0x5000000

/*
 * At various times, there is the need to clear the pipeline (i.e.
 * synchronize).  A "tcnd ne0, r0, foo" will do that (because a trap
 * instruction always synchronizes, and this particular instruction
 * will never actually take the trap).
 */
#define FLUSH_PIPELINE	tcnd	ne0, r0, 0

/*
 * NOP -- NO-Operation.
 *
 * A do-nothing one clock doesn't-touch-the-scoreboard type of instruction,
 * in case one's needed (sometimes useful for debugging).
 */
#define NOP		or r0, r0, r0

/*
 * These things for vector_init.c and locore.c
 */
#if defined(ASSEMBLER)
# define  PREDEFINED_BY_ROM       0xffffffff
# define  END_OF_VECTOR_LIST      0xfffffffe
#else
# define  PREDEFINED_BY_ROM       0xffffffffU
# define  END_OF_VECTOR_LIST      0xfffffffeU
#endif

/*
 * Define ERROR__XXX_USR if the xxx.usr bug (mask C82N) is present.
 * This implements the workaround.
 */
#define ERRATA__XXX_USR 	1

#define USERMODE(x) (!(x & (1 << PSR_SUPERVISOR_MODE_BIT)))

#endif /* __MACHINE_LOCORE_H__ */
