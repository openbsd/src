/*	$OpenBSD: dsrtc.c,v 1.14 2020/05/21 01:48:43 visa Exp $ */

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

#include <dev/clock_subr.h>
#include <dev/ic/ds1687reg.h>
#include <dev/ic/mk48txxreg.h>

#include <mips64/archtype.h>

#include <sgi/dev/dsrtcvar.h>
#include <sgi/localbus/macebusvar.h>
#include <sgi/pci/iocreg.h>
#include <sgi/pci/iocvar.h>
#include <sgi/pci/iofvar.h>

struct	dsrtc_softc {
	struct device		sc_dev;
	struct todr_chip_handle	sc_todr;
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

int	ds1687_gettime(struct todr_chip_handle *, struct timeval *);
int	ds1687_settime(struct todr_chip_handle *, struct timeval *);
int	ds1742_gettime(struct todr_chip_handle *, struct timeval *);
int	ds1742_settime(struct todr_chip_handle *, struct timeval *);

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
	 * The IOC3 RTC is either a Dallas (now Maxim) DS1397 or compatible
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

		sc->sc_todr.todr_gettime = ds1687_gettime;
		sc->sc_todr.todr_settime = ds1687_settime;
	} else {
		/* DS1742W */

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

		sc->sc_todr.todr_gettime = ds1742_gettime;
		sc->sc_todr.todr_settime = ds1742_settime;
	}
	sc->sc_todr.cookie = self;
	todr_attach(&sc->sc_todr);

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

	sc->sc_todr.cookie = self;
	sc->sc_todr.todr_gettime = ds1742_gettime;
	sc->sc_todr.todr_settime = ds1742_settime;
	todr_attach(&sc->sc_todr);

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

	sc->sc_todr.cookie = self;
	sc->sc_todr.todr_gettime = ds1687_gettime;
	sc->sc_todr.todr_settime = ds1687_settime;
	todr_attach(&sc->sc_todr);
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

int
ds1687_gettime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct clock_ymdhms dt;
	struct dsrtc_softc *sc = handle->cookie;
	int ctrl, dm;

	/* Select bank 1. */
	ctrl = (*sc->read)(sc, DS1687_CTRL_A);
	(*sc->write)(sc, DS1687_CTRL_A, ctrl | DS1687_BANK_1);

	/* Figure out which data mode to use. */
	dm = (*sc->read)(sc, DS1687_CTRL_B) & DS1687_DM_1;

	/* Wait for no update in progress. */
	while ((*sc->read)(sc, DS1687_CTRL_A) & DS1687_UIP)
		/* Do nothing. */;

	/* Read the RTC. */
	dt.dt_sec = frombcd((*sc->read)(sc, DS1687_SEC), dm);
	dt.dt_min = frombcd((*sc->read)(sc, DS1687_MIN), dm);
	dt.dt_hour = frombcd((*sc->read)(sc, DS1687_HOUR), dm);
	dt.dt_day = frombcd((*sc->read)(sc, DS1687_DAY), dm);
	dt.dt_mon = frombcd((*sc->read)(sc, DS1687_MONTH), dm);
	dt.dt_year = frombcd((*sc->read)(sc, DS1687_YEAR), dm);
	dt.dt_year += frombcd((*sc->read)(sc, DS1687_CENTURY), dm) * 100;

	tv->tv_sec = clock_ymdhms_to_secs(&dt);
	tv->tv_usec = 0;
	return 0;
}

int
ds1687_settime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct clock_ymdhms dt;
	struct dsrtc_softc *sc = handle->cookie;
	int year, century, ctrl, dm;

	clock_secs_to_ymdhms(tv->tv_sec, &dt);

	century = dt.dt_year / 100;
	year = dt.dt_year % 100;

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
	(*sc->write)(sc, DS1687_SEC, tobcd(dt.dt_sec, dm));
	(*sc->write)(sc, DS1687_MIN, tobcd(dt.dt_min, dm));
	(*sc->write)(sc, DS1687_HOUR, tobcd(dt.dt_hour, dm));
	(*sc->write)(sc, DS1687_DOW, tobcd(dt.dt_wday + 1, dm));
	(*sc->write)(sc, DS1687_DAY, tobcd(dt.dt_day, dm));
	(*sc->write)(sc, DS1687_MONTH, tobcd(dt.dt_mon, dm));
	(*sc->write)(sc, DS1687_YEAR, tobcd(year, dm));
	(*sc->write)(sc, DS1687_CENTURY, tobcd(century, dm));

	/* Enable updates. */
	(*sc->write)(sc, DS1687_CTRL_B, ctrl);

	return 0;
}

/*
 * Dallas DS1742 clock driver.
 */

int
ds1742_gettime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct clock_ymdhms dt;
	struct dsrtc_softc *sc = handle->cookie;
	int csr;

	/* Freeze update. */
	csr = bus_space_read_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_ICSR);
	csr |= MK48TXX_CSR_READ;
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_ICSR, csr);

	/* Read the RTC. */
	dt.dt_sec = frombcd(bus_space_read_1(sc->sc_clkt, sc->sc_clkh,
	    MK48TXX_ISEC), 0);
	dt.dt_min = frombcd(bus_space_read_1(sc->sc_clkt, sc->sc_clkh,
	    MK48TXX_IMIN), 0);
	dt.dt_hour = frombcd(bus_space_read_1(sc->sc_clkt, sc->sc_clkh,
	    MK48TXX_IHOUR), 0);
	dt.dt_day = frombcd(bus_space_read_1(sc->sc_clkt, sc->sc_clkh,
	    MK48TXX_IDAY), 0);
	dt.dt_mon = frombcd(bus_space_read_1(sc->sc_clkt, sc->sc_clkh,
	    MK48TXX_IMON), 0);
	dt.dt_year = frombcd(bus_space_read_1(sc->sc_clkt, sc->sc_clkh,
	    MK48TXX_IYEAR), 0) + sc->sc_yrbase;

	/* Enable updates again. */
	csr = bus_space_read_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_ICSR);
	csr &= ~MK48TXX_CSR_READ;
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_ICSR, csr);

	tv->tv_sec = clock_ymdhms_to_secs(&dt);
	tv->tv_usec = 0;
	return 0;
}

int
ds1742_settime(struct todr_chip_handle *handle, struct timeval *tv)
{
	struct clock_ymdhms dt;
	struct dsrtc_softc *sc = handle->cookie;
	int csr;

	clock_secs_to_ymdhms(tv->tv_sec, &dt);

	/* Enable write. */
	csr = bus_space_read_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_ICSR);
	csr |= MK48TXX_CSR_WRITE;
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_ICSR, csr);

	/* Update the RTC. */
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_ISEC,
	    tobcd(dt.dt_sec, 0));
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_IMIN,
	    tobcd(dt.dt_min, 0));
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_IHOUR,
	    tobcd(dt.dt_hour, 0));
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_IWDAY,
	    tobcd(dt.dt_wday + 1, 0));
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_IDAY,
	    tobcd(dt.dt_day, 0));
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_IMON,
	    tobcd(dt.dt_mon, 0));
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_IYEAR,
	    tobcd(dt.dt_year - sc->sc_yrbase, 0));

	/* Load new values. */
	csr = bus_space_read_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_ICSR);
	csr &= ~MK48TXX_CSR_WRITE;
	bus_space_write_1(sc->sc_clkt, sc->sc_clkh, MK48TXX_ICSR, csr);

	return 0;
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
