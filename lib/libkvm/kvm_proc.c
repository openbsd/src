/*	$OpenBSD: kvm_proc.c,v 1.52 2014/10/22 04:13:35 guenther Exp $	*/
/*	$NetBSD: kvm_proc.c,v 1.30 1999/03/24 05:50:50 mrg Exp $	*/
/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
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

/*
 * Proc traversal interface for kvm.  ps and w are (probably) the exclusive
 * users of this code, so we've factored it out into a separate module.
 * Thus, we keep this grunge out of the other kvm applications (i.e.,
 * most other applications are interested only in open/close/read/nlist).
 */

#define __need_process
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <nlist.h>
#include <kvm.h>
#include <errno.h>

#include <uvm/uvm_extern.h>
#include <uvm/uvm_amap.h>
#include <machine/vmparam.h>
#include <machine/pmap.h>

#include <sys/sysctl.h>

#include <limits.h>
#include <db.h>
#include <paths.h>

#include "kvm_private.h"


static char	*_kvm_ureadm(kvm_t *, const struct kinfo_proc *, u_long, u_long *);
static ssize_t	kvm_ureadm(kvm_t *, const struct kinfo_proc *, u_long, char *, size_t);

static char	**kvm_argv(kvm_t *, const struct kinfo_proc *, u_long, int, int);

static char	**kvm_doargv(kvm_t *, const struct kinfo_proc *, int,
		    void (*)(struct ps_strings *, u_long *, int *));
static int	proc_verify(kvm_t *, const struct kinfo_proc *);
static void	ps_str_a(struct ps_strings *, u_long *, int *);
static void	ps_str_e(struct ps_strings *, u_long *, int *);

static char *
_kvm_ureadm(kvm_t *kd, const struct kinfo_proc *p, u_long va, u_long *cnt)
{
	u_long addr, offset, slot;
	struct vmspace vm;
	struct vm_anon *anonp, anon;
	struct vm_map_entry vme;
	struct vm_amap amap;
	struct vm_page pg;

	if (kd->swapspc == 0) {
		kd->swapspc = _kvm_malloc(kd, kd->nbpg);
		if (kd->swapspc == 0)
			return (NULL);
	}

	/*
	 * Look through the address map for the memory object
	 * that corresponds to the given virtual address.
	 */
	if (KREAD(kd, (u_long)p->p_vmspace, &vm))
		return (NULL);
	addr = (u_long)RB_ROOT(&vm.vm_map.addr);
	while (1) {
		if (addr == 0)
			return (NULL);
		if (KREAD(kd, addr, &vme))
			return (NULL);

		if (va < vme.start)
			addr = (u_long)RB_LEFT(&vme, daddrs.addr_entry);
		else if (va >= vme.end + vme.guard + vme.fspace)
			addr = (u_long)RB_RIGHT(&vme, daddrs.addr_entry);
		else if (va >= vme.end)
			return (NULL);
		else
			break;
	}

	/*
	 * we found the map entry, now to find the object...
	 */
	if (vme.aref.ar_amap == NULL)
		return (NULL);

	addr = (u_long)vme.aref.ar_amap;
	if (KREAD(kd, addr, &amap))
		return (NULL);

	offset = va - vme.start;
	slot = offset / kd->nbpg + vme.aref.ar_pageoff;
	/* sanity-check slot number */
	if (slot > amap.am_nslot)
		return (NULL);

	addr = (u_long)amap.am_anon + (offset / kd->nbpg) * sizeof(anonp);
	if (KREAD(kd, addr, &anonp))
		return (NULL);

	addr = (u_long)anonp;
	if (KREAD(kd, addr, &anon))
		return (NULL);

	addr = (u_long)anon.an_page;
	if (addr) {
		if (KREAD(kd, addr, &pg))
			return (NULL);

		if (_kvm_pread(kd, kd->pmfd, (void *)kd->swapspc,
		    (size_t)kd->nbpg, (off_t)pg.phys_addr) != kd->nbpg)
			return (NULL);
	} else {
		if (kd->swfd == -1 ||
		    _kvm_pread(kd, kd->swfd, (void *)kd->swapspc,
		    (size_t)kd->nbpg,
		    (off_t)(anon.an_swslot * kd->nbpg)) != kd->nbpg)
			return (NULL);
	}

	/* Found the page. */
	offset %= kd->nbpg;
	*cnt = kd->nbpg - offset;
	return (&kd->swapspc[offset]);
}

void *
_kvm_reallocarray(kvm_t *kd, void *p, size_t i, size_t n)
{
	void *np = reallocarray(p, i, n);

	if (np == 0)
		_kvm_err(kd, kd->program, "out of memory");
	return (np);
}

