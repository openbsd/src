/* $OpenBSD: agintc.c,v 1.8 2018/03/30 16:55:20 patrick Exp $ */
/*
 * Copyright (c) 2007, 2009, 2011, 2017 Dale Rahn <drahn@dalerahn.com>
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

/*
 * This is a device driver for the GICv3/GICv4 IP from ARM as specified
 * in IHI0069C, an example of this hardware is the GIC 500.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/evcount.h>

#include <machine/bus.h>
#include <machine/fdt.h>

#include <dev/ofw/fdt.h>
#include <dev/ofw/openfirm.h>

#define ICC_PMR		s3_0_c4_c6_0
#define ICC_IAR0	s3_0_c12_c8_0
#define ICC_EOIR0	s3_0_c12_c8_1
#define ICC_HPPIR0	s3_0_c12_c8_2
#define ICC_BPR0	s3_0_c12_c8_3

#define ICC_DIR		s3_0_c12_c11_1
#define ICC_RPR		s3_0_c12_c11_3
#define ICC_SGI1R	s3_0_c12_c11_5
#define ICC_SGI0R	s3_0_c12_c11_7

#define ICC_IAR1	s3_0_c12_c12_0
#define ICC_EOIR1	s3_0_c12_c12_1
#define ICC_HPPIR1	s3_0_c12_c12_2
#define ICC_BPR1	s3_0_c12_c12_3
#define ICC_CTLR	s3_0_c12_c12_4
#define ICC_SRE_EL1	s3_0_c12_c12_5
#define  ICC_SRE_EL1_EN		0x7
#define ICC_IGRPEN0	s3_0_c12_c12_6
#define ICC_IGRPEN1	s3_0_c12_c12_7

#define _STR(x) #x
#define STR(x) _STR(x)

/* distributor registers */
#define GICD_CTLR		0x0000
/* non-secure */
#define  GICD_CTLR_RWP			(1U << 31)
#define  GICD_CTRL_EnableGrp1		(1 << 0)
#define  GICD_CTRL_EnableGrp1A		(1 << 1)
#define  GICD_CTRL_ARE_NS		(1 << 4)
#define GICD_TYPER		0x0004
#define  GICD_TYPER_ITLINE_M		0xf
#define GICD_IIDR		0x0008
#define GICD_ISENABLER(i)	(0x0100 + (IRQ_TO_REG32(i) * 4))
#define GICD_ICENABLER(i)	(0x0180 + (IRQ_TO_REG32(i) * 4))
#define GICD_ISPENDR(i)		(0x0200 + (IRQ_TO_REG32(i) * 4))
#define GICD_ICPENDR(i)		(0x0280 + (IRQ_TO_REG32(i) * 4))
#define GICD_ISACTIVER(i)	(0x0300 + (IRQ_TO_REG32(i) * 4))
#define GICD_ICACTIVER(i)	(0x0380 + (IRQ_TO_REG32(i) * 4))
#define GICD_IPRIORITYR(i)	(0x0400 + (i))
#define GICD_ICFGR(i)		(0x0c00 + (IRQ_TO_REG16(i) * 4))
#define GICD_IROUTER(i)		(0x6000 + ((i) * 8))

/* redistributor registers */
#define GICR_CTLR		0x00000
#define  GICR_CTLR_RWP			((1U << 31) | (1 << 3))
#define GICR_IIDR		0x00004
#define GICR_TYPER		0x00008
#define  GICR_TYPER_LAST		(1 << 4)
#define  GICR_TYPER_VLPIS		(1 << 1)
#define GICR_WAKER		0x00014
#define  GICR_WAKER_X31			(1U << 31)
#define  GICR_WAKER_CHILDRENASLEEP	(1 << 2)
#define  GICR_WAKER_PROCESSORSLEEP	(1 << 1)
#define  GICR_WAKER_X0			(1 << 0)
#define GICR_IGROUP0		0x10080
#define GICR_ISENABLE0		0x10100
#define GICR_ICENABLE0		0x10180
#define GICR_ISPENDR0		0x10200
#define GICR_ICPENDR0		0x10280
#define GICR_ISACTIVE0		0x10300
#define GICR_ICACTIVE0		0x10380
#define GICR_IPRIORITYR(i)	(0x10400 + (i))
#define GICR_ICFGR0		0x10c00
#define GICR_ICFGR1		0x10c04

