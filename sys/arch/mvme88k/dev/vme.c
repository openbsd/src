/*	$OpenBSD: vme.c,v 1.34 2004/04/24 19:51:48 miod Exp $ */
/*
 * Copyright (c) 2004, Miodrag Vallat.
 * Copyright (c) 1999 Steve Murphree, Jr.
 * Copyright (c) 1995 Theo de Raadt
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/uio.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/locore.h>

#include "pcctwo.h"
#include "syscon.h"

#include <mvme88k/dev/vme.h>
#if NSYSCON > 0
#include <mvme88k/dev/sysconreg.h>
#endif

int	vmematch(struct device *, void *, void *);
void	vmeattach(struct device *, struct device *, void *);

void	vme2chip_init(struct vmesoftc *);
void	vmesyscon_init(struct vmesoftc *);
u_long	vme2chip_map(u_long, int);
int	vme2abort(void *);
int	sysconabort(void *);
int	vmeprint(void *, const char *);

int vmebustype;
unsigned int vmevecbase;

struct cfattach vme_ca = {
        sizeof(struct vmesoftc), vmematch, vmeattach
};

struct cfdriver vme_cd = {
        NULL, "vme", DV_DULL
};

/*
 * bus_space routines for VME mappings
 */

int	vme_map(bus_addr_t, bus_size_t, int, bus_space_handle_t *);
void	vme_unmap(bus_space_handle_t, bus_size_t);
int	vme_subregion(bus_space_handle_t, bus_size_t, bus_size_t,
	    bus_space_handle_t *);
void *	vme_vaddr(bus_space_handle_t);

const struct mvme88k_bus_space_tag vme_bustag = {
	vme_map,
	vme_unmap,
	vme_subregion,
	vme_vaddr
};

/*
 * VME space mapping functions
 */

int
vme_map(bus_addr_t addr, bus_size_t size, int flags, bus_space_handle_t *ret)
{
	vaddr_t map;

#if 0
	map = iomap_mapin(addr, size, 0);
#else
	map = (vaddr_t)mapiodev((void *)addr, size);
#endif
	if (map == NULL)
		return ENOMEM;

	*ret = (bus_space_handle_t)map;
	return 0;
}

void
vme_unmap(bus_space_handle_t handle, bus_size_t size)
{
#if 0
	iomap_mapout(handle, size);
#else
	unmapiodev((void *)handle, size);
#endif
}

int
vme_subregion(bus_space_handle_t handle, bus_addr_t offset, bus_size_t size,
    bus_space_handle_t *ret)
{
	*ret = handle + offset;
	return (0);
}

void *
vme_vaddr(bus_space_handle_t handle)
{
	return (void *)handle;
}

/*
 * Configuration glue
 */

int
vmematch(parent, cf, args)
	struct device *parent;
	void *cf;
	void *args;
{
#ifdef MVME187
	if (brdtyp == BRD_8120)
		return (0);
#endif
	return (1);
}

int
vmeprint(args, bus)
	void *args;
	const char *bus;
{
	struct confargs *ca = args;

	printf(" addr 0x%x", ca->ca_paddr);
	if (ca->ca_ipl > 0)
		printf(" ipl %d", ca->ca_ipl);
	if (ca->ca_vec > 0)
		printf(" vec 0x%x", ca->ca_vec);
	return (UNCONF);
}

int
vmescan(parent, child, args, bustype)
	struct device *parent;
	void *child, *args;
	int bustype;
{
	struct cfdata *cf = child;
	struct confargs oca;

	bzero(&oca, sizeof oca);
	oca.ca_iot = &vme_bustag;
	oca.ca_bustype = bustype;
	oca.ca_paddr = cf->cf_loc[0];
	oca.ca_vec = cf->cf_loc[1];
	oca.ca_ipl = cf->cf_loc[2];
	if (oca.ca_ipl > 0 && oca.ca_vec < 0)
		oca.ca_vec = vme_findvec(-1);
	oca.ca_name = cf->cf_driver->cd_name;

	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0)
		return (0);

	config_attach(parent, cf, &oca, vmeprint);
	return (1);
}

