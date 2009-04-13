/*	$OpenBSD: owmac.c,v 1.2 2009/04/13 21:14:41 miod Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
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

/*
 * DS1981/1982/2502 1-Wire Add-only memory driver, for SGI machines.
 *
 * SGI uses DS1981 (or compatibles) to store the Ethernet address
 * on IOC boards.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/proc.h>

#include <dev/onewire/onewiredevs.h>
#include <dev/onewire/onewirereg.h>
#include <dev/onewire/onewirevar.h>

#include <sgi/dev/owmem_subr.h>
#include <sgi/dev/owmacvar.h>

int	owmac_match(struct device *, void *, void *);
void	owmac_attach(struct device *, struct device *, void *);

struct cfattach owmac_ca = {
	sizeof(struct owmac_softc), owmac_match, owmac_attach,
};

struct cfdriver owmac_cd = {
	NULL, "owmac", DV_DULL
};

#define	EEPROM_NPAGES		4

static const struct onewire_matchfam owmac_fams[] = {
	{ ONEWIRE_FAMILY_DS1982 }
};

int	owmac_read_page(struct owmac_softc *, int, uint8_t *);
int	owmac_read_redirect(struct owmac_softc *);

void	owmac_read_mac(struct owmac_softc *);

int
owmac_match(struct device *parent, void *match, void *aux)
{
	struct onewire_attach_args *oa = aux;
	
	if (ONEWIRE_ROM_FAMILY_TYPE(oa->oa_rom) == ONEWIRE_FAMILY_DS1982)
		return 1;

	/*
	 * Also match on UniqueWare devices with specific 0x91 family code.
	 */
	if ((ONEWIRE_ROM_SN(oa->oa_rom) >> (48 - 12)) == 0x5e7 &&
	    ONEWIRE_ROM_FAMILY_CUSTOM(oa->oa_rom) &&
	    ONEWIRE_ROM_FAMILY(oa->oa_rom) == 0x91)
		return 1;

	return 0;
}

void
owmac_attach(struct device *parent, struct device *self, void *aux)
{
	struct owmac_softc *sc = (struct owmac_softc *)self;
	struct onewire_attach_args *oa = aux;

	sc->sc_onewire = oa->oa_onewire;
	sc->sc_rom = oa->oa_rom;

	/*
	 * Read the redirection table.
	 */
	if (owmac_read_redirect(sc) != 0) {
		printf(": unable to read redirection data\n");	
		return;
	}

	printf("\n");

	/*
	 * Read the data.
	 */
	owmac_read_mac(sc);
}

int
owmac_read_redirect(struct owmac_softc *sc)
{
	int rc = 0;
	int status_offset;

	status_offset = 0x0001;		/* 1..4 */

	onewire_lock(sc->sc_onewire, 0);
	if ((rc = onewire_reset(sc->sc_onewire)) != 0)
		goto unlock;

	onewire_matchrom(sc->sc_onewire, sc->sc_rom);

	/*
	 * Start reading the EEPROM status block, at the page redirection
	 * offset.
	 */
	onewire_write_byte(sc->sc_onewire, ONEWIRE_CMD_READ_STATUS);
	onewire_write_byte(sc->sc_onewire, status_offset & 0xff);
	onewire_write_byte(sc->sc_onewire, status_offset >> 8);
	/* XXX should verify this crc value */
	(void)onewire_read_byte(sc->sc_onewire);

	onewire_read_block(sc->sc_onewire, &sc->sc_redir, EEPROM_NPAGES);

	onewire_reset(sc->sc_onewire);
unlock:
	onewire_unlock(sc->sc_onewire);

	return rc;
}

int
owmac_read_page(struct owmac_softc *sc, int page, uint8_t *buf)
{
	int rc = 0;
	int pg;

	/*
	 * Follow the redirection information.
	 */
	if ((pg = owmem_redirect(sc->sc_redir, EEPROM_NPAGES, page)) < 0)
		return EINVAL;

	pg = page * EEPROM_PAGE_SIZE;

	onewire_lock(sc->sc_onewire, 0);
	if ((rc = onewire_reset(sc->sc_onewire)) != 0)
		goto unlock;

	onewire_matchrom(sc->sc_onewire, sc->sc_rom);

	/*
	 * Start reading the EEPROM data.
	 */
	onewire_write_byte(sc->sc_onewire, ONEWIRE_CMD_READ_MEMORY);
	onewire_write_byte(sc->sc_onewire, pg & 0xff);
	onewire_write_byte(sc->sc_onewire, 0);
	/* XXX should verify this crc value */
	(void)onewire_read_byte(sc->sc_onewire);

	onewire_read_block(sc->sc_onewire, buf, EEPROM_PAGE_SIZE);

	onewire_reset(sc->sc_onewire);
unlock:
	onewire_unlock(sc->sc_onewire);

	return rc;
}

void
owmac_read_mac(struct owmac_softc *sc)
{
	uint8_t buf[EEPROM_PAGE_SIZE];

	if (owmac_read_page(sc, 0, buf) != 0)
		return;

	if (buf[0] != 0x0a)
		return;

	sc->sc_enaddr[0] = buf[10];
	sc->sc_enaddr[1] = buf[9];
	sc->sc_enaddr[2] = buf[8];
	sc->sc_enaddr[3] = buf[7];
	sc->sc_enaddr[4] = buf[6];
	sc->sc_enaddr[5] = buf[5];

	printf("%s: Ethernet Address %02x:%02x:%02x:%02x:%02x:%02x\n",
	    sc->sc_dev.dv_xname,
	    sc->sc_enaddr[0], sc->sc_enaddr[1], sc->sc_enaddr[2],
	    sc->sc_enaddr[3], sc->sc_enaddr[4], sc->sc_enaddr[5]);
}
