/*	$OpenBSD: autoconf.c,v 1.39 2007/06/01 19:25:10 deraadt Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 * 	The Regents of the University of California. All rights reserved.
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
 *
 * from: Utah $Hdr: autoconf.c 1.36 92/12/20$
 * 
 *	@(#)autoconf.c  8.2 (Berkeley) 1/12/94
 */

/*
 * Setup the system to run on the current machine.
 *
 * cpu_configure() is called at boot time.  Available
 * devices are determined (from possibilities mentioned in ioconf.c),
 * and the drivers are initialized.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/extent.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/dkstat.h>
#include <sys/conf.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/disklabel.h>

#include <machine/vmparam.h>
#include <machine/autoconf.h>
#include <machine/disklabel.h>
#include <machine/cpu.h>
#include <machine/pte.h>

#include <uvm/uvm_extern.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <dev/cons.h>

int	mainbus_print(void *, const char *);
int	mainbus_scan(struct device *, void *, void *);

extern void init_intrs(void);
extern void dumpconf(void);

/* boot device information */
paddr_t	bootaddr;
int	bootctrllun, bootdevlun, bootpart;
struct	device *bootdv;

/*
 * XXX some storage space must be allocated statically because of
 * early console init
 */
char	extiospace[EXTENT_FIXED_STORAGE_SIZE(8)];

struct	extent *extio;
extern	vaddr_t extiobase;

void mainbus_attach(struct device *, struct device *, void *);
int  mainbus_match(struct device *, void *, void *);

struct cfattach mainbus_ca = {
	sizeof(struct device), mainbus_match, mainbus_attach
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

int
mainbus_match(parent, cf, args)
	struct device *parent;
	void *cf;
	void *args;
{
	return (1);
}

int
mainbus_print(args, bus)
	void *args;
	const char *bus;
{
	struct confargs *ca = args;

	if (ca->ca_paddr != (paddr_t)-1)
		printf(" addr 0x%x", (u_int32_t)ca->ca_paddr);
	return (UNCONF);
}

int
mainbus_scan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	struct cfdata *cf = child;
	struct confargs oca;

	bzero(&oca, sizeof oca);
	oca.ca_paddr = cf->cf_loc[0];
	oca.ca_vaddr = (vaddr_t)-1;
	oca.ca_ipl = -1;
	oca.ca_bustype = BUS_MAIN;
	oca.ca_name = cf->cf_driver->cd_name;
	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0)
		return (0);
	config_attach(parent, cf, &oca, mainbus_print);
	return (1);
}

void
mainbus_attach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	printf("\n");

	/* XXX
	 * should have a please-attach-first list for mainbus,
	 * to ensure that the pcc/vme2/mcc chips are attached
	 * first.
	 */

	(void)config_search(mainbus_scan, self, args);
}

/*
 * Determine mass storage and memory configuration for a machine.
 */
void
cpu_configure()
{
	init_intrs();

	extio = extent_create("extio",
	    (u_long)extiobase, (u_long)extiobase + ctob(EIOMAPSIZE),
	    M_DEVBUF, extiospace, sizeof(extiospace), EX_NOWAIT);

	if (config_rootfound("mainbus", NULL) == NULL)
		panic("autoconfig failed, no root");

	printf("boot device: %s\n",
	    (bootdv) ? bootdv->dv_xname : "<unknown>");
	cold = 0;
}

void
diskconf(void)
{
	setroot(bootdv, bootpart, RB_USERREQ);
	dumpconf();
}

/*
 * Allocate/deallocate a cache-inhibited range of kernel virtual address
 * space mapping the indicated physical address range [pa - pa+size)
 */
vaddr_t
mapiodev(pa, size)
	paddr_t pa;
	int size;
{
	int error;
	paddr_t base;
	vaddr_t va, iova;

	if (size <= 0)
		return NULL;

	base = pa & PAGE_MASK;
	pa = trunc_page(pa);
	size = round_page(base + size);

	error = extent_alloc(extio, size, EX_NOALIGN, 0, EX_NOBOUNDARY,
	    EX_NOWAIT | EX_MALLOCOK, &iova);

	if (error != 0)
	        return NULL;

	va = iova;
	while (size != 0) {
		pmap_kenter_cache(va, pa, PG_RW | PG_CI);
		size -= PAGE_SIZE;
		va += PAGE_SIZE;
		pa += PAGE_SIZE;
	}
	pmap_update(pmap_kernel());
	return (iova + base);
}

void
unmapiodev(kva, size)
	vaddr_t kva;
	int size;
{
	int error;
	vaddr_t va;

#ifdef DEBUG
	if (kva < extiobase || kva + size >= extiobase + ctob(EIOMAPSIZE))
	        panic("unmapiodev: bad address");
#endif

	va = trunc_page(kva);
	size = round_page(kva + size) - va;
	pmap_kremove(va, size);
	pmap_update(pmap_kernel());

	error = extent_free(extio, va, size, EX_NOWAIT);
#ifdef DIAGNOSTIC
	if (error != 0)
		printf("unmapiodev: extent_free failed\n");
#endif
}

void
device_register(struct device *dev, void *aux)
{
	if (bootpart == -1) /* ignore flag from controller driver? */
		return;

	/*
	 * scsi: sd,cd
	 */
	if (strncmp("sd", dev->dv_xname, 2) == 0 ||
	    strncmp("cd", dev->dv_xname, 2) == 0) {
		struct scsi_attach_args *sa = aux;
		int target, lun;
#ifdef MVME147
		/*
		 * The 147 can only boot from the built-in scsi controller,
		 * and stores the scsi id as the controller number.
		 */
		if (cputyp == CPU_147) {
			target = bootctrllun;
			lun = 0;
		} else
#endif
		{
			target = bootdevlun >> 4;
			lun = bootdevlun & 0x0f;
		}
    		
		if (sa->sa_sc_link->target == target &&
		    sa->sa_sc_link->lun == lun) {
			bootdv = dev;
			return;
		}
	}

	/*
	 * ethernet: ie,le
	 */
	else if (strncmp("ie", dev->dv_xname, 2) == 0 ||
	    strncmp("le", dev->dv_xname, 2) == 0) {
		struct confargs *ca = aux;

		if (ca->ca_paddr == bootaddr) {
			bootdv = dev;
			return;
		}
	}
}

struct nam2blk nam2blk[] = {
	{ "sd",		4 },
	{ "st",		7 },
	{ "rd",		9 },
	{ NULL,		-1 }
};
