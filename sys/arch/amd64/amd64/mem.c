/*	$OpenBSD: mem.c,v 1.5 2005/10/26 18:46:06 martin Exp $ */
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
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/memrange.h>
#include <sys/proc.h>
#include <sys/fcntl.h>

#include <machine/cpu.h>
#include <machine/conf.h>

#include <uvm/uvm_extern.h>

caddr_t zeropage;
extern int start, end, etext;

/* open counter for aperture */
#ifdef APERTURE
static int ap_open_count = 0;
extern int allowaperture;

#define VGA_START 0xA0000
#define BIOS_END  0xFFFFF
#endif

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
	case 12:
		break;
#ifdef APERTURE
	case 4:
	        if (suser(p, 0) != 0 || !allowaperture)
			return (EPERM);

		/* authorize only one simultaneous open() */
		if (ap_open_count > 0)
			return(EPERM);
		ap_open_count++;
		break;
#endif
	default:
		return (ENXIO);
	}
	return (0);
}

/*ARGSUSED*/
int
mmclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
#ifdef APERTURE
	if (minor(dev) == 4)
		ap_open_count--;
#endif
	return (0);
}

/*ARGSUSED*/
int
mmrw(dev_t dev, struct uio *uio, int flags)
{
	extern vaddr_t kern_end;
	vaddr_t v;
	int c;
	struct iovec *iov;
	int error = 0;

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
			v = PMAP_DIRECT_MAP(uio->uio_offset);
			error = uiomove((caddr_t)v, uio->uio_resid, uio);
			continue;

/* minor device 1 is kernel memory */
		case 1:
			v = uio->uio_offset;
			c = min(iov->iov_len, MAXPHYS);
			if (v >= (vaddr_t)&start && v < kern_end) {
                                if (v < (vaddr_t)&etext &&
                                    uio->uio_rw == UIO_WRITE)
                                        return EFAULT;
                        } else if ((!uvm_kernacc((caddr_t)v, c,
			    uio->uio_rw == UIO_READ ? B_READ : B_WRITE)) &&
			    (v < PMAP_DIRECT_BASE && v > PMAP_DIRECT_END))
				return (EFAULT);
			error = uiomove((caddr_t)v, c, uio);
			continue;

/* minor device 2 is EOF/RATHOLE */
		case 2:
			if (uio->uio_rw == UIO_WRITE)
				uio->uio_resid = 0;
			return (0);

/* minor device 12 (/dev/zero) is source of nulls on read, rathole on write */
		case 12:
			if (uio->uio_rw == UIO_WRITE) {
				c = iov->iov_len;
				break;
			}
			if (zeropage == NULL) {
				zeropage = (caddr_t)
				    malloc(PAGE_SIZE, M_TEMP, M_WAITOK);
				bzero(zeropage, PAGE_SIZE);
			}
			c = min(iov->iov_len, PAGE_SIZE);
			error = uiomove(zeropage, c, uio);
			continue;

		default:
			return (ENXIO);
		}
		iov->iov_base += c;
		iov->iov_len -= c;
		uio->uio_offset += c;
		uio->uio_resid -= c;
	}

	return (error);
}

paddr_t
mmmmap(dev, off, prot)
	dev_t dev;
	off_t off;
	int prot;
{
	struct proc *p = curproc;	/* XXX */

	switch (minor(dev)) {
/* minor device 0 is physical memory */
	case 0:
		if ((paddr_t)off > (paddr_t)ctob(physmem) && suser(p, 0) != 0)
			return -1;
		return atop(off);

#ifdef APERTURE
/* minor device 4 is aperture driver */
	case 4:
		switch (allowaperture) {
		case 1:
			/* Allow mapping of the VGA framebuffer & BIOS only */
			if ((off >= VGA_START && off <= BIOS_END) ||
			    (unsigned)off > (unsigned)ctob(physmem))
				return atop(off);
			else
				return -1;
		case 2:
			/* Allow mapping of the whole 1st megabyte 
			   for x86emu */
			if (off <= BIOS_END || 
			    (unsigned)off > (unsigned)ctob(physmem))
				return atop(off);
			else 
				return -1;
		default:
			return -1;
		}
			
#endif
	default:
		return -1;
	}
}

int
mmioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	return (ENODEV);
}

