/*	$OpenBSD: pxa2x0_gpio.c,v 1.2 2005/01/02 19:52:36 drahn Exp $ */
/*	$NetBSD: pxa2x0_gpio.c,v 1.2 2003/07/15 00:24:55 lukem Exp $	*/

/*
 * Copyright 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
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

#include <sys/cdefs.h>
/*
__KERNEL_RCSID(0, "$NetBSD: pxa2x0_gpio.c,v 1.2 2003/07/15 00:24:55 lukem Exp $");
*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_gpio.h>

struct gpio_irq_handler {
	struct gpio_irq_handler *gh_next;
	int (*gh_func)(void *);
	void *gh_arg;
	int gh_spl;
	u_int gh_gpio;
};

struct pxagpio_softc {
	struct device sc_dev;
	bus_space_tag_t sc_bust;
	bus_space_handle_t sc_bush;
	void *sc_irqcookie[3];
	u_int32_t sc_mask[3];
#ifdef PXAGPIO_HAS_GPION_INTRS
	struct gpio_irq_handler *sc_handlers[GPIO_NPINS];
#else
	struct gpio_irq_handler *sc_handlers[2];
#endif
};

static int	pxagpio_match(struct device *, void *, void *);
static void	pxagpio_attach(struct device *, struct device *, void *);

#ifdef __NetBSD__
CFATTACH_DECL(pxagpio, sizeof(struct pxagpio_softc),
    pxagpio_match, pxagpio_attach, NULL, NULL);
#else
struct cfattach pxagpio_ca = {
        sizeof (struct pxagpio_softc), pxagpio_match, pxagpio_attach
};
	 
struct cfdriver pxagpio_cd = {
	NULL, "pxagpio", DV_DULL
};

#endif

static struct pxagpio_softc *pxagpio_softc;
static vaddr_t pxagpio_regs;
#define GPIO_BOOTSTRAP_REG(reg)	\
	(*((volatile u_int32_t *)(pxagpio_regs + (reg))))

static int gpio_intr0(void *);
static int gpio_intr1(void *);
#ifdef PXAGPIO_HAS_GPION_INTRS
static int gpio_dispatch(struct pxagpio_softc *, int);
static int gpio_intrN(void *);
#endif

static __inline u_int32_t
pxagpio_reg_read(struct pxagpio_softc *sc, int reg)
{
	if (__predict_true(sc != NULL))
		return (bus_space_read_4(sc->sc_bust, sc->sc_bush, reg));
	else
	if (pxagpio_regs)
		return (GPIO_BOOTSTRAP_REG(reg));
	panic("pxagpio_reg_read: not bootstrapped");
}

static __inline void
pxagpio_reg_write(struct pxagpio_softc *sc, int reg, u_int32_t val)
{
	if (__predict_true(sc != NULL))
		bus_space_write_4(sc->sc_bust, sc->sc_bush, reg, val);
	else
	if (pxagpio_regs)
		GPIO_BOOTSTRAP_REG(reg) = val;
	else
		panic("pxagpio_reg_write: not bootstrapped");
	return;
}

static int
pxagpio_match(struct device *parent, void *cf, void *aux)
{
	struct pxaip_attach_args *pxa = aux;

	if (pxagpio_softc != NULL || pxa->pxa_addr != PXA2X0_GPIO_BASE)
		return (0);

	pxa->pxa_size = PXA2X0_GPIO_SIZE;

	return (1);
}

void
pxagpio_attach(struct device *parent, struct device *self, void *aux)
{
	struct pxagpio_softc *sc = (struct pxagpio_softc *)self;
	struct pxaip_attach_args *pxa = aux;

	sc->sc_bust = pxa->pxa_iot;

	printf(": GPIO Controller\n");

	if (bus_space_map(sc->sc_bust, pxa->pxa_addr, pxa->pxa_size, 0,
	    &sc->sc_bush)) {
		printf("%s: Can't map registers!\n", sc->sc_dev.dv_xname);
		return;
	}

	memset(sc->sc_handlers, 0, sizeof(sc->sc_handlers));

	/*
	 * Disable all GPIO interrupts
	 */
	pxagpio_reg_write(sc, GPIO_GRER0, 0);
	pxagpio_reg_write(sc, GPIO_GRER1, 0);
	pxagpio_reg_write(sc, GPIO_GRER2, 0);
	pxagpio_reg_write(sc, GPIO_GFER0, 0);
	pxagpio_reg_write(sc, GPIO_GFER1, 0);
	pxagpio_reg_write(sc, GPIO_GFER2, 0);
	pxagpio_reg_write(sc, GPIO_GEDR0, ~0);
	pxagpio_reg_write(sc, GPIO_GEDR1, ~0);
	pxagpio_reg_write(sc, GPIO_GEDR2, ~0);

