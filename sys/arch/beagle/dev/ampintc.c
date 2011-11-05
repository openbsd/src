/* $OpenBSD: ampintc.c,v 1.1 2011/11/05 11:48:26 drahn Exp $ */
/*
 * Copyright (c) 2007,2009,2011 Dale Rahn <drahn@openbsd.org>
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
 * This driver implements the interrupt controller as specified in 
 * DDI0407E_cortex_a9_mpcore_r2p0_trm with the 
 * IHI0048A_gic_architecture_spec_v1_0 underlying specification
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/evcount.h>
#include <arm/cpufunc.h>
#include <machine/bus.h>
#include <arch/beagle/beagle/ahb.h>

/* registers */
#define	ICD_DCR			0x000
#define		ICD_DCR_ES		0x00000001
#define		ICD_DCR_ENS		0x00000002

#define ICD_ICTR			0x004
#define		ICD_ICTR_LSPI_SH	11
#define		ICD_ICTR_LSPI_M		0x1f
#define		ICD_ICTR_CPU_SH		5
#define		ICD_ICTR_CPU_M		0x07
#define		ICD_ICTR_ITL_SH		0
#define		ICD_ICTR_ITL_M		0x1f
#define ICD_IDIR			0x008
#define 	ICD_DIR_PROD_SH		24
#define 	ICD_DIR_PROD_M		0xff
#define 	ICD_DIR_REV_SH		12
#define 	ICD_DIR_REV_M		0xfff
#define 	ICD_DIR_IMP_SH		0
#define 	ICD_DIR_IMP_M		0xfff

#define IRQ_TO_REG32(i)		(((i) >> 5) & 0x7)
#define IRQ_TO_REG32BIT(i)	((i) & 0x1f)
#define IRQ_TO_REG4(i)		(((i) >> 2) & 0x3f)
#define IRQ_TO_REG4BIT(i)	((i) & 0x3)
#define IRQ_TO_REG16(i)		(((i) >> 4) & 0xf)
#define IRQ_TO_REGBIT_S(i)	8
#define IRQ_TO_REG4BIT_M(i)	8

#define ICD_ISRn(i)		(0x080 + (IRQ_TO_REG32(i) * 4))
#define ICD_ISERn(i)		(0x100 + (IRQ_TO_REG32(i) * 4))
#define ICD_ICERn(i)		(0x180 + (IRQ_TO_REG32(i) * 4))
#define ICD_ISPRn(i)		(0x200 + (IRQ_TO_REG32(i) * 4))
#define ICD_ICPRn(i)		(0x280 + (IRQ_TO_REG32(i) * 4))
#define ICD_ABRn(i)		(0x300 + (IRQ_TO_REG32(i) * 4))
#define ICD_IPRn(i)		(0x400 + (i))
#define ICD_IPTRn(i)		(0x800 + (IRQ_TO_REG4(i) * 4))
#define ICD_ICRn(i)		(0xC00 + (IRQ_TO_REG16(i) / 4))
/*
 * what about (ppi|spi)_status
 */
#define ICD_PPI			0xD00
#define 	ICD_PPI_GTIMER	(1 << 11)
#define 	ICD_PPI_FIQ		(1 << 12)
#define 	ICD_PPI_PTIMER	(1 << 13)
#define 	ICD_PPI_PWDOG	(1 << 14)
#define 	ICD_PPI_IRQ		(1 << 15)
#define ICD_SPI_BASE		0xD04
#define ICD_SPIn(i)			(ICD_SPI_BASE + ((i) * 4))


#define ICD_SGIR			0xF00

#define ICD_PERIPH_ID_0			0xFD0
#define ICD_PERIPH_ID_1			0xFD4
#define ICD_PERIPH_ID_2			0xFD8
#define ICD_PERIPH_ID_3			0xFDC
#define ICD_PERIPH_ID_4			0xFE0
#define ICD_PERIPH_ID_5			0xFE4
#define ICD_PERIPH_ID_6			0xFE8
#define ICD_PERIPH_ID_7			0xFEC

