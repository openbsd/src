/*	$OpenBSD: linux_dummy.c,v 1.10 2002/10/30 20:10:48 millert Exp $ */

/*-
 * Copyright (c) 1994-1995 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer 
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
 *
 * $FreeBSD: src/sys/i386/linux/linux_dummy.c,v 1.21 2000/01/29 12:45:35 peter Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>

#include <compat/linux/linux_types.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_syscallargs.h>

#define DUMMY(s)							\
int									\
linux_sys_ ## s(p, v, retval)						\
	struct proc *p;							\
	void *v;							\
	register_t *retval;						\
{									\
	return (unsupported_msg(p, #s));				\
}									

static int
unsupported_msg(struct proc *p, const char *fname)
{
	printf("linux: syscall %s is obsolete or not implemented (pid=%ld)\n",
	    fname, (long)p->p_pid);
	return (ENOSYS);
}

DUMMY(ostat);			/* #18 */
#ifdef PTRACE
DUMMY(ptrace);			/* #26 */
#endif
DUMMY(ofstat);			/* #28 */
DUMMY(stty);			/* #31 */
DUMMY(gtty);			/* #32 */
DUMMY(ftime);			/* #35 */
DUMMY(prof);			/* #44 */
DUMMY(phys);			/* #52 */
DUMMY(lock);			/* #53 */
DUMMY(mpx);			/* #56 */
DUMMY(ulimit);			/* #58 */
DUMMY(ustat);			/* #62 */
#ifndef __i386__
DUMMY(ioperm);			/* #101 */
#endif
DUMMY(klog);			/* #103 */
#ifndef __i386__
DUMMY(iopl);			/* #110 */
#endif
DUMMY(vhangup);			/* #111 */
DUMMY(idle);			/* #112 */
DUMMY(vm86old);			/* #113 */
DUMMY(swapoff);			/* #115 */
DUMMY(sysinfo);			/* #116 */
#ifndef __i386__
DUMMY(modify_ldt);		/* #123 */
#endif
DUMMY(adjtimex);		/* #124 */
DUMMY(create_module);		/* #127 */
DUMMY(init_module);		/* #128 */
DUMMY(delete_module);		/* #129 */
DUMMY(get_kernel_syms);		/* #130 */
DUMMY(quotactl);		/* #131 */
DUMMY(bdflush);			/* #134 */
DUMMY(sysfs);			/* #135 */
DUMMY(afs_syscall);		/* #137 */
DUMMY(mlockall);		/* #152 */
DUMMY(munlockall);		/* #153 */
DUMMY(sched_rr_get_interval);	/* #161 */
DUMMY(vm86);			/* #166 */
DUMMY(query_module);		/* #167 */
DUMMY(nfsservctl);		/* #169 */
DUMMY(prctl);			/* #172 */
DUMMY(rt_sigtimedwait);		/* #177 */
DUMMY(rt_queueinfo);		/* #178 */
DUMMY(capget);			/* #184 */
DUMMY(capset);			/* #185 */
DUMMY(sendfile);		/* #187 */
DUMMY(getpmsg);			/* #188 */
DUMMY(putpmsg);			/* #189 */
DUMMY(mmap2);			/* #192 */
DUMMY(lchown);			/* #198 */
DUMMY(setreuid);		/* #203 */
DUMMY(setregid);		/* #204 */
DUMMY(fchown);			/* #207 */
DUMMY(chown);			/* #212 */
DUMMY(setfsgid);		/* #216 */
DUMMY(pivot_root);		/* #217 */
DUMMY(mincore);			/* #218 */
DUMMY(madvise);			/* #219 */