#define IRQ_TO_REG32(i)		(((i) >> 5) & 0x7)
#define IRQ_TO_REG32BIT(i)	((i) & 0x1f)

#define IRQ_TO_REG16(i)		(((i) >> 4) & 0xf)
#define IRQ_TO_REG16BIT(i)	((i) & 0xf)

#define IRQ_ENABLE	1
#define IRQ_DISABLE	0

/*
 * This is not a true hard limit, but until bigger machines are supported
 * there is no need for this to be 96+, which the interrupt controller
 * does support. It may make sense to move to dynamic allocation of these 3
 * fields in the future, eg when hardware with 96 cores are supported.
 */
#define MAX_CORES	16

struct agintc_softc {
	struct device		 sc_dev;
	struct intrq		*sc_agintc_handler;
	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_d_ioh;
	bus_space_handle_t	 sc_r_ioh[MAX_CORES];
	bus_space_handle_t	 sc_redist_base;
	uint64_t		 sc_affinity[MAX_CORES];
	int			 sc_cpuremap[MAX_CORES]; /* bsd to redist */
	int			 sc_nintr;
	struct evcount		 sc_spur;
	int			 sc_ncells;
	int			 sc_num_redist;
	struct interrupt_controller sc_ic;
	int			 sc_ipi_num[2]; /* id for NOP and DDB ipi */
	int			 sc_ipi_reason[MAX_CORES]; /* NOP or DDB caused */
	void			*sc_ipi_irq[2]; /* irqhandle for each ipi */
};
struct agintc_softc *agintc_sc;

struct intrhand {
	TAILQ_ENTRY(intrhand)	 ih_list;		/* link on intrq list */
	int			(*ih_func)(void *);	/* handler */
	void			*ih_arg;		/* arg for handler */
	int			 ih_ipl;		/* IPL_* */
	int			 ih_flags;
	int			 ih_irq;		/* IRQ number */
	struct evcount		 ih_count;
	char			*ih_name;
};

struct intrq {
	TAILQ_HEAD(, intrhand)	iq_list;	/* handler list */
	int			iq_irq;		/* IRQ to mask while handling */
	int			iq_levels;	/* IPL_*'s this IRQ has */
	int			iq_ist;		/* share type */
	int			iq_route;
};

int		agintc_match(struct device *, void *, void *);
void		agintc_attach(struct device *, struct device *, void *);
void		agintc_cpuinit(void);
int		agintc_spllower(int);
void		agintc_splx(int);
int		agintc_splraise(int);
void		agintc_setipl(int);
void		agintc_calc_mask(void);
void		agintc_calc_irq(struct agintc_softc	*sc, int irq);
void		*agintc_intr_establish(int, int, int (*)(void *), void *,
		    char *);
void		*agintc_intr_establish_fdt(void *cookie, int *cell, int level,
		    int (*func)(void *), void *arg, char *name);
void		agintc_intr_disestablish(void *);
void		agintc_irq_handler(void *);
uint32_t	agintc_iack(void);
void		agintc_eoi(uint32_t);
void		agintc_set_priority(struct agintc_softc *sc, int, int);
void		agintc_intr_enable(struct agintc_softc *, int);
void		agintc_intr_disable(struct agintc_softc *, int);
void		agintc_route(struct agintc_softc *, int, int,
		    struct cpu_info *);
void		agintc_route_irq(void *, int, struct cpu_info *);
void		agintc_wait_rwp(struct agintc_softc *sc);
void		agintc_r_wait_rwp(struct agintc_softc *sc);
uint32_t	agintc_r_ictlr(void);

int		agintc_ipi_ddb(void *v);
int		agintc_ipi_nop(void *v);
int		agintc_ipi_combined(void *);
void		agintc_send_ipi(struct cpu_info *, int);

struct cfattach	agintc_ca = {
	sizeof (struct agintc_softc), agintc_match, agintc_attach
};

struct cfdriver agintc_cd = {
	NULL, "agintc", DV_DULL
};

