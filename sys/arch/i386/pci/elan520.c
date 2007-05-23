/*	$OpenBSD: elan520.c,v 1.13 2007/05/23 11:55:11 markus Exp $	*/
/*	$NetBSD: elan520.c,v 1.4 2002/10/02 05:47:15 thorpej Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
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
 * Device driver for the AMD Elan SC520 System Controller.  This attaches
 * where the "pchb" driver might normally attach, and provides support for
 * extra features on the SC520, such as the watchdog timer and GPIO.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/gpio.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/timetc.h>

#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/gpio/gpiovar.h>

#include <arch/i386/pci/elan520reg.h>

struct elansc_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_memt;
	bus_space_handle_t	sc_memh;

	/* GPIO interface */
	struct gpio_chipset_tag sc_gpio_gc;
	gpio_pin_t sc_gpio_pins[ELANSC_PIO_NPINS];

	/* GP timer */
	struct timecounter	sc_tc;
} *elansc;

int	elansc_match(struct device *, void *, void *);
void	elansc_attach(struct device *, struct device *, void *);
void	elansc_update_cpuspeed(void);
void	elansc_setperf(int);
int	elansc_cpuspeed(int *);

void	elansc_wdogctl(struct elansc_softc *, int, uint16_t);
#define elansc_wdogctl_reset(sc)	elansc_wdogctl(sc, 1, 0)
#define elansc_wdogctl_write(sc, val)	elansc_wdogctl(sc, 0, val)
int	elansc_wdogctl_cb(void *, int);

int	elansc_gpio_pin_read(void *, int);
void	elansc_gpio_pin_write(void *, int, int);
void	elansc_gpio_pin_ctl(void *, int, int);

u_int	elansc_tc_read(struct timecounter *);

struct cfattach elansc_ca = {
	sizeof(struct elansc_softc), elansc_match, elansc_attach
};

struct cfdriver elansc_cd = {
	NULL, "elansc", DV_DULL
};

static int cpuspeed;

int
elansc_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_AMD &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_AMD_ELANSC520)
		return (10);	/* beat pchb */

	return (0);
}

static const char *elansc_speeds[] = {
	"(reserved 00)",
	"100MHz",
	"133MHz",
	"(reserved 11)",
};

#define RSTBITS "\20\x07SCP\x06HRST\x05SRST\x04WDT\x03SD\x02PRGRST\x01PWRGOOD"

