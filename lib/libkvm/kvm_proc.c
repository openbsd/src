/*	$OpenBSD: kvm_proc.c,v 1.41 2010/07/26 01:56:27 guenther Exp $	*/
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
#include <sys/user.h>
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

#include <uvm/uvm_extern.h>
#include <uvm/uvm_amap.h>
#include <machine/vmparam.h>
#include <machine/pmap.h>

#include <sys/sysctl.h>

#include <limits.h>
#include <db.h>
#include <paths.h>

#include "kvm_private.h"

/*
 * Common info from kinfo_proc and kinfo_proc2 used by helper routines.
 */
struct miniproc {
	struct	vmspace *p_vmspace;
	char	p_stat;
	struct	proc *p_paddr;
	pid_t	p_pid;
};

/*
 * Convert from struct proc and kinfo_proc{,2} to miniproc.
 */
#define PTOMINI(kp, p) \
	do { \
		(p)->p_stat = (kp)->p_stat; \
		(p)->p_pid = (kp)->p_pid; \
		(p)->p_paddr = NULL; \
		(p)->p_vmspace = (kp)->p_vmspace; \
	} while (/*CONSTCOND*/0);

#define KPTOMINI(kp, p) \
	do { \
		(p)->p_stat = (kp)->kp_proc.p_stat; \
		(p)->p_pid = (kp)->kp_proc.p_pid; \
		(p)->p_paddr = (kp)->kp_eproc.e_paddr; \
		(p)->p_vmspace = (kp)->kp_proc.p_vmspace; \
	} while (/*CONSTCOND*/0);

#define KP2TOMINI(kp, p) \
	do { \
		(p)->p_stat = (kp)->p_stat; \
		(p)->p_pid = (kp)->p_pid; \
		(p)->p_paddr = (void *)(long)(kp)->p_paddr; \
		(p)->p_vmspace = (void *)(long)(kp)->p_vmspace; \
	} while (/*CONSTCOND*/0);


ssize_t		kvm_uread(kvm_t *, const struct proc *, u_long, char *, size_t);

static char	*_kvm_ureadm(kvm_t *, const struct miniproc *, u_long, u_long *);
static ssize_t	kvm_ureadm(kvm_t *, const struct miniproc *, u_long, char *, size_t);

static char	**kvm_argv(kvm_t *, const struct miniproc *, u_long, int, int);

static int	kvm_deadprocs(kvm_t *, int, int, u_long, u_long, int);
static char	**kvm_doargv(kvm_t *, const struct miniproc *, int,
		    void (*)(struct ps_strings *, u_long *, int *));
static int	kvm_proclist(kvm_t *, int, int, struct proc *,
		    struct kinfo_proc *, int);
static int	proc_verify(kvm_t *, const struct miniproc *);
static void	ps_str_a(struct ps_strings *, u_long *, int *);
static void	ps_str_e(struct ps_strings *, u_long *, int *);

