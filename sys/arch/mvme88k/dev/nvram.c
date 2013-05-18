/*	$OpenBSD: nvram.c,v 1.35 2013/05/18 10:21:50 miod Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <machine/autoconf.h>
#include <machine/bugio.h>
#include <machine/conf.h>
#include <machine/cpu.h>
#include <machine/mioctl.h>

#include <uvm/uvm_param.h>

#include <mvme88k/mvme88k/clockvar.h>
#include <mvme88k/dev/memdevs.h>
#include <mvme88k/dev/nvramreg.h>

struct nvramsoftc {
	struct device           sc_dev;
	paddr_t			sc_base;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_addr_t		sc_regs;
	size_t			sc_len;

#ifdef MVME188
	u_int8_t		*sc_nvram;
#endif
};

void    nvramattach(struct device *, struct device *, void *);
int     nvrammatch(struct device *, void *, void *);

struct cfattach nvram_ca = {
	sizeof(struct nvramsoftc), nvrammatch, nvramattach
};

struct cfdriver nvram_cd = {
	NULL, "nvram", DV_DULL
};

time_t	chiptotime(struct clock_ymdhms *);
void	timetochip(struct clock_ymdhms *);
time_t	nvram_inittodr(void);
void	nvram_resettodr(void);

int	nvram188read(struct nvramsoftc *, struct uio *, int);
int	nvram188write(struct nvramsoftc *, struct uio *, int);

int
nvrammatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct confargs *ca = args;
	bus_space_handle_t ioh;
	int rc;

	if (bus_space_map(ca->ca_iot, ca->ca_paddr, PAGE_SIZE, 0, &ioh) != 0)
		return (0);
	rc = badaddr((vaddr_t)bus_space_vaddr(ca->ca_iot, ioh), 1) == 0;
	bus_space_unmap(ca->ca_iot, ioh, PAGE_SIZE);
	return (rc);
}

void
nvramattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct confargs *ca = args;
	struct nvramsoftc *sc = (struct nvramsoftc *)self;
	bus_space_handle_t ioh;
	vsize_t maplen;

	switch (brdtyp) {
#ifdef MVME188
	case BRD_188:
		sc->sc_len = MK48T02_SIZE;
		maplen = sc->sc_len * 4;
		sc->sc_regs = M188_NVRAM_TOD_OFF;
		break;
#endif
	default:
		sc->sc_len = MK48T08_SIZE;
		maplen = sc->sc_len;
		sc->sc_regs = SBC_NVRAM_TOD_OFF;
		break;
	}

	sc->sc_iot = ca->ca_iot;
	sc->sc_base = ca->ca_paddr;

	if (bus_space_map(sc->sc_iot, sc->sc_base, round_page(maplen),
	    BUS_SPACE_MAP_LINEAR, &ioh) != 0) {
		printf(": can't map mem space\n");
		return;
	}

	sc->sc_ioh = ioh;

	printf(": MK48T0%lu\n", sc->sc_len / 1024);

	md_inittodr = nvram_inittodr;
	md_resettodr = nvram_resettodr;
}

time_t
chiptotime(struct clock_ymdhms *c)
{
	c->dt_sec = FROMBCD(c->dt_sec);
	c->dt_min = FROMBCD(c->dt_min);
	c->dt_hour = FROMBCD(c->dt_hour);
	c->dt_day = FROMBCD(c->dt_day);
	c->dt_mon = FROMBCD(c->dt_mon);
	c->dt_year = FROMBCD(c->dt_year) + YEAR0;
	/* XXX 2-digit year => Y2070 problem */
	if (c->dt_year < POSIX_BASE_YEAR)
		c->dt_year += 100;

	return clock_ymdhms_to_secs(c);
}

void
timetochip(struct clock_ymdhms *c)
{
	clock_secs_to_ymdhms(time_second, c);

	c->dt_sec = TOBCD(c->dt_sec);
	c->dt_min = TOBCD(c->dt_min);
	c->dt_hour = TOBCD(c->dt_hour);
	c->dt_day = TOBCD(c->dt_day);
	c->dt_mon = TOBCD(c->dt_mon);
	c->dt_year = TOBCD(c->dt_year % 100);
}

