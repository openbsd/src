/*	$OpenBSD: mem.c,v 1.12 2000/03/04 16:42:15 art Exp $	*/
/*	$NetBSD: mem.c,v 1.13 1996/03/30 21:12:16 christos Exp $ */

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	@(#)mem.c	8.3 (Berkeley) 1/12/94
 */

/*
 * Memory special file
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/conf.h>

#include <sparc/sparc/vaddrs.h>
#include <machine/eeprom.h>
#include <machine/conf.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

extern vaddr_t prom_vstart;
extern vaddr_t prom_vend;
caddr_t zeropage;
vaddr_t mem_page;

/*ARGSUSED*/
int
mmopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{

	switch (minor(dev)) {
		case 0:
		case 1:
		case 2:
		case 11:
		case 12:
			return (0);
		default:
			return (ENXIO);
	}
}

/*ARGSUSED*/
int
mmclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{

	return (0);
}

/*ARGSUSED*/
int
mmrw(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int o;
	paddr_t pa;
	vaddr_t va;
	int c;
	struct iovec *iov;
	int error = 0;
	static int physlock;

	if (minor(dev) == 0) {
		/* lock against other uses of shared mem_page */
		while (physlock > 0) {
			physlock++;
			error = tsleep((caddr_t)&physlock, PZERO | PCATCH,
			    "mmrw", 0);
			if (error)
				return (error);
		}
		physlock = 1;
#if defined(UVM)
		if (mem_page == 0)
			mem_page = uvm_km_valloc_wait(kernel_map, NBPG);
#else
		if (mem_page == 0)
			mem_page = kmem_alloc_pageable(kernel_map, NBPG);
#endif
		if (mem_page == 0)
			panic("mmrw: out of space in kernel_map");
	}
	while (uio->uio_resid > 0 && error == 0) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("mmrw");
			continue;
		}
		switch (minor(dev)) {

		/* minor device 0 is physical memory */
		case 0:
			pa = (paddr_t)uio->uio_offset;
			if (!pmap_pa_exists(pa)) {
				error = EFAULT;
				goto unlock;
			}
			pmap_enter(pmap_kernel(), mem_page,
			    trunc_page(pa), uio->uio_rw == UIO_READ ?
			    VM_PROT_READ : VM_PROT_WRITE, TRUE, 0);
			o = uio->uio_offset & PGOFSET;
			c = min(uio->uio_resid, (int)(NBPG - o));
			error = uiomove((caddr_t)mem_page + o, c, uio);
			pmap_remove(pmap_kernel(), mem_page, mem_page + NBPG);
			continue;

		/* minor device 1 is kernel memory */
		case 1:
			va = (vaddr_t)uio->uio_offset;
			if (va >= MSGBUF_VA && va < MSGBUF_VA+NBPG) {
				c = min(iov->iov_len, 4096);
			} else if (va >= prom_vstart && va < prom_vend &&
				   uio->uio_rw == UIO_READ) {
				/* Allow read-only access to the PROM */
				c = min(iov->iov_len, prom_vend - prom_vstart);
			} else {
				c = min(iov->iov_len, MAXPHYS);
#if defined(UVM)
				if (!uvm_kernacc((caddr_t)va, c,
				    uio->uio_rw == UIO_READ ? B_READ : B_WRITE))
					return (EFAULT);
#else
				if (!kernacc((caddr_t)va, c,
				    uio->uio_rw == UIO_READ ? B_READ : B_WRITE))
					return (EFAULT);
#endif
			}
			error = uiomove((caddr_t)va, c, uio);
			continue;

		/* minor device 2 is EOF/RATHOLE */
		case 2:
			if (uio->uio_rw == UIO_WRITE)
				uio->uio_resid = 0;
			return (0);

/* XXX should add sbus, etc */

#if defined(SUN4)
/* minor device 11 (/dev/eeprom) is the old-style (a'la Sun 3) EEPROM */
		case 11:
			if (cputyp == CPU_SUN4)
				error = eeprom_uio(uio);
			else
				error = ENXIO;

			continue;
#endif /* SUN4 */

/* minor device 12 (/dev/zero) is source of nulls on read, rathole on write */
		case 12:
			if (uio->uio_rw == UIO_WRITE) {
				uio->uio_resid = 0;
				return 0;
			}
			if (zeropage == NULL) {
				zeropage = (caddr_t)
				    malloc(CLBYTES, M_TEMP, M_WAITOK);
				bzero(zeropage, CLBYTES);
			}
			c = min(iov->iov_len, CLBYTES);
			error = uiomove(zeropage, c, uio);
			continue;

		default:
			return (ENXIO);
		}
	}
	if (minor(dev) == 0) {
unlock:
		if (physlock > 1)
			wakeup((caddr_t)&physlock);
		physlock = 0;
	}
	return (error);
}

int
mmmmap(dev, off, prot)
        dev_t dev;
        int off, prot;
{

	return (EOPNOTSUPP);
}

/*ARGSUSED*/
int
mmioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	return (EOPNOTSUPP);
}
