/* $OpenBSD: agintc.c,v 1.15 2018/12/07 21:33:28 patrick Exp $ */
/*
 * Copyright (c) 2007, 2009, 2011, 2017 Dale Rahn <drahn@dalerahn.com>
 * Copyright (c) 2018 Mark Kettenis <kettenis@openbsd.org>
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
#include <machine/cpufunc.h>
#include <machine/fdt.h>

#include <dev/ofw/fdt.h>
#include <dev/ofw/openfirm.h>

#include <arm64/dev/simplebusvar.h>

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
#define  GICD_TYPER_LPIS		(1 << 16)
#define  GICD_TYPER_ITLINE_M		0x1f
#define GICD_IIDR		0x0008
#define GICD_ISENABLER(i)	(0x0100 + (IRQ_TO_REG32(i) * 4))
#define GICD_ICENABLER(i)	(0x0180 + (IRQ_TO_REG32(i) * 4))
#define GICD_ISPENDR(i)		(0x0200 + (IRQ_TO_REG32(i) * 4))
#define GICD_ICPENDR(i)		(0x0280 + (IRQ_TO_REG32(i) * 4))
#define GICD_ISACTIVER(i)	(0x0300 + (IRQ_TO_REG32(i) * 4))
#define GICD_ICACTIVER(i)	(0x0380 + (IRQ_TO_REG32(i) * 4))
#define GICD_IPRIORITYR(i)	(0x0400 + (i))
#define GICD_ICFGR(i)		(0x0c00 + (IRQ_TO_REG16(i) * 4))
#define GICD_NSACR(i)		(0x0e00 + (IRQ_TO_REG16(i) * 4))
#define GICD_IROUTER(i)		(0x6000 + ((i) * 8))

/* redistributor registers */
#define GICR_CTLR		0x00000
#define  GICR_CTLR_RWP			((1U << 31) | (1 << 3))
#define  GICR_CTLR_ENABLE_LPIS		(1 << 0)
#define GICR_IIDR		0x00004
#define GICR_TYPER		0x00008
#define  GICR_TYPER_LAST		(1 << 4)
#define  GICR_TYPER_VLPIS		(1 << 1)
#define GICR_WAKER		0x00014
#define  GICR_WAKER_X31			(1U << 31)
#define  GICR_WAKER_CHILDRENASLEEP	(1 << 2)
#define  GICR_WAKER_PROCESSORSLEEP	(1 << 1)
#define  GICR_WAKER_X0			(1 << 0)
#define GICR_PROPBASER		0x00070
#define  GICR_PROPBASER_ISH		(1ULL << 10)
#define  GICR_PROPBASER_IC_NORM_NC	(1ULL << 7)
#define GICR_PENDBASER		0x00078
#define  GICR_PENDBASER_PTZ		(1ULL << 62)
#define  GICR_PENDBASER_ISH		(1ULL << 10)
#define  GICR_PENDBASER_IC_NORM_NC	(1ULL << 7)
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

#define GICR_PROP_SIZE		(64 * 1024)
#define  GICR_PROP_GROUP1	(1 << 1)
#define  GICR_PROP_ENABLE	(1 << 0)
#define GICR_PEND_SIZE		(64 * 1024)

#define PPI_BASE		16
#define SPI_BASE		32
#define LPI_BASE		8192

#define IRQ_TO_REG32(i)		(((i) >> 5) & 0x7)
#define IRQ_TO_REG32BIT(i)	((i) & 0x1f)

#define IRQ_TO_REG16(i)		(((i) >> 4) & 0xf)
#define IRQ_TO_REG16BIT(i)	((i) & 0xf)

#define IRQ_ENABLE	1
#define IRQ_DISABLE	0

struct agintc_softc {
	struct simplebus_softc	 sc_sbus;
	struct intrq		*sc_handler;
	struct intrhand		**sc_lpi_handler;
	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_d_ioh;
	bus_space_handle_t	*sc_r_ioh;
	bus_space_handle_t	 sc_redist_base;
	bus_dma_tag_t		 sc_dmat;
	uint64_t		*sc_affinity;
	int			 sc_cpuremap[MAXCPUS];
	int			 sc_nintr;
	int			 sc_nlpi;
	int			 sc_rk3399_quirk;
	struct evcount		 sc_spur;
	int			 sc_ncells;
	int			 sc_num_redist;
	struct agintc_dmamem	*sc_prop;
	struct agintc_dmamem	*sc_pend;
	struct interrupt_controller sc_ic;
	int			 sc_ipi_num[2]; /* id for NOP and DDB ipi */
	int			 sc_ipi_reason[MAXCPUS]; /* NOP or DDB caused */
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
	int			iq_irq_max;	/* IRQ to mask while handling */
	int			iq_irq_min;	/* lowest IRQ when shared */
	int			iq_ist;		/* share type */
	int			iq_route;
};