#ifdef PXAGPIO_HAS_GPION_INTRS
	sc->sc_irqcookie[2] = pxa2x0_intr_establish(PXA2X0_INT_GPION, IPL_BIO,
	    gpio_intrN, sc);
	if (sc->sc_irqcookie[2] == NULL) {
		printf("%s: failed to hook main GPIO interrupt\n",
		    sc->sc_dev.dv_xname);
		return;
	}

#endif

	sc->sc_irqcookie[0] = sc->sc_irqcookie[1] = NULL;

	pxagpio_softc = sc;
}

void
pxa2x0_gpio_bootstrap(vaddr_t gpio_regs)
{

	pxagpio_regs = gpio_regs;
}

void *
pxa2x0_gpio_intr_establish(u_int gpio, int level, int spl, int (*func)(void *),
    void *arg, char *name)
{
	struct pxagpio_softc *sc = pxagpio_softc;
	struct gpio_irq_handler *gh;
	u_int32_t bit, reg;

#ifdef DEBUG
#ifdef PXAGPIO_HAS_GPION_INTRS
	if (gpio >= GPIO_NPINS)
		panic("pxa2x0_gpio_intr_establish: bad pin number: %d", gpio);
#else
	if (gpio > 1)
		panic("pxa2x0_gpio_intr_establish: bad pin number: %d", gpio);
#endif
#endif

	if (!GPIO_IS_GPIO_IN(pxa2x0_gpio_get_function(gpio)))
		panic("pxa2x0_gpio_intr_establish: Pin %d not GPIO_IN", gpio);

	switch (level) {
	case IST_EDGE_FALLING:
	case IST_EDGE_RISING:
	case IST_EDGE_BOTH:
		break;

	default:
		panic("pxa2x0_gpio_intr_establish: bad level: %d", level);
		break;
	}

	MALLOC(gh, struct gpio_irq_handler *, sizeof(struct gpio_irq_handler),
	    M_DEVBUF, M_NOWAIT);

	gh->gh_func = func;
	gh->gh_arg = arg;
	gh->gh_spl = spl;
	gh->gh_gpio = gpio;

	gh->gh_next = sc->sc_handlers[gpio];
	sc->sc_handlers[gpio] = gh;

	if (gpio == 0) {
		KDASSERT(sc->sc_irqcookie[0] == NULL);
		sc->sc_irqcookie[0] = pxa2x0_intr_establish(PXA2X0_INT_GPIO0,
		    spl, gpio_intr0, sc);
		KDASSERT(sc->sc_irqcookie[0]);
	} else
	if (gpio == 1) {
		KDASSERT(sc->sc_irqcookie[1] == NULL);
		sc->sc_irqcookie[1] = pxa2x0_intr_establish(PXA2X0_INT_GPIO1,
		    spl, gpio_intr1, sc);
		KDASSERT(sc->sc_irqcookie[1]);
	}

	bit = GPIO_BIT(gpio);
	sc->sc_mask[GPIO_BANK(gpio)] |= bit;

	switch (level) {
	case IST_EDGE_FALLING:
		reg = pxagpio_reg_read(sc, GPIO_REG(GPIO_GFER0, gpio));
		pxagpio_reg_write(sc, GPIO_REG(GPIO_GFER0, gpio), reg | bit);
		break;

	case IST_EDGE_RISING:
		reg = pxagpio_reg_read(sc, GPIO_REG(GPIO_GRER0, gpio));
		pxagpio_reg_write(sc, GPIO_REG(GPIO_GRER0, gpio), reg | bit);
		break;

	case IST_EDGE_BOTH:
		reg = pxagpio_reg_read(sc, GPIO_REG(GPIO_GFER0, gpio));
		pxagpio_reg_write(sc, GPIO_REG(GPIO_GFER0, gpio), reg | bit);
		reg = pxagpio_reg_read(sc, GPIO_REG(GPIO_GRER0, gpio));
		pxagpio_reg_write(sc, GPIO_REG(GPIO_GRER0, gpio), reg | bit);
		break;
	}

	return (gh);
}

