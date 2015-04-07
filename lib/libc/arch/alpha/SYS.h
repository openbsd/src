/*	$OpenBSD: SYS.h,v 1.10 2015/04/07 01:27:06 guenther Exp $	*/
/*	$NetBSD: SYS.h,v 1.4 1996/10/17 03:03:53 cgd Exp $	*/

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


#define	CALLSYS_ERROR(name)					\
	CALLSYS_NOERROR(name);					\
	br	gp, LLABEL(name,0);				\
LLABEL(name,0):							\
	LDGP(gp);						\
	beq	a3, LLABEL(name,1);				\
	jmp	zero, __cerror;					\
LLABEL(name,1):

#define __LEAF(p,n,e)						\
	LEAF(___CONCAT(p,n),e)
#define __END(p,n)						\
	END(___CONCAT(p,n))

#define	__SYSCALL(p,name)					\
__LEAF(p,name,0);			/* XXX # of args? */	\
	CALLSYS_ERROR(name)

#define	__SYSCALL_NOERROR(p,name)				\
__LEAF(p,name,0);			/* XXX # of args? */	\
	CALLSYS_NOERROR(name)


#define __RSYSCALL(p,name)					\
	__SYSCALL(p,name);					\
	RET;							\
__END(p,name)

#define __RSYSCALL_NOERROR(p,name)				\
	__SYSCALL_NOERROR(p,name);				\
	RET;							\
__END(p,name)


#define	__PSEUDO(p,label,name)					\
__LEAF(p,label,0);			/* XXX # of args? */	\
	CALLSYS_ERROR(name);					\
	RET;							\
__END(p,label);

#define	__PSEUDO_NOERROR(p,label,name)				\
__LEAF(p,label,0);			/* XXX # of args? */	\
	CALLSYS_NOERROR(name);					\
	RET;							\
__END(p,label);

#define ALIAS(prefix,name) WEAK_ALIAS(name, ___CONCAT(prefix,name));

/*
 * For the thread_safe versions, we prepend _thread_sys_ to the function
 * name so that the 'C' wrapper can go around the real name.
 */
# define SYSCALL(x)		ALIAS(_thread_sys_,x) \
				__SYSCALL(_thread_sys_,x)
# define SYSCALL_NOERROR(x)	ALIAS(_thread_sys_,x) \
				__SYSCALL_NOERROR(_thread_sys_,x)
# define RSYSCALL(x)		ALIAS(_thread_sys_,x) \
				__RSYSCALL(_thread_sys_,x)
# define RSYSCALL_HIDDEN(x)	__RSYSCALL(_thread_sys_,x)
# define RSYSCALL_NOERROR(x)	ALIAS(_thread_sys_,x) \
				__RSYSCALL_NOERROR(_thread_sys_,x)
# define PSEUDO(x,y)		ALIAS(_thread_sys_,x) \
				__PSEUDO(_thread_sys_,x,y)
# define PSEUDO_NOERROR(x,y)	ALIAS(_thread_sys_,x) \
				__PSEUDO_NOERROR(_thread_sys_,x,y)
# define SYSLEAF(x,e)		ALIAS(_thread_sys_,x) \
				__LEAF(_thread_sys_,x,e)
# define SYSEND(x)		__END(_thread_sys_,x)
