/*	$OpenBSD: acpi.c,v 1.26 2006/02/06 23:12:09 brad Exp $	*/
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

#include <machine/conf.h>
#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

#ifdef ACPI_DEBUG
int acpi_debug = 60;
#endif

#define ACPIEN_RETRIES 15

int	acpi_match(struct device *, void *, void *);
void	acpi_attach(struct device *, struct device *, void *);
int	acpi_submatch(struct device *, void *, void *);
int	acpi_print(void *, const char *);

void	acpi_map_pmregs(struct acpi_softc *);
void	acpi_unmap_pmregs(struct acpi_softc *);
int	acpi_read_pmreg(struct acpi_softc *, int);
void	acpi_write_pmreg(struct acpi_softc *, int, int);

void	acpi_gpe(struct aml_node *, void *);
void	acpi_foundhid(struct aml_node *, void *);

int	acpi_loadtables(struct acpi_softc *, struct acpi_rsdp *);
void	acpi_load_table(paddr_t, size_t, acpi_qhead_t *);
void	acpi_load_dsdt(paddr_t, struct acpi_q **);

void	acpi_softintr(void *);
void	acpi_init_states(struct acpi_softc *);

void	acpi_filtdetach(struct knote *);
int	acpi_filtread(struct knote *, long);

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
int acpi_s5, acpi_evindex, icount;

#ifdef __i386__
#define acpi_bus_space_map     _bus_space_map
#define acpi_bus_space_unmap   _bus_space_unmap
#elif defined(__amd64__)
#define acpi_bus_space_map     _x86_memio_map
#define acpi_bus_space_unmap   _x86_memio_unmap
#else
#error ACPI supported on i386/amd64 only
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

	dnprintf(10, "gasio: %x %llx %x %x %p\n", iospace, address, access_size, len, buffer);

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
		for (reg=0; reg < len; reg += access_size) {
			if (iodir == ACPI_IOREAD) {
				switch (access_size) {
				case 1:
					*(uint8_t *)(pb+reg) = bus_space_read_1(sc->sc_iot, ioh, reg);
					break;
				case 2:
					*(uint16_t *)(pb+reg) = bus_space_read_2(sc->sc_iot, ioh, reg);
					break;
				case 4:
					*(uint32_t *)(pb+reg) = bus_space_read_4(sc->sc_iot, ioh, reg);
					break;
				}
			}
			else {
				switch (access_size) {
				case 1:
					bus_space_write_1(sc->sc_iot, ioh, reg, *(uint8_t *)(pb+reg));
					break;
				case 2:
					bus_space_write_2(sc->sc_iot, ioh, reg, *(uint16_t *)(pb+reg));
					break;
				case 4:
					bus_space_write_4(sc->sc_iot, ioh, reg, *(uint32_t *)(pb+reg));
					break;
				}
			}
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
				   ACPI_PCI_BUS(address),
				   ACPI_PCI_DEV(address),
				   ACPI_PCI_FN(address));

		/* XXX: This is ugly.. read-modify-write does a byte at a time */
		reg = ACPI_PCI_REG(address);
		for (idx=reg; idx<reg+len; idx++) {
			ival = pci_conf_read(pc, tag, idx & ~0x3);
			if (iodir == ACPI_IOREAD) {
				switch (idx & 0x3) {
				case 0:
					*pb = ival;
					break;
				case 1:
					*pb = (ival >> 8);
					break;
				case 2:
					*pb = (ival >> 16);
					break;
				case 3:
					*pb = (ival >> 24);
					break;
				}
			}
			else {
				sval = *pb;
				switch (idx & 0x3) {
				case 0:
					ival &= ~0xFF;
					ival |= sval;
					break;
				case 1:
					ival &= ~0xFF00;
					ival |= (sval << 8L);
					break;
				case 2:
					ival &= ~0xFF0000;
					ival |= (sval << 16L);
					break;
				case 3:
					ival &= ~0xFF000000L;
					ival |= (sval << 24L);
					break;
				}
				pci_conf_write(pc, tag, idx & ~0x3, ival);
			}
			pb++;
		}
		break;
	}
	return (0);
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

