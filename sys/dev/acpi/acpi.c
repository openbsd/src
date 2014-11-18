/* $OpenBSD: acpi.c,v 1.274 2014/11/18 23:55:01 krw Exp $ */
/*
 * Copyright (c) 2005 Thorsten Lockert <tholo@sigmasoft.com>
 * Copyright (c) 2005 Jordan Hargrave <jordan@openbsd.org>
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
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/event.h>
#include <sys/signalvar.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/sched.h>
#include <sys/reboot.h>
#include <sys/sysctl.h>

#ifdef HIBERNATE
#include <sys/hibernate.h>
#endif

#include <machine/conf.h>
#include <machine/cpufunc.h>
#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/dsdt.h>
#include <dev/wscons/wsdisplayvar.h>

#include <dev/pci/pcidevs.h>
#include <dev/pci/ppbreg.h>

#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>

#include <machine/apmvar.h>
#define APMUNIT(dev)	(minor(dev)&0xf0)
#define APMDEV(dev)	(minor(dev)&0x0f)
#define APMDEV_NORMAL	0
#define APMDEV_CTL	8

#include "wd.h"
#include "wsdisplay.h"

#ifdef ACPI_DEBUG
int	acpi_debug = 16;
#endif

int	acpi_poll_enabled;
int	acpi_hasprocfvs;

#define ACPIEN_RETRIES 15

void 	acpi_pci_match(struct device *, struct pci_attach_args *);
pcireg_t acpi_pci_min_powerstate(pci_chipset_tag_t, pcitag_t);
void	 acpi_pci_set_powerstate(pci_chipset_tag_t, pcitag_t, int, int);
int	acpi_pci_notify(struct aml_node *, int, void *);

int	acpi_match(struct device *, void *, void *);
void	acpi_attach(struct device *, struct device *, void *);
int	acpi_submatch(struct device *, void *, void *);
int	acpi_print(void *, const char *);

void	acpi_map_pmregs(struct acpi_softc *);

int	acpi_loadtables(struct acpi_softc *, struct acpi_rsdp *);

int	_acpi_matchhids(const char *, const char *[]);

int	acpi_inidev(struct aml_node *, void *);
int	acpi_foundprt(struct aml_node *, void *);

struct acpi_q *acpi_maptable(struct acpi_softc *, paddr_t, const char *,
	    const char *, const char *, int);

int	acpi_enable(struct acpi_softc *);
void	acpi_init_states(struct acpi_softc *);

void 	acpi_gpe_task(void *, int);
void	acpi_sbtn_task(void *, int);
void	acpi_pbtn_task(void *, int);

#ifndef SMALL_KERNEL

int	acpi_thinkpad_enabled;
int	acpi_toshiba_enabled;
int	acpi_asus_enabled;
int	acpi_saved_boothowto;
int	acpi_enabled;

int	acpi_matchhids(struct acpi_attach_args *aa, const char *hids[],
	    const char *driver);

void	acpi_thread(void *);
void	acpi_create_thread(void *);
void	acpi_init_pm(struct acpi_softc *);
void	acpi_init_gpes(struct acpi_softc *);
void	acpi_indicator(struct acpi_softc *, int);

int	acpi_founddock(struct aml_node *, void *);
int	acpi_foundpss(struct aml_node *, void *);
int	acpi_foundhid(struct aml_node *, void *);
int	acpi_foundec(struct aml_node *, void *);
int	acpi_foundtmp(struct aml_node *, void *);
int	acpi_foundprw(struct aml_node *, void *);
int	acpi_foundvideo(struct aml_node *, void *);
int	acpi_foundsony(struct aml_node *node, void *arg);

int	acpi_foundide(struct aml_node *node, void *arg);
int	acpiide_notify(struct aml_node *, int, void *);
void	wdcattach(struct channel_softc *);
int	wdcdetach(struct channel_softc *, int);
int	is_ejectable_bay(struct aml_node *node);
int	is_ata(struct aml_node *node);
int	is_ejectable(struct aml_node *node);

struct idechnl {
	struct acpi_softc *sc;
	int64_t		addr;
	int64_t		chnl;
	int64_t		sta;
};

int	acpi_add_device(struct aml_node *node, void *arg);

struct gpe_block *acpi_find_gpe(struct acpi_softc *, int);
void	acpi_enable_onegpe(struct acpi_softc *, int);
int	acpi_gpe(struct acpi_softc *, int, void *);

void	acpi_enable_rungpes(struct acpi_softc *);
void	acpi_enable_wakegpes(struct acpi_softc *, int);
void	acpi_disable_allgpes(struct acpi_softc *);

#endif /* SMALL_KERNEL */

/* XXX move this into dsdt softc at some point */
extern struct aml_node aml_root;

struct cfattach acpi_ca = {
	sizeof(struct acpi_softc), acpi_match, acpi_attach
};

struct cfdriver acpi_cd = {
	NULL, "acpi", DV_DULL
};

struct acpi_softc *acpi_softc;

#define acpi_bus_space_map	_bus_space_map
#define acpi_bus_space_unmap	_bus_space_unmap

int
acpi_gasio(struct acpi_softc *sc, int iodir, int iospace, uint64_t address,
    int access_size, int len, void *buffer)
{
	u_int8_t *pb;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	pci_chipset_tag_t pc;
	pcitag_t tag;
	int reg, idx, ival, sval;

	dnprintf(50, "gasio: %.2x 0x%.8llx %s\n",
	    iospace, address, (iodir == ACPI_IOWRITE) ? "write" : "read");

	KASSERT((len % access_size) == 0);

	pb = (u_int8_t *)buffer;
	switch (iospace) {
	case GAS_SYSTEM_MEMORY:
	case GAS_SYSTEM_IOSPACE:
		if (iospace == GAS_SYSTEM_MEMORY)
			iot = sc->sc_memt;
		else
			iot = sc->sc_iot;

		if (acpi_bus_space_map(iot, address, len, 0, &ioh) != 0) {
			printf("%s: unable to map iospace\n", DEVNAME(sc));
			return (-1);
		}
		for (reg = 0; reg < len; reg += access_size) {
			if (iodir == ACPI_IOREAD) {
				switch (access_size) {
				case 1:
					*(uint8_t *)(pb + reg) = 
					    bus_space_read_1(iot, ioh, reg);
					dnprintf(80, "os_in8(%llx) = %x\n",
					    reg+address, *(uint8_t *)(pb+reg));
					break;
				case 2:
					*(uint16_t *)(pb + reg) =
					    bus_space_read_2(iot, ioh, reg);
					dnprintf(80, "os_in16(%llx) = %x\n",
					    reg+address, *(uint16_t *)(pb+reg));
					break;
				case 4:
					*(uint32_t *)(pb + reg) =
					    bus_space_read_4(iot, ioh, reg);
					break;
				default:
					printf("%s: rdio: invalid size %d\n",
					    DEVNAME(sc), access_size);
					return (-1);
				}
			} else {
				switch (access_size) {
				case 1:
					bus_space_write_1(iot, ioh, reg,
					    *(uint8_t *)(pb + reg));
					dnprintf(80, "os_out8(%llx,%x)\n",
					    reg+address, *(uint8_t *)(pb+reg));
					break;
				case 2:
					bus_space_write_2(iot, ioh, reg,
					    *(uint16_t *)(pb + reg));
					dnprintf(80, "os_out16(%llx,%x)\n",
					    reg+address, *(uint16_t *)(pb+reg));
					break;
				case 4:
					bus_space_write_4(iot, ioh, reg,
					    *(uint32_t *)(pb + reg));
					break;
				default:
					printf("%s: wrio: invalid size %d\n",
					    DEVNAME(sc), access_size);
					return (-1);
				}
			}
		}
		acpi_bus_space_unmap(iot, ioh, len, NULL);
		break;

	case GAS_PCI_CFG_SPACE:
		/* format of address:
		 *    bits 00..15 = register
		 *    bits 16..31 = function
		 *    bits 32..47 = device
		 *    bits 48..63 = bus
		 */
		pc = NULL;
		tag = pci_make_tag(pc,
		    ACPI_PCI_BUS(address), ACPI_PCI_DEV(address),
		    ACPI_PCI_FN(address));

		/* XXX: This is ugly. read-modify-write does a byte at a time */
		reg = ACPI_PCI_REG(address);
		for (idx = reg; idx < reg+len; idx++) {
			ival = pci_conf_read(pc, tag, idx & ~0x3);
			if (iodir == ACPI_IOREAD) {
				*pb = ival >> (8 * (idx & 0x3));
			} else {
				sval = *pb;
				ival &= ~(0xFF << (8* (idx & 0x3)));
				ival |= sval << (8* (idx & 0x3));
				pci_conf_write(pc, tag, idx & ~0x3, ival);
			}
			pb++;
		}
		break;

	case GAS_EMBEDDED:
		if (sc->sc_ec == NULL) {
			printf("%s: WARNING EC not initialized\n", DEVNAME(sc));
			return (-1);
		}
#ifndef SMALL_KERNEL
		if (iodir == ACPI_IOREAD)
			acpiec_read(sc->sc_ec, (u_int8_t)address, len, buffer);
		else
			acpiec_write(sc->sc_ec, (u_int8_t)address, len, buffer);
#endif
		break;
	}
	return (0);
}

int
acpi_inidev(struct aml_node *node, void *arg)
{
	struct acpi_softc	*sc = (struct acpi_softc *)arg;
	int64_t st;

	/*
	 * Per the ACPI spec 6.5.1, only run _INI when device is there or
	 * when there is no _STA.  We terminate the tree walk (with return 1)
	 * early if necessary.
	 */

	/* Evaluate _STA to decide _INI fate and walk fate */
	if (aml_evalinteger(sc, node->parent, "_STA", 0, NULL, &st))
		st = STA_PRESENT | STA_ENABLED | STA_DEV_OK | 0x1000;

	/* Evaluate _INI if we are present */
	if (st & STA_PRESENT)
		aml_evalnode(sc, node, 0, NULL, NULL);

	/* If we are functioning, we walk/search our children */
	if (st & STA_DEV_OK)
		return 0;

	/* If we are not enabled, or not present, terminate search */
	if (!(st & (STA_PRESENT|STA_ENABLED)))
		return 1;

	/* Default just continue search */
	return 0;
}

int
acpi_foundprt(struct aml_node *node, void *arg)
{
	struct acpi_softc	*sc = (struct acpi_softc *)arg;
	struct device		*self = (struct device *)arg;
	struct acpi_attach_args	aaa;
	int64_t st = 0;

	dnprintf(10, "found prt entry: %s\n", node->parent->name);

	/* Evaluate _STA to decide _PRT fate and walk fate */
	if (aml_evalinteger(sc, node->parent, "_STA", 0, NULL, &st))
		st = STA_PRESENT | STA_ENABLED | STA_DEV_OK | 0x1000;

	if (st & STA_PRESENT) {
		memset(&aaa, 0, sizeof(aaa));
		aaa.aaa_iot = sc->sc_iot;
		aaa.aaa_memt = sc->sc_memt;
		aaa.aaa_node = node;
		aaa.aaa_name = "acpiprt";

		config_found(self, &aaa, acpi_print);
	}

	/* If we are functioning, we walk/search our children */
	if (st & STA_DEV_OK)
		return 0;

	/* If we are not enabled, or not present, terminate search */
	if (!(st & (STA_PRESENT|STA_ENABLED)))
		return 1;

	/* Default just continue search */
	return 0;
}

int
acpi_match(struct device *parent, void *match, void *aux)
{
	struct bios_attach_args	*ba = aux;
	struct cfdata		*cf = match;

	/* sanity */
	if (strcmp(ba->ba_name, cf->cf_driver->cd_name))
		return (0);

	if (!acpi_probe(parent, cf, ba))
		return (0);

	return (1);
}

TAILQ_HEAD(, acpi_pci) acpi_pcidevs =
    TAILQ_HEAD_INITIALIZER(acpi_pcidevs);
TAILQ_HEAD(, acpi_pci) acpi_pcirootdevs = 
    TAILQ_HEAD_INITIALIZER(acpi_pcirootdevs);

