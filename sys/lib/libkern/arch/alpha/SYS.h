/*	$NetBSD: SYS.h,v 1.1 1995/02/13 21:49:13 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
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

#include <machine/asm.h>
#include <sys/syscall.h>

#define CALLSYS(num)						\
	CONST(num, v0);						\
	call_pal 0x83;			/* op_callsys */	
	
#define	SYSCALL_NOLABEL(x)					\
	CALLSYS(SYS_/**/x);					\
	beq	a3, 9f;						\
	br	gp, 8f;						\
8:	SETGP(gp);						\
	lda	at_reg, cerror;					\
	jmp	zero, (at_reg);					\
9:

#define	SYSCALL(x)	LEAF(x, 0 /* XXX */); SYSCALL_NOLABEL(x);
#define RSYSCALL(x)	SYSCALL(x); RET; END(x);

#define	PSEUDO(x,y)						\
LEAF(x,0);				/* unknown # of args */	\
	CALLSYS(SYS_/**/y);					\
	RET;							\
END(x);