time_t
nvram_inittodr()
{
	struct nvramsoftc *sc = nvram_cd.cd_devs[0];
	struct clock_ymdhms c;

	switch (brdtyp) {
#ifdef MVME188
	case BRD_188:
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + (CLK_CSR << 2), CLK_READ |
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		      sc->sc_regs + (CLK_CSR << 2)));
		c.dt_sec = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + (CLK_SEC << 2)) & 0xff;
		c.dt_min = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + (CLK_MIN << 2)) & 0xff;
		c.dt_hour = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + (CLK_HOUR << 2)) & 0xff;
		c.dt_day = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + (CLK_DAY << 2)) & 0xff;
		c.dt_mon = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + (CLK_MONTH << 2)) & 0xff;
		c.dt_year = bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + (CLK_YEAR << 2)) & 0xff;
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + (CLK_CSR << 2),
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		      sc->sc_regs + (CLK_CSR << 2)) & ~CLK_READ);
		break;
#endif
	default:
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + CLK_CSR, CLK_READ |
		    bus_space_read_1(sc->sc_iot, sc->sc_ioh,
		      sc->sc_regs + CLK_CSR));
		c.dt_sec = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + CLK_SEC);
		c.dt_min = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + CLK_MIN);
		c.dt_hour = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + CLK_HOUR);
		c.dt_day = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + CLK_DAY);
		c.dt_mon = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + CLK_MONTH);
		c.dt_year = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + CLK_YEAR);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + CLK_CSR,
		    bus_space_read_1(sc->sc_iot, sc->sc_ioh,
		      sc->sc_regs + CLK_CSR) & ~CLK_READ);
	}

	return chiptotime(&c);
}

/*
 * Reset the clock based on the current time.
 * Used when the current clock is preposterous, when the time is changed,
 * and when rebooting.  Do nothing if the time is not yet known, e.g.,
 * when crashing during autoconfig.
 */
void
nvram_resettodr()
{
	struct nvramsoftc *sc = nvram_cd.cd_devs[0];
	struct clock_ymdhms c;

	timetochip(&c);

	switch (brdtyp) {
#ifdef MVME188
	case BRD_188:
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + (CLK_CSR << 2), CLK_WRITE |
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		      sc->sc_regs + (CLK_CSR << 2)));
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + (CLK_SEC << 2), c.dt_sec);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + (CLK_MIN << 2), c.dt_min);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + (CLK_HOUR << 2), c.dt_hour);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + (CLK_WDAY << 2), c.dt_wday);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + (CLK_DAY << 2), c.dt_day);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + (CLK_MONTH << 2), c.dt_mon);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + (CLK_YEAR << 2), c.dt_year);
		bus_space_write_4(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + (CLK_CSR << 2),
		    bus_space_read_4(sc->sc_iot, sc->sc_ioh,
		      sc->sc_regs + (CLK_CSR << 2)) & ~CLK_WRITE);
		break;
#endif
	default:
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + CLK_CSR, CLK_WRITE |
		    bus_space_read_1(sc->sc_iot, sc->sc_ioh,
		      sc->sc_regs + CLK_CSR));
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + CLK_SEC, c.dt_sec);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + CLK_MIN, c.dt_min);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + CLK_HOUR, c.dt_hour);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + CLK_WDAY, c.dt_wday);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + CLK_DAY, c.dt_day);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + CLK_MONTH, c.dt_mon);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + CLK_YEAR, c.dt_year);
		bus_space_write_1(sc->sc_iot, sc->sc_ioh,
		    sc->sc_regs + CLK_CSR,
		    bus_space_read_1(sc->sc_iot, sc->sc_ioh,
		      sc->sc_regs + CLK_CSR) & ~CLK_WRITE);
		break;
	}
}

/*ARGSUSED*/
int
nvramopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	if (minor(dev) >= nvram_cd.cd_ndevs ||
	    nvram_cd.cd_devs[minor(dev)] == NULL)
		return (ENODEV);

	return (0);
}

/*ARGSUSED*/
int
nvramclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	/*
	 * On MVME188, it might be worth free()ing the NVRAM copy here.
	 */
	return (0);
}

