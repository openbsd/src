/* $OpenBSD: acpibtn.c,v 1.33 2010/08/07 16:21:20 deraadt Exp $ */
/*
 * Copyright (c) 2005 Marco Peereboom <marco@openbsd.org>
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
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/apmvar.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include <sys/sensors.h>

int	acpibtn_match(struct device *, void *, void *);
void	acpibtn_attach(struct device *, struct device *, void *);
int	acpibtn_notify(struct aml_node *, int, void *);

struct acpibtn_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	int			sc_btn_type;
#define	ACPIBTN_UNKNOWN	0
#define ACPIBTN_LID	1
#define ACPIBTN_POWER	2
#define ACPIBTN_SLEEP	3
};

int	acpibtn_getsta(struct acpibtn_softc *);
int	acpibtn_setpsw(struct acpibtn_softc *, int);

struct acpi_lid {
	struct acpibtn_softc	*abl_softc;
	SLIST_ENTRY(acpi_lid)	abl_link;
};
SLIST_HEAD(acpi_lid_head, acpi_lid) acpibtn_lids =
    SLIST_HEAD_INITIALIZER(acpibtn_lids);

struct cfattach acpibtn_ca = {
	sizeof(struct acpibtn_softc), acpibtn_match, acpibtn_attach
};

struct cfdriver acpibtn_cd = {
	NULL, "acpibtn", DV_DULL
};

const char *acpibtn_hids[] = { ACPI_DEV_LD, ACPI_DEV_PBD, ACPI_DEV_SBD, 0 };

int
acpibtn_setpsw(struct acpibtn_softc *sc, int psw)
{
	struct aml_value	val;

	bzero(&val, sizeof val);
	val.type = AML_OBJTYPE_INTEGER;
	val.v_integer = psw;
	val.length = 1;

	return (aml_evalname(sc->sc_acpi, sc->sc_devnode, "_PSW", 1, &val,
	    NULL));
}

void
acpibtn_disable_psw(void)
{
	struct acpi_lid *lid;

	/* disable _LID for wakeup */
	SLIST_FOREACH(lid, &acpibtn_lids, abl_link)
		acpibtn_setpsw(lid->abl_softc, 0);
}

void
acpibtn_enable_psw(void)
{
	struct acpi_lid		*lid;

	/* enable _LID for wakeup */
	SLIST_FOREACH(lid, &acpibtn_lids, abl_link)
		acpibtn_setpsw(lid->abl_softc, 1);
}

int
acpibtn_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args	*aa = aux;
	struct cfdata		*cf = match;

	/* sanity */
	return (acpi_matchhids(aa, acpibtn_hids, cf->cf_driver->cd_name));
}

void
acpibtn_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpibtn_softc	*sc = (struct acpibtn_softc *)self;
	struct acpi_attach_args *aa = aux;
	struct acpi_lid		*lid;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node;

	if (!strcmp(aa->aaa_dev, ACPI_DEV_LD)) {
		sc->sc_btn_type = ACPIBTN_LID;
		if (acpibtn_setpsw(sc, 0) == 0) {
			lid = malloc(sizeof(*lid), M_DEVBUF, M_WAITOK | M_ZERO);
			lid->abl_softc = sc;
			SLIST_INSERT_HEAD(&acpibtn_lids, lid, abl_link);
		}
	} else if (!strcmp(aa->aaa_dev, ACPI_DEV_PBD))
		sc->sc_btn_type = ACPIBTN_POWER;
	else if (!strcmp(aa->aaa_dev, ACPI_DEV_SBD))
		sc->sc_btn_type = ACPIBTN_SLEEP;

	acpibtn_getsta(sc);

	printf(": %s\n", sc->sc_devnode->name);

	aml_register_notify(sc->sc_devnode, aa->aaa_dev, acpibtn_notify,
	    sc, ACPIDEV_NOPOLL);
}

int
acpibtn_getsta(struct acpibtn_softc *sc)
{
	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "_STA", 0, NULL, NULL) != 0) {
		dnprintf(20, "%s: no _STA\n", DEVNAME(sc));
		/* XXX not all buttons have _STA so FALLTROUGH */
	}

	return (0);
}

int
acpibtn_notify(struct aml_node *node, int notify_type, void *arg)
{
	struct acpibtn_softc	*sc = arg;
#ifndef SMALL_KERNEL
	extern int lid_suspend;
	int64_t lid;
#endif

	dnprintf(10, "acpibtn_notify: %.2x %s\n", notify_type,
	    sc->sc_devnode->name);

	switch (sc->sc_btn_type) {
	case ACPIBTN_LID:
#ifndef SMALL_KERNEL
		/*
		 * Notification of 0x80 for lid opens or closes.  We
		 * need to check the current status by calling the
		 * _LID method.  0 means the lid is closed and we
		 * should go to sleep.
		 */
		if (lid_suspend == 0)
			break;
		if (aml_evalinteger(sc->sc_acpi, sc->sc_devnode,
		    "_LID", 0, NULL, &lid))
			return (0);
		if (lid == 0)
			goto sleep;
#endif /* SMALL_KERNEL */
		break;
	case ACPIBTN_SLEEP:
#ifndef SMALL_KERNEL
		switch (notify_type) {
		case 0x02:
			/* "something" has been taken care of by the system */
			break;
		case 0x80:
sleep:
			/* Request to go to sleep */
			if (acpi_record_event(sc->sc_acpi, APM_USER_SUSPEND_REQ))
				sc->sc_acpi->sc_sleepmode = ACPI_STATE_S3;
			break;
		}
#endif /* SMALL_KERNEL */
		break;
	case ACPIBTN_POWER:
		if (notify_type == 0x80)
			sc->sc_acpi->sc_powerdown = 1;
		break;
	default:
		printf("%s: spurious acpi button interrupt %i\n", DEVNAME(sc),
		    sc->sc_btn_type);
		break;
	}

	return (0);
}
