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

#include <machine/conf.h>

#include <vm/vm.h>

vm_offset_t prom_vstart = VM_MIN_PROM_ADDRESS;
vm_offset_t prom_vend = VM_MAX_PROM_ADDRESS;
caddr_t zeropage;

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
	register vm_offset_t o, v;
	register int c;
	register struct iovec *iov;
	int error = 0;
	static int physlock;
	extern caddr_t vmmap;

	if (minor(dev) == 0) {
		/* lock against other uses of shared vmmap */
		while (physlock > 0) {
			physlock++;
			error = tsleep((caddr_t)&physlock, PZERO | PCATCH,
			    "mmrw", 0);
			if (error)
				return (error);
		}
		physlock = 1;
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
			v = uio->uio_offset;
#if 0 /* FIXME */
			if (!pmap_pa_exists(v)) {
				error = EFAULT;
				goto unlock;
			}
#endif
			pmap_enter(pmap_kernel(), (vm_offset_t)vmmap,
			    trunc_page(v), uio->uio_rw == UIO_READ ?
			    VM_PROT_READ : VM_PROT_WRITE, TRUE, 0);
			o = uio->uio_offset & PGOFSET;
			c = min(uio->uio_resid, (int)(NBPG - o));
			error = uiomove((caddr_t)vmmap + o, c, uio);
			pmap_remove(pmap_kernel(), (vm_offset_t)vmmap,
			    (vm_offset_t)vmmap + NBPG);
			continue;

/* minor device 1 is kernel memory */
		case 1:
			v = uio->uio_offset;
/*			if (v >= MSGBUF_VA && v < MSGBUF_VA+NBPG) {
				c = min(iov->iov_len, 4096);
			} else */ if (v >= prom_vstart && v < prom_vend &&
				   uio->uio_rw == UIO_READ) {
				/* Allow read-only access to the PROM */
				c = min(iov->iov_len, prom_vend - prom_vstart);
			} else {
				c = min(iov->iov_len, MAXPHYS);
				if (!kernacc((caddr_t)v, c,
				    uio->uio_rw == UIO_READ ? B_READ : B_WRITE))
					return (EFAULT);
			}
			error = uiomove((caddr_t)v, c, uio);
			continue;

/* minor device 2 is EOF/RATHOLE */
		case 2:
			if (uio->uio_rw == UIO_WRITE)
				uio->uio_resid = 0;
			return (0);



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
				c = iov->iov_len;
				break;
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
		if (error)
			break;
		iov->iov_base += c;
		iov->iov_len -= c;
		uio->uio_offset += c;
		uio->uio_resid -= c;
	}
	if (minor(dev) == 0) {
/* unlock: */
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
