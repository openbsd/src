/*	$OpenBSD: break.h,v 1.1 1998/07/07 21:32:38 mickey Exp $	*/

/* 
 * Copyright (c) 1990, 1991, 1994 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: break.h 1.10 94/12/14$
 *	Author: Bob Wheeler, University of Utah CSL
 */

/*
 *	Values for break instructions
 */

/*
 * values for the im5 field of the break instruction
 */
#define BREAK_KERNEL	0
#define BREAK_MAYDEBUG	31	/* Reserved for Mayfly debugger. */

/*
 * values for the im13 field of the break instruction
 *
 * BREAK_PDC_CALL calls the PDC routine. Users should use the routine 
 * pdc_call() which sets up the registers for this call. 
 */
#define BREAK_PDC_CALL		1
#define BREAK_PDC_DUMP  	2
#define BREAK_KERNTRACE		3
#define BREAK_MACH_DEBUGGER	4
#define BREAK_KGDB		5
#define BREAK_KERNPRINT		6
#define BREAK_IVA		7
#define BREAK_PDC_IODC_CALL	8
#define BREAK_GDB		8	/* Standard GDB breakpoint.  */
#define	BREAK_THREAD_SETSTATE	9	/* XXX */

/*
 * Tear apart a break instruction to find its type.
 */
#define break5(x)	((x) & 0x1F)
#define break13(x)	(((x) >> 13) & 0x1FFF)

/*
 * Trace debugging.
 */
#define TRACE_OFF	0
#define TRACE_JUMP      -1
#define TRACE_SUSPEND   -2
#define TRACE_RESUME    -3
