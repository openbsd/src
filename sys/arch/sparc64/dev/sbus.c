/*	$OpenBSD: sbus.c,v 1.15 2003/02/17 01:29:20 henric Exp $	*/
/*	$NetBSD: sbus.c,v 1.46 2001/10/07 20:30:41 eeh Exp $ */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)sbus.c	8.1 (Berkeley) 6/11/93
 */

/*
 * Copyright (c) 1999 Eduardo Horvath
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR  ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR  BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


/*
 * Sbus stuff.
 */

#include <sys/param.h>
#include <sys/extent.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <machine/bus.h>
#include <sparc64/sparc64/cache.h>
#include <sparc64/dev/iommureg.h>
#include <sparc64/dev/iommuvar.h>
#include <sparc64/dev/sbusreg.h>
#include <dev/sbus/sbusvar.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/sparc64.h>

#ifdef DEBUG
#define SDB_DVMA	0x1
#define SDB_INTR	0x2
int sbus_debug = 0;
#define DPRINTF(l, s)   do { if (sbus_debug & l) printf s; } while (0)
#else
#define DPRINTF(l, s)
#endif

void sbusreset(int);

static bus_space_tag_t sbus_alloc_bustag(struct sbus_softc *);
static bus_dma_tag_t sbus_alloc_dmatag(struct sbus_softc *);
static int sbus_get_intr(struct sbus_softc *, int,
    struct sbus_intr **, int *, int);
static int sbus_overtemp(void *);
static int _sbus_bus_map(bus_space_tag_t, bus_space_tag_t,
    bus_addr_t,		/*offset*/
    bus_size_t,		/*size*/
    int,		/*flags*/
    bus_space_handle_t *);
static void *sbus_intr_establish(bus_space_tag_t, bus_space_tag_t,
    int,		/*Sbus interrupt level*/
    int,		/*`device class' priority*/
    int,		/*flags*/
    int (*)(void *),	/*handler*/
    void *);		/*handler arg*/


/* autoconfiguration driver */
int	sbus_match(struct device *, void *, void *);
void	sbus_attach(struct device *, struct device *, void *);


struct cfattach sbus_ca = {
	sizeof(struct sbus_softc), sbus_match, sbus_attach
};

struct cfdriver sbus_cd = {
	NULL, "sbus", DV_DULL
};

extern struct cfdriver sbus_cd;

/*
 * DVMA routines
 */
int sbus_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *, bus_size_t,
    struct proc *, int);
void sbus_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
int sbus_dmamap_load_raw(bus_dma_tag_t, bus_dmamap_t, bus_dma_segment_t *,
    int, bus_size_t, int);
void sbus_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_addr_t, bus_size_t, int);
int sbus_dmamem_alloc(bus_dma_tag_t tag, bus_size_t size, bus_size_t alignment,
    bus_size_t boundary, bus_dma_segment_t *segs, int nsegs, int *rsegs,
    int flags);
void sbus_dmamem_free(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs);
int sbus_dmamem_map(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs,
    size_t size, caddr_t *kvap, int flags);
void sbus_dmamem_unmap(bus_dma_tag_t tag, caddr_t kva, size_t size);

/*
 * Child devices receive the Sbus interrupt level in their attach
 * arguments. We translate these to CPU IPLs using the following
 * tables. Note: obio bus interrupt levels are identical to the
 * processor IPL.
 *
 * The second set of tables is used when the Sbus interrupt level
 * cannot be had from the PROM as an `interrupt' property. We then
 * fall back on the `intr' property which contains the CPU IPL.
 */

/* Translate Sbus interrupt level to processor IPL */
static int intr_sbus2ipl_4u[] = {
	0, 2, 3, 5, 7, 9, 11, 13
};

/*
 * This value is or'ed into the attach args' interrupt level cookie
 * if the interrupt level comes from an `intr' property, i.e. it is
 * not an Sbus interrupt level.
 */
#define SBUS_INTR_COMPAT	0x80000000


/*
 * Print the location of some sbus-attached device (called just
 * before attaching that device).  If `sbus' is not NULL, the
 * device was found but not configured; print the sbus as well.
 * Return UNCONF (config_find ignores this if the device was configured).
 */
