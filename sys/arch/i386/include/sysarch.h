/*	$OpenBSD: sysarch.h,v 1.4 2002/03/14 01:26:33 millert Exp $	*/
/*	$NetBSD: sysarch.h,v 1.8 1996/01/08 13:51:44 mycroft Exp $	*/

#ifndef _I386_SYSARCH_H_
#define _I386_SYSARCH_H_

/*
 * Architecture specific syscalls (i386)
 */
#define I386_GET_LDT	0
#define I386_SET_LDT	1
#define	I386_IOPL	2
#define	I386_GET_IOPERM	3
#define	I386_SET_IOPERM	4
#define	I386_VM86	5

struct i386_get_ldt_args {
	int start;
	union descriptor *desc;
	int num;
};

struct i386_set_ldt_args {
	int start;
	union descriptor *desc;
	int num;
};

struct i386_iopl_args {
	int iopl;
};

struct i386_get_ioperm_args {
	u_long *iomap;
};

struct i386_set_ioperm_args {
	u_long *iomap;
};

#ifndef _KERNEL
int i386_get_ldt(int, union descriptor *, int);
int i386_set_ldt(int, union descriptor *, int);
int i386_iopl(int);
int i386_get_ioperm(u_long *);
int i386_set_ioperm(u_long *);
int sysarch(int, char *);
#endif

#endif /* !_I386_SYSARCH_H_ */
