/*	$OpenBSD: acpipci.c,v 1.2 2018/07/10 17:11:42 kettenis Exp $	*/
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

#include <machine/bus.h>

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/ppbreg.h>

bus_addr_t pci_mcfg_addr;
int pci_mcfg_min_bus, pci_mcfg_max_bus;
bus_space_tag_t pci_mcfgt;
bus_space_handle_t pci_mcfgh;

struct acpipci_trans {
	struct acpipci_trans *at_next;
	bus_space_tag_t	at_iot;
	bus_addr_t	at_base;
	bus_size_t	at_size;
	bus_size_t	at_offset;
};

struct acpipci_softc {
	struct device	sc_dev;
	struct acpi_softc *sc_acpi;
	struct aml_node *sc_node;

	bus_space_tag_t	sc_iot;
	bus_space_handle_t sc_ioh;

	struct bus_space sc_bus_iot;
	struct bus_space sc_bus_memt;
	struct acpipci_trans *sc_io_trans;
	struct acpipci_trans *sc_mem_trans;

	struct arm64_pci_chipset sc_pc;
	struct extent	*sc_busex;
	struct extent	*sc_memex;
	struct extent	*sc_ioex;
	char		sc_busex_name[32];
	char		sc_ioex_name[32];
	char		sc_memex_name[32];
	int		sc_bus;
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
	NULL
};

int	acpipci_parse_resources(int, union acpi_resource *, void *);
int	acpipci_bs_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);

void	acpipci_attach_hook(struct device *, struct device *,
	    struct pcibus_attach_args *);
int	acpipci_bus_maxdevs(void *, int);
pcitag_t acpipci_make_tag(void *, int, int, int);
void	acpipci_decompose_tag(void *, pcitag_t, int *, int *, int *);
int	acpipci_conf_size(void *, pcitag_t);
pcireg_t acpipci_conf_read(void *, pcitag_t, int);
void	acpipci_conf_write(void *, pcitag_t, int, pcireg_t);

int	acpipci_intr_map(struct pci_attach_args *, pci_intr_handle_t *);
int	acpipci_intr_map_msi(struct pci_attach_args *, pci_intr_handle_t *);
int	acpipci_intr_map_msix(struct pci_attach_args *, int,
	    pci_intr_handle_t *);
const char *acpipci_intr_string(void *, pci_intr_handle_t);
void	*acpipci_intr_establish(void *, pci_intr_handle_t, int,
	    int (*)(void *), void *, char *);
