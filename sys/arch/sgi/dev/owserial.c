/*	$OpenBSD: owserial.c,v 1.2 2009/04/19 18:33:53 miod Exp $	*/

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
 * DS2505 1-Wire Add-only memory driver, for SGI machines.
 *
 * SGI seems to use DS2505 (or compatibles) to store serial numbers.
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
#include <sgi/dev/owserialvar.h>

int	owserial_match(struct device *, void *, void *);
void	owserial_attach(struct device *, struct device *, void *);

struct cfattach owserial_ca = {
	sizeof(struct owserial_softc), owserial_match, owserial_attach,
};

struct cfdriver owserial_cd = {
	NULL, "owserial", DV_DULL
};

#define	EEPROM_PAGE_SIZE	(256 / NBBY)	/* 256 bits per page */

static const struct onewire_matchfam owserial_fams[] = {
	{ ONEWIRE_FAMILY_DS2505 },
	{ ONEWIRE_FAMILY_DS2506 }
};

int	owserial_read_page(struct owserial_softc *, int, uint8_t *);
int	owserial_read_redirect(struct owserial_softc *);

void	owserial_read_serial(struct owserial_softc *);

int
owserial_match(struct device *parent, void *match, void *aux)
{
	return (onewire_matchbyfam(aux, owserial_fams,
	    sizeof(owserial_fams) /sizeof(owserial_fams[0])));
}

void
owserial_attach(struct device *parent, struct device *self, void *aux)
{
	struct owserial_softc *sc = (struct owserial_softc *)self;
	struct onewire_attach_args *oa = aux;

	sc->sc_onewire = oa->oa_onewire;
	sc->sc_rom = oa->oa_rom;

	/*
	 * Decide how many pages of 256 bits we have.
	 */

	if (ONEWIRE_ROM_FAMILY_TYPE(sc->sc_rom) == ONEWIRE_FAMILY_DS2506)
		sc->sc_npages = 256;
	else
		sc->sc_npages = 64;

	/*
	 * Read the redirection table.
	 */
	if (owserial_read_redirect(sc) != 0) {
		printf(": unable to read redirection data\n");	
		return;
	}

	printf("\n");

	/*
	 * Read the data.
	 */
	owserial_read_serial(sc);
}

int
owserial_read_redirect(struct owserial_softc *sc)
{
	int rc = 0;
	int status_offset, pos;

	status_offset = 0x0100;		/* 100..13f or 100..1ff */
	pos = 0;

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

	for (pos = 0; pos < sc->sc_npages; pos += 8) {
		onewire_read_block(sc->sc_onewire, sc->sc_redir + pos, 8);
		/* XXX check crc */
		(void)onewire_read_byte(sc->sc_onewire);
		(void)onewire_read_byte(sc->sc_onewire);
	}

	onewire_reset(sc->sc_onewire);
unlock:
	onewire_unlock(sc->sc_onewire);

	return rc;
}

int
owserial_read_page(struct owserial_softc *sc, int page, uint8_t *buf)
{
	int rc = 0;
	int pg;

	/*
	 * Follow the redirection information.
	 */
	if ((pg = owmem_redirect(sc->sc_redir, sc->sc_npages, page)) < 0)
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
	onewire_write_byte(sc->sc_onewire, pg >> 8);

	onewire_read_block(sc->sc_onewire, buf, EEPROM_PAGE_SIZE);

	onewire_reset(sc->sc_onewire);
unlock:
	onewire_unlock(sc->sc_onewire);

	return rc;
}

void
owserial_read_serial(struct owserial_softc *sc)
{
	uint8_t buf[EEPROM_PAGE_SIZE * 2];
	char name[1 + OWSERIAL_NAME_LEN];
	char product[1 + OWSERIAL_PRODUCT_LEN];
	char serial[1 + OWSERIAL_SERIAL_LEN];
	char *s, *e;
	int pg;
	int i;

	pg = owmem_redirect(sc->sc_redir, sc->sc_npages, 0);
	if (pg < 0 || owserial_read_page(sc, pg, buf) != 0)
		return;

	pg = owmem_redirect(sc->sc_redir, sc->sc_npages, 1);
	if (pg < 0 || owserial_read_page(sc, pg, buf + EEPROM_PAGE_SIZE) != 0)
		return;

	/* minimal sanity check */
	if (buf[0] != 0x01)
		return;
	for (i = EEPROM_PAGE_SIZE + 10; i < EEPROM_PAGE_SIZE + 16; i++)
		if (buf[i] != 0xff)
			return;

	bcopy(buf + 21, product, 9);
	bcopy(buf + EEPROM_PAGE_SIZE + 3, product + 9, 3);
	product[OWSERIAL_PRODUCT_LEN] = '\0';
	for (i = 0; i < OWSERIAL_PRODUCT_LEN; i++)
		if (product[i] != '-' && (product[i] < '0' || product[i] > '9'))
			return;
		
	bcopy(buf + EEPROM_PAGE_SIZE + 16, name, OWSERIAL_NAME_LEN);
	name[OWSERIAL_NAME_LEN] = '\0';
	for (i = 0; i < OWSERIAL_NAME_LEN; i++)
		if (name[i] < ' ' || name[i] > '~')
			return;

	bcopy(buf + 1, serial, OWSERIAL_SERIAL_LEN);
	serial[OWSERIAL_SERIAL_LEN] = '\0';
	for (i = 0; i < OWSERIAL_SERIAL_LEN; i++)
		if (serial[i] < ' ' || serial[i] > '~')
			return;

	/*
	 * Trim leading and trailing spaces from name and serial #
	 */

	strlcpy(sc->sc_product, product, sizeof sc->sc_product);

	s = name;
	while (*s == ' ')
		s++;
	e = name + OWSERIAL_NAME_LEN - 1;
	while (*e == ' ' && e >= s)
		*e-- = '\0';
	strlcpy(sc->sc_name, s, sizeof sc->sc_name);

	s = serial;
	while (*s == ' ')
		s++;
	e = serial + OWSERIAL_SERIAL_LEN - 1;
	while (*e == ' ' && e >= s)
		*e-- = '\0';
	strlcpy(sc->sc_serial, s, sizeof sc->sc_serial);

	printf("%s: \"%s\" p/n %s, serial %s\n",
	    sc->sc_dev.dv_xname, sc->sc_name, sc->sc_product, sc->sc_serial);
}