/*
 * Read in an argument vector from the user address space of process p.
 * addr if the user-space base address of narg null-terminated contiguous
 * strings.  This is used to read in both the command arguments and
 * environment strings.  Read at most maxcnt characters of strings.
 */
static char **
kvm_argv(kvm_t *kd, const struct kinfo_proc *p, u_long addr, int narg,
    int maxcnt)
{
	char *np, *cp, *ep, *ap, **argv;
	u_long oaddr = -1;
	int len, cc;

	/*
	 * Check that there aren't an unreasonable number of arguments,
	 * and that the address is in user space.
	 */
	if (narg > ARG_MAX || addr < VM_MIN_ADDRESS || addr >= VM_MAXUSER_ADDRESS)
		return (0);

	if (kd->argv == 0) {
		/*
		 * Try to avoid reallocs.
		 */
		kd->argc = MAX(narg + 1, 32);
		kd->argv = _kvm_reallocarray(kd, NULL, kd->argc,
		    sizeof(*kd->argv));
		if (kd->argv == 0)
			return (0);
	} else if (narg + 1 > kd->argc) {
		kd->argc = MAX(2 * kd->argc, narg + 1);
		kd->argv = (char **)_kvm_reallocarray(kd, kd->argv, kd->argc,
		    sizeof(*kd->argv));
		if (kd->argv == 0)
			return (0);
	}
	if (kd->argspc == 0) {
		kd->argspc = _kvm_malloc(kd, kd->nbpg);
		if (kd->argspc == 0)
			return (0);
		kd->arglen = kd->nbpg;
	}
	if (kd->argbuf == 0) {
		kd->argbuf = _kvm_malloc(kd, kd->nbpg);
		if (kd->argbuf == 0)
			return (0);
	}
	cc = sizeof(char *) * narg;
	if (kvm_ureadm(kd, p, addr, (char *)kd->argv, cc) != cc)
		return (0);
	ap = np = kd->argspc;
	argv = kd->argv;
	len = 0;

	/*
	 * Loop over pages, filling in the argument vector.
	 */
	while (argv < kd->argv + narg && *argv != 0) {
		addr = (u_long)*argv & ~(kd->nbpg - 1);
		if (addr != oaddr) {
			if (kvm_ureadm(kd, p, addr, kd->argbuf, kd->nbpg) !=
			    kd->nbpg)
				return (0);
			oaddr = addr;
		}
		addr = (u_long)*argv & (kd->nbpg - 1);
		cp = kd->argbuf + addr;
		cc = kd->nbpg - addr;
		if (maxcnt > 0 && cc > maxcnt - len)
			cc = maxcnt - len;
		ep = memchr(cp, '\0', cc);
		if (ep != 0)
			cc = ep - cp + 1;
		if (len + cc > kd->arglen) {
			int off;
			char **pp;
			char *op = kd->argspc;
			char *newp;

			newp = _kvm_reallocarray(kd, kd->argspc,
			    kd->arglen, 2);
			if (newp == 0)
				return (0);
			kd->argspc = newp;
			kd->arglen *= 2;
			/*
			 * Adjust argv pointers in case realloc moved
			 * the string space.
			 */
			off = kd->argspc - op;
			for (pp = kd->argv; pp < argv; pp++)
				*pp += off;
			ap += off;
			np += off;
		}
		memcpy(np, cp, cc);
		np += cc;
		len += cc;
		if (ep != 0) {
			*argv++ = ap;
			ap = np;
		} else
			*argv += cc;
		if (maxcnt > 0 && len >= maxcnt) {
			/*
			 * We're stopping prematurely.  Terminate the
			 * current string.
			 */
			if (ep == 0) {
				*np = '\0';
				*argv++ = ap;
			}
			break;
		}
	}
	/* Make sure argv is terminated. */
	*argv = 0;
	return (kd->argv);
}

static void
ps_str_a(struct ps_strings *p, u_long *addr, int *n)
{
	*addr = (u_long)p->ps_argvstr;
	*n = p->ps_nargvstr;
}

static void
ps_str_e(struct ps_strings *p, u_long *addr, int *n)
{
	*addr = (u_long)p->ps_envstr;
	*n = p->ps_nenvstr;
}

/*
 * Determine if the proc indicated by p is still active.
 * This test is not 100% foolproof in theory, but chances of
 * being wrong are very low.
 */
