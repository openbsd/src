/*	$OpenBSD: vmes.c,v 1.2 1998/12/15 05:52:31 smurph Exp $ */

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Theo de Raadt
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <mvme88k/dev/vme.h>

/*
 * The VMES driver deals with D16 transfers on the VME bus. The number
 * of address bits (A16, A24, A32) is irrelevant since the mapping
 * functions will decide how many address bits are relevant.
 */

void vmesattach __P((struct device *, struct device *, void *));
int  vmesmatch __P((struct device *, void *, void *));

struct cfattach vmes_ca = {
        sizeof(struct vmessoftc), vmesmatch, vmesattach
}; 
 
struct cfdriver vmes_cd = {
        NULL, "vmes", DV_DULL, 0
};

int
vmesmatch(parent, cf, args)
	struct device *parent;
	void *cf, *args;
{
	return (1);
}

int
vmesscan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	return (vmescan(parent, child, args, BUS_VMES));
}

void
vmesattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct vmessoftc *sc = (struct vmessoftc *)self;

	printf("\n");

	sc->sc_vme = (struct vmesoftc *)parent;

	config_search(vmesscan, self, args);
}

/*ARGSUSED*/
int
vmesopen(dev, flag, mode)
	dev_t dev;
	int flag, mode;
{
	if (minor(dev) >= vmes_cd.cd_ndevs ||
	    vmes_cd.cd_devs[minor(dev)] == NULL)
		return (ENODEV);
	return (0);
}

/*ARGSUSED*/
int
vmesclose(dev, flag, mode)
	dev_t dev;
	int flag, mode;
{

	return (0);
}

/*ARGSUSED*/
int
vmesioctl(dev, cmd, data, flag, p)
	dev_t   dev;
	caddr_t data;
	int     cmd, flag;
	struct proc *p;
{
	int unit = minor(dev);
	struct vmessoftc *sc = (struct vmessoftc *) vmes_cd.cd_devs[unit];
	int error = 0;

	switch (cmd) {
	default:
		error = ENOTTY;
		break;
	}
	return (error);
}

int
vmesread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = minor(dev);
	struct vmessoftc *sc = (struct vmessoftc *) vmes_cd.cd_devs[unit];

	return (vmerw(sc->sc_vme, uio, flags, BUS_VMES));
}

int
vmeswrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = minor(dev);
	struct vmessoftc *sc = (struct vmessoftc *) vmes_cd.cd_devs[unit];

	return (vmerw(sc->sc_vme, uio, flags, BUS_VMES));
}

int
vmesmmap(dev, off, prot)
	dev_t dev;
	int off, prot;
{
	int unit = minor(dev);
	struct vmessoftc *sc = (struct vmessoftc *) vmes_cd.cd_devs[unit];
	void * pa;

	pa = vmepmap(sc->sc_vme, (void *)off, NBPG, BUS_VMES);
	printf("vmes %x pa %x\n", off, pa);
	if (pa == NULL)
		return (-1);
	return (m88k_btop(pa));
}
