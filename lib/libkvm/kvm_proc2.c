/*	$OpenBSD: kvm_proc2.c,v 1.2 2010/06/29 16:39:23 guenther Exp $	*/
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
#include <stddef.h>
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
 * Read proc's from memory file into buffer bp, which has space to hold
 * at most maxcnt procs.
 */
static int
kvm_proclist(kvm_t *kd, int op, int arg, struct proc *p,
    char *bp, int maxcnt, size_t esize)
{
	struct kinfo_proc2 kp;
	struct session sess;
	struct pcred pcred;
	struct ucred ucred;
	struct proc proc, proc2;
	struct process process;
	struct pgrp pgrp;
	struct tty tty;
	struct vmspace vm, *vmp;
	struct plimit limits, *limp;
	struct pstats pstats, *ps;
	pid_t parent_pid, leader_pid;
	int cnt = 0;

	for (; cnt < maxcnt && p != NULL; p = LIST_NEXT(&proc, p_list)) {
		if (KREAD(kd, (u_long)p, &proc)) {
			_kvm_err(kd, kd->program, "can't read proc at %x", p);
			return (-1);
		}
		if (KREAD(kd, (u_long)proc.p_p, &process)) {
			_kvm_err(kd, kd->program, "can't read process at %x",
			    proc.p_p);
			return (-1);
		}
		if (KREAD(kd, (u_long)process.ps_cred, &pcred)) {
			_kvm_err(kd, kd->program, "can't read pcred at %x",
			    process.ps_cred);
			return (-1);
		}
		if (KREAD(kd, (u_long)pcred.pc_ucred, &ucred)) {
			_kvm_err(kd, kd->program, "can't read ucred at %x",
			    pcred.pc_ucred);
			return (-1);
		}
		if (KREAD(kd, (u_long)proc.p_pgrp, &pgrp)) {
			_kvm_err(kd, kd->program, "can't read pgrp at %x",
			    proc.p_pgrp);
			return (-1);
		}
		if (KREAD(kd, (u_long)pgrp.pg_session, &sess)) {
			_kvm_err(kd, kd->program, "can't read session at %x",
			    pgrp.pg_session);
			return (-1);
		}
		if ((proc.p_flag & P_CONTROLT) && sess.s_ttyp != NULL &&
		    KREAD(kd, (u_long)sess.s_ttyp, &tty)) {
			_kvm_err(kd, kd->program,
			    "can't read tty at %x", sess.s_ttyp);
			return (-1);
		}
		if (proc.p_pptr) {
			if (KREAD(kd, (u_long)proc.p_pptr, &proc2)) {
				_kvm_err(kd, kd->program,
				    "can't read proc at %x", proc.p_pptr);
				return (-1);
			}
			parent_pid = proc2.p_pid;
		}
		else
			parent_pid = 0;
		if (sess.s_leader) {
			if (KREAD(kd, (u_long)sess.s_leader, &proc2)) {
				_kvm_err(kd, kd->program,
				    "can't read proc at %x", sess.s_leader);
				return (-1);
			}
			leader_pid = proc2.p_pid;
		}
		else
			leader_pid = 0;

		switch (op) {
		case KERN_PROC_PID:
			if (proc.p_pid != (pid_t)arg)
				continue;
			break;

		case KERN_PROC_PGRP:
			if (pgrp.pg_id != (pid_t)arg)
				continue;
			break;

		case KERN_PROC_SESSION:
			if (sess.s_leader == NULL ||
			    leader_pid == (pid_t)arg)
				continue;
			break;

		case KERN_PROC_TTY:
			if ((proc.p_flag & P_CONTROLT) == 0 ||
			    sess.s_ttyp == NULL ||
			    tty.t_dev != (dev_t)arg)
				continue;
			break;

		case KERN_PROC_UID:
			if (ucred.cr_uid != (uid_t)arg)
				continue;
			break;

		case KERN_PROC_RUID:
			if (pcred.p_ruid != (uid_t)arg)
				continue;
			break;

		case KERN_PROC_ALL:
			if (proc.p_flag & P_SYSTEM)
				continue;
			break;

		case KERN_PROC_KTHREAD:
			/* no filtering */
			break;

		default:
			_kvm_err(kd, kd->program, "invalid filter");
			return (-1);
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

		/* set up stuff that might not always be there */
		vmp = &vm;
		if (proc.p_stat == SIDL ||
		    P_ZOMBIE(&proc) ||
		    KREAD(kd, (u_long)proc.p_vmspace, &vm))
			vmp = NULL;

		limp = &limits;
		if (!process.ps_limit ||
		    KREAD(kd, (u_long)process.ps_limit, &limits))
			limp = NULL;

		ps = &pstats;
		if (P_ZOMBIE(&proc) ||
		    KREAD(kd, (u_long)proc.p_stats, &pstats))
			ps = NULL;

#define do_copy_str(_d, _s, _l)	kvm_read(kd, (u_long)(_s), (_d), (_l)-1)
		FILL_KPROC2(&kp, do_copy_str, &proc, &process, &pcred, &ucred,
		    &pgrp, p, &sess, vmp, limp, ps);
#undef do_copy_str

		/* stuff that's too painful to generalize into the macros */
		kp.p_ppid = parent_pid;
		kp.p_sid = leader_pid;
		if ((proc.p_flag & P_CONTROLT) && sess.s_ttyp != NULL) {
			kp.p_tdev = tty.t_dev;
			if (tty.t_pgrp != NULL &&
			    tty.t_pgrp != proc.p_pgrp &&
			    KREAD(kd, (u_long)tty.t_pgrp, &pgrp)) {
				_kvm_err(kd, kd->program,
				    "can't read tpgrp at &x", tty.t_pgrp);
				return (-1);
			}
			kp.p_tpgid = tty.t_pgrp ? pgrp.pg_id : -1;
			kp.p_tsess = PTRTOINT64(tty.t_session);
		} else {
			kp.p_tpgid = -1;
			kp.p_tdev = NODEV;
		}

		memcpy(bp, &kp, esize);
		bp += esize;
		++cnt;
	}
	return (cnt);
}

struct kinfo_proc2 *
kvm_getproc2(kvm_t *kd, int op, int arg, size_t esize, int *cnt)
{
	int mib[6], st, nprocs;
	size_t size;

	if ((ssize_t)esize < 0)
		return (NULL);

	if (kd->procbase2 != NULL) {
		free(kd->procbase2);
		/*
		 * Clear this pointer in case this call fails.  Otherwise,
		 * kvm_close() will free it again.
		 */
		kd->procbase2 = 0;
	}

	if (ISALIVE(kd)) {
		size = 0;
		mib[0] = CTL_KERN;
		mib[1] = KERN_PROC2;
		mib[2] = op;
		mib[3] = arg;
		mib[4] = esize;
		mib[5] = 0;
		st = sysctl(mib, 6, NULL, &size, NULL, 0);
		if (st == -1) {
			_kvm_syserr(kd, kd->program, "kvm_getproc2");
			return (NULL);
		}

		mib[5] = size / esize;
		kd->procbase2 = _kvm_malloc(kd, size);
		if (kd->procbase2 == 0)
			return (NULL);
		st = sysctl(mib, 6, kd->procbase2, &size, NULL, 0);
		if (st == -1) {
			_kvm_syserr(kd, kd->program, "kvm_getproc2");
			return (NULL);
		}
		nprocs = size / esize;
	} else {
		struct nlist nl[4];
		int i, maxprocs;
		struct proc *p;
		char *bp;

		if (esize > sizeof(struct kinfo_proc2)) {
			_kvm_syserr(kd, kd->program,
			    "kvm_getproc2: unknown fields requested: libkvm out of date?");
			return (NULL);
		}

		memset(nl, 0, sizeof(nl));
		nl[0].n_name = "_nprocs";
		nl[1].n_name = "_allproc";
		nl[2].n_name = "_zombproc";
		nl[3].n_name = NULL;

		if (kvm_nlist(kd, nl) != 0) {
			for (i = 0; nl[i].n_type != 0; ++i)
				;
			_kvm_err(kd, kd->program,
			    "%s: no such symbol", nl[i].n_name);
			return (NULL);
		}
		if (KREAD(kd, nl[0].n_value, &maxprocs)) {
			_kvm_err(kd, kd->program, "can't read nprocs");
			return (NULL);
		}

		kd->procbase2 = _kvm_malloc(kd, maxprocs * esize);
		if (kd->procbase2 == 0)
			return (NULL);
		bp = (char *)kd->procbase2;

		/* allproc */
		if (KREAD(kd, nl[1].n_value, &p)) {
			_kvm_err(kd, kd->program, "cannot read allproc");
			return (NULL);
		}
		nprocs = kvm_proclist(kd, op, arg, p, bp, maxprocs, esize);
		if (nprocs < 0)
			return (NULL);

		/* zombproc */
		if (KREAD(kd, nl[2].n_value, &p)) {
			_kvm_err(kd, kd->program, "cannot read zombproc");
			return (NULL);
		}
		i = kvm_proclist(kd, op, arg, p, bp + (esize * nprocs),
		    maxprocs - nprocs, esize);
		if (i > 0)
			nprocs += i;
	}
	*cnt = nprocs;
	return (kd->procbase2);
}