static char *agintc_compatibles[] = {
	"arm,gic-v3",
	"arm,gic-v4",
	NULL
};

int
agintc_match(struct device *parent, void *cfdata, void *aux)
{
	struct fdt_attach_args *faa = aux;
	int i;

	for (i = 0; agintc_compatibles[i]; i++)
		if (OF_is_compatible(faa->fa_node, agintc_compatibles[i]))
			return (1);

	return (0);
}

static void
__isb(void)
{
	__asm volatile("isb");
}

void
agintc_attach(struct device *parent, struct device *self, void *aux)
{
	struct agintc_softc	*sc = (struct agintc_softc *)self;
	struct fdt_attach_args	*faa = aux;
	int			 i, j, nintr;
	int			 psw;
	int			 offset, nredist;
	int			 grp1enable;
#ifdef MULTIPROCESSOR
	int			 nipi, ipiirq[2];
#endif

	psw = disable_interrupts();
	arm_init_smask();

	sc->sc_iot = faa->fa_iot;

	/* First row: distributor */
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_d_ioh))
		panic("%s: ICD bus_space_map failed!", __func__);

	/* Second row: redistributor */
	if (bus_space_map(sc->sc_iot, faa->fa_reg[1].addr,
	    faa->fa_reg[1].size, 0, &sc->sc_redist_base))
		panic("%s: ICP bus_space_map failed!", __func__);

	evcount_attach(&sc->sc_spur, "irq1023/spur", NULL);

	__asm volatile("msr "STR(ICC_SRE_EL1)", %x0" : : "r" (ICC_SRE_EL1_EN));
	__isb();

	nintr = 32 * (bus_space_read_4(sc->sc_iot, sc->sc_d_ioh, GICD_TYPER) &
	    GICD_TYPER_ITLINE_M);

	nintr += 32; /* ICD_ICTR + 1, irq 0-31 is SGI, 32+ is PPI */
	sc->sc_nintr = nintr;

	agintc_sc = sc; /* save this for global access */

	/* find and submap the redistributors. */
	offset = 0;
	for (nredist = 0; ; nredist++) {
		int32_t sz = (64 * 1024 * 2);
		uint64_t typer;

		typer = bus_space_read_8(sc->sc_iot, sc->sc_redist_base,
		    offset + GICR_TYPER);

		if (typer & GICR_TYPER_VLPIS)
			sz += (64 * 1024 * 2);

		sc->sc_affinity[nredist] = bus_space_read_8(sc->sc_iot,
		    sc->sc_redist_base, offset + GICR_TYPER) >> 32;

		bus_space_subregion(sc->sc_iot, sc->sc_redist_base,
		    offset, sz, &sc->sc_r_ioh[nredist]);

#ifdef DEBUG_AGINTC
		printf("probing redistributor %d %x %p\n", nredist, offset,
		    sc->sc_r_ioh[nredist]);
#endif

		offset += sz;

		if (typer & GICR_TYPER_LAST) {
			sc->sc_num_redist = nredist + 1;
			break;
		}
	}

	printf(" nirq %d, nredist %d", nintr, sc->sc_num_redist);

	/* Disable all interrupts, clear all pending */
	for (i = 1; i < nintr / 32; i++) {
		bus_space_write_4(sc->sc_iot, sc->sc_d_ioh,
		    GICD_ICENABLER(i * 32), ~0);
		for (j = 0; j < 32; j++) {
			__asm volatile("msr "STR(ICC_DIR)", %x0" : :
			    "r" ((i * 32) + j));
			__isb();
		}
	}

	for (i = 4; i < nintr; i += 4) {
		/* lowest priority ?? */
		bus_space_write_4(sc->sc_iot, sc->sc_d_ioh,
		    GICD_IPRIORITYR(i), 0xffffffff);
	}

	for (i = 2; i < nintr / 16; i++) {
		/* irq 32 - N */
		bus_space_write_4(sc->sc_iot, sc->sc_d_ioh,
		    GICD_ICFGR(i * 16), 0);
	}

	agintc_cpuinit();

	sc->sc_agintc_handler = mallocarray(nintr,
	    sizeof(*sc->sc_agintc_handler), M_DEVBUF, M_ZERO | M_NOWAIT);
	for (i = 0; i < nintr; i++)
		TAILQ_INIT(&sc->sc_agintc_handler[i].iq_list);

	/* set priority to IPL_HIGH until configure lowers to desired IPL */
	agintc_setipl(IPL_HIGH);

	/* initialize all interrupts as disabled */
	agintc_calc_mask();

	/* insert self as interrupt handler */
	arm_set_intr_handler(agintc_splraise, agintc_spllower, agintc_splx,
	    agintc_setipl, agintc_irq_handler);

	/* enable interrupts */
	bus_space_write_4(sc->sc_iot, sc->sc_d_ioh, GICD_CTLR,
	   GICD_CTRL_ARE_NS|GICD_CTRL_EnableGrp1A|GICD_CTRL_EnableGrp1);

	grp1enable = 1;
	__asm volatile("msr "STR(ICC_PMR)", %x0" :: "r"(0xff));
	__asm volatile("msr "STR(ICC_BPR1)", %x0" :: "r"(0));
	__asm volatile("msr "STR(ICC_IGRPEN1)", %x0" :: "r"(grp1enable));