void	acpipci_intr_disestablish(void *, void *);

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
	struct pcibus_attach_args pba;
	struct aml_value res;
	uint64_t bbn = 0;

	/* Bail out early if we don't have a valid MCFG table. */
	if (pci_mcfg_addr == 0 || pci_mcfg_max_bus <= pci_mcfg_min_bus) {
		printf(": no registers\n");
		return;
	}

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_node = aaa->aaa_node;
	printf(" %s", sc->sc_node->name);

	if (aml_evalname(sc->sc_acpi, sc->sc_node, "_CRS", 0, NULL, &res)) {
		printf(": can't find resources\n");
		return;
	}

	aml_evalinteger(sc->sc_acpi, sc->sc_node, "_BBN", 0, NULL, &bbn);
	sc->sc_bus = bbn;

	sc->sc_iot = pci_mcfgt;
	sc->sc_ioh = pci_mcfgh;

	printf("\n");

	/* Create extents for our address spaces. */
	snprintf(sc->sc_busex_name, sizeof(sc->sc_busex_name),
	    "%s pcibus", sc->sc_dev.dv_xname);
	snprintf(sc->sc_ioex_name, sizeof(sc->sc_ioex_name),
	    "%s pciio", sc->sc_dev.dv_xname);
	snprintf(sc->sc_memex_name, sizeof(sc->sc_memex_name),
	    "%s pcimem", sc->sc_dev.dv_xname);
	sc->sc_busex = extent_create(sc->sc_busex_name, 0, 255,
	    M_DEVBUF, NULL, 0, EX_WAITOK | EX_FILLED);
	sc->sc_ioex = extent_create(sc->sc_ioex_name, 0, 0xffffffff,
	    M_DEVBUF, NULL, 0, EX_WAITOK | EX_FILLED);
	sc->sc_memex = extent_create(sc->sc_memex_name, 0, (u_long)-1,
	    M_DEVBUF, NULL, 0, EX_WAITOK | EX_FILLED);

	aml_parse_resource(&res, acpipci_parse_resources, sc);

	memcpy(&sc->sc_bus_iot, sc->sc_iot, sizeof(sc->sc_bus_iot));
	sc->sc_bus_iot.bus_private = sc->sc_io_trans;
	sc->sc_bus_iot._space_map = acpipci_bs_map;
	memcpy(&sc->sc_bus_memt, sc->sc_iot, sizeof(sc->sc_bus_memt));
	sc->sc_bus_memt.bus_private = sc->sc_mem_trans;
	sc->sc_bus_memt._space_map = acpipci_bs_map;

	sc->sc_pc.pc_conf_v = sc;
	sc->sc_pc.pc_attach_hook = acpipci_attach_hook;
	sc->sc_pc.pc_bus_maxdevs = acpipci_bus_maxdevs;
	sc->sc_pc.pc_make_tag = acpipci_make_tag;
	sc->sc_pc.pc_decompose_tag = acpipci_decompose_tag;
	sc->sc_pc.pc_conf_size = acpipci_conf_size;
	sc->sc_pc.pc_conf_read = acpipci_conf_read;
	sc->sc_pc.pc_conf_write = acpipci_conf_write;

	sc->sc_pc.pc_intr_v = sc;
	sc->sc_pc.pc_intr_map = acpipci_intr_map;
	sc->sc_pc.pc_intr_map_msi = acpipci_intr_map_msi;
	sc->sc_pc.pc_intr_map_msix = acpipci_intr_map_msix;
	sc->sc_pc.pc_intr_string = acpipci_intr_string;
	sc->sc_pc.pc_intr_establish = acpipci_intr_establish;
	sc->sc_pc.pc_intr_disestablish = acpipci_intr_disestablish;

	memset(&pba, 0, sizeof(pba));
	pba.pba_busname = "pci";
	pba.pba_iot = &sc->sc_bus_iot;
	pba.pba_memt = &sc->sc_bus_memt;
	pba.pba_dmat = aaa->aaa_dmat;
	pba.pba_pc = &sc->sc_pc;
	pba.pba_busex = sc->sc_busex;
	pba.pba_ioex = sc->sc_ioex;
	pba.pba_memex = sc->sc_memex;
	pba.pba_domain = pci_ndomains++;
	pba.pba_bus = sc->sc_bus;

	config_found(self, &pba, NULL);
}

int
acpipci_parse_resources(int crsidx, union acpi_resource *crs, void *arg)
{
	struct acpipci_softc *sc = arg;
	struct acpipci_trans *at;
	int type = AML_CRSTYPE(crs);
	int restype, tflags;
	u_long min, len = 0, tra;

	switch (type) {
	case LR_WORD:
		restype = crs->lr_word.type;
		tflags = crs->lr_word.tflags;
		min = crs->lr_word._min;
		len = crs->lr_word._len;
		tra = crs->lr_word._tra;
		break;
	case LR_DWORD:
		restype = crs->lr_dword.type;
		tflags = crs->lr_dword.tflags;
		min = crs->lr_dword._min;
		len = crs->lr_dword._len;
		tra = crs->lr_dword._tra;
		break;
	case LR_QWORD:
		restype = crs->lr_qword.type;
		tflags = crs->lr_qword.tflags;
		min = crs->lr_qword._min;
		len = crs->lr_qword._len;
		tra = crs->lr_qword._tra;
		break;
	}

	if (len == 0)
		return 0;

	switch (restype) {
	case LR_TYPE_MEMORY:
		if (tflags & LR_MEMORY_TTP)
			return 0;
		extent_free(sc->sc_memex, min, len, EX_WAITOK);
		at = malloc(sizeof(struct acpipci_trans), M_DEVBUF, M_WAITOK);
		at->at_iot = sc->sc_iot;
		at->at_base = min;
		at->at_size = len;
		at->at_offset = tra;
		at->at_next = sc->sc_mem_trans;
		sc->sc_mem_trans = at;
		break;
	case LR_TYPE_IO:
		if ((tflags & LR_IO_TTP) == 0)
			return 0;
		extent_free(sc->sc_ioex, min, len, EX_WAITOK);
		at = malloc(sizeof(struct acpipci_trans), M_DEVBUF, M_WAITOK);
		at->at_iot = sc->sc_iot;
		at->at_base = min;
		at->at_size = len;
		at->at_offset = tra;
		at->at_next = sc->sc_io_trans;
		sc->sc_io_trans = at;
		break;
	case LR_TYPE_BUS:
		extent_free(sc->sc_busex, min, len, EX_WAITOK);
		break;
	}

	return 0;
}