int
sbus_print(void *args, const char *busname)
{
	struct sbus_attach_args *sa = args;
	int i;

	if (busname)
		printf("%s at %s", sa->sa_name, busname);
	printf(" slot %ld offset 0x%lx", (long)sa->sa_slot, 
	       (u_long)sa->sa_offset);
	for (i = 0; i < sa->sa_nintr; i++) {
		struct sbus_intr *sbi = &sa->sa_intr[i];

		printf(" vector %lx ipl %ld", 
		       (u_long)sbi->sbi_vec, 
		       (long)INTLEV(sbi->sbi_pri));
	}
	return (UNCONF);
}

int
sbus_match(struct device *parent, void *vcf, void *aux)
{
	struct cfdata *cf = vcf;
	struct mainbus_attach_args *ma = aux;

	return (strcmp(cf->cf_driver->cd_name, ma->ma_name) == 0);
}

/*
 * Attach an Sbus.
 */
void
sbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct sbus_softc *sc = (struct sbus_softc *)self;
	struct mainbus_attach_args *ma = aux;
	struct intrhand *ih;
	struct sysioreg *sysio;
	int ipl;
	char *name;
	int node = ma->ma_node;
	int node0, error;
	bus_space_tag_t sbt;
	struct sbus_attach_args sa;

	sc->sc_bustag = ma->ma_bustag;
	sc->sc_dmatag = ma->ma_dmatag;
	/* Find interrupt group no */
	sc->sc_ign = ma->ma_interrupts[0] & INTMAP_IGN;

	bus_space_map(sc->sc_bustag,
	    ma->ma_address[0], sizeof(struct sysioreg),
	    BUS_SPACE_MAP_PROMADDRESS, &sc->sc_bh);
	sysio = bus_space_vaddr(sc->sc_bustag, sc->sc_bh);

	/* Setup interrupt translation tables */
	sc->sc_intr2ipl = intr_sbus2ipl_4u;

	/*
	 * Record clock frequency for synchronous SCSI.
	 * IS THIS THE CORRECT DEFAULT??
	 */
	sc->sc_clockfreq = getpropint(node, "clock-frequency", 25*1000*1000);
	printf(": clock = %s MHz\n", clockfreq(sc->sc_clockfreq));

	sbt = sbus_alloc_bustag(sc);
	sc->sc_dmatag = sbus_alloc_dmatag(sc);

	/*
	 * Get the SBus burst transfer size if burst transfers are supported
	 */
	sc->sc_burst = getpropint(node, "burst-sizes", 0);

	/*
	 * Collect address translations from the OBP.
	 */
	error = getprop(node, "ranges", sizeof(struct sbus_range),
			 &sc->sc_nrange, (void **)&sc->sc_range);
	if (error)
		panic("%s: error getting ranges property", sc->sc_dev.dv_xname);

	/* initialize the IOMMU */

	/* punch in our copies */
	sc->sc_is.is_bustag = sc->sc_bustag;
	bus_space_subregion(sc->sc_bustag, sc->sc_bh,
	    offsetof(struct sysioreg, sys_iommu),
	    sizeof(struct iommureg), &sc->sc_is.is_iommu);

	/* initialize our strbuf_ctl */
	sc->sc_is.is_sb[0] = &sc->sc_sb;
	if (bus_space_subregion(sc->sc_bustag, sc->sc_bh,
	    offsetof(struct sysioreg, sys_strbuf),
	    sizeof(struct iommu_strbuf), &sc->sc_sb.sb_sb) == 0) {
		/* point sb_flush to our flush buffer */
		sc->sc_sb.sb_flush = &sc->sc_flush;
		sc->sc_sb.sb_bustag = sc->sc_bustag;
	} else
		sc->sc_is.is_sb[0] = NULL;

	/* give us a nice name.. */
	name = (char *)malloc(32, M_DEVBUF, M_NOWAIT);
	if (name == 0)
		panic("couldn't malloc iommu name");
	snprintf(name, 32, "%s dvma", sc->sc_dev.dv_xname);

	iommu_init(name, &sc->sc_is, 0, -1);

	/* Enable the over temp intr */
	ih = (struct intrhand *)
		malloc(sizeof(struct intrhand), M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
		panic("couldn't malloc intrhand");
	ih->ih_map = &sysio->therm_int_map;
	ih->ih_clr = NULL; /* &sysio->therm_clr_int; */
	ih->ih_fun = sbus_overtemp;
	ipl = 1;
	ih->ih_pil = (1 << ipl);
	ih->ih_number = INTVEC(*(ih->ih_map));
	intr_establish(ipl, ih);
	*(ih->ih_map) |= INTMAP_V;
	
	/*
	 * Note: the stupid SBUS IOMMU ignores the high bits of an address, so a
	 * NULL DMA pointer will be translated by the first page of the IOTSB.
	 * To avoid bugs we'll alloc and ignore the first entry in the IOTSB.
	 */
	{
		u_long dummy;

		if (extent_alloc_subregion(sc->sc_is.is_dvmamap,
		    sc->sc_is.is_dvmabase, sc->sc_is.is_dvmabase + NBPG, NBPG,
		    NBPG, 0, 0, EX_NOWAIT | EX_BOUNDZERO,
		    (u_long *)&dummy) != 0)
			panic("sbus iommu: can't toss first dvma page");
	}

	/*
	 * Loop through ROM children, fixing any relative addresses
	 * and then configuring each device.
	 * `specials' is an array of device names that are treated
	 * specially:
	 */
	node0 = firstchild(node);
	for (node = node0; node; node = nextsibling(node)) {
		char *name = getpropstring(node, "name");

		if (sbus_setup_attach_args(sc, sbt, sc->sc_dmatag,
					   node, &sa) != 0) {
			printf("sbus_attach: %s: incomplete\n", name);
			continue;
		}
		(void) config_found(&sc->sc_dev, (void *)&sa, sbus_print);
		sbus_destroy_attach_args(&sa);
	}
}