void
vmeattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct vmesoftc *sc = (struct vmesoftc *)self;
	struct confargs *ca = args;

	/*
	 * This is a waste if we are attached to SYSCON - but then obio
	 * mappings are free...
	 */
	sc->sc_iot = ca->ca_iot;
	if (bus_space_map(sc->sc_iot, ca->ca_paddr, PAGE_SIZE, 0,
	    &sc->sc_ioh) != 0) {
		printf(": can't map registers!\n");
		return;
	}

	vmebustype = ca->ca_bustype;

	switch (ca->ca_bustype) {
#if NPCCTWO > 0
	case BUS_PCCTWO:
	{
		u_int32_t vbr;

		/* Sanity check that the BUG is set up right */
		vbr = bus_space_read_4(sc->sc_iot, sc->sc_ioh, VME2_VBR);
		vmevecbase = VME2_GET_VBR1(vbr) + 0x10;
		if (vmevecbase >= 0x100) {
			panic("Correct the VME Vector Base Registers "
			    "in the Bug ROM.\n"
			    "Suggested values are 0x60 for VME Vec0 and "
			    "0x70 for VME Vec1.");
		}

		if ((bus_space_read_4(sc->sc_iot, sc->sc_ioh, VME2_TCTL) &
		    VME2_TCTL_SCON) != 0)
			printf(": system controller");
		printf("\n");

		vme2chip_init(sc);
	}
		break;
#endif
#if NSYSCON > 0
	case BUS_SYSCON:
	{
		u_int8_t sconc;

		vmevecbase = 0x80;  /* Hard coded for MVME188 */
		sconc = *(volatile u_int8_t *)GLOBAL1;
		if (!ISSET(sconc, M188_SYSCONNEG))
			printf(": system controller");
		printf("\n");

		vmesyscon_init(sc);
	}
		break;
#endif
	}

	while (config_found(self, NULL, NULL))
		;
}

/* find a VME vector based on what is in NVRAM settings. */
int
vme_findvec(int skip)
{
	return intr_findvec(vmevecbase, 0xff, skip);
}

/*
 * make local addresses 1G-2G correspond to VME addresses 3G-4G,
 * as D32
 */

#define VME2_D32STARTPHYS	(1*1024*1024*1024UL)
#define VME2_D32ENDPHYS		(2*1024*1024*1024UL)
#define VME2_D32STARTVME	(3*1024*1024*1024UL)
#define VME2_D32BITSVME		(3*1024*1024*1024UL)

/*
 * make local addresses 3G-3.75G correspond to VME addresses 3G-3.75G,
 * as D16
 */
#define VME2_D16STARTPHYS	(3*1024*1024*1024UL)
#define VME2_D16ENDPHYS		(3*1024*1024*1024UL + 768*1024*1024UL)
#define VME2_A32D16STARTPHYS	(0xff000000UL)
#define VME2_A32D16ENDPHYS	(0xff7fffffUL)


/*
 * Returns a physical address mapping for a VME address & length.
 * Note: on some hardware it is not possible to create certain
 * mappings, ie. the MVME147 cannot do 32 bit accesses to VME bus
 * addresses from 0 to physmem.
 */
void *
vmepmap(sc, vmeaddr, bustype)
	struct device *sc;
	off_t vmeaddr;
	int bustype;
{
	u_int32_t base = (u_int32_t)vmeaddr;	/* wrap around 4GB */

	switch (vmebustype) {
#if NPCCTWO > 0 || NSYSCON > 0
	case BUS_PCCTWO:
	case BUS_SYSCON:
		switch (bustype) {
		case BUS_VMES:		/* D16 VME Transfers */
#ifdef DEBUG
			printf("base 0x%8llx/0x%8x\n",
			    vmeaddr, base);
#endif
			base = vme2chip_map(base, 16);
#ifdef DEBUG
			if (base == NULL) {
				printf("%s: cannot map pa 0x%x\n",
				    sc->dv_xname, base);
			}
#endif
			break;
		case BUS_VMEL:		/* D32 VME Transfers */
#ifdef DEBUG
			printf("base 0x%8llx/0x%8x\n",
			    vmeaddr, base);
#endif
			base = vme2chip_map(base, 32);
#ifdef DEBUG
			if (base == NULL) {
				printf("%s: cannot map pa 0x%x\n",
				    sc->dv_xname, base);
			}
#endif
			break;
		}
		break;
#endif
	default:
		return NULL;
	}
	return ((void *)base);
}

static void *vmemap(struct vmesoftc *, off_t);
static void vmeunmap(void *);

/* if successful, returns the va of a vme bus mapping */
static __inline__ void *
vmemap(struct vmesoftc *sc, off_t vmeaddr)
{
	void *pa;

	pa = vmepmap((struct device *)sc, vmeaddr, BUS_VMES);
	if (pa == NULL)
		return (NULL);
	return mapiodev(pa, PAGE_SIZE);
}

static __inline__ void
vmeunmap(void *va)
{
	unmapiodev(va, PAGE_SIZE);
}

