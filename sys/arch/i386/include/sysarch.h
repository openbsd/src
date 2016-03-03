/*	$OpenBSD: sysarch.h,v 1.13 2016/03/03 12:41:30 naddy Exp $	*/
/*	$NetBSD: sysarch.h,v 1.8 1996/01/08 13:51:44 mycroft Exp $	*/

#ifndef _MACHINE_SYSARCH_H_
#define _MACHINE_SYSARCH_H_

/*
 * Architecture specific syscalls (i386)
 */
#define	I386_IOPL	2
#define	I386_GET_IOPERM	3
#define	I386_SET_IOPERM	4
#define	I386_VM86	5
#define	I386_GET_FSBASE	6
#define	I386_SET_FSBASE	7
#define	I386_GET_GSBASE	8
#define	I386_SET_GSBASE	9

struct i386_iopl_args {
	int iopl;
};

struct i386_get_ioperm_args {
	u_long *iomap;
};

struct i386_set_ioperm_args {
	u_long *iomap;
};

#ifdef _KERNEL
uint32_t i386_get_threadbase(struct proc *, int);
int i386_set_threadbase(struct proc *, uint32_t, int);
#else

#include <sys/cdefs.h>

__BEGIN_DECLS
int i386_iopl(int);
int i386_get_ioperm(u_long *);
int i386_set_ioperm(u_long *);
int i386_get_fsbase(void **);
int i386_set_fsbase(void *);
int i386_get_gsbase(void **);
int i386_set_gsbase(void *);
int sysarch(int, void *);
__END_DECLS
#endif

#endif /* !_MACHINE_SYSARCH_H_ */
