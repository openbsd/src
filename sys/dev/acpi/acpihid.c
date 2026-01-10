/*	$OpenBSD: acpihid.c,v 1.6 2026/01/10 16:12:36 kettenis Exp $	*/
/*
 * Copyright (c) 2025 Mark Kettenis <kettenis@openbsd.org>
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

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include <dev/hid/hid.h>

struct acpihid_softc;

struct acpihid_gpio {
	struct acpihid_softc	*ag_sc;
	struct aml_node		*ag_node;
	int			ag_pin;
	int			ag_flags;
	uint16_t		ag_usage_page;
	uint16_t		ag_usage_id;
	int			ag_last_state;
};

struct acpihid_softc {
	struct device		sc_dev;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_node;

	struct acpihid_gpio	*sc_gpios;
	u_int			sc_ngpios;
};

int	acpihid_match(struct device *, void *, void *);
void	acpihid_attach(struct device *, struct device *, void *);

const struct cfattach acpihid_ca = {
	sizeof (struct acpihid_softc), acpihid_match, acpihid_attach
};

struct cfdriver acpihid_cd = {
	NULL, "acpihid", DV_DULL
};

const char *acpihid_hids[] = {
	"ACPI0011",
	NULL
};

uint8_t acpihid_uuid[] =
    ACPI_UUID(0xfa6bd625, 0x9ce8, 0x470d, 0xa2c7, 0xb3ca36c4282e);

void	acpihid_attach_deferred(struct device *);
int	acpihid_count_gpios(int, union acpi_resource *, void *);
int	acpihid_get_gpios(int, union acpi_resource *, void *);
int	acpihid_powerbutton(void *);

int
acpihid_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct cfdata *cf = match;

	return acpi_matchhids(aaa, acpihid_hids, cf->cf_driver->cd_name);
}

void
acpihid_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpihid_softc *sc = (struct acpihid_softc *)self;
	struct acpi_attach_args *aaa = aux;
	struct aml_value res;
	int i;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_node = aaa->aaa_node;

	printf(": %s\n", aaa->aaa_node->name);

	if (aml_evalname(sc->sc_acpi, sc->sc_node, "_CRS",
	    0, NULL, &res) != 0) {
		printf("%s: missing _CRS\n", sc->sc_dev.dv_xname);
		return;
	}
	if (res.type != AML_OBJTYPE_BUFFER || res.length < 5) {
		printf("%s: invalid _CRS\n", sc->sc_dev.dv_xname);
		goto free;
	}
	aml_parse_resource(&res, acpihid_count_gpios, sc);
	sc->sc_gpios = mallocarray(sc->sc_ngpios, sizeof(*sc->sc_gpios),
	    M_DEVBUF, M_WAITOK | M_ZERO);
	sc->sc_ngpios = 0;
	aml_parse_resource(&res, acpihid_get_gpios, sc);
	aml_freevalue(&res);

	if (aml_evalname(sc->sc_acpi, sc->sc_node, "_DSD",
	    0, NULL, &res) != 0) {
		printf("%s: missing _DSD\n", sc->sc_dev.dv_xname);
		goto free;
	}
	if (res.type != AML_OBJTYPE_PACKAGE || res.length != 2 ||
	    res.v_package[0]->type != AML_OBJTYPE_BUFFER ||
	    res.v_package[0]->length != 16 ||
	    res.v_package[1]->type != AML_OBJTYPE_PACKAGE) {
		printf("%s: invalid _DSD\n", sc->sc_dev.dv_xname);
		goto free;
	}

	if (memcmp(res.v_package[0]->v_buffer, acpihid_uuid,
	    sizeof(acpihid_uuid)) != 0) {
		printf("%s: invalid _DSD UUID\n", sc->sc_dev.dv_xname);
		goto free;
	}

	for (i = 0; i < res.v_package[1]->length; i++) {
		struct aml_value *hid = res.v_package[1]->v_package[i];
		u_int idx;

		if (hid->type != AML_OBJTYPE_PACKAGE ||  hid->length != 5 ||
		    hid->v_package[0]->type != AML_OBJTYPE_INTEGER ||
		    hid->v_package[1]->type != AML_OBJTYPE_INTEGER ||
		    hid->v_package[2]->type != AML_OBJTYPE_INTEGER ||
		    hid->v_package[3]->type != AML_OBJTYPE_INTEGER ||
		    hid->v_package[4]->type != AML_OBJTYPE_INTEGER)
			continue;
		if (hid->v_package[0]->v_integer != 1)
			continue;
		if (hid->v_package[1]->v_integer < 0 ||
		    hid->v_package[1]->v_integer >= sc->sc_ngpios)
			continue;
		idx = hid->v_package[1]->v_integer;

		sc->sc_gpios[idx].ag_usage_page = hid->v_package[3]->v_integer;
		sc->sc_gpios[idx].ag_usage_id = hid->v_package[4]->v_integer;
	}
	aml_freevalue(&res);

	/*
	 * The relevant GPIO driver might not have been attached yet,
	 * so defer installing our interrupt handlers.
	 */
	config_defer(self, acpihid_attach_deferred);
	return;