int
vmerw(sc, uio, flags, bus)
	struct device *sc;
	struct uio *uio;
	int flags;
	int bus;
{
	vaddr_t v;
	int c;
	struct iovec *iov;
	void *vme;
	int error = 0;

	while (uio->uio_resid > 0 && error == 0) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("vmerw");
			continue;
		}

		v = uio->uio_offset;
		c = min(iov->iov_len, MAXPHYS);
		if ((v & PGOFSET) + c > PAGE_SIZE) /* max 1 page at a time */
			c = PAGE_SIZE - (v & PGOFSET);
		if (c == 0)
			return 0;
		vme = vmemap((struct vmesoftc *)sc, v & ~PGOFSET);
		if (vme == NULL)
			return EACCES;
		error = uiomove((void *)vme + (v & PGOFSET), c, uio);
		vmeunmap(vme);
	}
	return error;
}

/*
 * On the VMEbus, only one cpu may be configured to respond to any
 * particular vme ipl. Therefore, it wouldn't make sense to globally
 * enable all the interrupts all the time -- it would not be possible
 * to put two cpu's and one vme card into a single cage. Rather, we
 * enable each vme interrupt only when we are attaching a device that
 * uses it. This makes it easier (though not trivial) to put two cpu
 * cards in one VME cage, and both can have some limited access to vme
 * interrupts (just can't share the same irq).
 * Obviously no check is made to see if another cpu is using that
 * interrupt. If you share you will lose.
 */

/*
 * All VME bus devices will use a vector starting with VBR1 + 0x10
 * and determined by intr_findvec(). (in machdep.c) vmeintr_establish()
 * should be called with the 'vec' argument = 0 to 'auto vector' a
 * VME device.
 *
 * The 8 SW interrupters will start with VBR1.  The rest will start
 * with VBR0< 4) & 0xFF.
 */

int
vmeintr_establish(vec, ih)
	int vec;
	struct intrhand *ih;
{
	struct vmesoftc *sc = (struct vmesoftc *) vme_cd.cd_devs[0];

	switch (vmebustype) {
#if NPCCTWO > 0
	case BUS_PCCTWO:
		bus_space_write_4(sc->sc_iot, sc->sc_ioh, VME2_IRQEN,
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh, VME2_IRQEN) |
		    VME2_IRQ_VME(ih->ih_ipl));
		break;
#endif
	}

	return intr_establish(vec, ih);
}

#if NPCCTWO > 0
void
vme2chip_init(sc)
	struct vmesoftc *sc;
{
	u_int32_t ctl, irqen, master, master4mod;

