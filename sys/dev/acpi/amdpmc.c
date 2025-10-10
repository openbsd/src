/*	$OpenBSD: amdpmc.c,v 1.2 2025/10/10 16:12:58 kettenis Exp $	*/
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

#include <dev/pci/pcidevs.h>

#define SMN_ADDR		0x60
#define  SMN_PMC_BASE_ADDR_LO		0x13b102e8
#define  SMN_PMC_BASE_ADDR_HI		0x13b102ec
#define  SMN_PMC_BASE_ADDR_LO_MASK	0x0000ffff
#define  SMN_PMC_BASE_ADDR_HI_MASK	0xfff00000
#define  SMN_PMC_SIZE			0x1000
#define  SMN_PMC_BASE_ADDR_OFFSET	0x00010000
#define SMN_DATA		0x64

#define PMC_MSG			0x538
#define PMC_RESP		0x980
#define  PMC_RESP_OK		0x01
#define  PMC_RESP_BUSY		0xfc
#define PMC_ARG			0x9bc

#define SMU_MSG_GETVERSION		0x02
#define SMU_MSG_OS_HINT			0x03
#define SMU_MSG_LOG_GETDRAM_ADDR_HI	0x04
#define SMU_MSG_LOG_GETDRAM_ADDR_LO	0x05
#define SMU_MSG_LOG_START		0x06
#define SMU_MSG_LOG_RESET		0x07
#define SMU_MSG_LOG_DUMP_DATA		0x08
#define SMU_MSG_GET_SUP_CONSTRAINTS	0x09

struct smu_metrics {
	uint32_t	table_version;
	uint32_t	hint_count;
	uint32_t	s0i3_last_entry_status;
	uint32_t	timein_s0i2;
	uint64_t	timeentering_s0i3_lastcapture;
	uint64_t	timeentering_s0i3_totaltime;
	uint64_t	timeto_resume_to_os_lastcapture;
	uint64_t	timeto_resume_to_os_totaltime;
	uint64_t	timein_s0i3_lastcapture;
	uint64_t	timein_s0i3_totaltime;
	uint64_t	timein_swdrips_lastcapture;
	uint64_t	timein_swdrips_totaltime;
	uint64_t	timecondition_notmet_lastcapture[32];
	uint64_t	timecondition_notmet_totaltime[32];
};

const char *smu_blocks[] = {
	"DISPLAY",
	"CPU",
	"GFX",
	"VDD",
	"ACP",
	"VCN",
	"ISP",
	"NBIO",
	"DF",
	"USB3_0",
	"USB3_1",
	"LAPIC",
	"USB3_2",
	"USB3_3",
	"USB3_4",
	"USB4_0",
	"USB4_1",
	"MPM",
	"JPEG",
	"IPU",
	"UMSCH",
	"VPE",
};

/* Low Power S0 Idle DSM methods */
#define ACPI_LPS0_ENUM_FUNCTIONS 	0
#define ACPI_LPS0_GET_CONSTRAINTS	1
#define ACPI_LPS0_ENTRY			2
#define ACPI_LPS0_EXIT			3
#define ACPI_LPS0_SCREEN_OFF		4
#define ACPI_LPS0_SCREEN_ON		5

struct amdpmc_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;

	struct acpi_softc	*sc_acpi;
	struct aml_node		*sc_node;

	struct smu_metrics	*sc_metrics;
	uint32_t		sc_active_blocks;
};

int	amdpmc_match(struct device *, void *, void *);
void	amdpmc_attach(struct device *, struct device *, void *);
int	amdpmc_activate(struct device *, int);

const struct cfattach amdpmc_ca = {
	sizeof (struct amdpmc_softc), amdpmc_match, amdpmc_attach,
	NULL, amdpmc_activate
};

struct cfdriver amdpmc_cd = {
	NULL, "amdpmc", DV_DULL
};

const char *amdpmc_hids[] = {
	"AMDI0005",
	"AMDI0006",
	"AMDI0007",
	"AMDI0008",
	"AMDI0009",
	"AMDI000A",
	"AMDI000B",
	NULL
};

void	amdpmc_suspend(void *);
void	amdpmc_resume(void *);

void	amdpmc_smu_log_print(struct amdpmc_softc *);
int	amdpmc_send_msg(struct amdpmc_softc *, uint8_t, uint32_t,  uint32_t *);

int
amdpmc_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct cfdata *cf = match;

	return acpi_matchhids(aaa, amdpmc_hids, cf->cf_driver->cd_name);
}

