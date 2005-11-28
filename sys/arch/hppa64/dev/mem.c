/*	$OpenBSD: mem.c,v 1.3 2005/11/28 20:13:08 martin Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 1991,1992,1994, The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * Subject to your agreements with CMU,
 * permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *	Utah $Hdr: mem.c 1.9 94/12/16$
 */
/*
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/file.h>

#include <uvm/uvm.h>

#include <machine/conf.h>
#include <machine/bus.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>
#include <machine/pmap.h>

#include <arch/hppa/dev/cpudevs.h>

struct mem_softc {
	struct device sc_dev;
};

int	memmatch(struct device *, void *, void *);
void	memattach(struct device *, struct device *, void *);

struct cfattach mem_ca = {
	sizeof(struct mem_softc), memmatch, memattach
};

struct cfdriver mem_cd = {
	NULL, "mem", DV_DULL
};

caddr_t zeropage;

int
memmatch(parent, cfdata, aux)
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct confargs *ca = aux;

	if ((ca->ca_name && !strcmp(ca->ca_name, "memory")) ||
	    (ca->ca_type.iodc_type == HPPA_TYPE_MEMORY &&
	     ca->ca_type.iodc_sv_model == HPPA_MEMORY_PDEP))
		return 1;

	return 0;
}

void
memattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	struct pdc_iodc_minit pdc_minit PDC_ALIGNMENT;
	/* struct mem_softc *sc = (struct mem_softc *)self; */
	struct confargs *ca = aux;
	int err;

/* TODO scan the memory regions list */

	if ((err = pdc_call((iodcio_t)pdc, 0, PDC_IODC, PDC_IODC_NINIT,
	    &pdc_minit, ca->ca_hpa, PAGE0->imm_spa_size)) < 0)
		pdc_minit.max_spa = PAGE0->imm_max_mem;

	printf(": size %d", pdc_minit.max_spa / (1024*1024));
	if (pdc_minit.max_spa % (1024*1024))
		printf(".%d", pdc_minit.max_spa % (1024*1024));
	printf("MB\n");
}

int
mmopen(dev, flag, ioflag, p)
	dev_t dev;
	int flag;
	int ioflag;
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

int
mmrw(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	struct iovec	*iov;
	vaddr_t	v, o;
	int error = 0;
	u_int	c;

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

		case 0:				/*  /dev/mem  */

			/* If the address isn't in RAM, bail. */
			v = uio->uio_offset;
			if (btoc(v) > physmem) {
				error = EFAULT;
				/* this will break us out of the loop */
				continue;
			}
			c = ctob(physmem) - v;
			c = min(c, uio->uio_resid);
			error = uiomove((caddr_t)v, c, uio);
			break;

		case 1:				/*  /dev/kmem  */
			v = uio->uio_offset;
			o = v & PAGE_MASK;
			c = min(uio->uio_resid, (int)(PAGE_SIZE - o));
			if (btoc(v) > physmem && !uvm_kernacc((caddr_t)v,
			    c, (uio->uio_rw == UIO_READ) ? B_READ : B_WRITE)) {
				error = EFAULT;
				/* this will break us out of the loop */
				continue;
			}
			error = uiomove((caddr_t)v, c, uio);
			break;

		case 2:				/*  /dev/null  */
			if (uio->uio_rw == UIO_WRITE)
				uio->uio_resid = 0;
			return (0);

		case 12:			/*  /dev/zero  */
			/* Write to /dev/zero is ignored. */
			if (uio->uio_rw == UIO_WRITE) {
				uio->uio_resid = 0;
				return (0);
			}
			/*
			 * On the first call, allocate and zero a page
			 * of memory for use with /dev/zero.
			 */
			if (zeropage == NULL) {
				zeropage = malloc(PAGE_SIZE, M_TEMP, M_WAITOK);
				bzero(zeropage, PAGE_SIZE);
			}
			c = min(iov->iov_len, PAGE_SIZE);
			error = uiomove(zeropage, c, uio);
			break;

		default:
			return (ENXIO);
		}
	}

	return (error);
}

paddr_t
mmmmap(dev, off, prot)
	dev_t dev;
	off_t off;
	int prot;
{
	if (minor(dev) != 0)
		return (-1);

	/*
	 * Allow access only in RAM.
	 */
#if 0
	if (off < ctob(firstusablepage) ||
	    off >= ctob(lastusablepage + 1))
		return (-1);
#endif
	return (atop(off));
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
