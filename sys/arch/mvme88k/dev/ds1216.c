/*	$OpenBSD: ds1216.c,v 1.2 2013/05/24 16:49:54 miod Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
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
 * AngelFire Dallas DS1216 TOD clock support.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/time.h>

#include <machine/autoconf.h>
#include <machine/board.h>
#include <machine/bus.h>

#include <machine/mvme181.h>

#include <mvme88k/mvme88k/clockvar.h>

struct ds1216_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

int	ds1216_match(struct device *, void *, void *);
void	ds1216_attach(struct device *, struct device *, void *);

const struct cfattach dsrtc_ca = {
	sizeof(struct ds1216_softc), ds1216_match, ds1216_attach
};

struct cfdriver dsrtc_cd = {
	NULL, "dsrtc", DV_DULL
};

void	ds1216_write_byte(struct ds1216_softc *, uint8_t);
uint8_t	ds1216_read_byte(struct ds1216_softc *);
void	ds1216_select(struct ds1216_softc *);
void	ds1216_unselect(struct ds1216_softc *);

time_t	ds1216_inittodr(void);
void	ds1216_resettodr(void);

/*
 * Register layout. All numerical values are in BCD.
 */

#define	DS1216_FRAC	0	/* hundredths of seconds (00-99) */
#define	DS1216_SEC	1	/* seconds (00-59) */
#define	DS1216_MIN	2	/* minutes (00-59) */
#define	DS1216_HOUR	3	/* hour (01-12/00-23) and hour control */
#define	DS1216_HOUR_12		0x80	/* 12 hour mode */
#define	DS1216_HOUR_PM		0x20	/* PM bit in 12 hour mode */
#define	DS1216_HOUR_12_MASK	0x1f	/* valid digit bits in 12 hour mode */
#define	DS1216_HOUR_24_MASK	0x3f	/* valid digit bits in 24 hour mode */
#define	DS1216_DOW	4	/* day of week (01-07) and controls */
#define	DS1216_STOP		0x20	/* oscillator stop */
#define	DS1216_RESET		0x10	/* reset (active low) */
#define	DS1216_DOW_MASK		0x07	/* valid dow bits */
#define	DS1216_DAY	5	/* day of month (01-31) */
#define	DS1216_MONTH	6	/* month (01-12) */
#define	DS1216_YEAR	7	/* year (00-99) */

#define	DS1216_NREGS	8

/*
 * Low-level access routines.
 */

/*
 * Access to the Dallas chip is performed by sending a magic sequence
 * first, bit by bit. Two low-order address bits select whether the
 * TOD registers or the on-chip memory is accessed.
 *
 * After the sequence is sent, it is possible to read or write the TOD
 * register or the on-chip memory, using a similar bit-by-bit operation,
 * as a whole.
 *
 * When the read or write sequence completes, the Dallas chip becomes
 * pass-through again.
 */

#define	NCYCLES	64

const uint8_t ds1216_unlock_pattern[NCYCLES / 8] = {
	0xc5, 0x3a, 0xa3, 0x5c, 0xc5, 0x3a, 0xa3, 0x5c
};

/*
 * On the MVME181, the Dallas chip is connected to cpu A2 onwards (this is
 * similar to the DART wiring), so the A0 and A2 lines are actually A2 and A4.
 */
#define	A0	((1 << 0) << 2)
#define	A2	((1 << 2) << 2)
#define	DQ0	(1 << 0)

/*
 * Write 8 bits. A2 low for writing, data in A0.
 */
void
ds1216_write_byte(struct ds1216_softc *sc, uint8_t b)
{
	uint i;

	for (i = 8; i != 0; i--) {
		bus_space_read_1(sc->sc_iot, sc->sc_ioh, b & DQ0 ? A0 : 0);
		b >>= 1;
	}
}

/*
 * Read 8 bits. A2 high for reading, data in DQ0.
 */
uint8_t
ds1216_read_byte(struct ds1216_softc *sc)
{
	uint8_t b = 0, bit = DQ0;
	uint i;

	for (i = 8; i != 0; i--) {
		if (bus_space_read_1(sc->sc_iot, sc->sc_ioh, A2) & DQ0)
			b |= bit;
		bit <<= 1;
	}

	return b;
}

/*
 * Select the Dallas chip: read once with A2 high, then issue the 64-bit
 * unlock sequence on A0 with A2 low.
 */
void
ds1216_select(struct ds1216_softc *sc)
{
	const uint8_t *bsrc = ds1216_unlock_pattern;
	uint8_t b;
	uint i;

	bus_space_read_1(sc->sc_iot, sc->sc_ioh, A2);
	for (i = NCYCLES; i != 0; i--) {
		if ((i % 8) == 0)
			b = *bsrc++;
		bus_space_read_1(sc->sc_iot, sc->sc_ioh, b & DQ0 ? A0 : 0);
		b >>= 1;
	}
}