#define ICD_COMP_ID_0			0xFEC
#define ICD_COMP_ID_1			0xFEC
#define ICD_COMP_ID_2			0xFEC
#define ICD_COMP_ID_3			0xFEC

#define ICD_SIZE 			0x1000


#define ICPICR				0x00
#define ICPIPMR				0x04
/* XXX - must left justify bits to  0 - 7  */
#define 	ICMIPMR_SH 		4
#define ICPBPR				0x08
#define ICPIAR				0x0C
#define 	ICPIAR_IRQ_SH		0
#define 	ICPIAR_IRQ_M		0x3ff
#define 	ICPIAR_CPUID_SH		10
#define 	ICPIAR_CPUID_M		0x7
#define 	ICPIAR_NO_PENDING_IRQ	ICPIAR_IRQ_M
#define ICPEOIR				0x10
#define ICPPRP				0x14
#define ICPHPIR				0x18
#define ICPIIR				0xFC
#define ICP_SIZE			0x100
/*
 * what about periph_id and component_id 
 */

#define AMPAMPINTC_SIZE			0x1000

#define NIRQ			256

struct ampintc_softc {
	struct device		 sc_dev;
	struct intrq 		*sc_ampintc_handler;
	int			 sc_nintr;
	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_d_ioh, sc_p_ioh;
};
struct ampintc_softc *ampintc;


struct intrhand {
	TAILQ_ENTRY(intrhand) ih_list;	/* link on intrq list */
	int (*ih_func)(void *);		/* handler */
	void *ih_arg;			/* arg for handler */
	int ih_ipl;			/* IPL_* */
	int ih_irq;			/* IRQ number */
	struct evcount	ih_count;
	char *ih_name;
};

struct intrq {
	TAILQ_HEAD(, intrhand) iq_list;	/* handler list */
	int iq_irq;			/* IRQ to mask while handling */
	int iq_levels;			/* IPL_*'s this IRQ has */
	int iq_ist;			/* share type */
};


int		 ampintc_match(struct device *, void *, void *);
void		 ampintc_attach(struct device *, struct device *, void *);
int		 ampintc_spllower(int);
void		 ampintc_splx(int);
int		 ampintc_splraise(int);
void		 ampintc_setipl(int);
void		 ampintc_calc_mask(void);
void		*ampintc_intr_establish(int, int, int (*)(void *), void *,
		    char *);
void		 ampintc_intr_disestablish(void *);
void		 ampintc_irq_handler(void *);
const char	*ampintc_intr_string(void *);
uint32_t	 ampintc_iack(void);
void		 ampintc_eoi(uint32_t);
void		 ampintc_set_priority(int, int);
void		 ampintc_intr_enable(int);
void		 ampintc_intr_disable(int);

struct cfattach	ampintc_ca = {
	sizeof (struct ampintc_softc), ampintc_match, ampintc_attach
};

struct cfdriver ampintc_cd = {
	NULL, "ampintc", DV_DULL
};

int ampintc_attached = 0;

int
ampintc_match(struct device *parent, void *v, void *aux)
{
	printf("ampintc_match board %d\n", board_id);
	switch (board_id) {
	case BOARD_ID_OMAP3_BEAGLE:
		return 0; /* not ported yet ??? - different */
	case BOARD_ID_OMAP4_PANDA:
		printf("Yea?\n");
		break; /* continue trying */
	default:
		return 0; /* unknown */
	}
		printf("Yea1\n");
	if (ampintc_attached != 0)
		return 0;
	printf("Yea2\n");

	/* XXX */
	return (1);
}


