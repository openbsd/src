/* $NetBSD: SYS.h,v 1.1 2006/09/10 21:22:33 cherry Exp $ */

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

#include <sys/cdefs.h>

#include <machine/asm.h>
#include <sys/syscall.h>

#define RET { br.ret.sptk.few rp;; }

#define CALLSYS_ERROR(name)					\
	CALLSYS_NOERROR(name)					\
{	cmp.ne  p6,p0=r0,r10;					\
(p6)    br.cond.sptk.few __cerror ;; }

#define	SYSCALL(name)						\
ENTRY(name,0);				/* XXX # of args? */	\
	CALLSYS_ERROR(name)

#define	SYSCALL_NOERROR(name)					\
ENTRY(name,0);				/* XXX # of args? */	\
	CALLSYS_NOERROR(name)

#define	PSEUDO(label,name)					\
ENTRY(label,0);				/* XXX # of args? */	\
	CALLSYS_ERROR(name);					\
	RET;							\
END(label);

#define	PSEUDO_NOERROR(label,name)				\
ENTRY(label,0);				/* XXX # of args? */	\
	CALLSYS_NOERROR(name);					\
	RET;							\
END(label);

#define RSYSCALL(name)						\
	SYSCALL(name);						\
	RET;							\
END(name)

#define RSYSCALL_NOERROR(name)					\
	SYSCALL_NOERROR(name);					\
	RET;							\
END(name)

#define	WSYSCALL(weak,strong)					\
	WEAK_ALIAS(weak,strong);				\
	PSEUDO(strong,weak)
