/* $OpenBSD: omgpio.c,v 1.1 2013/09/04 14:38:31 patrick Exp $ */
/*
 * Copyright (c) 2007,2009 Dale Rahn <drahn@openbsd.org>
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
#include <sys/queue.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/evcount.h>

#include <arm/cpufunc.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <armv7/omap/omapvar.h>
#include <armv7/omap/omgpiovar.h>

/* OMAP3 registers */
#define	GPIO3_REVISION		0x00
#define	GPIO3_SYSCONFIG		0x10
#define	GPIO3_SYSSTATUS		0x14
#define	GPIO3_IRQSTATUS1	0x18
#define	GPIO3_IRQENABLE1	0x1C
#define	GPIO3_WAKEUPENABLE	0x20
#define	GPIO3_IRQSTATUS2	0x28
#define	GPIO3_IRQENABLE2	0x2C
#define	GPIO3_CTRL		0x30
#define	GPIO3_OE		0x34
#define	GPIO3_DATAIN		0x38
#define	GPIO3_DATAOUT		0x3C
#define	GPIO3_LEVELDETECT0	0x40
#define	GPIO3_LEVELDETECT1	0x44
#define	GPIO3_RISINGDETECT	0x48
#define	GPIO3_FALLINGDETECT	0x4C
#define	GPIO3_DEBOUNCENABLE	0x50
#define	GPIO3_DEBOUNCINGTIME	0x54
#define	GPIO3_CLEARIRQENABLE1	0x60
#define	GPIO3_SETIRQENABLE1	0x64
#define	GPIO3_CLEARIRQENABLE2	0x70
#define	GPIO3_SETIRQENABLE2	0x74
#define	GPIO3_CLEARWKUENA	0x80
#define	GPIO3_SETWKUENA		0x84
#define	GPIO3_CLEARDATAOUT	0x90
#define	GPIO3_SETDATAOUT	0x94
#define	GPIO3_SIZE		0x100

/* OMAP4 registers */
#define GPIO4_REVISION		0x00
#define GPIO4_SYSCONFIG		0x10
#define GPIO4_IRQSTATUS_RAW_0	0x24
#define GPIO4_IRQSTATUS_RAW_1	0x28
#define GPIO4_IRQSTATUS_0	0x2C
#define GPIO4_IRQSTATUS_1	0x30
#define GPIO4_IRQSTATUS_SET_0	0x34
#define GPIO4_IRQSTATUS_SET_1	0x38
#define GPIO4_IRQSTATUS_CLR_0	0x3C
#define GPIO4_IRQSTATUS_CLR_1	0x40
#define GPIO4_IRQWAKEN_0	0x44
#define GPIO4_IRQWAKEN_1	0x48
#define GPIO4_SYSSTATUS		0x114
#define GPIO4_IRQSTATUS1	0x118
#define GPIO4_IRQENABLE1	0x11C
#define GPIO4_WAKEUPENABLE	0x120
#define GPIO4_IRQSTATUS2	0x128
#define GPIO4_IRQENABLE2	0x12C
#define GPIO4_CTRL		0x130
#define GPIO4_OE		0x134
#define GPIO4_DATAIN		0x138
#define GPIO4_DATAOUT		0x13C
#define GPIO4_LEVELDETECT0	0x140
#define GPIO4_LEVELDETECT1	0x144
#define GPIO4_RISINGDETECT 	0x148
#define GPIO4_FALLINGDETECT	0x14C
#define GPIO4_DEBOUNCENABLE	0x150
#define GPIO4_DEBOUNCINGTIME	0x154
#define GPIO4_CLEARIRQENABLE1	0x160
#define GPIO4_SETIRQENABLE1	0x164
#define GPIO4_CLEARIRQENABLE2	0x170
#define GPIO4_SETIRQENABLE2	0x174
#define GPIO4_CLEARWKUPENA	0x180
#define GPIO4_SETWKUENA		0x184
#define GPIO4_CLEARDATAOUT	0x190
#define GPIO4_SETDATAOUT	0x194

#define GPIO_NUM_PINS		32

