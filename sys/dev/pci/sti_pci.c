/*	$OpenBSD: sti_pci.c,v 1.7 2009/02/06 22:51:04 miod Exp $	*/

/*
 * Copyright (c) 2006, 2007 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice, this permission notice, and the disclaimer below
 * appear in all copies.
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

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/wscons/wsdisplayvar.h>

#include <dev/ic/stireg.h>
#include <dev/ic/stivar.h>

int	sti_pci_match(struct device *, void *, void *);
void	sti_pci_attach(struct device *, struct device *, void *);

struct	sti_pci_softc {
	struct sti_softc	sc_base;

	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_tag;

	bus_space_handle_t	sc_romh;
};

struct cfattach sti_pci_ca = {
	sizeof(struct sti_pci_softc), sti_pci_match, sti_pci_attach
};

const struct pci_matchid sti_pci_devices[] = {
	{ PCI_VENDOR_HP, PCI_PRODUCT_HP_VISUALIZE_EG },
	{ PCI_VENDOR_HP, PCI_PRODUCT_HP_VISUALIZE_FX2 },
	{ PCI_VENDOR_HP, PCI_PRODUCT_HP_VISUALIZE_FX4 },
	{ PCI_VENDOR_HP, PCI_PRODUCT_HP_VISUALIZE_FX6 },
	{ PCI_VENDOR_HP, PCI_PRODUCT_HP_VISUALIZE_FXE },
};

int	sti_readbar(struct sti_softc *, struct pci_attach_args *, u_int, int);
int	sti_check_rom(struct sti_pci_softc *, struct pci_attach_args *);
void	sti_pci_enable_rom(struct sti_softc *);
void	sti_pci_disable_rom(struct sti_softc *);

int	sti_pci_is_console(struct pci_attach_args *, bus_addr_t *);

int
sti_pci_match(struct device *parent, void *cf, void *aux)
{
	struct pci_attach_args *paa = aux;

	return (pci_matchbyid(paa, sti_pci_devices,
	    sizeof(sti_pci_devices) / sizeof(sti_pci_devices[0])));
}

void
sti_pci_attach(struct device *parent, struct device *self, void *aux)
{
	struct sti_pci_softc *spc = (void *)self;
	struct pci_attach_args *paa = aux;

	spc->sc_pc = paa->pa_pc;
	spc->sc_tag = paa->pa_tag;
	spc->sc_base.sc_enable_rom = sti_pci_enable_rom;
	spc->sc_base.sc_disable_rom = sti_pci_disable_rom;

	printf("\n");

	if (sti_check_rom(spc, paa) != 0)
		return;

	printf("%s", self->dv_xname);
	if (sti_pci_is_console(paa, spc->sc_base.bases) != 0)
		spc->sc_base.sc_flags |= STI_CONSOLE;
	if (sti_attach_common(&spc->sc_base, paa->pa_iot, paa->pa_memt,
	    spc->sc_romh, STI_CODEBASE_MAIN) == 0)
		startuphook_establish(sti_end_attach, spc);
}

/*
 * Grovel the STI ROM image.
 */