static char *
_kvm_ureadm(kvm_t *kd, const struct miniproc *p, u_long va, u_long *cnt)
{
	u_long addr, head, offset, slot;
	struct vm_anon *anonp, anon;
	struct vm_map_entry vme;
	struct vm_amap amap;
	struct vm_page pg;

	if (kd->swapspc == 0) {
		kd->swapspc = _kvm_malloc(kd, kd->nbpg);
		if (kd->swapspc == 0)
			return (0);
	}

	/*
	 * Look through the address map for the memory object
	 * that corresponds to the given virtual address.
	 * The header just has the entire valid range.
	 */
	head = (u_long)&p->p_vmspace->vm_map.header;
	addr = head;
	while (1) {
		if (KREAD(kd, addr, &vme))
			return (0);

		if (va >= vme.start && va < vme.end &&
		    vme.aref.ar_amap != NULL)
			break;

		addr = (u_long)vme.next;
		if (addr == head)
			return (0);
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

char *
_kvm_uread(kvm_t *kd, const struct proc *p, u_long va, u_long *cnt)
{
	struct miniproc mp;

	PTOMINI(p, &mp);
	return (_kvm_ureadm(kd, &mp, va, cnt));
}

/*
 * Read proc's from memory file into buffer bp, which has space to hold
 * at most maxcnt procs.
 */
static int
kvm_proclist(kvm_t *kd, int what, int arg, struct proc *p,
    struct kinfo_proc *bp, int maxcnt)
{
	struct session sess;
	struct eproc eproc;
	struct proc proc;
	struct process process;
	struct pgrp pgrp;
	struct tty tty;
	int cnt = 0;

	for (; cnt < maxcnt && p != NULL; p = LIST_NEXT(&proc, p_list)) {
		if (KREAD(kd, (u_long)p, &proc)) {
			_kvm_err(kd, kd->program, "can't read proc at %x", p);
			return (-1);
		}
		if (KREAD(kd, (u_long)proc.p_p, &process)) {
			_kvm_err(kd, kd->program, "can't read process at %x", proc.p_p);
			return (-1);
		}
		if (KREAD(kd, (u_long)process.ps_cred, &eproc.e_pcred) == 0)
			KREAD(kd, (u_long)eproc.e_pcred.pc_ucred,
			    &eproc.e_ucred);

		switch (what) {
		case KERN_PROC_PID:
			if (proc.p_pid != (pid_t)arg)
				continue;
			break;

		case KERN_PROC_UID:
			if (eproc.e_ucred.cr_uid != (uid_t)arg)
				continue;
			break;

		case KERN_PROC_RUID:
			if (eproc.e_pcred.p_ruid != (uid_t)arg)
				continue;
			break;

		case KERN_PROC_ALL:
			if (proc.p_flag & P_SYSTEM)
				continue;
			break;
		}
		/*
		 * We're going to add another proc to the set.  If this
		 * will overflow the buffer, assume the reason is because
		 * nprocs (or the proc list) is corrupt and declare an error.
		 */
		if (cnt >= maxcnt) {
			_kvm_err(kd, kd->program, "nprocs corrupt");
			return (-1);
		}
		/*
		 * gather eproc
		 */
		eproc.e_paddr = p;
		if (KREAD(kd, (u_long)process.ps_pgrp, &pgrp)) {
			_kvm_err(kd, kd->program, "can't read pgrp at %x",
			    process.ps_pgrp);
			return (-1);
		}
		eproc.e_sess = pgrp.pg_session;
		eproc.e_pgid = pgrp.pg_id;
		eproc.e_jobc = pgrp.pg_jobc;
		if (KREAD(kd, (u_long)pgrp.pg_session, &sess)) {
			_kvm_err(kd, kd->program, "can't read session at %x",
			    pgrp.pg_session);
			return (-1);
		}
		if ((process.ps_flags & PS_CONTROLT) && sess.s_ttyp != NULL) {
			if (KREAD(kd, (u_long)sess.s_ttyp, &tty)) {
				_kvm_err(kd, kd->program,
				    "can't read tty at %x", sess.s_ttyp);
				return (-1);
			}
			eproc.e_tdev = tty.t_dev;
			eproc.e_tsess = tty.t_session;
			if (tty.t_pgrp != NULL) {
				if (KREAD(kd, (u_long)tty.t_pgrp, &pgrp)) {
					_kvm_err(kd, kd->program,
					    "can't read tpgrp at &x",
					    tty.t_pgrp);
					return (-1);
				}
				eproc.e_tpgid = pgrp.pg_id;
			} else
				eproc.e_tpgid = -1;
		} else
			eproc.e_tdev = NODEV;
		eproc.e_flag = sess.s_ttyvp ? EPROC_CTTY : 0;
		if (sess.s_leader == proc.p_p)
			eproc.e_flag |= EPROC_SLEADER;
		if (proc.p_wmesg)
			(void)kvm_read(kd, (u_long)proc.p_wmesg,
			    eproc.e_wmesg, WMESGLEN);

		(void)kvm_read(kd, (u_long)proc.p_vmspace,
		    &eproc.e_vm, sizeof(eproc.e_vm));

		eproc.e_xsize = eproc.e_xrssize = 0;
		eproc.e_xccount = eproc.e_xswrss = 0;

		switch (what) {
		case KERN_PROC_PGRP:
			if (eproc.e_pgid != (pid_t)arg)
				continue;
			break;

		case KERN_PROC_TTY:
			if ((process.ps_flags & PS_CONTROLT) == 0 ||
			    eproc.e_tdev != (dev_t)arg)
				continue;
			break;
		}
		proc.p_flag |= process.ps_flags;
		bcopy(&proc, &bp->kp_proc, sizeof(proc));
		bcopy(&eproc, &bp->kp_eproc, sizeof(eproc));
		++bp;
		++cnt;
	}
	return (cnt);
}

/*
 * Build proc info array by reading in proc list from a crash dump.
 * Return number of procs read.  maxcnt is the max we will read.
 */
static int
kvm_deadprocs(kvm_t *kd, int what, int arg, u_long a_allproc,
    u_long a_zombproc, int maxcnt)
{
	struct kinfo_proc *bp = kd->procbase;
	struct proc *p;
	int acnt, zcnt;

	if (KREAD(kd, a_allproc, &p)) {
		_kvm_err(kd, kd->program, "cannot read allproc");
		return (-1);
	}
	acnt = kvm_proclist(kd, what, arg, p, bp, maxcnt);
	if (acnt < 0)
		return (acnt);

	if (KREAD(kd, a_zombproc, &p)) {
		_kvm_err(kd, kd->program, "cannot read zombproc");
		return (-1);
	}
	zcnt = kvm_proclist(kd, what, arg, p, bp + acnt, maxcnt - acnt);
	if (zcnt < 0)
		zcnt = 0;

	return (acnt + zcnt);
}

struct kinfo_proc *
kvm_getprocs(kvm_t *kd, int op, int arg, int *cnt)
{
	int mib[4], st, nprocs;
	size_t size;

	if (kd->procbase != 0) {
		free((void *)kd->procbase);
		/*
		 * Clear this pointer in case this call fails.  Otherwise,
		 * kvm_close() will free it again.
		 */
		kd->procbase = 0;
	}
	if (ISALIVE(kd)) {
		size = 0;
		mib[0] = CTL_KERN;
		mib[1] = KERN_PROC;
		mib[2] = op;
		mib[3] = arg;
		st = sysctl(mib, 4, NULL, &size, NULL, 0);
		if (st == -1) {
			_kvm_syserr(kd, kd->program, "kvm_getprocs");
			return (0);
		}
		kd->procbase = _kvm_malloc(kd, size);
		if (kd->procbase == 0)
			return (0);
		st = sysctl(mib, 4, kd->procbase, &size, NULL, 0);
		if (st == -1) {
			_kvm_syserr(kd, kd->program, "kvm_getprocs");
			return (0);
		}
		if (size % sizeof(struct kinfo_proc) != 0) {
			_kvm_err(kd, kd->program,
			    "proc size mismatch (%d total, %d chunks)",
			    size, sizeof(struct kinfo_proc));
			return (0);
		}
		nprocs = size / sizeof(struct kinfo_proc);
	} else {
		struct nlist nl[4], *p;

		memset(nl, 0, sizeof(nl));
		nl[0].n_name = "_nprocs";
		nl[1].n_name = "_allproc";
		nl[2].n_name = "_zombproc";
		nl[3].n_name = NULL;

		if (kvm_nlist(kd, nl) != 0) {
			for (p = nl; p->n_type != 0; ++p)
				;
			_kvm_err(kd, kd->program,
			    "%s: no such symbol", p->n_name);
			return (0);
		}
		if (KREAD(kd, nl[0].n_value, &nprocs)) {
			_kvm_err(kd, kd->program, "can't read nprocs");
			return (0);
		}
		size = nprocs * sizeof(struct kinfo_proc);
		kd->procbase = _kvm_malloc(kd, size);
		if (kd->procbase == 0)
			return (0);

		nprocs = kvm_deadprocs(kd, op, arg, nl[1].n_value,
		    nl[2].n_value, nprocs);
#ifdef notdef
		size = nprocs * sizeof(struct kinfo_proc);
		(void)realloc(kd->procbase, size);
#endif
	}
	*cnt = nprocs;
	return (kd->procbase);
}

void
_kvm_freeprocs(kvm_t *kd)
{
	if (kd->procbase) {
		free(kd->procbase);
		kd->procbase = 0;
	}
}

void *
_kvm_realloc(kvm_t *kd, void *p, size_t n)
{
	void *np = (void *)realloc(p, n);

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
kvm_argv(kvm_t *kd, const struct miniproc *p, u_long addr, int narg,
    int maxcnt)
{
	char *np, *cp, *ep, *ap, **argv;
	u_long oaddr = -1;
	int len, cc;

	/*
	 * Check that there aren't an unreasonable number of agruments,
	 * and that the address is in user space.
	 */
	if (narg > ARG_MAX || addr < VM_MIN_ADDRESS || addr >= VM_MAXUSER_ADDRESS)
		return (0);

	if (kd->argv == 0) {
		/*
		 * Try to avoid reallocs.
		 */
		kd->argc = MAX(narg + 1, 32);
		kd->argv = _kvm_malloc(kd, kd->argc *
		    sizeof(*kd->argv));
		if (kd->argv == 0)
			return (0);
	} else if (narg + 1 > kd->argc) {
		kd->argc = MAX(2 * kd->argc, narg + 1);
		kd->argv = (char **)_kvm_realloc(kd, kd->argv, kd->argc *
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

			kd->arglen *= 2;
			kd->argspc = (char *)_kvm_realloc(kd, kd->argspc,
			    kd->arglen);
			if (kd->argspc == 0)
				return (0);
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
proc_verify(kvm_t *kd, const struct miniproc *p)
{
	struct proc kernproc;

	/*
	 * Just read in the whole proc.  It's not that big relative
	 * to the cost of the read system call.
	 */
	if (kvm_read(kd, (u_long)p->p_paddr, &kernproc, sizeof(kernproc)) !=
	    sizeof(kernproc))
		return (0);
	return (p->p_pid == kernproc.p_pid &&
	    (kernproc.p_stat != SZOMB || p->p_stat == SZOMB));
}

static char **
kvm_doargv(kvm_t *kd, const struct miniproc *p, int nchr,
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
	if (p->p_stat == SZOMB ||
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
		orglen *= 2;
		buf = _kvm_realloc(kd, kd->argbuf, orglen);
		if (buf == NULL)
			return (NULL);
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
	struct miniproc p;

	if (ISALIVE(kd))
		return (kvm_arg_sysctl(kd, kp->kp_proc.p_pid, nchr, 0));
	KPTOMINI(kp, &p);
	return (kvm_doargv(kd, &p, nchr, ps_str_a));
}

char **
kvm_getenvv(kvm_t *kd, const struct kinfo_proc *kp, int nchr)
{
	struct miniproc p;

	if (ISALIVE(kd))
		return (kvm_arg_sysctl(kd, kp->kp_proc.p_pid, nchr, 1));
	KPTOMINI(kp, &p);
	return (kvm_doargv(kd, &p, nchr, ps_str_e));
}

char **
kvm_getargv2(kvm_t *kd, const struct kinfo_proc2 *kp, int nchr)
{
	struct miniproc p;

	if (ISALIVE(kd))
		return (kvm_arg_sysctl(kd, kp->p_pid, nchr, 0));
	KP2TOMINI(kp, &p);
	return (kvm_doargv(kd, &p, nchr, ps_str_a));
}

char **
kvm_getenvv2(kvm_t *kd, const struct kinfo_proc2 *kp, int nchr)
{
	struct miniproc p;

	if (ISALIVE(kd))
		return (kvm_arg_sysctl(kd, kp->p_pid, nchr, 1));
	KP2TOMINI(kp, &p);
	return (kvm_doargv(kd, &p, nchr, ps_str_e));
}

/*
 * Read from user space.  The user context is given by p.
 */
static ssize_t
kvm_ureadm(kvm_t *kd, const struct miniproc *p, u_long uva, char *buf,
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

ssize_t
kvm_uread(kvm_t *kd, const struct proc *p, u_long uva, char *buf,
    size_t len)
{
	struct miniproc mp;

	PTOMINI(p, &mp);
	return (kvm_ureadm(kd, &mp, uva, buf, len));
}