void
acpi_unmap_pmregs(struct acpi_softc *sc)
{
	int idx;

	for (idx = 0; idx < ACPIREG_MAXREG; idx++) {
		if (sc->sc_pmregs[idx].size) {
			bus_space_unmap(sc->sc_iot, sc->sc_pmregs[idx].ioh,
					sc->sc_pmregs[idx].size);
		}
	}
}

/* Read from power management register */
int
acpi_read_pmreg(struct acpi_softc *sc, int reg)
{
	bus_space_handle_t ioh;
	bus_size_t size;
	int regval;

	/* Special cases: 1A/1B blocks can be OR'ed together */
	if (reg == ACPIREG_PM1_EN) {
		return (acpi_read_pmreg(sc, ACPIREG_PM1A_EN) |
			acpi_read_pmreg(sc, ACPIREG_PM1B_EN));
	}
	else if (reg == ACPIREG_PM1_STS) {
		return (acpi_read_pmreg(sc, ACPIREG_PM1A_STS) |
			acpi_read_pmreg(sc, ACPIREG_PM1B_STS));
	}
	else if (reg == ACPIREG_PM1_CNT) {
		return (acpi_read_pmreg(sc, ACPIREG_PM1A_CNT) |
			acpi_read_pmreg(sc, ACPIREG_PM1B_CNT));
	}

	if (reg >= ACPIREG_MAXREG || sc->sc_pmregs[reg].size == 0)
		return (0);

	regval = 0;
	ioh = sc->sc_pmregs[reg].ioh;
	size = sc->sc_pmregs[reg].size;
	if (size > 4)
		size = 4;

	switch (size) {
	case 1:
		regval = bus_space_read_1(sc->sc_iot, ioh, 0);
		break;
	case 2:
		regval = bus_space_read_2(sc->sc_iot, ioh, 0);
		break;
	case 4:
		regval = bus_space_read_4(sc->sc_iot, ioh, 0);
		break;
	}

	dnprintf(30, "acpi_readpm: %s = %.4x %x\n",
	       sc->sc_pmregs[reg].name,
	       sc->sc_pmregs[reg].addr, regval);
	return (regval);
}

/* Write to power management register */
void
acpi_write_pmreg(struct acpi_softc *sc, int reg, int regval)
{
	bus_space_handle_t ioh;
	bus_size_t size;

	/* Special cases: 1A/1B blocks can be written with same value */
	if (reg == ACPIREG_PM1_EN) {
		acpi_write_pmreg(sc, ACPIREG_PM1A_EN, regval);
		acpi_write_pmreg(sc, ACPIREG_PM1B_EN, regval);
	}
	else if (reg == ACPIREG_PM1_STS) {
		acpi_write_pmreg(sc, ACPIREG_PM1A_STS, regval);
		acpi_write_pmreg(sc, ACPIREG_PM1B_STS, regval);
	}
	else if (reg == ACPIREG_PM1_CNT) {
		acpi_write_pmreg(sc, ACPIREG_PM1A_CNT, regval);
		acpi_write_pmreg(sc, ACPIREG_PM1B_CNT, regval);
	}

	/* All special case return here */
	if (reg >= ACPIREG_MAXREG)
		return;

	ioh = sc->sc_pmregs[reg].ioh;
	size = sc->sc_pmregs[reg].size;
	if (size > 4)
		size = 4;
	switch (size) {
	case 1:
		bus_space_write_1(sc->sc_iot, ioh, 0, regval);
		break;
	case 2:
		bus_space_write_2(sc->sc_iot, ioh, 0, regval);
		break;
	case 4:
		bus_space_write_4(sc->sc_iot, ioh, 0, regval);
		break;
	}

	dnprintf(30, "acpi_writepm: %s = %.4x %x\n",
		 sc->sc_pmregs[reg].name,
		 sc->sc_pmregs[reg].addr,
		 regval);
}