#ifdef MULTIPROCESSOR
	/* setup IPI interrupts */

	/*
	 * Ideally we want two IPI interrupts, one for NOP and one for
	 * DDB, however we can survive if only one is available it is
	 * possible that most are not available to the non-secure OS.
	 */
	nipi = 0;
	for (i = 0; i < 16; i++) {
		int hwcpu = sc->sc_cpuremap[cpu_number()];
		int reg, oldreg;

		oldreg = bus_space_read_1(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_IPRIORITYR(i));
		bus_space_write_1(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_IPRIORITYR(i), oldreg ^ 0x20);

		/* if this interrupt is not usable, pri will be unmodified */
		reg = bus_space_read_1(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_IPRIORITYR(i));
		if (reg == oldreg)
			continue;

		/* return to original value, will be set when used */
		bus_space_write_1(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_IPRIORITYR(i), oldreg);

		if (nipi == 0)
			printf(" ipi: %d", i);
		else
			printf(", %d", i);
		ipiirq[nipi++] = i;
		if (nipi == 2)
			break;
	}

	if (nipi == 0)
		panic("no irq available for IPI");

	switch (nipi) {
	case 1:
		sc->sc_ipi_irq[0] = agintc_intr_establish(ipiirq[0],
		    IPL_IPI|IPL_MPSAFE, agintc_ipi_combined, sc, "ipi");
		sc->sc_ipi_num[ARM_IPI_NOP] = ipiirq[0];
		sc->sc_ipi_num[ARM_IPI_DDB] = ipiirq[0];
		break;
	case 2:
		sc->sc_ipi_irq[0] = agintc_intr_establish(ipiirq[0],
		    IPL_IPI|IPL_MPSAFE, agintc_ipi_nop, sc, "ipinop");
		sc->sc_ipi_num[ARM_IPI_NOP] = ipiirq[0];
		sc->sc_ipi_irq[1] = agintc_intr_establish(ipiirq[1],
		    IPL_IPI|IPL_MPSAFE, agintc_ipi_ddb, sc, "ipiddb");
		sc->sc_ipi_num[ARM_IPI_DDB] = ipiirq[1];
		break;
	default:
		panic("nipi unexpected number %d", nipi);
	}

	intr_send_ipi_func = agintc_send_ipi;
#endif

	printf("\n");

	sc->sc_ic.ic_node = faa->fa_node;
	sc->sc_ic.ic_cookie = self;
	sc->sc_ic.ic_establish = agintc_intr_establish_fdt;
	sc->sc_ic.ic_disestablish = agintc_intr_disestablish;
	sc->sc_ic.ic_route = agintc_route_irq;
	sc->sc_ic.ic_cpu_enable = agintc_cpuinit;
	arm_intr_register_fdt(&sc->sc_ic);

	restore_interrupts(psw);
}

