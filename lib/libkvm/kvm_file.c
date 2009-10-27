/*	$OpenBSD: kvm_file.c,v 1.16 2009/10/27 23:59:28 deraadt Exp $ */
/*	$NetBSD: kvm_file.c,v 1.5 1996/03/18 22:33:18 thorpej Exp $	*/

/*-
 * Copyright (c) 1989, 1992, 1993
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
 * File list interface for kvm.  pstat, fstat and netstat are
 * users of this code, so we've factored it out into a separate module.
 * Thus, we keep this grunge out of the other kvm applications (i.e.,
 * most other applications are interested only in open/close/read/nlist).
 */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/exec.h>
#define _KERNEL
#include <sys/file.h>
#undef _KERNEL
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <nlist.h>
#include <kvm.h>

#include <sys/sysctl.h>

#include <limits.h>
#include <db.h>
#include <paths.h>

#include "kvm_private.h"

static int kvm_deadfiles(kvm_t *kd, int op, int arg, long filehead_o,
    int nfiles);

/*
 * Get file structures.
 */
static int
kvm_deadfiles(kvm_t *kd, int op, int arg, long filehead_o, int kvm_nfiles)
{
	int buflen = kd->arglen, n = 0;
	char *where = kd->argspc;
	struct file *fp;
	struct filelist kvm_filehead;

	/*
	 * first copyout filehead
	 */
	if (buflen > sizeof (kvm_filehead)) {
		if (KREAD(kd, filehead_o, &kvm_filehead)) {
			_kvm_err(kd, kd->program, "can't read filehead");
			return (0);
		}
		buflen -= sizeof(kvm_filehead);
		where += sizeof(kvm_filehead);
		*(struct filelist *)kd->argspc = kvm_filehead;
	}
	/*
	 * followed by an array of file structures
	 */
	LIST_FOREACH(fp, &kvm_filehead, f_list) {
		if (buflen > sizeof (struct file)) {
			if (KREAD(kd, (long)fp, ((struct file *)where))) {
				_kvm_err(kd, kd->program, "can't read kfp");
				return (0);
			}
			buflen -= sizeof (struct file);
			fp = (struct file *)where;
			where += sizeof (struct file);
			n++;
		}
	}
	if (n != kvm_nfiles) {
		_kvm_err(kd, kd->program, "inconsistent nfiles");
		return (0);
	}
	return (kvm_nfiles);
}

char *
kvm_getfiles(kvm_t *kd, int op, int arg, int *cnt)
{
	struct filelist kvm_filehead;
	struct file *fp, *fplim;
	int mib[2], st, kvm_nfiles;
	size_t size;

	if (ISALIVE(kd)) {
		size = 0;
		mib[0] = CTL_KERN;
		mib[1] = KERN_FILE;
		st = sysctl(mib, 2, NULL, &size, NULL, 0);
		if (st == -1) {
			_kvm_syserr(kd, kd->program, "kvm_getfiles");
			return (0);
		}
		if (kd->argspc == 0)
			kd->argspc = (char *)_kvm_malloc(kd, size);
		else if (kd->arglen < size)
			kd->argspc = (char *)_kvm_realloc(kd, kd->argspc, size);
		if (kd->argspc == 0)
			return (0);
		kd->arglen = size;
		st = sysctl(mib, 2, kd->argspc, &size, NULL, 0);
		if (st == -1 || size < sizeof(kvm_filehead)) {
			_kvm_syserr(kd, kd->program, "kvm_getfiles");
			return (0);
		}
		kvm_filehead = *(struct filelist *)kd->argspc;
		fp = (struct file *)(kd->argspc + sizeof(kvm_filehead));
		fplim = (struct file *)(kd->argspc + size);
		for (kvm_nfiles = 0; LIST_FIRST(&kvm_filehead) && (fp < fplim);
		    kvm_nfiles++, fp++)
			LIST_FIRST(&kvm_filehead) = LIST_NEXT(fp, f_list);
	} else {
		struct nlist nl[3], *p;

		nl[0].n_name = "_filehead";
		nl[1].n_name = "_nfiles";
		nl[2].n_name = 0;

		if (kvm_nlist(kd, nl) != 0) {
			for (p = nl; p->n_type != 0; ++p)
				;
			_kvm_err(kd, kd->program,
				 "%s: no such symbol", p->n_name);
			return (0);
		}
		if (KREAD(kd, nl[0].n_value, &kvm_nfiles)) {
			_kvm_err(kd, kd->program, "can't read nfiles");
			return (0);
		}
		size = sizeof(kvm_filehead) + (kvm_nfiles + 10) * sizeof(struct file);
		if (kd->argspc == 0)
			kd->argspc = (char *)_kvm_malloc(kd, size);
		else if (kd->arglen < size)
			kd->argspc = (char *)_kvm_realloc(kd, kd->argspc, size);
		if (kd->argspc == 0)
			return (0);
		kd->arglen = size;
		kvm_nfiles = kvm_deadfiles(kd, op, arg, nl[1].n_value, kvm_nfiles);
		if (kvm_nfiles == 0)
			return (0);
	}
	*cnt = kvm_nfiles;
	return (kd->argspc);
}