int acpi_getpci(struct aml_node *node, void *arg);
int acpi_getminbus(union acpi_resource *crs, void *arg);

int
acpi_getminbus(union acpi_resource *crs, void *arg)
{
	int *bbn = arg;
	int typ = AML_CRSTYPE(crs);

	/* Check for embedded bus number */
	if (typ == LR_WORD && crs->lr_word.type == 2) {
		/* If _MIN > _MAX, the resource is considered to be invalid. */
		if (crs->lr_word._min > crs->lr_word._max)
			return -1;
		*bbn = crs->lr_word._min;
	}
	return 0;
}

int
_acpi_matchhids(const char *hid, const char *hids[])
{
	int i;

	for (i = 0; hids[i]; i++) 
		if (!strcmp(hid, hids[i]))
			return (1);
	return (0);
}

#ifndef SMALL_KERNEL
int
acpi_matchhids(struct acpi_attach_args *aa, const char *hids[],
    const char *driver)
{
	if (aa->aaa_dev == NULL || aa->aaa_node == NULL)
		return (0);
	if (_acpi_matchhids(aa->aaa_dev, hids)) {
		dnprintf(5, "driver %s matches at least one hid\n", driver);
		return (1);
	}

	return (0);
}
#endif /* SMALL_KERNEL */

/* Map ACPI device node to PCI */
int
acpi_getpci(struct aml_node *node, void *arg)
{
	const char *pcihid[] = { ACPI_DEV_PCIB, ACPI_DEV_PCIEB, "HWP0002", 0 };
	struct acpi_pci *pci, *ppci;
	struct aml_value res;
	struct acpi_softc *sc = arg;
	pci_chipset_tag_t pc = NULL;
	pcitag_t tag;
	uint64_t val;
	uint32_t reg;

	if (!node->value || node->value->type != AML_OBJTYPE_DEVICE)
		return 0;
	if (!aml_evalhid(node, &res)) {
		/* Check if this is a PCI Root node */
		if (_acpi_matchhids(res.v_string, pcihid)) {
			aml_freevalue(&res);

			pci = malloc(sizeof(*pci), M_DEVBUF, M_WAITOK|M_ZERO);

			pci->bus = -1;
			if (!aml_evalinteger(sc, node, "_SEG", 0, NULL, &val))
				pci->seg = val;
			if (!aml_evalname(sc, node, "_CRS", 0, NULL, &res)) {
				aml_parse_resource(&res, acpi_getminbus,
				    &pci->bus);
				dnprintf(10, "%s post-crs: %d\n", aml_nodename(node), 
				    pci->bus);
			}
			if (!aml_evalinteger(sc, node, "_BBN", 0, NULL, &val)) {
				dnprintf(10, "%s post-bbn: %d, %lld\n", aml_nodename(node), 
				    pci->bus, val);
				if (pci->bus == -1)
					pci->bus = val;
			}
			pci->sub = pci->bus;
			node->pci = pci;
			dnprintf(10, "found PCI root: %s %d\n",
			    aml_nodename(node), pci->bus);
			TAILQ_INSERT_TAIL(&acpi_pcirootdevs, pci, next);
		}
		aml_freevalue(&res);
		return 0;
	}

	/* If parent is not PCI, or device does not have _ADR, return */
	if (!node->parent || (ppci = node->parent->pci) == NULL)
		return 0;
	if (aml_evalinteger(sc, node, "_ADR", 0, NULL, &val))
		return 0;

	pci = malloc(sizeof(*pci), M_DEVBUF, M_WAITOK|M_ZERO);
	pci->bus = ppci->sub;
	pci->dev = ACPI_ADR_PCIDEV(val);
	pci->fun = ACPI_ADR_PCIFUN(val);
	pci->node = node;
	pci->sub = -1;

	dnprintf(10, "%.2x:%.2x.%x -> %s\n", 
		pci->bus, pci->dev, pci->fun,
		aml_nodename(node));

	/* Collect device power state information. */
	if (aml_evalinteger(sc, node, "_S3D", 0, NULL, &val) == 0)
		pci->_s3d = val;
	else
		pci->_s3d = -1;
	if (aml_evalinteger(sc, node, "_S3W", 0, NULL, &val) == 0)
		pci->_s3w = val;
	else
		pci->_s3w = -1;
	if (aml_evalinteger(sc, node, "_S4D", 0, NULL, &val) == 0)
		pci->_s4d = val;
	else
		pci->_s4d = -1;
	if (aml_evalinteger(sc, node, "_S4W", 0, NULL, &val) == 0)
		pci->_s4w = val;
	else
		pci->_s4w = -1;

	/* Check if PCI device exists */
	if (pci->dev > 0x1F || pci->fun > 7) {
		free(pci, M_DEVBUF, 0);
		return (1);
	}
	tag = pci_make_tag(pc, pci->bus, pci->dev, pci->fun);
	reg = pci_conf_read(pc, tag, PCI_ID_REG);
	if (PCI_VENDOR(reg) == PCI_VENDOR_INVALID) {
		free(pci, M_DEVBUF, 0);
		return (1);
	}
	node->pci = pci;

	TAILQ_INSERT_TAIL(&acpi_pcidevs, pci, next);

	/* Check if this is a PCI bridge */
	reg = pci_conf_read(pc, tag, PCI_CLASS_REG);
	if (PCI_CLASS(reg) == PCI_CLASS_BRIDGE &&
	    PCI_SUBCLASS(reg) == PCI_SUBCLASS_BRIDGE_PCI) {
		reg = pci_conf_read(pc, tag, PPB_REG_BUSINFO);
		pci->sub = PPB_BUSINFO_SECONDARY(reg);

		dnprintf(10, "found PCI bridge: %s %d\n", 
		    aml_nodename(node), pci->sub);

		/* Continue scanning */
		return (0);
	}

	/* Device does not have children, stop scanning */
	return (1);
}

void
acpi_pci_match(struct device *dev, struct pci_attach_args *pa)
{
	struct acpi_pci *pdev;
	int state;

	TAILQ_FOREACH(pdev, &acpi_pcidevs, next) {
		if (pdev->bus != pa->pa_bus ||
		    pdev->dev != pa->pa_device ||
		    pdev->fun != pa->pa_function)
			continue;

		dnprintf(10,"%s at acpi0 %s\n", dev->dv_xname,
		    aml_nodename(pdev->node));

		pdev->device = dev;

		/*
		 * If some Power Resources are dependent on this device
		 * initialize them.
		 */
		state = pci_get_powerstate(pa->pa_pc, pa->pa_tag);
		acpi_pci_set_powerstate(pa->pa_pc, pa->pa_tag, state, 1);
		acpi_pci_set_powerstate(pa->pa_pc, pa->pa_tag, state, 0);

		aml_register_notify(pdev->node, NULL, acpi_pci_notify, pdev, 0);
	}
}

pcireg_t
acpi_pci_min_powerstate(pci_chipset_tag_t pc, pcitag_t tag)
{
	struct acpi_pci *pdev;
	int bus, dev, fun;
	int state = -1, defaultstate = pci_get_powerstate(pc, tag);

	pci_decompose_tag(pc, tag, &bus, &dev, &fun);
	TAILQ_FOREACH(pdev, &acpi_pcidevs, next) {
		if (pdev->bus == bus && pdev->dev == dev && pdev->fun == fun) {
			switch (acpi_softc->sc_state) {
			case ACPI_STATE_S3:
				defaultstate = PCI_PMCSR_STATE_D3;
				state = MAX(pdev->_s3d, pdev->_s3w);
				break;
			case ACPI_STATE_S4:
				state = MAX(pdev->_s4d, pdev->_s4w);
				break;
			case ACPI_STATE_S5:
			default:
				break;
			}

			if (state >= PCI_PMCSR_STATE_D0 &&
			    state <= PCI_PMCSR_STATE_D3)
				return state;
		}
	}

	return defaultstate;
}

void
acpi_pci_set_powerstate(pci_chipset_tag_t pc, pcitag_t tag, int state, int pre)
{
#if NACPIPWRRES > 0
	struct acpi_softc *sc = acpi_softc;
	struct acpi_pwrres *pr;
	struct acpi_pci *pdev;
	int bus, dev, fun;
	char name[5];

	pci_decompose_tag(pc, tag, &bus, &dev, &fun);
	TAILQ_FOREACH(pdev, &acpi_pcidevs, next) {
		if (pdev->bus == bus && pdev->dev == dev && pdev->fun == fun)
			break;
	}

	/* XXX Add a check to discard nodes without Power Resources? */
	if (pdev == NULL)
		return;

	SIMPLEQ_FOREACH(pr, &sc->sc_pwrresdevs, p_next) {
		if (pr->p_node != pdev->node)
			continue;

		/*
		 * If the firmware is already aware that the device
		 * is in the given state, there's nothing to do.
		 */
		if (pr->p_state == state)
			continue;

		if (pre) {
			/*
			 * If a Resource is dependent on this device for
			 * the given state, make sure it is turned "_ON".
			 */
			if (pr->p_res_state == state)
				acpipwrres_ref_incr(pr->p_res_sc, pr->p_node);
		} else {
			/*
			 * If a Resource was referenced for the state we
			 * left, drop a reference and turn it "_OFF" if
			 * it was the last one.
			 */
			if (pr->p_res_state == pr->p_state)
				acpipwrres_ref_decr(pr->p_res_sc, pr->p_node);

			if (pr->p_res_state == state) {
				snprintf(name, sizeof(name), "_PS%d", state);
				aml_evalname(sc, pr->p_node, name, 0,
				    NULL, NULL);
			}

			pr->p_state = state;
		}

	}
#endif /* NACPIPWRRES > 0 */
}

int
acpi_pci_notify(struct aml_node *node, int ntype, void *arg)
{
	struct acpi_pci *pdev = arg;
	pci_chipset_tag_t pc = NULL;
	pcitag_t tag;
	pcireg_t reg;
	int offset;

	/* We're only interested in Device Wake notifications. */
	if (ntype != 2)
		return (0);

	tag = pci_make_tag(pc, pdev->bus, pdev->dev, pdev->fun);
	if (pci_get_capability(pc, tag, PCI_CAP_PWRMGMT, &offset, 0)) {
		/* Clear the PME Status bit if it is set. */
		reg = pci_conf_read(pc, tag, offset + PCI_PMCSR);
		pci_conf_write(pc, tag, offset + PCI_PMCSR, reg);
	}

	return (0);
}

void
acpi_pciroots_attach(struct device *dev, void *aux, cfprint_t pr)
{
	struct acpi_pci			*pdev;
	struct pcibus_attach_args	*pba = aux;

	KASSERT(pba->pba_busex != NULL);

	TAILQ_FOREACH(pdev, &acpi_pcirootdevs, next) {
		if (extent_alloc_region(pba->pba_busex, pdev->bus,
		    1, EX_NOWAIT) != 0)
			continue;
		pba->pba_bus = pdev->bus;
		config_found(dev, pba, pr);
	}
}

