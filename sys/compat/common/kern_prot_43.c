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
		syscallarg(int) rgid;
		syscallarg(int) egid;
	} */ *uap = v;
	struct sys_setegid_args segidargs;
	struct sys_setgid_args sgidargs;

	/*
	 * There are five cases, described above in osetreuid()
	 */
	if (SCARG(uap, rgid) == (gid_t)-1) {
		if (SCARG(uap, egid) == (gid_t)-1)
			return (0);				/* -1, -1 */
		SCARG(&segidargs, egid) = SCARG(uap, egid);	/* -1,  N */
		return (sys_setegid(p, &segidargs, retval));
	}
	if (SCARG(uap, egid) == (gid_t)-1) {
		SCARG(&segidargs, egid) = SCARG(uap, rgid);	/* N, -1 */
		return (sys_setegid(p, &segidargs, retval));
	}
	SCARG(&sgidargs, gid) = SCARG(uap, rgid);	/* N, N and N, M */
	return (sys_setgid(p, &sgidargs, retval));
}

/* ARGSUSED */
int
compat_43_sys_setreuid(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct compat_43_sys_setreuid_args /* {
		syscallarg(int) ruid;
		syscallarg(int) euid;
	} */ *uap = v;
	struct sys_seteuid_args seuidargs;
	struct sys_setuid_args suidargs;

	/*
	 * There are five cases, and we attempt to emulate them in
	 * the following fashion:
	 * -1, -1: return 0. This is correct emulation.
	 * -1,  N: call seteuid(N). This is correct emulation.
	 *  N, -1: if we called setuid(N), our euid would be changed
	 *         to N as well. the theory is that we don't want to
	 * 	   revoke root access yet, so we call seteuid(N)
	 * 	   instead. This is incorrect emulation, but often
	 *	   suffices enough for binary compatibility.
	 *  N,  N: call setuid(N). This is correct emulation.
	 *  N,  M: call setuid(N). This is close to correct emulation.
	 */
	if (SCARG(uap, ruid) == (uid_t)-1) {
		if (SCARG(uap, euid) == (uid_t)-1)
			return (0);				/* -1, -1 */
		SCARG(&seuidargs, euid) = SCARG(uap, euid);	/* -1,  N */
		return (sys_seteuid(p, &seuidargs, retval));
	}
	if (SCARG(uap, euid) == (uid_t)-1) {
		SCARG(&seuidargs, euid) = SCARG(uap, ruid);	/* N, -1 */
		return (sys_seteuid(p, &seuidargs, retval));
	}
	SCARG(&suidargs, uid) = SCARG(uap, ruid);	/* N, N and N, M */
	return (sys_setuid(p, &suidargs, retval));
}
