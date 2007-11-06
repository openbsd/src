/*	$OpenBSD: acpi.c,v 1.96 2007/11/06 22:12:34 deraadt Exp $	*/
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

#include <machine/conf.h>
#include <machine/cpufunc.h>
#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/acpidev.h>
#include <dev/acpi/dsdt.h>

#include <machine/apmvar.h>

#ifdef ACPI_DEBUG
int acpi_debug = 16;
#endif
int acpi_enabled = 0;
int acpi_poll_enabled = 0;
int acpi_hasprocfvs = 0;

#define ACPIEN_RETRIES 15

void	acpi_isr_thread(void *);
void	acpi_create_thread(void *);

int	acpi_match(struct device *, void *, void *);
void	acpi_attach(struct device *, struct device *, void *);
int	acpi_submatch(struct device *, void *, void *);
int	acpi_print(void *, const char *);

void	acpi_map_pmregs(struct acpi_softc *);

int	acpi_founddock(struct aml_node *, void *);
int	acpi_foundpss(struct aml_node *, void *);
int	acpi_foundhid(struct aml_node *, void *);
int	acpi_foundec(struct aml_node *, void *);
int	acpi_foundtmp(struct aml_node *, void *);
int	acpi_foundprt(struct aml_node *, void *);
int	acpi_foundprw(struct aml_node *, void *);
int	acpi_inidev(struct aml_node *, void *);

int	acpi_loadtables(struct acpi_softc *, struct acpi_rsdp *);
void	acpi_load_table(paddr_t, size_t, acpi_qhead_t *);
void	acpi_load_dsdt(paddr_t, struct acpi_q **);

void	acpi_init_states(struct acpi_softc *);
void	acpi_init_gpes(struct acpi_softc *);
void	acpi_init_pm(struct acpi_softc *);

void	acpi_filtdetach(struct knote *);
int	acpi_filtread(struct knote *, long);

void	acpi_enable_onegpe(struct acpi_softc *, int, int);
int	acpi_gpe_level(struct acpi_softc *, int, void *);
int	acpi_gpe_edge(struct acpi_softc *, int, void *);

#define	ACPI_LOCK(sc)
#define	ACPI_UNLOCK(sc)

/* XXX move this into dsdt softc at some point */
extern struct aml_node aml_root;

struct filterops acpiread_filtops = {
	1, NULL, acpi_filtdetach, acpi_filtread
};

struct cfattach acpi_ca = {
	sizeof(struct acpi_softc), acpi_match, acpi_attach
};

struct cfdriver acpi_cd = {
	NULL, "acpi", DV_DULL
};

struct acpi_softc *acpi_softc;
int acpi_s5, acpi_evindex;

#ifdef __i386__
#define acpi_bus_space_map	_bus_space_map
#define acpi_bus_space_unmap	_bus_space_unmap
#elif defined(__amd64__)
#define acpi_bus_space_map	_x86_memio_map
#define acpi_bus_space_unmap	_x86_memio_unmap
#else
#error ACPI supported on i386/amd64 only
#endif

#define pch(x) (((x)>=' ' && (x)<='z') ? (x) : ' ')