int
sbus_setup_attach_args(struct sbus_softc *sc, bus_space_tag_t bustag,
    bus_dma_tag_t dmatag, int node, struct sbus_attach_args *sa)
{
	int	error;
	int	n;

	bzero(sa, sizeof(struct sbus_attach_args));
	error = getprop(node, "name", 1, &n, (void **)&sa->sa_name);
	if (error != 0)
		return (error);
	sa->sa_name[n] = '\0';

	sa->sa_bustag = bustag;
	sa->sa_dmatag = dmatag;
	sa->sa_node = node;
	sa->sa_frequency = sc->sc_clockfreq;

	error = getprop(node, "reg", sizeof(struct sbus_reg),
			 &sa->sa_nreg, (void **)&sa->sa_reg);
	if (error != 0) {
		char buf[32];
		if (error != ENOENT ||
		    !node_has_property(node, "device_type") ||
		    strcmp(getpropstringA(node, "device_type", buf),
			   "hierarchical") != 0)
			return (error);
	}
	for (n = 0; n < sa->sa_nreg; n++) {
		/* Convert to relative addressing, if necessary */
		u_int32_t base = sa->sa_reg[n].sbr_offset;
		if (SBUS_ABS(base)) {
			sa->sa_reg[n].sbr_slot = SBUS_ABS_TO_SLOT(base);
			sa->sa_reg[n].sbr_offset = SBUS_ABS_TO_OFFSET(base);
		}
	}

	if ((error = sbus_get_intr(sc, node, &sa->sa_intr, &sa->sa_nintr,
	    sa->sa_slot)) != 0)
		return (error);

	error = getprop(node, "address", sizeof(u_int32_t),
			 &sa->sa_npromvaddrs, (void **)&sa->sa_promvaddrs);
	if (error != 0 && error != ENOENT)
		return (error);

	return (0);
}

void
sbus_destroy_attach_args(struct sbus_attach_args *sa)
{
	if (sa->sa_name != NULL)
		free(sa->sa_name, M_DEVBUF);

	if (sa->sa_nreg != 0)
		free(sa->sa_reg, M_DEVBUF);

	if (sa->sa_intr)
		free(sa->sa_intr, M_DEVBUF);

	if (sa->sa_promvaddrs)
		free((void *)sa->sa_promvaddrs, M_DEVBUF);

	bzero(sa, sizeof(struct sbus_attach_args)); /*DEBUG*/
}