void
acpi_attach(struct device *parent, struct device *self, void *aux)
{
	struct bios_attach_args *ba = aux;
	struct acpi_softc *sc = (struct acpi_softc *)self;
	struct acpi_mem_map handle;
	struct acpi_rsdp *rsdp;
	struct acpi_q *entry;
	struct acpi_dsdt *p_dsdt;
#ifndef SMALL_KERNEL
	int wakeup_dev_ct;
	struct acpi_wakeq *wentry;
	struct device *dev;
	struct acpi_ac *ac;
	struct acpi_bat *bat;
	int s;
#endif /* SMALL_KERNEL */
	paddr_t facspa;

	sc->sc_iot = ba->ba_iot;
	sc->sc_memt = ba->ba_memt;

	rw_init(&sc->sc_lck, "acpilk");

	acpi_softc = sc;

	if (acpi_map(ba->ba_acpipbase, sizeof(struct acpi_rsdp), &handle)) {
		printf(": can't map memory\n");
		return;
	}

	rsdp = (struct acpi_rsdp *)handle.va;
	sc->sc_revision = (int)rsdp->rsdp_revision;
	printf(": rev %d", sc->sc_revision);

	SIMPLEQ_INIT(&sc->sc_tables);
	SIMPLEQ_INIT(&sc->sc_wakedevs);
#if NACPIPWRRES > 0
	SIMPLEQ_INIT(&sc->sc_pwrresdevs);
#endif /* NACPIPWRRES > 0 */


#ifndef SMALL_KERNEL
	sc->sc_note = malloc(sizeof(struct klist), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->sc_note == NULL) {
		printf(", can't allocate memory\n");
		acpi_unmap(&handle);
		return;
	}
#endif /* SMALL_KERNEL */

	if (acpi_loadtables(sc, rsdp)) {
		printf(", can't load tables\n");
		acpi_unmap(&handle);
		return;
	}

	acpi_unmap(&handle);

	/*
	 * Find the FADT
	 */
	SIMPLEQ_FOREACH(entry, &sc->sc_tables, q_next) {
		if (memcmp(entry->q_table, FADT_SIG,
		    sizeof(FADT_SIG) - 1) == 0) {
			sc->sc_fadt = entry->q_table;
			break;
		}
	}
	if (sc->sc_fadt == NULL) {
		printf(", no FADT\n");
		return;
	}

	/*
	 * Check if we are able to enable ACPI control
	 */
	if (sc->sc_fadt->smi_cmd &&
	    (!sc->sc_fadt->acpi_enable && !sc->sc_fadt->acpi_disable)) {
		printf(", ACPI control unavailable\n");
		return;
	}

	/*
	 * Set up a pointer to the firmware control structure
	 */
	if (sc->sc_fadt->hdr_revision < 3 || sc->sc_fadt->x_firmware_ctl == 0)
		facspa = sc->sc_fadt->firmware_ctl;
	else
		facspa = sc->sc_fadt->x_firmware_ctl;

	if (acpi_map(facspa, sizeof(struct acpi_facs), &handle))
		printf(" !FACS");
	else
		sc->sc_facs = (struct acpi_facs *)handle.va;

	/* Create opcode hashtable */
	aml_hashopcodes();

	/* Create Default AML objects */
	aml_create_defaultobjects();

	/*
	 * Load the DSDT from the FADT pointer -- use the
	 * extended (64-bit) pointer if it exists
	 */
	if (sc->sc_fadt->hdr_revision < 3 || sc->sc_fadt->x_dsdt == 0)
		entry = acpi_maptable(sc, sc->sc_fadt->dsdt, NULL, NULL, NULL, -1);
	else
		entry = acpi_maptable(sc, sc->sc_fadt->x_dsdt, NULL, NULL, NULL, -1);

	if (entry == NULL)
		printf(" !DSDT");

	p_dsdt = entry->q_table;
	acpi_parse_aml(sc, p_dsdt->aml, p_dsdt->hdr_length -
	    sizeof(p_dsdt->hdr));

	/* Load SSDT's */
	SIMPLEQ_FOREACH(entry, &sc->sc_tables, q_next) {
		if (memcmp(entry->q_table, SSDT_SIG,
		    sizeof(SSDT_SIG) - 1) == 0) {
			p_dsdt = entry->q_table;
			acpi_parse_aml(sc, p_dsdt->aml, p_dsdt->hdr_length -
			    sizeof(p_dsdt->hdr));
		}
	}

	/* Perform post-parsing fixups */
	aml_postparse();

	/* Find available sleeping states */
	acpi_init_states(sc);

#ifndef SMALL_KERNEL
	/* Find available sleep/resume related methods. */
	acpi_init_pm(sc);
#endif /* SMALL_KERNEL */

	/* Map Power Management registers */
	acpi_map_pmregs(sc);

#ifndef SMALL_KERNEL
	/* Initialize GPE handlers */
	s = spltty();
	acpi_init_gpes(sc);
	splx(s);

	/* some devices require periodic polling */
	timeout_set(&sc->sc_dev_timeout, acpi_poll, sc);

	acpi_enabled = 1;
#endif /* SMALL_KERNEL */

	/*
	 * Take over ACPI control.  Note that once we do this, we
	 * effectively tell the system that we have ownership of
	 * the ACPI hardware registers, and that SMI should leave
	 * them alone
	 *
	 * This may prevent thermal control on some systems where
	 * that actually does work
	 */
	if (sc->sc_fadt->smi_cmd) {
		if (acpi_enable(sc)) {
			printf(", can't enable ACPI\n");
			return;
		}
	}

	printf("\n%s: tables", DEVNAME(sc));
	SIMPLEQ_FOREACH(entry, &sc->sc_tables, q_next) {
		printf(" %.4s", (char *)entry->q_table);
	}
	printf("\n");

#ifndef SMALL_KERNEL
	/* Display wakeup devices and lowest S-state */
	wakeup_dev_ct = 0;
	printf("%s: wakeup devices", DEVNAME(sc));
	SIMPLEQ_FOREACH(wentry, &sc->sc_wakedevs, q_next) {
		if (wakeup_dev_ct < 16)
			printf(" %.4s(S%d)", wentry->q_node->name,
			    wentry->q_state);
		else if (wakeup_dev_ct == 16)
			printf(" [...]");
		wakeup_dev_ct ++;
	}
	printf("\n");

	/*
	 * ACPI is enabled now -- attach timer
	 */
	{
		struct acpi_attach_args aaa;

		memset(&aaa, 0, sizeof(aaa));
		aaa.aaa_name = "acpitimer";
		aaa.aaa_iot = sc->sc_iot;
		aaa.aaa_memt = sc->sc_memt;
#if 0
		aaa.aaa_pcit = sc->sc_pcit;
		aaa.aaa_smbust = sc->sc_smbust;
#endif
		config_found(self, &aaa, acpi_print);
	}
#endif /* SMALL_KERNEL */

	/*
	 * Attach table-defined devices
	 */
	SIMPLEQ_FOREACH(entry, &sc->sc_tables, q_next) {
		struct acpi_attach_args aaa;

		memset(&aaa, 0, sizeof(aaa));
		aaa.aaa_iot = sc->sc_iot;
		aaa.aaa_memt = sc->sc_memt;
	#if 0
		aaa.aaa_pcit = sc->sc_pcit;
		aaa.aaa_smbust = sc->sc_smbust;
	#endif
		aaa.aaa_table = entry->q_table;
		config_found_sm(self, &aaa, acpi_print, acpi_submatch);
	}

	/* initialize runtime environment */
	aml_find_node(&aml_root, "_INI", acpi_inidev, sc);

	/* Get PCI mapping */
	aml_walknodes(&aml_root, AML_WALK_PRE, acpi_getpci, sc);

	/* attach pci interrupt routing tables */
	aml_find_node(&aml_root, "_PRT", acpi_foundprt, sc);

#ifndef SMALL_KERNEL
	aml_find_node(&aml_root, "_HID", acpi_foundec, sc);

	aml_walknodes(&aml_root, AML_WALK_PRE, acpi_add_device, sc);

	/* attach battery, power supply and button devices */
	aml_find_node(&aml_root, "_HID", acpi_foundhid, sc);

#if NWD > 0
	/* Attach IDE bay */
	aml_walknodes(&aml_root, AML_WALK_PRE, acpi_foundide, sc);
#endif

	/* attach docks */
	aml_find_node(&aml_root, "_DCK", acpi_founddock, sc);

	/* check if we're running on a sony */
	aml_find_node(&aml_root, "GBRT", acpi_foundsony, sc);

	/* attach video only if this is not a stinkpad or toshiba */
	if (!acpi_thinkpad_enabled && !acpi_toshiba_enabled &&
	    !acpi_asus_enabled)
		aml_find_node(&aml_root, "_DOS", acpi_foundvideo, sc);

	/* create list of devices we want to query when APM come in */
	SLIST_INIT(&sc->sc_ac);
	SLIST_INIT(&sc->sc_bat);
	TAILQ_FOREACH(dev, &alldevs, dv_list) {
		if (!strcmp(dev->dv_cfdata->cf_driver->cd_name, "acpiac")) {
			ac = malloc(sizeof(*ac), M_DEVBUF, M_WAITOK | M_ZERO);
			ac->aac_softc = (struct acpiac_softc *)dev;
			SLIST_INSERT_HEAD(&sc->sc_ac, ac, aac_link);
		} else if (!strcmp(dev->dv_cfdata->cf_driver->cd_name, "acpibat")) {
			bat = malloc(sizeof(*bat), M_DEVBUF, M_WAITOK | M_ZERO);
			bat->aba_softc = (struct acpibat_softc *)dev;
			SLIST_INSERT_HEAD(&sc->sc_bat, bat, aba_link);
		}
	}

	/* Setup threads */
	sc->sc_thread = malloc(sizeof(struct acpi_thread), M_DEVBUF, M_WAITOK);
	sc->sc_thread->sc = sc;
	sc->sc_thread->running = 1;

	/* Enable PCI Power Management. */
	pci_dopm = 1;

	acpi_attach_machdep(sc);

	kthread_create_deferred(acpi_create_thread, sc);
#endif /* SMALL_KERNEL */
}

int
acpi_submatch(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = (struct acpi_attach_args *)aux;
	struct cfdata *cf = match;

	if (aaa->aaa_table == NULL)
		return (0);
	return ((*cf->cf_attach->ca_match)(parent, match, aux));
}

int
acpi_print(void *aux, const char *pnp)
{
	struct acpi_attach_args *aa = aux;

	if (pnp) {
		if (aa->aaa_name)
			printf("%s at %s", aa->aaa_name, pnp);
		else
			return (QUIET);
	}

	return (UNCONF);
}

struct acpi_q *
acpi_maptable(struct acpi_softc *sc, paddr_t addr, const char *sig,
    const char *oem, const char *tbl, int flag)
{
	static int tblid;
	struct acpi_mem_map handle;
	struct acpi_table_header *hdr;
	struct acpi_q *entry;
	size_t len;

	/* Check if we can map address */
	if (addr == 0)
		return NULL;
	if (acpi_map(addr, sizeof(*hdr), &handle))
		return NULL;
	hdr = (struct acpi_table_header *)handle.va;
	len = hdr->length;
	acpi_unmap(&handle);

	/* Validate length/checksum */
	if (acpi_map(addr, len, &handle))
		return NULL;
	hdr = (struct acpi_table_header *)handle.va;
	if (acpi_checksum(hdr, len)) {
		acpi_unmap(&handle);
		return NULL;
	}
	if ((sig && memcmp(sig, hdr->signature, 4)) ||
	    (oem && memcmp(oem, hdr->oemid, 6)) ||
	    (tbl && memcmp(tbl, hdr->oemtableid, 8))) {
		acpi_unmap(&handle);
		return NULL;
	}

	/* Allocate copy */
	entry = malloc(len + sizeof(*entry), M_DEVBUF, M_NOWAIT);
	if (entry != NULL) {
		memcpy(entry->q_data, handle.va, len);
		entry->q_table = entry->q_data;
		entry->q_id = ++tblid;

		if (flag < 0)
			SIMPLEQ_INSERT_HEAD(&sc->sc_tables, entry,
			    q_next);
		else if (flag > 0)
			SIMPLEQ_INSERT_TAIL(&sc->sc_tables, entry,
			    q_next);
	}
	acpi_unmap(&handle);
	return entry;
}