/* Initialize redistributors on each core. */
void
agintc_cpuinit(void)
{
	struct cpu_info *ci = curcpu();
	struct agintc_softc *sc = agintc_sc;
	uint64_t mpidr = READ_SPECIALREG(mpidr_el1);
	uint32_t affinity, waker;
	int timeout = 100000;
	int hwcpu = -1;
	int i;

	/* match this processor to one of the redistributors */
	affinity = (((mpidr >> 8) & 0xff000000) | (mpidr & 0x00ffffff));

	for (i = 0; i < sc->sc_num_redist; i++) {
		if (affinity == sc->sc_affinity[i]) {
			sc->sc_cpuremap[ci->ci_cpuid] = hwcpu = i;
#ifdef DEBUG_AGINTC
			printf("found cpu%d at %d\n", ci->ci_cpuid, i);
#endif
			break;
		}
	}

	if (hwcpu == -1) {
		printf("cpu mpidr not found mpidr %llx affinity %08x\n",
		    mpidr, affinity);
		for (i = 0; i < sc->sc_num_redist; i++)
			printf("rdist%d: %016llx\n", i, sc->sc_affinity[i]);
		panic("failed to indentify cpunumber %d", ci->ci_cpuid);
	}

	waker = bus_space_read_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
	    GICR_WAKER);
	waker &= ~(GICR_WAKER_PROCESSORSLEEP);
	bus_space_write_4(sc->sc_iot, sc->sc_r_ioh[hwcpu], GICR_WAKER,
	    waker);

	do {
		waker = bus_space_read_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_WAKER);
	} while (--timeout && (waker & GICR_WAKER_CHILDRENASLEEP));
	if (timeout == 0)
		printf("%s: waker timed out\n", __func__);

	bus_space_write_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
	    GICR_ICENABLE0, ~0);
	bus_space_write_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
	    GICR_ICPENDR0, ~0);
	bus_space_write_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
	    GICR_ICACTIVE0, ~0);
	for (i = 0; i < 32; i += 4) {
		bus_space_write_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_IPRIORITYR(i), ~0);
	}
	
	if (sc->sc_ipi_irq[0] != NULL)
		agintc_route_irq(sc->sc_ipi_irq[0], IRQ_ENABLE, curcpu());
	if (sc->sc_ipi_irq[1] != NULL)
		agintc_route_irq(sc->sc_ipi_irq[1], IRQ_ENABLE, curcpu());

	__asm volatile("msr "STR(ICC_PMR)", %x0" :: "r"(0xff));
	__asm volatile("msr "STR(ICC_BPR1)", %x0" :: "r"(0));
	__asm volatile("msr "STR(ICC_IGRPEN1)", %x0" :: "r"(1));
	enable_interrupts();
}

void
agintc_set_priority(struct agintc_softc *sc, int irq, int pri)
{
	struct cpu_info	*ci = curcpu();
	int		 hwcpu = sc->sc_cpuremap[ci->ci_cpuid];
	uint32_t	 prival;

	/*
	 * The interrupt priority registers expose the full range of
	 * priorities available in secure mode, and at least bit 3-7
	 * must be implemented.  For non-secure interrupts the top bit
	 * must be one.  We only use 16 (13 really) interrupt
	 * priorities, so shift into bits 3-6.
	 * also low values are higher priority thus NIPL - pri
	 */
	prival = 0x80 | ((NIPL - pri) << 3);
	if (irq >= 32) {
		bus_space_write_1(sc->sc_iot, sc->sc_d_ioh,
		    GICD_IPRIORITYR(irq), prival);
	} else  {
		/* only sets local redistributor */
		bus_space_write_1(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_IPRIORITYR(irq), prival);
	}
}

void
agintc_setipl(int new)
{
	struct cpu_info		*ci = curcpu();
	int			 psw;
	uint32_t		 prival;

	/* disable here is only to keep hardware in sync with ci->ci_cpl */
	psw = disable_interrupts();
	ci->ci_cpl = new;

	/*
	 * The priority mask register only exposes the priorities
	 * available in non-secure mode, so the top bit is hidden.  So
	 * here we shift into bits 4-7.
	 * low values are higher priority thus NIPL - pri
	 */
	if (new == IPL_NONE)
		prival = 0xff;		/* minimum priority */
	else
		prival = ((NIPL - new) << 4);

	__asm volatile("msr "STR(ICC_PMR)", %x0" : : "r" (prival));
	__isb();

	restore_interrupts(psw);
}