void
amdpmc_attach(struct device *parent, struct device *self, void *aux)
{
	struct amdpmc_softc *sc = (struct amdpmc_softc *)self;
	struct acpi_attach_args *aaa = aux;
	pci_chipset_tag_t pc;
	pcitag_t tag;
	pcireg_t reg;
	bus_space_handle_t ioh;
	uint32_t addr_lo, addr_hi;
	bus_addr_t addr;
	uint32_t version;
	int error;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_node = aaa->aaa_node;

	printf(": %s\n", aaa->aaa_node->name);

	sc->sc_acpi->sc_pmc_suspend = amdpmc_suspend;
	sc->sc_acpi->sc_pmc_resume = amdpmc_resume;
	sc->sc_acpi->sc_pmc_cookie = sc;

	pc = pci_lookup_segment(0, 0);
	tag = pci_make_tag(pc, 0, 0, 0);
	reg = pci_conf_read(pc, tag, PCI_ID_REG);
	KASSERT(PCI_VENDOR(reg) == PCI_VENDOR_AMD);

	switch (PCI_PRODUCT(reg)) {
	case PCI_PRODUCT_AMD_17_6X_RC: /* RN/CZN */
	case PCI_PRODUCT_AMD_19_4X_RC: /* YC */
	case PCI_PRODUCT_AMD_19_6X_RC: /* CB */
	case PCI_PRODUCT_AMD_19_7X_RC: /* PS */
		/* Supported */
		break;
	case PCI_PRODUCT_AMD_17_1X_RC: /* RV/PCO */
	case PCI_PRODUCT_AMD_19_1X_RC: /* SP */
	default:
		/* Unsupported */
		return;
	}

	pci_conf_write(pc, tag, SMN_ADDR, SMN_PMC_BASE_ADDR_LO);
	addr_lo = pci_conf_read(pc, tag, SMN_DATA);
	pci_conf_write(pc, tag, SMN_ADDR, SMN_PMC_BASE_ADDR_HI);
	addr_hi = pci_conf_read(pc, tag, SMN_DATA);
	if (addr_lo == 0xffffffff || addr_hi == 0xffffffff)
		return;

	addr_lo &= SMN_PMC_BASE_ADDR_HI_MASK;
	addr_hi &= SMN_PMC_BASE_ADDR_LO_MASK;
	addr = (uint64_t)addr_hi << 32 | addr_lo;

	sc->sc_iot = aaa->aaa_memt;
	if (bus_space_map(sc->sc_iot, addr + SMN_PMC_BASE_ADDR_OFFSET,
	    SMN_PMC_SIZE, 0, &sc->sc_ioh)) {
		printf("%s: can't map SMU registers\n", sc->sc_dev.dv_xname);
		return;
	}

	error = amdpmc_send_msg(sc, SMU_MSG_GETVERSION, 0, &version);
	if (error) {
		printf("%s: can't read SMU version\n", sc->sc_dev.dv_xname);
		return;
	}

	printf("%s: SMU program %u version %u.%u.%u\n", sc->sc_dev.dv_xname,
	    (version >> 24) & 0xff, (version >> 16) & 0xff,
	    (version >> 8) & 0xff, (version >> 0) & 0xff);

	error = amdpmc_send_msg(sc, SMU_MSG_GET_SUP_CONSTRAINTS, 0,
	    &sc->sc_active_blocks);
	if (error) {
		printf("%s: can't read active blocks\n", sc->sc_dev.dv_xname);
		return;
	}

	error = amdpmc_send_msg(sc, SMU_MSG_LOG_GETDRAM_ADDR_LO, 0, &addr_lo);
	if (error) {
		printf("%s: can't read SMU DRAM address (%d)\n",
		    sc->sc_dev.dv_xname, error);
		return;
	}
	error = amdpmc_send_msg(sc, SMU_MSG_LOG_GETDRAM_ADDR_HI, 0, &addr_hi);
	if (error) {
		printf("%s: can't read SMU DRAM address (%d)\n",
		    sc->sc_dev.dv_xname, error);
		return;
	}

	addr = (uint64_t)addr_hi << 32 | addr_lo;
	if (bus_space_map(sc->sc_iot, addr, sizeof(struct smu_metrics),
			  0, &ioh)) {
		printf("%s: can't map SMU metrics\n", sc->sc_dev.dv_xname);
		return;
	}

	sc->sc_metrics = bus_space_vaddr(sc->sc_iot, ioh);
	memset(sc->sc_metrics, 0, sizeof(struct smu_metrics));
	error = amdpmc_send_msg(sc, SMU_MSG_LOG_RESET, 0, NULL);
	if (error) {
		printf("%s: can't reset SMU message log (%d)\n",
		    sc->sc_dev.dv_xname, error);
		return;
	}
	error = amdpmc_send_msg(sc, SMU_MSG_LOG_START, 0, NULL);
	if (error) {
		printf("%s: can't start SMU message log (%d)\n",
		    sc->sc_dev.dv_xname, error);
		return;
	}
}