int
acpi_loadtables(struct acpi_softc *sc, struct acpi_rsdp *rsdp)
{
	struct acpi_q *sdt;
	int i, ntables;
	size_t len;

	if (rsdp->rsdp_revision == 2 && rsdp->rsdp_xsdt) {
		struct acpi_xsdt *xsdt;

		sdt = acpi_maptable(sc, rsdp->rsdp_xsdt, NULL, NULL, NULL, 0);
		if (sdt == NULL) {
			printf("couldn't map rsdt\n");
			return (ENOMEM);
		}

		xsdt = (struct acpi_xsdt *)sdt->q_data;
		len  = xsdt->hdr.length;
		ntables = (len - sizeof(struct acpi_table_header)) /
		    sizeof(xsdt->table_offsets[0]);

		for (i = 0; i < ntables; i++)
			acpi_maptable(sc, xsdt->table_offsets[i], NULL, NULL,
			    NULL, 1);

		free(sdt, M_DEVBUF, 0);
	} else {
		struct acpi_rsdt *rsdt;

		sdt = acpi_maptable(sc, rsdp->rsdp_rsdt, NULL, NULL, NULL, 0);
		if (sdt == NULL) {
			printf("couldn't map rsdt\n");
			return (ENOMEM);
		}

		rsdt = (struct acpi_rsdt *)sdt->q_data;
		len  = rsdt->hdr.length;
		ntables = (len - sizeof(struct acpi_table_header)) /
		    sizeof(rsdt->table_offsets[0]);

		for (i = 0; i < ntables; i++)
			acpi_maptable(sc, rsdt->table_offsets[i], NULL, NULL,
			    NULL, 1);

		free(sdt, M_DEVBUF, 0);
	}

	return (0);
}

/* Read from power management register */
int
acpi_read_pmreg(struct acpi_softc *sc, int reg, int offset)
{
	bus_space_handle_t ioh;
	bus_size_t size;
	int regval;

	/* Special cases: 1A/1B blocks can be OR'ed together */
	switch (reg) {
	case ACPIREG_PM1_EN:
		return (acpi_read_pmreg(sc, ACPIREG_PM1A_EN, offset) |
		    acpi_read_pmreg(sc, ACPIREG_PM1B_EN, offset));
	case ACPIREG_PM1_STS:
		return (acpi_read_pmreg(sc, ACPIREG_PM1A_STS, offset) |
		    acpi_read_pmreg(sc, ACPIREG_PM1B_STS, offset));
	case ACPIREG_PM1_CNT:
		return (acpi_read_pmreg(sc, ACPIREG_PM1A_CNT, offset) |
		    acpi_read_pmreg(sc, ACPIREG_PM1B_CNT, offset));
	case ACPIREG_GPE_STS:
		dnprintf(50, "read GPE_STS  offset: %.2x %.2x %.2x\n", offset,
		    sc->sc_fadt->gpe0_blk_len>>1, sc->sc_fadt->gpe1_blk_len>>1);
		if (offset < (sc->sc_fadt->gpe0_blk_len >> 1)) {
			reg = ACPIREG_GPE0_STS;
		}
		break;
	case ACPIREG_GPE_EN:
		dnprintf(50, "read GPE_EN   offset: %.2x %.2x %.2x\n",
		    offset, sc->sc_fadt->gpe0_blk_len>>1,
		    sc->sc_fadt->gpe1_blk_len>>1);
		if (offset < (sc->sc_fadt->gpe0_blk_len >> 1)) {
			reg = ACPIREG_GPE0_EN;
		}
		break;
	}

	if (reg >= ACPIREG_MAXREG || sc->sc_pmregs[reg].size == 0)
		return (0);

	regval = 0;
	ioh = sc->sc_pmregs[reg].ioh;
	size = sc->sc_pmregs[reg].size;
	if (size > sc->sc_pmregs[reg].access)
		size = sc->sc_pmregs[reg].access;

	switch (size) {
	case 1:
		regval = bus_space_read_1(sc->sc_iot, ioh, offset);
		break;
	case 2:
		regval = bus_space_read_2(sc->sc_iot, ioh, offset);
		break;
	case 4:
		regval = bus_space_read_4(sc->sc_iot, ioh, offset);
		break;
	}

	dnprintf(30, "acpi_readpm: %s = %.4x:%.4x %x\n",
	    sc->sc_pmregs[reg].name,
	    sc->sc_pmregs[reg].addr, offset, regval);
	return (regval);
}

/* Write to power management register */
void
acpi_write_pmreg(struct acpi_softc *sc, int reg, int offset, int regval)
{
	bus_space_handle_t ioh;
	bus_size_t size;

	/* Special cases: 1A/1B blocks can be written with same value */
	switch (reg) {
	case ACPIREG_PM1_EN:
		acpi_write_pmreg(sc, ACPIREG_PM1A_EN, offset, regval);
		acpi_write_pmreg(sc, ACPIREG_PM1B_EN, offset, regval);
		break;
	case ACPIREG_PM1_STS:
		acpi_write_pmreg(sc, ACPIREG_PM1A_STS, offset, regval);
		acpi_write_pmreg(sc, ACPIREG_PM1B_STS, offset, regval);
		break;
	case ACPIREG_PM1_CNT:
		acpi_write_pmreg(sc, ACPIREG_PM1A_CNT, offset, regval);
		acpi_write_pmreg(sc, ACPIREG_PM1B_CNT, offset, regval);
		break;
	case ACPIREG_GPE_STS:
		dnprintf(50, "write GPE_STS offset: %.2x %.2x %.2x %.2x\n",
		    offset, sc->sc_fadt->gpe0_blk_len>>1,
		    sc->sc_fadt->gpe1_blk_len>>1, regval);
		if (offset < (sc->sc_fadt->gpe0_blk_len >> 1)) {
			reg = ACPIREG_GPE0_STS;
		}
		break;
	case ACPIREG_GPE_EN:
		dnprintf(50, "write GPE_EN  offset: %.2x %.2x %.2x %.2x\n",
		    offset, sc->sc_fadt->gpe0_blk_len>>1,
		    sc->sc_fadt->gpe1_blk_len>>1, regval);
		if (offset < (sc->sc_fadt->gpe0_blk_len >> 1)) {
			reg = ACPIREG_GPE0_EN;
		}
		break;
	}

	/* All special case return here */
	if (reg >= ACPIREG_MAXREG)
		return;

	ioh = sc->sc_pmregs[reg].ioh;
	size = sc->sc_pmregs[reg].size;
	if (size > sc->sc_pmregs[reg].access)
		size = sc->sc_pmregs[reg].access;

	switch (size) {
	case 1:
		bus_space_write_1(sc->sc_iot, ioh, offset, regval);
		break;
	case 2:
		bus_space_write_2(sc->sc_iot, ioh, offset, regval);
		break;
	case 4:
		bus_space_write_4(sc->sc_iot, ioh, offset, regval);
		break;
	}

	dnprintf(30, "acpi_writepm: %s = %.4x:%.4x %x\n",
	    sc->sc_pmregs[reg].name, sc->sc_pmregs[reg].addr, offset, regval);
}

/* Map Power Management registers */
void
acpi_map_pmregs(struct acpi_softc *sc)
{
	bus_addr_t addr;
	bus_size_t size, access;
	const char *name;
	int reg;

	for (reg = 0; reg < ACPIREG_MAXREG; reg++) {
		size = 0;
		access = 0;
		switch (reg) {
		case ACPIREG_SMICMD:
			name = "smi";
			size = access = 1;
			addr = sc->sc_fadt->smi_cmd;
			break;
		case ACPIREG_PM1A_STS:
		case ACPIREG_PM1A_EN:
			name = "pm1a_sts";
			size = sc->sc_fadt->pm1_evt_len >> 1;
			addr = sc->sc_fadt->pm1a_evt_blk;
			access = 2;
			if (reg == ACPIREG_PM1A_EN && addr) {
				addr += size;
				name = "pm1a_en";
			}
			break;
		case ACPIREG_PM1A_CNT:
			name = "pm1a_cnt";
			size = sc->sc_fadt->pm1_cnt_len;
			addr = sc->sc_fadt->pm1a_cnt_blk;
			access = 2;
			break;
		case ACPIREG_PM1B_STS:
		case ACPIREG_PM1B_EN:
			name = "pm1b_sts";
			size = sc->sc_fadt->pm1_evt_len >> 1;
			addr = sc->sc_fadt->pm1b_evt_blk;
			access = 2;
			if (reg == ACPIREG_PM1B_EN && addr) {
				addr += size;
				name = "pm1b_en";
			}
			break;
		case ACPIREG_PM1B_CNT:
			name = "pm1b_cnt";
			size = sc->sc_fadt->pm1_cnt_len;
			addr = sc->sc_fadt->pm1b_cnt_blk;
			access = 2;
			break;
		case ACPIREG_PM2_CNT:
			name = "pm2_cnt";
			size = sc->sc_fadt->pm2_cnt_len;
			addr = sc->sc_fadt->pm2_cnt_blk;
			access = size;
			break;
#if 0
		case ACPIREG_PM_TMR:
			/* Allocated in acpitimer */
			name = "pm_tmr";
			size = sc->sc_fadt->pm_tmr_len;
			addr = sc->sc_fadt->pm_tmr_blk;
			access = 4;
			break;
#endif
		case ACPIREG_GPE0_STS:
		case ACPIREG_GPE0_EN:
			name = "gpe0_sts";
			size = sc->sc_fadt->gpe0_blk_len >> 1;
			addr = sc->sc_fadt->gpe0_blk;
			access = 1;

			dnprintf(20, "gpe0 block len : %x\n",
			    sc->sc_fadt->gpe0_blk_len >> 1);
			dnprintf(20, "gpe0 block addr: %x\n",
			    sc->sc_fadt->gpe0_blk);
			if (reg == ACPIREG_GPE0_EN && addr) {
				addr += size;
				name = "gpe0_en";
			}
			break;
		case ACPIREG_GPE1_STS:
		case ACPIREG_GPE1_EN:
			name = "gpe1_sts";
			size = sc->sc_fadt->gpe1_blk_len >> 1;
			addr = sc->sc_fadt->gpe1_blk;
			access = 1;

			dnprintf(20, "gpe1 block len : %x\n",
			    sc->sc_fadt->gpe1_blk_len >> 1);
			dnprintf(20, "gpe1 block addr: %x\n",
			    sc->sc_fadt->gpe1_blk);
			if (reg == ACPIREG_GPE1_EN && addr) {
				addr += size;
				name = "gpe1_en";
			}
			break;
		}
		if (size && addr) {
			dnprintf(50, "mapping: %.4lx %.4lx %s\n",
			    addr, size, name);

			/* Size and address exist; map register space */
			bus_space_map(sc->sc_iot, addr, size, 0,
			    &sc->sc_pmregs[reg].ioh);

			sc->sc_pmregs[reg].name = name;
			sc->sc_pmregs[reg].size = size;
			sc->sc_pmregs[reg].addr = addr;
			sc->sc_pmregs[reg].access = min(access, 4);
		}
	}
}

int
acpi_enable(struct acpi_softc *sc)
{
	int idx;

	acpi_write_pmreg(sc, ACPIREG_SMICMD, 0, sc->sc_fadt->acpi_enable);
	idx = 0;
	do {
		if (idx++ > ACPIEN_RETRIES) {
			return ETIMEDOUT;
		}
	} while (!(acpi_read_pmreg(sc, ACPIREG_PM1_CNT, 0) & ACPI_PM1_SCI_EN));

	return 0;
}

void
acpi_init_states(struct acpi_softc *sc)
{
	struct aml_value res;
	char name[8];
	int i;

	printf("\n%s: sleep states", DEVNAME(sc));
	for (i = ACPI_STATE_S0; i <= ACPI_STATE_S5; i++) {
		snprintf(name, sizeof(name), "_S%d_", i);
		sc->sc_sleeptype[i].slp_typa = -1;
		sc->sc_sleeptype[i].slp_typb = -1;
		if (aml_evalname(sc, &aml_root, name, 0, NULL, &res) == 0) {
			if (res.type == AML_OBJTYPE_PACKAGE) {
				sc->sc_sleeptype[i].slp_typa = aml_val2int(res.v_package[0]);
				sc->sc_sleeptype[i].slp_typb = aml_val2int(res.v_package[1]);
				printf(" S%d", i);
			}
			aml_freevalue(&res);
		}
	}
}

/* ACPI Workqueue support */
SIMPLEQ_HEAD(,acpi_taskq) acpi_taskq =
    SIMPLEQ_HEAD_INITIALIZER(acpi_taskq);

