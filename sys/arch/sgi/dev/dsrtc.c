/*	$OpenBSD: dsrtc.c,v 1.11 2009/11/07 14:49:01 miod Exp $ */

/*
 * Copyright (c) 2001-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/bus.h>

#include <dev/ic/ds1687reg.h>
#define	todr_chip_handle_t void *	/* XXX that's just to eat prototypes */
#include <dev/ic/mk48txxreg.h>

#include <mips64/archtype.h>
#include <mips64/dev/clockvar.h>

#include <sgi/dev/dsrtcvar.h>
#include <sgi/localbus/macebusvar.h>
#include <sgi/pci/iocreg.h>
#include <sgi/pci/iocvar.h>
#include <sgi/pci/iofvar.h>

struct	dsrtc_softc {
	struct device		sc_dev;
	bus_space_tag_t		sc_clkt;
	bus_space_handle_t	sc_clkh, sc_clkh2;
	int			sc_yrbase;

	int			(*read)(struct dsrtc_softc *, int);
	void			(*write)(struct dsrtc_softc *, int, int);
};

int	dsrtc_match(struct device *, void *, void *);
void	dsrtc_attach_ioc(struct device *, struct device *, void *);
void	dsrtc_attach_iof(struct device *, struct device *, void *);
void	dsrtc_attach_macebus(struct device *, struct device *, void *);

struct cfdriver dsrtc_cd = {
	NULL, "dsrtc", DV_DULL
};

struct cfattach dsrtc_macebus_ca = {
	sizeof(struct dsrtc_softc), dsrtc_match, dsrtc_attach_macebus
};

struct cfattach dsrtc_ioc_ca = {
	sizeof(struct dsrtc_softc), dsrtc_match, dsrtc_attach_ioc
};

struct cfattach dsrtc_iof_ca = {
	sizeof(struct dsrtc_softc), dsrtc_match, dsrtc_attach_ioc
};

int	ip32_dsrtc_read(struct dsrtc_softc *, int);
void	ip32_dsrtc_write(struct dsrtc_softc *, int, int);
int	ioc_ds1687_dsrtc_read(struct dsrtc_softc *, int);
void	ioc_ds1687_dsrtc_write(struct dsrtc_softc *, int, int);

void	ds1687_get(void *, time_t, struct tod_time *);
void	ds1687_set(void *, struct tod_time *);
void	ds1742_get(void *, time_t, struct tod_time *);
void	ds1742_set(void *, struct tod_time *);

static inline int frombcd(int, int);
static inline int tobcd(int, int);
static inline int
frombcd(int x, int binary)
{
	return binary ? x : (x >> 4) * 10 + (x & 0xf);
}
static inline int
tobcd(int x, int binary)
{
	return binary ? x : (x / 10 * 16) + (x % 10);
}

int
dsrtc_match(struct device *parent, void *match, void *aux)
{
	/*
	 * Depending on what dsrtc attaches to, the actual attach_args
	 * may be a different struct, but all of them start with the
	 * same name field.
	 */
	struct mainbus_attach_args *maa = aux;

	return strcmp(maa->maa_name, dsrtc_cd.cd_name) == 0;
}

void
dsrtc_attach_ioc(struct device *parent, struct device *self, void *aux)
{
	struct dsrtc_softc *sc = (void *)self;
	struct ioc_attach_args *iaa = aux;
	bus_space_handle_t ih, ih2;

	/*
	 * The IOC3 RTC is either a Dallas (now Maxim) DS1386 or compatible
	 * (likely a more recent DS1687), or a DS1747 or compatible
	 * (itself being a Mostek MK48T35 clone).
	 *
	 * Surprisingly, the chip found on Fuel has a DS1742W label,
	 * which has much less memory than the DS1747. I guess whatever
	 * the chip is, it is mapped to the end of the DS1747 address
	 * space, so that the clock registers always appear at the same
	 * addresses in memory.
	 */

	sc->sc_clkt = iaa->iaa_memt;

	if (iaa->iaa_base != IOC3_BYTEBUS_0) {
		/* DS1687 */

		if (bus_space_subregion(iaa->iaa_memt, iaa->iaa_memh,
		    IOC3_BYTEBUS_1, 1, &ih) != 0 ||
		    bus_space_subregion(iaa->iaa_memt, iaa->iaa_memh,
		    IOC3_BYTEBUS_2, 1, &ih2) != 0)
			goto fail;

		printf(": DS1687\n");

		sc->sc_clkh = ih;
		sc->sc_clkh2 = ih2;

		sc->read = ioc_ds1687_dsrtc_read;
		sc->write = ioc_ds1687_dsrtc_write;

		sys_tod.tod_get = ds1687_get;
		sys_tod.tod_set = ds1687_set;
	} else {
		/* DS1742W */

		bus_space_unmap(iaa->iaa_memt, ih, 1);
		bus_space_unmap(iaa->iaa_memt, ih2, 1);

		if (bus_space_subregion(iaa->iaa_memt, iaa->iaa_memh,
		    iaa->iaa_base + MK48T35_CLKOFF,
		    MK48T35_CLKSZ - MK48T35_CLKOFF, &ih) != 0)
			goto fail;

		printf(": DS1742W\n");

		sc->sc_clkh = ih;

		/*
		 * For some reason, the base year differs between IP27
		 * and IP35.
		 */
		sc->sc_yrbase = sys_config.system_type == SGI_IP35 ?
		    POSIX_BASE_YEAR - 2 : POSIX_BASE_YEAR;
		/* mips64 clock code expects year relative to 1900 */
		sc->sc_yrbase -= 1900;

		sys_tod.tod_get = ds1742_get;
		sys_tod.tod_set = ds1742_set;
	}
	sys_tod.tod_cookie = self;

	return;

fail:
	printf(": can't map registers\n");
}

