/* $OpenBSD: vga_pci.c,v 1.20 2005/11/19 02:18:00 pedro Exp $ */
/* $NetBSD: vga_pci.c,v 1.3 1998/06/08 06:55:58 thorpej Exp $ */

/*-
 * Copyright (c) 2000 Doug Rabson
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD: src/sys/pci/agp.c,v 1.12 2001/05/19 01:28:07 alfred Exp $
 */
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
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
/*
 * Copyright (c) 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include "vga.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/agpio.h>

#include <uvm/uvm.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/ic/vgareg.h>
#include <dev/ic/vgavar.h>
#include <dev/pci/vga_pcivar.h>

#ifdef PCIAGP
#include <sys/fcntl.h>

#include <dev/pci/agpvar.h>
#include <dev/pci/agpreg.h>
#endif

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>

int	vga_pci_match(struct device *, void *, void *);
void	vga_pci_attach(struct device *, struct device *, void *);
paddr_t	vga_pci_mmap(void* v, off_t off, int prot);

struct cfattach vga_pci_ca = {
	sizeof(struct vga_pci_softc), vga_pci_match, vga_pci_attach,
};

#ifdef PCIAGP
struct agp_memory *agp_find_memory(struct vga_pci_softc *sc, int id);
const struct agp_product *agp_lookup(struct pci_attach_args *pa);

struct pci_attach_args	agp_pchb_pa;
int	agp_pchb_pa_set = 0;
#endif

int
vga_pci_match(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa = aux;
	int potential;

	potential = 0;

	/*
	 * If it's prehistoric/vga or display/vga, we might match.
	 * For the console device, this is jut a sanity check.
	 */
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_PREHISTORIC &&
	    PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_PREHISTORIC_VGA)
		potential = 1;
	if (PCI_CLASS(pa->pa_class) == PCI_CLASS_DISPLAY &&
	     PCI_SUBCLASS(pa->pa_class) == PCI_SUBCLASS_DISPLAY_VGA)
		potential = 1;

	if (!potential)
		return (0);

	/* check whether it is disabled by firmware */
	if ((pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG)
	    & (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE))
	    != (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE))
		return (0);

	/* If it's the console, we have a winner! */
	if (vga_is_console(pa->pa_iot, WSDISPLAY_TYPE_PCIVGA))
		return (1);

	/*
	 * If we might match, make sure that the card actually looks OK.
	 */
#ifdef MD_DISPLAY_ISA_IOT
	if (!vga_common_probe(MD_DISPLAY_ISA_IOT, MD_DISPLAY_ISA_MEMT))
		return (0);
#else
	if (!vga_common_probe(pa->pa_iot, pa->pa_memt))
		return (0);
#endif

	return (1);
}

void
vga_pci_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
#ifndef MD_DISPLAY_ISA_IOT
	struct pci_attach_args *pa = aux;
#endif
#ifdef PCIAGP
	struct vga_pci_softc *sc = (struct vga_pci_softc *)self;
	const struct agp_product *ap;
	u_int memsize;
	int i, ret;
#endif

#ifdef PCIAGP
	ap = agp_lookup(pa);
	if (ap) {
		static const int agp_max[][2] = {
			{0,		0},
			{32,		4},
			{64,		28},
			{128,		96},
			{256,		204},
			{512,		440},
			{1024,		942},
			{2048,		1920},
			{4096,		3932}
		};
#define	agp_max_size	 (sizeof(agp_max)/sizeof(agp_max[0]))

		/*
		 * Work out an upper bound for agp memory allocation. This
		 * uses a heuristic table from the Linux driver.
		 */
		memsize = ptoa(physmem) >> 20;

		for (i = 0; i < agp_max_size && memsize > agp_max[i][0]; i++)
			;
		if (i == agp_max_size)
			i = agp_max_size - 1;
		sc->sc_maxmem = agp_max[i][1] << 20;

		/*
		 * The lock is used to prevent re-entry to
		 * agp_generic_bind_memory() since that function can sleep.
		 */

		lockinit(&sc->sc_lock, PZERO|PCATCH, "agplk", 0, 0);

		TAILQ_INIT(&sc->sc_memory);

		sc->sc_pcitag = pa->pa_tag;
		sc->sc_pc = pa->pa_pc;
		sc->sc_id = pa->pa_id;
		sc->sc_dmat = pa->pa_dmat;

		pci_get_capability(sc->sc_pc, sc->sc_pcitag, PCI_CAP_AGP,
		    &sc->sc_capoff, NULL);

		ret = (*ap->ap_attach)(sc, pa, &agp_pchb_pa);
		if (ret == 0)
			printf(": aperture at 0x%lx, size 0x%lx",
			    (u_long)sc->sc_apaddr,
			    (u_long)AGP_GET_APERTURE(sc));
		else {
			sc->sc_chipc = NULL;
			printf(": AGP GART");
		}
	}