struct agintc_dmamem {
	bus_dmamap_t		adm_map;
	bus_dma_segment_t	adm_seg;
	size_t			adm_size;
	caddr_t			adm_kva;
};

#define AGINTC_DMA_MAP(_adm)	((_adm)->adm_map)
#define AGINTC_DMA_LEN(_adm)	((_adm)->adm_size)
#define AGINTC_DMA_DVA(_adm)	((_adm)->adm_map->dm_segs[0].ds_addr)
#define AGINTC_DMA_KVA(_adm)	((void *)(_adm)->adm_kva)

struct agintc_dmamem *agintc_dmamem_alloc(bus_dma_tag_t, bus_size_t,
		    bus_size_t);
void		agintc_dmamem_free(bus_dma_tag_t, struct agintc_dmamem *);

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
	uint32_t		 typer;
	uint32_t		 nsacr, oldnsacr;
	uint32_t		 ctrl, bits;
	int			 i, j, nintr;
	int			 psw;
	int			 offset, nredist;
#ifdef MULTIPROCESSOR
	int			 nipi, ipiirq[2];
#endif

	psw = disable_interrupts();
	arm_init_smask();

	sc->sc_iot = faa->fa_iot;
	sc->sc_dmat = faa->fa_dmat;

	/* First row: distributor */
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_d_ioh))
		panic("%s: ICD bus_space_map failed!", __func__);

	/* Second row: redistributor */
	if (bus_space_map(sc->sc_iot, faa->fa_reg[1].addr,
	    faa->fa_reg[1].size, 0, &sc->sc_redist_base))
		panic("%s: ICP bus_space_map failed!", __func__);

	typer = bus_space_read_4(sc->sc_iot, sc->sc_d_ioh, GICD_TYPER);

	if (typer & GICD_TYPER_LPIS) {
		/* Allocate redistributor tables */
		sc->sc_prop = agintc_dmamem_alloc(sc->sc_dmat,
		    GICR_PROP_SIZE, GICR_PROP_SIZE);
		if (sc->sc_prop == NULL) {
			printf(": can't alloc LPI config table\n");
			goto unmap;
		}
		sc->sc_pend = agintc_dmamem_alloc(sc->sc_dmat,
		    GICR_PEND_SIZE, GICR_PEND_SIZE);
		if (sc->sc_prop == NULL) {
			printf(": can't alloc LPI pending table\n");
			goto unmap;
		}

		/* Minimum number of LPIs supported by any implementation. */
		sc->sc_nlpi = 8192;
	}

	/*
	 * The Rockchip RK3399 is busted.  Its GIC-500 treats all
	 * access to its memory mapped registers as "secure".  As a
	 * result, several registers don't behave as expected.  For
	 * example, the GICD_IPRIORITYRn and GICR_IPRIORITYRn
	 * registers expose the full priority range available to
	 * secure interrupts.  We need to be aware of this and write
	 * an adjusted priority value into these registers.  We also
	 * need to be careful not to touch any bits that shouldn't be
	 * writable in non-secure mode.
	 *
	 * We check whether we have secure mode access to these
	 * registers by attempting to write to the GICD_NSACR register
	 * and check whether its contents actually change.
	 */
	oldnsacr = bus_space_read_4(sc->sc_iot, sc->sc_d_ioh, GICD_NSACR(32));
	bus_space_write_4(sc->sc_iot, sc->sc_d_ioh, GICD_NSACR(32),
	    oldnsacr ^ 0xffffffff);
	nsacr = bus_space_read_4(sc->sc_iot, sc->sc_d_ioh, GICD_NSACR(32));
	if (nsacr != oldnsacr) {
		bus_space_write_4(sc->sc_iot, sc->sc_d_ioh, GICD_NSACR(32),
		    oldnsacr);
		sc->sc_rk3399_quirk = 1;
	}

	evcount_attach(&sc->sc_spur, "irq1023/spur", NULL);

	__asm volatile("msr "STR(ICC_SRE_EL1)", %x0" : : "r" (ICC_SRE_EL1_EN));
	__isb();

	nintr = 32 * (typer & GICD_TYPER_ITLINE_M);
	nintr += 32; /* ICD_ICTR + 1, irq 0-31 is SGI, 32+ is PPI */
	sc->sc_nintr = nintr;

	agintc_sc = sc; /* save this for global access */

	/* find the redistributors. */
	offset = 0;
	for (nredist = 0; ; nredist++) {
		int32_t sz = (64 * 1024 * 2);
		uint64_t typer;

		typer = bus_space_read_8(sc->sc_iot, sc->sc_redist_base,
		    offset + GICR_TYPER);

		if (typer & GICR_TYPER_VLPIS)
			sz += (64 * 1024 * 2);

#ifdef DEBUG_AGINTC
		printf("probing redistributor %d %x\n", nredist, offset);
#endif

		offset += sz;

		if (typer & GICR_TYPER_LAST) {
			sc->sc_num_redist = nredist + 1;
			break;
		}
	}

	printf(" nirq %d, nredist %d", nintr, sc->sc_num_redist);
	
	sc->sc_r_ioh = mallocarray(sc->sc_num_redist,
	    sizeof(*sc->sc_r_ioh), M_DEVBUF, M_WAITOK);
	sc->sc_affinity = mallocarray(sc->sc_num_redist,
	    sizeof(*sc->sc_affinity), M_DEVBUF, M_WAITOK);

	/* submap and configure the redistributors. */
	offset = 0;
	for (nredist = 0; nredist < sc->sc_num_redist; nredist++) {
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

		if (sc->sc_nlpi > 0) {
			bus_space_write_8(sc->sc_iot, sc->sc_redist_base,
			    offset + GICR_PROPBASER,
			    AGINTC_DMA_DVA(sc->sc_prop) |
			    GICR_PROPBASER_ISH | GICR_PROPBASER_IC_NORM_NC |
			    fls(LPI_BASE + sc->sc_nlpi - 1) - 1);
			bus_space_write_8(sc->sc_iot, sc->sc_redist_base,
			    offset + GICR_PENDBASER,
			    AGINTC_DMA_DVA(sc->sc_pend) |
			    GICR_PENDBASER_ISH | GICR_PENDBASER_IC_NORM_NC |
			    GICR_PENDBASER_PTZ);
			bus_space_write_4(sc->sc_iot, sc->sc_redist_base,
			    offset + GICR_CTLR, GICR_CTLR_ENABLE_LPIS);
		}

		offset += sz;
	}

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

	sc->sc_handler = mallocarray(nintr,
	    sizeof(*sc->sc_handler), M_DEVBUF, M_ZERO | M_WAITOK);
	for (i = 0; i < nintr; i++)
		TAILQ_INIT(&sc->sc_handler[i].iq_list);
	sc->sc_lpi_handler = mallocarray(sc->sc_nlpi,
	    sizeof(*sc->sc_lpi_handler), M_DEVBUF, M_ZERO | M_WAITOK);

	/* set priority to IPL_HIGH until configure lowers to desired IPL */
	agintc_setipl(IPL_HIGH);

	/* initialize all interrupts as disabled */
	agintc_calc_mask();

	/* insert self as interrupt handler */
	arm_set_intr_handler(agintc_splraise, agintc_spllower, agintc_splx,
	    agintc_setipl, agintc_irq_handler);

	/* enable interrupts */
	ctrl = bus_space_read_4(sc->sc_iot, sc->sc_d_ioh, GICD_CTLR);
	bits = GICD_CTRL_ARE_NS | GICD_CTRL_EnableGrp1A | GICD_CTRL_EnableGrp1;
	if (sc->sc_rk3399_quirk) {
		bits &= ~GICD_CTRL_EnableGrp1A;
		bits <<= 1;
	}
	bus_space_write_4(sc->sc_iot, sc->sc_d_ioh, GICD_CTLR, ctrl | bits);

	__asm volatile("msr "STR(ICC_PMR)", %x0" :: "r"(0xff));
	__asm volatile("msr "STR(ICC_BPR1)", %x0" :: "r"(0));
	__asm volatile("msr "STR(ICC_IGRPEN1)", %x0" :: "r"(1));

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

	sc->sc_ic.ic_node = faa->fa_node;
	sc->sc_ic.ic_cookie = self;
	sc->sc_ic.ic_establish = agintc_intr_establish_fdt;
	sc->sc_ic.ic_disestablish = agintc_intr_disestablish;
	sc->sc_ic.ic_route = agintc_route_irq;
	sc->sc_ic.ic_cpu_enable = agintc_cpuinit;
	arm_intr_register_fdt(&sc->sc_ic);

	restore_interrupts(psw);

	/* Attach ITS. */
	simplebus_attach(parent, &sc->sc_sbus.sc_dev, faa);

	return;