void
dsrtc_attach_iof(struct device *parent, struct device *self, void *aux)
{
	struct dsrtc_softc *sc = (void *)self;
	struct iof_attach_args *iaa = aux;
	bus_space_handle_t ih;

	/*
	 * The IOC4 RTC is a DS1747 or compatible (itself being a Mostek
	 * MK48T35 clone).
	 */

	if (bus_space_subregion(iaa->iaa_memt, iaa->iaa_memh,
	    iaa->iaa_base + MK48T35_CLKOFF,
	    MK48T35_CLKSZ - MK48T35_CLKOFF, &ih) != 0)
		goto fail;

	printf(": DS1742W\n");

	sc->sc_clkh = ih;

	/*
	 * For some reason, the base year differs between IP27
	 * and IP35.
	 */
	sc->sc_yrbase = sys_config.system_type == SGI_IP35 ?
	    POSIX_BASE_YEAR - 2 : POSIX_BASE_YEAR;
	/* mips64 clock code expects year relative to 1900 */
	sc->sc_yrbase -= 1900;

	sys_tod.tod_cookie = self;
	sys_tod.tod_get = ds1742_get;
	sys_tod.tod_set = ds1742_set;

	return;

fail:
	printf(": can't map registers\n");
}

void
dsrtc_attach_macebus(struct device *parent, struct device *self, void *aux)
{
	struct dsrtc_softc *sc = (void *)self;
	struct macebus_attach_args *maa = aux;

	sc->sc_clkt = maa->maa_iot;
	if (bus_space_map(sc->sc_clkt, maa->maa_baseaddr, 128 * 256, 0,
	    &sc->sc_clkh)) {
		printf(": can't map registers\n");
		return;
	}

	printf(": DS1687\n");

	sc->read = ip32_dsrtc_read;
	sc->write = ip32_dsrtc_write;

	sys_tod.tod_cookie = self;
	sys_tod.tod_get = ds1687_get;
	sys_tod.tod_set = ds1687_set;
}

int
ip32_dsrtc_read(struct dsrtc_softc *sc, int reg)
{
	return bus_space_read_1(sc->sc_clkt, sc->sc_clkh, reg);
}

void
ip32_dsrtc_write(struct dsrtc_softc *sc, int reg, int val)
{
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, reg, val);
}

int
ioc_ds1687_dsrtc_read(struct dsrtc_softc *sc, int reg)
{
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, 0, reg);
	return bus_space_read_1(sc->sc_clkt, sc->sc_clkh2, 0);
}

void
ioc_ds1687_dsrtc_write(struct dsrtc_softc *sc, int reg, int val)
{
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, 0, reg);
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh2, 0, val);
}

/*
 * Dallas DS1687 clock driver.
 */

void
ds1687_get(void *v, time_t base, struct tod_time *ct)
{
	struct dsrtc_softc *sc = v;
	int ctrl, century, dm;

	/* Select bank 1. */
	ctrl = (*sc->read)(sc, DS1687_CTRL_A);
	(*sc->write)(sc, DS1687_CTRL_A, ctrl | DS1687_BANK_1);

	/* Figure out which data mode to use. */
	dm = (*sc->read)(sc, DS1687_CTRL_B) & DS1687_DM_1;

	/* Wait for no update in progress. */
	while ((*sc->read)(sc, DS1687_CTRL_A) & DS1687_UIP)
		/* Do nothing. */;

	/* Read the RTC. */
	ct->sec = frombcd((*sc->read)(sc, DS1687_SEC), dm);
	ct->min = frombcd((*sc->read)(sc, DS1687_MIN), dm);
	ct->hour = frombcd((*sc->read)(sc, DS1687_HOUR), dm);
	ct->day = frombcd((*sc->read)(sc, DS1687_DAY), dm);
	ct->mon = frombcd((*sc->read)(sc, DS1687_MONTH), dm);
	ct->year = frombcd((*sc->read)(sc, DS1687_YEAR), dm);
	century = frombcd((*sc->read)(sc, DS1687_CENTURY), dm);

	ct->year += 100 * (century - 19);
}

