/*	$OpenBSD: vme.c,v 1.2 1999/01/11 05:11:28 millert Exp $	*/
/*	$NetBSD: vme.c,v 1.6 1996/11/20 18:57:02 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Gordon W. Ross.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/map.h>
#include <sys/proc.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/autoconf.h>
#include <machine/dvma.h>
#include <machine/kbus.h>
/* #include <machine/vme.h> */

static int  vme_match __P((struct device *, void *, void *));

static void vme_attach __P((struct device *, struct device *, void *));

struct cfattach vmel_ca = {
	sizeof(struct device), vme_match, vme_attach
};

struct cfdriver vmel_cd = {
	NULL, "vmel", DV_DULL
};

struct cfattach vmeh_ca = {
	sizeof(struct device), vme_match, vme_attach
};

struct cfdriver vmeh_cd = {
	NULL, "vmeh", DV_DULL
};

struct cfattach vmes_ca = {
	sizeof(struct device), vme_match, vme_attach
};

struct cfdriver vmes_cd = {
	NULL, "vmes", DV_DULL
};

struct intrhand **vmeints;

static int
vme_match(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct confargs *ca = aux;

	if (ca->ca_bustype != BUS_VME32
	    && ca->ca_bustype != BUS_VME24
	    && ca->ca_bustype != BUS_VME16)
		return 0;
	return 1;
}

static char *dvma_map_base;
static unsigned short *vme_iack;

static void
vme_attach(parent, self, args)
	struct device *parent;
	struct device *self;
	void *args;
{
	printf("\n");

	/* Map the vme address map.  */
	if (!dvma_map_base)
	  {
	    dvma_map_base = bus_mapin (BUS_KBUS, VME_MAP_BASE, DVMA_SIZE);
	    if (!dvma_map_base)
	      panic ("Cannot map vme address map");
	  }

	/* Be sure the vme_iack was set. */
	if (!vme_iack)
	  {
	    vme_iack = (unsigned short *)
	      bus_mapin (BUS_KBUS, VME_IACK_BASE, VME_IACK_SIZE);
	    if (!vme_iack)
	      panic ("Can't map vme iack page");
	  }

	if (vmeints == NULL) {
		vmeints = (struct intrhand **)malloc(256 *
		    sizeof(struct intrhand *), M_TEMP, M_NOWAIT);
		bzero(vmeints, 256 * sizeof(struct intrhand *));
	}

	/* We know ca_bustype == BUS_VMExx */
	(void) config_search(bus_scan, self, args);
}

/*
 * Wrapper for dvma_mapin() in kernel space,
 * so drivers need not include VM goo to get at kernel_map.
 */
caddr_t
kdvma_mapin(va, len, canwait)
	caddr_t	va;
	int	len, canwait;
{
	return (caddr_t) dvma_mapin(kernel_map, (vm_offset_t)va, len, canwait);
}

caddr_t
dvma_malloc(len, kaddr, flags)
	size_t	len;
	void	*kaddr;
	int	flags;
{
	vm_offset_t	kva;
	vm_offset_t	dva;

	len = round_page(len);
	kva = (vm_offset_t)malloc(len, M_DEVBUF, flags);
	if (kva == NULL)
		return (NULL);

	*(vm_offset_t *)kaddr = kva;
	dva = dvma_mapin(kernel_map, kva, len, (flags & M_NOWAIT) ? 0 : 1);
	if (dva == NULL) {
		free((void *)kva, M_DEVBUF);
		return (NULL);
	}
	return (caddr_t)dva;
}

void
dvma_free(dva, len, kaddr)
	caddr_t	dva;
	size_t	len;
	void	*kaddr;
{
	vm_offset_t	kva = *(vm_offset_t *)kaddr;

	dvma_mapout((vm_offset_t)dva, kva, round_page(len));
	free((void *)kva, M_DEVBUF);
}

/*
 * Map a range [va, va+len] of wired virtual addresses in the given map
 * to a kernel address in DVMA space.
 * Return a DVMA address, ie suitable for VME devices.
 */
vm_offset_t
dvma_mapin(map, va, len, canwait)
	struct vm_map	*map;
	vm_offset_t	va;
	int		len, canwait;
{
	vm_offset_t	kva, tva;
	register int npf, s;
	register vm_offset_t pa;
	long off, pn;

	off = (int)va & PGOFSET;
	va -= off;
	len = round_page(len + off);
	npf = btoc(len);

	s = splimp();
	for (;;) {

		pn = rmalloc(dvmamap, npf);

		if (pn != 0)
			break;
		if (canwait) {
			(void)tsleep(dvmamap, PRIBIO+1, "physio", 0);
			continue;
		}
		splx(s);
		return NULL;
	}
	splx(s);

	kva = tva = rctov (pn);

	while (npf--) {
		pa = pmap_extract(vm_map_pmap(map), va);
		if (pa == 0)
			panic("dvma_mapin: null page frame");
		pa = trunc_page(pa);

		*(u_int *)(dvma_map_base + tva) = pa | VME_MAP_V;

		tva += PAGE_SIZE;
		va += PAGE_SIZE;
	}
	return kva + off;
}

/*
 * Remove double map of `va' in DVMA space at `kva'.
 */
void
dvma_mapout(kva, va, len)
	vm_offset_t	kva, va;
	int		len;
{
	register int s, off;
	vm_offset_t tva;

	off = (int)kva & PGOFSET;
	kva -= off;
	len = round_page(len + off);

	for (tva = kva; tva < kva + len; tva += VME_PAGESIZE)
	  *(u_int *)(dvma_map_base + tva) = VME_MAP_NV;

	s = splimp();
	rmfree(dvmamap, btoc(len), vtorc(kva));
	wakeup(dvmamap);
	splx(s);
}

static int vmeintr __P((void *arg));

static int
vmeintr(arg)
	void *arg;
{
	int level = (int)arg, vec;
	struct intrhand *ih;
	int i = 0;

	vec = vme_iack[level] & 0xff;
#if 0
	printf ("vme intr: level = %d, vec = 0x%x\n", level, vec);
#endif

	for (ih = vmeints[vec]; ih; ih = ih->ih_next)
		if (ih->ih_fun)
			i += (ih->ih_fun)(ih->ih_arg);
	if (!i)
	  printf ("Stray vme int, level %d, vec 0x%02x\n", level, vec);

	return i;
}

void
vmeintr_establish(vec, level, ih)
	int vec, level;
	struct intrhand *ih;
{
	struct intrhand *ihs;

	if (vec == -1)
		panic("vmeintr_establish: uninitialized vec");

	if (vmeints[vec] == NULL)
		vmeints[vec] = ih;
	else {
		for (ihs = vmeints[vec]; ihs->ih_next; ihs = ihs->ih_next)
			;
		ihs->ih_next = ih;
	}

	/* Be sure the vme_iack was set. */
	if (!vme_iack)
	  panic ("vme_iack was not set");

	/* ensure the interrupt subsystem will call us at this level */
	for (ihs = intrhand[level]; ihs; ihs = ihs->ih_next)
		if (ihs->ih_fun == vmeintr)
			return;

	ihs = (struct intrhand *)malloc(sizeof(struct intrhand),
	    M_TEMP, M_NOWAIT);
	if (ihs == NULL)
		panic("vme_addirq");
	bzero(ihs, sizeof *ihs);
	ihs->ih_fun = vmeintr;
	ihs->ih_arg = (void *)level;
	intr_establish(VME_IPL_TO_INTR (level), IH_CAN_DELAY, ihs);
}