void
acpi_gpe(struct aml_node *node, void *arg)
{
	struct aml_node *child;
	struct acpi_softc *sc = arg;
	uint32_t flag;

	flag = acpi_read_pmreg(sc, ACPIREG_GPE0_EN);
	for (child = node->child; child; child = child->sibling) {
		dnprintf(30, "gpe: %s\n", child->name);
	}
	flag = -1;
	flag &= ~(1L << 0x1C);
}

void
acpi_foundhid(struct aml_node *node, void *arg)
{
	struct acpi_softc	*sc = (struct acpi_softc *)arg;
	struct device		*self = (struct device *)arg;
	const char		*dev;
	struct aml_value	res;

	dnprintf(10, "found hid device: %s ", node->parent->name);
	aml_eval_object(sc, node, &res, 0, NULL);

	switch (res.type) {
	case AML_OBJTYPE_STRING:
		dev = res.v_string;
		break;
	case AML_OBJTYPE_INTEGER:
		dev = aml_eisaid(res.v_integer);
		break;
	default:
		dev = "unknown";
		break;
	}
	dnprintf(10, "  device: %s\n", dev);

	if (!strcmp(dev, ACPI_DEV_AC)) {
		struct acpi_attach_args aaa;

		memset(&aaa, 0, sizeof(aaa));
		aaa.aaa_name = "acpiac";
		aaa.aaa_iot = sc->sc_iot;
		aaa.aaa_memt = sc->sc_memt;
		aaa.aaa_node = node->parent;
		config_found(self, &aaa, acpi_print);
	} else if (!strcmp(dev, ACPI_DEV_CMB)) {
		struct acpi_attach_args aaa;

		memset(&aaa, 0, sizeof(aaa));
		aaa.aaa_name = "acpibat";
		aaa.aaa_iot = sc->sc_iot;
		aaa.aaa_memt = sc->sc_memt;
		aaa.aaa_node = node->parent;
		config_found(self, &aaa, acpi_print);
	}
}

int
acpi_match(struct device *parent, void *match, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct cfdata *cf = match;

	/* sanity */
	if (strcmp(aaa->aaa_name, cf->cf_driver->cd_name))
		return (0);

	if (!acpi_probe(parent, cf, aaa))
		return (0);

	return (1);
}

