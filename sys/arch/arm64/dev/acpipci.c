/*	$OpenBSD: acpipci.c,v 1.7 2018/08/19 08:23:47 kettenis Exp $	*/
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

struct acpipci_mcfg {
	SLIST_ENTRY(acpipci_mcfg) am_list;

	uint16_t	am_segment;
	uint8_t		am_min_bus;
	uint8_t		am_max_bus;

	bus_space_tag_t	am_iot;
	bus_space_handle_t am_ioh;

	struct arm64_pci_chipset am_pc;
};

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
	pci_chipset_tag_t sc_pc;

	struct bus_space sc_bus_iot;
	struct bus_space sc_bus_memt;
	struct acpipci_trans *sc_io_trans;
	struct acpipci_trans *sc_mem_trans;

	struct extent	*sc_busex;
	struct extent	*sc_memex;
	struct extent	*sc_ioex;
	char		sc_busex_name[32];
	char		sc_ioex_name[32];
	char		sc_memex_name[32];
	int		sc_bus;
	uint32_t	sc_seg;
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

uint32_t acpipci_iort_map_msi(pci_chipset_tag_t, pcitag_t);

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
	uint64_t seg = 0;

	sc->sc_acpi = (struct acpi_softc *)parent;
	sc->sc_node = aaa->aaa_node;
	printf(" %s", sc->sc_node->name);

	if (aml_evalname(sc->sc_acpi, sc->sc_node, "_CRS", 0, NULL, &res)) {
		printf(": can't find resources\n");
		return;
	}

	aml_evalinteger(sc->sc_acpi, sc->sc_node, "_BBN", 0, NULL, &bbn);
	sc->sc_bus = bbn;

	aml_evalinteger(sc->sc_acpi, sc->sc_node, "_SEG", 0, NULL, &seg);
	sc->sc_seg = seg;

	sc->sc_iot = aaa->aaa_memt;
	
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

	sc->sc_pc = pci_lookup_segment(seg);
	KASSERT(sc->sc_pc->pc_intr_v == NULL);

	sc->sc_pc->pc_intr_v = sc;
	sc->sc_pc->pc_intr_map = acpipci_intr_map;
	sc->sc_pc->pc_intr_map_msi = acpipci_intr_map_msi;
	sc->sc_pc->pc_intr_map_msix = acpipci_intr_map_msix;
	sc->sc_pc->pc_intr_string = acpipci_intr_string;
	sc->sc_pc->pc_intr_establish = acpipci_intr_establish;
	sc->sc_pc->pc_intr_disestablish = acpipci_intr_disestablish;

	memset(&pba, 0, sizeof(pba));
	pba.pba_busname = "pci";
	pba.pba_iot = &sc->sc_bus_iot;
	pba.pba_memt = &sc->sc_bus_memt;
	pba.pba_dmat = aaa->aaa_dmat;
	pba.pba_pc = sc->sc_pc;
	pba.pba_busex = sc->sc_busex;
	pba.pba_ioex = sc->sc_ioex;
	pba.pba_memex = sc->sc_memex;
	pba.pba_domain = pci_ndomains++;
	pba.pba_bus = sc->sc_bus;
	pba.pba_flags |= PCI_FLAGS_MSI_ENABLED;

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
	struct acpipci_mcfg *am = v;

	if (tag < (am->am_min_bus << 20) ||
	    tag >= ((am->am_max_bus + 1) << 20))
		return 0xffffffff;

	return bus_space_read_4(am->am_iot, am->am_ioh, tag | reg);
}

void
acpipci_conf_write(void *v, pcitag_t tag, int reg, pcireg_t data)
{
	struct acpipci_mcfg *am = v;

	if (tag < (am->am_min_bus << 20) ||
	    tag >= ((am->am_max_bus + 1) << 20))
		return;

	bus_space_write_4(am->am_iot, am->am_ioh, tag | reg, data);
}

struct acpipci_intr_handle {
	pci_chipset_tag_t	ih_pc;
	pcitag_t		ih_tag;
	int			ih_intrpin;
	int			ih_msi;
};