int
sti_check_rom(struct sti_pci_softc *spc, struct pci_attach_args *pa)
{
	struct sti_softc *sc = &spc->sc_base;
	pcireg_t address, mask;
	bus_space_handle_t romh;
	bus_size_t romsize, subsize, stiromsize;
	bus_addr_t selected, offs, suboffs;
	u_int32_t tmp;
	int i;
	int rc;

	/* sort of inline sti_pci_enable_rom(sc) */
	address = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_ROM_REG);
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_ROM_REG, ~PCI_ROM_ENABLE);
	mask = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_ROM_REG);
	address |= PCI_ROM_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_ROM_REG, address);
	sc->sc_flags |= STI_ROM_ENABLED;

	/*
	 * Map the complete ROM for now.
	 */

	romsize = PCI_ROM_SIZE(mask);
	rc = bus_space_map(pa->pa_memt, PCI_ROM_ADDR(address), romsize,
	    0, &romh);
	sti_pci_disable_rom(sc);
	if (rc != 0) {
		printf("%s: can't map PCI ROM (%d)\n",
		    sc->sc_dev.dv_xname, rc);
		goto fail2;
	}

	/*
	 * Iterate over the ROM images, pick the best candidate.
	 */

	selected = (bus_addr_t)-1;
	for (offs = 0; offs < romsize; offs += subsize) {
		sti_pci_enable_rom(sc);
		/*
		 * Check for a valid ROM header.
		 */
		tmp = bus_space_read_4(pa->pa_memt, romh, offs + 0);
		tmp = letoh32(tmp);
		if (tmp != 0x55aa0000) {
			sti_pci_disable_rom(sc);
			if (offs == 0) {
				printf("%s: invalid PCI ROM header signature"
				    " (%08x)\n",
				    sc->sc_dev.dv_xname, tmp);
				rc = EINVAL;
			}
			break;
		}

		/*
		 * Check ROM type.
		 */
		tmp = bus_space_read_4(pa->pa_memt, romh, offs + 4);
		tmp = letoh32(tmp);
		if (tmp != 0x00000001) {	/* 1 == STI ROM */
			sti_pci_disable_rom(sc);
			if (offs == 0) {
				printf("%s: invalid PCI ROM type (%08x)\n",
				    sc->sc_dev.dv_xname, tmp);
				rc = EINVAL;
			}
			break;
		}

		subsize = (bus_addr_t)bus_space_read_2(pa->pa_memt, romh,
		    offs + 0x0c);
		subsize <<= 9;

#ifdef STIDEBUG
		sti_pci_disable_rom(sc);
		printf("ROM offset %08x size %08x type %08x",
		    offs, subsize, tmp);
		sti_pci_enable_rom(sc);
#endif

		/*
		 * Check for a valid ROM data structure.
		 * We do not need it except to know what architecture the ROM
		 * code is for.
		 */

		suboffs = offs +(bus_addr_t)bus_space_read_2(pa->pa_memt, romh,
		    offs + 0x18);
		tmp = bus_space_read_4(pa->pa_memt, romh, suboffs + 0);
		tmp = letoh32(tmp);
		if (tmp != 0x50434952) {	/* PCIR */
			sti_pci_disable_rom(sc);
			if (offs == 0) {
				printf("%s: invalid PCI data signature"
				    " (%08x)\n",
				    sc->sc_dev.dv_xname, tmp);
				rc = EINVAL;
			} else {
#ifdef STIDEBUG
				printf(" invalid PCI data signature %08x\n",
				    tmp);
#endif
				continue;
			}
		}

		tmp = bus_space_read_1(pa->pa_memt, romh, suboffs + 0x14);
		sti_pci_disable_rom(sc);
#ifdef STIDEBUG
		printf(" code %02x", tmp);
#endif

		switch (tmp) {
#ifdef __hppa__
		case 0x10:
			if (selected == (bus_addr_t)-1)
				selected = offs;
			break;
#endif
#ifdef __i386__
		case 0x00:
			if (selected == (bus_addr_t)-1)
				selected = offs;
			break;
#endif
		default:
#ifdef STIDEBUG
			printf(" (wrong architecture)");
#endif
			break;
		}

#ifdef STIDEBUG
		if (selected == offs)
			printf(" -> SELECTED");
		printf("\n");
#endif
	}

	if (selected == (bus_addr_t)-1) {
		if (rc == 0) {
			printf("%s: found no ROM with correct microcode"
			    " architecture\n", sc->sc_dev.dv_xname);
			rc = ENOEXEC;
		}
		goto fail;
	}

	/*
	 * Read the STI region BAR assignments.
	 */

	sti_pci_enable_rom(sc);
	offs = selected +
	    (bus_addr_t)bus_space_read_2(pa->pa_memt, romh, selected + 0x0e);
	for (i = 0; i < STI_REGION_MAX; i++) {
		rc = sti_readbar(sc, pa, i,
		    bus_space_read_1(pa->pa_memt, romh, offs + i));
		if (rc != 0)
			goto fail;
	}

	/*
	 * Find out where the STI ROM itself lies, and its size.
	 */

	offs = selected +
	    (bus_addr_t)bus_space_read_4(pa->pa_memt, romh, selected + 0x08);
	stiromsize = (bus_addr_t)bus_space_read_4(pa->pa_memt, romh,
	    offs + 0x18);
	stiromsize = letoh32(stiromsize);
	sti_pci_disable_rom(sc);

	/*
	 * Replace our mapping with a smaller mapping of only the area
	 * we are interested in.
	 */

	bus_space_unmap(pa->pa_memt, romh, romsize);
	rc = bus_space_map(pa->pa_memt, PCI_ROM_ADDR(address) + offs,
	    stiromsize, 0, &spc->sc_romh);
	if (rc != 0) {
		printf("%s: can't map STI ROM (%d)\n",
		    sc->sc_dev.dv_xname, rc);
		goto fail2;
	}

	return (0);

