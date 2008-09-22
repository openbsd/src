/*	$OpenBSD: mbus.c,v 1.2 2008/09/22 19:45:35 miod Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/nexus.h>
#include <machine/scb.h>

#include <vax/mbus/mbusreg.h>
#include <vax/mbus/mbusvar.h>

#define	PR_CPUID 14

/*
 * FBIC interrupt handlers
 */
struct fbic_irq {
	int	(*fi_fn)(void *);
	void	*fi_arg;
	struct evcount fi_cnt;
	int	fi_ipl;
};

/*
 * Generic information for each slot.
 *
 * This information is maintained at the mbus level, rather than
 * enforcing each child driver to provide it.  This allows proper
 * M-bus configuration on slots where no driver attaches.
 */

struct	fbic {
	paddr_t		base;
	vaddr_t		regs;
	int		vecbase;
	struct fbic_irq	*firq[FBIC_DEVIRQMAX];
};

struct mbus_slot {
	uint8_t		ms_interface;	/* MODTYPE interface */
	uint8_t		ms_class;	/* MODTYPE class */

	unsigned int	ms_nfbic;
	struct fbic	ms_fbic[2];
};

struct mbus_softc {
	struct device	sc_dev;
	struct mbus_slot *sc_slots[MBUS_SLOT_MAX];
};

void	mbus_attach(struct device *, struct device *, void *);
int	mbus_match(struct device *, void *, void *);

struct cfdriver mbus_cd = {
	NULL, "mbus", DV_DULL
};

const struct cfattach mbus_ca = {
	sizeof(struct mbus_softc), mbus_match, mbus_attach
};

void	mbus_initialize_cpu(struct mbus_slot *, unsigned int, int);
void	mbus_initialize_device(struct mbus_slot *, unsigned int, uint8_t);
void	mbus_intr_dispatch(void *);
int	mbus_print(void *, const char *);
int	mbus_submatch(struct device *, void *, void *);

unsigned int mbus_ioslot = (unsigned int)-1;

int
mbus_match(struct device *parent, void *vcf, void *aux)
{
	struct mainbus_attach_args *maa = (struct mainbus_attach_args *)aux;

	return maa->maa_bustype == VAX_MBUS ? 1 : 0;
}

void
mbus_attach(struct device *parent, struct device *self, void *aux)
{
	struct mbus_softc *sc = (struct mbus_softc *)self;
	struct mbus_slot *ms;
	unsigned int mid;
	struct mbus_attach_args maa;
	paddr_t pa;
	vaddr_t fbic;
	uint32_t modtype;
	uint8_t class, interface;

	printf("\n");

	/*
	 * Walk the bus and probe slots.
	 * We will also record information about all occupied slots,
	 * and keep a permanent mapping of their FBIC, as we will end
	 * up needing to play with them often...
	 */

	for (mid = 0; mid < MBUS_SLOT_MAX; mid++) {

		/*
		 * Map main (and often, only) FBIC.
		 */

		pa = MBUS_SLOT_BASE(mid);
		fbic = vax_map_physmem(pa + FBIC_BASE, 1);
		if (fbic == NULL)
			panic("unable to map slot %d registers", mid);

		if (badaddr((caddr_t)(fbic + FBIC_MODTYPE), 4) != 0)
			modtype = 0;
		else
			modtype = *(uint32_t *)(fbic + FBIC_MODTYPE);

		if (modtype == 0 || modtype == 0xffffffff) {
			vax_unmap_physmem(fbic, 1);
			continue;
		}

		/*
		 * The slot is populated.  Write things down.
		 */

		ms = (struct mbus_slot *)malloc(sizeof(*ms),
		    M_DEVBUF, M_NOWAIT | M_ZERO);
		if (ms == NULL)
			panic("not enough memory to probe M-bus");

		sc->sc_slots[mid] = ms;
		ms->ms_nfbic = 1;	/* only one so far! */
		ms->ms_fbic[0].base = pa + FBIC_BASE;
		ms->ms_fbic[0].regs = fbic;

		class = (modtype & MODTYPE_CLASS_MASK) >> MODTYPE_CLASS_SHIFT;
		interface = (modtype & MODTYPE_INTERFACE_MASK) >>
		    MODTYPE_INTERFACE_SHIFT;

		ms->ms_interface = interface;
		ms->ms_class = class;

		/*
		 * If there are two FBICs on this board, map the second one.
		 */

		if (class == CLASS_CPU) {
			/* the FBIC we mapped is in fact the second one... */
			ms->ms_fbic[1].base = ms->ms_fbic[0].base;
			ms->ms_fbic[1].regs = ms->ms_fbic[0].regs;
			ms->ms_nfbic = 2;
			fbic = vax_map_physmem(pa + FBIC_CPUA_BASE, 1);
			if (fbic == NULL)
				panic("unable to map slot %d registers", mid);
			ms->ms_fbic[0].base = pa + FBIC_CPUA_BASE;
			ms->ms_fbic[0].regs = fbic;
		}

		/*
		 * Perform a minimal sane initialization.
		 */

		if (class == CLASS_CPU) {
			mbus_initialize_cpu(ms, mid, 0);
			mbus_initialize_cpu(ms, mid, 1);
		} else
			mbus_initialize_device(ms, mid, interface);

		/*
		 * Attach subdevices if possible.
		 */

		maa.maa_mid = mid;
		maa.maa_class = class;
		maa.maa_subclass = (modtype & MODTYPE_SUBCLASS_MASK) >>
		    MODTYPE_SUBCLASS_SHIFT;
		maa.maa_interface = interface;
		maa.maa_revision = (modtype & MODTYPE_REVISION_MASK) >>
		    MODTYPE_REVISION_SHIFT;
		maa.maa_addr = pa;
		maa.maa_vecbase = ms->ms_fbic[0].vecbase;

		(void)config_found_sm(self, &maa, mbus_print, mbus_submatch);
	}
}