void
agintc_intr_enable(struct agintc_softc *sc, int irq)
{
	struct cpu_info	*ci = curcpu();
	int hwcpu = sc->sc_cpuremap[ci->ci_cpuid];
	int bit = 1 << IRQ_TO_REG32BIT(irq);
	uint32_t enable;

	if (irq >= 32) {
		bus_space_write_4(sc->sc_iot, sc->sc_d_ioh,
		    GICD_ISENABLER(irq), bit);
	} else {
		bus_space_write_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_ISENABLE0, bit);
		/* enable group1 as well */
		bus_space_read_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_IGROUP0);
		enable |= 1 << IRQ_TO_REG32BIT(irq);
		bus_space_write_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_IGROUP0, enable);
	}
}

void
agintc_intr_disable(struct agintc_softc *sc, int irq)
{
	struct cpu_info	*ci = curcpu();
	int hwcpu = sc->sc_cpuremap[ci->ci_cpuid];

	if (irq >= 32) {
		bus_space_write_4(sc->sc_iot, sc->sc_d_ioh,
		    GICD_ICENABLER(irq), 1 << IRQ_TO_REG32BIT(irq));
	} else {
		bus_space_write_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_ICENABLE0, 1 << IRQ_TO_REG32BIT(irq));
	}
}

void
agintc_calc_mask(void)
{
	struct agintc_softc	*sc = agintc_sc;
	int			 irq;

	for (irq = 0; irq < sc->sc_nintr; irq++)
		agintc_calc_irq(sc, irq);
}

void
agintc_calc_irq(struct agintc_softc *sc, int irq)
{
	struct cpu_info	*ci = curcpu();
	struct intrhand	*ih;
	int max = IPL_NONE;
	int min = IPL_HIGH;
	
	TAILQ_FOREACH(ih, &sc->sc_agintc_handler[irq].iq_list, ih_list) {
		if (ih->ih_ipl > max)
			max = ih->ih_ipl;

		if (ih->ih_ipl < min)
			min = ih->ih_ipl;
	}

	if (sc->sc_agintc_handler[irq].iq_irq == max)
		return;
	sc->sc_agintc_handler[irq].iq_irq = max;

	if (max == IPL_NONE)
		min = IPL_NONE;

#ifdef DEBUG_AGINTC
	if (min != IPL_NONE)
		printf("irq %d to block at %d %d \n", irq, max, min );
#endif
	/* Enable interrupts at lower levels, clear -> enable */
	/* Set interrupt priority/enable */
	if (min != IPL_NONE) {
		agintc_set_priority(sc, irq, min);
		agintc_route(sc, irq, IRQ_ENABLE, ci);
		agintc_intr_enable(sc, irq);
	} else {
		agintc_intr_disable(sc, irq);
		agintc_route(sc, irq, IRQ_DISABLE, ci);
	}
}

void
agintc_splx(int new)
{
	struct cpu_info *ci = curcpu();

	if (ci->ci_ipending & arm_smask[new])
		arm_do_pending_intr(new);

	agintc_setipl(new);
}

int
agintc_spllower(int new)
{
	struct cpu_info *ci = curcpu();
	int old = ci->ci_cpl;

	agintc_splx(new);
	return (old);
}

int
agintc_splraise(int new)
{
	struct cpu_info	*ci = curcpu();
	int old = ci->ci_cpl;

	/*
	 * setipl must always be called because there is a race window
	 * where the variable is updated before the mask is set
	 * an interrupt occurs in that window without the mask always
	 * being set, the hardware might not get updated on the next
	 * splraise completely messing up spl protection.
	 */
	if (old > new)
		new = old;

	agintc_setipl(new);
	return (old);
}

uint32_t
agintc_iack(void)
{
	int irq;

	__asm volatile("mrs %x0, "STR(ICC_IAR1) : "=r" (irq));
	__asm volatile("dsb sy");
	return irq;
}

