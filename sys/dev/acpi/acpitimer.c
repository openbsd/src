/* $OpenBSD: acpitimer.c,v 1.9 2011/04/22 18:22:01 jordan Exp $ */
/*
 * Copyright (c) 2005 Thorsten Lockert <tholo@sigmasoft.com>
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
#ifdef __HAVE_TIMECOUNTER
#include <sys/timetc.h>
#endif

#include <machine/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>

int acpitimermatch(struct device *, void *, void *);
void acpitimerattach(struct device *, struct device *, void *);

#ifdef __HAVE_TIMECOUNTER
u_int acpi_get_timecount(struct timecounter *tc);

static struct timecounter acpi_timecounter = {
	acpi_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	0x00ffffff,		/* counter_mask (24 bits) */
	ACPI_FREQUENCY,		/* frequency */
	0,			/* name */
	1000			/* quality */
};
#endif

struct acpitimer_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

struct cfattach acpitimer_ca = {
	sizeof(struct acpitimer_softc), acpitimermatch, acpitimerattach
};

struct cfdriver acpitimer_cd = {
	NULL, "acpitimer", DV_DULL
};

int
acpitimermatch(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aa = aux;
	struct cfdata *cf = match;

	/* sanity */
	if (aa->aaa_name == NULL ||
	    strcmp(aa->aaa_name, cf->cf_driver->cd_name) != 0 ||
	    aa->aaa_table != NULL)
		return (0);

	return (1);
}

void
acpitimerattach(struct device *parent, struct device *self, void *aux)
{
	struct acpitimer_softc *sc = (struct acpitimer_softc *) self;
	struct acpi_softc *psc = (struct acpi_softc *) parent;
	int rc;

	if (psc->sc_fadt->hdr_revision >= 3 &&
	    psc->sc_fadt->x_pm_tmr_blk.address != 0)
		rc = acpi_map_address(psc, &psc->sc_fadt->x_pm_tmr_blk, 0,
		    psc->sc_fadt->pm_tmr_len, &sc->sc_ioh, &sc->sc_iot);
	else
		rc = acpi_map_address(psc, NULL, psc->sc_fadt->pm_tmr_blk,
		    psc->sc_fadt->pm_tmr_len, &sc->sc_ioh, &sc->sc_iot);
	if (rc) {
		printf(": can't map i/o space\n");
		return;
	}

	printf(": %d Hz, %d bits\n", ACPI_FREQUENCY,
	    psc->sc_fadt->flags & FADT_TMR_VAL_EXT ? 32 : 24);

#ifdef __HAVE_TIMECOUNTER
	if (psc->sc_fadt->flags & FADT_TMR_VAL_EXT)
		acpi_timecounter.tc_counter_mask = 0xffffffffU;
	acpi_timecounter.tc_priv = sc;
	acpi_timecounter.tc_name = sc->sc_dev.dv_xname;
	tc_init(&acpi_timecounter);
#endif
}


#ifdef __HAVE_TIMECOUNTER
u_int
acpi_get_timecount(struct timecounter *tc)
{
	struct acpitimer_softc *sc = tc->tc_priv;
	u_int u1, u2, u3;

	u2 = bus_space_read_4(sc->sc_iot, sc->sc_ioh, 0);
	u3 = bus_space_read_4(sc->sc_iot, sc->sc_ioh, 0);
	do {
		u1 = u2;
		u2 = u3;
		u3 = bus_space_read_4(sc->sc_iot, sc->sc_ioh, 0);
	} while (u1 > u2 || u2 > u3);

	return (u2);
}
#endif