int
_sbus_bus_map(bus_space_tag_t t, bus_space_tag_t t0, bus_addr_t addr,
    bus_size_t size, int flags, bus_space_handle_t *hp)
{
	struct sbus_softc *sc = t->cookie;
	int64_t slot = BUS_ADDR_IOSPACE(addr);
	int64_t offset = BUS_ADDR_PADDR(addr);
	int i;

	if (t->parent == NULL || t->parent->sparc_bus_map == NULL) {
		printf("\n_psycho_bus_map: invalid parent");
		return (EINVAL);
	}

	if (flags & BUS_SPACE_MAP_PROMADDRESS) {
		return ((*t->parent->sparc_bus_map)(t, t0, addr,
					size, flags, hp));
	}

	for (i = 0; i < sc->sc_nrange; i++) {
		bus_addr_t paddr;

		if (sc->sc_range[i].cspace != slot)
			continue;

		/* We've found the connection to the parent bus */
		paddr = sc->sc_range[i].poffset + offset;
		paddr |= ((bus_addr_t)sc->sc_range[i].pspace << 32);
		DPRINTF(SDB_DVMA, ("_sbus_bus_map: mapping paddr "
			"slot %lx offset %lx poffset %lx paddr %lx\n",
		    (long)slot, (long)offset, (long)sc->sc_range[i].poffset,
		    (long)paddr));
		return ((*t->parent->sparc_bus_map)(t, t0, paddr,
					size, flags, hp));
	}

	return (EINVAL);
}

bus_addr_t
sbus_bus_addr(bus_space_tag_t t, u_int btype, u_int offset)
{
	bus_addr_t baddr = ~(bus_addr_t)0;
	int slot = btype;
	struct sbus_softc *sc = t->cookie;
	int i;

	for (i = 0; i < sc->sc_nrange; i++) {
		if (sc->sc_range[i].cspace != slot)
			continue;

		baddr = sc->sc_range[i].poffset + offset;
		baddr |= (bus_addr_t)sc->sc_range[i].pspace << 32;
	}

	return (baddr);
}


/*
 * Each attached device calls sbus_establish after it initializes
 * its sbusdev portion.
 */
void
sbus_establish(struct sbusdev *sd, struct device *dev)
{
	struct sbus_softc *sc;
	struct device *curdev;

	/*
	 * We have to look for the sbus by name, since it is not necessarily
	 * our immediate parent (i.e. sun4m /iommu/sbus/espdma/esp)
	 * We don't just use the device structure of the above-attached
	 * sbus, since we might (in the future) support multiple sbus's.
	 */
	for (curdev = dev->dv_parent; ; curdev = curdev->dv_parent) {
		if (!curdev || !curdev->dv_xname)
			panic("sbus_establish: can't find sbus parent for %s",
			      sd->sd_dev->dv_xname
					? sd->sd_dev->dv_xname
					: "<unknown>" );

		if (strncmp(curdev->dv_xname, "sbus", 4) == 0)
			break;
	}
	sc = (struct sbus_softc *) curdev;

	sd->sd_dev = dev;
	sd->sd_bchain = sc->sc_sbdev;
	sc->sc_sbdev = sd;
}

/*
 * Reset the given sbus.
 */
void
sbusreset(int sbus)
{
	struct sbusdev *sd;
	struct sbus_softc *sc = sbus_cd.cd_devs[sbus];
	struct device *dev;

	printf("reset %s:", sc->sc_dev.dv_xname);
	for (sd = sc->sc_sbdev; sd != NULL; sd = sd->sd_bchain) {
		if (sd->sd_reset) {
			dev = sd->sd_dev;
			(*sd->sd_reset)(dev);
			printf(" %s", dev->dv_xname);
		}
	}
	/* Reload iommu regs */
	iommu_reset(&sc->sc_is);
}

/*
 * Handle an overtemp situation.
 *
 * SPARCs have temperature sensors which generate interrupts
 * if the machine's temperature exceeds a certain threshold.
 * This handles the interrupt and powers off the machine.
 * The same needs to be done to PCI controller drivers.
 */