void
ampintc_attach(struct device *parent, struct device *self, void *args)
{
        struct ahb_attach_args *aa = args;
	struct ampintc_softc *sc = (struct ampintc_softc *)self;
	int i, nintr;
	bus_space_tag_t		iot;
	bus_space_handle_t	d_ioh, p_ioh;

	ampintc = sc;

	arm_init_smask();

#define ICD_ADDR 0x48241000
#define ICP_ADDR 0x48240100
	iot = aa->aa_iot;
	if (bus_space_map(iot, ICD_ADDR, ICD_SIZE, 0, &d_ioh))
		panic("ampintc_attach: bus_space_map failed!");

	if (bus_space_map(iot, ICP_ADDR, ICP_SIZE, 0, &p_ioh))
		panic("ampintc_attach: bus_space_map failed!");

	sc->sc_d_ioh = d_ioh;
	sc->sc_p_ioh = p_ioh;

	nintr = 32 * (bus_space_read_4(iot, d_ioh, ICD_ICTR) & ICD_ICTR_ITL_M);
	nintr += 32; /* ICD_ICTR + 1, irq 0-31 is SGI, 32+ is PPI */
	sc->sc_nintr = nintr;
	printf(" nirq %d\n", nintr);
	if (nintr > NIRQ) {
		panic("ampintc needs NIRQ bumped to at least %d", nintr);
		/*
		 * or should this code just dynamically allocate 
		 * ampintc_handler[]
		 */
	}
	printf("periph_id 0 %x\n",
	    bus_space_read_1(iot, d_ioh, ICD_PERIPH_ID_0));
	printf("periph_id 1 %x\n",
	    bus_space_read_1(iot, d_ioh, ICD_PERIPH_ID_1));
	printf("periph_id 2 %x\n",
	    bus_space_read_1(iot, d_ioh, ICD_PERIPH_ID_2));
	printf("periph_id 3 %x\n",
	    bus_space_read_1(iot, d_ioh, ICD_PERIPH_ID_3));
	printf("periph_id 4 %x\n",
	    bus_space_read_1(iot, d_ioh, ICD_PERIPH_ID_4));

	/* Disable all interrupts, clear all pending */
	for (i = 0; i < nintr/32; i++) {
		bus_space_write_4(iot, d_ioh, ICD_ICERn(i*32), ~0);
		bus_space_write_4(iot, d_ioh, ICD_ICPRn(i*32), ~0);
	}
	for (i = 0; i < nintr/4; i++) {

		/* lowest priority ?? */
		bus_space_write_4(iot, d_ioh, ICD_IPRn(i*32), 0xffffffff);
		/* target no cpus */
		bus_space_write_4(iot, d_ioh, ICD_IPTRn(i*32), 0);
	}
	for (i = 2; i < nintr/15; i++) {
		/* irq 32 - N */
		bus_space_write_4(iot, d_ioh, ICD_ICRn(i*32), 0);
	}

	/* software reset of the part? */
	/* set protection bit (kernel only)? */

	/* XXX - check power saving bit */


	sc->sc_ampintc_handler = malloc(
	    (sizeof (*sc->sc_ampintc_handler) * nintr),
	    M_DEVBUF, M_ZERO);
	for (i = 0; i < nintr; i++) {

		TAILQ_INIT(&sc->sc_ampintc_handler[i].iq_list);
	}

	ampintc_calc_mask();

	ampintc_attached = 1;

	/* insert self as interrupt handler */
	arm_set_intr_handler(ampintc_splraise, ampintc_spllower, ampintc_splx,
	    ampintc_setipl, ampintc_intr_establish, ampintc_intr_disestablish,
	    ampintc_intr_string, ampintc_irq_handler);

	ampintc_setipl(IPL_HIGH);  /* XXX ??? */
	enable_interrupts(I32_bit);
}

void
ampintc_set_priority(int irq, int pri)
{
        struct ampintc_softc	*sc = ampintc;
	uint32_t		 prival;

	/*
	 * We only use 16 (13 really) interrupt priorities, 
	 * and a CPU is only required to implement bit 4-7 of each field
	 * so shift into the top bits.
	 * also low values are higher priority thus IPL_HIGH - pri
	 */
	prival = (IPL_HIGH - pri) << ICMIPMR_SH;
	bus_space_write_4(sc->sc_iot, sc->sc_d_ioh, ICD_IPRn(irq), prival);
}