void
agintc_route_irq(void *v, int enable, struct cpu_info *ci)
{
	struct agintc_softc	*sc = agintc_sc;
	struct intrhand		*ih = v;

	if (enable) {
		agintc_set_priority(sc, ih->ih_irq,
		    sc->sc_agintc_handler[ih->ih_irq].iq_irq);
		agintc_route(sc, ih->ih_irq, IRQ_ENABLE, ci);
		agintc_intr_enable(sc, ih->ih_irq);
	}
}

void
agintc_route(struct agintc_softc *sc, int irq, int enable, struct cpu_info *ci)
{
	uint64_t  val;

	/* XXX does not yet support 'participating node' */
	if (irq >= 32) {
		val = ((sc->sc_affinity[ci->ci_cpuid] & 0x00ffffff) |
		    ((sc->sc_affinity[ci->ci_cpuid] & 0xff000000) << 8));
#ifdef DEBUG_AGINTC
		printf("router %x irq %d val %016llx\n", GICD_IROUTER(irq),irq,
		    val);
#endif
		bus_space_write_8(sc->sc_iot, sc->sc_d_ioh,
		    GICD_IROUTER(irq), val);
	}
}

void
agintc_irq_handler(void *frame)
{
	struct cpu_info		*ci = curcpu();
	struct agintc_softc	*sc = agintc_sc;
	struct intrhand		*ih;
	void			*arg;
	int			 irq, pri, s, handled;

	ci->ci_idepth++;
	irq = agintc_iack();

#ifdef DEBUG_AGINTC
	if (irq != 30)
		printf("irq  %d fired\n", irq);
	else {
		static int cnt = 0;
		if ((cnt++ % 100) == 0) {
			printf("irq  %d fired * _100\n", irq);
#ifdef DDB
			db_enter();
#endif
		}
	}
#endif

	if (irq == 1023) {
		sc->sc_spur.ec_count++;
		ci->ci_idepth--;
		return;
	}

	if (irq >= sc->sc_nintr) {
		ci->ci_idepth--;
		return;
	}

	pri = sc->sc_agintc_handler[irq].iq_irq;
	s = agintc_splraise(pri);
	TAILQ_FOREACH(ih, &sc->sc_agintc_handler[irq].iq_list, ih_list) {
#ifdef MULTIPROCESSOR
		int need_lock;

		if (ih->ih_flags & IPL_MPSAFE)
			need_lock = 0;
		else
			need_lock = s < IPL_SCHED;

		if (need_lock)
			KERNEL_LOCK();
#endif

		if (ih->ih_arg != 0)
			arg = ih->ih_arg;
		else
			arg = frame;

		enable_interrupts();
		handled = ih->ih_func(arg);
		disable_interrupts();
		if (handled)
			ih->ih_count.ec_count++;

#ifdef MULTIPROCESSOR
		if (need_lock)
			KERNEL_UNLOCK();
#endif
	}
	agintc_eoi(irq);

	agintc_splx(s);
	ci->ci_idepth--;
}

void *
agintc_intr_establish_fdt(void *cookie, int *cell, int level,
    int (*func)(void *), void *arg, char *name)
{
	struct agintc_softc	*sc = agintc_sc;
	int			 irq;

	/* 2nd cell contains the interrupt number */
	irq = cell[1];

	/* 1st cell contains type: 0 SPI (32-X), 1 PPI (16-31) */
	if (cell[0] == 0)
		irq += 32;
	else if (cell[0] == 1)
		irq += 16;
	else
		panic("%s: bogus interrupt type", sc->sc_dev.dv_xname);

	return agintc_intr_establish(irq, level, func, arg, name);
}

