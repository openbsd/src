/*	$OpenBSD: sram.c,v 1.17 2004/04/24 19:51:48 miod Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/conf.h>
#include <machine/cpu.h>
#include <machine/mioctl.h>

#include <mvme88k/dev/memdevs.h>

#include <uvm/uvm_extern.h>

struct sramsoftc {
	struct device		sc_dev;
	paddr_t			sc_base;
	size_t			sc_len;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
};

void sramattach(struct device *, struct device *, void *);
int  srammatch(struct device *, void *, void *);

struct cfattach sram_ca = {
	sizeof(struct sramsoftc), srammatch, sramattach
};

struct cfdriver sram_cd = {
	NULL, "sram", DV_DULL
};

int
srammatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	if (brdtyp != BRD_187 && brdtyp != BRD_8120)	/* The only ones... */
		return (0);

	return (1);
}

void
sramattach(parent, self, args)
	struct device *parent, *self;
	void	*args;
{
	struct confargs *ca = args;
	struct sramsoftc *sc = (struct sramsoftc *)self;
	bus_space_handle_t ioh;

	sc->sc_iot = ca->ca_iot;
	sc->sc_base = ca->ca_paddr;
	sc->sc_len = 128 * 1024;		/* always 128K */

	if (bus_space_map(sc->sc_iot, sc->sc_base, sc->sc_len, 0, &ioh) != 0) {
		printf(": can't map memory!\n");
		return;
	}

	sc->sc_ioh = ioh;

	printf(": %dKB\n", sc->sc_len / 1024);
}

/*ARGSUSED*/
int
sramopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	if (minor(dev) >= sram_cd.cd_ndevs ||
	    sram_cd.cd_devs[minor(dev)] == NULL)
		return (ENODEV);
	return (0);
}

/*ARGSUSED*/
int
sramclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{

	return (0);
}

/*ARGSUSED*/
int
sramioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = minor(dev);
	struct sramsoftc *sc = (struct sramsoftc *) sram_cd.cd_devs[unit];
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
sramrw(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = minor(dev);
	struct sramsoftc *sc = (struct sramsoftc *) sram_cd.cd_devs[unit];

	return memdevrw(bus_space_vaddr(sc->sc_iot, sc->sc_ioh),
	    sc->sc_len, uio, flags);
}

paddr_t
srammmap(dev, off, prot)
	dev_t dev;
	off_t off;
	int prot;
{
	int unit = minor(dev);
	struct sramsoftc *sc = (struct sramsoftc *) sram_cd.cd_devs[unit];

	if (minor(dev) != 0)
		return (-1);

	/* allow access only in RAM */
	if (off < 0 || off > sc->sc_len)
		return (-1);
	return (atop(sc->sc_base + off));
}