void
ampintc_setipl(int new)
{
	struct cpu_info		*ci = curcpu();
	struct ampintc_softc	*sc = ampintc;
	int			 psw;

	/* disable here is only to keep hardware in sync with ci->ci_cpl */
	psw = disable_interrupts(I32_bit);
	ci->ci_cpl = new;
	/* low values are higher priority thus IPL_HIGH - pri */

	bus_space_write_4(sc->sc_iot, sc->sc_p_ioh, ICPIPMR,
	    (IPL_HIGH - new) << ICMIPMR_SH);
	restore_interrupts(psw);
}

void
ampintc_intr_enable(int irq)
{
        struct ampintc_softc	*sc = ampintc;

	bus_space_write_4(sc->sc_iot, sc->sc_d_ioh, ICD_ISERn(irq),
	    1 << IRQ_TO_REG32BIT(irq));
}

void
ampintc_intr_disable(int irq)
{
        struct ampintc_softc	*sc = ampintc;

	bus_space_write_4(sc->sc_iot, sc->sc_d_ioh, ICD_ICERn(irq),
	    1 << IRQ_TO_REG32BIT(irq));
}


void
ampintc_calc_mask(void)
{
	struct cpu_info		*ci = curcpu();
        struct ampintc_softc	*sc = ampintc;
	struct intrhand		*ih;
	int			 irq;

	for (irq = 0; irq < NIRQ; irq++) {
		int max = IPL_NONE;
		int min = IPL_HIGH;
		TAILQ_FOREACH(ih, &sc->sc_ampintc_handler[irq].iq_list,
		    ih_list) {
			if (ih->ih_ipl > max)
				max = ih->ih_ipl;

			if (ih->ih_ipl < min)
				min = ih->ih_ipl;
		}

		if (sc->sc_ampintc_handler[irq].iq_irq == max) {
			continue;
		}
		sc->sc_ampintc_handler[irq].iq_irq = max;

		if (max == IPL_NONE)
			min = IPL_NONE;

#ifdef DEBUG_INTC
		if (min != IPL_NONE) {
			printf("irq %d to block at %d %d reg %d bit %d\n",
			    irq, max, min, AMPINTC_IRQ_TO_REG(irq),
			    AMPINTC_IRQ_TO_REGi(irq));
		}
#endif
		/* Enable interrupts at lower levels, clear -> enable */
		/* Set interrupt priority/enable */
		if (min != IPL_NONE) {
			ampintc_set_priority(irq, min);
			ampintc_intr_enable(irq);
		} else {
			ampintc_intr_disable(irq);
		}
	}
	ampintc_setipl(ci->ci_cpl);
}

void
ampintc_splx(int new)
{
	struct cpu_info *ci = curcpu();
	ampintc_setipl(new);

        if (ci->ci_ipending & arm_smask[ci->ci_cpl])
		arm_do_pending_intr(ci->ci_cpl);
}

int
ampintc_spllower(int new)
{
	struct cpu_info *ci = curcpu();
	int old = ci->ci_cpl;
	ampintc_splx(new);
	return (old);
}

int
ampintc_splraise(int new)
{
	struct cpu_info *ci = curcpu();
	int old;
	old = ci->ci_cpl;

	/*
	 * setipl must always be called because there is a race window
	 * where the variable is updated before the mask is set
	 * an interrupt occurs in that window without the mask always
	 * being set, the hardware might not get updated on the next
	 * splraise completely messing up spl protection.
	 */
	if (old > new)
		new = old;

	ampintc_setipl(new);
  
	return (old);
}


