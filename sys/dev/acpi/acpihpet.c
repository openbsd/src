/* $OpenBSD: acpihpet.c,v 1.12 2010/07/21 19:35:15 deraadt Exp $ */
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
#include <dev/acpi/acpidev.h>

int acpihpet_match(struct device *, void *, void *);
void acpihpet_attach(struct device *, struct device *, void *);
int acpihpet_activate(struct device *, int);

#ifdef __HAVE_TIMECOUNTER
u_int acpihpet_gettime(struct timecounter *tc);

static struct timecounter hpet_timecounter = {
	acpihpet_gettime,	/* get_timecount */
	0,			/* no poll_pps */
	0xffffffff,		/* counter_mask (24 bits) */
	0,			/* frequency */
	0,			/* name */
	1000			/* quality */
};
#endif

struct acpihpet_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

struct cfattach acpihpet_ca = {
	sizeof(struct acpihpet_softc),
	acpihpet_match,
	acpihpet_attach,
	NULL,
	acpihpet_activate
};

struct cfdriver acpihpet_cd = {
	NULL, "acpihpet", DV_DULL
};

int
acpihpet_activate(struct device *self, int act)
{
	struct acpihpet_softc *sc = (struct acpihpet_softc *) self;

	switch (act) {
	case DVACT_RESUME:
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    HPET_CONFIGURATION, 1);
		break;
	}

	return 0;
}

int
acpihpet_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct acpi_table_header *hdr;

	/*
	 * If we do not have a table, it is not us
	 */
	if (aaa->aaa_table == NULL)
		return (0);

	/*
	 * If it is an HPET table, we can attach
	 */
	hdr = (struct acpi_table_header *)aaa->aaa_table;
	if (memcmp(hdr->signature, HPET_SIG, sizeof(HPET_SIG) - 1) != 0)
		return (0);

	return (1);
}

void
acpihpet_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpihpet_softc *sc = (struct acpihpet_softc *) self;
	struct acpi_softc *psc = (struct acpi_softc *)parent;
	struct acpi_attach_args *aaa = aux;
	struct acpi_hpet *hpet = (struct acpi_hpet *)aaa->aaa_table;
	u_int64_t period, freq;	/* timer period in femtoseconds (10^-15) */
	u_int32_t v1, v2;
	int timeout;

	if (acpi_map_address(psc, &hpet->base_address, 0, HPET_REG_SIZE,
	    &sc->sc_ioh, &sc->sc_iot))	{
		printf(": can't map i/o space\n");
		return;
	}

	/*
	 * Revisions 0x30 through 0x3a of the AMD SB700, with spread
	 * spectrum enabled, have an SMM based HPET emulation that's
	 * subtly broken.  The hardware is initialized upon first
	 * access of the configuration register.  Initialization takes
	 * some time during which the configuration register returns
	 * 0xffffffff.
	 */
	timeout = 1000;
	do {
		if (bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    HPET_CONFIGURATION) != 0xffffffff)
			break;
	} while(--timeout > 0);

	if (timeout == 0) {
		printf(": disabled\n");
		return;
	}

	/* enable hpet */
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, HPET_CONFIGURATION, 1);

	/* make sure hpet is working */
	v1 = bus_space_read_4(sc->sc_iot, sc->sc_ioh, HPET_MAIN_COUNTER);
	delay(1);
	v2 = bus_space_read_4(sc->sc_iot, sc->sc_ioh, HPET_MAIN_COUNTER);
	if (v1 == v2) {
		printf(": counter not incrementing\n");
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    HPET_CONFIGURATION, 0);
		return;
	}

	period = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
	    HPET_CAPABILITIES + sizeof(u_int32_t));
	if (period == 0) {
		printf(": invalid period\n");
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    HPET_CONFIGURATION, 0);
		return;
	}
	freq =  1000000000000000ull / period;
	printf(": %lld Hz\n", freq);

#ifdef __HAVE_TIMECOUNTER
	hpet_timecounter.tc_frequency = (u_int32_t)freq;
	hpet_timecounter.tc_priv = sc;
	hpet_timecounter.tc_name = sc->sc_dev.dv_xname;
	tc_init(&hpet_timecounter);
#endif
}

#ifdef __HAVE_TIMECOUNTER
u_int
acpihpet_gettime(struct timecounter *tc)
{
	struct acpihpet_softc *sc = tc->tc_priv;

	return (bus_space_read_4(sc->sc_iot, sc->sc_ioh, HPET_MAIN_COUNTER));
}
#endif