struct intrhand {
	int (*ih_func)(void *);		/* handler */
	void *ih_arg;			/* arg for handler */
	int ih_ipl;			/* IPL_* */
	int ih_irq;			/* IRQ number */
	int ih_gpio;			/* gpio pin */
	struct evcount	ih_count;
	char *ih_name;
};

struct omgpio_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	void			*sc_ih_h;
	void			*sc_ih_l;
	int 			sc_max_il;
	int 			sc_min_il;
	int			sc_irq;
	struct intrhand		*sc_handlers[GPIO_NUM_PINS];
	int			sc_omap_ver;
	unsigned int (*sc_get_bit)(struct omgpio_softc *sc,
	    unsigned int gpio);
	void (*sc_set_bit)(struct omgpio_softc *sc,
	    unsigned int gpio);
	void (*sc_clear_bit)(struct omgpio_softc *sc,
	    unsigned int gpio);
	void (*sc_set_dir)(struct omgpio_softc *sc,
	    unsigned int gpio, unsigned int dir);
};

#define GPIO_PIN_TO_INST(x)	((x) >> 5)
#define GPIO_PIN_TO_OFFSET(x)	((x) & 0x1f)

int omgpio_match(struct device *parent, void *v, void *aux);
void omgpio_attach(struct device *parent, struct device *self, void *args);
void omgpio_recalc_interrupts(struct omgpio_softc *sc);
int omgpio_irq(void *);
int omgpio_irq_dummy(void *);

unsigned int omgpio_v3_get_bit(struct omgpio_softc *, unsigned int);
void omgpio_v3_set_bit(struct omgpio_softc *, unsigned int);
void omgpio_v3_clear_bit(struct omgpio_softc *, unsigned int);
void omgpio_v3_set_dir(struct omgpio_softc *, unsigned int , unsigned int);
unsigned int omgpio_v4_get_bit(struct omgpio_softc *, unsigned int);
void omgpio_v4_set_bit(struct omgpio_softc *, unsigned int);
void omgpio_v4_clear_bit(struct omgpio_softc *, unsigned int);
void omgpio_v4_set_dir(struct omgpio_softc *, unsigned int, unsigned int);
unsigned int omgpio_v4_get_dir(struct omgpio_softc *, unsigned int);


struct cfattach omgpio_ca = {
	sizeof (struct omgpio_softc), omgpio_match, omgpio_attach
};

struct cfdriver omgpio_cd = {
	NULL, "omgpio", DV_DULL
};

int
omgpio_match(struct device *parent, void *v, void *aux)
{
	switch (board_id) {
	case BOARD_ID_OMAP3_BEAGLE:
	case BOARD_ID_OMAP3_OVERO:
		break; /* continue trying */
	case BOARD_ID_OMAP4_PANDA:
		break; /* continue trying */
	default:
		return 0; /* unknown */
	}
	return (1);
}

void
omgpio_attach(struct device *parent, struct device *self, void *args)
{
	struct omap_attach_args *oa = args;
	struct omgpio_softc *sc = (struct omgpio_softc *) self;
	u_int32_t rev;

	sc->sc_iot = oa->oa_iot;
	if (bus_space_map(sc->sc_iot, oa->oa_dev->mem[0].addr,
	    oa->oa_dev->mem[0].size, 0, &sc->sc_ioh))
		panic("omgpio_attach: bus_space_map failed!");


	switch (board_id) {
	case BOARD_ID_OMAP3_BEAGLE:
	case BOARD_ID_OMAP3_OVERO:
		sc->sc_omap_ver  = 3;
		sc->sc_get_bit  = omgpio_v3_get_bit;
		sc->sc_set_bit = omgpio_v3_set_bit;
		sc->sc_clear_bit = omgpio_v3_clear_bit;
		sc->sc_set_dir = omgpio_v3_set_dir;
		break;
	case BOARD_ID_OMAP4_PANDA:
		sc->sc_omap_ver  = 4;
		sc->sc_get_bit  = omgpio_v4_get_bit;
		sc->sc_set_bit = omgpio_v4_set_bit;
		sc->sc_clear_bit = omgpio_v4_clear_bit;
		sc->sc_set_dir = omgpio_v4_set_dir;
		break;
	}

	rev = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO3_REVISION);

	printf(" omap%d rev %d.%d\n", sc->sc_omap_ver, rev >> 4 & 0xf,
	    rev & 0xf);

	sc->sc_irq = oa->oa_dev->irq[0];

	if (sc->sc_omap_ver == 3) {
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    GPIO3_CLEARIRQENABLE1, ~0);
	} else if (sc->sc_omap_ver == 4) {
		/* XXX - nothing? */
	}

	/* XXX - SYSCONFIG */
	/* XXX - CTRL */
	/* XXX - DEBOUNCE */
}