void
acpi_delay(struct acpi_softc *sc, int64_t uSecs)
{
	/* XXX this needs to become a tsleep later */
	delay(uSecs);
}

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
			printf("Unable to map iospace!\n");
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
	struct aml_value	res;
	int st = 0;

	/* Default value */
	st = STA_PRESENT|STA_ENABLED;
	st |= STA_SHOW_UI|STA_DEV_OK;
	st |= STA_BATTERY;

	/*
	 * Per the ACPI spec 6.5.1, only run _INI when device is there or
	 * when there is no _STA.  We terminate the tree walk (with return 1)
	 * early if necessary.
	 */

	/* Evaluate _STA to decide _INI fate and walk fate */
	if (!aml_evalname(sc, node, "_STA", 0, NULL, &res))
		st = (int)aml_val2int(&res);
	aml_freevalue(&res);

	/* Evaluate _INI if we are present */
	if (st & STA_PRESENT)
		aml_evalnode(sc, node, 0, NULL, NULL);

	/* If we are functioning, we walk/search our children */
	if(st & STA_DEV_OK)
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
	const char		*dev;
	struct acpi_attach_args	aaa;
	struct aml_value	res;
	int st = 0;

	dnprintf(10, "found prt entry: %s\n", node->parent->name);

	/* Default value */
	st = STA_PRESENT|STA_ENABLED;
	st |= STA_SHOW_UI|STA_DEV_OK;
	st |= STA_BATTERY;

	/* Evaluate _STA to decide _PRT fate and walk fate */
	if (!aml_evalname(sc, node, "_STA", 0, NULL, &res))
		st = (int)aml_val2int(&res);
	aml_freevalue(&res);

	if (st & STA_PRESENT) {
		memset(&aaa, 0, sizeof(aaa));
		aaa.aaa_iot = sc->sc_iot;
		aaa.aaa_memt = sc->sc_memt;
		aaa.aaa_node = node;
		aaa.aaa_dev = dev;
		aaa.aaa_name = "acpiprt";

		config_found(self, &aaa, acpi_print);
	}

	/* If we are functioning, we walk/search our children */
	if(st & STA_DEV_OK)
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
	struct acpi_attach_args	*aaa = aux;
	struct cfdata		*cf = match;

	/* sanity */
	if (strcmp(aaa->aaa_name, cf->cf_driver->cd_name))
		return (0);

	if (!acpi_probe(parent, cf, aaa))
		return (0);

	return (1);
}

int acpi_add_device(struct aml_node *node, void *arg);

int
acpi_add_device(struct aml_node *node, void *arg)
{
	struct device *self = arg;
	struct acpi_softc *sc = arg;
	struct acpi_attach_args aaa;

	memset(&aaa, 0, sizeof(aaa));
	aaa.aaa_node = node;
	aaa.aaa_dev = "";
	aaa.aaa_iot = sc->sc_iot;
	aaa.aaa_memt = sc->sc_memt;
	if (node == NULL || node->value == NULL)
		return 0;

	switch (node->value->type) {
	case AML_OBJTYPE_PROCESSOR:
		aaa.aaa_name = "acpicpu";
		break;
	case AML_OBJTYPE_THERMZONE:
		aaa.aaa_name = "acpitz";
 		break;
        default:
		return 0;
	}
	config_found(self, &aaa, acpi_print);
	return 0;
}

void
acpi_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct acpi_softc *sc = (struct acpi_softc *)self;
	struct acpi_mem_map handle;
	struct acpi_rsdp *rsdp;
	struct acpi_q *entry;
	struct acpi_wakeq *wentry;
	struct acpi_dsdt *p_dsdt;
#ifndef SMALL_KERNEL
	struct device *dev;
	struct acpi_ac *ac;
	struct acpi_bat *bat;
	paddr_t facspa;
#endif
	sc->sc_iot = aaa->aaa_iot;
	sc->sc_memt = aaa->aaa_memt;


	if (acpi_map(aaa->aaa_pbase, sizeof(struct acpi_rsdp), &handle)) {
		printf(": can't map memory\n");
		return;
	}

	rsdp = (struct acpi_rsdp *)handle.va;
	printf(": rev %d", (int)rsdp->rsdp_revision);

	SIMPLEQ_INIT(&sc->sc_tables);
	SIMPLEQ_INIT(&sc->sc_wakedevs);

	sc->sc_fadt = NULL;
	sc->sc_facs = NULL;
	sc->sc_powerbtn = 0;
	sc->sc_sleepbtn = 0;

	sc->sc_note = malloc(sizeof(struct klist), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->sc_note == NULL) {
		printf(": can't allocate memory\n");
		acpi_unmap(&handle);
		return;
	}

	if (acpi_loadtables(sc, rsdp)) {
		printf(": can't load tables\n");
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
		printf(": no FADT\n");
		return;
	}