	/* turn off SYSFAIL LED */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, VME2_TCTL,
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, VME2_TCTL) &
	    ~VME2_TCTL_SYSFAIL);

	/*
	 * Display the VMEChip2 decoder status.
	 */
	printf("%s: using BUG parameters\n", sc->sc_dev.dv_xname);
	ctl = bus_space_read_4(sc->sc_iot, sc->sc_ioh, VME2_GCSRCTL);
	if (ctl & VME2_GCSRCTL_MDEN1) {
		master = bus_space_read_4(sc->sc_iot, sc->sc_ioh, VME2_MASTER1);
		printf("%s: 1phys 0x%08lx-0x%08lx to VME 0x%08lx-0x%08lx\n",
		    sc->sc_dev.dv_xname, master << 16, master & 0xffff0000,
		    master << 16, master & 0xffff0000);
	}
	if (ctl & VME2_GCSRCTL_MDEN2) {
		master = bus_space_read_4(sc->sc_iot, sc->sc_ioh, VME2_MASTER2);
		printf("%s: 2phys 0x%08lx-0x%08lx to VME 0x%08lx-0x%08lx\n",
		    sc->sc_dev.dv_xname, master << 16, master & 0xffff0000,
		    master << 16, master & 0xffff0000);
	}
	if (ctl & VME2_GCSRCTL_MDEN3) {
		master = bus_space_read_4(sc->sc_iot, sc->sc_ioh, VME2_MASTER3);
		printf("%s: 3phys 0x%08lx-0x%08lx to VME 0x%08lx-0x%08lx\n",
		    sc->sc_dev.dv_xname, master << 16, master & 0xffff0000,
		    master << 16, master & 0xffff0000);
	}
	if (ctl & VME2_GCSRCTL_MDEN4) {
		master = bus_space_read_4(sc->sc_iot, sc->sc_ioh, VME2_MASTER4);
		master4mod = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    VME2_MASTER4MOD);
		printf("%s: 4phys 0x%08lx-0x%08lx to VME 0x%08lx-0x%08lx\n",
		    sc->sc_dev.dv_xname, master << 16, master & 0xffff0000,
		    (master << 16) + (master4mod << 16),
		    (master & 0xffff0000) + (master4mod & 0xffff0000));
	}

	/*
	 * Map the VME irq levels to the cpu levels 1:1.
	 * This is rather inflexible, but much easier.
	 */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, VME2_IRQL4,
	    (7 << VME2_IRQL4_VME7SHIFT) | (6 << VME2_IRQL4_VME6SHIFT) |
	    (5 << VME2_IRQL4_VME5SHIFT) | (4 << VME2_IRQL4_VME4SHIFT) |
	    (3 << VME2_IRQL4_VME3SHIFT) | (2 << VME2_IRQL4_VME2SHIFT) |
	    (1 << VME2_IRQL4_VME1SHIFT));
	printf("%s: vme to cpu irq level 1:1\n",sc->sc_dev.dv_xname);

	/* Enable the reset switch */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, VME2_TCTL, VME2_TCTL_RSWE |
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, VME2_TCTL));
	/* Set Watchdog timeout to about 1 minute */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, VME2_TCR, VME2_TCR_64S |
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, VME2_TCR));
	/* Enable VMEChip2 Interrupts */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, VME2_VBR, VME2_IOCTL1_MIEN |
	    bus_space_read_4(sc->sc_iot, sc->sc_ioh, VME2_VBR));

	/*
	 * Map the Software VME irq levels to the cpu level 7.
	*/
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, VME2_IRQL3,
	    (7 << VME2_IRQL3_SW7SHIFT) | (7 << VME2_IRQL3_SW6SHIFT) |
	    (7 << VME2_IRQL3_SW5SHIFT) | (7 << VME2_IRQL3_SW4SHIFT) |
	    (7 << VME2_IRQL3_SW3SHIFT) | (7 << VME2_IRQL3_SW2SHIFT) |
	    (7 << VME2_IRQL3_SW1SHIFT));

	/*
	 * pseudo driver, abort interrupt handler
	 */
	sc->sc_abih.ih_fn = vme2abort;
	sc->sc_abih.ih_arg = 0;
	sc->sc_abih.ih_wantframe = 1;
	sc->sc_abih.ih_ipl = IPL_NMI;
	intr_establish(110, &sc->sc_abih);

	irqen = bus_space_read_4(sc->sc_iot, sc->sc_ioh, VME2_IRQEN);
	irqen |= VME2_IRQ_AB;
	/* bus_space_write_4(sc->sc_iot, sc->sc_ioh, VME2_IRQEN, irqen); */

	/*
	 * Enable ACFAIL interrupt, but disable Timer 1 interrupt - we
	 * prefer it without for delay().
	 */
	irqen = (irqen | VME2_IRQ_ACF) & ~VME2_IRQ_TIC1;
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, VME2_IRQEN, irqen);
}
#endif /* NPCCTWO */

#if NSYSCON > 0
void
vmesyscon_init(sc)
	struct vmesoftc *sc;
{
	/*
	 * Nothing to do - though we ought to check the settings and
	 * print them.  Abort button vector has already been setup in
	 * sysconattach().
	 */
}
#endif /* NSYSCON */

/*
 * A32 accesses on the MVME1[6789]x require setting up mappings in
 * the VME2 chip.
 * XXX VME address must be between 2G and 4G
 * XXX We only support D32 at the moment..
 * XXX smurph - This is bogus, get rid of it! Should check vme/syscon for offsets.
 */
u_long
vme2chip_map(base, dwidth)
	u_long base;
	int dwidth;
{
	/*
	 * Since we are checking range for one page only, no need to check
	 * for address wraparound.
	 */
	switch (dwidth) {
	case 16:
		if (base < VME2_D16STARTPHYS ||
		    base + PAGE_SIZE > VME2_D16ENDPHYS)
			return NULL;
		break;
	case 32:
		if (base < VME2_D32STARTPHYS ||
		    base + PAGE_SIZE > VME2_D32ENDPHYS)
			return NULL;
		break;
	default:
		return NULL;
	}
	return base;
}

#if NPCCTWO > 0
int
vme2abort(eframe)
	void *eframe;
{
	struct frame *frame = eframe;

	struct vmesoftc *sc = (struct vmesoftc *)vme_cd.cd_devs[0];

	if ((bus_space_read_4(sc->sc_iot, sc->sc_ioh, VME2_IRQSTAT) &
	    VME2_IRQ_AB) == 0) {
		printf("%s: abort irq not set\n", sc->sc_dev.dv_xname);
		return (0);
	}

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, VME2_IRQCLR, VME2_IRQ_AB);
	nmihand(frame);
	return (1);
}
#endif