/* XXX - This assumes MCU INTERRUPTS are IRQ1, and DSP are IRQ2 */

#if 0
/* XXX - FIND THESE REGISTERS !!! */
unsigned int
omgpio_get_function(unsigned int gpio, unsigned int fn)
{
	return 0;
}

void
omgpio_set_function(unsigned int gpio, unsigned int fn)
{
}
#endif

/*
 * get_bit() is not reliable if used on an output pin.
 */

unsigned int
omgpio_get_bit(unsigned int gpio)
{
	struct omgpio_softc *sc = omgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];

	return sc->sc_get_bit(sc, gpio);
}

void
omgpio_set_bit(unsigned int gpio)
{
	struct omgpio_softc *sc = omgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];

	sc->sc_set_bit(sc, gpio);
}

void
omgpio_clear_bit(unsigned int gpio)
{
	struct omgpio_softc *sc = omgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];

	sc->sc_clear_bit(sc, gpio);
}
void
omgpio_set_dir(unsigned int gpio, unsigned int dir)
{
	struct omgpio_softc *sc = omgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];

	sc->sc_set_dir(sc, gpio, dir);
}

unsigned int
omgpio_v3_get_bit(struct omgpio_softc *sc, unsigned int gpio)
{
	u_int32_t reg;

	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO3_DATAIN);
	return (reg >> GPIO_PIN_TO_OFFSET(gpio)) & 0x1;
}

void
omgpio_v3_set_bit(struct omgpio_softc *sc, unsigned int gpio)
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO3_SETDATAOUT,
	    1 << GPIO_PIN_TO_OFFSET(gpio));
}

void
omgpio_v3_clear_bit(struct omgpio_softc *sc, unsigned int gpio)
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO3_CLEARDATAOUT,
	    1 << GPIO_PIN_TO_OFFSET(gpio));
}

void
omgpio_v3_set_dir(struct omgpio_softc *sc, unsigned int gpio, unsigned int dir)
{
	int s;
	u_int32_t reg;

	s = splhigh();

	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO3_DATAIN);
	if (dir == OMGPIO_DIR_IN)
		reg |= 1 << GPIO_PIN_TO_OFFSET(gpio);
	else
		reg &= ~(1 << GPIO_PIN_TO_OFFSET(gpio));
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO3_OE, reg);

	splx(s);
}

unsigned int
omgpio_v4_get_bit(struct omgpio_softc *sc, unsigned int gpio)
{
	u_int32_t reg;

	if(omgpio_v4_get_dir(sc, gpio) == OMGPIO_DIR_IN)
		reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO4_DATAIN);
	else
		reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO4_DATAOUT);
	return (reg >> GPIO_PIN_TO_OFFSET(gpio)) & 0x1;
}

void
omgpio_v4_set_bit(struct omgpio_softc *sc, unsigned int gpio)
{

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO4_SETDATAOUT,
		1 << GPIO_PIN_TO_OFFSET(gpio));
}

void
omgpio_v4_clear_bit(struct omgpio_softc *sc, unsigned int gpio)
{
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO4_CLEARDATAOUT,
		1 << GPIO_PIN_TO_OFFSET(gpio));
}

void
omgpio_v4_set_dir(struct omgpio_softc *sc, unsigned int gpio, unsigned int dir)
{
	int s;
	u_int32_t reg;

	s = splhigh();

	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO4_OE);
	if (dir == OMGPIO_DIR_IN)
		reg |= 1 << GPIO_PIN_TO_OFFSET(gpio);
	else
		reg &= ~(1 << GPIO_PIN_TO_OFFSET(gpio));
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO4_OE, reg);

	splx(s);
}