void
ds1687_set(void *v, struct tod_time *ct)
{
	struct dsrtc_softc *sc = v;
	int year, century, ctrl, dm;

	century = ct->year / 100 + 19;
	year = ct->year % 100;

	/* Select bank 1. */
	ctrl = (*sc->read)(sc, DS1687_CTRL_A);
	(*sc->write)(sc, DS1687_CTRL_A, ctrl | DS1687_BANK_1);

	/* Figure out which data mode to use, and select 24 hour time. */
	ctrl = (*sc->read)(sc, DS1687_CTRL_B);
	dm = ctrl & DS1687_DM_1;
	(*sc->write)(sc, DS1687_CTRL_B, ctrl | DS1687_24_HR);

	/* Prevent updates. */
	ctrl = (*sc->read)(sc, DS1687_CTRL_B);
	(*sc->write)(sc, DS1687_CTRL_B, ctrl | DS1687_SET_CLOCK);

	/* Update the RTC. */
	(*sc->write)(sc, DS1687_SEC, tobcd(ct->sec, dm));
	(*sc->write)(sc, DS1687_MIN, tobcd(ct->min, dm));
	(*sc->write)(sc, DS1687_HOUR, tobcd(ct->hour, dm));
	(*sc->write)(sc, DS1687_DOW, tobcd(ct->dow, dm));
	(*sc->write)(sc, DS1687_DAY, tobcd(ct->day, dm));
	(*sc->write)(sc, DS1687_MONTH, tobcd(ct->mon, dm));
	(*sc->write)(sc, DS1687_YEAR, tobcd(year, dm));
	(*sc->write)(sc, DS1687_CENTURY, tobcd(century, dm));

	/* Enable updates. */
	(*sc->write)(sc, DS1687_CTRL_B, ctrl);
}

/*
 * Dallas DS1742 clock driver.
 */

void
ds1742_get(void *v, time_t base, struct tod_time *ct)
{
	struct dsrtc_softc *sc = v;
	int csr;

	/* Freeze update. */
	csr = bus_space_read_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_ICSR);
	csr |= MK48TXX_CSR_READ;
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_ICSR, csr);

	/* Read the RTC. */
	ct->sec = frombcd(bus_space_read_1(sc->sc_clkt, sc->sc_clkh,
	    MK48TXX_ISEC), 0);
	ct->min = frombcd(bus_space_read_1(sc->sc_clkt, sc->sc_clkh,
	    MK48TXX_IMIN), 0);
	ct->hour = frombcd(bus_space_read_1(sc->sc_clkt, sc->sc_clkh,
	    MK48TXX_IHOUR), 0);
	ct->day = frombcd(bus_space_read_1(sc->sc_clkt, sc->sc_clkh,
	    MK48TXX_IDAY), 0);
	ct->mon = frombcd(bus_space_read_1(sc->sc_clkt, sc->sc_clkh,
	    MK48TXX_IMON), 0);
	ct->year = frombcd(bus_space_read_1(sc->sc_clkt, sc->sc_clkh,
	    MK48TXX_IYEAR), 0) + sc->sc_yrbase;

	/* Enable updates again. */
	csr = bus_space_read_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_ICSR);
	csr &= ~MK48TXX_CSR_READ;
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_ICSR, csr);
}

void
ds1742_set(void *v, struct tod_time *ct)
{
	struct dsrtc_softc *sc = v;
	int csr;

	/* Enable write. */
	csr = bus_space_read_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_ICSR);
	csr |= MK48TXX_CSR_WRITE;
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_ICSR, csr);

	/* Update the RTC. */
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_ISEC,
	    tobcd(ct->sec, 0));
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_IMIN,
	    tobcd(ct->min, 0));
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_IHOUR,
	    tobcd(ct->hour, 0));
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_IWDAY,
	    tobcd(ct->dow, 0));
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_IDAY,
	    tobcd(ct->day, 0));
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_IMON,
	    tobcd(ct->mon, 0));
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_IYEAR,
	    tobcd(ct->year - sc->sc_yrbase, 0));

	/* Load new values. */
	csr = bus_space_read_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_ICSR);
	csr &= ~MK48TXX_CSR_WRITE;
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_ICSR, csr);
}

/*
 * Routines allowing external access to the RTC registers, used by
 * power(4).
 */

int
dsrtc_register_read(int reg)
{
	struct dsrtc_softc *sc;

	if (dsrtc_cd.cd_ndevs == 0 ||
	    (sc = (struct dsrtc_softc *)dsrtc_cd.cd_devs[0]) == NULL ||
	    sc->read == NULL)
		return -1;

	return (*sc->read)(sc, reg);
}

void
dsrtc_register_write(int reg, int val)
{
	struct dsrtc_softc *sc;

	if (dsrtc_cd.cd_ndevs == 0 ||
	    (sc = (struct dsrtc_softc *)dsrtc_cd.cd_devs[0]) == NULL ||
	    sc->write == NULL)
		return;

	(*sc->write)(sc, reg, val);
}