void
amdpmc_smu_log_dump(struct amdpmc_softc *sc)
{
	int error;

	error = amdpmc_send_msg(sc, SMU_MSG_LOG_DUMP_DATA, 0, NULL);
	if (error) {
		printf("%s: can't dump SMU message log (%d)\n",
		    sc->sc_dev.dv_xname, error);
		return;
	}
}

void
amdpmc_smu_log_print(struct amdpmc_softc *sc)
{
	const char *devname = sc->sc_dev.dv_xname;
	int i;

	printf("%s: SMU table_version %u\n", devname,
	    sc->sc_metrics->table_version);
	printf("%s: SMU hint_count %u\n", devname,
	     sc->sc_metrics->hint_count);
	printf("%s: SMU s0i3_last_entry_status %u\n", devname,
	    sc->sc_metrics->s0i3_last_entry_status);
	printf("%s: SMU timeentering_s0i3_lastcapture %llu\n", devname,
	    sc->sc_metrics->timeentering_s0i3_lastcapture);
	printf("%s: SMU timein_s0i3_lastcapture %llu\n", devname,
	    sc->sc_metrics->timein_s0i3_lastcapture);
	printf("%s: SMU timeto_resume_to_os_lastcapture %llu\n", devname,
	    sc->sc_metrics->timeto_resume_to_os_lastcapture);
	for (i = 0; i < nitems(smu_blocks); i++) {
		if ((sc->sc_active_blocks & (1U << i)) == 0)
			continue;
		if (sc->sc_metrics->timecondition_notmet_lastcapture[i] == 0)
			continue;
		printf("%s: SMU %s: %llu\n", devname, smu_blocks[i],
		    sc->sc_metrics->timecondition_notmet_lastcapture[i]);
	}
}

int
amdpmc_activate(struct device *self, int act)
{
	struct amdpmc_softc *sc = (struct amdpmc_softc *)self;

	switch (act) {
	case DVACT_SUSPEND:
		break;
	case DVACT_RESUME:
		if (sc->sc_metrics)
			amdpmc_smu_log_print(sc);
		break;
	}

	return 0;
}

int
amdpmc_dsm(struct acpi_softc *sc, struct aml_node *node, int func)
{
	struct aml_value cmd[4];
	struct aml_value res;

	/* e3f32452-febc-43ce-9039-932122d37721 */
	static uint8_t lps0_dsm_guid[] = {
		0x52, 0x24, 0xF3, 0xE3, 0xBC, 0xFE, 0xCE, 0x43,
		0x90, 0x39, 0x93, 0x21, 0x22, 0xD3, 0x77, 0x21,
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
amdpmc_suspend(void *cookie)
{
	struct amdpmc_softc *sc = cookie;

	if (sc->sc_acpi->sc_state != ACPI_STATE_S0)
		return;

	if (sc->sc_metrics) {
		memset(sc->sc_metrics, 0, sizeof(struct smu_metrics));
		amdpmc_send_msg(sc, SMU_MSG_LOG_RESET, 0, NULL);
		amdpmc_send_msg(sc, SMU_MSG_LOG_START, 0, NULL);
		amdpmc_send_msg(sc, SMU_MSG_OS_HINT, 1, NULL);
	}

	amdpmc_dsm(sc->sc_acpi, sc->sc_node, ACPI_LPS0_SCREEN_OFF);
	amdpmc_dsm(sc->sc_acpi, sc->sc_node, ACPI_LPS0_ENTRY);
}

void
amdpmc_resume(void *cookie)
{
	struct amdpmc_softc *sc = cookie;

	if (sc->sc_acpi->sc_state != ACPI_STATE_S0)
		return;

	amdpmc_dsm(sc->sc_acpi, sc->sc_node, ACPI_LPS0_EXIT);
	amdpmc_dsm(sc->sc_acpi, sc->sc_node, ACPI_LPS0_SCREEN_ON);

	if (sc->sc_metrics) {
		amdpmc_send_msg(sc, SMU_MSG_OS_HINT, 0, NULL);
		amdpmc_smu_log_dump(sc);
	}
}

int
amdpmc_send_msg(struct amdpmc_softc *sc, uint8_t msg, uint32_t arg,
    uint32_t *data)
{
	uint32_t val;
	int timo;

	for (timo = 1000000; timo > 0; timo -= 50) {
		val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, PMC_RESP);
		if (val)
			break;
		delay(50);
	}
	if (timo == 0)
		return ETIMEDOUT;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, PMC_RESP, 0);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, PMC_ARG, arg);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, PMC_MSG, msg);

	for (timo = 1000000; timo > 0; timo -= 50) {
		val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, PMC_RESP);
		if (val)
			break;
		delay(50);
	}
	if (timo == 0)
		return ETIMEDOUT;

	switch (val) {
	case PMC_RESP_OK:
		if (data) {
			delay(50);
			*data = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
			    PMC_ARG);
		}
		return 0;
	case PMC_RESP_BUSY:
		return EBUSY;
	default:
		return EIO;
	}
}