#endif
	printf("\n");
#ifdef MD_DISPLAY_ISA_IOT
	vga_extended_attach(self, MD_DISPLAY_ISA_IOT, ppc_isa_membus_space,
	    WSDISPLAY_TYPE_PCIVGA, vga_pci_mmap);
#else
	vga_common_attach(self, pa->pa_iot, pa->pa_memt,
	    WSDISPLAY_TYPE_PCIVGA);
#endif
}

paddr_t
vga_pci_mmap(void *v, off_t off, int prot)
{
#ifdef PCIAGP
	struct vga_config* vs = (struct vga_config*) v;
	struct vga_pci_softc* sc = (struct vga_pci_softc *)vs->vc_softc;

	if (sc->sc_apaddr) {

		if (off > AGP_GET_APERTURE(sc))
			return (-1);

		return atop(sc->sc_apaddr + off);
	}
#endif
	return -1;
}

int
vga_pci_cnattach(iot, memt, pc, bus, device, function)
	bus_space_tag_t iot, memt;
	pci_chipset_tag_t pc;
	int bus, device, function;
{
	return (vga_cnattach(iot, memt, WSDISPLAY_TYPE_PCIVGA, 0));
}

int
vga_pci_ioctl(v, cmd, addr, flag, p)
	void *v;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	int error = 0;
#ifdef PCIAGP
	struct vga_config *vc = v;
	struct vga_pci_softc *sc = (struct vga_pci_softc *)vc->vc_softc;
	struct agp_memory *mem;
	agp_info *info;
	agp_setup *setup;
	agp_allocate *alloc;
	agp_bind *bind;
	agp_unbind *unbind;
	vsize_t size;

	switch (cmd) {
	case AGPIOC_INFO:
		if (!sc->sc_chipc)
			return (ENXIO);
	case AGPIOC_ACQUIRE:
	case AGPIOC_RELEASE:
	case AGPIOC_SETUP:
	case AGPIOC_ALLOCATE:
	case AGPIOC_DEALLOCATE:
	case AGPIOC_BIND:
	case AGPIOC_UNBIND:
		if (cmd != AGPIOC_INFO && !(flag & FWRITE))
			return (EPERM);
		break;
	}
#endif

	switch (cmd) {
#ifdef PCIAGP
	case AGPIOC_INFO:
		info = (agp_info *)addr;
		bzero(info, sizeof *info);
		info->bridge_id = sc->sc_id;
		if (sc->sc_capoff != 0)
			info->agp_mode = pci_conf_read(sc->sc_pc, sc->sc_pcitag,
			    AGP_STATUS + sc->sc_capoff);
		else
			info->agp_mode = 0; /* i810 doesn't have real AGP */
		info->aper_base = sc->sc_apaddr;
		info->aper_size = AGP_GET_APERTURE(sc) >> 20;
		info->pg_total =
		info->pg_system = sc->sc_maxmem >> AGP_PAGE_SHIFT;
		info->pg_used = sc->sc_allocated >> AGP_PAGE_SHIFT;
		break;

	case AGPIOC_ACQUIRE:
		if (sc->sc_state != AGP_ACQUIRE_FREE)
			error = EBUSY;
		else
			sc->sc_state = AGP_ACQUIRE_USER;
		break;

	case AGPIOC_RELEASE:
		if (sc->sc_state == AGP_ACQUIRE_FREE)
			break;

		if (sc->sc_state != AGP_ACQUIRE_USER) {
			error = EBUSY;
			break;
		}

		/*
		 * Clear out the aperture and free any
		 * outstanding memory blocks.
		 */
		TAILQ_FOREACH(mem, &sc->sc_memory, am_link) {
			if (mem->am_is_bound) {
				printf("agp_release_helper: mem %d is bound\n",
				    mem->am_id);
				AGP_UNBIND_MEMORY(sc, mem);
			}
		}
		sc->sc_state = AGP_ACQUIRE_FREE;
		break;

	case AGPIOC_SETUP:
		setup = (agp_setup *)addr;
		error = AGP_ENABLE(sc, setup->agp_mode);
		break;

	case AGPIOC_ALLOCATE:
		alloc = (agp_allocate *)addr;
		size = alloc->pg_count << AGP_PAGE_SHIFT;
		if (sc->sc_allocated + size > sc->sc_maxmem)
			error = EINVAL;
		else {
			mem = AGP_ALLOC_MEMORY(sc, alloc->type, size);
			if (mem) {
				alloc->key = mem->am_id;
				alloc->physical = mem->am_physical;
			} else
				error = ENOMEM;
		}
		break;

	case AGPIOC_DEALLOCATE:
		mem = agp_find_memory(sc, *(int *)addr);
		if (mem)
			AGP_FREE_MEMORY(sc, mem);
		else
			error = ENOENT;
		break;

	case AGPIOC_BIND:
		bind = (agp_bind *)addr;
		mem = agp_find_memory(sc, bind->key);
		if (!mem)
			error = ENOENT;
		else
			error = AGP_BIND_MEMORY(sc, mem,
			    bind->pg_start << AGP_PAGE_SHIFT);
		break;

	case AGPIOC_UNBIND:
		unbind = (agp_unbind *)addr;
		mem = agp_find_memory(sc, unbind->key);
		if (!mem)
			error = ENOENT;
		else
			error = AGP_UNBIND_MEMORY(sc, mem);
		break;
#endif /* PCIAGP */

	default:
		error = ENOTTY;
	}

	return (error);
}