unsigned int
omgpio_v4_get_dir(struct omgpio_softc *sc, unsigned int gpio)
{
	int s;
	u_int32_t dir, reg;

	s = splhigh();

	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO4_OE);
	if (reg & (1 << GPIO_PIN_TO_OFFSET(gpio)))
		dir = OMGPIO_DIR_IN;
	else
		dir = OMGPIO_DIR_OUT;

	splx(s);

	return dir;
}

#if 0
void
omgpio_clear_intr(struct omgpio_softc *sc, unsigned int gpio)
{
	struct omgpio_softc *sc = omgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO3_IRQSTATUS1,
	    1 << GPIO_PIN_TO_OFFSET(gpio));
}

void
omgpio_intr_mask(struct omgpio_softc *sc, unsigned int gpio)
{
	struct omgpio_softc *sc = omgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO3_CLEARIRQENABLE1,
	    1 << GPIO_PIN_TO_OFFSET(gpio));
}

void
omgpio_intr_unmask(struct omgpio_softc *sc, unsigned int gpio)
{
	struct omgpio_softc *sc = omgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO3_SETIRQENABLE1,
	    1 << GPIO_PIN_TO_OFFSET(gpio));
}

void
omgpio_intr_level(struct omgpio_softc *sc, unsigned int gpio, unsigned int level)
{
	u_int32_t fe, re, l0, l1, bit;
	struct omgpio_softc *sc = omgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];
	int s;

	s = splhigh();

	fe = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO3_FALLINGDETECT);
	re = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO3_RISINGDETECT);
	l0 = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO3_LEVELDETECT0);
	l1 = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO3_LEVELDETECT1);

	bit = 1 << GPIO_PIN_TO_OFFSET(gpio);

	switch (level) {
	case IST_NONE:
		fe &= ~bit;
		re &= ~bit;
		l0 &= ~bit;
		l1 &= ~bit;
		break;
	case IST_EDGE_FALLING:
		fe |= bit;
		re &= ~bit;
		l0 &= ~bit;
		l1 &= ~bit;
		break;
	case IST_EDGE_RISING:
		fe &= ~bit;
		re |= bit;
		l0 &= ~bit;
		l1 &= ~bit;
		break;
	case IST_PULSE: /* XXX */
		/* FALLTHRU */
	case IST_EDGE_BOTH:
		fe |= bit;
		re |= bit;
		l0 &= ~bit;
		l1 &= ~bit;
		break;
	case IST_LEVEL_LOW:
		fe &= ~bit;
		re &= ~bit;
		l0 |= bit;
		l1 &= ~bit;
		break;
	case IST_LEVEL_HIGH:
		fe &= ~bit;
		re &= ~bit;
		l0 &= ~bit;
		l1 |= bit;
		break;
	default:
		panic("omgpio_intr_level: bad level: %d", level);
		break;
	}

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO3_FALLINGDETECT, fe);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO3_RISINGDETECT, re);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO3_LEVELDETECT0, l0);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO3_LEVELDETECT1, l1);

	splx(s);
}

void *
omgpio_intr_establish(struct omgpio_softc *sc, unsigned int gpio, int level, int spl,
    int (*func)(void *), void *arg, char *name)
{
	int psw;
	struct intrhand *ih;
	struct omgpio_softc *sc;

	/*
	 * XXX - is gpio here the pin or the interrupt number
	 * which is 96 + gpio pin?
	 */

	if (GPIO_PIN_TO_INST(gpio) > omgpio_cd.cd_ndevs)
		panic("omgpio_intr_establish: bogus irqnumber %d: %s",
		    gpio, name);

	sc = omgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];

	if (sc->sc_handlers[GPIO_PIN_TO_OFFSET(gpio)] != NULL)
		panic("omgpio_intr_establish: gpio pin busy %d old %s new %s",
		    gpio, sc->sc_handlers[GPIO_PIN_TO_OFFSET(gpio)]->ih_name,
		    name);

	psw = disable_interrupts(I32_bit);

	/* no point in sleeping unless someone can free memory. */
	ih = (struct intrhand *)malloc( sizeof *ih, M_DEVBUF,
	    cold ? M_NOWAIT : M_WAITOK);
	if (ih == NULL)
		panic("intr_establish: can't malloc handler info");
	ih->ih_func = func;
	ih->ih_arg = arg;
	ih->ih_ipl = level;
	ih->ih_gpio = gpio;
	ih->ih_irq = gpio + 512;
	ih->ih_name = name;

	sc->sc_handlers[GPIO_PIN_TO_OFFSET(gpio)] = ih;

	evcount_attach(&ih->ih_count, name, &ih->ih_irq);

	omgpio_intr_level(gpio, level);
	omgpio_intr_unmask(gpio);

	omgpio_recalc_interrupts(sc);

	restore_interrupts(psw);

	return (ih);
}