fail:
	bus_space_unmap(pa->pa_memt, romh, romsize);
fail2:
	sti_pci_disable_rom(sc);

	return (rc);
}

/*
 * Decode a BAR register.
 */
int
sti_readbar(struct sti_softc *sc, struct pci_attach_args *pa, u_int region,
    int bar)
{
	bus_addr_t addr;
	bus_size_t size;
	u_int32_t cf;
	int rc;

	if (bar == 0) {
		sc->bases[region] = 0;
		return (0);
	}

#ifdef DIAGNOSTIC
	if (bar < PCI_MAPREG_START || bar > PCI_MAPREG_PPB_END) {
		sti_pci_disable_rom(sc);
		printf("%s: unexpected bar %02x for region %d\n",
		    sc->sc_dev.dv_xname, bar, region);
		sti_pci_enable_rom(sc);
	}
#endif

	cf = pci_conf_read(pa->pa_pc, pa->pa_tag, bar);

	if (PCI_MAPREG_TYPE(cf) == PCI_MAPREG_TYPE_IO)
		rc = pci_io_find(pa->pa_pc, pa->pa_tag, bar, &addr, &size);
	else
		rc = pci_mem_find(pa->pa_pc, pa->pa_tag, bar, &addr, &size,
		    NULL);

	if (rc != 0) {
		sti_pci_disable_rom(sc);
		printf("%s: invalid bar %02x for region %d\n",
		    sc->sc_dev.dv_xname, bar, region);
		sti_pci_enable_rom(sc);
		return (rc);
	}

	sc->bases[region] = addr;
	return (0);
}

/*
 * Enable PCI ROM.
 */
void
sti_pci_enable_rom(struct sti_softc *sc)
{
	struct sti_pci_softc *spc = (struct sti_pci_softc *)sc;
	pcireg_t address;

	if (!ISSET(sc->sc_flags, STI_ROM_ENABLED)) {
		address = pci_conf_read(spc->sc_pc, spc->sc_tag, PCI_ROM_REG);
		address |= PCI_ROM_ENABLE;
		pci_conf_write(spc->sc_pc, spc->sc_tag, PCI_ROM_REG, address);
		SET(sc->sc_flags, STI_ROM_ENABLED);
	}
}

/*
 * Disable PCI ROM.
 */
void
sti_pci_disable_rom(struct sti_softc *sc)
{
	struct sti_pci_softc *spc = (struct sti_pci_softc *)sc;
	pcireg_t address;

	if (ISSET(sc->sc_flags, STI_ROM_ENABLED)) {
		address = pci_conf_read(spc->sc_pc, spc->sc_tag, PCI_ROM_REG);
		address &= ~PCI_ROM_ENABLE;
		pci_conf_write(spc->sc_pc, spc->sc_tag, PCI_ROM_REG, address);

		CLR(sc->sc_flags, STI_ROM_ENABLED);
	}
}