int
mbus_print(void *aux, const char *pnp)
{
	struct mbus_attach_args *maa = (struct mbus_attach_args *)aux;
	int rc = UNCONF;
	const char *descr;

	switch (maa->maa_class) {
	case CLASS_BA:
		descr = "Bus Adaptor";
		break;
	case CLASS_GRAPHICS:
		descr = "Graphics";
		break;
	case CLASS_IO:
		descr = "I/O Module";
		break;
	case CLASS_CPU:
		descr = "cpu";
		break;
	case CLASS_MEMORY:
		descr = maa->maa_interface == INTERFACE_FMDC ?
		    "ECC memory" : "Memory";
		break;
	default:
		descr = NULL;
		rc = UNSUPP;
		break;
	}

	if (maa->maa_interface != INTERFACE_FBIC) {
		if (maa->maa_class != CLASS_MEMORY ||
		    (maa->maa_interface != INTERFACE_FMCM &&
		     maa->maa_interface != INTERFACE_FMDC))
			rc = UNSUPP;
	}

	if (pnp != NULL) {
		if (rc == UNSUPP) {
			printf("logic board class %02x:%02x interface %u.%u ",
			    maa->maa_class, maa->maa_subclass,
			    maa->maa_interface, maa->maa_revision);
			if (descr != NULL)
				printf("(%s)", descr);
		} else
			printf("%s", descr);
		printf(" at %s", pnp);
	}
	printf(" mid %u", maa->maa_mid);

	return (rc);
}

int
mbus_submatch(struct device *parent, void *vcf, void *aux)
{
	struct mbus_attach_args *maa = (struct mbus_attach_args *)aux;
	struct cfdata *cf = (struct cfdata *)vcf;

	/*
	 * If attachment specifies the mid, it has to match.
	 */
	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != maa->maa_mid)
		return 0;

	return (*cf->cf_attach->ca_match)(parent, vcf, aux);
}

/*
 * CPU board initialization.
 */

void
mbus_initialize_cpu(struct mbus_slot *ms, unsigned int mid, int cpu)
{
	struct fbic *fbic = &ms->ms_fbic[cpu];
	uint32_t fbicsr;
	int cpuid;

	cpuid = (mid << CPUID_MID_SHIFT) |
	    (cpu != 0 ? CPUID_PROC_1 : CPUID_PROC_0);

	/*
	 * Clear error log
	 */
	*(uint32_t *)(fbic->regs + FBIC_BUSCSR) = BUSCSR_RESET;

	/*
	 * Set (IPI) interrupt vectors base, but do not enable them yet.
	 */
	fbic->vecbase = MBUS_VECTOR_BASE(mid, cpu);
	*(uint32_t *)(fbic->regs + FBIC_IPDVINT) = 0 /* IPDVINT_IPUNIT */ |
	    (fbic->vecbase & IPDVINT_VECTOR_MASK);

	/*
	 * Enable all interrupt sources if on the boot processor,
	 * disable them otherwise (this does not affect IPIs).
	 */
	fbicsr = *(uint32_t *)(fbic->regs + FBIC_BUSCSR);
	if (cpuid == mfpr(PR_CPUID))
		fbicsr |= FBICSR_IRQEN_MASK;
	else
		fbicsr &= ~FBICSR_IRQEN_MASK;

	/*
	 * Route interrupts from the M-bus to the CVAX.
	 */
	fbicsr &= ~FBICSR_IRQC2M_MASK;

	/*
	 * Allow the CPU to be halted.
	 */
	fbicsr |= FBICSR_HALTEN;

	*(uint32_t *)(fbic->regs + FBIC_BUSCSR) = fbicsr;
}