free:
	free(sc->sc_gpios, M_DEVBUF, sc->sc_ngpios * sizeof(*sc->sc_gpios));
	aml_freevalue(&res);
}

void
acpihid_attach_deferred(struct device *self)
{
	struct acpihid_softc *sc = (struct acpihid_softc *)self;
	u_int idx;

	for (idx = 0; idx < sc->sc_ngpios; idx++) {
		struct acpi_gpio *gpio;
		int pin, flags;

		/* Skip if we still have no GPIO driver. */
		if (sc->sc_gpios[idx].ag_node->gpio == NULL)
			continue;

		gpio = sc->sc_gpios[idx].ag_node->gpio;
		flags = sc->sc_gpios[idx].ag_flags;
		pin = sc->sc_gpios[idx].ag_pin;

		if (sc->sc_gpios[idx].ag_usage_page == HUP_GENERIC_DESKTOP &&
		    sc->sc_gpios[idx].ag_usage_id == HUG_SYSTEM_POWER_DOWN) {
			/* Power Button */
			gpio->intr_establish(gpio->cookie, pin, flags,
			    IPL_TTY, acpihid_powerbutton, &sc->sc_gpios[idx]);
		}
	}
}

int
acpihid_count_gpios(int crsidx, union acpi_resource *crs, void *arg)
{
	struct acpihid_softc *sc = arg;

	switch (AML_CRSTYPE(crs)) {
	case LR_GPIO:
		if (crs->lr_gpio.type == LR_GPIO_INT)
			sc->sc_ngpios++;
		break;
	}

	return 0;
}

int
acpihid_get_gpios(int crsidx, union acpi_resource *crs, void *arg)
{
	struct acpihid_softc *sc = arg;
	struct aml_node *node;
	int pin;

	switch (AML_CRSTYPE(crs)) {
	case LR_GPIO:
		if (crs->lr_gpio.type != LR_GPIO_INT)
			break;
		node = aml_searchname(sc->sc_node,
		    (char *)&crs->pad[crs->lr_gpio.res_off]);
		pin = *(uint16_t *)&crs->pad[crs->lr_gpio.pin_off];
		sc->sc_gpios[sc->sc_ngpios].ag_node = node;
		sc->sc_gpios[sc->sc_ngpios].ag_flags = crs->lr_gpio.tflags;
		sc->sc_gpios[sc->sc_ngpios].ag_pin = pin;
		sc->sc_ngpios++;
		break;
	}

	return 0;
}

int
acpihid_powerbutton(void *arg)
{
	struct acpihid_gpio *ag = arg;
	struct acpi_gpio *gpio = ag->ag_node->gpio;
	int state;

	/* Power button is active-low. */
	state = !gpio->read_pin(gpio->cookie, ag->ag_pin);

	/* Generate event on button release. */
	if (ag->ag_last_state && !state)
		powerbutton_event();

	ag->ag_last_state = state;
	return 0;
}
