/*	$OpenBSD: mem.c,v 1.7 1998/08/31 17:42:42 millert Exp $	*/
/*	$NetBSD: mem.c,v 1.19 1995/08/08 21:09:01 gwr Exp $	*/

/*
 * Copyright (c) 1994, 1995 Gordon W. Ross
 * Copyright (c) 1993 Adam Glass 
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
 *	from: @(#)mem.c	8.3 (Berkeley) 1/12/94
 */

/*
 * Memory special file
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>

#include <machine/conf.h>
#include <machine/cpu.h>
#include <machine/eeprom.h>
#include <machine/machdep.h>
#include <machine/pte.h>
#include <machine/pmap.h>

extern int ledrw __P((struct uio *));

static caddr_t devzeropage;

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
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
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

	if (minor(dev) == 0) {
		if (vmmap == 0)
			return (EIO);
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
			/* allow reads only in RAM */
			if (v < 0 || v >= avail_end) {
				error = EFAULT;
				goto unlock;
			}
			/*
			 * If the offset (physical address) is outside the
			 * region of physical memory that is "managed" by
			 * the pmap system, then we are not allowed to
			 * call pmap_enter with that physical address.
			 * Everything from zero to avail_start is mapped
			 * linearly with physical zero at virtual KERNBASE,
			 * so redirect the access to /dev/kmem instead.
			 * This is a better alternative than hacking the
			 * pmap to deal with requests on unmanaged memory.
			 * Also note: unlock done at end of function.
			 */
			if (v < avail_start) {
				v += KERNBASE;
				goto use_kmem;
			}
			/* Temporarily map the memory at vmmap. */
			pmap_enter(pmap_kernel(), vmmap,
			    trunc_page(v), uio->uio_rw == UIO_READ ?
			    VM_PROT_READ : VM_PROT_WRITE, TRUE);
			o = uio->uio_offset & PGOFSET;
			c = min(uio->uio_resid, (int)(NBPG - o));
			error = uiomove((caddr_t)vmmap + o, c, uio);
			pmap_remove(pmap_kernel(), vmmap, vmmap + NBPG);
			continue;

/* minor device 1 is kernel memory */
		/* XXX - Allow access to the PROM? */
		case 1:
			v = uio->uio_offset;
		use_kmem:
			/*
			 * Watch out!  You might assume it is OK to copy
			 * up to MAXPHYS bytes here, but that is wrong.
			 * The next page might NOT be part of the range:
			 *   	(KERNBASE..(KERNBASE+avail_start))
			 * which is asked for here via the goto in the
			 * /dev/mem case above.  The consequence is that
			 * we copy one page at a time.  Big deal.
			 * Most requests are small anyway. -gwr
			 */
			o = v & PGOFSET;
			c = min(uio->uio_resid, (int)(NBPG - o));
			if (!kernacc((caddr_t)v, c,
			    uio->uio_rw == UIO_READ ? B_READ : B_WRITE))
			{
				error = EFAULT;
				goto unlock;
			}
			error = uiomove((caddr_t)v, c, uio);
			continue;

/* minor device 2 is EOF/RATHOLE */
		case 2:
			if (uio->uio_rw == UIO_WRITE)
				uio->uio_resid = 0;
			return (0);

/* minor device 11 (/dev/eeprom) accesses Non-Volatile RAM */
		case 11:
			error = eeprom_uio(uio);
			return (error);

/* minor device 12 (/dev/zero) is source of nulls on read, rathole on write */
		case 12:
			if (uio->uio_rw == UIO_WRITE) {
				c = iov->iov_len;
				break;
			}
			/*
			 * On the first call, allocate and zero a page
			 * of memory for use with /dev/zero.
			 */
			if (devzeropage == NULL) {
				devzeropage = (caddr_t)
				    malloc(CLBYTES, M_TEMP, M_WAITOK);
				bzero(devzeropage, CLBYTES);
			}
			c = min(iov->iov_len, CLBYTES);
			error = uiomove(devzeropage, c, uio);
			continue;

/* minor device 13 (/dev/leds) accesses the blinkenlights */
		case 13:
			error = ledrw(uio);
			return(error);

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

	/*
	 * Note the different location of this label, compared with
	 * other ports.  This is because the /dev/mem to /dev/kmem
	 * redirection above jumps here on error to do its unlock.
	 */
unlock:
	if (minor(dev) == 0) {
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
	register int v = off;

	/*
	 * Check address validity.
	 */
	if (v & PGOFSET)
		return (-1);

	switch (minor(dev)) {

	case 0:		/* dev/mem */
		/* Allow access only in "managed" RAM. */
		if (v < avail_start || v >= avail_end)
			break;
		return (v);

	case 5: 	/* dev/vme16d16 */
		if (v & 0xffff0000)
			break;
		v |= 0xff0000;
		/* fall through */
	case 6: 	/* dev/vme24d16 */
		if (v & 0xff000000)
			break;
		v |= 0xff000000;
		/* fall through */
	case 7: 	/* dev/vme32d16 */
		return (v | PMAP_VME16);

	case 8: 	/* dev/vme16d32 */
		if (v & 0xffff0000)
			break;
		v |= 0xff0000;
		/* fall through */
	case 9: 	/* dev/vme24d32 */
		if (v & 0xff000000)
			break;
		v |= 0xff000000;
		/* fall through */
	case 10:	/* dev/vme32d32 */
		return (v | PMAP_VME32);
	}

	return (-1);
}
