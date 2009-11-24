/* $OpenBSD: acpi.c,v 1.150 2009/11/24 23:01:41 jsg Exp $ */
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
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/event.h>
#include <sys/signalvar.h>
#include <sys/proc.h>
#include <sys/kthread.h>
#include <sys/workq.h>

#include <machine/conf.h>
#include <machine/cpufunc.h>
#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/dsdt.h>

#include <dev/pci/pciidereg.h>
#include <dev/pci/pciidevar.h>

#include <machine/apmvar.h>
#define APMUNIT(dev)	(minor(dev)&0xf0)
#define APMDEV(dev)	(minor(dev)&0x0f)
#define APMDEV_NORMAL	0
#define APMDEV_CTL	8

#ifdef ACPI_DEBUG
int acpi_debug = 16;
#endif
int acpi_enabled;
int acpi_poll_enabled;
int acpi_hasprocfvs;
int acpi_thinkpad_enabled;
int acpi_saved_spl;

#define ACPIEN_RETRIES 15

void	acpi_isr_thread(void *);
void	acpi_create_thread(void *);

int	acpi_match(struct device *, void *, void *);
void	acpi_attach(struct device *, struct device *, void *);
int	acpi_submatch(struct device *, void *, void *);
int	acpi_print(void *, const char *);
void	acpi_handle_suspend_failure(struct acpi_softc *);

void	acpi_map_pmregs(struct acpi_softc *);

int	acpi_founddock(struct aml_node *, void *);
int	acpi_foundpss(struct aml_node *, void *);
int	acpi_foundhid(struct aml_node *, void *);
int	acpi_foundec(struct aml_node *, void *);
int	acpi_foundtmp(struct aml_node *, void *);
int	acpi_foundprt(struct aml_node *, void *);
int	acpi_foundprw(struct aml_node *, void *);
int	acpi_foundvideo(struct aml_node *, void *);
int	acpi_inidev(struct aml_node *, void *);

int	acpi_loadtables(struct acpi_softc *, struct acpi_rsdp *);

void	acpi_init_states(struct acpi_softc *);
void	acpi_init_gpes(struct acpi_softc *);
void	acpi_init_pm(struct acpi_softc *);

int acpi_foundide(struct aml_node *node, void *arg);
int acpiide_notify(struct aml_node *, int, void *);

void  wdcattach(struct channel_softc *);
int   wdcdetach(struct channel_softc *, int);

struct acpi_q *acpi_maptable(paddr_t, const char *, const char *, const char *);

struct idechnl
{
	struct acpi_softc *sc;
	int64_t 	addr;
	int64_t 	chnl;
	int64_t 	sta;
};

int is_ejectable_bay(struct aml_node *node);
int is_ata(struct aml_node *node);
int is_ejectable(struct aml_node *node);

#ifndef SMALL_KERNEL
void	acpi_resume(struct acpi_softc *, int);
void	acpi_susp_resume_gpewalk(struct acpi_softc *, int, int);
#endif /* SMALL_KERNEL */

#ifndef SMALL_KERNEL
int acpi_add_device(struct aml_node *node, void *arg);
#endif /* SMALL_KERNEL */

void	acpi_enable_onegpe(struct acpi_softc *, int, int);
int	acpi_gpe_level(struct acpi_softc *, int, void *);
int	acpi_gpe_edge(struct acpi_softc *, int, void *);

struct gpe_block *acpi_find_gpe(struct acpi_softc *, int);

#define	ACPI_LOCK(sc)
#define	ACPI_UNLOCK(sc)

/* XXX move this into dsdt softc at some point */
extern struct aml_node aml_root;

/* XXX do we need this? */
void	acpi_filtdetach(struct knote *);
int	acpi_filtread(struct knote *, long);

struct filterops acpiread_filtops = {
	1, NULL, acpi_filtdetach, acpi_filtread
};

struct cfattach acpi_ca = {
	sizeof(struct acpi_softc), acpi_match, acpi_attach, NULL,
	config_activate_children
};

struct cfdriver acpi_cd = {
	NULL, "acpi", DV_DULL
};

struct acpi_softc *acpi_softc;
int acpi_evindex;

#define acpi_bus_space_map	_bus_space_map
#define acpi_bus_space_unmap	_bus_space_unmap

#define pch(x) (((x)>=' ' && (x)<='z') ? (x) : ' ')

#if 0
void
acpi_delay(struct acpi_softc *sc, int64_t uSecs)
{
	/* XXX this needs to become a tsleep later */
	delay(uSecs);
}
#endif