void
acpipci_attach_hook(struct device *parent, struct device *self,
    struct pcibus_attach_args *pba)
{
}

int
acpipci_bus_maxdevs(void *v, int bus)
{
	return 32;
}

pcitag_t
acpipci_make_tag(void *v, int bus, int device, int function)
{
	return ((bus << 20) | (device << 15) | (function << 12));
}

void
acpipci_decompose_tag(void *v, pcitag_t tag, int *bp, int *dp, int *fp)
{
	if (bp != NULL)
		*bp = (tag >> 20) & 0xff;
	if (dp != NULL)
		*dp = (tag >> 15) & 0x1f;
	if (fp != NULL)
		*fp = (tag >> 12) & 0x7;
}

int
acpipci_conf_size(void *v, pcitag_t tag)
{
	return PCIE_CONFIG_SPACE_SIZE;
}

pcireg_t
acpipci_conf_read(void *v, pcitag_t tag, int reg)
{
	struct acpipci_softc *sc = v;

	return bus_space_read_4(sc->sc_iot, sc->sc_ioh, tag | reg);
}

void
acpipci_conf_write(void *v, pcitag_t tag, int reg, pcireg_t data)
{
	struct acpipci_softc *sc = v;

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, tag | reg, data);
}

struct acpipci_intr_handle {
	pci_chipset_tag_t	ih_pc;
	pcitag_t		ih_tag;
	int			ih_intrpin;
	int			ih_msi;
};

int
acpipci_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	struct acpipci_softc *sc = pa->pa_pc->pc_intr_v;
	struct aml_node *node;
	struct aml_value res;
	uint64_t addr, pin, source, index;
	struct acpipci_intr_handle *ih;
	int i;

	if (pa->pa_bridgetag == NULL)
		return -1;

	node = acpi_find_pci(pa->pa_pc, *pa->pa_bridgetag);
	if (node == NULL)
		return -1;

	if (aml_evalname(sc->sc_acpi, node, "_PRT", 0, NULL, &res))
		return -1;

	if (res.type != AML_OBJTYPE_PACKAGE)
		return -1;

	for (i = 0; i < res.length; i++) {
		struct aml_value *val = res.v_package[i];

		if (val->type != AML_OBJTYPE_PACKAGE)
			continue;
		if (val->length != 4)
			continue;
		if (val->v_package[0]->type != AML_OBJTYPE_INTEGER ||
		    val->v_package[1]->type != AML_OBJTYPE_INTEGER ||
		    val->v_package[2]->type != AML_OBJTYPE_INTEGER ||
		    val->v_package[3]->type != AML_OBJTYPE_INTEGER)
			continue;
		    
		addr = val->v_package[0]->v_integer;
		pin = val->v_package[1]->v_integer;
		source = val->v_package[2]->v_integer;
		index = val->v_package[3]->v_integer;
		if (ACPI_ADR_PCIDEV(addr) != pa->pa_device ||
		    ACPI_ADR_PCIFUN(addr) != 0xffff ||
		    pin != pa->pa_intrpin - 1 || source != 0)
			continue;
		
		ih = malloc(sizeof(struct acpipci_intr_handle),
		    M_DEVBUF, M_WAITOK);
		ih->ih_pc = pa->pa_pc;
		ih->ih_tag = pa->pa_tag;
		ih->ih_intrpin = index;
		ih->ih_msi = 0;
		*ihp = (pci_intr_handle_t)ih;

		return 0;
	}

	return -1;
}

int
acpipci_intr_map_msi(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	pci_chipset_tag_t pc = pa->pa_pc;
	pcitag_t tag = pa->pa_tag;
	struct acpipci_intr_handle *ih;

	if (pci_get_capability(pc, tag, PCI_CAP_MSI, NULL, NULL) == 0)
		return -1;

	ih = malloc(sizeof(struct acpipci_intr_handle), M_DEVBUF, M_WAITOK);
	ih->ih_pc = pa->pa_pc;
	ih->ih_tag = pa->pa_tag;
	ih->ih_intrpin = pa->pa_intrpin;
	ih->ih_msi = 1;
	*ihp = (pci_intr_handle_t)ih;

	return 0;
}

int
acpipci_intr_map_msix(struct pci_attach_args *pa, int vec,
    pci_intr_handle_t *ihp)
{
	return -1;
}