int
acpipci_intr_swizzle(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	struct acpipci_intr_handle *ih;
	int dev, swizpin;

	if (pa->pa_bridgetag == NULL)
		return -1;

	pci_decompose_tag(pa->pa_pc, pa->pa_tag, NULL, &dev, NULL);
	swizpin = PPB_INTERRUPT_SWIZZLE(pa->pa_rawintrpin, dev);
	if ((void *)pa->pa_bridgeih[swizpin - 1] == NULL)
		return -1;

	ih = malloc(sizeof(struct acpipci_intr_handle), M_DEVBUF, M_WAITOK);
	memcpy(ih, (void *)pa->pa_bridgeih[swizpin - 1],
	    sizeof(struct acpipci_intr_handle));
	*ihp = (pci_intr_handle_t)ih;

	return 0;
}

int
acpipci_intr_map(struct pci_attach_args *pa, pci_intr_handle_t *ihp)
{
	struct acpipci_softc *sc = pa->pa_pc->pc_intr_v;
	struct aml_node *node = sc->sc_node;
	struct aml_value res;
	uint64_t addr, pin, source, index;
	struct acpipci_intr_handle *ih;
	int i;

	/*
	 * If we're behind a bridge, we need to look for a _PRT for
	 * it.  If we don't find a _PRT, we need to swizzle.  If we're
	 * not behind a bridge we need to look for a _PRT on the host
	 * bridge node itself.
	 */
	if (pa->pa_bridgetag) {
		node = acpi_find_pci(pa->pa_pc, *pa->pa_bridgetag);
		if (node == NULL)
			return acpipci_intr_swizzle(pa, ihp);
	}

	if (aml_evalname(sc->sc_acpi, node, "_PRT", 0, NULL, &res))
		return acpipci_intr_swizzle(pa, ihp);

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

	if ((pa->pa_flags & PCI_FLAGS_MSI_ENABLED) == 0 ||
	    pci_get_capability(pc, tag, PCI_CAP_MSI, NULL, NULL) == 0)
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

		/* Map Requester ID through IORT to get sideband data. */
		data = acpipci_iort_map_msi(ih->ih_pc, ih->ih_tag);
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

SLIST_HEAD(,acpipci_mcfg) acpipci_mcfgs =
    SLIST_HEAD_INITIALIZER(acpipci_mcfgs);

void
pci_mcfg_init(bus_space_tag_t iot, bus_addr_t addr, int segment,
    int min_bus, int max_bus)
{
	struct acpipci_mcfg *am;

	am = malloc(sizeof(struct acpipci_mcfg), M_DEVBUF, M_WAITOK | M_ZERO);
	am->am_segment = segment;
	am->am_min_bus = min_bus;
	am->am_max_bus = max_bus;

	am->am_iot = iot;
	if (bus_space_map(iot, addr, (max_bus + 1) << 20, 0, &am->am_ioh))
		panic("%s: can't map config space", __func__);

	am->am_pc.pc_conf_v = am;
	am->am_pc.pc_attach_hook = acpipci_attach_hook;
	am->am_pc.pc_bus_maxdevs = acpipci_bus_maxdevs;
	am->am_pc.pc_make_tag = acpipci_make_tag;
	am->am_pc.pc_decompose_tag = acpipci_decompose_tag;
	am->am_pc.pc_conf_size = acpipci_conf_size;
	am->am_pc.pc_conf_read = acpipci_conf_read;
	am->am_pc.pc_conf_write = acpipci_conf_write;
	SLIST_INSERT_HEAD(&acpipci_mcfgs, am, am_list);
}

pcireg_t
acpipci_dummy_conf_read(void *v, pcitag_t tag, int reg)
{
	return 0xffffffff;
}

void
acpipci_dummy_conf_write(void *v, pcitag_t tag, int reg, pcireg_t data)
{
}

struct arm64_pci_chipset acpipci_dummy_chipset = {
	.pc_attach_hook = acpipci_attach_hook,
	.pc_bus_maxdevs = acpipci_bus_maxdevs,
	.pc_make_tag = acpipci_make_tag,
	.pc_decompose_tag = acpipci_decompose_tag,
	.pc_conf_size = acpipci_conf_size,
	.pc_conf_read = acpipci_dummy_conf_read,
	.pc_conf_write = acpipci_dummy_conf_write,
};

pci_chipset_tag_t
pci_lookup_segment(int segment)
{
	struct acpipci_mcfg *am;

	SLIST_FOREACH(am, &acpipci_mcfgs, am_list) {
		if (am->am_segment == segment)
			return &am->am_pc;
	}

	return &acpipci_dummy_chipset;
}

/*
 * IORT support.
 */

struct acpi_iort {
	struct acpi_table_header	hdr;
#define IORT_SIG	"IORT"
	uint32_t	number_of_nodes;
	uint32_t	offset;
	uint32_t	reserved;
} __packed;

struct acpi_iort_node {
	uint8_t		type;
#define ACPI_IORT_ITS		0
#define ACPI_IORT_ROOT_COMPLEX	2
	uint16_t	length;
	uint8_t		revision;
	uint32_t	reserved1;
	uint32_t	number_of_mappings;
	uint32_t	mapping_offset;
	uint64_t	memory_access_properties;
	uint32_t	atf_attributes;
	uint32_t	segment;
	uint8_t		memory_address_size_limit;
	uint8_t		reserved2[3];
} __packed;

struct acpi_iort_mapping {
	uint32_t	input_base;
	uint32_t	length;
	uint32_t	output_base;
	uint32_t	output_reference;
	uint32_t	flags;
#define ACPI_IORT_MAPPING_SINGLE	0x00000001
} __packed;

uint32_t
acpipci_iort_map_node(struct acpi_iort_node *node, uint32_t id, uint32_t reference)
{
	struct acpi_iort_mapping *map =
	    (struct acpi_iort_mapping *)((char *)node + node->mapping_offset);
	int i;
	
	for (i = 0; i < node->number_of_mappings; i++) {
		if (map[i].output_reference != reference)
			continue;
		
		if (map[i].flags & ACPI_IORT_MAPPING_SINGLE)
			return map[i].output_base;

		if (map[i].input_base <= id &&
		    id < map[i].input_base + map[i].length)
			return map[i].output_base + (id - map[i].input_base);
	}

	return id;
}

uint32_t
acpipci_iort_map_msi(pci_chipset_tag_t pc, pcitag_t tag)
{
	struct acpipci_softc *sc = pc->pc_intr_v;
	struct acpi_table_header *hdr;
	struct acpi_iort *iort = NULL;
	struct acpi_iort_node *node;
	struct acpi_q *entry;
	uint32_t rid, its = 0;
	uint32_t offset;
	int i;

	rid = pci_requester_id(pc, tag);

	/* Look for IORT table. */
	SIMPLEQ_FOREACH(entry, &sc->sc_acpi->sc_tables, q_next) {
		hdr = entry->q_table;
		if (strncmp(hdr->signature, IORT_SIG,
		    sizeof(hdr->signature)) == 0) {
			iort = entry->q_table;
			break;
		}
	}
	if (iort == NULL)
		return rid;

	/* Find reference to ITS group. */
	offset = iort->offset;
	for (i = 0; i < iort->number_of_nodes; i++) {
		node = (struct acpi_iort_node *)((char *)iort + offset);
		switch (node->type) {
		case ACPI_IORT_ITS:
			its = offset;
			break;
		}
		offset += node->length;
	}
	if (its == 0)
		return rid;

	/* Find our root complex and map. */
	offset = iort->offset;
	for (i = 0; i < iort->number_of_nodes; i++) {
		node = (struct acpi_iort_node *)((char *)iort + offset);
		switch (node->type) {
		case ACPI_IORT_ROOT_COMPLEX:
			if (node->segment == sc->sc_seg)
				return acpipci_iort_map_node(node, rid, its);
			break;
		}
		offset += node->length;
	}

	return rid;
}
