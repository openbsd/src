/*	$OpenBSD: octrtc.c,v 1.4 2014/10/26 15:13:04 jasper Exp $	*/

/*
 * Copyright (c) 2013, 2014 Paul Irofti.
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
#include <sys/proc.h>

#include <mips64/dev/clockvar.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/octeonvar.h>

#ifdef OCTRTC_DEBUG
#define DPRINTF(x)	printf x
#else
#define DPRINTF(x)
#endif

#define MIO_TWS_SW_TWSI		0x0001180000001000ULL
#define MIO_TWS_SW_TWSI_EXT	0x0001180000001018ULL
#define OCTRTC_REG	0x68

struct cfdriver octrtc_cd = {
	NULL, "octrtc", DV_DULL
};

int	octrtc_match(struct device *, void *, void *);
void	octrtc_attach(struct device *, struct device *, void *);

void	octrtc_gettime(void *, time_t, struct tod_time *);
int	octrtc_read(uint8_t *, char);

void	octrtc_settime(void *, struct tod_time *);
int	octrtc_write(uint8_t);


struct cfattach octrtc_ca = {
	sizeof(struct device), octrtc_match, octrtc_attach,
};


union mio_tws_sw_twsi_reg {
	uint64_t reg;
	struct cvmx_mio_twsx_sw_twsi_s {
		uint64_t v:1;		/* Valid bit */
		uint64_t slonly:1;	/* Slave Only Mode */
		uint64_t eia:1;		/* Extended Internal Address */
		uint64_t op:4;		/* Opcode field */
		uint64_t r:1;		/* Read bit or result */
		uint64_t sovr:1;	/* Size Override */
		uint64_t size:3;	/* Size in bytes */
		uint64_t scr:2;		/* Scratch, unused */
		uint64_t a:10;		/* Address field */
		uint64_t ia:5;		/* Internal Address */
		uint64_t eop_ia:3;	/* Extra opcode */
		uint64_t d:32;		/* Data Field */
	} field;
};


int
octrtc_match(struct device *parent, void *match, void *aux)
{
	struct mainbus_attach_args *maa = aux;
	struct cfdata *cf = match;

	if (strcmp(maa->maa_name, cf->cf_driver->cd_name) != 0)
		return 0;
	/* No RTC on Ubiquiti */
	if ((octeon_boot_info->board_type == BOARD_TYPE_UBIQUITI_E100) ||
	    (octeon_boot_info->board_type == BOARD_TYPE_UBIQUITI_E200))
		return 0;
	return 1;
}

void
octrtc_attach(struct device *parent, struct device *self, void *aux)
{
	struct octrtc_softc *sc = (struct octrtc_softc *)self;

	sys_tod.tod_cookie = sc;
	sys_tod.tod_get = octrtc_gettime;
	sys_tod.tod_set = octrtc_settime;

	printf(": DS1337\n");
}

void
octrtc_gettime(void *cookie, time_t unused, struct tod_time *tt)
{
	uint8_t tod[8];
	uint8_t check;
	int i, rc;

	int nretries = 2;

	DPRINTF(("\nTOD: "));
	while (nretries--) {
		rc = octrtc_read(&tod[0], 1);	/* ia read */
		if (rc) {
			DPRINTF(("octrtc_read(0) failed %d", rc));
			return;
		}

		for (i = 1; i < 8; i++) {
			rc = octrtc_read(&tod[i], 0);	/* current address */
			if (rc) {
				DPRINTF(("octrtc_read(%d) failed %d", i, rc));
				return;
			}
			DPRINTF(("%#X ", tod[i]));
		}

		/* Check against time-wrap */
		rc = octrtc_read(&check, 1);	/* ia read */
		if (rc) {
			DPRINTF(("octrtc_read(check) failed %d", i, rc));
			return;
		}
		if ((check & 0xf) == (tod[0] & 0xf))
			break;
	}
	DPRINTF(("\n"));

	DPRINTF(("Time: %d %d %d (%d) %02d:%02d:%02d\n",
	    ((tod[5] & 0x80) ? 2000 : 1900) + FROMBCD(tod[6]),	/* year */
	    FROMBCD(tod[5] & 0x1f),	/* month */
	    FROMBCD(tod[4] & 0x3f),	/* day */
	    (tod[3] & 0x7),		/* day of the week */
	    FROMBCD(tod[2] & 0x3f),	/* hour */
	    FROMBCD(tod[1] & 0x7f),	/* minute */
	    FROMBCD(tod[0] & 0x7f)));	/* second */

	tt->year = ((tod[5] & 0x80) ? 100 : 0) + FROMBCD(tod[6]);
	tt->mon = FROMBCD(tod[5] & 0x1f);
	tt->day = FROMBCD(tod[4] & 0x3f);
	tt->dow = (tod[3] & 0x7);
	tt->hour = FROMBCD(tod[2] & 0x3f);
	if ((tod[2] & 0x40) && (tod[2] & 0x20))	/* adjust AM/PM format */
		tt->hour = (tt->hour + 12) % 24;
	tt->min = FROMBCD(tod[1] & 0x7f);
	tt->sec = FROMBCD(tod[0] & 0x7f);
}