const char *
acpipci_intr_string(void *v, pci_intr_handle_t ihp)
{
	struct acpipci_intr_handle *ih = (struct acpipci_intr_handle *)ihp;
	static char irqstr[32];

	if (ih->ih_msi)
		return "msi";

	snprintf(irqstr, sizeof(irqstr), "irq %d", ih->ih_intrpin);
	return irqstr;
}

void *
acpipci_intr_establish(void *v, pci_intr_handle_t ihp, int level,
    int (*func)(void *), void *arg, char *name)
{
	struct acpipci_intr_handle *ih = (struct acpipci_intr_handle *)ihp;
	struct interrupt_controller *ic;
	void *cookie;

	extern LIST_HEAD(, interrupt_controller) interrupt_controllers;
	LIST_FOREACH(ic, &interrupt_controllers, ic_list) {
		if (ic->ic_establish_msi)
			break;
	}
	if (ic == NULL)
		return NULL;
	
	if (ih->ih_msi) {
		uint64_t addr, data;
		pcireg_t reg;
		int off;

		cookie = ic->ic_establish_msi(ic->ic_cookie, &addr,
		    &data, level, func, arg, name);
		if (cookie == NULL)
			return NULL;

		/* TODO: translate address to the PCI device's view */

		if (pci_get_capability(ih->ih_pc, ih->ih_tag, PCI_CAP_MSI,
		    &off, &reg) == 0)
			panic("%s: no msi capability", __func__);

		if (reg & PCI_MSI_MC_C64) {
			pci_conf_write(ih->ih_pc, ih->ih_tag,
			    off + PCI_MSI_MA, addr);
			pci_conf_write(ih->ih_pc, ih->ih_tag,
			    off + PCI_MSI_MAU32, addr >> 32);
			pci_conf_write(ih->ih_pc, ih->ih_tag,
			    off + PCI_MSI_MD64, data);
		} else {
			pci_conf_write(ih->ih_pc, ih->ih_tag,
			    off + PCI_MSI_MA, addr);
			pci_conf_write(ih->ih_pc, ih->ih_tag,
			    off + PCI_MSI_MD32, data);
		}
		pci_conf_write(ih->ih_pc, ih->ih_tag,
		    off, reg | PCI_MSI_MC_MSIE);
	} else {
		cookie = acpi_intr_establish(ih->ih_intrpin, 0, level,
		    func, arg, name);
	}

	free(ih, M_DEVBUF, sizeof(struct acpipci_intr_handle));
	return cookie;
}

void
acpipci_intr_disestablish(void *v, void *cookie)
{
	panic("%s", __func__);
}

/*
 * Translate memory address if needed.
 */
int
acpipci_bs_map(bus_space_tag_t t, bus_addr_t addr, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	struct acpipci_trans *at;

	for (at = t->bus_private; at; at = at->at_next) {
		if (addr >= at->at_base && addr < at->at_base + at->at_size) {
			return bus_space_map(at->at_iot,
			    addr + at->at_offset, size, flags, bshp);
		}
	}
	
	return ENXIO;
}

struct arm64_pci_chipset pci_mcfg_chipset;

pcireg_t
pci_mcfg_conf_read(void *v, pcitag_t tag, int reg)
{
	return bus_space_read_4(pci_mcfgt, pci_mcfgh, tag | reg);
}

void
pci_mcfg_conf_write(void *v, pcitag_t tag, int reg, pcireg_t data)
{
	bus_space_write_4(pci_mcfgt, pci_mcfgh, tag | reg, data);
}

pci_chipset_tag_t
pci_mcfg_init(bus_space_tag_t iot, bus_addr_t addr, int min_bus, int max_bus)
{
	pci_chipset_tag_t pc = &pci_mcfg_chipset;

	pci_mcfgt = iot;
	pci_mcfg_addr = addr;
	pci_mcfg_min_bus = min_bus;
	pci_mcfg_max_bus = max_bus;

	if (bus_space_map(iot, addr, (pci_mcfg_max_bus + 1) << 20, 0,
	    &pci_mcfgh))
		panic("%s: can't map config space", __func__);

	memset(pc, 0, sizeof(*pc));
	pc->pc_bus_maxdevs = acpipci_bus_maxdevs;
	pc->pc_make_tag = acpipci_make_tag;
	pc->pc_decompose_tag = acpipci_decompose_tag;
	pc->pc_conf_size = acpipci_conf_size;
	pc->pc_conf_read = pci_mcfg_conf_read;
	pc->pc_conf_write = pci_mcfg_conf_write;

	return pc;
}
