/*	$OpenBSD: vmel.c,v 1.14 2005/11/24 22:43:16 miod Exp $ */

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
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/conf.h>
#include <machine/cpu.h>

#include <mvme68k/dev/vme.h>

/*
 * The VMEL driver deals with D32 transfers on the VME bus. The number
 * of address bits (A16, A24, A32) is irrelevant since the mapping
 * functions will decide how many address bits are relevant.
 */

void vmelattach(struct device *, struct device *, void *);
int  vmelmatch(struct device *, void *, void *);

int vmelscan(struct device *, void *, void *);

struct cfattach vmel_ca = {
	sizeof(struct vmelsoftc), vmelmatch, vmelattach
};

struct cfdriver vmel_cd = {
	NULL, "vmel", DV_DULL
};

int
vmelmatch(parent, cf, args)
	struct device *parent;
	void *cf, *args;
{
	return (1);
}

int
vmelscan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	return (vmescan(parent, child, args, BUS_VMEL));
}

void
vmelattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct vmelsoftc *sc = (struct vmelsoftc *)self;

	printf("\n");

	sc->sc_vme = (struct vmesoftc *)parent;

	config_search(vmelscan, self, args);
}

/*ARGSUSED*/
int
vmelopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	if (minor(dev) >= vmel_cd.cd_ndevs ||
	    vmel_cd.cd_devs[minor(dev)] == NULL)
		return (ENODEV);
	return (0);
}

/*ARGSUSED*/
int
vmelclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{

	return (0);
}

/*ARGSUSED*/
int
vmelioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int error = 0;

	switch (cmd) {
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

int
vmelread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = minor(dev);
	struct vmelsoftc *sc = (struct vmelsoftc *) vmel_cd.cd_devs[unit];

	return (vmerw(sc->sc_vme, uio, flags, BUS_VMEL));
}

int
vmelwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = minor(dev);
	struct vmelsoftc *sc = (struct vmelsoftc *) vmel_cd.cd_devs[unit];

	return (vmerw(sc->sc_vme, uio, flags, BUS_VMEL));
}

paddr_t
vmelmmap(dev, off, prot)
	dev_t dev;
	off_t off;
	int prot;
{
	int unit = minor(dev);
	struct vmelsoftc *sc = (struct vmelsoftc *) vmel_cd.cd_devs[unit];
	paddr_t pa;

	pa = vmepmap(sc->sc_vme, (vaddr_t)off, NBPG, BUS_VMEL);
#ifdef DEBUG
	printf("vmel %llx pa %p\n", off, pa);
#endif
	if (pa == NULL)
		return (-1);
	return (atop(pa));
}