unmap:
	if (sc->sc_r_ioh) {
		free(sc->sc_r_ioh, M_DEVBUF,
		    sc->sc_num_redist * sizeof(*sc->sc_r_ioh));
	}
	if (sc->sc_affinity) {
		free(sc->sc_affinity, M_DEVBUF,
		     sc->sc_num_redist * sizeof(*sc->sc_affinity));
	}

	if (sc->sc_pend)
		agintc_dmamem_free(sc->sc_dmat, sc->sc_pend);
	if (sc->sc_prop)
		agintc_dmamem_free(sc->sc_dmat, sc->sc_prop);

	bus_space_unmap(sc->sc_iot, sc->sc_redist_base, faa->fa_reg[1].size);
	bus_space_unmap(sc->sc_iot, sc->sc_d_ioh, faa->fa_reg[0].size);
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
	 * The interrupt priority registers only expose the priorities
	 * available in non-secure mode, so the top bit is hidden.  So
	 * here we shift into bits 4-7.
	 * Also low values are higher priority thus NIPL - pri
	 */
	prival = ((NIPL - pri) << 4);
	if (sc->sc_rk3399_quirk)
		prival = 0x80 | (prival >> 1);

	if (irq >= SPI_BASE) {
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
	 * The priority mask register exposes the full range of
	 * priorities available in secure mode, and at least bit 3-7
	 * must be implemented.  For non-secure interrupts the top bit
	 * must be one.  We only use 16 (13 really) interrupt
	 * priorities, so shift into bits 3-6.
	 * Low values are higher priority thus NIPL - pri
	 */
	if (new == IPL_NONE)
		prival = 0xff;		/* minimum priority */
	else
		prival = 0x80 | ((NIPL - new) << 3);

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

	if (irq >= 32) {
		bus_space_write_4(sc->sc_iot, sc->sc_d_ioh,
		    GICD_ISENABLER(irq), bit);
	} else {
		bus_space_write_4(sc->sc_iot, sc->sc_r_ioh[hwcpu],
		    GICR_ISENABLE0, bit);
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
	
	TAILQ_FOREACH(ih, &sc->sc_handler[irq].iq_list, ih_list) {
		if (ih->ih_ipl > max)
			max = ih->ih_ipl;

		if (ih->ih_ipl < min)
			min = ih->ih_ipl;
	}

	if (max == IPL_NONE)
		min = IPL_NONE;

	if (sc->sc_handler[irq].iq_irq_max == max &&
	    sc->sc_handler[irq].iq_irq_min == min)
		return;

	sc->sc_handler[irq].iq_irq_max = max;
	sc->sc_handler[irq].iq_irq_min = min;

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
		    sc->sc_handler[ih->ih_irq].iq_irq_min);
		agintc_route(sc, ih->ih_irq, IRQ_ENABLE, ci);
		agintc_intr_enable(sc, ih->ih_irq);
	}
}