void *
agintc_intr_establish(int irqno, int level, int (*func)(void *),
    void *arg, char *name)
{
	struct agintc_softc	*sc = agintc_sc;
	struct intrhand		*ih;
	int			 psw;

	if (irqno < 0 || irqno >= sc->sc_nintr)
		panic("agintc_intr_establish: bogus irqnumber %d: %s",
		    irqno, name);

	ih = malloc(sizeof *ih, M_DEVBUF, M_WAITOK);
	ih->ih_func = func;
	ih->ih_arg = arg;
	ih->ih_ipl = level & IPL_IRQMASK;
	ih->ih_flags = level & IPL_FLAGMASK;
	ih->ih_irq = irqno;
	ih->ih_name = name;

	psw = disable_interrupts();

	TAILQ_INSERT_TAIL(&sc->sc_agintc_handler[irqno].iq_list, ih, ih_list);

	if (name != NULL)
		evcount_attach(&ih->ih_count, name, &ih->ih_irq);

#ifdef DEBUG_AGINTC
	printf("%s: irq %d level %d [%s]\n", __func__, irqno, level, name);
#endif

	agintc_calc_irq(sc, irqno);

	restore_interrupts(psw);
	return (ih);
}

void
agintc_intr_disestablish(void *cookie)
{
	struct agintc_softc	*sc = agintc_sc;
	struct intrhand		*ih = cookie;
	int			 irqno = ih->ih_irq;
	int			 psw;

	psw = disable_interrupts();

	TAILQ_REMOVE(&sc->sc_agintc_handler[irqno].iq_list, ih, ih_list);
	if (ih->ih_name != NULL)
		evcount_detach(&ih->ih_count);

	agintc_calc_irq(sc, irqno);

	restore_interrupts(psw);

	free(ih, M_DEVBUF, 0);
}

void
agintc_eoi(uint32_t eoi)
{
	__asm volatile("msr "STR(ICC_EOIR1)", %x0" :: "r" (eoi));
	__isb();
}

uint32_t
agintc_r_ictlr(void)
{
	int ictlr;

	__asm volatile("mrs %x0, "STR(ICC_CTLR) : "=r" (ictlr));
	__isb();
	return ictlr;
}

void
agintc_d_wait_rwp(struct agintc_softc *sc)
{
	int count = 100000;
	uint32_t v;

	do {
		v = bus_space_read_4(sc->sc_iot, sc->sc_d_ioh, GICD_CTLR);
	} while (--count && (v & GICD_CTLR_RWP));

	if (count == 0)
		panic("%s: RWP timed out 0x08%x", __func__, v);
}

void
agintc_r_wait_rwp(struct agintc_softc *sc)
{
	struct cpu_info *ci = curcpu();
	int hwcpu = sc->sc_cpuremap[ci->ci_cpuid];
	int count = 100000;
	uint32_t v;

	do {
		v = bus_space_read_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_CTLR);
	} while (--count && (v & GICR_CTLR_RWP));

	if (count == 0)
		panic("%s: RWP timed out 0x08%x", __func__, v);
}

#ifdef MULTIPROCESSOR
int
agintc_ipi_ddb(void *v)
{
	/* XXX */
	db_enter();
	return 1;
}

int
agintc_ipi_nop(void *v)
{
	/* Nothing to do here, just enough to wake up from WFI */
	return 1;
}

int
agintc_ipi_combined(void *v)
{
	struct agintc_softc *sc = v;

	if (sc->sc_ipi_reason[cpu_number()] == ARM_IPI_DDB) {
		sc->sc_ipi_reason[cpu_number()] = ARM_IPI_NOP;
		return agintc_ipi_ddb(v);
	} else {
		return agintc_ipi_nop(v);
	}
}

void
agintc_send_ipi(struct cpu_info *ci, int id)
{
	struct agintc_softc	*sc = agintc_sc;
	uint64_t sendmask;

	if (ci == curcpu() && id == ARM_IPI_NOP)
		return;

	/* never overwrite IPI_DDB with IPI_NOP */
	if (id == ARM_IPI_DDB)
		sc->sc_ipi_reason[ci->ci_cpuid] = id;

	/* will only send 1 cpu */
	sendmask = (sc->sc_affinity[ci->ci_cpuid]  & 0xff000000) << 48;
	sendmask |= (sc->sc_affinity[ci->ci_cpuid] & 0x00ffff00) << 8;
	sendmask |= 1 << (sc->sc_affinity[ci->ci_cpuid] & 0x0000000f);
	sendmask |= (sc->sc_ipi_num[id] << 24);

	__asm volatile ("msr " STR(ICC_SGI1R)", %x0" ::"r"(sendmask));
}
#endif