int
octrtc_read(uint8_t *data, char ia)
{
	int nretries = 5;
	union mio_tws_sw_twsi_reg twsi;

again:
	twsi.reg = 0;
	twsi.field.v = 1;
	twsi.field.r = 1;
	twsi.field.sovr = 1;
	twsi.field.a = OCTRTC_REG;
	if (ia) {
		twsi.field.op = 1;
	}

	octeon_xkphys_write_8(MIO_TWS_SW_TWSI, twsi.reg);
	/* The 1st bit is cleared when the operation is complete */
	do {
		delay(1000);
		twsi.reg = octeon_xkphys_read_8(MIO_TWS_SW_TWSI);
	} while (twsi.field.v);
	DPRINTF(("%#llX ", twsi.reg));

	/*
	 * The data field is in the upper 32 bits and we're only
	 * interested in the first byte.
	 */
	*data = twsi.field.d & 0xff;

	/* 8th bit is the read result: 1 = success, 0 = failure */
	if (twsi.field.r == 0) {
		/*
		 * Lost arbitration : 0x38, 0x68, 0xB0, 0x78
		 * Core busy as slave: 0x80, 0x88, 0xA0, 0xA8, 0xB8, 0xC0, 0xC8
		 */
		if (*data == 0x38 || *data == 0x68 || *data == 0xB0 ||
		    *data == 0x78 || *data == 0x80 || *data == 0x88 ||
		    *data == 0xA0 || *data == 0xA8 || *data == 0xB8 ||
		    *data == 0xC0 || *data == 0xC8)
			if (nretries--)
				goto again;
		return EIO;
	}

	return 0;
}

void
octrtc_settime(void *cookie, struct tod_time *tt)
{
	int nretries = 2;
	int rc, i;
	uint8_t tod[8];
	uint8_t check;

	DPRINTF(("settime: %d %d %d (%d) %02d:%02d:%02d\n",
	    tt->year, tt->mon, tt->day, tt->dow,
	    tt->hour, tt->min, tt->sec));

	tod[0] = TOBCD(tt->sec);
	tod[1] = TOBCD(tt->min);
	tod[2] = TOBCD(tt->hour);
	tod[3] = TOBCD(tt->dow);
	tod[4] = TOBCD(tt->day);
	tod[5] = TOBCD(tt->mon);
	if (tt->year >= 100)
		tod[5] |= 0x80;
	tod[6] = TOBCD(tt->year % 100);

	while (nretries--) {
		for (i = 0; i < 7; i++) {
			rc = octrtc_write(tod[i]);
			if (rc) {
				DPRINTF(("octrtc_write(%d) failed %d", i, rc));
				return;
			}
		}

		rc = octrtc_read(&check, 1);
		if (rc) {
			DPRINTF(("octrtc_read(check) failed %d", rc));
			return;
		}

		if ((check & 0xf) == (tod[0] & 0xf))
			break;
	}
}

int
octrtc_write(uint8_t data)
{
	union mio_tws_sw_twsi_reg twsi;
	int npoll = 128;

	twsi.reg = 0;
	twsi.field.v = 1;
	twsi.field.sovr = 1;
	twsi.field.op = 1;
	twsi.field.a = OCTRTC_REG;
	twsi.field.d = data & 0xffffffff;

	octeon_xkphys_write_8(MIO_TWS_SW_TWSI_EXT, 0);
	octeon_xkphys_write_8(MIO_TWS_SW_TWSI, twsi.reg);
	do {
		delay(1000);
		twsi.reg = octeon_xkphys_read_8(MIO_TWS_SW_TWSI);
	} while (twsi.field.v);

	/* Try to read back */
	while (npoll-- && octrtc_read(&data, 0));

	return npoll ? 0 : EIO;
}
