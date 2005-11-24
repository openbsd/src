/*	$OpenBSD: sram.c,v 1.16 2005/11/24 22:43:16 miod Exp $ */

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

#include <mvme68k/dev/memdevs.h>

#include "mc.h"

#if NMC > 0
#include <mvme68k/dev/mcreg.h>
#endif

#include <uvm/uvm_extern.h>

struct sramsoftc {
	struct device	sc_dev;
	paddr_t		sc_paddr;
	vaddr_t		sc_vaddr;
	int		sc_len;
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
	struct confargs *ca = args;

	if (cputyp == CPU_147)
		return (0);
	if (ca->ca_vaddr == (vaddr_t)-1)
		return (!badpaddr(ca->ca_paddr, 1));
	return (!badvaddr((vaddr_t)ca->ca_vaddr, 1));
}

void
sramattach(parent, self, args)
	struct device *parent, *self;
	void	*args;
{
	struct confargs *ca = args;
	struct sramsoftc *sc = (struct sramsoftc *)self;
#ifdef MVME162
	struct mcreg *mc;
#endif

	switch (cputyp) {
#ifdef MVME162
	case CPU_162:
		/* XXX this code will almost never be used. just in case. */
		mc = sys_mc;
		if (!mc)
			mc = (struct mcreg *)(IIOV(0xfff00000) + MC_MCCHIP_OFF);

		switch (mc->mc_memoptions & MC_MEMOPTIONS_SRAMMASK) {
		case MC_MEMOPTIONS_SRAM128K:
			sc->sc_len = 128*1024;
			break;
		case MC_MEMOPTIONS_SRAM512K:
			sc->sc_len = 512*1024;
			break;
		case MC_MEMOPTIONS_SRAM1M:
			sc->sc_len = 1024*1024;
			break;
		case MC_MEMOPTIONS_SRAM2M:
			sc->sc_len = 2048*1024;
			break;
		}
		break;
#endif
#ifdef MVME167
	case CPU_167:
	case CPU_166:
		sc->sc_len = 128*1024;		/* always 128K */
		break;
#endif
#ifdef MVME177
	case CPU_177:
		sc->sc_len = 128*1024;		/* always 128K */
		break;
#endif
	default:
		sc->sc_len = 0;
		break;
	}

	printf(": len %d", sc->sc_len);

	sc->sc_paddr = ca->ca_paddr;
	sc->sc_vaddr = mapiodev(sc->sc_paddr, sc->sc_len);
	if (sc->sc_vaddr == 0) {
		sc->sc_len = 0;
		printf(" -- failed to map");
	}
	printf("\n");
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

	return (memdevrw(sc->sc_vaddr, sc->sc_len, uio, flags));
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
	return (atop(sc->sc_paddr + off));
}