void
acpi_addtask(struct acpi_softc *sc, void (*handler)(void *, int), 
    void *arg0, int arg1)
{
	struct acpi_taskq *wq;
	int s;

	wq = malloc(sizeof(*wq), M_DEVBUF, M_ZERO | M_NOWAIT);
	if (wq == NULL)
		return;
	wq->handler = handler;
	wq->arg0 = arg0;
	wq->arg1 = arg1;
	
	s = spltty();
	SIMPLEQ_INSERT_TAIL(&acpi_taskq, wq, next);
	splx(s);
}

int
acpi_dotask(struct acpi_softc *sc)
{
	struct acpi_taskq *wq;
	int s;

	s = spltty();
	if (SIMPLEQ_EMPTY(&acpi_taskq)) {
		splx(s);

		/* we don't have anything to do */
		return (0);
	}
	wq = SIMPLEQ_FIRST(&acpi_taskq);
	SIMPLEQ_REMOVE_HEAD(&acpi_taskq, next);
	splx(s);

	wq->handler(wq->arg0, wq->arg1);

	free(wq, M_DEVBUF, 0);

	/* We did something */
	return (1);	
}

#ifndef SMALL_KERNEL
int
is_ata(struct aml_node *node)
{
	return (aml_searchname(node, "_GTM") != NULL ||
	    aml_searchname(node, "_GTF") != NULL ||
	    aml_searchname(node, "_STM") != NULL ||
	    aml_searchname(node, "_SDD") != NULL);
}

int
is_ejectable(struct aml_node *node)
{
	return (aml_searchname(node, "_EJ0") != NULL);
}

int
is_ejectable_bay(struct aml_node *node)
{
	return ((is_ata(node) || is_ata(node->parent)) && is_ejectable(node));
}

#if NWD > 0
int
acpiide_notify(struct aml_node *node, int ntype, void *arg)
{
	struct idechnl 		*ide = arg;
	struct acpi_softc 	*sc = ide->sc;
	struct pciide_softc 	*wsc;
	struct device 		*dev;
	int 			b,d,f;
	int64_t 		sta;

	if (aml_evalinteger(sc, node, "_STA", 0, NULL, &sta) != 0)
		return (0);

	dnprintf(10, "IDE notify! %s %d status:%llx\n", aml_nodename(node),
	    ntype, sta);

	/* Walk device list looking for IDE device match */
	TAILQ_FOREACH(dev, &alldevs, dv_list) {
		if (strcmp(dev->dv_cfdata->cf_driver->cd_name, "pciide"))
			continue;

		wsc = (struct pciide_softc *)dev;
		pci_decompose_tag(NULL, wsc->sc_tag, &b, &d, &f);
		if (b != ACPI_PCI_BUS(ide->addr) ||
		    d != ACPI_PCI_DEV(ide->addr) ||
		    f != ACPI_PCI_FN(ide->addr))
			continue;
		dnprintf(10, "Found pciide: %s %x.%x.%x channel:%llx\n",
		    dev->dv_xname, b,d,f, ide->chnl);

		if (sta == 0 && ide->sta)
			wdcdetach(
			    &wsc->pciide_channels[ide->chnl].wdc_channel, 0);
		else if (sta && !ide->sta)
			wdcattach(
			    &wsc->pciide_channels[ide->chnl].wdc_channel);
		ide->sta = sta;
	}
	return (0);
}

int
acpi_foundide(struct aml_node *node, void *arg)
{
	struct acpi_softc 	*sc = arg;
	struct aml_node 	*pp;
	struct idechnl 		*ide;
	union amlpci_t 		pi;
	int 			lvl;

	/* Check if this is an ejectable bay */
	if (!is_ejectable_bay(node))
		return (0);

	ide = malloc(sizeof(struct idechnl), M_DEVBUF, M_NOWAIT | M_ZERO);
	ide->sc = sc;

	/* GTM/GTF can be at 2/3 levels:  pciX.ideX.channelX[.driveX] */
	lvl = 0;
	for (pp=node->parent; pp; pp=pp->parent) {
		lvl++;
		if (aml_searchname(pp, "_HID"))
			break;
	}

	/* Get PCI address and channel */
	if (lvl == 3) {
		aml_evalinteger(sc, node->parent, "_ADR", 0, NULL,
		    &ide->chnl);
		aml_rdpciaddr(node->parent->parent, &pi);
		ide->addr = pi.addr;
	} else if (lvl == 4) {
		aml_evalinteger(sc, node->parent->parent, "_ADR", 0, NULL,
		    &ide->chnl);
		aml_rdpciaddr(node->parent->parent->parent, &pi);
		ide->addr = pi.addr;
	}
	dnprintf(10, "%s %llx channel:%llx\n",
	    aml_nodename(node), ide->addr, ide->chnl);

	aml_evalinteger(sc, node, "_STA", 0, NULL, &ide->sta);
	dnprintf(10, "Got Initial STA: %llx\n", ide->sta);

	aml_register_notify(node, "acpiide", acpiide_notify, ide, 0);
	return (0);
}
#endif /* NWD > 0 */

void
acpi_reset(void)
{
	u_int32_t		 reset_as, reset_len;
	u_int32_t		 value;
	struct acpi_softc	*sc = acpi_softc;
	struct acpi_fadt	*fadt = sc->sc_fadt;

	if (acpi_enabled == 0)
		return;

	/*
	 * RESET_REG_SUP is not properly set in some implementations,
	 * but not testing against it breaks more machines than it fixes
	 */
	if (fadt->hdr_revision <= 1 ||
	    !(fadt->flags & FADT_RESET_REG_SUP) || fadt->reset_reg.address == 0)
		return;

	value = fadt->reset_value;

	reset_as = fadt->reset_reg.register_bit_width / 8;
	if (reset_as == 0)
		reset_as = 1;

	reset_len = fadt->reset_reg.access_size;
	if (reset_len == 0)
		reset_len = reset_as;

	acpi_gasio(sc, ACPI_IOWRITE,
	    fadt->reset_reg.address_space_id,
	    fadt->reset_reg.address, reset_as, reset_len, &value);

	delay(100000);
}

void
acpi_gpe_task(void *arg0, int gpe)
{
	struct acpi_softc *sc = acpi_softc;
	struct gpe_block *pgpe = &sc->gpe_table[gpe];

	dnprintf(10, "handle gpe: %x\n", gpe);
	if (pgpe->handler && pgpe->active) {
		pgpe->active = 0;
		pgpe->handler(sc, gpe, pgpe->arg);
	}
}

void
acpi_pbtn_task(void *arg0, int dummy)
{
	struct acpi_softc *sc = arg0;
	uint16_t en;
	int s;

	dnprintf(1,"power button pressed\n");

	/* Reset the latch and re-enable the GPE */
	s = spltty();
	en = acpi_read_pmreg(sc, ACPIREG_PM1_EN, 0);
	acpi_write_pmreg(sc, ACPIREG_PM1_EN,  0,
	    en | ACPI_PM1_PWRBTN_EN);
	splx(s);

	acpi_addtask(sc, acpi_powerdown_task, sc, 0);
}

void
acpi_sbtn_task(void *arg0, int dummy)
{
	struct acpi_softc *sc = arg0;
	uint16_t en;
	int s;

	dnprintf(1,"sleep button pressed\n");
	aml_notify_dev(ACPI_DEV_SBD, 0x80);

	/* Reset the latch and re-enable the GPE */
	s = spltty();
	en = acpi_read_pmreg(sc, ACPIREG_PM1_EN, 0);
	acpi_write_pmreg(sc, ACPIREG_PM1_EN,  0,
	    en | ACPI_PM1_SLPBTN_EN);
	splx(s);
}

void
acpi_powerdown_task(void *arg0, int dummy)
{
	extern int allowpowerdown;

	if (allowpowerdown == 1) {
		allowpowerdown = 0;
		prsignal(initprocess, SIGUSR2);
	}
}

void
acpi_sleep_task(void *arg0, int sleepmode)
{
	struct acpi_softc *sc = arg0;
	struct acpi_ac *ac;
	struct acpi_bat *bat;

	/* System goes to sleep here.. */
	acpi_sleep_state(sc, sleepmode);

	/* AC and battery information needs refreshing */
	SLIST_FOREACH(ac, &sc->sc_ac, aac_link)
		aml_notify(ac->aac_softc->sc_devnode,
		    0x80);
	SLIST_FOREACH(bat, &sc->sc_bat, aba_link)
		aml_notify(bat->aba_softc->sc_devnode,
		    0x80);
}

int
acpi_interrupt(void *arg)
{
	struct acpi_softc *sc = (struct acpi_softc *)arg;
	u_int32_t processed = 0, idx, jdx;
	u_int16_t sts, en;

	dnprintf(40, "ACPI Interrupt\n");
	for (idx = 0; idx < sc->sc_lastgpe; idx += 8) {
		sts = acpi_read_pmreg(sc, ACPIREG_GPE_STS, idx>>3);
		en  = acpi_read_pmreg(sc, ACPIREG_GPE_EN,  idx>>3);
		if (en & sts) {
			dnprintf(10, "GPE block: %.2x %.2x %.2x\n", idx, sts,
			    en);
			/* Mask the GPE until it is serviced */
			acpi_write_pmreg(sc, ACPIREG_GPE_EN, idx>>3, en & ~sts);
			for (jdx = 0; jdx < 8; jdx++) {
				if (en & sts & (1L << jdx)) {
					/* Signal this GPE */
					sc->gpe_table[idx+jdx].active = 1;
					dnprintf(10, "queue gpe: %x\n", idx+jdx);
					acpi_addtask(sc, acpi_gpe_task, NULL, idx+jdx);

					/*
					 * Edge interrupts need their STS bits
					 * cleared now.  Level interrupts will
					 * have their STS bits cleared just
					 * before they are re-enabled.
					 */
					if (sc->gpe_table[idx+jdx].edge)
						acpi_write_pmreg(sc,
						    ACPIREG_GPE_STS, idx>>3,
						    1L << jdx);
					processed = 1;
				}
			}
		}
	}

	sts = acpi_read_pmreg(sc, ACPIREG_PM1_STS, 0);
	en  = acpi_read_pmreg(sc, ACPIREG_PM1_EN, 0);
	if (sts & en) {
		dnprintf(10,"GEN interrupt: %.4x\n", sts & en);
		sts &= en;
		if (sts & ACPI_PM1_PWRBTN_STS) {
			/* Mask and acknowledge */
			en &= ~ACPI_PM1_PWRBTN_EN;
			acpi_write_pmreg(sc, ACPIREG_PM1_EN, 0, en);
			acpi_write_pmreg(sc, ACPIREG_PM1_STS, 0,
			    ACPI_PM1_PWRBTN_STS);
			sts &= ~ACPI_PM1_PWRBTN_STS;

			acpi_addtask(sc, acpi_pbtn_task, sc, 0);
		}
		if (sts & ACPI_PM1_SLPBTN_STS) {
			/* Mask and acknowledge */
			en &= ~ACPI_PM1_SLPBTN_EN;
			acpi_write_pmreg(sc, ACPIREG_PM1_EN, 0, en);
			acpi_write_pmreg(sc, ACPIREG_PM1_STS, 0,
			    ACPI_PM1_SLPBTN_STS);
			sts &= ~ACPI_PM1_SLPBTN_STS;

			acpi_addtask(sc, acpi_sbtn_task, sc, 0);
		}
		if (sts) {
			printf("%s: PM1 stuck (en 0x%x st 0x%x), clearing\n",
			    sc->sc_dev.dv_xname, en, sts);
			acpi_write_pmreg(sc, ACPIREG_PM1_EN, 0, en & ~sts);
			acpi_write_pmreg(sc, ACPIREG_PM1_STS, 0, sts);
		}
		processed = 1;
	}

	if (processed) {
		acpi_wakeup(sc);
	}

	return (processed);
}

