/* $OpenBSD: osf1_misc.c,v 1.18 2007/09/15 10:10:37 martin Exp $ */
/* $NetBSD: osf1_misc.c,v 1.55 2000/06/28 15:39:33 mrg Exp $ */

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
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/filedesc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/reboot.h>
#include <sys/syscallargs.h>
#include <sys/exec.h>
#include <sys/vnode.h>
#include <sys/socketvar.h>
#include <sys/resource.h>
#include <sys/resourcevar.h>
#include <sys/wait.h>

#include <compat/osf1/osf1.h>
#include <compat/osf1/osf1_syscallargs.h>
#include <compat/osf1/osf1_util.h>
#include <compat/osf1/osf1_cvt.h>

#ifdef SYSCALL_DEBUG
extern int scdebug;
#endif

const char osf1_emul_path[] = "/emul/osf1";

int
osf1_sys_classcntl(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	/* XXX */
	return (ENOSYS);
}

int
osf1_sys_reboot(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_reboot_args *uap = v;
	struct sys_reboot_args a;
	unsigned long leftovers;

	/* translate opt */
	SCARG(&a, opt) = emul_flags_translate(osf1_reboot_opt_xtab,
	    SCARG(uap, opt), &leftovers);
	if (leftovers != 0)
		return (EINVAL);

	/* SCARG(&a, bootstr) = NULL; */

	return sys_reboot(p, &a, retval);
}

int
osf1_sys_set_program_attributes(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_set_program_attributes_args *uap = v;
	segsz_t tsize, dsize;

	tsize = atop(SCARG(uap, tsize));
	dsize = atop(SCARG(uap, dsize));

	if (dsize > p->p_rlimit[RLIMIT_DATA].rlim_cur)
		return (ENOMEM);
	if (tsize > MAXTSIZ)
		return (ENOMEM);

	p->p_vmspace->vm_taddr = SCARG(uap, taddr);
	p->p_vmspace->vm_tsize = tsize;
	p->p_vmspace->vm_daddr = SCARG(uap, daddr);
	p->p_vmspace->vm_dsize = dsize;

	return (0);
}

int
osf1_sys_setsysinfo(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{

	/* XXX */
	return (0);
}

int
osf1_sys_sysinfo(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_sysinfo_args *uap = v;
	const char *string;
	int error;

	error = 0;
	switch (SCARG(uap, cmd)) {
	case OSF1_SI_SYSNAME:
		goto should_handle;
		/* string = ostype; */
		break;

	case OSF1_SI_HOSTNAME:
		string = hostname;
		break;

	case OSF1_SI_RELEASE:
		string = version;
		break;

	case OSF1_SI_VERSION:
		goto should_handle;

	case OSF1_SI_MACHINE:
		string = MACHINE;
		break;

	case OSF1_SI_ARCHITECTURE:
		string = MACHINE_ARCH;
		break;

	case OSF1_SI_HW_SERIAL:
		string = "666";			/* OSF/1 emulation?  YES! */
		break;

	case OSF1_SI_HW_PROVIDER:
		string = "unknown";
		break;

	case OSF1_SI_SRPC_DOMAIN:
		goto dont_care;

	case OSF1_SI_SET_HOSTNAME:
		goto should_handle;

	case OSF1_SI_SET_SYSNAME:
		goto should_handle;

	case OSF1_SI_SET_SRPC_DOMAIN:
		goto dont_care;

	default:
should_handle:
		printf("osf1_sys_sysinfo(%d, %p, 0x%lx)\n", SCARG(uap, cmd),
		    SCARG(uap, buf), SCARG(uap,len));
dont_care:
		error = EINVAL;
		break;
	};

	if (error == 0)
		error = copyoutstr(string, SCARG(uap, buf), SCARG(uap, len),
		    NULL);

	return (error);
}

int
osf1_sys_uname(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_uname_args *uap = v;
        struct osf1_utsname u;
        const char *cp;
	extern char hostname[], machine[];
        char *dp, *ep;

	/* XXX would use stackgap, but our struct utsname is too big! */

	bzero(&u, sizeof(u));
	strlcpy(u.sysname, ostype, sizeof(u.sysname));
	strlcpy(u.nodename, hostname, sizeof(u.nodename));
	strlcpy(u.release, osrelease, sizeof(u.release));

        dp = u.version;
        ep = &u.version[sizeof(u.version) - 1];
        for (cp = version; *cp && *cp != '('; cp++)
                ;
        for (cp++; *cp && *cp != ')' && dp < ep; cp++)
                *dp++ = *cp;
        for (; *cp && *cp != '#'; cp++)
                ;
        for (; *cp && *cp != ':' && dp < ep; cp++)
                *dp++ = *cp;
        *dp = '\0';

	strlcpy(u.machine, machine, sizeof(u.machine));

        return (copyout((caddr_t)&u, (caddr_t)SCARG(uap, name), sizeof u));
}

int
osf1_sys_usleep_thread(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_usleep_thread_args *uap = v;
	struct osf1_timeval otv, endotv;
	struct timeval tv, endtv;
	u_long ticks;
	int error;

	if ((error = copyin(SCARG(uap, sleep), &otv, sizeof otv)))
		return (error);
	tv.tv_sec = otv.tv_sec;
	tv.tv_usec = otv.tv_usec;

	ticks = howmany((u_long)tv.tv_sec * 1000000 + tv.tv_usec, tick);
	if (ticks == 0)
		ticks = 1;

	getmicrotime(&tv);

	tsleep(p, PUSER|PCATCH, "uslpthrd", ticks);	/* XXX */

	if (SCARG(uap, slept) != NULL) {
		struct timeval tv2;

		getmicrotime(&tv2);
		timersub(&tv2, &tv, &endtv);

		if (endtv.tv_sec < 0 || endtv.tv_usec < 0)
			endtv.tv_sec = endtv.tv_usec = 0;

		endotv.tv_sec = endtv.tv_sec;
		endotv.tv_usec = endtv.tv_usec;
		error = copyout(&endotv, SCARG(uap, slept), sizeof endotv);
	}
	return (error);
}

int
osf1_sys_wait4(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct osf1_sys_wait4_args *uap = v;
	struct sys_wait4_args a;
	struct osf1_rusage osf1_rusage;
	struct rusage netbsd_rusage;
	unsigned long leftovers;
	caddr_t sg;
	int error;

	SCARG(&a, pid) = SCARG(uap, pid);
	SCARG(&a, status) = SCARG(uap, status);

	/* translate options */
	SCARG(&a, options) = emul_flags_translate(osf1_wait_options_xtab,
	    SCARG(uap, options), &leftovers);
	if (leftovers != 0)
		return (EINVAL);

	if (SCARG(uap, rusage) == NULL)
		SCARG(&a, rusage) = NULL;
	else {
		sg = stackgap_init(p->p_emul);
		SCARG(&a, rusage) = stackgap_alloc(&sg, sizeof netbsd_rusage);
	}

	error = sys_wait4(p, &a, retval);

	if (error == 0 && SCARG(&a, rusage) != NULL) {
		error = copyin((caddr_t)SCARG(&a, rusage),
		    (caddr_t)&netbsd_rusage, sizeof netbsd_rusage);
		if (error == 0) {
			osf1_cvt_rusage_from_native(&netbsd_rusage,
			    &osf1_rusage);
			error = copyout((caddr_t)&osf1_rusage,
			    (caddr_t)SCARG(uap, rusage), sizeof osf1_rusage);
		}
	}

	return (error);
}
