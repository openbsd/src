/*	$OpenBSD: mem.c,v 1.7 2003/01/09 22:27:11 miod Exp $	*/
/*	$NetBSD: mem.c,v 1.18 2001/04/24 04:31:12 thorpej Exp $ */

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

#include <machine/eeprom.h>
#include <machine/conf.h>
#include <machine/ctlreg.h>

#include <uvm/uvm_extern.h>

vaddr_t prom_vstart = 0xf000000;
vaddr_t prom_vend = 0xf0100000;
caddr_t zeropage;

/*ARGSUSED*/
int
mmopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{

	return (0);
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
	vaddr_t o, v;
	int c;
	struct iovec *iov;
	int error = 0;
	static int physlock;
	vm_prot_t prot;
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
		int n;
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("mmrw");
			continue;
		}

		/* Note how much is still to go */
		n = uio->uio_resid;

		switch (minor(dev)) {

		/* minor device 0 is physical memory */
		case 0:
#if 1
			v = uio->uio_offset;
			if (!pmap_pa_exists(v)) {
				error = EFAULT;
				goto unlock;
			}
			prot = uio->uio_rw == UIO_READ ? VM_PROT_READ :
			    VM_PROT_WRITE;
			pmap_enter(pmap_kernel(), (vaddr_t)vmmap,
			    trunc_page(v), prot, prot|PMAP_WIRED);
			pmap_update(pmap_kernel());
			o = uio->uio_offset & PGOFSET;
			c = min(uio->uio_resid, (int)(NBPG - o));
			error = uiomove((caddr_t)vmmap + o, c, uio);
			pmap_remove(pmap_kernel(), (vaddr_t)vmmap,
			    (vaddr_t)vmmap + NBPG);
			pmap_update(pmap_kernel());
			break;
#else
			/* On v9 we can just use the physical ASI and not bother w/mapin & mapout */
			v = uio->uio_offset;
			if (!pmap_pa_exists(v)) {
				error = EFAULT;
				goto unlock;
			}
			o = uio->uio_offset & PGOFSET;
			c = min(uio->uio_resid, (int)(NBPG - o));
			/* However, we do need to partially re-implement uiomove() */
			if (uio->uio_rw != UIO_READ && uio->uio_rw != UIO_WRITE)
				panic("mmrw: uio mode");
			if (uio->uio_segflg == UIO_USERSPACE && uio->uio_procp != curproc)
				panic("mmrw: uio proc");
			while (c > 0 && uio->uio_resid) {
				struct iovec *iov;
				u_int cnt;
				int d;

				iov = uio->uio_iov;
				cnt = iov->iov_len;
				if (cnt == 0) {
					uio->uio_iov++;
					uio->uio_iovcnt--;
					continue;
				}
				if (cnt > c)
					cnt = c;
				d = iov->iov_base;
				switch (uio->uio_segflg) {
					
				case UIO_USERSPACE:
					if (uio->uio_rw == UIO_READ)
						while (cnt--) {
							char tmp;

							tmp = lduba(v++, ASI_PHYS_CACHED);
							error = copyout(&tmp, d++, sizeof(tmp));
							if (error != 0)
								break;
						}
					else
						while (cnt--) {
							char tmp;

							error = copyin(d++, &tmp, sizeof(tmp));
							if (error != 0)
								break;
							stba(v++, ASI_PHYS_CACHED, tmp);
						}
					if (error)
						goto unlock;
					break;
					
				case UIO_SYSSPACE:
					if (uio->uio_rw == UIO_READ)
						while (cnt--)
							stba(d++, ASI_P, lduba(v++, ASI_PHYS_CACHED));
					else
						while (cnt--)
							stba(v++, ASI_PHYS_CACHED, lduba(d++, ASI_P));
					break;
				}
				iov->iov_base =  (caddr_t)iov->iov_base + cnt;
				iov->iov_len -= cnt;
				uio->uio_resid -= cnt;
				uio->uio_offset += cnt;
				c -= cnt;
			}
			/* Should not be necessary */
			blast_vcache();
			break;
#endif

		/* minor device 1 is kernel memory */
		case 1:
			v = uio->uio_offset;
			c = min(iov->iov_len, MAXPHYS);
			if (!uvm_kernacc((caddr_t)v, c,
			    uio->uio_rw == UIO_READ ? B_READ : B_WRITE))
				return (EFAULT);
			error = uiomove((caddr_t)v, c, uio);
			break;

		/* minor device 2 is EOF/RATHOLE */
		case 2:
			if (uio->uio_rw == UIO_WRITE)
				uio->uio_resid = 0;
			return (0);

/* XXX should add sbus, etc */

		/*
		 * minor device 12 (/dev/zero) is source of nulls on read,
		 * rathole on write.
		 */
		case 12:
			if (uio->uio_rw == UIO_WRITE) {
				uio->uio_resid = 0;
				return(0);
			}
			if (zeropage == NULL) {
				zeropage = (caddr_t)
				    malloc(NBPG, M_TEMP, M_WAITOK);
				bzero(zeropage, NBPG);
			}
			c = min(iov->iov_len, NBPG);
			error = uiomove(zeropage, c, uio);
			break;

		default:
			return (ENXIO);
		}

		/* If we didn't make any progress (i.e. EOF), we're done here */
		if (n == uio->uio_resid)
			break;
	}
	if (minor(dev) == 0) {
unlock:
		if (physlock > 1)
			wakeup((caddr_t)&physlock);
		physlock = 0;
	}
	return (error);
}

paddr_t
mmmmap(dev, off, prot)
	dev_t dev;
	off_t off;
	int prot;
{

	return (-1);
}

int
mmioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	return (EOPNOTSUPP);
}