#ifdef ACPI_ENABLE
	/*
	 * Check if we are able to enable ACPI control
	 */
	if (!sc->sc_fadt->smi_cmd ||
	    (!sc->sc_fadt->acpi_enable && !sc->sc_fadt->acpi_disable)) {
		printf(": ACPI control unavailable\n");
		return;
	}
#endif

	/* Create opcode hashtable */
	aml_hashopcodes();

	acpi_enabled=1;

	/* Create Default AML objects */
	aml_create_defaultobjects();

	/*
	 * Load the DSDT from the FADT pointer -- use the
	 * extended (64-bit) pointer if it exists
	 */
	if (sc->sc_fadt->hdr_revision < 3 || sc->sc_fadt->x_dsdt == 0)
		acpi_load_dsdt(sc->sc_fadt->dsdt, &entry);
	else
		acpi_load_dsdt(sc->sc_fadt->x_dsdt, &entry);

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

	/* Walk AML Tree */
	/* aml_walkroot(); */

#ifndef SMALL_KERNEL
	/* Find available sleeping states */
	acpi_init_states(sc);

	/* Find available sleep/resume related methods. */
	acpi_init_pm(sc);

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

	/* Map Power Management registers */
	acpi_map_pmregs(sc);

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
#ifdef ACPI_ENABLE
	int idx;

	acpi_write_pmreg(sc, ACPIREG_SMICMD, 0, sc->sc_fadt->acpi_enable);
	idx = 0;
	do {
		if (idx++ > ACPIEN_RETRIES) {
			printf(": can't enable ACPI\n");
			return;
		}
	} while (!(acpi_read_pmreg(sc, ACPIREG_PM1_CNT, 0) & ACPI_PM1_SCI_EN));
#endif

	printf("\n");

	printf("%s: tables ", DEVNAME(sc));
	SIMPLEQ_FOREACH(entry, &sc->sc_tables, q_next) {
		printf("%.4s ", entry->q_table);
	}
	printf("\n");

	/* Display wakeup devices and lowest S-state */
	printf("%s: wakeup devices ", DEVNAME(sc));
	SIMPLEQ_FOREACH(wentry, &sc->sc_wakedevs, q_next) {
		printf("%.4s(S%d) ", 
		       wentry->q_node->name,
		       wentry->q_state);
	}
	printf("\n");


#ifndef SMALL_KERNEL
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
	aml_find_node(aml_root.child, "_INI", acpi_inidev, sc);

	/* attach pci interrupt routing tables */
	aml_find_node(aml_root.child, "_PRT", acpi_foundprt, sc);