void
pxa2x0_gpio_intr_disestablish(void *cookie)
{
	struct pxagpio_softc *sc = pxagpio_softc;
	struct gpio_irq_handler *gh = cookie;
	u_int32_t bit, reg;

	bit = GPIO_BIT(gh->gh_gpio);

	reg = pxagpio_reg_read(sc, GPIO_REG(GPIO_GFER0, gh->gh_gpio));
	reg &= ~bit;
	pxagpio_reg_write(sc, GPIO_REG(GPIO_GFER0, gh->gh_gpio), reg);
	reg = pxagpio_reg_read(sc, GPIO_REG(GPIO_GRER0, gh->gh_gpio));
	reg &= ~bit;
	pxagpio_reg_write(sc, GPIO_REG(GPIO_GRER0, gh->gh_gpio), reg);

	pxagpio_reg_write(sc, GPIO_REG(GPIO_GEDR0, gh->gh_gpio), bit);

	sc->sc_mask[GPIO_BANK(gh->gh_gpio)] &= ~bit;
	sc->sc_handlers[gh->gh_gpio] = NULL;

	if (gh->gh_gpio == 0) {
#if 0
		pxa2x0_intr_disestablish(sc->sc_irqcookie[0]);
		sc->sc_irqcookie[0] = NULL;
#else
		panic("pxa2x0_gpio_intr_disestablish: can't unhook GPIO#0");
#endif
	} else
	if (gh->gh_gpio == 1) {
#if 0
		pxa2x0_intr_disestablish(sc->sc_irqcookie[1]);
		sc->sc_irqcookie[0] = NULL;
#else
		panic("pxa2x0_gpio_intr_disestablish: can't unhook GPIO#0");
#endif
	}

	FREE(gh, M_DEVBUF);
}

static int
gpio_intr0(void *arg)
{
	struct pxagpio_softc *sc = arg;

#ifdef DIAGNOSTIC
	if (sc->sc_handlers[0] == NULL) {
		printf("%s: stray GPIO#0 edge interrupt\n",
		    sc->sc_dev.dv_xname);
		return (0);
	}
#endif

	bus_space_write_4(sc->sc_bust, sc->sc_bush, GPIO_REG(GPIO_GEDR0, 0),
	    GPIO_BIT(0));

	return ((sc->sc_handlers[0]->gh_func)(sc->sc_handlers[0]->gh_arg));
}

static int
gpio_intr1(void *arg)
{
	struct pxagpio_softc *sc = arg;

#ifdef DIAGNOSTIC
	if (sc->sc_handlers[1] == NULL) {
		printf("%s: stray GPIO#1 edge interrupt\n",
		    sc->sc_dev.dv_xname);
		return (0);
	}
#endif

	bus_space_write_4(sc->sc_bust, sc->sc_bush, GPIO_REG(GPIO_GEDR0, 1),
	    GPIO_BIT(1));

	return ((sc->sc_handlers[1]->gh_func)(sc->sc_handlers[1]->gh_arg));
}

#ifdef PXAGPIO_HAS_GPION_INTRS
static int
gpio_dispatch(struct pxagpio_softc *sc, int gpio_base)
{
	struct gpio_irq_handler **ghp, *gh;
	int i, s, handled, pins;
	u_int32_t gedr, mask;
	int bank;

	/* Fetch bitmap of pending interrupts on this GPIO bank */
	gedr = pxagpio_reg_read(sc, GPIO_REG(GPIO_GEDR0, gpio_base));

	/* Don't handle GPIO 0/1 here */
	if (gpio_base == 0)
		gedr &= ~(GPIO_BIT(0) | GPIO_BIT(1));

	/* Bail early if there are no pending interrupts in this bank */
	if (gedr == 0)
		return (0);

	/* Acknowledge pending interrupts. */
	pxagpio_reg_write(sc, GPIO_REG(GPIO_GEDR0, gpio_base), gedr);

	bank = GPIO_BANK(gpio_base);

	/*
	 * We're only interested in those for which we have a handler
	 * registered
	 */
#ifdef DEBUG
	if ((gedr & sc->sc_mask[bank]) == 0) {
		printf("%s: stray GPIO interrupt. Bank %d, GEDR 0x%08x, mask 0x%08x\n",
		    sc->sc_dev.dv_xname, bank, gedr, sc->sc_mask[bank]);
		return (1);	/* XXX: Pretend we dealt with it */
	}
#endif

	gedr &= sc->sc_mask[bank];
	ghp = &sc->sc_handlers[gpio_base];
	pins = (gpio_base < 64) ? 32 : 17;
	handled = 0;

	for (i = 0, mask = 1; i < pins && gedr; i++, ghp++, mask <<= 1) {
		if ((gedr & mask) == 0)
			continue;
		gedr &= ~mask;

		if ((gh = *ghp) == NULL) {
			printf("%s: unhandled GPIO interrupt. GPIO#%d\n",
			    sc->sc_dev.dv_xname, gpio_base + i);
			continue;
		}

		s = _splraise(gh->gh_spl);
		do {
			handled |= (gh->gh_func)(gh->gh_arg);
			gh = gh->gh_next;
		} while (gh != NULL);
		splx(s);
	}

	return (handled);
}

