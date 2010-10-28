/*	$OpenBSD: sysconf.c,v 1.12 2010/10/28 02:06:00 deraadt Exp $ */
/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Sean Eric Fagan of Cygnus Support.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#include <sys/param.h>
#include <sys/sem.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/vmmeter.h>

#include <errno.h>
#include <pwd.h>
#include <unistd.h>

/*
 * sysconf --
 *	get configurable system variables.
 *
 * XXX
 * POSIX 1003.1 (ISO/IEC 9945-1, 4.8.1.3) states that the variable values
 * not change during the lifetime of the calling process.  This would seem
 * to require that any change to system limits kill all running processes.
 * A workaround might be to cache the values when they are first retrieved
 * and then simply return the cached value on subsequent calls.  This is
 * less useful than returning up-to-date values, however.
 */
long
sysconf(int name)
{
	struct rlimit rl;
	size_t len;
	int mib[3], value, namelen;

	len = sizeof(value);
	namelen = 2;

	switch (name) {
/* 1003.1 */
	case _SC_ARG_MAX:
		mib[0] = CTL_KERN;
		mib[1] = KERN_ARGMAX;
		break;
	case _SC_CHILD_MAX:
		return (getrlimit(RLIMIT_NPROC, &rl) ? -1 : rl.rlim_cur);
	case _SC_CLK_TCK:
		return (CLK_TCK);
	case _SC_JOB_CONTROL:
		mib[0] = CTL_KERN;
		mib[1] = KERN_JOB_CONTROL;
		goto yesno;
	case _SC_NGROUPS_MAX:
		mib[0] = CTL_KERN;
		mib[1] = KERN_NGROUPS;
		break;
	case _SC_OPEN_MAX:
		return (getrlimit(RLIMIT_NOFILE, &rl) ? -1 : rl.rlim_cur);
	case _SC_STREAM_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_STREAM_MAX;
		break;
	case _SC_TZNAME_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_TZNAME_MAX;
		break;
	case _SC_SAVED_IDS:
		mib[0] = CTL_KERN;
		mib[1] = KERN_SAVED_IDS;
		goto yesno;
	case _SC_VERSION:
		mib[0] = CTL_KERN;
		mib[1] = KERN_POSIX1;
		break;

/* 1003.1b */
	case _SC_PAGESIZE:
		mib[0] = CTL_HW;
		mib[1] = HW_PAGESIZE;
		break;
	case _SC_FSYNC:
		mib[0] = CTL_KERN;
		mib[1] = KERN_FSYNC;
		goto yesno;

/* 1003.1c */
	case _SC_LOGIN_NAME_MAX:
		return (LOGIN_NAME_MAX);

	case _SC_THREAD_SAFE_FUNCTIONS:
		return (_POSIX_THREAD_SAFE_FUNCTIONS);

	case _SC_GETGR_R_SIZE_MAX:
		return (_PW_BUF_LEN);

	case _SC_GETPW_R_SIZE_MAX:
		return (_PW_BUF_LEN);

/* 1003.2 */
	case _SC_BC_BASE_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_BC_BASE_MAX;
		break;
	case _SC_BC_DIM_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_BC_DIM_MAX;
		break;
	case _SC_BC_SCALE_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_BC_SCALE_MAX;
		break;
	case _SC_BC_STRING_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_BC_STRING_MAX;
		break;
	case _SC_COLL_WEIGHTS_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_COLL_WEIGHTS_MAX;
		break;
	case _SC_EXPR_NEST_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_EXPR_NEST_MAX;
		break;
	case _SC_LINE_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_LINE_MAX;
		break;
	case _SC_RE_DUP_MAX:
		mib[0] = CTL_USER;
		mib[1] = USER_RE_DUP_MAX;
		break;
	case _SC_2_VERSION:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_VERSION;
		break;
	case _SC_2_C_BIND:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_C_BIND;
		goto yesno;
	case _SC_2_C_DEV:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_C_DEV;
		goto yesno;
	case _SC_2_CHAR_TERM:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_CHAR_TERM;
		goto yesno;
	case _SC_2_FORT_DEV:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_FORT_DEV;
		goto yesno;
	case _SC_2_FORT_RUN:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_FORT_RUN;
		goto yesno;
	case _SC_2_LOCALEDEF:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_LOCALEDEF;
		goto yesno;
	case _SC_2_SW_DEV:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_SW_DEV;
		goto yesno;
	case _SC_2_UPE:
		mib[0] = CTL_USER;
		mib[1] = USER_POSIX2_UPE;
		goto yesno;

/* XPG 4.2 */
	case _SC_XOPEN_SHM:
		mib[0] = CTL_KERN;
		mib[1] = KERN_SYSVSHM;

yesno:		if (sysctl(mib, namelen, &value, &len, NULL, 0) == -1)
			return (-1);
		if (value == 0)
			return (-1);
		return (value);
		break;
	case _SC_SEM_NSEMS_MAX:
	case _SC_SEM_VALUE_MAX:
		mib[0] = CTL_KERN;
		mib[1] = KERN_SEMINFO;
		mib[2] = name = _SC_SEM_NSEMS_MAX ?
		    KERN_SEMINFO_SEMMNS : KERN_SEMINFO_SEMVMX;
		namelen = 3;
		break;

/* Unsorted */
	case _SC_HOST_NAME_MAX:
		return (MAXHOSTNAMELEN - 1); /* does not include \0 */

/* Extensions */
	case _SC_PHYS_PAGES:
	{
		int64_t physmem;

		mib[0] = CTL_HW;
		mib[1] = HW_PHYSMEM64;
		len = sizeof(physmem);
		if (sysctl(mib, namelen, &physmem, &len, NULL, 0) == -1)
			return (-1);
		return (physmem / getpagesize());
	}
	case _SC_AVPHYS_PAGES:
	{
		struct vmtotal vmtotal;

		mib[0] = CTL_VM;
		mib[1] = VM_METER;
		len = sizeof(vmtotal);
		if (sysctl(mib, namelen, &vmtotal, &len, NULL, 0) == -1)
			return (-1);
		return (vmtotal.t_free);
	}

	case _SC_NPROCESSORS_CONF:
		mib[0] = CTL_HW;
		mib[1] = HW_NCPU;
		break;
	case _SC_NPROCESSORS_ONLN:
		mib[0] = CTL_HW;
		mib[1] = HW_NCPU;
		break;

	default:
		errno = EINVAL;
		return (-1);
	}
	return (sysctl(mib, namelen, &value, &len, NULL, 0) == -1 ? -1 : value); 
}