void
elansc_attach(struct device *parent, struct device *self, void *aux)
{
	struct elansc_softc *sc = (void *) self;
	struct pci_attach_args *pa = aux;
	struct gpiobus_attach_args gba;
	struct timecounter *tc;
	uint16_t rev, data;
	uint8_t ressta, cpuctl, tmr;
	int pin, reg, shift;

	sc->sc_memt = pa->pa_memt;
	if (bus_space_map(sc->sc_memt, MMCR_BASE_ADDR, NBPG, 0,
	    &sc->sc_memh) != 0) {
		printf(": unable to map registers\n");
		return;
	}

	rev = bus_space_read_2(sc->sc_memt, sc->sc_memh, MMCR_REVID);
	cpuctl = bus_space_read_1(sc->sc_memt, sc->sc_memh, MMCR_CPUCTL);
	ressta = bus_space_read_1(sc->sc_memt, sc->sc_memh, MMCR_RESSTA);

	printf(": product %d stepping %d.%d, CPU clock %s, reset %b\n",
	    (rev & REVID_PRODID) >> REVID_PRODID_SHIFT,
	    (rev & REVID_MAJSTEP) >> REVID_MAJSTEP_SHIFT,
	    (rev & REVID_MINSTEP),
	    elansc_speeds[cpuctl & CPUCTL_CPU_CLK_SPD_MASK],
	    ressta, RSTBITS);

	/*
	 * Determine cause of the last reset, and issue a warning if it
	 * was due to watchdog expiry.
	 */
	if (ressta & RESSTA_WDT_RST_DET)
		printf("%s: WARNING: LAST RESET DUE TO WATCHDOG EXPIRATION!\n",
		    sc->sc_dev.dv_xname);
	bus_space_write_1(sc->sc_memt, sc->sc_memh, MMCR_RESSTA, ressta);

	/* Set up the watchdog registers with some defaults. */
	elansc_wdogctl_write(sc, WDTMRCTL_WRST_ENB | WDTMRCTL_EXP_SEL30);

	/* ...and clear it. */
	elansc_wdogctl_reset(sc);

	wdog_register(sc, elansc_wdogctl_cb);
	elansc = sc;
	cpu_setperf = elansc_setperf;
	cpu_cpuspeed = elansc_cpuspeed;
	elansc_update_cpuspeed();

	/* Initialize GPIO pins array */
	for (pin = 0; pin < ELANSC_PIO_NPINS; pin++) {
		sc->sc_gpio_pins[pin].pin_num = pin;
		sc->sc_gpio_pins[pin].pin_caps = GPIO_PIN_INPUT |
		    GPIO_PIN_OUTPUT;

		/* Read initial state */
		reg = (pin < 16 ? MMCR_PIODIR15_0 : MMCR_PIODIR31_16);
		shift = pin % 16;
		data = bus_space_read_2(sc->sc_memt, sc->sc_memh, reg);
		if ((data & (1 << shift)) == 0)
			sc->sc_gpio_pins[pin].pin_flags = GPIO_PIN_INPUT;
		else
			sc->sc_gpio_pins[pin].pin_flags = GPIO_PIN_OUTPUT;
		if (elansc_gpio_pin_read(sc, pin) == 0)
			sc->sc_gpio_pins[pin].pin_state = GPIO_PIN_LOW;
		else
			sc->sc_gpio_pins[pin].pin_state = GPIO_PIN_HIGH;
	}

	/* Create controller tag */
	sc->sc_gpio_gc.gp_cookie = sc;
	sc->sc_gpio_gc.gp_pin_read = elansc_gpio_pin_read;
	sc->sc_gpio_gc.gp_pin_write = elansc_gpio_pin_write;
	sc->sc_gpio_gc.gp_pin_ctl = elansc_gpio_pin_ctl;

	gba.gba_name = "gpio";
	gba.gba_gc = &sc->sc_gpio_gc;
	gba.gba_pins = sc->sc_gpio_pins;
	gba.gba_npins = ELANSC_PIO_NPINS;

	/* Attach GPIO framework */
	config_found(&sc->sc_dev, &gba, gpiobus_print);

	/* Disable GP1/2, clear the current count, and set the period to max */
	bus_space_write_2(sc->sc_memt, sc->sc_memh, GPTMR1CTL,
		GPTMRCTL_ENB_WR | GPTMRCTL_CONT_CMP |
		GPTMRCTL_PSC_SEL | GPTMRCTL_RTG);
	bus_space_write_2(sc->sc_memt, sc->sc_memh, GPTMR1CNT, 0);
	bus_space_write_2(sc->sc_memt, sc->sc_memh, GPTMR1MAXCMPA, 0);

	bus_space_write_2(sc->sc_memt, sc->sc_memh, GPTMR2CTL,
		GPTMRCTL_ENB_WR | GPTMRCTL_CONT_CMP);
	bus_space_write_2(sc->sc_memt, sc->sc_memh, GPTMR2CNT, 0);
	bus_space_write_2(sc->sc_memt, sc->sc_memh, GPTMR2MAXCMPA, 0);

	tmr = bus_space_read_1(sc->sc_memt, sc->sc_memh, SWTMRCFG);

	/* Enable GP1/2 */
	bus_space_write_2(sc->sc_memt, sc->sc_memh, GPTMR1CTL,
		GPTMRCTL_ENB | GPTMRCTL_ENB_WR | GPTMRCTL_CONT_CMP |
		GPTMRCTL_PSC_SEL | GPTMRCTL_RTG);
	bus_space_write_2(sc->sc_memt, sc->sc_memh, GPTMR2CTL,
		GPTMRCTL_ENB | GPTMRCTL_ENB_WR | GPTMRCTL_CONT_CMP);

	/* Attach timer */
	tc = &sc->sc_tc;
	tc->tc_get_timecount = elansc_tc_read;
	tc->tc_poll_pps = NULL;
	tc->tc_counter_mask = ~0;
	tc->tc_frequency = (tmr & 1) ? (33000000 / 4) : (33333333 / 4);
	tc->tc_name = sc->sc_dev.dv_xname;
	tc->tc_quality = 1000;
	tc->tc_priv = sc;
	tc_init(tc);
}

u_int
elansc_tc_read(struct timecounter *tc)
{
	struct elansc_softc *sc = tc->tc_priv;
	u_int32_t m1, m2, l;

	do {
		m1 = bus_space_read_2(sc->sc_memt, sc->sc_memh, GPTMR1CNT);
		l = bus_space_read_2(sc->sc_memt, sc->sc_memh, GPTMR2CNT);
		m2 = bus_space_read_2(sc->sc_memt, sc->sc_memh, GPTMR1CNT);
	} while (m1 != m2);

	return ((m1 << 16) | l);
}

void
elansc_wdogctl(struct elansc_softc *sc, int do_reset, uint16_t val)
{
	int s;
	uint8_t echo_mode;

	s = splhigh();

	/* Switch off GP bus echo mode. */
	echo_mode = bus_space_read_1(sc->sc_memt, sc->sc_memh, MMCR_GPECHO);
	bus_space_write_1(sc->sc_memt, sc->sc_memh, MMCR_GPECHO,
	    echo_mode & ~GPECHO_GP_ECHO_ENB);

	if (do_reset) {
		/* Reset the watchdog. */
		bus_space_write_2(sc->sc_memt, sc->sc_memh, MMCR_WDTMRCTL,
		    WDTMRCTL_RESET1);
		bus_space_write_2(sc->sc_memt, sc->sc_memh, MMCR_WDTMRCTL,
		    WDTMRCTL_RESET2);
	} else {
		/* Unlock the register. */
		bus_space_write_2(sc->sc_memt, sc->sc_memh, MMCR_WDTMRCTL,
		    WDTMRCTL_UNLOCK1);
		bus_space_write_2(sc->sc_memt, sc->sc_memh, MMCR_WDTMRCTL,
		    WDTMRCTL_UNLOCK2);

		/* Write the value. */
		bus_space_write_2(sc->sc_memt, sc->sc_memh, MMCR_WDTMRCTL,
		   val);
	}

	/* Switch GP bus echo mode back. */
	bus_space_write_1(sc->sc_memt, sc->sc_memh, MMCR_GPECHO, echo_mode);

	splx(s);
}