int
acpi_gasio(struct acpi_softc *sc, int iodir, int iospace, uint64_t address,
    int access_size, int len, void *buffer)
{
	u_int8_t *pb;
	bus_space_handle_t ioh;
	struct acpi_mem_map mh;
	pci_chipset_tag_t pc;
	pcitag_t tag;
	bus_addr_t ioaddr;
	int reg, idx, ival, sval;

	dnprintf(50, "gasio: %.2x 0x%.8llx %s\n",
	    iospace, address, (iodir == ACPI_IOWRITE) ? "write" : "read");

	pb = (u_int8_t *)buffer;
	switch (iospace) {
	case GAS_SYSTEM_MEMORY:
		/* copy to/from system memory */
		acpi_map(address, len, &mh);
		if (iodir == ACPI_IOREAD)
			memcpy(buffer, mh.va, len);
		else
			memcpy(mh.va, buffer, len);
		acpi_unmap(&mh);
		break;

	case GAS_SYSTEM_IOSPACE:
		/* read/write from I/O registers */
		ioaddr = address;
		if (acpi_bus_space_map(sc->sc_iot, ioaddr, len, 0, &ioh) != 0) {
			printf("unable to map iospace\n");
			return (-1);
		}
		for (reg = 0; reg < len; reg += access_size) {
			if (iodir == ACPI_IOREAD) {
				switch (access_size) {
				case 1:
					*(uint8_t *)(pb+reg) = bus_space_read_1(
					    sc->sc_iot, ioh, reg);
					dnprintf(80, "os_in8(%llx) = %x\n",
					    reg+address, *(uint8_t *)(pb+reg));
					break;
				case 2:
					*(uint16_t *)(pb+reg) = bus_space_read_2(
					    sc->sc_iot, ioh, reg);
					dnprintf(80, "os_in16(%llx) = %x\n",
					    reg+address, *(uint16_t *)(pb+reg));
					break;
				case 4:
					*(uint32_t *)(pb+reg) = bus_space_read_4(
					    sc->sc_iot, ioh, reg);
					break;
				default:
					printf("rdio: invalid size %d\n", access_size);
					break;
				}
			} else {
				switch (access_size) {
				case 1:
					bus_space_write_1(sc->sc_iot, ioh, reg,
					    *(uint8_t *)(pb+reg));
					dnprintf(80, "os_out8(%llx,%x)\n",
					    reg+address, *(uint8_t *)(pb+reg));
					break;
				case 2:
					bus_space_write_2(sc->sc_iot, ioh, reg,
					    *(uint16_t *)(pb+reg));
					dnprintf(80, "os_out16(%llx,%x)\n",
					    reg+address, *(uint16_t *)(pb+reg));
					break;
				case 4:
					bus_space_write_4(sc->sc_iot, ioh, reg,
					    *(uint32_t *)(pb+reg));
					break;
				default:
					printf("wrio: invalid size %d\n", access_size);
					break;
				}
			}

			/* During autoconf some devices are still gathering
			 * information.  Delay here to give them an opportunity
			 * to finish.  During runtime we simply need to ignore
			 * transient values.
			 */
			if (cold)
				delay(10000);
		}
		acpi_bus_space_unmap(sc->sc_iot, ioh, len, &ioaddr);
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
		if (sc->sc_ec == NULL)
			break;
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

void
acpi_attach(struct device *parent, struct device *self, void *aux)
{
	struct bios_attach_args *ba = aux;
	struct acpi_softc *sc = (struct acpi_softc *)self;
	struct acpi_mem_map handle;
	struct acpi_rsdp *rsdp;
	struct acpi_q *entry;
	struct acpi_dsdt *p_dsdt;
	int idx;
#ifndef SMALL_KERNEL
	struct acpi_wakeq *wentry;
	struct device *dev;
	struct acpi_ac *ac;
	struct acpi_bat *bat;
#endif /* SMALL_KERNEL */
	paddr_t facspa;

	sc->sc_iot = ba->ba_iot;
	sc->sc_memt = ba->ba_memt;

	if (acpi_map(ba->ba_acpipbase, sizeof(struct acpi_rsdp), &handle)) {
		printf(": can't map memory\n");
		return;
	}

	rsdp = (struct acpi_rsdp *)handle.va;
	sc->sc_revision = (int)rsdp->rsdp_revision;
	printf(": rev %d", sc->sc_revision);

	SIMPLEQ_INIT(&sc->sc_tables);
	SIMPLEQ_INIT(&sc->sc_wakedevs);

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
	if (!sc->sc_fadt->smi_cmd ||
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

	acpi_enabled = 1;

	/* Create opcode hashtable */
	aml_hashopcodes();

	/* Create Default AML objects */
	aml_create_defaultobjects();

	/*
	 * Load the DSDT from the FADT pointer -- use the
	 * extended (64-bit) pointer if it exists
	 */
	if (sc->sc_fadt->hdr_revision < 3 || sc->sc_fadt->x_dsdt == 0)
		entry = acpi_maptable(sc->sc_fadt->dsdt, NULL, NULL, NULL);
	else
		entry = acpi_maptable(sc->sc_fadt->x_dsdt, NULL, NULL, NULL);

	if (entry == NULL)
		printf(" !DSDT");
	SIMPLEQ_INSERT_HEAD(&sc->sc_tables, entry, q_next);

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

#ifndef SMALL_KERNEL
	/* Find available sleeping states */
	acpi_init_states(sc);

	/* Find available sleep/resume related methods. */
	acpi_init_pm(sc);
#endif /* SMALL_KERNEL */

	/* Map Power Management registers */
	acpi_map_pmregs(sc);

#ifndef SMALL_KERNEL
	/* Initialize GPE handlers */
	acpi_init_gpes(sc);

	/* some devices require periodic polling */
	timeout_set(&sc->sc_dev_timeout, acpi_poll, sc);
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
	acpi_write_pmreg(sc, ACPIREG_SMICMD, 0, sc->sc_fadt->acpi_enable);
	idx = 0;
	do {
		if (idx++ > ACPIEN_RETRIES) {
			printf(", can't enable ACPI\n");
			return;
		}
	} while (!(acpi_read_pmreg(sc, ACPIREG_PM1_CNT, 0) & ACPI_PM1_SCI_EN));

	printf("\n%s: tables", DEVNAME(sc));
	SIMPLEQ_FOREACH(entry, &sc->sc_tables, q_next) {
		printf(" %.4s", entry->q_table);
	}
	printf("\n");

#ifndef SMALL_KERNEL
	/* Display wakeup devices and lowest S-state */
	printf("%s: wakeup devices", DEVNAME(sc));
	SIMPLEQ_FOREACH(wentry, &sc->sc_wakedevs, q_next) {
		printf(" %.4s(S%d)", wentry->q_node->name,
		    wentry->q_state);
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

	acpi_softc = sc;

	/* initialize runtime environment */
	aml_find_node(&aml_root, "_INI", acpi_inidev, sc);

	/* attach pci interrupt routing tables */
	aml_find_node(&aml_root, "_PRT", acpi_foundprt, sc);

#ifndef SMALL_KERNEL
	 /* XXX EC needs to be attached first on some systems */
	aml_find_node(&aml_root, "_HID", acpi_foundec, sc);

	aml_walknodes(&aml_root, AML_WALK_PRE, acpi_add_device, sc);

	/* attach battery, power supply and button devices */
	aml_find_node(&aml_root, "_HID", acpi_foundhid, sc);

	/* Attach IDE bay */
	aml_walknodes(&aml_root, AML_WALK_PRE, acpi_foundide, sc);

	/* attach docks */
	aml_find_node(&aml_root, "_DCK", acpi_founddock, sc);

	/* attach video only if this is not a stinkpad */
	if (!acpi_thinkpad_enabled)
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
acpi_maptable(paddr_t addr, const char *sig, const char *oem, const char *tbl)
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
	}
	acpi_unmap(&handle);
	return entry;
}

int
acpi_loadtables(struct acpi_softc *sc, struct acpi_rsdp *rsdp)
{
	struct acpi_q *entry, *sdt;
	int i, ntables;
	size_t len;

	if (rsdp->rsdp_revision == 2 && rsdp->rsdp_xsdt) {
		struct acpi_xsdt *xsdt;

		sdt = acpi_maptable(rsdp->rsdp_xsdt, NULL, NULL, NULL);
		if (sdt == NULL) {
			printf("couldn't map rsdt\n");
			return (ENOMEM);
		}

		xsdt = (struct acpi_xsdt *)sdt->q_data;
		len  = xsdt->hdr.length;
		ntables = (len - sizeof(struct acpi_table_header)) /
		    sizeof(xsdt->table_offsets[0]);

		for (i = 0; i < ntables; i++) {
			entry = acpi_maptable(xsdt->table_offsets[i], NULL, NULL,
			    NULL);
			if (entry != NULL)
				SIMPLEQ_INSERT_TAIL(&sc->sc_tables, entry,
				    q_next);
		}
		free(sdt, M_DEVBUF);
	} else {
		struct acpi_rsdt *rsdt;

		sdt = acpi_maptable(rsdp->rsdp_rsdt, NULL, NULL, NULL);
		if (sdt == NULL) {
			printf("couldn't map rsdt\n");
			return (ENOMEM);
		}

		rsdt = (struct acpi_rsdt *)sdt->q_data;
		len  = rsdt->hdr.length;
		ntables = (len - sizeof(struct acpi_table_header)) /
		    sizeof(rsdt->table_offsets[0]);

		for (i = 0; i < ntables; i++) {
			entry = acpi_maptable(rsdt->table_offsets[i], NULL, NULL,
			    NULL);
			if (entry != NULL)
				SIMPLEQ_INSERT_TAIL(&sc->sc_tables, entry,
				    q_next);
		}
		free(sdt, M_DEVBUF);
	}

	return (0);
}

int
acpiopen(dev_t dev, int flag, int mode, struct proc *p)
{
	int error = 0;
#ifndef SMALL_KERNEL
	struct acpi_softc *sc;

	if (!acpi_cd.cd_ndevs || APMUNIT(dev) != 0 ||
	    !(sc = acpi_cd.cd_devs[APMUNIT(dev)]))
		return (ENXIO);

	switch (APMDEV(dev)) {
	case APMDEV_CTL:
		if (!(flag & FWRITE)) {
			error = EINVAL;
			break;
		}
		break;
	case APMDEV_NORMAL:
		if (!(flag & FREAD) || (flag & FWRITE)) {
			error = EINVAL;
			break;
		}
		break;
	default:
		error = ENXIO;
		break;
	}
#else
	error = ENXIO;
#endif
	return (error);
}

int
acpiclose(dev_t dev, int flag, int mode, struct proc *p)
{
	int error = 0;
#ifndef SMALL_KERNEL
	struct acpi_softc *sc;

	if (!acpi_cd.cd_ndevs || APMUNIT(dev) != 0 ||
	    !(sc = acpi_cd.cd_devs[APMUNIT(dev)]))
		return (ENXIO);

	switch (APMDEV(dev)) {
	case APMDEV_CTL:
	case APMDEV_NORMAL:
		break;
	default:
		error = ENXIO;
		break;
	}
#else
	error = ENXIO;
#endif
	return (error);
}

int
acpiioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	int error = 0;
#ifndef SMALL_KERNEL
	struct acpi_softc *sc;
	struct acpi_ac *ac;
	struct acpi_bat *bat;
	struct apm_power_info *pi = (struct apm_power_info *)data;
	int bats;
	unsigned int remaining, rem, minutes, rate;

	if (!acpi_cd.cd_ndevs || APMUNIT(dev) != 0 ||
	    !(sc = acpi_cd.cd_devs[APMUNIT(dev)]))
		return (ENXIO);

	ACPI_LOCK(sc);
	/* fake APM */
	switch (cmd) {
	case APM_IOC_STANDBY_REQ:
	case APM_IOC_SUSPEND_REQ:
	case APM_IOC_SUSPEND:
	case APM_IOC_STANDBY:
		workq_add_task(NULL, 0, (workq_fn)acpi_sleep_state,
		    acpi_softc, (void *)ACPI_STATE_S3);
		break;
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
			pi->minutes_left = 100 * minutes / rate;

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

	ACPI_UNLOCK(sc);
#else
	error = ENXIO;
#endif /* SMALL_KERNEL */
	return (error);
}

void
acpi_filtdetach(struct knote *kn)
{
#ifndef SMALL_KERNEL
	struct acpi_softc *sc = kn->kn_hook;

	ACPI_LOCK(sc);
	SLIST_REMOVE(sc->sc_note, kn, knote, kn_selnext);
	ACPI_UNLOCK(sc);
#endif
}

int
acpi_filtread(struct knote *kn, long hint)
{
#ifndef SMALL_KERNEL
	/* XXX weird kqueue_scan() semantics */
	if (hint & !kn->kn_data)
		kn->kn_data = hint;
#endif
	return (1);
}

int
acpikqfilter(dev_t dev, struct knote *kn)
{
#ifndef SMALL_KERNEL
	struct acpi_softc *sc;

	if (!acpi_cd.cd_ndevs || APMUNIT(dev) != 0 ||
	    !(sc = acpi_cd.cd_devs[APMUNIT(dev)]))
		return (ENXIO);

	switch (kn->kn_filter) {
	case EVFILT_READ:
		kn->kn_fop = &acpiread_filtops;
		break;
	default:
		return (1);
	}

	kn->kn_hook = sc;

	ACPI_LOCK(sc);
	SLIST_INSERT_HEAD(sc->sc_note, kn, kn_selnext);
	ACPI_UNLOCK(sc);

	return (0);
#else
	return (1);
#endif
}

/* Read from power management register */
int
acpi_read_pmreg(struct acpi_softc *sc, int reg, int offset)
{
	bus_space_handle_t ioh;
	bus_size_t size, __size;
	int regval;

	__size = 0;
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
		__size = 1;
		dnprintf(50, "read GPE_STS  offset: %.2x %.2x %.2x\n", offset,
		    sc->sc_fadt->gpe0_blk_len>>1, sc->sc_fadt->gpe1_blk_len>>1);
		if (offset < (sc->sc_fadt->gpe0_blk_len >> 1)) {
			reg = ACPIREG_GPE0_STS;
		}
		break;
	case ACPIREG_GPE_EN:
		__size = 1;
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
	if (__size)
		size = __size;
	if (size > 4)
		size = 4;

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
	bus_size_t size, __size;

	__size = 0;
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
		__size = 1;
		dnprintf(50, "write GPE_STS offset: %.2x %.2x %.2x %.2x\n",
		    offset, sc->sc_fadt->gpe0_blk_len>>1,
		    sc->sc_fadt->gpe1_blk_len>>1, regval);
		if (offset < (sc->sc_fadt->gpe0_blk_len >> 1)) {
			reg = ACPIREG_GPE0_STS;
		}
		break;
	case ACPIREG_GPE_EN:
		__size = 1;
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
	if (__size)
		size = __size;
	if (size > 4)
		size = 4;
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
	bus_size_t size;
	const char *name;
	int reg;

	for (reg = 0; reg < ACPIREG_MAXREG; reg++) {
		size = 0;
		switch (reg) {
		case ACPIREG_SMICMD:
			name = "smi";
			size = 1;
			addr = sc->sc_fadt->smi_cmd;
			break;
		case ACPIREG_PM1A_STS:
		case ACPIREG_PM1A_EN:
			name = "pm1a_sts";
			size = sc->sc_fadt->pm1_evt_len >> 1;
			addr = sc->sc_fadt->pm1a_evt_blk;
			if (reg == ACPIREG_PM1A_EN && addr) {
				addr += size;
				name = "pm1a_en";
			}
			break;
		case ACPIREG_PM1A_CNT:
			name = "pm1a_cnt";
			size = sc->sc_fadt->pm1_cnt_len;
			addr = sc->sc_fadt->pm1a_cnt_blk;
			break;
		case ACPIREG_PM1B_STS:
		case ACPIREG_PM1B_EN:
			name = "pm1b_sts";
			size = sc->sc_fadt->pm1_evt_len >> 1;
			addr = sc->sc_fadt->pm1b_evt_blk;
			if (reg == ACPIREG_PM1B_EN && addr) {
				addr += size;
				name = "pm1b_en";
			}
			break;
		case ACPIREG_PM1B_CNT:
			name = "pm1b_cnt";
			size = sc->sc_fadt->pm1_cnt_len;
			addr = sc->sc_fadt->pm1b_cnt_blk;
			break;
		case ACPIREG_PM2_CNT:
			name = "pm2_cnt";
			size = sc->sc_fadt->pm2_cnt_len;
			addr = sc->sc_fadt->pm2_cnt_blk;
			break;
#if 0
		case ACPIREG_PM_TMR:
			/* Allocated in acpitimer */
			name = "pm_tmr";
			size = sc->sc_fadt->pm_tmr_len;
			addr = sc->sc_fadt->pm_tmr_blk;
			break;
#endif
		case ACPIREG_GPE0_STS:
		case ACPIREG_GPE0_EN:
			name = "gpe0_sts";
			size = sc->sc_fadt->gpe0_blk_len >> 1;
			addr = sc->sc_fadt->gpe0_blk;

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
			dnprintf(50, "mapping: %.4x %.4x %s\n",
			    addr, size, name);

			/* Size and address exist; map register space */
			bus_space_map(sc->sc_iot, addr, size, 0,
			    &sc->sc_pmregs[reg].ioh);

			sc->sc_pmregs[reg].name = name;
			sc->sc_pmregs[reg].size = size;
			sc->sc_pmregs[reg].addr = addr;
		}
	}
}

/* move all stuff that doesn't go on the boot media in here */
#ifndef SMALL_KERNEL
void
acpi_reset(void)
{
	struct acpi_fadt	*fadt;
	u_int32_t		 reset_as, reset_len;
	u_int32_t		 value;

	fadt = acpi_softc->sc_fadt;

	/*
	 * RESET_REG_SUP is not properly set in some implementations,
	 * but not testing against it breaks more machines than it fixes
	 */
	if (acpi_softc->sc_revision <= 1 ||
	    !(fadt->flags & FADT_RESET_REG_SUP) || fadt->reset_reg.address == 0)
		return;

	value = fadt->reset_value;

	reset_as = fadt->reset_reg.register_bit_width / 8;
	if (reset_as == 0)
		reset_as = 1;

	reset_len = fadt->reset_reg.access_size;
	if (reset_len == 0)
		reset_len = reset_as;

	acpi_gasio(acpi_softc, ACPI_IOWRITE,
	    fadt->reset_reg.address_space_id,
	    fadt->reset_reg.address, reset_as, reset_len, &value);

	delay(100000);
}

int
acpi_interrupt(void *arg)
{
	struct acpi_softc *sc = (struct acpi_softc *)arg;
	u_int32_t processed, sts, en, idx, jdx;

	processed = 0;

#if 0
	acpi_add_gpeblock(sc, sc->sc_fadt->gpe0_blk, sc->sc_fadt->gpe0_blk_len>>1, 0);
	acpi_add_gpeblock(sc, sc->sc_fadt->gpe1_blk, sc->sc_fadt->gpe1_blk_len>>1,
	    sc->sc_fadt->gpe1_base);
#endif

	dnprintf(40, "ACPI Interrupt\n");
	for (idx = 0; idx < sc->sc_lastgpe; idx += 8) {
		sts = acpi_read_pmreg(sc, ACPIREG_GPE_STS, idx>>3);
		en  = acpi_read_pmreg(sc, ACPIREG_GPE_EN,  idx>>3);
		if (en & sts) {
			dnprintf(10, "GPE block: %.2x %.2x %.2x\n", idx, sts,
			    en);
			acpi_write_pmreg(sc, ACPIREG_GPE_EN, idx>>3, en & ~sts);
			for (jdx = 0; jdx < 8; jdx++) {
				if (en & sts & (1L << jdx)) {
					/* Signal this GPE */
					sc->gpe_table[idx+jdx].active = 1;
					processed = 1;
				}
			}
		}
	}

	sts = acpi_read_pmreg(sc, ACPIREG_PM1_STS, 0);
	en  = acpi_read_pmreg(sc, ACPIREG_PM1_EN, 0);
	if (sts & en) {
		dnprintf(10,"GEN interrupt: %.4x\n", sts & en);
		acpi_write_pmreg(sc, ACPIREG_PM1_EN, 0, en & ~sts);
		acpi_write_pmreg(sc, ACPIREG_PM1_STS, 0, en);
		acpi_write_pmreg(sc, ACPIREG_PM1_EN, 0, en);
		if (sts & ACPI_PM1_PWRBTN_STS)
			sc->sc_powerbtn = 1;
		if (sts & ACPI_PM1_SLPBTN_STS)
			sc->sc_sleepbtn = 1;
		processed = 1;
	}

	if (processed) {
		sc->sc_wakeup = 0;
		wakeup(sc);
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
acpi_enable_onegpe(struct acpi_softc *sc, int gpe, int enable)
{
	uint8_t mask = (1L << (gpe & 7));
	uint8_t en;

	/* Read enabled register */
	en = acpi_read_pmreg(sc, ACPIREG_GPE_EN, gpe>>3);
	dnprintf(50, "%sabling GPE %.2x (current: %sabled) %.2x\n",
	    enable ? "en" : "dis", gpe, (en & mask) ? "en" : "dis", en);
	if (enable)
		en |= mask;
	else
		en &= ~mask;
	acpi_write_pmreg(sc, ACPIREG_GPE_EN, gpe>>3, en);
}

int
acpi_set_gpehandler(struct acpi_softc *sc, int gpe, int (*handler)
    (struct acpi_softc *, int, void *), void *arg, const char *label)
{
	struct gpe_block *ptbl;

	ptbl = acpi_find_gpe(sc, gpe);
	if (ptbl == NULL || handler == NULL)
		return -EINVAL;
	if (ptbl->handler != NULL) {
		dnprintf(10, "error: GPE %.2x already enabled\n", gpe);
		return -EBUSY;
	}
	dnprintf(50, "Adding GPE handler %.2x (%s)\n", gpe, label);
	ptbl->handler = handler;
	ptbl->arg = arg;

	return (0);
}

int
acpi_gpe_level(struct acpi_softc *sc, int gpe, void *arg)
{
	struct aml_node *node = arg;
	uint8_t mask;

	dnprintf(10, "handling Level-sensitive GPE %.2x\n", gpe);
	mask = (1L << (gpe & 7));

	aml_evalnode(sc, node, 0, NULL, NULL);
	acpi_write_pmreg(sc, ACPIREG_GPE_STS, gpe>>3, mask);
	acpi_write_pmreg(sc, ACPIREG_GPE_EN,  gpe>>3, mask);

	return (0);
}

int
acpi_gpe_edge(struct acpi_softc *sc, int gpe, void *arg)
{

	struct aml_node *node = arg;
	uint8_t mask;

	dnprintf(10, "handling Edge-sensitive GPE %.2x\n", gpe);
	mask = (1L << (gpe & 7));

	aml_evalnode(sc, node, 0, NULL, NULL);
	acpi_write_pmreg(sc, ACPIREG_GPE_STS, gpe>>3, mask);
	acpi_write_pmreg(sc, ACPIREG_GPE_EN,  gpe>>3, mask);

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
	if (wq == NULL) {
		return 0;
	}

	wq->q_wakepkg = malloc(sizeof(struct aml_value), M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (wq->q_wakepkg == NULL) {
		free(wq, M_DEVBUF);
		return 0;
	}
	dnprintf(10, "Found _PRW (%s)\n", node->parent->name);
	aml_evalnode(sc, node, 0, NULL, wq->q_wakepkg);
	wq->q_node = node->parent;
	wq->q_gpe = -1;

	/* Get GPE of wakeup device, and lowest sleep level */
	if (wq->q_wakepkg->type == AML_OBJTYPE_PACKAGE && wq->q_wakepkg->length >= 2) {
	  if (wq->q_wakepkg->v_package[0]->type == AML_OBJTYPE_INTEGER) {
	    wq->q_gpe = wq->q_wakepkg->v_package[0]->v_integer;
	  }
	  if (wq->q_wakepkg->v_package[1]->type == AML_OBJTYPE_INTEGER) {
	    wq->q_state = wq->q_wakepkg->v_package[1]->v_integer;
	  }
	}
	SIMPLEQ_INSERT_TAIL(&sc->sc_wakedevs, wq, q_next);
	return 0;
}

struct gpe_block *
acpi_find_gpe(struct acpi_softc *sc, int gpe)
{
#if 1
	if (gpe >= sc->sc_lastgpe)
		return NULL;
	return &sc->gpe_table[gpe];
#else
	SIMPLEQ_FOREACH(pgpe, &sc->sc_gpes, gpe_link) {
		if (gpe >= pgpe->start && gpe <= (pgpe->start+7))
			return &pgpe->table[gpe & 7];
	}
	return NULL;
#endif
}

#if 0
/* New GPE handling code: Create GPE block */
void
acpi_init_gpeblock(struct acpi_softc *sc, int reg, int len, int base)
{
	int i, j;

	if (!reg || !len)
		return;
	for (i = 0; i < len; i++) {
		pgpe = acpi_os_malloc(sizeof(gpeblock));
		if (pgpe == NULL)
			return;

		/* Allocate GPE Handler Block */
		pgpe->start = base + i;
		acpi_bus_space_map(sc->sc_iot, reg+i,     1, 0, &pgpe->sts_ioh);
		acpi_bus_space_map(sc->sc_iot, reg+i+len, 1, 0, &pgpe->en_ioh);
		SIMPLEQ_INSERT_TAIL(&sc->sc_gpes, gpe, gpe_link);

		/* Clear pending GPEs */
		bus_space_write_1(sc->sc_iot, pgpe->sts_ioh, 0, 0xFF);
		bus_space_write_1(sc->sc_iot, pgpe->en_ioh,  0, 0x00);
	}

	/* Search for GPE handlers */
	for (i = 0; i < len*8; i++) {
		char gpestr[32];
		struct aml_node *h;

		snprintf(gpestr, sizeof(gpestr), "\\_GPE._L%.2X", base+i);
		h = aml_searchnode(&aml_root, gpestr);
		if (acpi_set_gpehandler(sc, base+i, acpi_gpe_level, h, "level") != 0) {
			snprintf(gpestr, sizeof(gpestr), "\\_GPE._E%.2X", base+i);
			h = aml_searchnode(&aml_root, gpestr);
			acpi_set_gpehandler(sc, base+i, acpi_gpe_edge, h, "edge");
		}
	}
}

/* Process GPE interrupts */
int
acpi_handle_gpes(struct acpi_softc *sc)
{
	uint8_t en, sts;
	int processed, i;

	processed = 0;
	SIMPLEQ_FOREACH(pgpe, &sc->sc_gpes, gpe_link) {
		sts = bus_space_read_1(sc->sc_iot, pgpe->sts_ioh, 0);
		en = bus_space_read_1(sc->sc_iot, pgpe->en_ioh, 0);
		for (i = 0; i< 8 ; i++) {
			if (en & sts & (1L << i)) {
				pgpe->table[i].active = 1;
				processed = 1;
			}
		}
	}
	return processed;
}
#endif

#if 0
void
acpi_add_gpeblock(struct acpi_softc *sc, int reg, int len, int gpe)
{
	int idx, jdx;
	u_int8_t en, sts;

	if (!reg || !len)
		return;
	for (idx = 0; idx < len; idx++) {
		sts = inb(reg + idx);
		en  = inb(reg + len + idx);
		printf("-- gpe %.2x-%.2x : en:%.2x sts:%.2x  %.2x\n",
		    gpe+idx*8, gpe+idx*8+7, en, sts, en&sts);
		for (jdx = 0; jdx < 8; jdx++) {
			char gpestr[32];
			struct aml_node *l, *e;

			if (en & sts & (1L << jdx)) {
				snprintf(gpestr,sizeof(gpestr), "\\_GPE._L%.2X", gpe+idx*8+jdx);
				l = aml_searchname(&aml_root, gpestr);
				snprintf(gpestr,sizeof(gpestr), "\\_GPE._E%.2X", gpe+idx*8+jdx);
				e = aml_searchname(&aml_root, gpestr);
				printf("  GPE %.2x active L%x E%x\n", gpe+idx*8+jdx, l, e);
			}
		}
	}
}
#endif

void
acpi_init_gpes(struct acpi_softc *sc)
{
	struct aml_node *gpe;
	char name[12];
	int  idx, ngpe;

#if 0
	acpi_add_gpeblock(sc, sc->sc_fadt->gpe0_blk, sc->sc_fadt->gpe0_blk_len>>1, 0);
	acpi_add_gpeblock(sc, sc->sc_fadt->gpe1_blk, sc->sc_fadt->gpe1_blk_len>>1,
	    sc->sc_fadt->gpe1_base);
#endif

	sc->sc_lastgpe = sc->sc_fadt->gpe0_blk_len << 2;
	if (sc->sc_fadt->gpe1_blk_len) {
	}
	dnprintf(50, "Last GPE: %.2x\n", sc->sc_lastgpe);

	/* Allocate GPE table */
	sc->gpe_table = malloc(sc->sc_lastgpe * sizeof(struct gpe_block),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	ngpe = 0;

	/* Clear GPE status */
	for (idx = 0; idx < sc->sc_lastgpe; idx += 8) {
		acpi_write_pmreg(sc, ACPIREG_GPE_EN,  idx>>3, 0);
		acpi_write_pmreg(sc, ACPIREG_GPE_STS, idx>>3, -1);
	}
	for (idx = 0; idx < sc->sc_lastgpe; idx++) {
		/* Search Level-sensitive GPES */
		snprintf(name, sizeof(name), "\\_GPE._L%.2X", idx);
		gpe = aml_searchname(&aml_root, name);
		if (gpe != NULL)
			acpi_set_gpehandler(sc, idx, acpi_gpe_level, gpe,
			    "level");
		if (gpe == NULL) {
			/* Search Edge-sensitive GPES */
			snprintf(name, sizeof(name), "\\_GPE._E%.2X", idx);
			gpe = aml_searchname(&aml_root, name);
			if (gpe != NULL)
				acpi_set_gpehandler(sc, idx, acpi_gpe_edge, gpe,
				    "edge");
		}
	}
	aml_find_node(&aml_root, "_PRW", acpi_foundprw, sc);
	sc->sc_maxgpe = ngpe;
}

void
acpi_init_states(struct acpi_softc *sc)
{
	struct aml_value res;
	char name[8];
	int i;

	for (i = ACPI_STATE_S0; i <= ACPI_STATE_S5; i++) {
		snprintf(name, sizeof(name), "_S%d_", i);
		sc->sc_sleeptype[i].slp_typa = -1;
		sc->sc_sleeptype[i].slp_typb = -1;
		if (aml_evalname(sc, &aml_root, name, 0, NULL, &res) == 0) {
			if (res.type == AML_OBJTYPE_PACKAGE) {
				sc->sc_sleeptype[i].slp_typa = aml_val2int(res.v_package[0]);
				sc->sc_sleeptype[i].slp_typb = aml_val2int(res.v_package[1]);
			}
			aml_freevalue(&res);
		}
	}
}

void
acpi_init_pm(struct acpi_softc *sc)
{
	sc->sc_tts = aml_searchname(&aml_root, "_TTS");
	sc->sc_pts = aml_searchname(&aml_root, "_PTS");
	sc->sc_wak = aml_searchname(&aml_root, "_WAK");
	sc->sc_bfs = aml_searchname(&aml_root, "_BFS");
	sc->sc_gts = aml_searchname(&aml_root, "_GTS");
	sc->sc_sst = aml_searchname(&aml_root, "_SST");
}

#ifndef SMALL_KERNEL
void
acpi_susp_resume_gpewalk(struct acpi_softc *sc, int state,
    int wake_gpe_state)
{
	struct acpi_wakeq *wentry;
	int idx;
	u_int32_t gpe;

	/* Clear GPE status */
	for (idx = 0; idx < sc->sc_lastgpe; idx += 8) {
		acpi_write_pmreg(sc, ACPIREG_GPE_EN,  idx>>3, 0);
		acpi_write_pmreg(sc, ACPIREG_GPE_STS, idx>>3, -1);
	}

	SIMPLEQ_FOREACH(wentry, &sc->sc_wakedevs, q_next) {
		dnprintf(10, "%.4s(S%d) gpe %.2x\n", wentry->q_node->name,
		    wentry->q_state,
		    wentry->q_gpe);

		if (state <= wentry->q_state)
			acpi_enable_onegpe(sc, wentry->q_gpe,
			    wake_gpe_state);
	}

	/* If we are resuming (disabling wake GPEs), enable other GPEs */

	if (wake_gpe_state == 0) {
		for (gpe = 0; gpe < sc->sc_lastgpe; gpe++) {
			if (sc->gpe_table[gpe].handler)
				acpi_enable_onegpe(sc, gpe, 1);
		}
	}
}
#endif /* ! SMALL_KERNEL */

int
acpi_sleep_state(struct acpi_softc *sc, int state)
{
	int ret;

#ifdef MULTIPROCESSOR
	if (ncpus > 1)	/* cannot suspend MP yet */
		return (0);
#endif
	switch (state) {
	case ACPI_STATE_S0:
		return (0);
	case ACPI_STATE_S4:
		return (EOPNOTSUPP);
	case ACPI_STATE_S5:
		break;
	case ACPI_STATE_S1:
	case ACPI_STATE_S2:
	case ACPI_STATE_S3:
		if (sc->sc_sleeptype[state].slp_typa == -1 ||
		    sc->sc_sleeptype[state].slp_typb == -1)
			return (EOPNOTSUPP);
	}

	if ((ret = acpi_prepare_sleep_state(sc, state)) != 0)
		return (ret);

	if (state != ACPI_STATE_S1)
		ret = acpi_sleep_machdep(sc, state);
	else
		ret = acpi_enter_sleep_state(sc, state);

#ifndef SMALL_KERNEL
	if (state == ACPI_STATE_S3)
		acpi_resume(sc, state);
#endif /* !SMALL_KERNEL */
	return (ret);
}

int
acpi_enter_sleep_state(struct acpi_softc *sc, int state)
{
	uint16_t rega, regb;
	int retries;

	/* Clear WAK_STS bit */
	acpi_write_pmreg(sc, ACPIREG_PM1_STS, 1, ACPI_PM1_WAK_STS);

	/* Disable BM arbitration */
	acpi_write_pmreg(sc, ACPIREG_PM2_CNT, 1, ACPI_PM2_ARB_DIS);

	/* Write SLP_TYPx values */
	rega = acpi_read_pmreg(sc, ACPIREG_PM1A_CNT, 0);
	regb = acpi_read_pmreg(sc, ACPIREG_PM1B_CNT, 0);
	rega &= ~(ACPI_PM1_SLP_TYPX_MASK | ACPI_PM1_SLP_EN);
	regb &= ~(ACPI_PM1_SLP_TYPX_MASK | ACPI_PM1_SLP_EN);
	rega |= ACPI_PM1_SLP_TYPX(sc->sc_sleeptype[state].slp_typa);
	regb |= ACPI_PM1_SLP_TYPX(sc->sc_sleeptype[state].slp_typb);
	acpi_write_pmreg(sc, ACPIREG_PM1A_CNT, 0, rega);
	acpi_write_pmreg(sc, ACPIREG_PM1B_CNT, 0, regb);

	/* Set SLP_EN bit */
	rega |= ACPI_PM1_SLP_EN;
	regb |= ACPI_PM1_SLP_EN;

	/*
	 * Let the machdep code flush caches and do any other necessary
	 * tasks before going away.
	 */
	acpi_cpu_flush(sc, state);

	acpi_write_pmreg(sc, ACPIREG_PM1A_CNT, 0, rega);
	acpi_write_pmreg(sc, ACPIREG_PM1B_CNT, 0, regb);

	/* Loop on WAK_STS */
	for (retries = 1000; retries > 0; retries--) {
		rega = acpi_read_pmreg(sc, ACPIREG_PM1A_STS, 0);
		regb = acpi_read_pmreg(sc, ACPIREG_PM1B_STS, 0);
		if (rega & ACPI_PM1_WAK_STS ||
		    regb & ACPI_PM1_WAK_STS)
			break;
		DELAY(10);
	}

	return (-1);
}

#ifndef SMALL_KERNEL
void
acpi_resume(struct acpi_softc *sc, int state)
{
	struct aml_value env;

	memset(&env, 0, sizeof(env));
	env.type = AML_OBJTYPE_INTEGER;
	env.v_integer = sc->sc_state;

	if (sc->sc_bfs)
		if (aml_evalnode(sc, sc->sc_bfs, 1, &env, NULL) != 0) {
			dnprintf(10, "%s evaluating method _BFS failed.\n",
			    DEVNAME(sc));
		}

	if (sc->sc_wak)
		if (aml_evalnode(sc, sc->sc_wak, 1, &env, NULL) != 0) {
			dnprintf(10, "%s evaluating method _WAK failed.\n",
			    DEVNAME(sc));
		}

	/* Disable wake GPEs */
	acpi_susp_resume_gpewalk(sc, state, 0);

	config_suspend(TAILQ_FIRST(&alldevs), DVACT_RESUME);

	enable_intr();
	splx(acpi_saved_spl);

	sc->sc_state = ACPI_STATE_S0;
	if (sc->sc_tts) {
		env.v_integer = sc->sc_state;
		if (aml_evalnode(sc, sc->sc_tts, 1, &env, NULL) != 0) {
			dnprintf(10, "%s evaluating method _TTS failed.\n",
			    DEVNAME(sc));
		}
	}
}
#endif /* ! SMALL_KERNEL */

void
acpi_handle_suspend_failure(struct acpi_softc *sc)
{
	struct aml_value env;

	/* Undo a partial suspend. Devices will have already been resumed */
	enable_intr();
	splx(acpi_saved_spl);


	/* Tell ACPI to go back to S0 */
	memset(&env, 0, sizeof(env));
	env.type = AML_OBJTYPE_INTEGER;
	sc->sc_state = ACPI_STATE_S0;
	if (sc->sc_tts) {
		env.v_integer = sc->sc_state;
		if (aml_evalnode(sc, sc->sc_tts, 1, &env, NULL) != 0) {
			dnprintf(10, "%s evaluating method _TTS failed.\n",
			    DEVNAME(sc));
		}
	}
}

int
acpi_prepare_sleep_state(struct acpi_softc *sc, int state)
{
	struct aml_value env;

	if (sc == NULL || state == ACPI_STATE_S0)
		return(0);

	if (sc->sc_sleeptype[state].slp_typa == -1 ||
	    sc->sc_sleeptype[state].slp_typb == -1) {
		printf("%s: state S%d unavailable\n",
		    sc->sc_dev.dv_xname, state);
		return (ENXIO);
	}

	memset(&env, 0, sizeof(env));
	env.type = AML_OBJTYPE_INTEGER;
	env.v_integer = state;
	/* _TTS(state) */
	if (sc->sc_tts)
		if (aml_evalnode(sc, sc->sc_tts, 1, &env, NULL) != 0) {
			dnprintf(10, "%s evaluating method _TTS failed.\n",
			    DEVNAME(sc));
			return (ENXIO);
		}

	acpi_saved_spl = splhigh();
	disable_intr();
#ifndef SMALL_KERNEL
	if (state == ACPI_STATE_S3)
		if (config_suspend(TAILQ_FIRST(&alldevs), DVACT_SUSPEND) != 0) {
			acpi_handle_suspend_failure(sc);
			return (1);
		}
#endif /* ! SMALL_KERNEL */

	/* _PTS(state) */
	if (sc->sc_pts)
		if (aml_evalnode(sc, sc->sc_pts, 1, &env, NULL) != 0) {
			dnprintf(10, "%s evaluating method _PTS failed.\n",
			    DEVNAME(sc));
			return (ENXIO);
		}

	sc->sc_state = state;
	/* _GTS(state) */
	if (sc->sc_gts)
		if (aml_evalnode(sc, sc->sc_gts, 1, &env, NULL) != 0) {
			dnprintf(10, "%s evaluating method _GTS failed.\n",
			    DEVNAME(sc));
			return (ENXIO);
		}

	if (sc->sc_sst)
		aml_evalnode(sc, sc->sc_sst, 1, &env, NULL);

	/* Enable wake GPEs */
	acpi_susp_resume_gpewalk(sc, state, 1);

	return (0);
}



void
acpi_powerdown(void)
{
	/*
	 * In case acpi_prepare_sleep fails, we shouldn't try to enter
	 * the sleep state. It might cost us the battery.
	 */
	acpi_susp_resume_gpewalk(acpi_softc, ACPI_STATE_S5, 1);
	if (acpi_prepare_sleep_state(acpi_softc, ACPI_STATE_S5) == 0)
		acpi_enter_sleep_state(acpi_softc, ACPI_STATE_S5);
}


extern int aml_busy;

void
acpi_isr_thread(void *arg)
{
	struct acpi_thread *thread = arg;
	struct acpi_softc  *sc = thread->sc;
	u_int32_t gpe;

	/*
	 * If we have an interrupt handler, we can get notification
	 * when certain status bits changes in the ACPI registers,
	 * so let us enable some events we can forward to userland
	 */
	if (sc->sc_interrupt) {
		int16_t flag;

		dnprintf(1,"slpbtn:%c  pwrbtn:%c\n",
		    sc->sc_fadt->flags & FADT_SLP_BUTTON ? 'n' : 'y',
		    sc->sc_fadt->flags & FADT_PWR_BUTTON ? 'n' : 'y');
		dnprintf(10, "Enabling acpi interrupts...\n");
		sc->sc_wakeup = 1;

		/* Enable Sleep/Power buttons if they exist */
		flag = acpi_read_pmreg(sc, ACPIREG_PM1_EN, 0);
		if (!(sc->sc_fadt->flags & FADT_PWR_BUTTON)) {
			flag |= ACPI_PM1_PWRBTN_EN;
		}
		if (!(sc->sc_fadt->flags & FADT_SLP_BUTTON)) {
			flag |= ACPI_PM1_SLPBTN_EN;
		}
		acpi_write_pmreg(sc, ACPIREG_PM1_EN, 0, flag);

		/* Enable handled GPEs here */
		for (gpe = 0; gpe < sc->sc_lastgpe; gpe++) {
			if (sc->gpe_table[gpe].handler)
				acpi_enable_onegpe(sc, gpe, 1);
		}
	}

	while (thread->running) {
		dnprintf(10, "sleep... %d\n", sc->sc_wakeup);
		while (sc->sc_wakeup)
			tsleep(sc, PWAIT, "acpi_idle", 0);
		sc->sc_wakeup = 1;
		dnprintf(10, "wakeup..\n");
		if (aml_busy)
			continue;

		for (gpe = 0; gpe < sc->sc_lastgpe; gpe++) {
			struct gpe_block *pgpe = &sc->gpe_table[gpe];

			if (pgpe->active) {
				pgpe->active = 0;
				dnprintf(50, "softgpe: %.2x\n", gpe);
				if (pgpe->handler)
					pgpe->handler(sc, gpe, pgpe->arg);
			}
		}
		if (sc->sc_powerbtn) {
			sc->sc_powerbtn = 0;

			aml_notify_dev(ACPI_DEV_PBD, 0x80);

			acpi_evindex++;
			dnprintf(1,"power button pressed\n");
			KNOTE(sc->sc_note, ACPI_EVENT_COMPOSE(ACPI_EV_PWRBTN,
			    acpi_evindex));
		}
		if (sc->sc_sleepbtn) {
			sc->sc_sleepbtn = 0;

			aml_notify_dev(ACPI_DEV_SBD, 0x80);

			acpi_evindex++;
			dnprintf(1,"sleep button pressed\n");
			KNOTE(sc->sc_note, ACPI_EVENT_COMPOSE(ACPI_EV_SLPBTN,
			    acpi_evindex));
		}

		/* handle polling here to keep code non-concurrent*/
		if (sc->sc_poll) {
			sc->sc_poll = 0;
			acpi_poll_notify();
		}
	}
	free(thread, M_DEVBUF);

	kthread_exit(0);
}

void
acpi_create_thread(void *arg)
{
	struct acpi_softc *sc = arg;

	if (kthread_create(acpi_isr_thread, sc->sc_thread, NULL, DEVNAME(sc))
	    != 0) {
		printf("%s: unable to create isr thread, GPEs disabled\n",
		    DEVNAME(sc));
		return;
	}
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
acpi_matchhids(struct acpi_attach_args *aa, const char *hids[],
    const char *driver)
{
	int i;

	if (aa->aaa_dev == NULL || aa->aaa_node == NULL)
		return (0);
	for (i = 0; hids[i]; i++) {
		if (!strcmp(aa->aaa_dev, hids[i])) {
			dnprintf(5, "driver %s matches %s\n", driver, hids[i]);
			return (1);
		}
	}
	return (0);
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
	else if (!strcmp(dev, ACPI_DEV_ASUS))
		aaa.aaa_name = "acpiasus";
	else if (!strcmp(dev, ACPI_DEV_THINKPAD)) {
		aaa.aaa_name = "acpithinkpad";
		acpi_thinkpad_enabled = 1;
	} else if (!strcmp(dev, ACPI_DEV_ASUSAIBOOSTER))
		aaa.aaa_name = "aibs";

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
#endif /* SMALL_KERNEL */
