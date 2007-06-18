/*	$OpenBSD: procfs_linux.c,v 1.8 2007/06/18 08:30:07 jasper Exp $	*/
/*      $NetBSD: procfs_linux.c,v 1.2.4.1 2001/03/30 21:48:11 he Exp $      */

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/vnode.h>

#include <miscfs/procfs/procfs.h>

#include <uvm/uvm_extern.h>

#define PGTOB(p)	((unsigned long)(p) << PAGE_SHIFT)
#define PGTOKB(p)	((unsigned long)(p) << (PAGE_SHIFT - 10))

/*
 * Linux compatible /proc/meminfo. Only active when the -o linux
 * mountflag is used.
 */
int
procfs_domeminfo(struct proc *curp, struct proc *p, struct pfsnode *pfs,
		 struct uio *uio)
{
	char buf[512], *cp;
	int len, error;

	len = snprintf(buf, sizeof buf,
		"        total:    used:    free:  shared: buffers: cached:\n"
		"Mem:  %8lu %8lu %8lu %8lu %8lu %8lu\n"
		"Swap: %8lu %8lu %8lu\n"
		"MemTotal:  %8lu kB\n"
		"MemFree:   %8lu kB\n"
		"MemShared: %8lu kB\n"
		"Buffers:   %8lu kB\n"
		"Cached:    %8lu kB\n"
		"SwapTotal: %8lu kB\n" 
		"SwapFree:  %8lu kB\n",
		PGTOB(uvmexp.npages),
		PGTOB(uvmexp.npages - uvmexp.free),
		PGTOB(uvmexp.free),
		0L,
		0L,
		0L,
		PGTOB(uvmexp.swpages),
		PGTOB(uvmexp.swpginuse),
		PGTOB(uvmexp.swpages - uvmexp.swpginuse),
		PGTOKB(uvmexp.npages),
		PGTOKB(uvmexp.free),
		0L,
		0L,
		0L,
		PGTOKB(uvmexp.swpages),
		PGTOKB(uvmexp.swpages - uvmexp.swpginuse));

	if (len <= 0 || len >= sizeof buf ||
	    len < uio->uio_offset || uio->uio_resid == 0)
		return EINVAL;

	len -= uio->uio_offset;
	cp = buf + uio->uio_offset;
	len = imin(len, uio->uio_resid);
	error = uiomove(cp, len, uio);
	return error;
}

int
procfs_docpuinfo(struct proc *curp, struct proc *p, struct pfsnode *pfs,
		 struct uio *uio)
{
	char buf[512], *cp;
	int len, error;

	len = sizeof buf;
	if (procfs_getcpuinfstr(buf, &len) < 0)
		return EIO;

	if (len == 0 || uio->uio_offset > sizeof(buf))
		return 0;

	len -= uio->uio_offset;
	cp = buf + uio->uio_offset;
	len = imin(len, uio->uio_resid);
	if (len <= 0)
		error = 0;
	else
		error = uiomove(cp, len, uio);
	return error;
}

#ifndef __i386__
int
procfs_getcpuinfstr(char *buf, int *len)
{
	*len = 0;

	return 0;
}
#endif