/*
 * Device board initialization.
 */

void
mbus_initialize_device(struct mbus_slot *ms, unsigned int mid,
    uint8_t interface)
{
	struct fbic *fbic = ms->ms_fbic;
	uint32_t fbicsr;

	/*
	 * Clear error log if applicable
	 */
	if (interface == INTERFACE_FBIC || interface == INTERFACE_FMDC)
		*(uint32_t *)(fbic->regs + FBIC_BUSCSR) = BUSCSR_RESET;

	if (interface == INTERFACE_FBIC) {
		/*
		 * Set interrupt vector base.
		 */
		fbic->vecbase = MBUS_VECTOR_BASE(mid, 0);
		*(uint32_t *)(fbic->regs + FBIC_IPDVINT) = IPDVINT_DEVICE |
		    (fbic->vecbase & IPDVINT_VECTOR_MASK);

		/*
		 * Disable all interrupt sources, and route them
		 * from the devices to the M-bus.
		 */
		fbicsr = *(uint32_t *)(fbic->regs + FBIC_BUSCSR);
		fbicsr &= ~FBICSR_IRQEN_MASK;
		fbicsr |= FBICSR_IRQC2M_MASK;
		*(uint32_t *)(fbic->regs + FBIC_BUSCSR) = fbicsr;
	}
}

/*
 * Interrupt handling.
 */

int
mbus_intr_establish(unsigned int vec, int ipl, int (*fn)(void *), void *arg,
    const char *name)
{
	struct mbus_softc *sc;
	struct mbus_slot *ms;
	struct fbic *fbic;
	struct fbic_irq *fi;
	uint32_t fbicsr;
	unsigned int mid, fbicirq;

	mid = MBUS_VECTOR_TO_MID(vec);

#ifdef DIAGNOSTIC
	if (mid >= MBUS_SLOT_MAX)
		return EINVAL;
	if (mbus_cd.cd_ndevs == 0)
		return ENXIO;
#endif
	sc = (struct mbus_softc *)mbus_cd.cd_devs[0];
#ifdef DIAGNOSTIC
	if (sc == NULL)
		return ENXIO;
#endif
	ms = sc->sc_slots[mid];
#ifdef DIAGNOSTIC
	if (ms == NULL)
		return ENXIO;
#endif
	fi = (struct fbic_irq *)malloc(sizeof *fi, M_DEVBUF, M_NOWAIT);
	if (fi == NULL)
		return ENOMEM;

	/*
	 * This interface is intended to be used for device interrupts
	 * only, so there is no need to handle dual-FBIC slots.
	 */
	fbic = &ms->ms_fbic[0 /* MBUS_VECTOR_TO_FBIC(vec) */];

	fi->fi_fn = fn;
	fi->fi_arg = arg;
	fi->fi_ipl = ipl;
	evcount_attach(&fi->fi_cnt, name, &fi->fi_ipl, &evcount_intr);

	fbicirq = MBUS_VECTOR_TO_IRQ(vec);
	fbic->firq[fbicirq] = fi;
	scb_vecalloc(vec, mbus_intr_dispatch, fi, SCB_ISTACK, &fi->fi_cnt);

	/*
	 * Enable device interrupt in the module FBIC.  Proper direction
	 * has been setup in mbus_slot_initialize().
	 */

	fbicsr = *(uint32_t *)(fbic->regs + FBIC_BUSCSR);
	fbicsr |= fbicirq << FBICSR_IRQEN_SHIFT;
	*(uint32_t *)(fbic + FBIC_BUSCSR) = fbicsr;

	return 0;
}

/*
 * Interrupt dispatcher.
 */

void
mbus_intr_dispatch(void *v)
{
	struct fbic_irq *fi = (struct fbic_irq *)v;
	int s;

	/*
	 * FBIC interrupts are at fixed levels.  In case the level is
	 * below the level the driver expects the interrupt at, we need
	 * to raise spl to be safe (e.g. for sii).
	 */
	s = _splraise(fi->fi_ipl);
	(void)(*fi->fi_fn)(fi->fi_arg);
	splx(s);
}