/*
 * Make sure the Dallas chip is not currently selected by performing enough
 * dummy read cycles to switch it to pass-through mode, if it wasn't already
 * in this state.
 */
void
ds1216_unselect(struct ds1216_softc *sc)
{
	uint i;

	for (i = NCYCLES; i != 0; i--) {
		bus_space_read_1(sc->sc_iot, sc->sc_ioh, A2 | A0);
	}
}

/*
 * Kernel TOD glue.
 */

time_t
ds1216_inittodr()
{
	struct ds1216_softc *sc = (struct ds1216_softc *)dsrtc_cd.cd_devs[0];
	struct clock_ymdhms c;
	uint8_t regs[DS1216_NREGS];
	uint i;

	ds1216_select(sc);
	for (i = 0; i < DS1216_NREGS; i++)
		regs[i] = ds1216_read_byte(sc);

	c.dt_sec = FROMBCD(regs[DS1216_SEC]);
	c.dt_min = FROMBCD(regs[DS1216_MIN]);
	/*
	 * BUG should have set up the clock in 24 hour mode. Allow for
	 * 12 hour mode, just in case.
	 */
	if (ISSET(regs[DS1216_HOUR], DS1216_HOUR_12)) {
		c.dt_hour = FROMBCD(regs[DS1216_HOUR] & DS1216_HOUR_12_MASK);
		if (c.dt_hour == 12)
			c.dt_hour = 0;
		if (ISSET(regs[DS1216_HOUR], DS1216_HOUR_PM))
			c.dt_hour += 12;
	} else
		c.dt_hour = FROMBCD(regs[DS1216_HOUR] & DS1216_HOUR_24_MASK);
	c.dt_day = FROMBCD(regs[DS1216_DAY]);
	c.dt_mon = FROMBCD(regs[DS1216_MONTH]);
	c.dt_year = FROMBCD(regs[DS1216_YEAR]) + 1900;
	/* XXX 2-digit year => Y2070 problem */
	if (c.dt_year < POSIX_BASE_YEAR)
		c.dt_year += 100;

	return clock_ymdhms_to_secs(&c);
}

void
ds1216_resettodr()
{
	struct ds1216_softc *sc = (struct ds1216_softc *)dsrtc_cd.cd_devs[0];
	struct clock_ymdhms c;
	uint8_t regs[DS1216_NREGS];
	uint i;

	clock_secs_to_ymdhms(time_second, &c);

	regs[DS1216_FRAC] = 0;
	regs[DS1216_SEC] = TOBCD(c.dt_sec);
	regs[DS1216_MIN] = TOBCD(c.dt_min);
	regs[DS1216_HOUR] = TOBCD(c.dt_hour);
	regs[DS1216_DOW] = DS1216_RESET | (c.dt_wday + 1);
	regs[DS1216_DAY] = TOBCD(c.dt_day);
	regs[DS1216_MONTH] = TOBCD(c.dt_mon);
	regs[DS1216_YEAR] = TOBCD(c.dt_year % 100);

	ds1216_select(sc);
	for (i = 0; i < DS1216_NREGS; i++)
		ds1216_write_byte(sc, regs[i]);
}

/*
 * Autoconf glue.
 */

int
ds1216_match(struct device *parent, void *vcf, void *aux)
{
	struct confargs *ca = (struct confargs *)aux;

	switch (brdtyp) {
#ifdef MVME181
	case BRD_180:
	case BRD_181:
		break;
#endif
	default:
		return 0;
	}

	if (ca->ca_paddr != M181_DSRTC)
		return 0;

	/*
	 * Can't use badaddr() here - the Dallas chip overlaps the BUG image
	 * when selected; therefore access never fails even if the chip
	 * were to be missing.
	 */
	return 1;
}

void
ds1216_attach(struct device *parent, struct device *self, void *aux)
{
	struct ds1216_softc *sc = (struct ds1216_softc *)self;
	struct confargs *ca = (struct confargs *)aux;

	sc->sc_iot = ca->ca_iot;
	if (bus_space_map(ca->ca_iot, ca->ca_paddr, A2 << 1, 0,
	    &sc->sc_ioh) != 0) {
		printf(": can't map registers\n");
		return;
	}

	printf("\n");

	/*
	 * Force the chip out of selection. This is unlikely necessary,
	 * the BUG is expected to have done this for us.
	 */
	ds1216_unselect(sc);

	md_inittodr = ds1216_inittodr;
	md_resettodr = ds1216_resettodr;
}
