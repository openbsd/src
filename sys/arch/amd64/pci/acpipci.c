/*	$OpenBSD: acpipci.c,v 1.1 2018/10/26 20:26:19 kettenis Exp $	*/
/*
 * Copyright (c) 2018 Mark Kettenis
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
#include <sys/device.h>
#include <sys/extent.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

/* 33DB4D5B-1FF7-401C-9657-7441C03DD766 */
#define ACPI_PCI_UUID \
  { 0x5b, 0x4d, 0xdb, 0x33, \
    0xf7, 0x1f, \
    0x1c, 0x40, \
    0x96, 0x57, \
    0x74, 0x41, 0xc0, 0x3d, 0xd7, 0x66 }

/* Support field. */
#define ACPI_PCI_PCIE_CONFIG	0x00000001
#define ACPI_PCI_ASPM		0x00000002
#define ACPI_PCI_CPMC		0x00000004
#define ACPI_PCI_SEGMENTS	0x00000008
#define ACPI_PCI_MSI		0x00000010

/* Control field. */
#define ACPI_PCI_PCIE_HOTPLUG	0x00000001

struct acpipci_softc {
	struct device	sc_dev;
	struct acpi_softc *sc_acpi;
	struct aml_node *sc_node;
};

int	acpipci_match(struct device *, void *, void *);
void	acpipci_attach(struct device *, struct device *, void *);

struct cfattach acpipci_ca = {
	sizeof(struct acpipci_softc), acpipci_match, acpipci_attach
};

struct cfdriver acpipci_cd = {
	NULL, "acpipci", DV_DULL
};

const char *acpipci_hids[] = {
	"PNP0A08",
	"PNP0A03",
	NULL
};

int
acpipci_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct cfdata *cf = match;

	return acpi_matchhids(aaa, acpipci_hids, cf->cf_driver->cd_name);
}

void
acpipci_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct acpipci_softc *sc = (struct acpipci_softc *)self;
	struct aml_value args[4];
	struct aml_value res;
	static uint8_t uuid[16] = ACPI_PCI_UUID;
	uint32_t buf[3];

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_node = aaa->aaa_node;
	printf(" %s", sc->sc_node->name);

	memset(args, 0, sizeof(args));
	args[0].type = AML_OBJTYPE_BUFFER;
	args[0].v_buffer = uuid;
	args[0].length = sizeof(uuid);
	args[1].type = AML_OBJTYPE_INTEGER;
	args[1].v_integer = 1;
	args[2].type = AML_OBJTYPE_INTEGER;
	args[2].v_integer = 3;
	args[3].type = AML_OBJTYPE_BUFFER;
	args[3].v_buffer = (uint8_t *)buf;
	args[3].length = sizeof(buf);

	memset(buf, 0, sizeof(buf));
	buf[0] = 0x0;
	buf[1] = ACPI_PCI_PCIE_CONFIG | ACPI_PCI_MSI;
	buf[2] = ACPI_PCI_PCIE_HOTPLUG;

	if (aml_evalname(sc->sc_acpi, sc->sc_node, "_OSC", 4, args, &res)) {
		printf(": _OSC failed\n");
		return;
	}

	if (res.type == AML_OBJTYPE_BUFFER) {
		size_t len = res.length;
		uint32_t *p = (uint32_t *)res.v_buffer;

		printf(":");
		while (len >= 4) {
			printf(" 0x%08x", *p);
			p++;
			len -= 4;
		}
	}

	printf("\n");
}
