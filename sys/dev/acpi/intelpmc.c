/*	$OpenBSD: intelpmc.c,v 1.1 2024/08/04 11:05:18 kettenis Exp $	*/
/*
 * Copyright (c) 2024 Mark Kettenis <kettenis@openbsd.org>
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

#define INTELPMC_DEBUG

/* Low Power S0 Idle DSM methods */
#define ACPI_LPS0_ENUM_FUNCTIONS 	0
#define ACPI_LPS0_GET_CONSTRAINTS	1
#define ACPI_LPS0_SCREEN_OFF		3
#define ACPI_LPS0_SCREEN_ON		4
#define ACPI_LPS0_ENTRY			5
#define ACPI_LPS0_EXIT			6

struct intelpmc_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_node;

#ifdef INTELPMC_DEBUG
	uint64_t		sc_c3[2];
	uint64_t		sc_c6[2];
	uint64_t		sc_c7[2];
	uint64_t		sc_pc2[2];
	uint64_t		sc_pc3[2];
	uint64_t		sc_pc6[2];
	uint64_t		sc_pc7[2];
	uint64_t		sc_pc8[2];
	uint64_t		sc_pc9[2];
	uint64_t		sc_pc10[2];
#endif
};

int	intelpmc_match(struct device *, void *, void *);
void	intelpmc_attach(struct device *, struct device *, void *);
int	intelpmc_activate(struct device *, int);

const struct cfattach intelpmc_ca = {
	sizeof (struct intelpmc_softc), intelpmc_match, intelpmc_attach,
	NULL, intelpmc_activate
};

struct cfdriver intelpmc_cd = {
	NULL, "intelpmc", DV_DULL
};

const char *intelpmc_hids[] = {
	"INT33A1",
	NULL
};

void	intelpmc_suspend(void *);
void	intelpmc_resume(void *);

int
intelpmc_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct cfdata *cf = match;

	return acpi_matchhids(aaa, intelpmc_hids, cf->cf_driver->cd_name);
}

void
intelpmc_attach(struct device *parent, struct device *self, void *aux)
{
	struct intelpmc_softc *sc = (struct intelpmc_softc *)self;
	struct acpi_attach_args *aaa = aux;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_node = aaa->aaa_node;

	printf(": %s\n", aaa->aaa_node->name);

	sc->sc_acpi->sc_pmc_suspend = intelpmc_suspend;
	sc->sc_acpi->sc_pmc_resume = intelpmc_resume;
	sc->sc_acpi->sc_pmc_cookie = sc;
}

int
intelpmc_activate(struct device *self, int act)
{
#ifdef INTELPMC_DEBUG
	struct intelpmc_softc *sc = (struct intelpmc_softc *)self;

	switch (act) {
	case DVACT_RESUME:
		printf("C3: %lld -> %lld\n", sc->sc_c3[0], sc->sc_c3[1]);
		printf("C6: %lld -> %lld\n", sc->sc_c6[0], sc->sc_c6[1]);
		printf("C7: %lld -> %lld\n", sc->sc_c7[0], sc->sc_c7[1]);
		printf("PC2: %lld -> %lld\n", sc->sc_pc2[0], sc->sc_pc2[1]);
		printf("PC3: %lld -> %lld\n", sc->sc_pc3[0], sc->sc_pc3[1]);
		printf("PC6: %lld -> %lld\n", sc->sc_pc6[0], sc->sc_pc6[1]);
		printf("PC7: %lld -> %lld\n", sc->sc_pc7[0], sc->sc_pc7[1]);
		printf("PC8: %lld -> %lld\n", sc->sc_pc8[0], sc->sc_pc8[1]);
		printf("PC9: %lld -> %lld\n", sc->sc_pc9[0], sc->sc_pc9[1]);
		printf("PC10: %lld -> %lld\n", sc->sc_pc10[0], sc->sc_pc10[1]);
		break;
	}
#endif

	return 0;
}

