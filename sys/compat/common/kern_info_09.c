/*	$OpenBSD: kern_info_09.c,v 1.8 2001/11/06 19:53:17 miod Exp $	*/
/*	$NetBSD: kern_info_09.c,v 1.5 1996/02/21 00:10:59 cgd Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)subr_xxx.c	8.1 (Berkeley) 6/10/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/unistd.h>
#include <uvm/uvm_extern.h>
#include <sys/sysctl.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

/*
 * Note that while we no longer have a COMPAT_09 kernel option,
 * there are other COMPAT_* options that need these old functions.
 */

/* ARGSUSED */
int
compat_09_sys_getdomainname(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_09_sys_getdomainname_args /* {
		syscallarg(char *) domainname;
		syscallarg(int) len;
	} */ *uap = v;
	int name;
	size_t sz;

	name = KERN_DOMAINNAME;
	sz = SCARG(uap,len);
	return (kern_sysctl(&name, 1, SCARG(uap, domainname), &sz, 0, 0, p));
}


/* ARGSUSED */
int
compat_09_sys_setdomainname(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_09_sys_setdomainname_args /* {
		syscallarg(char *) domainname;
		syscallarg(int) len;
	} */ *uap = v;
	int name;
	int error;

	if ((error = suser(p->p_ucred, &p->p_acflag)) != 0)
		return (error);
	name = KERN_DOMAINNAME;
	return (kern_sysctl(&name, 1, 0, 0, SCARG(uap, domainname),
			    SCARG(uap, len), p));
}

struct outsname {
	char	sysname[32];
	char	nodename[32];
	char	release[32];
	char	version[32];
	char	machine[32];
};

/* ARGSUSED */
int
compat_09_sys_uname(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_09_sys_uname_args /* {
		syscallarg(struct outsname *) name;
	} */ *uap = v;
	struct outsname outsname;
	char *cp, *dp, *ep;
	extern char ostype[], osrelease[];

	strncpy(outsname.sysname, ostype, sizeof(outsname.sysname));
	strncpy(outsname.nodename, hostname, sizeof(outsname.nodename));
	strncpy(outsname.release, osrelease, sizeof(outsname.release));
	dp = outsname.version;
	ep = &outsname.version[sizeof(outsname.version) - 1];
	for (cp = version; *cp && *cp != '('; cp++)
		;
	for (cp++; *cp && *cp != ')' && dp < ep; cp++)
		*dp++ = *cp;
	for (; *cp && *cp != '#'; cp++)
		;
	for (; *cp && *cp != ':' && dp < ep; cp++)
		*dp++ = *cp;
	*dp = '\0';
	strncpy(outsname.machine, MACHINE, sizeof(outsname.machine));

	return (copyout((caddr_t)&outsname, (caddr_t)SCARG(uap, name),
			sizeof(struct outsname)));
}