int
sbus_overtemp(void *arg)
{
	/* Should try a clean shutdown first */
	printf("DANGER: OVER TEMPERATURE detected\nShutting down...\n");
	delay(20);
	boot(RB_POWERDOWN|RB_HALT);
	/*NOTREACHED*/
	return (1);
}

/*
 * Get interrupt attributes for an Sbus device.
 */
int
sbus_get_intr(struct sbus_softc *sc, int node, struct sbus_intr **ipp, int *np,
    int slot)
{
	int *ipl;
	int n, i;
	char buf[32];

	/*
	 * The `interrupts' property contains the Sbus interrupt level.
	 */
	ipl = NULL;
	if (getprop(node, "interrupts", sizeof(int), np, (void **)&ipl) == 0) {
		struct sbus_intr *ip;
		int pri;

		/* Default to interrupt level 2 -- otherwise unused */
		pri = INTLEVENCODE(2);

		/* Change format to an `struct sbus_intr' array */
		ip = malloc(*np * sizeof(struct sbus_intr), M_DEVBUF, M_NOWAIT);
		if (ip == NULL)
			return (ENOMEM);

		/*
		 * Now things get ugly.  We need to take this value which is
		 * the interrupt vector number and encode the IPL into it
		 * somehow. Luckily, the interrupt vector has lots of free
		 * space and we can easily stuff the IPL in there for a while.
		 */
		getpropstringA(node, "device_type", buf);
		if (!buf[0])
			getpropstringA(node, "name", buf);

		for (i = 0; intrmap[i].in_class; i++) 
			if (strcmp(intrmap[i].in_class, buf) == 0) {
				pri = INTLEVENCODE(intrmap[i].in_lev);
				break;
			}

		/*
		 * Sbus card devices need the slot number encoded into
		 * the vector as this is generally not done.
		 */
		if ((ipl[0] & INTMAP_OBIO) == 0)
			pri |= slot << 3;

		for (n = 0; n < *np; n++) {
			/* 
			 * We encode vector and priority into sbi_pri so we 
			 * can pass them as a unit.  This will go away if 
			 * sbus_establish ever takes an sbus_intr instead 
			 * of an integer level.
			 * Stuff the real vector in sbi_vec.
			 */

			ip[n].sbi_pri = pri | ipl[n];
			ip[n].sbi_vec = ipl[n];
		}
		free(ipl, M_DEVBUF);
		*ipp = ip;
	}
	
	return (0);
}


/*
 * Install an interrupt handler for an Sbus device.
 */
