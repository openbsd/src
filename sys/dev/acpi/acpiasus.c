/* $OpenBSD: acpiasus.c,v 1.4 2008/06/01 17:59:55 marco Exp $ */
/* $NetBSD: asus_acpi.c,v 1.2.2.2 2008/04/03 12:42:37 mjf Exp $ */

/*-
 * Copyright (c) 2007, 2008 Jared D. McNeill <jmcneill@invisible.ca>
 * All rights reserved.
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
 *        This product includes software developed by Jared D. McNeill.
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
 * ASUS ACPI hotkeys driver.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/workq.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include "audio.h"
#include "wskbd.h"

struct acpiasus_softc {
	struct device		sc_dev;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_devnode;

	void 			*sc_powerhook;
};

#define ASUS_NOTIFY_WirelessSwitch	0x10
#define ASUS_NOTIFY_BrightnessLow	0x20
#define ASUS_NOTIFY_BrightnessHigh	0x2f
#define ASUS_NOTIFY_DisplayCycle	0x30
#define ASUS_NOTIFY_TaskSwitch		0x12
#define ASUS_NOTIFY_VolumeMute		0x13
#define ASUS_NOTIFY_VolumeDown		0x14
#define ASUS_NOTIFY_VolumeUp		0x15

#define	ASUS_SDSP_LCD			0x01
#define	ASUS_SDSP_CRT			0x02
#define	ASUS_SDSP_TV			0x04
#define	ASUS_SDSP_DVI			0x08
#define	ASUS_SDSP_ALL \
	(ASUS_SDSP_LCD | ASUS_SDSP_CRT | ASUS_SDSP_TV | ASUS_SDSP_DVI)

int	acpiasus_match(struct device *, void *, void *);
void	acpiasus_attach(struct device *, struct device *, void *);
void	acpiasus_init(struct device *);
int	acpiasus_notify(struct aml_node *, int, void *);
void	acpiasus_power(int, void *);

#if NAUDIO > 0 && NWSKBD > 0
extern int wskbd_set_mixervolume(long dir);
#endif

struct cfattach acpiasus_ca = {
	sizeof(struct acpiasus_softc), acpiasus_match, acpiasus_attach
};

struct cfdriver acpiasus_cd = {
	NULL, "acpiasus", DV_DULL
};

int
acpiasus_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aa = aux;
	struct cfdata *cf = match;

	if (aa->aaa_name == NULL ||
	    strcmp(aa->aaa_name, cf->cf_driver->cd_name) != 0 ||
	    aa->aaa_table != NULL)
		return 0;

	return 1;
}

void
acpiasus_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpiasus_softc *sc = (struct acpiasus_softc *)self;
	struct acpi_attach_args *aa = aux;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_devnode = aa->aaa_node;

	sc->sc_powerhook = powerhook_establish(acpiasus_power, sc);

	printf("\n");

	acpiasus_init(self);

	aml_register_notify(sc->sc_devnode, aa->aaa_dev,
	    acpiasus_notify, sc, ACPIDEV_NOPOLL);
}

void
acpiasus_init(struct device *self)
{
	struct acpiasus_softc *sc = (struct acpiasus_softc *)self;
	struct aml_value cmd;
	struct aml_value ret;

	cmd.type = AML_OBJTYPE_INTEGER;
	cmd.v_integer = 0x40;		/* Disable ASL display switching. */

	if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "INIT", 1, &cmd, &ret))
		printf("%s: no INIT\n", DEVNAME(sc));
}

int
acpiasus_notify(struct aml_node *node, int notify, void *arg)
{
	struct acpiasus_softc *sc = arg;

	if (notify >= ASUS_NOTIFY_BrightnessLow &&
	    notify <= ASUS_NOTIFY_BrightnessHigh) {
#if ACPIASUS_DEBUG
		printf("%s: brightness %d percent\n", DEVNAME(sc),
		    (notify & 0xf) * 100 / 0xf);
#endif
		return 0;
	}

	switch (notify) {
	case ASUS_NOTIFY_WirelessSwitch:	/* Handled by AML. */
		break;
	case ASUS_NOTIFY_TaskSwitch:
		break;
	case ASUS_NOTIFY_DisplayCycle:
		break;
#if NAUDIO > 0 && NWSKBD > 0
	case ASUS_NOTIFY_VolumeMute:
		workq_add_task(NULL, 0, (workq_fn)wskbd_set_mixervolume,
		    (void *)(long)0, NULL);
		break;
	case ASUS_NOTIFY_VolumeDown:
		workq_add_task(NULL, 0, (workq_fn)wskbd_set_mixervolume,
		    (void *)(long)-1, NULL);
		break;
	case ASUS_NOTIFY_VolumeUp:
		workq_add_task(NULL, 0, (workq_fn)wskbd_set_mixervolume,
		    (void *)(long)1, NULL);
		break;
#else
	case ASUS_NOTIFY_VolumeMute:
	case ASUS_NOTIFY_VolumeDown:
	case ASUS_NOTIFY_VolumeUp:
		break;
#endif
	default:
		printf("%s: unknown event 0x%02x\n", DEVNAME(sc), notify);
		break;
	}

	return 0;
}

void
acpiasus_power(int why, void *arg)
{
	struct acpiasus_softc *sc = (struct acpiasus_softc *)arg;
	struct aml_value cmd;
	struct aml_value ret;

	switch (why) {
	case PWR_STANDBY:
	case PWR_SUSPEND:
		break;
	case PWR_RESUME:
		acpiasus_init(arg);

		cmd.type = AML_OBJTYPE_INTEGER;
		cmd.v_integer = ASUS_SDSP_LCD;

		if (aml_evalname(sc->sc_acpi, sc->sc_devnode, "SDSP", 1,
		    &cmd, &ret))
			printf("%s: no SDSP\n", DEVNAME(sc));
		break;
	}
}
