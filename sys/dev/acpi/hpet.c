/*	$OpenBSD: hpet.c,v 1.3 2005/12/16 21:11:51 marco Exp $	*/
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

int hpetmatch(struct device *, void *, void *);
void hpetattach(struct device *, struct device *, void *);

#ifdef __HAVE_TIMECOUNTER
u_int hpet_get_timecount(struct timecounter *tc);

static struct timecounter hpet_timecounter = {
	hpet_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	0xffffffff,		/* counter_mask (24 bits) */
	0,			/* frequency */
	0,			/* name */
	1000			/* quality */
};
#endif

struct hpet_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

struct cfattach hpet_ca = {
	sizeof(struct hpet_softc), hpetmatch, hpetattach
};

struct cfdriver hpet_cd = {
	NULL, "hpet", DV_DULL
};

int
hpetmatch(struct device *parent, void *match, void *aux)
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
hpetattach(struct device *parent, struct device *self, void *aux)
{
	struct hpet_softc *sc = (struct hpet_softc *) self;
	struct acpi_attach_args *aa = aux;
	struct acpi_hpet *hpet = (struct acpi_hpet *)aa->aaa_table;
	u_int64_t period, freq;	/* timer period in femtoseconds (10^-15) */

	switch (hpet->base_address.address_space_id) {
	case GAS_SYSTEM_MEMORY:
		sc->sc_iot = aa->aaa_memt;
		break;

	case GAS_SYSTEM_IOSPACE:
		sc->sc_iot = aa->aaa_iot;
		break;

#if 0
	case GAS_SYSTEM_PCI_CFG_SPACE:
		sc->sc_iot = aa->aaa_pcit;
		break;

	case GAS_SYSTEM_SMBUS:
		sc->sc_iot = aa->aaa_smbust;
		break;
#endif

	default:
		printf(": can't identify bus\n");
		return;
	}

	if (bus_space_map(sc->sc_iot, hpet->base_address.address,
	    HPET_REG_SIZE, 0, &sc->sc_ioh)) {
		printf(": can't map i/o space\n");
		return;
	}

	period = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
				  HPET_CAPABILITIES + sizeof(u_int32_t));
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
hpet_get_timecount(struct timecounter *tc)
{
	struct hpet_softc *sc = tc->tc_priv;

	return (bus_space_read_4(sc->sc_iot, sc->sc_ioh, HPET_MAIN_COUNTER));
}
#endif