void *
sbus_intr_establish(bus_space_tag_t t, bus_space_tag_t t0, int pri, int level,
    int flags, int (*handler)(void *), void *arg)
{
	struct sbus_softc *sc = t->cookie;
	struct sysioreg *sysio;
	struct intrhand *ih;
	int ipl;
	long vec = pri; 

	sysio = bus_space_vaddr(sc->sc_bustag, sc->sc_bh);

	ih = (struct intrhand *)
		malloc(sizeof(struct intrhand), M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
		return (NULL);

	if ((flags & BUS_INTR_ESTABLISH_SOFTINTR) != 0)
		ipl = 1 << vec;
	else if ((vec & SBUS_INTR_COMPAT) != 0)
		ipl = 1 << (vec & ~SBUS_INTR_COMPAT);
	else {
		/* Decode and remove IPL */
		ipl = level;
		if (ipl == IPL_NONE)
			ipl = 1 << INTLEV(vec);
		if (ipl == IPL_NONE) {
			printf("ERROR: no IPL, setting IPL 2.\n");
			ipl = 2;
		}
		vec = INTVEC(vec);
		DPRINTF(SDB_INTR,
		    ("\nsbus: intr[%ld]%lx: %lx\nHunting for IRQ...\n",
		    (long)ipl, (long)vec, (u_long)intrlev[vec]));
		if ((vec & INTMAP_OBIO) == 0) {
			/* We're in an SBUS slot */
			/* Register the map and clear intr registers */
			bus_space_handle_t maph;
			int slot = INTSLOT(pri);

			ih->ih_map = &(&sysio->sbus_slot0_int)[slot];
			ih->ih_clr = &sysio->sbus0_clr_int[vec];
#ifdef DEBUG
			if (sbus_debug & SDB_INTR) {
				int64_t intrmap = *ih->ih_map;
				
				printf("SBUS %lx IRQ as %llx in slot %d\n", 
				       (long)vec, (long long)intrmap, slot);
				printf("\tmap addr %p clr addr %p\n",
				    ih->ih_map, ih->ih_clr);
			}
#endif
			/* Enable the interrupt */
			vec |= INTMAP_V;
			/* Insert IGN */
			vec |= sc->sc_ign;
			/*
			 * This would be cleaner if the underlying interrupt
			 * infrastructure took a bus tag/handle pair.  Even
			 * if not, the following could be done with a write
			 * to the appropriate offset from sc->sc_bustag and
			 * sc->sc_bh.
			 */
			bus_space_map(sc->sc_bustag, (bus_addr_t)ih->ih_map, 8,
			    BUS_SPACE_MAP_PROMADDRESS, &maph);
			bus_space_write_8(sc->sc_bustag, maph, 0, vec);
		} else {
			bus_space_handle_t maph;
			volatile int64_t *intrptr = &sysio->scsi_int_map;
			int64_t intrmap = 0;
			int i;

			/* Insert IGN */
			vec |= sc->sc_ign;
			for (i = 0; &intrptr[i] <=
			    (int64_t *)&sysio->reserved_int_map &&
			    INTVEC(intrmap = intrptr[i]) != INTVEC(vec); i++)
				;
			if (INTVEC(intrmap) == INTVEC(vec)) {
				DPRINTF(SDB_INTR,
				    ("OBIO %lx IRQ as %lx in slot %d\n", 
				    vec, (long)intrmap, i));
				/* Register the map and clear intr registers */
				ih->ih_map = &intrptr[i];
				intrptr = (int64_t *)&sysio->scsi_clr_int;
				ih->ih_clr = &intrptr[i];
				/* Enable the interrupt */
				intrmap |= INTMAP_V;
				/*
				 * This would be cleaner if the underlying
				 * interrupt infrastructure took a bus tag/
				 * handle pair.  Even if not, the following
				 * could be done with a write to the
				 * appropriate offset from sc->sc_bustag and
				 * sc->sc_bh.
				 */
				bus_space_map(sc->sc_bustag,
				    (bus_addr_t)ih->ih_map, 8,
				    BUS_SPACE_MAP_PROMADDRESS, &maph);
				bus_space_write_8(sc->sc_bustag, maph, 0,
				    (u_long)intrmap);
			} else
				panic("IRQ not found!");
		}
	}
#ifdef DEBUG
	if (sbus_debug & SDB_INTR) { long i; for (i = 0; i < 400000000; i++); }
#endif

	ih->ih_fun = handler;
	ih->ih_arg = arg;
	ih->ih_number = vec;
	ih->ih_pil = ipl;
	intr_establish(ih->ih_pil, ih);
	return (ih);
}

static bus_space_tag_t
sbus_alloc_bustag(struct sbus_softc *sc)
{
	struct sparc_bus_space_tag *sbt;

	sbt = malloc(sizeof(*sbt), M_DEVBUF, M_NOWAIT);
	if (sbt == NULL)
		return (NULL);

	bzero(sbt, sizeof *sbt);
	snprintf(sbt->name, sizeof(sbt->name), "%s",
		sc->sc_dev.dv_xname);
	sbt->cookie = sc;
	sbt->parent = sc->sc_bustag;
	sbt->default_type = SBUS_BUS_SPACE;
	sbt->asi = ASI_PRIMARY;
	sbt->sasi = ASI_PRIMARY;
	sbt->sparc_bus_map = _sbus_bus_map;
	sbt->sparc_bus_mmap = sc->sc_bustag->sparc_bus_mmap;
	sbt->sparc_intr_establish = sbus_intr_establish;
	return (sbt);
}


static bus_dma_tag_t
sbus_alloc_dmatag(struct sbus_softc *sc)
{
	bus_dma_tag_t sdt, psdt = sc->sc_dmatag;

	sdt = (bus_dma_tag_t)
		malloc(sizeof(struct sparc_bus_dma_tag), M_DEVBUF, M_NOWAIT);
	if (sdt == NULL)
		/* Panic? */
		return (psdt);

	sdt->_cookie = sc;
	sdt->_parent = psdt;
#define PCOPY(x)	sdt->x = psdt->x
	PCOPY(_dmamap_create);
	PCOPY(_dmamap_destroy);
	sdt->_dmamap_load = sbus_dmamap_load;
	PCOPY(_dmamap_load_mbuf);
	PCOPY(_dmamap_load_uio);
	sdt->_dmamap_load_raw = sbus_dmamap_load_raw;
	sdt->_dmamap_unload = sbus_dmamap_unload;
	sdt->_dmamap_sync = sbus_dmamap_sync;
	sdt->_dmamem_alloc = sbus_dmamem_alloc;
	sdt->_dmamem_free = sbus_dmamem_free;
	sdt->_dmamem_map = sbus_dmamem_map;
	sdt->_dmamem_unmap = sbus_dmamem_unmap;
	PCOPY(_dmamem_mmap);
#undef	PCOPY
	sc->sc_dmatag = sdt;
	return (sdt);
}

int
sbus_dmamap_load(bus_dma_tag_t tag, bus_dmamap_t map, void *buf,
    bus_size_t buflen, struct proc *p, int flags)
{
	struct sbus_softc *sc = tag->_cookie;

	return (iommu_dvmamap_load(tag, &sc->sc_is, map, buf, buflen,
	    p, flags));
}

int
sbus_dmamap_load_raw(bus_dma_tag_t tag, bus_dmamap_t map,
    bus_dma_segment_t *segs, int nsegs, bus_size_t size, int flags)
{
	struct sbus_softc *sc = tag->_cookie;

	return (iommu_dvmamap_load_raw(tag, &sc->sc_is, map, segs,
	    nsegs, flags, size));
}

void
sbus_dmamap_unload(bus_dma_tag_t tag, bus_dmamap_t map)
{
	struct sbus_softc *sc = tag->_cookie;

	iommu_dvmamap_unload(tag, &sc->sc_is, map);
}

void
sbus_dmamap_sync(bus_dma_tag_t tag, bus_dmamap_t map, bus_addr_t offset,
    bus_size_t len, int ops)
{
	struct sbus_softc *sc = tag->_cookie;

	if (ops & (BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE)) {
		/* Flush the CPU then the IOMMU */
		bus_dmamap_sync(tag->_parent, map, offset, len, ops);
		iommu_dvmamap_sync(tag, &sc->sc_is, map, offset, len, ops);
	}
	if (ops & (BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE)) {
		/* Flush the IOMMU then the CPU */
		iommu_dvmamap_sync(tag, &sc->sc_is, map, offset, len, ops);
		bus_dmamap_sync(tag->_parent, map, offset, len, ops);
	}
}

int
sbus_dmamem_alloc(bus_dma_tag_t tag, bus_size_t size, bus_size_t alignment,
    bus_size_t boundary, bus_dma_segment_t *segs, int nsegs, int *rsegs,
    int flags)
{
	struct sbus_softc *sc = tag->_cookie;

	return (iommu_dvmamem_alloc(tag, &sc->sc_is, size, alignment, boundary,
	    segs, nsegs, rsegs, flags));
}

void
sbus_dmamem_free(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs)
{
	struct sbus_softc *sc = (struct sbus_softc *)tag->_cookie;

	iommu_dvmamem_free(tag, &sc->sc_is, segs, nsegs);
}

int
sbus_dmamem_map(bus_dma_tag_t tag, bus_dma_segment_t *segs, int nsegs,
    size_t size, caddr_t *kvap, int flags)
{
	struct sbus_softc *sc = tag->_cookie;

	return (iommu_dvmamem_map(tag, &sc->sc_is, segs, nsegs, size,
	    kvap, flags));
}

void
sbus_dmamem_unmap(bus_dma_tag_t tag, caddr_t kva, size_t size)
{
	struct sbus_softc *sc = (struct sbus_softc *)tag->_cookie;

	iommu_dvmamem_unmap(tag, &sc->sc_is, kva, size);
}