#ifdef PCIAGP
struct agp_memory *
agp_find_memory(struct vga_pci_softc *sc, int id)
{
	struct agp_memory *mem;

	AGP_DPF("searching for memory block %d\n", id);
	TAILQ_FOREACH(mem, &sc->sc_memory, am_link) {
		AGP_DPF("considering memory block %d\n", mem->am_id);
		if (mem->am_id == id)
			return (mem);
	}
	return 0;
}

const struct agp_product *
agp_lookup(struct pci_attach_args *pa)
{
	const struct agp_product *ap;

	if (!agp_pchb_pa_set)
		return (NULL);
	agp_pchb_pa_set = 0;

	/* First find the vendor. */
	for (ap = agp_products; ap->ap_attach != NULL; ap++)
		if (ap->ap_vendor == PCI_VENDOR(pa->pa_id))
			break;

	if (ap->ap_attach == NULL)
		return (NULL);

	/* Now find the product within the vendor's domain. */
	for (; ap->ap_attach != NULL; ap++) {
		/* Ran out of this vendor's section of the table. */
		if (ap->ap_vendor != PCI_VENDOR(pa->pa_id))
			return (NULL);

		if (ap->ap_product == PCI_PRODUCT(pa->pa_id))
			break;		/* Exact match. */
		if (ap->ap_product == (u_int32_t) -1)
			break;		/* Wildcard match. */
	}

	if (ap->ap_attach == NULL)
		ap = NULL;

	return (ap);
}

void
pciagp_set_pchb(struct pci_attach_args *pa)
{
	if (!agp_pchb_pa_set) {
		memcpy(&agp_pchb_pa, pa, sizeof *pa);
		agp_pchb_pa_set++;
	}
}

int
agp_map_aperture(struct vga_pci_softc *sc)
{
	/*
	 * Find and the aperture. Don't map it (yet), this would
	 * eat KVA.
	 */
	if (pci_mapreg_info(sc->sc_pc, sc->sc_pcitag, AGP_APBASE,
	    PCI_MAPREG_TYPE_MEM, &sc->sc_apaddr, &sc->sc_apsize,
	    &sc->sc_apflags) != 0)
		return ENXIO;

	return 0;
}

struct agp_gatt *
agp_alloc_gatt(struct vga_pci_softc *sc)
{
	u_int32_t apsize = AGP_GET_APERTURE(sc);
	u_int32_t entries = apsize >> AGP_PAGE_SHIFT;
	struct agp_gatt *gatt;
	int nseg;

	gatt = malloc(sizeof(*gatt), M_DEVBUF, M_NOWAIT);
	if (!gatt)
		return (NULL);
	bzero(gatt, sizeof(*gatt));
	gatt->ag_entries = entries;

	if (agp_alloc_dmamem(sc->sc_dmat, entries * sizeof(u_int32_t),
	    0, &gatt->ag_dmamap, (caddr_t *)&gatt->ag_virtual,
	    &gatt->ag_physical, &gatt->ag_dmaseg, 1, &nseg) != 0)
		return NULL;

	gatt->ag_size = entries * sizeof(u_int32_t);
	memset(gatt->ag_virtual, 0, gatt->ag_size);
	agp_flush_cache();

	return gatt;
}