static int
proc_verify(kvm_t *kd, const struct kinfo_proc *p)
{
	struct proc kernproc;
	struct process kernprocess;

	if (p->p_psflags & (PS_EMBRYO | PS_ZOMBIE))
		return (0);

	/*
	 * Just read in the whole proc.  It's not that big relative
	 * to the cost of the read system call.
	 */
	if (KREAD(kd, (u_long)p->p_paddr, &kernproc))
		return (0);
	if (p->p_pid != kernproc.p_pid)
		return (0);
	if (KREAD(kd, (u_long)kernproc.p_p, &kernprocess))
		return (0);
	return ((kernprocess.ps_flags & (PS_EMBRYO | PS_ZOMBIE)) == 0);
}

static char **
kvm_doargv(kvm_t *kd, const struct kinfo_proc *p, int nchr,
    void (*info)(struct ps_strings *, u_long *, int *))
{
	static struct ps_strings *ps;
	struct ps_strings arginfo;
	u_long addr;
	char **ap;
	int cnt;

	if (ps == NULL) {
		struct _ps_strings _ps;
		int mib[2];
		size_t len;

		mib[0] = CTL_VM;
		mib[1] = VM_PSSTRINGS;
		len = sizeof(_ps);
		sysctl(mib, 2, &_ps, &len, NULL, 0);
		ps = (struct ps_strings *)_ps.val;
	}

	/*
	 * Pointers are stored at the top of the user stack.
	 */
	if (p->p_psflags & (PS_EMBRYO | PS_ZOMBIE) ||
	    kvm_ureadm(kd, p, (u_long)ps, (char *)&arginfo,
	    sizeof(arginfo)) != sizeof(arginfo))
		return (0);

	(*info)(&arginfo, &addr, &cnt);
	if (cnt == 0)
		return (0);
	ap = kvm_argv(kd, p, addr, cnt, nchr);
	/*
	 * For live kernels, make sure this process didn't go away.
	 */
	if (ap != 0 && ISALIVE(kd) && !proc_verify(kd, p))
		ap = 0;
	return (ap);
}

static char **
kvm_arg_sysctl(kvm_t *kd, pid_t pid, int nchr, int env)
{
	size_t len, orglen;
	int mib[4], ret;
	char *buf;

	orglen = env ? kd->nbpg : 8 * kd->nbpg;	/* XXX - should be ARG_MAX */
	if (kd->argbuf == NULL &&
	    (kd->argbuf = _kvm_malloc(kd, orglen)) == NULL)
		return (NULL);

again:
	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC_ARGS;
	mib[2] = (int)pid;
	mib[3] = env ? KERN_PROC_ENV : KERN_PROC_ARGV;

	len = orglen;
	ret = (sysctl(mib, 4, kd->argbuf, &len, NULL, 0) < 0);
	if (ret && errno == ENOMEM) {
		buf = _kvm_reallocarray(kd, kd->argbuf, orglen, 2);
		if (buf == NULL)
			return (NULL);
		orglen *= 2;
		kd->argbuf = buf;
		goto again;
	}

	if (ret) {
		free(kd->argbuf);
		kd->argbuf = NULL;
		_kvm_syserr(kd, kd->program, "kvm_arg_sysctl");
		return (NULL);
	}
#if 0
	for (argv = (char **)kd->argbuf; *argv != NULL; argv++)
		if (strlen(*argv) > nchr)
			*argv[nchr] = '\0';
#endif

	return (char **)(kd->argbuf);
}

/*
 * Get the command args.  This code is now machine independent.
 */
char **
kvm_getargv(kvm_t *kd, const struct kinfo_proc *kp, int nchr)
{
	if (ISALIVE(kd))
		return (kvm_arg_sysctl(kd, kp->p_pid, nchr, 0));
	return (kvm_doargv(kd, kp, nchr, ps_str_a));
}

char **
kvm_getenvv(kvm_t *kd, const struct kinfo_proc *kp, int nchr)
{
	if (ISALIVE(kd))
		return (kvm_arg_sysctl(kd, kp->p_pid, nchr, 1));
	return (kvm_doargv(kd, kp, nchr, ps_str_e));
}

/*
 * Read from user space.  The user context is given by p.
 */
static ssize_t
kvm_ureadm(kvm_t *kd, const struct kinfo_proc *p, u_long uva, char *buf,
    size_t len)
{
	char *cp = buf;

	while (len > 0) {
		u_long cnt;
		size_t cc;
		char *dp;

		dp = _kvm_ureadm(kd, p, uva, &cnt);
		if (dp == 0) {
			_kvm_err(kd, 0, "invalid address (%lx)", uva);
			return (0);
		}
		cc = (size_t)MIN(cnt, len);
		bcopy(dp, cp, cc);
		cp += cc;
		uva += cc;
		len -= cc;
	}
	return (ssize_t)(cp - buf);
}