void
omgpio_intr_disestablish(struct omgpio_softc *sc, void *cookie)
{
	int psw;
	struct intrhand *ih = cookie;
	struct omgpio_softc *sc = omgpio_cd.cd_devs[GPIO_PIN_TO_INST(ih->ih_gpio)];
	int gpio = ih->ih_gpio;
	psw = disable_interrupts(I32_bit);

	ih = sc->sc_handlers[GPIO_PIN_TO_OFFSET(gpio)];
	sc->sc_handlers[GPIO_PIN_TO_OFFSET(gpio)] = NULL;

	evcount_detach(&ih->ih_count);

	free(ih, M_DEVBUF);

	omgpio_intr_level(gpio, IST_NONE);
	omgpio_intr_mask(gpio);
	omgpio_clear_intr(gpio); /* Just in case */

	omgpio_recalc_interrupts(sc);

	restore_interrupts(psw);
}

int
omgpio_irq(void *v)
{
	struct omgpio_softc *sc = v;
	u_int32_t pending;
	struct intrhand *ih;
	int bit;

	pending = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO3_IRQSTATUS1);

	while (pending != 0) {
		bit = ffs(pending) - 1;
		ih = sc->sc_handlers[bit];

		if (ih != NULL) {
			if (ih->ih_func(ih->ih_arg))
				ih->ih_count.ec_count++;
			omgpio_clear_intr(ih->ih_gpio);
		} else {
			panic("omgpio: irq fired no handler, gpio %x %x %x",
				sc->sc_dev.dv_unit * 32 + bit, pending,
	bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO3_IRQENABLE1)

				);
		}
		pending &= ~(1 << bit);
	}
	return 1;
}

int
omgpio_irq_dummy(void *v)
{
	return 0;
}

void
omgpio_recalc_interrupts(struct omgpio_softc *sc)
{
	struct intrhand *ih;
	int max = IPL_NONE;
	int min = IPL_HIGH;
	int i;

	for (i = 0; i < GPIO_NUM_PINS; i++) {
		ih = sc->sc_handlers[i];
		if (ih != NULL) {
			if (ih->ih_ipl > max)
				max = ih->ih_ipl;

			if (ih->ih_ipl < min)
				min = ih->ih_ipl;
		}
	}
	if (max == IPL_NONE)
		min = IPL_NONE;

#if 0
	if ((max == IPL_NONE || max != sc->sc_max_il) && sc->sc_ih_h != NULL)
		arm_intr_disestablish(sc->sc_ih_h);

	if (max != IPL_NONE && max != sc->sc_max_il) {
		sc->sc_ih_h = arm_intr_establish(sc->sc_irq, max, omgpio_irq,
		    sc, NULL);
	}
#else
	if (sc->sc_ih_h != NULL)
		arm_intr_disestablish(sc->sc_ih_h);

	if (max != IPL_NONE) {
		sc->sc_ih_h = arm_intr_establish(sc->sc_irq, max, omgpio_irq,
		    sc, NULL);
	}
#endif

	sc->sc_max_il = max;

	if (sc->sc_ih_l != NULL)
		arm_intr_disestablish(sc->sc_ih_l);

	if (max != min) {
		sc->sc_ih_h = arm_intr_establish(sc->sc_irq, min,
		    omgpio_irq_dummy, sc, NULL);
	}
	sc->sc_min_il = min;
}
#endif
