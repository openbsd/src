/*	$OpenBSD: kern_prot_43.c,v 1.3 2002/10/30 20:07:41 millert Exp $	*/
/*	$NetBSD: kern_prot_43.c,v 1.3 1995/10/07 06:26:27 mycroft Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)kern_prot.c	8.6 (Berkeley) 1/21/94
 */

#include <sys/param.h>
#include <sys/acct.h>
#include <sys/systm.h>
#include <sys/ucred.h>
#include <sys/proc.h>
#include <sys/timeb.h>
#include <sys/times.h>
#include <sys/malloc.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

/* ARGSUSED */
int
compat_43_sys_setregid(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_43_sys_setregid_args /* {
		syscallarg(gid_t) rgid;
		syscallarg(gid_t) egid;
	} */ *uap = v;
	struct pcred *pc = p->p_cred;
	struct sys_setresgid_args sresgidargs;
	gid_t rgid, egid, sgid;

	rgid = SCARG(uap, rgid);
	egid = SCARG(uap, egid);
	/*
         * The saved gid presents a bit of a dilemma, as it did not
         * appear in 4.3BSD.  We only set the saved gid when the real
         * gid is specified and either its value would change, or,
         * where the saved and effective gids are different.
	 */
	if (rgid != (gid_t)-1 && (rgid != pc->p_rgid ||
	    pc->p_svgid != (egid != (gid_t)-1 ? egid : pc->pc_ucred->cr_gid)))
		sgid = rgid;
	else
		sgid = (gid_t)-1;

	SCARG(&sresgidargs, rgid) = rgid;
	SCARG(&sresgidargs, egid) = egid;
	SCARG(&sresgidargs, egid) = sgid;

	return (sys_setresgid(p, &sresgidargs, retval));
}

/* ARGSUSED */
int
compat_43_sys_setreuid(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_43_sys_setreuid_args /* {
		syscallarg(uid_t) ruid;
		syscallarg(uid_t) euid;
	} */ *uap = v;
	struct pcred *pc = p->p_cred;
	struct sys_setresuid_args sresuidargs;
	uid_t ruid, euid, suid;

	ruid = SCARG(uap, ruid);
	euid = SCARG(uap, euid);
	/*
         * The saved uid presents a bit of a dilemma, as it did not
         * appear in 4.3BSD.  We only set the saved uid when the real
         * uid is specified and either its value would change, or,
         * where the saved and effective uids are different.
	 */
	if (ruid != (uid_t)-1 && (ruid != pc->p_ruid ||
	    pc->p_svuid != (euid != (uid_t)-1 ? euid : pc->pc_ucred->cr_uid)))
		suid = ruid;
	else
		suid = (uid_t)-1;

	SCARG(&sresuidargs, ruid) = ruid;
	SCARG(&sresuidargs, euid) = euid;
	SCARG(&sresuidargs, euid) = suid;

	return (sys_setresuid(p, &sresuidargs, retval));
}