static const struct {
	int	period;		/* whole seconds */
	uint16_t exp;		/* exponent select */
} elansc_wdog_periods[] = {
	{ 1,	WDTMRCTL_EXP_SEL25 },
	{ 2,	WDTMRCTL_EXP_SEL26 },
	{ 4,	WDTMRCTL_EXP_SEL27 },
	{ 8,	WDTMRCTL_EXP_SEL28 },
	{ 16,	WDTMRCTL_EXP_SEL29 },
	{ 32,	WDTMRCTL_EXP_SEL30 },
};

int
elansc_wdogctl_cb(void *self, int period)
{
	struct elansc_softc *sc = self;
	int i;

	if (period == 0) {
		elansc_wdogctl_write(sc,
		    WDTMRCTL_WRST_ENB | WDTMRCTL_EXP_SEL30);
	} else {
		for (i = 0; i < (sizeof(elansc_wdog_periods) /
		    sizeof(elansc_wdog_periods[0])) - 1; i++)
			if (elansc_wdog_periods[i].period >= period)
				break;
		period = elansc_wdog_periods[i].period;
		elansc_wdogctl_write(sc, WDTMRCTL_ENB |
		    WDTMRCTL_WRST_ENB | elansc_wdog_periods[i].exp);
		elansc_wdogctl_reset(sc);
	}
	return (period);
}

void
elansc_update_cpuspeed(void)
{
#ifdef I586_CPU
	static const int elansc_mhz[] = { 0, 100, 133, 999 };
#endif
	uint8_t cpuctl;

	cpuctl = bus_space_read_1(elansc->sc_memt, elansc->sc_memh,
	    MMCR_CPUCTL);
#ifdef I586_CPU
	cpuspeed = elansc_mhz[cpuctl & CPUCTL_CPU_CLK_SPD_MASK];
#endif
}

void
elansc_setperf(int level)
{
	uint32_t eflags;
	uint8_t cpuctl, speed;

	level = (level > 50) ? 100 : 0;

	cpuctl = bus_space_read_1(elansc->sc_memt, elansc->sc_memh,
	    MMCR_CPUCTL);
	speed = (level == 100) ? 2 : 1;
	if ((cpuctl & CPUCTL_CPU_CLK_SPD_MASK) == speed)
		return;

	eflags = read_eflags();
	disable_intr();
	bus_space_write_1(elansc->sc_memt, elansc->sc_memh, MMCR_CPUCTL,
	    (cpuctl & ~CPUCTL_CPU_CLK_SPD_MASK) | speed);
	enable_intr();
	write_eflags(eflags);

	elansc_update_cpuspeed();
}

int
elansc_cpuspeed(int *freq)
{
	*freq = cpuspeed;
	return (0);
}

int
elansc_gpio_pin_read(void *arg, int pin)
{
	struct elansc_softc *sc = arg;
	int reg, shift;
	u_int16_t data;

	reg = (pin < 16 ? MMCR_PIODATA15_0 : MMCR_PIODATA31_16);
	shift = pin % 16;
	data = bus_space_read_2(sc->sc_memt, sc->sc_memh, reg);

	return ((data >> shift) & 0x1);
}

void
elansc_gpio_pin_write(void *arg, int pin, int value)
{
	struct elansc_softc *sc = arg;
	int reg, shift;
	u_int16_t data;

	reg = (pin < 16 ? MMCR_PIODATA15_0 : MMCR_PIODATA31_16);
	shift = pin % 16;
	data = bus_space_read_2(sc->sc_memt, sc->sc_memh, reg);
	if (value == 0)
		data &= ~(1 << shift);
	else if (value == 1)
		data |= (1 << shift);

	bus_space_write_2(sc->sc_memt, sc->sc_memh, reg, data);
}

void
elansc_gpio_pin_ctl(void *arg, int pin, int flags)
{
	struct elansc_softc *sc = arg;
	int reg, shift;
	u_int16_t data;

	reg = (pin < 16 ? MMCR_PIODIR15_0 : MMCR_PIODIR31_16);
	shift = pin % 16;
	data = bus_space_read_2(sc->sc_memt, sc->sc_memh, reg);
	if (flags & GPIO_PIN_INPUT)
		data &= ~(1 << shift);
	if (flags & GPIO_PIN_OUTPUT)
		data |= (1 << shift);

	bus_space_write_2(sc->sc_memt, sc->sc_memh, reg, data);
}