void
agp_free_gatt(struct vga_pci_softc *sc, struct agp_gatt *gatt)
{
	agp_free_dmamem(sc->sc_dmat, gatt->ag_size, gatt->ag_dmamap,
	    (caddr_t)gatt->ag_virtual, &gatt->ag_dmaseg, 1);
	free(gatt, M_DEVBUF);
}

int
agp_generic_detach(struct vga_pci_softc *sc)
{
	lockmgr(&sc->sc_lock, LK_DRAIN, NULL);
	agp_flush_cache();
	return 0;
}

int
agp_generic_enable(struct vga_pci_softc *sc, u_int32_t mode)
{
	pcireg_t tstatus, mstatus;
	pcireg_t command;
	int rq, sba, fw, rate, capoff;
	
	if (pci_get_capability(sc->sc_pc, sc->sc_pcitag, PCI_CAP_AGP,
	     &capoff, NULL) == 0) {
		printf("agp_generic_enable: not an AGP capable device\n");
		return -1;
	}

	tstatus = pci_conf_read(sc->sc_pc, sc->sc_pcitag,
	    sc->sc_capoff + AGP_STATUS);
	mstatus = pci_conf_read(sc->sc_pc, sc->sc_pcitag,
	    capoff + AGP_STATUS);

	/* Set RQ to the min of mode, tstatus and mstatus */
	rq = AGP_MODE_GET_RQ(mode);
	if (AGP_MODE_GET_RQ(tstatus) < rq)
		rq = AGP_MODE_GET_RQ(tstatus);
	if (AGP_MODE_GET_RQ(mstatus) < rq)
		rq = AGP_MODE_GET_RQ(mstatus);

	/* Set SBA if all three can deal with SBA */
	sba = (AGP_MODE_GET_SBA(tstatus)
	       & AGP_MODE_GET_SBA(mstatus)
	       & AGP_MODE_GET_SBA(mode));

	/* Similar for FW */
	fw = (AGP_MODE_GET_FW(tstatus)
	       & AGP_MODE_GET_FW(mstatus)
	       & AGP_MODE_GET_FW(mode));

	/* Figure out the max rate */
	rate = (AGP_MODE_GET_RATE(tstatus)
		& AGP_MODE_GET_RATE(mstatus)
		& AGP_MODE_GET_RATE(mode));
	if (rate & AGP_MODE_RATE_4x)
		rate = AGP_MODE_RATE_4x;
	else if (rate & AGP_MODE_RATE_2x)
		rate = AGP_MODE_RATE_2x;
	else
		rate = AGP_MODE_RATE_1x;

	/* Construct the new mode word and tell the hardware  */
	command = AGP_MODE_SET_RQ(0, rq);
	command = AGP_MODE_SET_SBA(command, sba);
	command = AGP_MODE_SET_FW(command, fw);
	command = AGP_MODE_SET_RATE(command, rate);
	command = AGP_MODE_SET_AGP(command, 1);
	pci_conf_write(sc->sc_pc, sc->sc_pcitag,
	    sc->sc_capoff + AGP_COMMAND, command);
	pci_conf_write(sc->sc_pc, sc->sc_pcitag, capoff + AGP_COMMAND, command);
	return 0;
}

struct agp_memory *
agp_generic_alloc_memory(struct vga_pci_softc *sc, int type, vsize_t size)
{
	struct agp_memory *mem;

	if (type != 0) {
		printf("agp_generic_alloc_memory: unsupported type %d\n", type);
		return 0;
	}

	mem = malloc(sizeof *mem, M_DEVBUF, M_WAITOK);
	if (mem == NULL)
		return NULL;

	if (bus_dmamap_create(sc->sc_dmat, size, size / PAGE_SIZE + 1,
	    size, 0, BUS_DMA_NOWAIT, &mem->am_dmamap) != 0) {
		free(mem, M_DEVBUF);
		return NULL;
	}

	mem->am_id = sc->sc_nextid++;
	mem->am_size = size;
	mem->am_type = 0;
	mem->am_physical = 0;
	mem->am_offset = 0;
	mem->am_is_bound = 0;
	TAILQ_INSERT_TAIL(&sc->sc_memory, mem, am_link);
	sc->sc_allocated += size;

	return mem;
}

