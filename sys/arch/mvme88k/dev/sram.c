/*	$OpenBSD: sram.c,v 1.1 1998/12/15 05:52:31 smurph Exp $ */

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
 *      This product includes software developed under OpenBSD by
 *	Theo de Raadt for Willowglen Singapore.
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
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/malloc.h>

#include <sys/device.h>
#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <machine/mioctl.h>
#include <vm/vm.h>

struct sramsoftc {
	struct device	sc_dev;
	void *		sc_paddr;
	void *		sc_vaddr;
	int		sc_len;
};

void sramattach __P((struct device *, struct device *, void *));
int  srammatch __P((struct device *, void *, void *));

struct cfattach sram_ca = {
	sizeof(struct sramsoftc), srammatch, sramattach
};

struct cfdriver sram_cd = {
	NULL, "sram", DV_DULL, 0
};

int
srammatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = args;
	int ret;

	if (cputyp != CPU_187)
		return (0);

	ca->ca_paddr = (void *)0xffe00000;
	ca->ca_vaddr = (void *)0xffe00000;

	if (ca->ca_vaddr == (void *)-1){
	    if (badvaddr(ca->ca_vaddr, 4) <= 0){
		printf("==> sram: failed physical address check.\n");
		return (0);
	    }
	}
	if (badvaddr(ca->ca_vaddr, 1) <= 0){
	    printf("==> sram: failed virtual address check.\n");
	    return (0);
	}
	return (1);
}

void
sramattach(parent, self, args)
	struct device *parent, *self;
	void	*args;
{
	struct confargs *ca = args;
	struct sramsoftc *sc = (struct sramsoftc *)self;
	struct mcreg *mc;
	int i;

	switch (cputyp) {
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
#ifdef MVME187
	case CPU_187:
		sc->sc_len = 128*1024;		/* always 128K */
		break;
#endif
	default:
		sc->sc_len = 0;
		break;
	}

	printf(": len %d", sc->sc_len);

	sc->sc_paddr = ca->ca_paddr;
	sc->sc_vaddr = mapiodev((void *)sc->sc_paddr, sc->sc_len);
	if (sc->sc_vaddr == NULL) {
		sc->sc_len = 0;
		printf(" -- failed to map");
	}
	printf("\n");
}

/*ARGSUSED*/
int
sramopen(dev, flag, mode)
	dev_t dev;
	int flag, mode;
{
	if (minor(dev) >= sram_cd.cd_ndevs ||
	    sram_cd.cd_devs[minor(dev)] == NULL)
		return (ENODEV);
	return (0);
}

/*ARGSUSED*/
int
sramclose(dev, flag, mode)
	dev_t dev;
	int flag, mode;
{

	return (0);
}

/*ARGSUSED*/
int
sramioctl(dev, cmd, data, flag, p)
	dev_t   dev;
	caddr_t data;
	int     cmd, flag;
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
sramread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = minor(dev);
	struct sramsoftc *sc = (struct sramsoftc *) sram_cd.cd_devs[unit];

	return (memdevrw(sc->sc_vaddr, sc->sc_len, uio, flags));
}

/*ARGSUSED*/
int
sramwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = minor(dev);
	struct sramsoftc *sc = (struct sramsoftc *) sram_cd.cd_devs[unit];

	return (memdevrw(sc->sc_vaddr, sc->sc_len, uio, flags));
}

int
srammmap(dev, off, prot)
	dev_t dev;
	int off, prot;
{
	int unit = minor(dev);
	struct sramsoftc *sc = (struct sramsoftc *) sram_cd.cd_devs[unit];

	if (minor(dev) != 0)
		return (-1);

	/* allow access only in RAM */
	if (off > sc->sc_len)
		return (-1);
	return (m88k_btop(sc->sc_paddr + off));
}
