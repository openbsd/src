/* $OpenBSD: omgpio.c,v 1.4 2010/09/20 06:33:48 matthew Exp $ */
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
#include <machine/bus.h>
#include <machine/intr.h>
#include <arch/beagle/beagle/ahb.h>
#include <arch/beagle/dev/omgpiovar.h>
#include "omgpio.h"


/* registers */
#define	GPIO_REVISION		0x00
#define	GPIO_SYSCONFIG		0x10
#define	GPIO_SYSSTATUS		0x14
#define	GPIO_IRQSTATUS1		0x18
#define	GPIO_IRQENABLE1		0x1C
#define	GPIO_WAKEUPENABLE	0x20
#define	GPIO_IRQSTATUS2		0x28
#define	GPIO_IRQENABLE2		0x2C
#define	GPIO_CTRL		0x30
#define	GPIO_OE			0x34
#define	GPIO_DATAIN		0x38
#define	GPIO_DATAOUT		0x3C
#define	GPIO_LEVELDETECT0	0x40
#define	GPIO_LEVELDETECT1	0x44
#define	GPIO_RISINGDETECT	0x48
#define	GPIO_FALLINGDETECT	0x4C
#define	GPIO_DEBOUNCENABLE	0x50
#define	GPIO_DEBOUNCINGTIME	0x54
#define	GPIO_CLEARIRQENABLE1	0x60
#define	GPIO_SETIRQENABLE1	0x64
#define	GPIO_CLEARIRQENABLE2	0x70
#define	GPIO_SETIRQENABLE2	0x74
#define	GPIO_CLEARWKUENA	0x80
#define	GPIO_SETWKUENA		0x84
#define	GPIO_CLEARDATAOUT	0x90
#define	GPIO_SETDATAOUT		0x94
#define	GPIO_SIZE		0x100


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
};

#define GPIO_PIN_TO_INST(x)	((x) >> 5)
#define GPIO_PIN_TO_OFFSET(x)	((x) & 0x1f)

int omgpio_match(struct device *parent, void *v, void *aux);
void omgpio_attach(struct device *parent, struct device *self, void *args);
void omgpio_recalc_interrupts(struct omgpio_softc *sc);
int omgpio_irq(void *);
int omgpio_irq_dummy(void *);

struct cfattach	omgpio_ca = {
	sizeof (struct omgpio_softc), omgpio_match, omgpio_attach
};

struct cfdriver omgpio_cd = {
	NULL, "omgpio", DV_DULL
};

int
omgpio_match(struct device *parent, void *v, void *aux)
{
	return (1);
}

void
omgpio_attach(struct device *parent, struct device *self, void *args)
{
        struct ahb_attach_args *aa = args;
	struct omgpio_softc *sc = (struct omgpio_softc *) self;
	u_int32_t rev;

	sc->sc_iot = aa->aa_iot;
	if (bus_space_map(sc->sc_iot, aa->aa_addr, GPIO_SIZE, 0, &sc->sc_ioh))
		panic("omgpio_attach: bus_space_map failed!");

	rev = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_REVISION);

	printf(" rev %d.%d\n", rev >> 4 & 0xf, rev & 0xf);

	sc->sc_irq = aa->aa_intr;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO_CLEARIRQENABLE1, ~0);

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
	u_int32_t reg;
	
	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_DATAIN);
	return (reg >> GPIO_PIN_TO_OFFSET(gpio)) & 0x1;
}

void
omgpio_set_bit(unsigned int gpio)
{
	struct omgpio_softc *sc = omgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];
	
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO_SETDATAOUT,
	    1 << GPIO_PIN_TO_OFFSET(gpio));
}

void
omgpio_clear_bit(unsigned int gpio)
{
	struct omgpio_softc *sc = omgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];
	
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO_CLEARDATAOUT,
	    1 << GPIO_PIN_TO_OFFSET(gpio));
}

void
omgpio_set_dir(unsigned int gpio, unsigned int dir)
{
	struct omgpio_softc *sc = omgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];
	int s;
	u_int32_t reg;

	s = splhigh();

	reg = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_DATAIN);
	if (dir == OMGPIO_DIR_IN)
		reg |= 1 << GPIO_PIN_TO_OFFSET(gpio);
	else
		reg &= ~(1 << GPIO_PIN_TO_OFFSET(gpio));
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO_OE, reg);

	splx(s);
}

void
omgpio_clear_intr(unsigned int gpio)
{
	struct omgpio_softc *sc = omgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];
	
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO_IRQSTATUS1,
	    1 << GPIO_PIN_TO_OFFSET(gpio));
}

void
omgpio_intr_mask(unsigned int gpio)
{
	struct omgpio_softc *sc = omgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];
	
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO_CLEARIRQENABLE1,
	    1 << GPIO_PIN_TO_OFFSET(gpio));
}

void
omgpio_intr_unmask(unsigned int gpio)
{
	struct omgpio_softc *sc = omgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];
	
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO_SETIRQENABLE1,
	    1 << GPIO_PIN_TO_OFFSET(gpio));
}

void
omgpio_intr_level(unsigned int gpio, unsigned int level)
{
	u_int32_t fe, re, l0, l1, bit;
	struct omgpio_softc *sc = omgpio_cd.cd_devs[GPIO_PIN_TO_INST(gpio)];
	int s;

	s = splhigh();

	fe = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_FALLINGDETECT);
	re = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_RISINGDETECT);
	l0 = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_LEVELDETECT0);
	l1 = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_LEVELDETECT1);
	
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
		break;
        default:
                panic("omgpio_intr_level: bad level: %d", level);
                break;
        }

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO_FALLINGDETECT, fe);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO_RISINGDETECT, re);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO_LEVELDETECT0, l0);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, GPIO_LEVELDETECT1, l1);

	splx(s);
}

void *
omgpio_intr_establish(unsigned int gpio, int level, int spl,
    int (*func)(void *), void *arg, char *name)
{
	int psw;
	struct intrhand *ih;
	struct omgpio_softc *sc;

	/*
	 * XXX - is gpio here the pin or the interrupt number
	 * which is 96 + gpio pin?
	 */

	if (GPIO_PIN_TO_INST(gpio) > NOMGPIO)
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
	ih->ih_irq = gpio + INTC_NUM_IRQ;
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
omgpio_intr_disestablish(void *cookie)
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

	pending = bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_IRQSTATUS1);

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
	bus_space_read_4(sc->sc_iot, sc->sc_ioh, GPIO_IRQENABLE1)

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
		intc_intr_disestablish(sc->sc_ih_h);

	if (max != IPL_NONE && max != sc->sc_max_il) {
		sc->sc_ih_h = intc_intr_establish(sc->sc_irq, max, omgpio_irq,
		    sc, NULL);
	}
#else
	if (sc->sc_ih_h != NULL)
		intc_intr_disestablish(sc->sc_ih_h);

	if (max != IPL_NONE) {
		sc->sc_ih_h = intc_intr_establish(sc->sc_irq, max, omgpio_irq,
		    sc, NULL);
	}
#endif

	sc->sc_max_il = max;

	if (sc->sc_ih_l != NULL)
		intc_intr_disestablish(sc->sc_ih_l);

	if (max != min) {
		sc->sc_ih_h = intc_intr_establish(sc->sc_irq, min,
		    omgpio_irq_dummy, sc, NULL);
	}
	sc->sc_min_il = min;
}