int
agp_generic_free_memory(struct vga_pci_softc *sc, struct agp_memory *mem)
{
	if (mem->am_is_bound)
		return EBUSY;

	sc->sc_allocated -= mem->am_size;
	TAILQ_REMOVE(&sc->sc_memory, mem, am_link);
	bus_dmamap_destroy(sc->sc_dmat, mem->am_dmamap);
	free(mem, M_DEVBUF);
	return 0;
}

int
agp_generic_bind_memory(struct vga_pci_softc *sc, struct agp_memory *mem,
			off_t offset)
{
	bus_dma_segment_t *segs, *seg;
	bus_size_t done, j;
	bus_addr_t pa;
	off_t i, k;
	int contigpages, nseg, error;

	lockmgr(&sc->sc_lock, LK_EXCLUSIVE, NULL);

	if (mem->am_is_bound) {
		printf("AGP: memory already bound\n");
		lockmgr(&sc->sc_lock, LK_RELEASE, NULL);
		return EINVAL;
	}

	if (offset < 0
	    || (offset & (AGP_PAGE_SIZE - 1)) != 0
	    || offset + mem->am_size > AGP_GET_APERTURE(sc)) {
		printf("AGP: binding memory at bad offset %#lx\n",
			      (unsigned long) offset);
		lockmgr(&sc->sc_lock, LK_RELEASE, NULL);
		return EINVAL;
	}

	/*
	 * XXXfvdl
	 * The memory here needs to be directly accessable from the
	 * AGP video card, so it should be allocated using bus_dma.
	 * However, it need not be contiguous, since individual pages
	 * are translated using the GATT.
	 *
	 * Using a large chunk of contiguous memory may get in the way
	 * of other subsystems that may need one, so we try to be friendly
	 * and ask for allocation in chunks of a minimum of 8 pages
	 * of contiguous memory on average, falling back to 4, 2 and 1
	 * if really needed. Larger chunks are preferred, since allocating
	 * a bus_dma_segment per page would be overkill.
	 */

	for (contigpages = 32; contigpages > 0; contigpages >>= 1) {
		nseg = (mem->am_size / (contigpages * PAGE_SIZE)) + 1;
		segs = malloc(nseg * sizeof *segs, M_DEVBUF, M_WAITOK);
		if (segs == NULL)
			return ENOMEM;
		if ((error = bus_dmamem_alloc(sc->sc_dmat, mem->am_size, PAGE_SIZE, 0,
		    segs, nseg, &mem->am_nseg, BUS_DMA_WAITOK)) != 0) {
			free(segs, M_DEVBUF);
			AGP_DPF("bus_dmamem_alloc failed %d\n", error);
			continue;
		}
		if ((error = bus_dmamem_map(sc->sc_dmat, segs, mem->am_nseg,
		    mem->am_size, &mem->am_virtual, BUS_DMA_WAITOK)) != 0) {
			bus_dmamem_free(sc->sc_dmat, segs, mem->am_nseg);
			free(segs, M_DEVBUF);
			AGP_DPF("bus_dmamem_map failed %d\n", error);
			continue;
		}
		if ((error = bus_dmamap_load(sc->sc_dmat, mem->am_dmamap,
		    mem->am_virtual, mem->am_size, NULL,
		    BUS_DMA_WAITOK)) != 0) {
			bus_dmamem_unmap(sc->sc_dmat, mem->am_virtual,
			    mem->am_size);
			bus_dmamem_free(sc->sc_dmat, segs, mem->am_nseg);
			free(segs, M_DEVBUF);
			AGP_DPF("bus_dmamap_load failed %d\n", error);
			continue;
		}
		mem->am_dmaseg = segs;
		break;
	}

	if (contigpages == 0) {
		lockmgr(&sc->sc_lock, LK_RELEASE, NULL);
		return ENOMEM;
	}

	/*
	 * Bind the individual pages and flush the chipset's
	 * TLB.
	 */
	done = 0;
	for (i = 0; i < mem->am_dmamap->dm_nsegs; i++) {
		seg = &mem->am_dmamap->dm_segs[i];
		/*
		 * Install entries in the GATT, making sure that if
		 * AGP_PAGE_SIZE < PAGE_SIZE and mem->am_size is not
		 * aligned to PAGE_SIZE, we don't modify too many GATT
		 * entries.
		 */
		for (j = 0; j < seg->ds_len && (done + j) < mem->am_size;
		     j += AGP_PAGE_SIZE) {
			pa = seg->ds_addr + j;
			AGP_DPF("binding offset %#lx to pa %#lx\n",
				(unsigned long)(offset + done + j),
				(unsigned long)pa);
			error = AGP_BIND_PAGE(sc, offset + done + j, pa);
			if (error) {
				/*
				 * Bail out. Reverse all the mappings
				 * and unwire the pages.
				 */
				for (k = 0; k < done + j; k += AGP_PAGE_SIZE)
					AGP_UNBIND_PAGE(sc, offset + k);

				bus_dmamap_unload(sc->sc_dmat, mem->am_dmamap);
				bus_dmamem_unmap(sc->sc_dmat, mem->am_virtual,
						 mem->am_size);
				bus_dmamem_free(sc->sc_dmat, mem->am_dmaseg,
						mem->am_nseg);
				free(mem->am_dmaseg, M_DEVBUF);
				lockmgr(&sc->sc_lock, LK_RELEASE, NULL);
				return error;
			}
		}
		done += seg->ds_len;
	}

	/*
	 * Flush the cpu cache since we are providing a new mapping
	 * for these pages.
	 */
	agp_flush_cache();

	/*
	 * Make sure the chipset gets the new mappings.
	 */
	AGP_FLUSH_TLB(sc);

	mem->am_offset = offset;
	mem->am_is_bound = 1;

	lockmgr(&sc->sc_lock, LK_RELEASE, NULL);

	return 0;
}

