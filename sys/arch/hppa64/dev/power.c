/*	$OpenBSD: power.c,v 1.1 2005/04/01 10:40:47 mickey Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/kthread.h>

#include <machine/reg.h>
#include <machine/pdc.h>
#include <machine/autoconf.h>

struct power_softc {
	struct device sc_dev;
	void *sc_ih;

	struct proc *sc_thread;
	void (*sc_kicker)(void *);

	int sc_dr_cnt;
	paddr_t sc_pwr_reg;
	volatile int sc_interrupted;
};

int	powermatch(struct device *, void *, void *);
void	powerattach(struct device *, struct device *, void *);

struct cfattach power_ca = {
	sizeof(struct power_softc), powermatch, powerattach
};

struct cfdriver power_cd = {
	NULL, "power", DV_DULL
};

void power_thread_create(void *v);
void power_thread_reg(void *v);
void power_cold_hook_reg(int);
int power_intr(void *);

int
powermatch(struct device *parent, void *cfdata, void *aux)
{
	struct cfdata *cf = cfdata;
	struct confargs *ca = aux;

	if (cf->cf_unit > 0 && !strcmp(ca->ca_name, "power"))
		return (0);

	return (1);
}

void
powerattach(struct device *parent, struct device *self, void *aux)
{
	struct power_softc *sc = (struct power_softc *)self;
	struct confargs *ca = aux;

	if (ca->ca_hpa) {
		extern void (*cold_hook)(int);

		sc->sc_pwr_reg = ca->ca_hpa;
		cold_hook = power_cold_hook_reg;
		sc->sc_kicker = power_thread_reg;
		printf("\n");
	} else
		printf(": not available\n");

	if (ca->ca_irq >= 0)
		sc->sc_ih = cpu_intr_establish(IPL_CLOCK, ca->ca_irq,
		    power_intr, sc, sc->sc_dev.dv_xname);

	if (sc->sc_kicker)
		kthread_create_deferred(power_thread_create, sc);
}

int
power_intr(void *v)
{
	struct power_softc *sc = v;

	sc->sc_interrupted = 1;

	return (1);
}

void
power_thread_create(void *v)
{
	struct power_softc *sc = v;

	if (kthread_create(sc->sc_kicker, sc, &sc->sc_thread,
	    sc->sc_dev.dv_xname))
		printf("WARNING: failed to create kernel power thread\n");
}

void
power_thread_reg(void *v)
{
	struct power_softc *sc = v;
	u_int32_t r;

	for (;;) {
		__asm __volatile("ldwas 0(%1), %0"
		    : "=&r" (r) : "r" (sc->sc_pwr_reg));

		if (!(r & 1))
			boot(RB_POWERDOWN | RB_HALT);

		tsleep(v, PWAIT, "regpower", 10);
	}
}

void
power_cold_hook_reg(int on)
{
	extern struct pdc_power_info pdc_power_info;	/* machdep.c */
	int error;

	if ((error = pdc_call((iodcio_t)pdc, 0, PDC_SOFT_POWER,
	    PDC_SOFT_POWER_ENABLE, &pdc_power_info,
	    on == HPPA_COLD_HOT)))
		printf("power_cold_hook_reg: failed (%d)\n", error);
}