static int
gpio_intrN(void *arg)
{
	struct pxagpio_softc *sc = arg;
	int handled;

	handled = gpio_dispatch(sc, 0);
	handled |= gpio_dispatch(sc, 32);
	handled |= gpio_dispatch(sc, 64);

	return (handled);
}
#endif	/* PXAGPIO_HAS_GPION_INTRS */

u_int
pxa2x0_gpio_get_function(u_int gpio)
{
	struct pxagpio_softc *sc = pxagpio_softc;
	u_int32_t rv, io;

	KDASSERT(gpio < GPIO_NPINS);

	rv = pxagpio_reg_read(sc, GPIO_FN_REG(gpio)) >> GPIO_FN_SHIFT(gpio);
	rv = GPIO_FN(rv);

	io = pxagpio_reg_read(sc, GPIO_REG(GPIO_GPDR0, gpio));
	if (io & GPIO_BIT(gpio))
		rv |= GPIO_OUT;

	io = pxagpio_reg_read(sc, GPIO_REG(GPIO_GPLR0, gpio));
	if (io & GPIO_BIT(gpio))
		rv |= GPIO_SET;

	return (rv);
}

u_int
pxa2x0_gpio_set_function(u_int gpio, u_int fn)
{
	struct pxagpio_softc *sc = pxagpio_softc;
	u_int32_t rv, bit;
	u_int oldfn;

	KDASSERT(gpio < GPIO_NPINS);

	oldfn = pxa2x0_gpio_get_function(gpio);

	if (GPIO_FN(fn) == GPIO_FN(oldfn) &&
	    GPIO_FN_IS_OUT(fn) == GPIO_FN_IS_OUT(oldfn)) {
		/*
		 * The pin's function is not changing.
		 * For Alternate Functions and GPIO input, we can just
		 * return now.
		 * For GPIO output pins, check the initial state is
		 * the same.
		 *
		 * Return 'fn' instead of 'oldfn' so the caller can
		 * reliably detect that we didn't change anything.
		 * (The initial state might be different for non-
		 * GPIO output pins).
		 */
		if (!GPIO_IS_GPIO_OUT(fn) ||
		    GPIO_FN_IS_SET(fn) == GPIO_FN_IS_SET(oldfn))
			return (fn);
	}

	/*
	 * See section 4.1.3.7 of the PXA2x0 Developer's Manual for
	 * the correct procedure for changing GPIO pin functions.
	 */

	bit = GPIO_BIT(gpio);

	/*
	 * 1. Configure the correct set/clear state of the pin
	 */
	if (GPIO_FN_IS_SET(fn))
		pxagpio_reg_write(sc, GPIO_REG(GPIO_GPSR0, gpio), bit);
	else
		pxagpio_reg_write(sc, GPIO_REG(GPIO_GPCR0, gpio), bit);

	/*
	 * 2. Configure the pin as an input or output as appropriate
	 */
	rv = pxagpio_reg_read(sc, GPIO_REG(GPIO_GPDR0, gpio)) & ~bit;
	if (GPIO_FN_IS_OUT(fn))
		rv |= bit;
	pxagpio_reg_write(sc, GPIO_REG(GPIO_GPDR0, gpio), rv);

	/*
	 * 3. Configure the pin's function
	 */
	bit = GPIO_FN_MASK << GPIO_FN_SHIFT(gpio);
	fn = GPIO_FN(fn) << GPIO_FN_SHIFT(gpio);
	rv = pxagpio_reg_read(sc, GPIO_FN_REG(gpio)) & ~bit;
	pxagpio_reg_write(sc, GPIO_FN_REG(gpio), rv | fn);

	return (oldfn);
}