int
acpi_add_device(struct aml_node *node, void *arg)
{
	static int nacpicpus = 0;
	struct device *self = arg;
	struct acpi_softc *sc = arg;
	struct acpi_attach_args aaa;
#ifdef MULTIPROCESSOR
	struct aml_value res;
	int proc_id = -1;
#endif

	memset(&aaa, 0, sizeof(aaa));
	aaa.aaa_node = node;
	aaa.aaa_iot = sc->sc_iot;
	aaa.aaa_memt = sc->sc_memt;
	if (node == NULL || node->value == NULL)
		return 0;

	switch (node->value->type) {
	case AML_OBJTYPE_PROCESSOR:
		if (nacpicpus >= ncpus)
			return 0;
#ifdef MULTIPROCESSOR
		if (aml_evalnode(sc, aaa.aaa_node, 0, NULL, &res) == 0) {
			if (res.type == AML_OBJTYPE_PROCESSOR)
				proc_id = res.v_processor.proc_id;
			aml_freevalue(&res);
		}
		if (proc_id < -1 || proc_id >= LAPIC_MAP_SIZE ||
		    (acpi_lapic_flags[proc_id] & ACPI_PROC_ENABLE) == 0)
			return 0;
#endif
		nacpicpus++;

		aaa.aaa_name = "acpicpu";
		break;
	case AML_OBJTYPE_THERMZONE:
		aaa.aaa_name = "acpitz";
		break;
	case AML_OBJTYPE_POWERRSRC:
		aaa.aaa_name = "acpipwrres";
		break;
	default:
		return 0;
	}
	config_found(self, &aaa, acpi_print);
	return 0;
}

void
acpi_enable_onegpe(struct acpi_softc *sc, int gpe)
{
	uint8_t mask, en;

	/* Read enabled register */
	mask = (1L << (gpe & 7));
	en = acpi_read_pmreg(sc, ACPIREG_GPE_EN, gpe>>3);
	dnprintf(50, "enabling GPE %.2x (current: %sabled) %.2x\n",
	    gpe, (en & mask) ? "en" : "dis", en);
	acpi_write_pmreg(sc, ACPIREG_GPE_EN, gpe>>3, en | mask);
}

/* Clear all GPEs */
void
acpi_disable_allgpes(struct acpi_softc *sc)
{
	int idx;

	for (idx = 0; idx < sc->sc_lastgpe; idx += 8) {
		acpi_write_pmreg(sc, ACPIREG_GPE_EN, idx >> 3, 0);
		acpi_write_pmreg(sc, ACPIREG_GPE_STS, idx >> 3, -1);
	}
}

/* Enable runtime GPEs */
void
acpi_enable_rungpes(struct acpi_softc *sc)
{
	int idx;

	for (idx = 0; idx < sc->sc_lastgpe; idx++)
		if (sc->gpe_table[idx].handler)
			acpi_enable_onegpe(sc, idx);
}

/* Enable wakeup GPEs */
void
acpi_enable_wakegpes(struct acpi_softc *sc, int state)
{
	struct acpi_wakeq *wentry;

	SIMPLEQ_FOREACH(wentry, &sc->sc_wakedevs, q_next) {
		dnprintf(10, "%.4s(S%d) gpe %.2x\n", wentry->q_node->name,
		    wentry->q_state,
		    wentry->q_gpe);
		if (state <= wentry->q_state)
			acpi_enable_onegpe(sc, wentry->q_gpe);
	}
}

int
acpi_set_gpehandler(struct acpi_softc *sc, int gpe, int (*handler)
    (struct acpi_softc *, int, void *), void *arg, int edge)
{
	struct gpe_block *ptbl;

	ptbl = acpi_find_gpe(sc, gpe);
	if (ptbl == NULL || handler == NULL)
		return -EINVAL;
	if (ptbl->handler != NULL) {
		dnprintf(10, "error: GPE %.2x already enabled\n", gpe);
		return -EBUSY;
	}
	dnprintf(50, "Adding GPE handler %.2x (%s)\n", gpe, edge ? "edge" : "level");
	ptbl->handler = handler;
	ptbl->arg = arg;
	ptbl->edge = edge;

	return (0);
}

int
acpi_gpe(struct acpi_softc *sc, int gpe, void *arg)
{
	struct aml_node *node = arg;
	uint8_t mask, en;

	dnprintf(10, "handling GPE %.2x\n", gpe);
	aml_evalnode(sc, node, 0, NULL, NULL);

	mask = (1L << (gpe & 7));
	if (!sc->gpe_table[gpe].edge)
		acpi_write_pmreg(sc, ACPIREG_GPE_STS, gpe>>3, mask);
	en = acpi_read_pmreg(sc, ACPIREG_GPE_EN,  gpe>>3);
	acpi_write_pmreg(sc, ACPIREG_GPE_EN,  gpe>>3, en | mask);
	return (0);
}

/* Discover Devices that can wakeup the system
 * _PRW returns a package
 *  pkg[0] = integer (FADT gpe bit) or package (gpe block,gpe bit)
 *  pkg[1] = lowest sleep state
 *  pkg[2+] = power resource devices (optional)
 *
 * To enable wakeup devices:
 *    Evaluate _ON method in each power resource device
 *    Evaluate _PSW method
 */
