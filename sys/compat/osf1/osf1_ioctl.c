/*	$NetBSD: osf1_ioctl.c,v 1.3 1995/10/07 06:27:19 mycroft Exp $	*/

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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/termios.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <compat/osf1/osf1_syscallargs.h>

#ifdef SYSCALL_DEBUG
extern int scdebug;
#endif

#define OSF1_IOCPARM_MASK	0x1fff	/* parameter length, at most 13 bits */
#define	OSF1_IOCPARM_LEN(x)	(((x) >> 16) & OSF1_IOCPARM_MASK)
#define	OSF1_IOCGROUP(x)	(((x) >> 8) & 0xff)

#define	OSF1_IOCPARM_MAX	NBPG		/* max size of ioctl */
#define	OSF1_IOC_VOID		0x20000000	/* no parameters */
#define	OSF1_IOC_OUT		0x40000000	/* copy out parameters */
#define	OSF1_IOC_IN		0x80000000	/* copy in parameters */
#define	OSF1_IOC_INOUT		(OSF1_IOC_IN|OSF1_IOC_OUT)
#define	OSF1_IOC_DIRMASK	0xe0000000	/* mask for IN/OUT/VOID */

#define OSF1_IOCCMD(x)		((x) & 0xff)

int osf1_ioctl_i	__P((struct proc *p, struct sys_ioctl_args *nuap,
			    register_t *retval, int cmd, int dir, int len));
int osf1_ioctl_t	__P((struct proc *p, struct sys_ioctl_args *nuap,
			    register_t *retval, int cmd, int dir, int len));

int
osf1_sys_ioctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(int) com;
		syscallarg(caddr_t) data;
	} */ *uap = v;
	struct sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(u_long) com;
		syscallarg(caddr_t) data;
	} */ a;
	int op, dir, group, cmd, len;
#ifdef SYSCALL_DEBUG
	char *dirstr;
#endif

	op = SCARG(uap, com);
	dir = op & OSF1_IOC_DIRMASK;
	group = OSF1_IOCGROUP(op);
	cmd = OSF1_IOCCMD(op);
	len = OSF1_IOCPARM_LEN(op);

	switch (dir) {
	case OSF1_IOC_VOID:
		dir = IOC_VOID;
#ifdef SYSCALL_DEBUG
		dirstr = "none";
#endif
		break;
	case OSF1_IOC_OUT:
		dir = IOC_OUT;
#ifdef SYSCALL_DEBUG
		dirstr = "out";
#endif
		break;
	case OSF1_IOC_IN:
		dir = IOC_IN;
#ifdef SYSCALL_DEBUG
		dirstr = "in";
#endif
		break;
	case OSF1_IOC_INOUT:
		dir = IOC_INOUT;
#ifdef SYSCALL_DEBUG
		dirstr = "in-out";
#endif
		break;
	default:
		return (EINVAL);
		break;
	}
#ifdef SYSCALL_DEBUG
	if (scdebug)
		printf(
		    "OSF/1 IOCTL: group = %c, cmd = %d, len = %d, dir = %s\n",
		    group, cmd, len, dirstr);
#endif

	SCARG(&a, fd) = SCARG(uap, fd);
	SCARG(&a, com) = SCARG(uap, com);
	SCARG(&a, data) = SCARG(uap, data);
	switch (group) {
	case 'i':
		return osf1_ioctl_i(p, &a, retval, cmd, dir, len);
	case 't':
		return osf1_ioctl_t(p, &a, retval, cmd, dir, len);
	default:
		return (ENOTTY);
	}
}

int
osf1_ioctl_i(p, uap, retval, cmd, dir, len)
	struct proc *p;
	struct sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(int) com;
		syscallarg(caddr_t) data;
	} */ *uap;
	register_t *retval;
	int cmd;
	int dir;
	int len;
{

	switch (cmd) {
	case 12:			/* OSF/1 SIOCSIFADDR */
	case 14:			/* OSF/1 SIOCSIFDSTADDR */
	case 16:			/* OSF/1 SIOCSIFFLAGS (XXX) */
	case 17:			/* OSF/1 SIOCGIFFLAGS (XXX) */
	case 19:			/* OSF/1 SIOCSIFBRDADDR */
	case 22:			/* OSF/1 SIOCSIFNETMASK */
	case 23:			/* OSF/1 SIOCGIFMETRIC */
	case 24:			/* OSF/1 SIOCSIFMETRIC */
	case 25:			/* OSF/1 SIOCDIFADDR */
	case 33:			/* OSF/1 SIOCGIFADDR */
	case 34:			/* OSF/1 SIOCGIFDSTADDR */
	case 35:			/* OSF/1 SIOCGIFBRDADDR */
	case 37:			/* OSF/1 SIOCGIFNETMASK */
		/* same as in NetBSD */
		break;

	default:
		return (ENOTTY);
	}

	return sys_ioctl(p, uap, retval);
}

int
osf1_ioctl_t(p, uap, retval, cmd, dir, len)
	struct proc *p;
	struct sys_ioctl_args /* {
		syscallarg(int) fd;
		syscallarg(int) com;
		syscallarg(caddr_t) data;
	} */ *uap;
	register_t *retval;
	int cmd;
	int dir;
	int len;
{

	switch (cmd) {
#ifdef COMPAT_43
	case 8:				/* OSF/1 COMPAT_43 TIOCGETP (XXX) */
	case 9:				/* OSF/1 COMPAT_43 TIOCSETP (XXX) */
#endif
	case 19:			/* OSF/1 TIOCGETA (XXX) */
	case 20:			/* OSF/1 TIOCSETA (XXX) */
	case 21:			/* OSF/1 TIOCSETAW (XXX) */
	case 22:			/* OSF/1 TIOCSETAF (XXX) */
	case 26:			/* OSF/1 TIOCGETD (XXX) */
	case 27:			/* OSF/1 TIOCSETD (XXX) */
	case 97:			/* OSF/1 TIOCSCTTY */
	case 103:			/* OSF/1 TIOCSWINSZ */
	case 104:			/* OSF/1 TIOCGWINSZ */
		/* same as in NetBSD */
		break;
		
	default:
		return (ENOTTY);
	}

	return sys_ioctl(p, uap, retval);
}