int
agp_generic_unbind_memory(struct vga_pci_softc *sc, struct agp_memory *mem)
{
	int i;

	lockmgr(&sc->sc_lock, LK_EXCLUSIVE, NULL);

	if (!mem->am_is_bound) {
		printf("AGP: memory is not bound\n");
		lockmgr(&sc->sc_lock, LK_RELEASE, NULL);
		return EINVAL;
	}


	/*
	 * Unbind the individual pages and flush the chipset's
	 * TLB. Unwire the pages so they can be swapped.
	 */
	for (i = 0; i < mem->am_size; i += AGP_PAGE_SIZE)
		AGP_UNBIND_PAGE(sc, mem->am_offset + i);

	agp_flush_cache();
	AGP_FLUSH_TLB(sc);

	bus_dmamap_unload(sc->sc_dmat, mem->am_dmamap);
	bus_dmamem_unmap(sc->sc_dmat, mem->am_virtual, mem->am_size);
	bus_dmamem_free(sc->sc_dmat, mem->am_dmaseg, mem->am_nseg);

	free(mem->am_dmaseg, M_DEVBUF);

	mem->am_offset = 0;
	mem->am_is_bound = 0;

	lockmgr(&sc->sc_lock, LK_RELEASE, NULL);

	return 0;
}

int
agp_alloc_dmamem(bus_dma_tag_t tag, size_t size, int flags,
		 bus_dmamap_t *mapp, caddr_t *vaddr, bus_addr_t *baddr,
		 bus_dma_segment_t *seg, int nseg, int *rseg)

{
	int error, level = 0;

	if ((error = bus_dmamem_alloc(tag, size, PAGE_SIZE, 0,
			seg, nseg, rseg, BUS_DMA_NOWAIT)) != 0)
		goto out;
	level++;

	if ((error = bus_dmamem_map(tag, seg, *rseg, size, vaddr,
			BUS_DMA_NOWAIT | flags)) != 0)
		goto out;
	level++;

	if ((error = bus_dmamap_create(tag, size, *rseg, size, 0,
			BUS_DMA_NOWAIT, mapp)) != 0)
		goto out;
	level++;

	if ((error = bus_dmamap_load(tag, *mapp, *vaddr, size, NULL,
			BUS_DMA_NOWAIT)) != 0)
		goto out;

	*baddr = (*mapp)->dm_segs[0].ds_addr;

	return 0;
out:
	switch (level) {
	case 3:
		bus_dmamap_destroy(tag, *mapp);
		/* FALLTHROUGH */
	case 2:
		bus_dmamem_unmap(tag, *vaddr, size);
		/* FALLTHROUGH */
	case 1:
		bus_dmamem_free(tag, seg, *rseg);
		break;
	default:
		break;
	}

	return error;
}

void
agp_free_dmamem(bus_dma_tag_t tag, size_t size, bus_dmamap_t map,
		caddr_t vaddr, bus_dma_segment_t *seg, int nseg)
{

	bus_dmamap_unload(tag, map);
	bus_dmamap_destroy(tag, map);
	bus_dmamem_unmap(tag, vaddr, size);
	bus_dmamem_free(tag, seg, nseg);
}
#endif