/*ARGSUSED*/
int
nvramioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = minor(dev);
	struct nvramsoftc *sc = (struct nvramsoftc *) nvram_cd.cd_devs[unit];
	int error = 0;

	switch (cmd) {
	case MIOCGSIZ:
		*(int *)data = sc->sc_len;
		break;
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

/*ARGSUSED*/
int
nvramread(dev_t dev, struct uio *uio, int flags)
{
	int unit = minor(dev);
	struct nvramsoftc *sc = (struct nvramsoftc *)nvram_cd.cd_devs[unit];

#ifdef MVME188
	if (brdtyp == BRD_188)
		return (nvram188read(sc, uio, flags));
#endif

	return (memdevrw(bus_space_vaddr(sc->sc_iot, sc->sc_ioh),
	    sc->sc_len, uio, flags));
}

/*ARGSUSED*/
int
nvramwrite(dev_t dev, struct uio *uio, int flags)
{
	int unit = minor(dev);
	struct nvramsoftc *sc = (struct nvramsoftc *) nvram_cd.cd_devs[unit];

#ifdef MVME188
	if (brdtyp == BRD_188)
		return (nvram188write(sc, uio, flags));
#endif

	return (memdevrw(bus_space_vaddr(sc->sc_iot, sc->sc_ioh),
	    sc->sc_len, uio, flags));
}

paddr_t
nvrammmap(dev, off, prot)
	dev_t dev;
	off_t off;
	int prot;
{
	int unit = minor(dev);
	struct nvramsoftc *sc = (struct nvramsoftc *) nvram_cd.cd_devs[unit];

	if (minor(dev) != 0)
		return (-1);

#ifdef MVME188
	/* disallow mmap on MVME188 due to non-linear layout */
	if (brdtyp == BRD_188)
		return (-1);
#endif

	/* allow access only in RAM */
	if (off < 0 || off >= round_page(sc->sc_len))
		return (-1);
	return (sc->sc_base + off);
}

#ifdef MVME188

int	read_nvram(struct nvramsoftc *);

/*
 * Build a local copy of the NVRAM contents.
 */
int
read_nvram(struct nvramsoftc *sc)
{
	size_t cnt;
	u_int8_t *dest;
	u_int32_t *src;

	if (sc->sc_nvram == NULL) {
		sc->sc_nvram = (u_int8_t *)malloc(sc->sc_len, M_DEVBUF, 
		    M_WAITOK | M_CANFAIL);
		if (sc->sc_nvram == NULL)
			return (EAGAIN);
	}

	dest = sc->sc_nvram;
	src = (u_int32_t *)bus_space_vaddr(sc->sc_iot, sc->sc_ioh);
	cnt = sc->sc_len;
	while (cnt-- != 0)
		*dest++ = (u_int8_t)*src++;

	return (0);
}

/*
 * Specific memdevrw wrappers to cope with the 188 design.
 */

int
nvram188read(struct nvramsoftc *sc, struct uio *uio, int flags)
{
	int rc;

	/*
	 * Get a copy of the NVRAM contents.
	 */
	rc = read_nvram(sc);
	if (rc != 0)
		return (rc);

	/*
	 * Move data from our NVRAM copy to the user.
	 */
	return (memdevrw(sc->sc_nvram, sc->sc_len, uio, flags));
}

int
nvram188write(struct nvramsoftc *sc, struct uio *uio, int flags)
{
	size_t cnt;
	u_int8_t *src;
	u_int32_t *dest;
	int rc;

	/*
	 * Get a copy of the NVRAM contents.
	 */
	rc = read_nvram(sc);
	if (rc != 0)
		return (rc);

	/*
	 * Move data from the user to our NVRAM copy.
	 */
	rc = memdevrw(sc->sc_nvram, sc->sc_len, uio, flags);
	if (rc != 0) {
		/* reset NVRAM copy contents */
		read_nvram(sc);
		return (rc);
	}

	/*
	 * Update the NVRAM. This could be optimized by only working on
	 * the areas which have been modified by the user.
	 */
	src = sc->sc_nvram;
	dest = (u_int32_t *)bus_space_vaddr(sc->sc_iot, sc->sc_ioh);
	cnt = sc->sc_len;
	while (cnt-- != 0) {
		if ((*dest & 0xff) != *src)
			*dest = (u_int32_t)*src;
		dest++;
		src++;
	}

	return (0);
}
#endif