int
intelpmc_dsm(struct acpi_softc *sc, struct aml_node *node, int func)
{
	struct aml_value cmd[4];
	struct aml_value res;

	/* c4eb40a0-6cd2-11e2-bcfd-0800200c9a66 */
	static uint8_t lps0_dsm_guid[] = {
		0xA0, 0x40, 0xEB, 0xC4, 0xD2, 0x6C, 0xE2, 0x11,
		0xBC, 0xFD, 0x08, 0x00, 0x20, 0x0C, 0x9A, 0x66,
	};

	bzero(&cmd, sizeof(cmd));
	cmd[0].type = AML_OBJTYPE_BUFFER;
	cmd[0].v_buffer = (uint8_t *)&lps0_dsm_guid;
	cmd[0].length = sizeof(lps0_dsm_guid);
	/* rev */
	cmd[1].type = AML_OBJTYPE_INTEGER;
	cmd[1].v_integer = 0;
	cmd[1].length = 1;
	/* func */
	cmd[2].type = AML_OBJTYPE_INTEGER;
	cmd[2].v_integer = func;
	cmd[2].length = 1;
	/* not used */
	cmd[3].type = AML_OBJTYPE_PACKAGE;
	cmd[3].length = 0;

	if (aml_evalname(sc, node, "_DSM", 4, cmd, &res)) {
		printf("%s: eval of _DSM at %s failed\n",
		    sc->sc_dev.dv_xname, aml_nodename(node));
		return 1;
	}
	aml_freevalue(&res);

	return 0;
}

void
intelpmc_suspend(void *cookie)
{
	struct intelpmc_softc *sc = cookie;

	if (sc->sc_acpi->sc_state != ACPI_STATE_S0)
		return;

#ifdef INTELPMC_DEBUG
	rdmsr_safe(MSR_CORE_C3_RESIDENCY, &sc->sc_c3[0]);
	rdmsr_safe(MSR_CORE_C6_RESIDENCY, &sc->sc_c6[0]);
	rdmsr_safe(MSR_CORE_C7_RESIDENCY, &sc->sc_c7[0]);
	rdmsr_safe(MSR_PKG_C2_RESIDENCY, &sc->sc_pc2[0]);
	rdmsr_safe(MSR_PKG_C3_RESIDENCY, &sc->sc_pc3[0]);
	rdmsr_safe(MSR_PKG_C6_RESIDENCY, &sc->sc_pc6[0]);
	rdmsr_safe(MSR_PKG_C7_RESIDENCY, &sc->sc_pc7[0]);
	rdmsr_safe(MSR_PKG_C8_RESIDENCY, &sc->sc_pc8[0]);
	rdmsr_safe(MSR_PKG_C9_RESIDENCY, &sc->sc_pc9[0]);
	rdmsr_safe(MSR_PKG_C10_RESIDENCY, &sc->sc_pc10[0]);
#endif

	intelpmc_dsm(sc->sc_acpi, sc->sc_node, ACPI_LPS0_SCREEN_OFF);
	intelpmc_dsm(sc->sc_acpi, sc->sc_node, ACPI_LPS0_ENTRY);
}

void
intelpmc_resume(void *cookie)
{
	struct intelpmc_softc *sc = cookie;

	if (sc->sc_acpi->sc_state != ACPI_STATE_S0)
		return;

	intelpmc_dsm(sc->sc_acpi, sc->sc_node, ACPI_LPS0_EXIT);
	intelpmc_dsm(sc->sc_acpi, sc->sc_node, ACPI_LPS0_SCREEN_ON);

#ifdef INTELPMC_DEBUG
	rdmsr_safe(MSR_CORE_C3_RESIDENCY, &sc->sc_c3[1]);
	rdmsr_safe(MSR_CORE_C6_RESIDENCY, &sc->sc_c6[1]);
	rdmsr_safe(MSR_CORE_C7_RESIDENCY, &sc->sc_c7[1]);
	rdmsr_safe(MSR_PKG_C2_RESIDENCY, &sc->sc_pc2[1]);
	rdmsr_safe(MSR_PKG_C3_RESIDENCY, &sc->sc_pc3[1]);
	rdmsr_safe(MSR_PKG_C6_RESIDENCY, &sc->sc_pc6[1]);
	rdmsr_safe(MSR_PKG_C7_RESIDENCY, &sc->sc_pc7[1]);
	rdmsr_safe(MSR_PKG_C8_RESIDENCY, &sc->sc_pc8[1]);
	rdmsr_safe(MSR_PKG_C9_RESIDENCY, &sc->sc_pc9[1]);
	rdmsr_safe(MSR_PKG_C10_RESIDENCY, &sc->sc_pc10[1]);
#endif
}
