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
/*
 * HISTORY
 * $Log: asm.h,v $
 * Revision 1.1.1.1  1995/10/18 10:54:22  deraadt
 * initial 88k import; code by nivas and based on mach luna88k
 *
 * Revision 2.3  93/01/26  18:05:05  danner
 * 	Added #ifndef file wrapper.
 * 	[93/01/24            jfriedl]
 * 
 * Revision 2.2  92/08/03  17:46:50  jfriedl
 * 	Brought to m88k directory.
 * 	[92/07/24            jfriedl]
 * 
 * Revision 2.1.1.1  92/05/27  15:24:16  danner
 * 	Move FLUSH_PIPELINE, REG_OFF definitions here.
 * 	[92/05/17            danner]
 * 
 * Revision 2.3  92/02/18  18:00:24  elf
 * 	Typo correction (from Torbjorn Granlund <tege@sics.se>).
 * 	[92/02/06            danner]
 * 
 * 	moved RTE definition here
 * 	[92/02/02            danner]
 * 
 * Revision 2.2  91/07/09  23:16:20  danner
 * 	Initial 3.0 Checkin
 * 	[91/06/26  11:57:57  danner]
 * 
 * Revision 2.2  91/04/05  13:55:26  mbj
 * 	Initial code from the Omron 1.10 kernel release corresponding to X130.
 * 	The Copyright has been adjusted to correspond to the understanding
 * 	between CMU and the Omron Corporation.
 * 	[91/04/04            rvb]
 * 
 * 	Corrected ENTRY Macro to use NEWLINE instead of \\ Hack
 * 	[91/03/07            danner]
 * 
 */

/*
 * 	File:	m88k/asm.h
 *
 *	This header file is intended to hold definitions useful for M88K
 *	assembly routines.
 *
 */
#ifndef __M88K_ASM_H__
#define __M88K_ASM_H__

#ifndef prepend_underbar
#    ifdef __STDC__
#        define prepend_underbar(NAME) _##NAME
#    else
#        define prepend_underbar(NAME) _/**/NAME
#    endif
#endif

#define	ENTRY(NAME) \
    align 4 NEWLINE prepend_underbar(NAME): NEWLINE global prepend_underbar(NAME)

#define RTE	NOP NEWLINE rte

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
 * At various times, there is the need to clear the pipeline (i.e.
 * synchronize).  A "tcnd ne0, r0, foo" will do that (because a trap
 * instruction always synchronizes, and this particular instruction
 * will never actually take the trap).
 */
#define FLUSH_PIPELINE	tcnd ne0, r0, 0
#define NOP		or r0, r0, r0

/* REGister OFFset into the E.F. (exception frame) */
#define REG_OFF(reg_num)  ((reg_num) * 4) /* (num * sizeof(register int))  */
#define GENREG_OFF(num)	(REG_OFF(EF_R0 + (num))) /* GENeral REGister OFFset */

#if !defined(LABEL)
#define  LABEL(name)	name: global name NEWLINE
#define _LABEL(name)	name:               NEWLINE
#endif /* LABEL */

#endif /* __M88K_ASM_H__ */