void
acpi_attach(struct device *parent, struct device *self, void *aux)
{
	struct acpi_attach_args *aaa = aux;
	struct acpi_softc *sc = (struct acpi_softc *)self;
	struct acpi_mem_map handle;
	struct acpi_rsdp *rsdp;
	struct acpi_q *entry;
	struct acpi_dsdt *p_dsdt;
	paddr_t facspa;

	sc->sc_iot = aaa->aaa_iot;
	sc->sc_memt = aaa->aaa_memt;

	if (acpi_map(aaa->aaa_pbase, sizeof(struct acpi_rsdp), &handle)) {
		printf(": can't map memory\n");
		return;
	}

	rsdp = (struct acpi_rsdp *)handle.va;
	printf(": rev %d", (int)rsdp->rsdp_revision);

	SIMPLEQ_INIT(&sc->sc_tables);

	sc->sc_fadt = NULL;
	sc->sc_facs = NULL;
	sc->sc_powerbtn = 0;
	sc->sc_sleepbtn = 0;

	sc->sc_note = malloc(sizeof(struct klist), M_DEVBUF, M_NOWAIT);
	memset(sc->sc_note, 0, sizeof(struct klist));

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

	/* Find available sleeping states */
	acpi_init_states(sc);

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

	acpi_write_pmreg(sc, ACPIREG_SMICMD, sc->sc_fadt->acpi_enable);
	idx = 0;
	do {
		if (idx++ > ACPIEN_RETRIES) {
			printf(": can't enable ACPI\n");
			return;
		}
	} while (!(acpi_read_pmreg(sc, ACPIREG_PM1_CNT) & ACPI_PM1_SCI_EN));
#endif

#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
	sc->sc_softih = softintr_establish(IPL_TTY, acpi_softintr, sc);
#else
	timeout_set(&sc->sc_timeout, acpi_softintr, sc);
#endif
	acpi_attach_machdep(sc);

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

		/* Enable Sleep/Power buttons if they exist */
		flag = acpi_read_pmreg(sc, ACPIREG_PM1_EN);
		if (!(sc->sc_fadt->flags & FADT_PWR_BUTTON)) {
			flag |= ACPI_PM1_PWRBTN_EN;
		}
		if (!(sc->sc_fadt->flags & FADT_SLP_BUTTON)) {
			flag |= ACPI_PM1_SLPBTN_EN;
		}
		acpi_write_pmreg(sc, ACPIREG_PM1_EN, flag);

#if 0
		flag = acpi_read_pmreg(sc, ACPIREG_GPE0_STS);
		acpi_write_pmreg(sc, ACPIREG_GPE0_STS, flag);
		acpi_write_pmreg(sc, ACPIREG_GPE0_EN, 0);
		acpi_write_pmreg(sc, ACPIREG_GPE0_EN, (1L << 0x1D));
#endif
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

	/* attach devices found in dsdt */
	aml_find_node(aml_root.child, "_HID", acpi_foundhid, sc);
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
			acpi_map(xsdt->table_offsets[i], sizeof(*hdr),
			    &handle);
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
			acpi_map(rsdt->table_offsets[i], sizeof(*hdr),
			    &handle);
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
acpi_interrupt(void *arg)
{
	struct acpi_softc *sc = (struct acpi_softc *)arg;
	u_int32_t processed, sts, en;

	processed = 0;

	sts = acpi_read_pmreg(sc, ACPIREG_GPE0_STS);
	en  = acpi_read_pmreg(sc, ACPIREG_GPE0_EN);
	if (sts & en) {
		dnprintf(10, "GPE interrupt: %.8x %.8x %.8x\n",
		    sts, en, sts & en);
		acpi_write_pmreg(sc, ACPIREG_GPE0_EN, en & ~sts);
		acpi_write_pmreg(sc, ACPIREG_GPE0_STS,en);
		acpi_write_pmreg(sc, ACPIREG_GPE0_EN, en);
		processed = 1;
		for (en = 0; en < icount; en++) {
			icount = (icount << 1) | 1;
		}
		icount++;
	}

	sts = acpi_read_pmreg(sc, ACPIREG_PM1_STS);
	en  = acpi_read_pmreg(sc, ACPIREG_PM1_EN);
	if (sts & en) {
		dnprintf(10,"GEN interrupt: %.4x\n", sts & en);
		acpi_write_pmreg(sc, ACPIREG_PM1_EN, en & ~sts);
		acpi_write_pmreg(sc, ACPIREG_PM1_STS,en);
		acpi_write_pmreg(sc, ACPIREG_PM1_EN, en);
		if (sts & ACPI_PM1_PWRBTN_STS)
			sc->sc_powerbtn = 1;
		if (sts & ACPI_PM1_SLPBTN_STS)
			sc->sc_sleepbtn = 1;
		processed = 1;
	}
	if (processed) {
#ifdef __HAVE_GENERIC_SOFT_INTERRUPTS
		softintr_schedule(sc->sc_softih);
#else
		if (!timeout_pending(&sc->sc_timeout))
			timeout_add(&sc->sc_timeout, 0);
#endif
	}

	return (processed);
}

void
acpi_softintr(void *arg)
{
	struct acpi_softc *sc = arg;

	if (sc->sc_powerbtn) {
		sc->sc_powerbtn = 0;
		acpi_evindex++;
		dnprintf(1,"power button pressed\n");
		KNOTE(sc->sc_note, ACPI_EVENT_COMPOSE(ACPI_EV_PWRBTN,
						      acpi_evindex));

		/* power down */
		acpi_s5 = 1;
		psignal(initproc, SIGUSR1);
	}
	if (sc->sc_sleepbtn) {
		sc->sc_sleepbtn = 0;
		acpi_evindex++;
		dnprintf(1,"sleep button pressed\n");
		KNOTE(sc->sc_note, ACPI_EVENT_COMPOSE(ACPI_EV_SLPBTN,
						      acpi_evindex));
	}
}

void
acpi_init_states(struct acpi_softc *sc)
{
	struct aml_value res, env;
	char name[8];
	int i;

	for (i = ACPI_STATE_S0; i <= ACPI_STATE_S5; i++) {
		snprintf(name, sizeof(name), "_S%d_", i);
		sc->sc_sleeptype[i].slp_typa = -1;
		sc->sc_sleeptype[i].slp_typb = -1;
		if (aml_eval_name(sc, aml_root.child, name, &res, &env))
			continue;
		if (res.type == AML_OBJTYPE_PACKAGE) {
			sc->sc_sleeptype[i].slp_typa = aml_intval(res.v_package[0]);
			sc->sc_sleeptype[i].slp_typb = aml_intval(res.v_package[1]);
		}
	}
}

void
acpi_enter_sleep_state(struct acpi_softc *sc, int state)
{
#ifdef ACPI_ENABLE
	u_int16_t flag;

	flag = acpi_read_pmreg(sc, ACPIREG_PM1_CNT);
	/* XXX This is sick and wrong and illegal! */
	acpi_write_pmreg(sc, ACPIREG_PM1_CNT,  flag |= (state << 10));
	acpi_write_pmreg(sc, ACPIREG_PM1_CNT,  flag |= ACPI_PM1_SLP_EN);
#endif
}

void
acpi_powerdown(void)
{
	acpi_enter_sleep_state(acpi_softc, ACPI_STATE_S5);
}

int
acpiopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct acpi_softc *sc;
	int error = 0;

	if (!acpi_cd.cd_ndevs || minor(dev) != 0 ||
	    !(sc = acpi_cd.cd_devs[minor(dev)]))
		return (ENXIO);

	if (!(flag & FREAD) || (flag & FWRITE))
		error = EINVAL;

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
	struct acpi_softc *sc;
	int error = 0;

	if (!acpi_cd.cd_ndevs || minor(dev) != 0 ||
	    !(sc = acpi_cd.cd_devs[minor(dev)]))
		return (ENXIO);

	ACPI_LOCK(sc);
	switch (cmd) {
	case ACPI_IOC_SETSLEEPSTATE:
		if (suser(p, 0) != 0)
			error = EPERM;
		else {
			acpi_enter_sleep_state(sc, *(int *)data);
		}
		break;

	case ACPI_IOC_GETFACS:
		if (suser(p, 0) != 0)
			error = EPERM;
		else {
			struct acpi_facs *facs = (struct acpi_facs *)data;

			bcopy(sc->sc_facs, facs, sc->sc_facs->length);
		}
		break;

	case ACPI_IOC_GETTABLE:
		if (suser(p, 0) != 0)
			error = EPERM;
		else {
			struct acpi_table *table = (struct acpi_table *)data;
			struct acpi_table_header *hdr;
			struct acpi_q *entry;

			error = ENOENT;
			SIMPLEQ_FOREACH(entry, &sc->sc_tables, q_next) {
				if (table->offset-- == 0) {
					hdr = (struct acpi_table_header *)
					    entry->q_table;
					if (table->table == NULL) {
						table->size = hdr->length;
						error = 0;
					} else if (hdr->length > table->size)
						error = ENOSPC;
					else
						error = copyout(hdr,
						    table->table, hdr->length);
					break;
				}
			}
		}
		break;

	default:
		error = ENOTTY;
	}

	ACPI_UNLOCK(sc);
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
