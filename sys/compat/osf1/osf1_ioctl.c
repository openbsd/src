/* 	$OpenBSD: osf1_ioctl.c,v 1.5 2002/03/14 01:26:50 millert Exp $ */
/*	$NetBSD: osf1_ioctl.c,v 1.11 1999/05/05 01:51:33 cgd Exp $	*/

/*
 * Copyright (c) 1999 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

#include <compat/osf1/osf1.h>
#include <compat/osf1/osf1_syscallargs.h>

#ifdef SYSCALL_DEBUG
extern int scdebug;
#endif

int osf1_ioctl_f(struct proc *p, struct sys_ioctl_args *nuap,
			    register_t *retval, int cmd, int dir, int len);
int osf1_ioctl_i(struct proc *p, struct sys_ioctl_args *nuap,
			    register_t *retval, int cmd, int dir, int len);
int osf1_ioctl_t(struct proc *p, struct sys_ioctl_args *nuap,
			    register_t *retval, int cmd, int dir, int len);
int osf1_ioctl_m(struct proc *p, struct sys_ioctl_args *nuap,
			    register_t *retval, int cmd, int dir, int len);

int
osf1_sys_ioctl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_ioctl_args *uap = v;
	struct sys_ioctl_args a;
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
	SCARG(&a, com) = SCARG(uap, com) & 0xffffffff;		/* XXX */
	SCARG(&a, data) = SCARG(uap, data);
	switch (group) {
	case 'f':
		return osf1_ioctl_f(p, &a, retval, cmd, dir, len);
	case 'i':
		return osf1_ioctl_i(p, &a, retval, cmd, dir, len);
	case 't':
		return osf1_ioctl_t(p, &a, retval, cmd, dir, len);
	case 'm':
		return osf1_ioctl_m(p, &a, retval, cmd, dir, len); 
	default:
		return (ENOTTY);
	}
}

int
osf1_ioctl_f(p, uap, retval, cmd, dir, len)
	struct proc *p;
	struct sys_ioctl_args *uap;
	register_t *retval;
	int cmd;
	int dir;
	int len;
{

	switch (cmd) {
	case 1:				/* OSF/1 FIOCLEX */
	case 2:				/* OSF/1 FIONCLEX */
	case 123:			/* OSF/1 FIOGETOWN */
	case 124:			/* OSF/1 FIOSETOWN */
	case 125:			/* OSF/1 FIOASYNC */
	case 126:			/* OSF/1 FIONBIO */
	case 127:			/* OSF/1 FIONREAD */
		/* same as in OpenBSD */
		break;
		
	default:
		return (ENOTTY);
	}

	return sys_ioctl(p, uap, retval);
}

/*
 * Mag Tape ioctl's
 */
int
osf1_ioctl_m(p, uap, retval, cmd, dir, len)
	struct proc *p;
	struct sys_ioctl_args *uap;
	register_t *retval;
	int cmd;
	int dir;
	int len;
{
	switch (cmd) {
	case 1:				/* OSF/1 MTIOCTOP (XXX) */
	case 2:				/* OSF/1 MTIOCGET (XXX) */
		/* same as in OpenBSD */
		break;
	default:
		return (ENOTTY);
	}

	return sys_ioctl(p, uap, retval);
}

int
osf1_ioctl_i(p, uap, retval, cmd, dir, len)
	struct proc *p;
	struct sys_ioctl_args *uap;
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
		/* same as in OpenBSD */
		break;

	default:
		return (ENOTTY);
	}

	return sys_ioctl(p, uap, retval);
}

int
osf1_ioctl_t(p, uap, retval, cmd, dir, len)
	struct proc *p;
	struct sys_ioctl_args *uap;
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
		/* same as in OpenBSD */
		break;
		
	default:
		return (ENOTTY);
	}

	return sys_ioctl(p, uap, retval);
}