void
agintc_route(struct agintc_softc *sc, int irq, int enable, struct cpu_info *ci)
{
	/* XXX does not yet support 'participating node' */
	if (irq >= 32) {
#ifdef DEBUG_AGINTC
		printf("router %x irq %d val %016llx\n", GICD_IROUTER(irq),
		    irq, ci->ci_mpidr & MPIDR_AFF);
		    val);
#endif
		bus_space_write_8(sc->sc_iot, sc->sc_d_ioh,
		    GICD_IROUTER(irq), ci->ci_mpidr & MPIDR_AFF);
	}
}

void
agintc_run_handler(struct intrhand *ih, void *frame, int s)
{
	void *arg;
	int handled;

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

void
agintc_irq_handler(void *frame)
{
	struct cpu_info		*ci = curcpu();
	struct agintc_softc	*sc = agintc_sc;
	struct intrhand		*ih;
	int			 irq, pri, s;

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

	if ((irq >= sc->sc_nintr && irq < LPI_BASE) ||
	    irq >= LPI_BASE + sc->sc_nlpi) {
		ci->ci_idepth--;
		return;
	}

	if (irq >= LPI_BASE) {
		ih = sc->sc_lpi_handler[irq - LPI_BASE];
		if (ih == NULL) {
			ci->ci_idepth--;
			return;
		}
		
		s = agintc_splraise(ih->ih_ipl);
		agintc_run_handler(ih, frame, s);
		agintc_eoi(irq);

		agintc_splx(s);
		ci->ci_idepth--;
		return;
	}

	pri = sc->sc_handler[irq].iq_irq_max;
	s = agintc_splraise(pri);
	TAILQ_FOREACH(ih, &sc->sc_handler[irq].iq_list, ih_list) {
		agintc_run_handler(ih, frame, s);
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
		irq += SPI_BASE;
	else if (cell[0] == 1)
		irq += PPI_BASE;
	else
		panic("%s: bogus interrupt type", sc->sc_sbus.sc_dev.dv_xname);

	return agintc_intr_establish(irq, level, func, arg, name);
}

void *
agintc_intr_establish(int irqno, int level, int (*func)(void *),
    void *arg, char *name)
{
	struct agintc_softc	*sc = agintc_sc;
	struct intrhand		*ih;
	int			 psw;

	if (irqno < 0 || (irqno >= sc->sc_nintr && irqno < LPI_BASE) ||
	    irqno >= LPI_BASE + sc->sc_nlpi)
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

	if (irqno < LPI_BASE)
		TAILQ_INSERT_TAIL(&sc->sc_handler[irqno].iq_list, ih, ih_list);
	else
		sc->sc_lpi_handler[irqno - LPI_BASE] = ih;

	if (name != NULL)
		evcount_attach(&ih->ih_count, name, &ih->ih_irq);

#ifdef DEBUG_AGINTC
	printf("%s: irq %d level %d [%s]\n", __func__, irqno, level, name);
#endif

	if (irqno < LPI_BASE) {
		agintc_calc_irq(sc, irqno);
	} else {
		uint8_t *prop = AGINTC_DMA_KVA(sc->sc_prop);

		prop[irqno - LPI_BASE] = ((NIPL - ih->ih_ipl) << 4) |
		    GICR_PROP_GROUP1 | GICR_PROP_ENABLE;

		/* Make globally visible. */
		cpu_dcache_wb_range((vaddr_t)prop, 1);
		__asm volatile("dsb sy");
	}

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

	TAILQ_REMOVE(&sc->sc_handler[irqno].iq_list, ih, ih_list);
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
	sendmask = (ci->ci_mpidr & MPIDR_AFF3) << 16;
	sendmask |= (ci->ci_mpidr & MPIDR_AFF2) << 16;
	sendmask |= (ci->ci_mpidr & MPIDR_AFF1) << 8;
	sendmask |= 1 << (ci->ci_mpidr & 0x0f);
	sendmask |= (sc->sc_ipi_num[id] << 24);

	__asm volatile ("msr " STR(ICC_SGI1R)", %x0" ::"r"(sendmask));
}
#endif

/*
 * GICv3 ITS controller for MSI interrupts.
 */
#define GITS_CTLR		0x0000
#define  GITS_CTLR_ENABLED	(1UL << 0)
#define GITS_TYPER		0x0008
#define  GITS_TYPER_CIL		(1ULL << 36)
#define  GITS_TYPER_HCC(x)	(((x) >> 24) & 0xff)
#define  GITS_TYPER_PTA		(1ULL << 19)
#define  GITS_TYPER_ITE_SZ(x)	(((x) >> 4) & 0xf)
#define  GITS_TYPER_PHYS	(1ULL << 0)
#define GITS_CBASER		0x0080
#define  GITS_CBASER_VALID	(1ULL << 63)
#define  GITS_CBASER_IC_NORM_NC	(1ULL << 59)
#define  GITS_CBASER_MASK	0x1ffffffffff000ULL
#define GITS_CWRITER		0x0088
#define GITS_CREADR		0x0090
#define GITS_BASER(i)		(0x0100 + ((i) * 8))
#define  GITS_BASER_VALID	(1ULL << 63)
#define  GITS_BASER_INDIRECT	(1ULL << 62)
#define  GITS_BASER_IC_NORM_NC	(1ULL << 59)
#define  GITS_BASER_TYPE_MASK	(7ULL << 56)
#define  GITS_BASER_TYPE_DEVICE	(1ULL << 56)
#define  GITS_BASER_MASK	0x7ffffffff000ULL
#define GITS_TRANSLATER		0x10040

struct gits_cmd {
	uint8_t cmd;
	uint32_t deviceid;
	uint32_t eventid;
	uint32_t intid;
	uint64_t dw2;
	uint64_t dw3;
};

#define GITS_CMD_VALID		(1ULL << 63)

/* ITS commands */
#define SYNC	0x05
#define MAPD	0x08
#define MAPC	0x09
#define MAPTI	0x0a

#define GITS_CMDQ_SIZE		(64 * 1024)
#define GITS_CMDQ_NENTRIES	(GITS_CMDQ_SIZE / sizeof(struct gits_cmd))

#define GITS_DTT_SIZE		(64 * 1024)

struct agintc_msi_device {
	LIST_ENTRY(agintc_msi_device) md_list;

	uint32_t		md_deviceid;
	uint32_t		md_eventid;
	struct agintc_dmamem	*md_itt;
};

int	 agintc_msi_match(struct device *, void *, void *);
void	 agintc_msi_attach(struct device *, struct device *, void *);
void	*agintc_intr_establish_msi(void *, uint64_t *, uint64_t *,
	    int , int (*)(void *), void *, char *);
void	 agintc_intr_disestablish_msi(void *);

struct agintc_msi_softc {
	struct device			sc_dev;
	bus_space_tag_t			sc_iot;
	bus_space_handle_t		sc_ioh;
	bus_dma_tag_t			sc_dmat;

	bus_addr_t			sc_msi_addr;
	int				sc_msi_delta;

	int				sc_nlpi;
	void				**sc_lpi;

	struct agintc_dmamem		*sc_cmdq;
	uint16_t			sc_cmdidx;
	struct agintc_dmamem		*sc_dtt;
	uint8_t				sc_ite_sz;

	LIST_HEAD(, agintc_msi_device)	sc_msi_devices;

	struct interrupt_controller	sc_ic;
};

struct cfattach	agintcmsi_ca = {
	sizeof (struct agintc_msi_softc), agintc_msi_match, agintc_msi_attach
};

struct cfdriver agintcmsi_cd = {
	NULL, "agintcmsi", DV_DULL
};

void	agintc_msi_send_cmd(struct agintc_msi_softc *, struct gits_cmd *);
void	agintc_msi_wait_cmd(struct agintc_msi_softc *);

int
agintc_msi_match(struct device *parent, void *cfdata, void *aux)
{
	struct fdt_attach_args *faa = aux;

	return OF_is_compatible(faa->fa_node, "arm,gic-v3-its");
}

void
agintc_msi_attach(struct device *parent, struct device *self, void *aux)
{
	struct agintc_msi_softc *sc = (struct agintc_msi_softc *)self;
	struct fdt_attach_args *faa = aux;
	struct gits_cmd cmd;
	uint32_t pre_its[2];
	uint64_t typer;
	int i;

	if (faa->fa_nreg < 1) {
		printf(": no registers\n");
		return;
	}

	sc->sc_iot = faa->fa_iot;
	if (bus_space_map(sc->sc_iot, faa->fa_reg[0].addr,
	    faa->fa_reg[0].size, 0, &sc->sc_ioh)) {
		printf(": can't map registers\n");
		return;
	}
	sc->sc_dmat = faa->fa_dmat;

	sc->sc_msi_addr = faa->fa_reg[0].addr + GITS_TRANSLATER;
	if (OF_getpropintarray(faa->fa_node, "socionext,synquacer-pre-its",
	    pre_its, sizeof(pre_its)) == sizeof(pre_its)) {
		sc->sc_msi_addr = pre_its[0];
		sc->sc_msi_delta = 4;
	}

	typer = bus_space_read_8(sc->sc_iot, sc->sc_ioh, GITS_TYPER);
	if ((typer & GITS_TYPER_PHYS) == 0 || typer & GITS_TYPER_PTA ||
	    GITS_TYPER_HCC(typer) == 0 || typer & GITS_TYPER_CIL) {
		printf(": unsupported type 0x%016llx\n", typer);
		goto unmap;
	}
	sc->sc_ite_sz = GITS_TYPER_ITE_SZ(typer) + 1;

	sc->sc_nlpi = agintc_sc->sc_nlpi;
	sc->sc_lpi = mallocarray(sc->sc_nlpi, sizeof(void *), M_DEVBUF,
	    M_WAITOK|M_ZERO);

	/* Set up command queue. */
	sc->sc_cmdq = agintc_dmamem_alloc(sc->sc_dmat,
	    GITS_CMDQ_SIZE, GITS_CMDQ_SIZE);
	if (sc->sc_cmdq == NULL) {
		printf(": can't alloc command queue\n");
		goto unmap;
	}
	bus_space_write_8(sc->sc_iot, sc->sc_ioh, GITS_CBASER,
	    AGINTC_DMA_DVA(sc->sc_cmdq) | GITS_CBASER_IC_NORM_NC |
	    (GITS_CMDQ_SIZE / PAGE_SIZE) - 1 | GITS_CBASER_VALID);

	/* Set up device translation table. */
	for (i = 0; i < 8; i++) {
		uint64_t baser;

		baser = bus_space_read_8(sc->sc_iot, sc->sc_ioh, GITS_BASER(i));
		if ((baser & GITS_BASER_TYPE_MASK) != GITS_BASER_TYPE_DEVICE)
			continue;

		sc->sc_dtt = agintc_dmamem_alloc(sc->sc_dmat,
		    GITS_DTT_SIZE, GITS_DTT_SIZE);
		if (sc->sc_dtt == NULL) {
			printf(": can't alloc translation table\n");
			goto unmap;
		}
		bus_space_write_8(sc->sc_iot, sc->sc_ioh, GITS_BASER(i),
		    AGINTC_DMA_DVA(sc->sc_dtt) | GITS_BASER_IC_NORM_NC |
		    (GITS_DTT_SIZE / PAGE_SIZE) - 1 | GITS_BASER_VALID);
	}

	/* Enable ITS. */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GITS_CTLR,
	    GITS_CTLR_ENABLED);

	LIST_INIT(&sc->sc_msi_devices);

	/* Map collection 0 to redistributor 0. */
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = MAPC;
	cmd.dw2 = GITS_CMD_VALID;
	agintc_msi_send_cmd(sc, &cmd);
	agintc_msi_wait_cmd(sc);

	printf("\n");

	sc->sc_ic.ic_node = faa->fa_node;
	sc->sc_ic.ic_cookie = sc;
	sc->sc_ic.ic_establish_msi = agintc_intr_establish_msi;
	sc->sc_ic.ic_disestablish = agintc_intr_disestablish_msi;
	arm_intr_register_fdt(&sc->sc_ic);
	return;

unmap:
	if (sc->sc_dtt)
		agintc_dmamem_free(sc->sc_dmat, sc->sc_dtt);
	if (sc->sc_cmdq)
		agintc_dmamem_free(sc->sc_dmat, sc->sc_cmdq);

	if (sc->sc_lpi)
		free(sc->sc_lpi, M_DEVBUF, sc->sc_nlpi * sizeof(void *));

	bus_space_unmap(sc->sc_iot, sc->sc_ioh, faa->fa_reg[0].size);
}

void
agintc_msi_send_cmd(struct agintc_msi_softc *sc, struct gits_cmd *cmd)
{
	struct gits_cmd *queue = AGINTC_DMA_KVA(sc->sc_cmdq);

	memcpy(&queue[sc->sc_cmdidx], cmd, sizeof(*cmd));

	/* Make globally visible. */
	cpu_dcache_wb_range((vaddr_t)&queue[sc->sc_cmdidx], sizeof(*cmd));
	__asm volatile("dsb sy");

	sc->sc_cmdidx++;
	sc->sc_cmdidx %= GITS_CMDQ_NENTRIES;
	bus_space_write_8(sc->sc_iot, sc->sc_ioh, GITS_CWRITER,
	    sc->sc_cmdidx * sizeof(*cmd));
}

void
agintc_msi_wait_cmd(struct agintc_msi_softc *sc)
{
	uint64_t creadr;
	int timo;

	for (timo = 1000; timo > 0; timo--) {
		creadr = bus_space_read_8(sc->sc_iot, sc->sc_ioh, GITS_CREADR);
		if (creadr == sc->sc_cmdidx * sizeof(struct gits_cmd))
			break;
		delay(1);
	}
	if (timo == 0)
		printf("%s: command queue timeout\n", sc->sc_dev.dv_xname);
}

struct agintc_msi_device *
agintc_msi_create_device(struct agintc_msi_softc *sc, uint32_t deviceid)
{
	struct agintc_msi_device *md;
	struct gits_cmd cmd;

	md = malloc(sizeof(*md), M_DEVBUF, M_ZERO | M_WAITOK);
	md->md_deviceid = deviceid;
	md->md_itt = agintc_dmamem_alloc(sc->sc_dmat,
	    32 * sc->sc_ite_sz, PAGE_SIZE);
	LIST_INSERT_HEAD(&sc->sc_msi_devices, md, md_list);

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = MAPD;
	cmd.deviceid = deviceid;
	cmd.eventid = 4;	/* size */
	cmd.dw2 = AGINTC_DMA_DVA(md->md_itt) | GITS_CMD_VALID;
	agintc_msi_send_cmd(sc, &cmd);
	agintc_msi_wait_cmd(sc);

	return md;
}

struct agintc_msi_device *
agintc_msi_find_device(struct agintc_msi_softc *sc, uint32_t deviceid)
{
	struct agintc_msi_device *md;

	LIST_FOREACH(md, &sc->sc_msi_devices, md_list) {
		if (md->md_deviceid == deviceid)
			return md;
	}

	return agintc_msi_create_device(sc, deviceid);
}

void *
agintc_intr_establish_msi(void *self, uint64_t *addr, uint64_t *data,
    int level, int (*func)(void *), void *arg, char *name)
{
	struct agintc_msi_softc *sc = (struct agintc_msi_softc *)self;
	struct agintc_msi_device *md;
	struct gits_cmd cmd;
	uint32_t deviceid = *data;
	uint32_t eventid;
	void *cookie;
	int i;

	md = agintc_msi_find_device(sc, deviceid);
	if (md == NULL)
		return NULL;

	eventid = md->md_eventid++;
	if (eventid >= 32)
		return NULL;

	for (i = 0; i < sc->sc_nlpi; i++) {
		if (sc->sc_lpi[i] != NULL)
			continue;

		cookie = agintc_intr_establish(LPI_BASE + i,
		    level, func, arg, name);
		if (cookie == NULL)
			return NULL;

		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd = MAPTI;
		cmd.deviceid = deviceid;
		cmd.eventid = eventid;
		cmd.intid = LPI_BASE + i;
		cmd.dw2 = GITS_CMD_VALID;
		agintc_msi_send_cmd(sc, &cmd);

		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd = SYNC;
		cmd.dw2 = 0;
		agintc_msi_send_cmd(sc, &cmd);
		agintc_msi_wait_cmd(sc);

		*addr = sc->sc_msi_addr + deviceid * sc->sc_msi_delta;
		*data = eventid;
		sc->sc_lpi[i] = cookie;
		return &sc->sc_lpi[i];
	}

	return NULL;
}

void
agintc_intr_disestablish_msi(void *cookie)
{
	agintc_intr_disestablish(*(void **)cookie);
	*(void **)cookie = NULL;
}

struct agintc_dmamem *
agintc_dmamem_alloc(bus_dma_tag_t dmat, bus_size_t size, bus_size_t align)
{
	struct agintc_dmamem *adm;
	int nsegs;

	adm = malloc(sizeof(*adm), M_DEVBUF, M_WAITOK | M_ZERO);
	adm->adm_size = size;

	if (bus_dmamap_create(dmat, size, 1, size, 0,
	    BUS_DMA_WAITOK | BUS_DMA_ALLOCNOW, &adm->adm_map) != 0)
		goto admfree;

	if (bus_dmamem_alloc(dmat, size, align, 0, &adm->adm_seg, 1,
	    &nsegs, BUS_DMA_WAITOK | BUS_DMA_ZERO) != 0)
		goto destroy;

	if (bus_dmamem_map(dmat, &adm->adm_seg, nsegs, size,
	    &adm->adm_kva, BUS_DMA_WAITOK | BUS_DMA_NOCACHE) != 0)
		goto free;

	if (bus_dmamap_load_raw(dmat, adm->adm_map, &adm->adm_seg,
	    nsegs, size, BUS_DMA_WAITOK) != 0)
		goto unmap;

	/* Make globally visible. */
	cpu_dcache_wb_range((vaddr_t)adm->adm_kva, size);
	__asm volatile("dsb sy");
	return adm;

unmap:
	bus_dmamem_unmap(dmat, adm->adm_kva, size);
free:
	bus_dmamem_free(dmat, &adm->adm_seg, 1);
destroy:
	bus_dmamap_destroy(dmat, adm->adm_map);
admfree:
	free(adm, M_DEVBUF, sizeof(*adm));

	return NULL;
}

void
agintc_dmamem_free(bus_dma_tag_t dmat, struct agintc_dmamem *adm)
{
	bus_dmamem_unmap(dmat, adm->adm_kva, adm->adm_size);
	bus_dmamem_free(dmat, &adm->adm_seg, 1);
	bus_dmamap_destroy(dmat, adm->adm_map);
	free(adm, M_DEVBUF, sizeof(*adm));
}
