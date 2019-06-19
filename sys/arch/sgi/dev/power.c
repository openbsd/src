/*	$OpenBSD: power.c,v 1.16 2018/12/03 13:46:30 visa Exp $	*/

/*
 * Copyright (c) 2007 Jasper Lievisse Adriaanse <jasper@openbsd.org>
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
#include <sys/proc.h>
#include <sys/signalvar.h>

#include <machine/autoconf.h>
#include <mips64/archtype.h>

#include <dev/ic/ds1687reg.h>
#include <sgi/dev/dsrtcvar.h>

#include "power.h"

/*
 * Power button driver for the SGI O2 and Octane.
 */

int	power_intr(void *);

struct cfdriver power_cd = {
	NULL, "power", DV_DULL
};

#if NPOWER_MACEBUS > 0

#include <sgi/localbus/macebusvar.h>

void	power_macebus_attach(struct device *, struct device *, void *);
int	power_macebus_match(struct device *, void *, void *);

struct cfattach power_macebus_ca = {
	sizeof(struct device), power_macebus_match, power_macebus_attach
};

int
power_macebus_match(struct device *parent, void *match, void *aux)
{
	return (1);
}

void
power_macebus_attach(struct device *parent, struct device *self, void *aux)
{
	struct macebus_attach_args *maa = aux;

	/* Establish interrupt handler. */
	if (macebus_intr_establish(maa->maa_intr, maa->maa_mace_intr,
	    IPL_TTY, power_intr, self, self->dv_xname))
		printf("\n");
	else
		printf(": unable to establish interrupt!\n");
}

#endif

#if NPOWER_MAINBUS > 0

#include <sgi/xbow/xbow.h>
#include <sgi/xbow/xheartreg.h>

void	power_mainbus_attach(struct device *, struct device *, void *);
int	power_mainbus_match(struct device *, void *, void *);
int	power_mainbus_intr(void *);

struct cfattach power_mainbus_ca = {
	sizeof(struct device), power_mainbus_match, power_mainbus_attach
};

int
power_mainbus_match(struct device *parent, void *match, void *aux)
{
	struct mainbus_attach_args *maa = aux;

	if (strcmp(maa->maa_name, power_cd.cd_name) != 0)
		return 0;

	return sys_config.system_type == SGI_OCTANE ? 1 : 0;
}

void
power_mainbus_attach(struct device *parent, struct device *self, void *aux)
{
	/* Establish interrupt handler. */
	if (xbow_intr_establish(power_mainbus_intr, self, HEART_ISR_POWER,
	    IPL_TTY, self->dv_xname, NULL) != 0) {
		printf(": unable to establish interrupt!\n");
		return;
	}

	printf("\n");
}

int
power_mainbus_intr(void *v)
{
	/*
	 * Clear interrupt condition; debouncing the kickstart bit will not
	 * suffice.
	 */
	xbow_intr_clear(HEART_ISR_POWER);

	return power_intr(v);
}

#endif

int
power_intr(void *unused)
{
	extern int allowpowerdown;
	int val;

	/* 
	 * Prevent further interrupts by clearing the kickstart flag
	 * in the DS1687's extended control register.
	 */
	val = dsrtc_register_read(DS1687_EXT_CTRL);
	if (val == -1)
		return 1;		/* no rtc attached */

	/* debounce condition */
	dsrtc_register_write(DS1687_EXT_CTRL, val & ~DS1687_KICKSTART);

	if (allowpowerdown == 1) {
		allowpowerdown = 0;
		prsignal(initprocess, SIGUSR2);
	}

	return 1;
}