uint32_t 
ampintc_iack(void)
{
	uint32_t intid;
	struct ampintc_softc	*sc = ampintc;
	intid = bus_space_read_4(sc->sc_iot, sc->sc_p_ioh, ICPIAR);

	return (intid);
}

void
ampintc_eoi(uint32_t eoi)
{
#if 0
	uint32_t intid;
	struct ampintc_softc	*sc = ampintc;
	intid = bus_space_write_4(sc->sc_cpu, ICPEOIR, eoi);
#endif
}

void
ampintc_irq_handler(void *frame)
{
	struct ampintc_softc	*sc = ampintc;
	struct intrhand		*ih;
	void			*arg;
	uint32_t		 iack_val;
	int			 irq, pri, s;

#ifdef DEBUG_INTC
	printf("irq %d fired\n", irq);
#endif
	iack_val = ampintc_iack();
	irq = iack_val & ((1 << sc->sc_nintr) - 1);

	pri = sc->sc_ampintc_handler[irq].iq_irq;
	s = ampintc_splraise(pri);
	TAILQ_FOREACH(ih, &sc->sc_ampintc_handler[irq].iq_list, ih_list) {
		if (ih->ih_arg != 0)
			arg = ih->ih_arg;
		else
			arg = frame;

		if (ih->ih_func(arg)) 
			ih->ih_count.ec_count++;

	}
	ampintc_eoi(iack_val);

	ampintc_splx(s);
}

void *
ampintc_intr_establish(int irqno, int level, int (*func)(void *),
    void *arg, char *name)
{
	struct ampintc_softc	*sc = ampintc;
	struct intrhand		*ih;
	int			 psw;

	if (irqno < 0 || irqno >= NIRQ)
		panic("ampintc_intr_establish: bogus irqnumber %d: %s",
		     irqno, name);

	/* no point in sleeping unless someone can free memory. */
	ih = (struct intrhand *)malloc (sizeof *ih, M_DEVBUF,
	    cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("intr_establish: can't malloc handler info");
	ih->ih_func = func;
	ih->ih_arg = arg;
	ih->ih_ipl = level;
	ih->ih_irq = irqno;
	ih->ih_name = name;

	psw = disable_interrupts(I32_bit);

	TAILQ_INSERT_TAIL(&sc->sc_ampintc_handler[irqno].iq_list, ih, ih_list);

	if (name != NULL)
		evcount_attach(&ih->ih_count, name, &ih->ih_irq);

#ifdef DEBUG_INTC
	printf("ampintc_intr_establish irq %d level %d [%s]\n", irqno, level,
	    name);
#endif
	ampintc_calc_mask();
	
	restore_interrupts(psw);
	return (ih);
}

void
ampintc_intr_disestablish(void *cookie)
{
#if 0
	int psw;
	struct intrhand *ih = cookie;
	int irqno = ih->ih_irq;
	psw = disable_interrupts(I32_bit);
	TAILQ_REMOVE(&sc->sc_ampintc_handler[irqno].iq_list, ih, ih_list);
	if (ih->ih_name != NULL)
		evcount_detach(&ih->ih_count);
	free(ih, M_DEVBUF);
	restore_interrupts(psw);
#endif
}

const char *
ampintc_intr_string(void *cookie)
{
	return "huh?";
}


#if 0
int ampintc_tst(void *a);

int
ampintc_tst(void *a)
{
	printf("inct_tst called\n");
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, AMPINTC_ISR_CLEAR0, 2);
	return 1;
}

void ampintc_test(void);
void ampintc_test(void)
{
	void * ih;
	printf("about to register handler\n");
	ih = ampintc_intr_establish(1, IPL_BIO, ampintc_tst, NULL, "intctst");

	printf("about to set bit\n");
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, AMPINTC_ISR_SET0, 2);

	printf("about to clear bit\n");
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, AMPINTC_ISR_CLEAR0, 2);

	printf("about to remove handler\n");
	ampintc_intr_disestablish(ih);

	printf("done\n");
}
#endif