#ifndef SMALL_KERNEL
	 /* XXX EC needs to be attached first on some systems */
	aml_find_node(aml_root.child, "_HID", acpi_foundec, sc);

	aml_walknodes(&aml_root, AML_WALK_PRE, acpi_add_device, sc);

	/* attach battery, power supply and button devices */
	aml_find_node(aml_root.child, "_HID", acpi_foundhid, sc);

	/* attach docks */
	aml_find_node(aml_root.child, "_DCK", acpi_founddock, sc);

	/* create list of devices we want to query when APM come in */
	SLIST_INIT(&sc->sc_ac);
	SLIST_INIT(&sc->sc_bat);
	TAILQ_FOREACH(dev, &alldevs, dv_list) {
		if (!strncmp(dev->dv_xname, "acpiac", strlen("acpiac"))) {
			ac = malloc(sizeof(*ac), M_DEVBUF, M_WAITOK | M_ZERO);
			ac->aac_softc = (struct acpiac_softc *)dev;
			SLIST_INSERT_HEAD(&sc->sc_ac, ac, aac_link);
		}
		if (!strncmp(dev->dv_xname, "acpibat", strlen("acpibat"))) {
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
	/* XXX ACPIVERBOSE should be replaced with dnprintf */
	struct acpi_attach_args *aa = aux;
#ifdef ACPIVERBOSE
	struct acpi_table_header *hdr =
		(struct acpi_table_header *)aa->aaa_table;
#endif

	if (pnp) {
		if (aa->aaa_name)
			printf("%s at %s", aa->aaa_name, pnp);
#ifdef ACPIVERBOSE
		else
			printf("acpi device at %s from", pnp);
#else
		else
			return (QUIET);
#endif
	}
#ifdef ACPIVERBOSE
	if (hdr)
		printf(" table %c%c%c%c",
		    hdr->signature[0], hdr->signature[1],
		    hdr->signature[2], hdr->signature[3]);
#endif

	return (UNCONF);
}

int
acpi_loadtables(struct acpi_softc *sc, struct acpi_rsdp *rsdp)
{
	struct acpi_mem_map hrsdt, handle;
	struct acpi_table_header *hdr;
	int i, ntables;
	size_t len;

	if (rsdp->rsdp_revision == 2) {
		struct acpi_xsdt *xsdt;

		if (acpi_map(rsdp->rsdp_xsdt, sizeof(*hdr), &handle)) {
			printf("couldn't map rsdt\n");
			return (ENOMEM);
		}

		hdr = (struct acpi_table_header *)handle.va;
		len = hdr->length;
		acpi_unmap(&handle);
		hdr = NULL;

		acpi_map(rsdp->rsdp_xsdt, len, &hrsdt);
		xsdt = (struct acpi_xsdt *)hrsdt.va;

		ntables = (len - sizeof(struct acpi_table_header)) /
		    sizeof(xsdt->table_offsets[0]);

		for (i = 0; i < ntables; i++) {
			acpi_map(xsdt->table_offsets[i], sizeof(*hdr), &handle);
			hdr = (struct acpi_table_header *)handle.va;
			acpi_load_table(xsdt->table_offsets[i], hdr->length,
			    &sc->sc_tables);
			acpi_unmap(&handle);
		}
		acpi_unmap(&hrsdt);
	} else {
		struct acpi_rsdt *rsdt;

		if (acpi_map(rsdp->rsdp_rsdt, sizeof(*hdr), &handle)) {
			printf("couldn't map rsdt\n");
			return (ENOMEM);
		}

		hdr = (struct acpi_table_header *)handle.va;
		len = hdr->length;
		acpi_unmap(&handle);
		hdr = NULL;

		acpi_map(rsdp->rsdp_rsdt, len, &hrsdt);
		rsdt = (struct acpi_rsdt *)hrsdt.va;

		ntables = (len - sizeof(struct acpi_table_header)) /
		    sizeof(rsdt->table_offsets[0]);

		for (i = 0; i < ntables; i++) {
			acpi_map(rsdt->table_offsets[i], sizeof(*hdr), &handle);
			hdr = (struct acpi_table_header *)handle.va;
			acpi_load_table(rsdt->table_offsets[i], hdr->length,
			    &sc->sc_tables);
			acpi_unmap(&handle);
		}
		acpi_unmap(&hrsdt);
	}

	return (0);
}

void
acpi_load_table(paddr_t pa, size_t len, acpi_qhead_t *queue)
{
	struct acpi_mem_map handle;
	struct acpi_q *entry;

	entry = malloc(len + sizeof(struct acpi_q), M_DEVBUF, M_NOWAIT);

	if (entry != NULL) {
		if (acpi_map(pa, len, &handle)) {
			free(entry, M_DEVBUF);
			return;
		}
		memcpy(entry->q_data, handle.va, len);
		entry->q_table = entry->q_data;
		acpi_unmap(&handle);
		SIMPLEQ_INSERT_TAIL(queue, entry, q_next);
	}
}

void
acpi_load_dsdt(paddr_t pa, struct acpi_q **dsdt)
{
	struct acpi_mem_map handle;
	struct acpi_table_header *hdr;
	size_t len;

	if (acpi_map(pa, sizeof(*hdr), &handle))
		return;
	hdr = (struct acpi_table_header *)handle.va;
	len = hdr->length;
	acpi_unmap(&handle);

	*dsdt = malloc(len + sizeof(struct acpi_q), M_DEVBUF, M_NOWAIT);

	if (*dsdt != NULL) {
		if (acpi_map(pa, len, &handle)) {
			free(*dsdt, M_DEVBUF);
			*dsdt = NULL;
			return;
		}
		memcpy((*dsdt)->q_data, handle.va, len);
		(*dsdt)->q_table = (*dsdt)->q_data;
		acpi_unmap(&handle);
	}
}

int
acpiopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct acpi_softc *sc;
	int error = 0;

	if (!acpi_cd.cd_ndevs || minor(dev) != 0 ||
	    !(sc = acpi_cd.cd_devs[minor(dev)]))
		return (ENXIO);

	return (error);
}