int
acpi_foundprw(struct aml_node *node, void *arg)
{
	struct acpi_softc *sc = arg;
	struct acpi_wakeq *wq;

	wq = malloc(sizeof(struct acpi_wakeq), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (wq == NULL)
		return 0;

	wq->q_wakepkg = malloc(sizeof(struct aml_value), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (wq->q_wakepkg == NULL) {
		free(wq, M_DEVBUF, 0);
		return 0;
	}
	dnprintf(10, "Found _PRW (%s)\n", node->parent->name);
	aml_evalnode(sc, node, 0, NULL, wq->q_wakepkg);
	wq->q_node = node->parent;
	wq->q_gpe = -1;

	/* Get GPE of wakeup device, and lowest sleep level */
	if (wq->q_wakepkg->type == AML_OBJTYPE_PACKAGE &&
	    wq->q_wakepkg->length >= 2) {
		if (wq->q_wakepkg->v_package[0]->type == AML_OBJTYPE_INTEGER)
			wq->q_gpe = wq->q_wakepkg->v_package[0]->v_integer;
		if (wq->q_wakepkg->v_package[1]->type == AML_OBJTYPE_INTEGER)
			wq->q_state = wq->q_wakepkg->v_package[1]->v_integer;
	}
	SIMPLEQ_INSERT_TAIL(&sc->sc_wakedevs, wq, q_next);
	return 0;
}

struct gpe_block *
acpi_find_gpe(struct acpi_softc *sc, int gpe)
{
	if (gpe >= sc->sc_lastgpe)
		return NULL;
	return &sc->gpe_table[gpe];
}

void
acpi_init_gpes(struct acpi_softc *sc)
{
	struct aml_node *gpe;
	char name[12];
	int  idx, ngpe;

	sc->sc_lastgpe = sc->sc_fadt->gpe0_blk_len << 2;
	if (sc->sc_fadt->gpe1_blk_len) {
	}
	dnprintf(50, "Last GPE: %.2x\n", sc->sc_lastgpe);

	/* Allocate GPE table */
	sc->gpe_table = malloc(sc->sc_lastgpe * sizeof(struct gpe_block),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	ngpe = 0;

	/* Clear GPE status */
	acpi_disable_allgpes(sc);
	for (idx = 0; idx < sc->sc_lastgpe; idx++) {
		/* Search Level-sensitive GPES */
		snprintf(name, sizeof(name), "\\_GPE._L%.2X", idx);
		gpe = aml_searchname(&aml_root, name);
		if (gpe != NULL)
			acpi_set_gpehandler(sc, idx, acpi_gpe, gpe, 0);
		if (gpe == NULL) {
			/* Search Edge-sensitive GPES */
			snprintf(name, sizeof(name), "\\_GPE._E%.2X", idx);
			gpe = aml_searchname(&aml_root, name);
			if (gpe != NULL)
				acpi_set_gpehandler(sc, idx, acpi_gpe, gpe, 1);
		}
	}
	aml_find_node(&aml_root, "_PRW", acpi_foundprw, sc);
	sc->sc_maxgpe = ngpe;
}

void
acpi_init_pm(struct acpi_softc *sc)
{
	sc->sc_tts = aml_searchname(&aml_root, "_TTS");
	sc->sc_pts = aml_searchname(&aml_root, "_PTS");
	sc->sc_wak = aml_searchname(&aml_root, "_WAK");
	sc->sc_bfs = aml_searchname(&aml_root, "_BFS");
	sc->sc_gts = aml_searchname(&aml_root, "_GTS");
	sc->sc_sst = aml_searchname(&aml_root, "_SI_._SST");
}

void
acpi_sleep_pm(struct acpi_softc *sc, int state)
{
	uint16_t rega, regb, regra, regrb;
	int retry = 0;

	disable_intr();

	/* Clear WAK_STS bit */
	acpi_write_pmreg(sc, ACPIREG_PM1_STS, 0, ACPI_PM1_WAK_STS);

	/* Disable BM arbitration at deep sleep and beyond */
	if (state >= ACPI_STATE_S3 &&
	    sc->sc_fadt->pm2_cnt_blk && sc->sc_fadt->pm2_cnt_len)
		acpi_write_pmreg(sc, ACPIREG_PM2_CNT, 0, ACPI_PM2_ARB_DIS);

	/* Write SLP_TYPx values */
	rega = acpi_read_pmreg(sc, ACPIREG_PM1A_CNT, 0);
	regb = acpi_read_pmreg(sc, ACPIREG_PM1B_CNT, 0);
	rega &= ~(ACPI_PM1_SLP_TYPX_MASK | ACPI_PM1_SLP_EN);
	regb &= ~(ACPI_PM1_SLP_TYPX_MASK | ACPI_PM1_SLP_EN);
	rega |= ACPI_PM1_SLP_TYPX(sc->sc_sleeptype[state].slp_typa);
	regb |= ACPI_PM1_SLP_TYPX(sc->sc_sleeptype[state].slp_typb);
	acpi_write_pmreg(sc, ACPIREG_PM1A_CNT, 0, rega);
	acpi_write_pmreg(sc, ACPIREG_PM1B_CNT, 0, regb);

	/* Loop on WAK_STS, setting the SLP_EN bits once in a while */
	rega |= ACPI_PM1_SLP_EN;
	regb |= ACPI_PM1_SLP_EN;
	while (1) {
		if (retry == 0) {
			acpi_write_pmreg(sc, ACPIREG_PM1A_CNT, 0, rega);
			acpi_write_pmreg(sc, ACPIREG_PM1B_CNT, 0, regb);
		}
		retry = (retry + 1) % 100000;

		regra = acpi_read_pmreg(sc, ACPIREG_PM1A_STS, 0);
		regrb = acpi_read_pmreg(sc, ACPIREG_PM1B_STS, 0);
		if ((regra & ACPI_PM1_WAK_STS) ||
		    (regrb & ACPI_PM1_WAK_STS))
			break;
	}
}

void
acpi_resume_pm(struct acpi_softc *sc, int fromstate)
{
	uint16_t rega, regb, en;

	/* Write SLP_TYPx values */
	rega = acpi_read_pmreg(sc, ACPIREG_PM1A_CNT, 0);
	regb = acpi_read_pmreg(sc, ACPIREG_PM1B_CNT, 0);
	rega &= ~(ACPI_PM1_SLP_TYPX_MASK | ACPI_PM1_SLP_EN);
	regb &= ~(ACPI_PM1_SLP_TYPX_MASK | ACPI_PM1_SLP_EN);
	rega |= ACPI_PM1_SLP_TYPX(sc->sc_sleeptype[ACPI_STATE_S0].slp_typa);
	regb |= ACPI_PM1_SLP_TYPX(sc->sc_sleeptype[ACPI_STATE_S0].slp_typb);
	acpi_write_pmreg(sc, ACPIREG_PM1A_CNT, 0, rega);
	acpi_write_pmreg(sc, ACPIREG_PM1B_CNT, 0, regb);

	/* Force SCI_EN on resume to fix horribly broken machines */
	acpi_write_pmreg(sc, ACPIREG_PM1_CNT, 0, ACPI_PM1_SCI_EN);

	/* Clear fixed event status */
	acpi_write_pmreg(sc, ACPIREG_PM1_STS, 0, ACPI_PM1_ALL_STS);

	/* acpica-reference.pdf page 148 says do not call _BFS */
	/* 1st resume AML step: _BFS(fromstate) */
	aml_node_setval(sc, sc->sc_bfs, fromstate);

	/* Enable runtime GPEs */
	acpi_disable_allgpes(sc);
	acpi_enable_rungpes(sc);

	acpi_indicator(sc, ACPI_SST_WAKING);

	/* 2nd resume AML step: _WAK(fromstate) */
	aml_node_setval(sc, sc->sc_wak, fromstate);

	/* Clear WAK_STS bit */
	acpi_write_pmreg(sc, ACPIREG_PM1_STS, 0, ACPI_PM1_WAK_STS);

	en = acpi_read_pmreg(sc, ACPIREG_PM1_EN, 0);
	if (!(sc->sc_fadt->flags & FADT_PWR_BUTTON))
		en |= ACPI_PM1_PWRBTN_EN;
	if (!(sc->sc_fadt->flags & FADT_SLP_BUTTON))
		en |= ACPI_PM1_SLPBTN_EN;
	acpi_write_pmreg(sc, ACPIREG_PM1_EN, 0, en);

	/*
	 * If PM2 exists, re-enable BM arbitration (reportedly some
	 * BIOS forget to)
	 */
	if (sc->sc_fadt->pm2_cnt_blk && sc->sc_fadt->pm2_cnt_len) {
		rega = acpi_read_pmreg(sc, ACPIREG_PM2_CNT, 0);
		rega &= ~ACPI_PM2_ARB_DIS;
		acpi_write_pmreg(sc, ACPIREG_PM2_CNT, 0, rega);
	}
}

/* Set the indicator light to some state */
void
acpi_indicator(struct acpi_softc *sc, int led_state)
{
	static int save_led_state = -1;

	if (save_led_state != led_state) {
		aml_node_setval(sc, sc->sc_sst, led_state);
		save_led_state = led_state;
	}
}

int
acpi_sleep_state(struct acpi_softc *sc, int state)
{
	extern int perflevel;
	int error = ENXIO;
	int s;

	switch (state) {
	case ACPI_STATE_S0:
		return (0);
	case ACPI_STATE_S1:
		return (EOPNOTSUPP);
	case ACPI_STATE_S5:	/* only sleep states handled here */
		return (EOPNOTSUPP);
	}

	if (sc->sc_sleeptype[state].slp_typa == -1 ||
	    sc->sc_sleeptype[state].slp_typb == -1) {
		printf("%s: state S%d unavailable\n",
		    sc->sc_dev.dv_xname, state);
		return (EOPNOTSUPP);
	}

	/* 1st suspend AML step: _TTS(tostate) */
	if (aml_node_setval(sc, sc->sc_tts, state) != 0)
		goto fail_tts;
	acpi_indicator(sc, ACPI_SST_WAKING);	/* blink */

#if NWSDISPLAY > 0
	/*
	 * Temporarily release the lock to prevent the X server from
	 * blocking on setting the display brightness.
	 */
	rw_exit_write(&sc->sc_lck);
	wsdisplay_suspend();
	rw_enter_write(&sc->sc_lck);
#endif /* NWSDISPLAY > 0 */

#ifdef HIBERNATE
	if (state == ACPI_STATE_S4) {
		uvmpd_hibernate();
		hibernate_suspend_bufcache();
		if (hibernate_alloc())
			goto fail_alloc;
	}
#endif /* HIBERNATE */

	if (config_suspend_all(DVACT_QUIESCE))
		goto fail_quiesce;

	bufq_quiesce();

#ifdef MULTIPROCESSOR
	acpi_sleep_mp();
#endif

	resettodr();

	s = splhigh();
	disable_intr();	/* PSL_I for resume; PIC/APIC broken until repair */
	cold = 1;	/* Force other code to delay() instead of tsleep() */

	if (config_suspend_all(DVACT_SUSPEND) != 0)
		goto fail_suspend;
	acpi_sleep_clocks(sc, state);

	/* 2nd suspend AML step: _PTS(tostate) */
	if (aml_node_setval(sc, sc->sc_pts, state) != 0)
		goto fail_pts;

	acpibtn_enable_psw();	/* enable _LID for wakeup */
	acpi_indicator(sc, ACPI_SST_SLEEPING);

	/* 3rd suspend AML step: _GTS(tostate) */
	aml_node_setval(sc, sc->sc_gts, state);

	/* Clear fixed event status */
	acpi_write_pmreg(sc, ACPIREG_PM1_STS, 0, ACPI_PM1_ALL_STS);

	/* Enable wake GPEs */
	acpi_disable_allgpes(sc);
	acpi_enable_wakegpes(sc, state);

	/* Sleep */
	sc->sc_state = state;
	error = acpi_sleep_cpu(sc, state);
	sc->sc_state = ACPI_STATE_S0;
	/* Resume */

#ifdef HIBERNATE
	if (state == ACPI_STATE_S4) {
		uvm_pmr_dirty_everything();
		uvm_pmr_zero_everything();
	}
#endif /* HIBERNATE */

	acpi_resume_clocks(sc);		/* AML may need clocks */
	acpi_resume_pm(sc, state);
	acpi_resume_cpu(sc);

fail_pts:
	config_suspend_all(DVACT_RESUME);

fail_suspend:
	cold = 0;
	enable_intr();
	splx(s);

	acpibtn_disable_psw();		/* disable _LID for wakeup */
	inittodr(time_second);

	/* 3rd resume AML step: _TTS(runstate) */
	aml_node_setval(sc, sc->sc_tts, sc->sc_state);

#ifdef MULTIPROCESSOR
	acpi_resume_mp();
#endif

	bufq_restart();

fail_quiesce:
	config_suspend_all(DVACT_WAKEUP);

#ifdef HIBERNATE
fail_alloc:
	if (state == ACPI_STATE_S4) {
		hibernate_free();
		hibernate_resume_bufcache();
	}
#endif /* HIBERNATE */

#if NWSDISPLAY > 0
	rw_exit_write(&sc->sc_lck);
	wsdisplay_resume();
	rw_enter_write(&sc->sc_lck);
#endif /* NWSDISPLAY > 0 */

	/* Restore hw.setperf */
	if (cpu_setperf != NULL)
		cpu_setperf(perflevel);

	acpi_record_event(sc, APM_NORMAL_RESUME);
	acpi_indicator(sc, ACPI_SST_WORKING);

	/*
	 * If we woke up but the lid is closed, go back to sleep
	 */
	if (!acpibtn_checklidopen())
		acpi_addtask(sc, acpi_sleep_task, sc, state);

fail_tts:
	return (error);
}

void
acpi_wakeup(void *arg)
{
	struct acpi_softc  *sc = (struct acpi_softc *)arg;

	sc->sc_threadwaiting = 0;
	wakeup(sc);
}

/* XXX
 * We are going to do AML execution but are not in the acpi thread.
 * We do not know if the acpi thread is sleeping on acpiec in some
 * intermediate context.  Wish us luck.
 */
void
acpi_powerdown(void)
{
	int state = ACPI_STATE_S5, s;
	struct acpi_softc *sc = acpi_softc;

	if (acpi_enabled == 0)
		return;

	s = splhigh();
	disable_intr();
	cold = 1;

	/* 1st powerdown AML step: _PTS(tostate) */
	aml_node_setval(sc, sc->sc_pts, state);
	
	acpi_disable_allgpes(sc);
	acpi_enable_wakegpes(sc, state);

	/* 2nd powerdown AML step: _GTS(tostate) */
	aml_node_setval(sc, sc->sc_gts, state);

	acpi_sleep_pm(sc, state);
	panic("acpi S5 transition did not happen");
	while (1)
		;
}

void
acpi_thread(void *arg)
{
	struct acpi_thread *thread = arg;
	struct acpi_softc  *sc = thread->sc;
	extern int aml_busy;
	int s;

	/* AML/SMI cannot be trusted -- only run on the BSP */
	sched_peg_curproc(&cpu_info_primary);

	rw_enter_write(&sc->sc_lck);

	/*
	 * If we have an interrupt handler, we can get notification
	 * when certain status bits changes in the ACPI registers,
	 * so let us enable some events we can forward to userland
	 */
	if (sc->sc_interrupt) {
		int16_t en;

		dnprintf(1,"slpbtn:%c  pwrbtn:%c\n",
		    sc->sc_fadt->flags & FADT_SLP_BUTTON ? 'n' : 'y',
		    sc->sc_fadt->flags & FADT_PWR_BUTTON ? 'n' : 'y');
		dnprintf(10, "Enabling acpi interrupts...\n");
		sc->sc_threadwaiting = 1;

		/* Enable Sleep/Power buttons if they exist */
		s = spltty();
		en = acpi_read_pmreg(sc, ACPIREG_PM1_EN, 0);
		if (!(sc->sc_fadt->flags & FADT_PWR_BUTTON))
			en |= ACPI_PM1_PWRBTN_EN;
		if (!(sc->sc_fadt->flags & FADT_SLP_BUTTON))
			en |= ACPI_PM1_SLPBTN_EN;
		acpi_write_pmreg(sc, ACPIREG_PM1_EN, 0, en);

		/* Enable handled GPEs here */
		acpi_enable_rungpes(sc);
		splx(s);
	}

	while (thread->running) {
		s = spltty();
		while (sc->sc_threadwaiting) {
			dnprintf(10, "acpi thread going to sleep...\n");
			rw_exit_write(&sc->sc_lck);
			tsleep(sc, PWAIT, "acpi0", 0);
			rw_enter_write(&sc->sc_lck);
		}
		sc->sc_threadwaiting = 1;
		splx(s);
		if (aml_busy) {
			panic("thread woke up to find aml was busy");
			continue;
		}

		/* Run ACPI taskqueue */
		while(acpi_dotask(acpi_softc))
			;
	}
	free(thread, M_DEVBUF, 0);

	kthread_exit(0);
}

void
acpi_create_thread(void *arg)
{
	struct acpi_softc *sc = arg;

	if (kthread_create(acpi_thread, sc->sc_thread, NULL, DEVNAME(sc))
	    != 0)
		printf("%s: unable to create isr thread, GPEs disabled\n",
		    DEVNAME(sc));
}

int
acpi_map_address(struct acpi_softc *sc, struct acpi_gas *gas, bus_addr_t base,
    bus_size_t size, bus_space_handle_t *pioh, bus_space_tag_t *piot)
{
	int iospace = GAS_SYSTEM_IOSPACE;

	/* No GAS structure, default to I/O space */
	if (gas != NULL) {
		base += gas->address;
		iospace = gas->address_space_id;
	}
	switch (iospace) {
	case GAS_SYSTEM_MEMORY:
		*piot = sc->sc_memt;
		break;
	case GAS_SYSTEM_IOSPACE:
		*piot = sc->sc_iot;
		break;
	default:
		return -1;
	}
	if (bus_space_map(*piot, base, size, 0, pioh))
		return -1;

	return 0;
}

int
acpi_foundec(struct aml_node *node, void *arg)
{
	struct acpi_softc	*sc = (struct acpi_softc *)arg;
	struct device		*self = (struct device *)arg;
	const char		*dev;
	struct aml_value	 res;
	struct acpi_attach_args	aaa;

	if (aml_evalnode(sc, node, 0, NULL, &res) != 0)
		return 0;

	switch (res.type) {
	case AML_OBJTYPE_STRING:
		dev = res.v_string;
		break;
	case AML_OBJTYPE_INTEGER:
		dev = aml_eisaid(aml_val2int(&res));
		break;
	default:
		dev = "unknown";
		break;
	}

	if (strcmp(dev, ACPI_DEV_ECD))
		return 0;

	/* Check if we're already attached */
	if (sc->sc_ec && sc->sc_ec->sc_devnode == node->parent)
		return 0;

	memset(&aaa, 0, sizeof(aaa));
	aaa.aaa_iot = sc->sc_iot;
	aaa.aaa_memt = sc->sc_memt;
	aaa.aaa_node = node->parent;
	aaa.aaa_dev = dev;
	aaa.aaa_name = "acpiec";
	config_found(self, &aaa, acpi_print);
	aml_freevalue(&res);

	return 0;
}

int
acpi_foundhid(struct aml_node *node, void *arg)
{
	struct acpi_softc	*sc = (struct acpi_softc *)arg;
	struct device		*self = (struct device *)arg;
	const char		*dev;
	struct aml_value	 res;
	struct acpi_attach_args	aaa;

	dnprintf(10, "found hid device: %s ", node->parent->name);
	if (aml_evalnode(sc, node, 0, NULL, &res) != 0)
		return 0;

	switch (res.type) {
	case AML_OBJTYPE_STRING:
		dev = res.v_string;
		break;
	case AML_OBJTYPE_INTEGER:
		dev = aml_eisaid(aml_val2int(&res));
		break;
	default:
		dev = "unknown";
		break;
	}
	dnprintf(10, "	device: %s\n", dev);

	memset(&aaa, 0, sizeof(aaa));
	aaa.aaa_iot = sc->sc_iot;
	aaa.aaa_memt = sc->sc_memt;
	aaa.aaa_node = node->parent;
	aaa.aaa_dev = dev;

	if (!strcmp(dev, ACPI_DEV_AC))
		aaa.aaa_name = "acpiac";
	else if (!strcmp(dev, ACPI_DEV_CMB))
		aaa.aaa_name = "acpibat";
	else if (!strcmp(dev, ACPI_DEV_LD) ||
	    !strcmp(dev, ACPI_DEV_PBD) ||
	    !strcmp(dev, ACPI_DEV_SBD))
		aaa.aaa_name = "acpibtn";
	else if (!strcmp(dev, ACPI_DEV_ASUS) || !strcmp(dev, ACPI_DEV_ASUS1)) {
		aaa.aaa_name = "acpiasus";
		acpi_asus_enabled = 1;
	} else if (!strcmp(dev, ACPI_DEV_IBM) ||
	    !strcmp(dev, ACPI_DEV_LENOVO)) {
		aaa.aaa_name = "acpithinkpad";
		acpi_thinkpad_enabled = 1;
	} else if (!strcmp(dev, ACPI_DEV_ASUSAIBOOSTER))
		aaa.aaa_name = "aibs";
	else if (!strcmp(dev, ACPI_DEV_TOSHIBA_LIBRETTO) ||
	    !strcmp(dev, ACPI_DEV_TOSHIBA_DYNABOOK) ||
	    !strcmp(dev, ACPI_DEV_TOSHIBA_SPA40)) {
		aaa.aaa_name = "acpitoshiba";
		acpi_toshiba_enabled = 1;
	}


	if (aaa.aaa_name)
		config_found(self, &aaa, acpi_print);

	aml_freevalue(&res);

	return 0;
}

int
acpi_founddock(struct aml_node *node, void *arg)
{
	struct acpi_softc	*sc = (struct acpi_softc *)arg;
	struct device		*self = (struct device *)arg;
	struct acpi_attach_args	aaa;

	dnprintf(10, "found dock entry: %s\n", node->parent->name);

	memset(&aaa, 0, sizeof(aaa));
	aaa.aaa_iot = sc->sc_iot;
	aaa.aaa_memt = sc->sc_memt;
	aaa.aaa_node = node->parent;
	aaa.aaa_name = "acpidock";

	config_found(self, &aaa, acpi_print);

	return 0;
}

int
acpi_foundvideo(struct aml_node *node, void *arg)
{
	struct acpi_softc *sc = (struct acpi_softc *)arg;
	struct device *self = (struct device *)arg;
	struct acpi_attach_args	aaa;

	memset(&aaa, 0, sizeof(aaa));
	aaa.aaa_iot = sc->sc_iot;
	aaa.aaa_memt = sc->sc_memt;
	aaa.aaa_node = node->parent;
	aaa.aaa_name = "acpivideo";

	config_found(self, &aaa, acpi_print);

	return (0);
}

int
acpi_foundsony(struct aml_node *node, void *arg)
{
	struct acpi_softc *sc = (struct acpi_softc *)arg;
	struct device *self = (struct device *)arg;
	struct acpi_attach_args aaa;

	memset(&aaa, 0, sizeof(aaa));
	aaa.aaa_iot = sc->sc_iot;
	aaa.aaa_memt = sc->sc_memt;
	aaa.aaa_node = node->parent;
	aaa.aaa_name = "acpisony";

	config_found(self, &aaa, acpi_print);

	return 0;
}

int
acpiopen(dev_t dev, int flag, int mode, struct proc *p)
{
	int error = 0;
	struct acpi_softc *sc;
	int s;

	if (!acpi_cd.cd_ndevs || APMUNIT(dev) != 0 ||
	    !(sc = acpi_cd.cd_devs[APMUNIT(dev)]))
		return (ENXIO);

	s = spltty();
	switch (APMDEV(dev)) {
	case APMDEV_CTL:
		if (!(flag & FWRITE)) {
			error = EINVAL;
			break;
		}
		if (sc->sc_flags & SCFLAG_OWRITE) {
			error = EBUSY;
			break;
		}
		sc->sc_flags |= SCFLAG_OWRITE;
		break;
	case APMDEV_NORMAL:
		if (!(flag & FREAD) || (flag & FWRITE)) {
			error = EINVAL;
			break;
		}
		sc->sc_flags |= SCFLAG_OREAD;
		break;
	default:
		error = ENXIO;
		break;
	}
	splx(s);
	return (error);
}

int
acpiclose(dev_t dev, int flag, int mode, struct proc *p)
{
	int error = 0;
	struct acpi_softc *sc;
	int s;

	if (!acpi_cd.cd_ndevs || APMUNIT(dev) != 0 ||
	    !(sc = acpi_cd.cd_devs[APMUNIT(dev)]))
		return (ENXIO);

	s = spltty();
	switch (APMDEV(dev)) {
	case APMDEV_CTL:
		sc->sc_flags &= ~SCFLAG_OWRITE;
		break;
	case APMDEV_NORMAL:
		sc->sc_flags &= ~SCFLAG_OREAD;
		break;
	default:
		error = ENXIO;
		break;
	}
	splx(s);
	return (error);
}

int
acpiioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	int error = 0;
	struct acpi_softc *sc;
	struct acpi_ac *ac;
	struct acpi_bat *bat;
	struct apm_power_info *pi = (struct apm_power_info *)data;
	int bats;
	unsigned int remaining, rem, minutes, rate;
	int s;

	if (!acpi_cd.cd_ndevs || APMUNIT(dev) != 0 ||
	    !(sc = acpi_cd.cd_devs[APMUNIT(dev)]))
		return (ENXIO);

	s = spltty();
	/* fake APM */
	switch (cmd) {
	case APM_IOC_SUSPEND:
	case APM_IOC_STANDBY:
		if ((flag & FWRITE) == 0) {
			error = EBADF;
			break;
		}
		acpi_addtask(sc, acpi_sleep_task, sc, ACPI_STATE_S3);
		acpi_wakeup(sc);
		break;
#ifdef HIBERNATE
	case APM_IOC_HIBERNATE:
		if ((error = suser(p, 0)) != 0)
			break;
		if ((flag & FWRITE) == 0) {
			error = EBADF;
			break;
		}
		if (get_hibernate_io_function(swdevt[0].sw_dev) == NULL) {
			error = EOPNOTSUPP;
			break;
		}
		acpi_addtask(sc, acpi_sleep_task, sc, ACPI_STATE_S4);
		acpi_wakeup(sc);
		break;
#endif
	case APM_IOC_GETPOWER:
		/* A/C */
		pi->ac_state = APM_AC_UNKNOWN;
		SLIST_FOREACH(ac, &sc->sc_ac, aac_link) {
			if (ac->aac_softc->sc_ac_stat == PSR_ONLINE)
				pi->ac_state = APM_AC_ON;
			else if (ac->aac_softc->sc_ac_stat == PSR_OFFLINE)
				if (pi->ac_state == APM_AC_UNKNOWN)
					pi->ac_state = APM_AC_OFF;
		}

		/* battery */
		pi->battery_state = APM_BATT_UNKNOWN;
		pi->battery_life = 0;
		pi->minutes_left = 0;
		bats = 0;
		remaining = rem = 0;
		minutes = 0;
		rate = 0;
		SLIST_FOREACH(bat, &sc->sc_bat, aba_link) {
			if (bat->aba_softc->sc_bat_present == 0)
				continue;

			if (bat->aba_softc->sc_bif.bif_last_capacity == 0)
				continue;

			bats++;
			rem = (bat->aba_softc->sc_bst.bst_capacity * 100) /
			    bat->aba_softc->sc_bif.bif_last_capacity;
			if (rem > 100)
				rem = 100;
			remaining += rem;

			if (bat->aba_softc->sc_bst.bst_rate == BST_UNKNOWN)
				continue;
			else if (bat->aba_softc->sc_bst.bst_rate > 1)
				rate = bat->aba_softc->sc_bst.bst_rate;

			minutes += bat->aba_softc->sc_bst.bst_capacity;
		}

		if (bats == 0) {
			pi->battery_state = APM_BATTERY_ABSENT;
			pi->battery_life = 0;
			pi->minutes_left = (unsigned int)-1;
			break;
		}

		if (pi->ac_state == APM_AC_ON || rate == 0)
			pi->minutes_left = (unsigned int)-1;
		else
			pi->minutes_left = 60 * minutes / rate;

		/* running on battery */
		pi->battery_life = remaining / bats;
		if (pi->battery_life > 50)
			pi->battery_state = APM_BATT_HIGH;
		else if (pi->battery_life > 25)
			pi->battery_state = APM_BATT_LOW;
		else
			pi->battery_state = APM_BATT_CRITICAL;

		break;

	default:
		error = ENOTTY;
	}

	splx(s);
	return (error);
}

void	acpi_filtdetach(struct knote *);
int	acpi_filtread(struct knote *, long);

struct filterops acpiread_filtops = {
	1, NULL, acpi_filtdetach, acpi_filtread
};

int acpi_evindex;

int
acpi_record_event(struct acpi_softc *sc, u_int type)
{
	if ((sc->sc_flags & SCFLAG_OPEN) == 0)
		return (1);

	acpi_evindex++;
	KNOTE(sc->sc_note, APM_EVENT_COMPOSE(type, acpi_evindex));
	return (0);
}

void
acpi_filtdetach(struct knote *kn)
{
	struct acpi_softc *sc = kn->kn_hook;
	int s;

	s = spltty();
	SLIST_REMOVE(sc->sc_note, kn, knote, kn_selnext);
	splx(s);
}

int
acpi_filtread(struct knote *kn, long hint)
{
	/* XXX weird kqueue_scan() semantics */
	if (hint && !kn->kn_data)
		kn->kn_data = hint;
	return (1);
}

int
acpikqfilter(dev_t dev, struct knote *kn)
{
	struct acpi_softc *sc;
	int s;

	if (!acpi_cd.cd_ndevs || APMUNIT(dev) != 0 ||
	    !(sc = acpi_cd.cd_devs[APMUNIT(dev)]))
		return (ENXIO);

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &acpiread_filtops;
		break;
	default:
		return (EINVAL);
	}

	kn->kn_hook = sc;

	s = spltty();
	SLIST_INSERT_HEAD(sc->sc_note, kn, kn_selnext);
	splx(s);

	return (0);
}

#else /* SMALL_KERNEL */

int
acpiopen(dev_t dev, int flag, int mode, struct proc *p)
{
	return (ENXIO);
}

int
acpiclose(dev_t dev, int flag, int mode, struct proc *p)
{
	return (ENXIO);
}

int
acpiioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	return (ENXIO);
}

int
acpikqfilter(dev_t dev, struct knote *kn)
{
	return (ENXIO);
}
#endif /* SMALL_KERNEL */