int
acpiclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct acpi_softc *sc;

	if (!acpi_cd.cd_ndevs || minor(dev) != 0 ||
	    !(sc = acpi_cd.cd_devs[minor(dev)]))
		return (ENXIO);

	return (0);
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

	if (!acpi_cd.cd_ndevs || minor(dev) != 0 ||
	    !(sc = acpi_cd.cd_devs[minor(dev)]))
		return (ENXIO);

	ACPI_LOCK(sc);
	/* fake APM */
	switch (cmd) {
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
	struct acpi_softc *sc = kn->kn_hook;

	ACPI_LOCK(sc);
	SLIST_REMOVE(sc->sc_note, kn, knote, kn_selnext);
	ACPI_UNLOCK(sc);
}

int
acpi_filtread(struct knote *kn, long hint)
{
	/* XXX weird kqueue_scan() semantics */
	if (hint & !kn->kn_data)
		kn->kn_data = hint;

	return (1);
}

int
acpikqfilter(dev_t dev, struct knote *kn)
{
	struct acpi_softc *sc;

	if (!acpi_cd.cd_ndevs || minor(dev) != 0 ||
	    !(sc = acpi_cd.cd_devs[minor(dev)]))
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

#ifdef ACPI_ENABLE
int
acpi_interrupt(void *arg)
{
	struct acpi_softc *sc = (struct acpi_softc *)arg;
	u_int32_t processed, sts, en, idx, jdx;

	processed = 0;

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
#endif /* ACPI_ENABLE */

/* move all stuff that doesn't go on the boot media in here */
#ifndef SMALL_KERNEL

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
	if (gpe >= sc->sc_lastgpe || handler == NULL)
		return -EINVAL;

	if (sc->gpe_table[gpe].handler != NULL) {
		dnprintf(10, "error: GPE %.2x already enabled!\n", gpe);
		return -EBUSY;
	}

	dnprintf(50, "Adding GPE handler %.2x (%s)\n", gpe, label);
	sc->gpe_table[gpe].handler = handler;
	sc->gpe_table[gpe].arg = arg;

	/* Defer enabling GPEs */

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

	wq = (struct acpi_wakeq *)malloc(sizeof(struct acpi_wakeq), M_DEVBUF, M_NOWAIT);
	if (wq == NULL) {
		return 0;
	}
	memset(wq, 0, sizeof(struct acpi_wakeq));

	wq->q_wakepkg = (struct aml_value *)malloc(sizeof(struct aml_value), M_DEVBUF, M_NOWAIT);
	if (wq->q_wakepkg == NULL) {
		free(wq, M_DEVBUF);
		return 0;
	}
	memset(wq->q_wakepkg, 0, sizeof(struct aml_value));
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
	aml_find_node(aml_root.child, "_PRW", acpi_foundprw, sc);
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
		if (aml_evalname(sc, aml_root.child, name, 0, NULL, &res) == 0) {
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
	sc->sc_tts = aml_searchname(aml_root.child, "_TTS");
	sc->sc_pts = aml_searchname(aml_root.child, "_PTS");
	sc->sc_wak = aml_searchname(aml_root.child, "_WAK");
	sc->sc_bfs = aml_searchname(aml_root.child, "_BFS");
	sc->sc_gts = aml_searchname(aml_root.child, "_GTS");
}

void
acpi_enter_sleep_state(struct acpi_softc *sc, int state)
{
#ifdef ACPI_ENABLE
	struct aml_value env;
	u_int16_t rega, regb;
	int retries;

	if (state == ACPI_STATE_S0)
		return;
	if (sc->sc_sleeptype[state].slp_typa == -1 ||
	    sc->sc_sleeptype[state].slp_typb == -1) {
		printf("%s: state S%d unavailable\n",
		    sc->sc_dev.dv_xname, state);
		return;
	}

	env.type = AML_OBJTYPE_INTEGER;
	env.v_integer = state;
	/* _TTS(state) */
	if (sc->sc_tts) {
		if (aml_evalnode(sc, sc->sc_tts, 1, &env, NULL) != 0) {
			dnprintf(10, "%s evaluating method _TTS failed.\n",
			    DEVNAME(sc));
			return;
		}
	}
	switch (state) {
	case ACPI_STATE_S1:
	case ACPI_STATE_S2:
		resettodr();
		dopowerhooks(PWR_SUSPEND);
		break;
	case ACPI_STATE_S3:
		resettodr();
		dopowerhooks(PWR_STANDBY);
		break;
	}
	/* _PTS(state) */
	if (sc->sc_pts) {
		if (aml_evalnode(sc, sc->sc_pts, 1, &env, NULL) != 0) {
			dnprintf(10, "%s evaluating method _PTS failed.\n",
			    DEVNAME(sc));
			return;
		}
	}
	sc->sc_state = state;
	/* _GTS(state) */
	if (sc->sc_gts) {
		if (aml_evalnode(sc, sc->sc_gts, 1, &env, NULL) != 0) {
			dnprintf(10, "%s evaluating method _GTS failed.\n",
			    DEVNAME(sc));
			return;
		}
	}
	disable_intr();

	/* Clear WAK_STS bit */
	acpi_write_pmreg(sc, ACPIREG_PM1_STS, 0, ACPI_PM1_WAK_STS);

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

	enable_intr();
#endif
}

void
acpi_resume(struct acpi_softc *sc)
{
	struct aml_value env;

	env.type = AML_OBJTYPE_INTEGER;
	env.v_integer = sc->sc_state;

	if (sc->sc_bfs) {
		if (aml_evalnode(sc, sc->sc_pts, 1, &env, NULL) != 0) {
			dnprintf(10, "%s evaluating method _BFS failed.\n",
			    DEVNAME(sc));
		}
	}
	dopowerhooks(PWR_RESUME);
	inittodr(0);
	if (sc->sc_wak) {
		if (aml_evalnode(sc, sc->sc_wak, 1, &env, NULL) != 0) {
			dnprintf(10, "%s evaluating method _WAK failed.\n",
			    DEVNAME(sc));
		}
	}
	sc->sc_state = ACPI_STATE_S0;
	if (sc->sc_tts) {
		env.v_integer = sc->sc_state;
		if (aml_evalnode(sc, sc->sc_wak, 1, &env, NULL) != 0) {
			dnprintf(10, "%s evaluating method _TTS failed.\n",
			    DEVNAME(sc));
		}
	}
}

void
acpi_powerdown(void)
{
	acpi_enter_sleep_state(acpi_softc, ACPI_STATE_S5);
}

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
acpi_map_address(struct acpi_softc *sc, struct acpi_gas *gas,  bus_addr_t base, bus_size_t size, 
		 bus_space_handle_t *pioh, bus_space_tag_t *piot)
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
	const char		*dev;
	struct acpi_attach_args	aaa;

	dnprintf(10, "found dock entry: %s\n", node->parent->name);

	memset(&aaa, 0, sizeof(aaa));
	aaa.aaa_iot = sc->sc_iot;
	aaa.aaa_memt = sc->sc_memt;
	aaa.aaa_node = node->parent;
	aaa.aaa_dev = dev;
	aaa.aaa_name = "acpidock";

	config_found(self, &aaa, acpi_print);

	return 0;
}
#endif /* SMALL_KERNEL */
