/*	$Id: flash.c,v 1.2 1995/11/07 08:48:55 deraadt Exp $ */

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
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/callout.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/device.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/mioctl.h>

#include "mc.h"

#if NMC > 0
#include <mvme68k/dev/mcreg.h>
#endif

#include <mvme68k/dev/flashreg.h>

struct flashsoftc {
	struct device		sc_dev;
	caddr_t			sc_paddr;
	volatile u_char *	sc_vaddr;
	u_char			sc_manu;
	u_char			sc_ii;
	int			sc_len;
	int			sc_zonesize;
};

void flashattach __P((struct device *, struct device *, void *));
int  flashmatch __P((struct device *, void *, void *));

struct cfdriver flashcd = {
	NULL, "flash", flashmatch, flashattach,
	DV_DULL, sizeof(struct flashsoftc), 0
};

int flashwritebyte __P((struct flashsoftc *sc, int addr, u_char val));
int flasherasezone __P((struct flashsoftc *sc, int addr));

struct flashii intel_flashii[] = {
	{ "28F008SA",	FLII_INTEL_28F008SA,	1024*1024,	64*1024 },
	{ "28F008SA-L",	FLII_INTEL_28F008SA_L,	1024*1024,	64*1024 },
	{ NULL },
};

struct flashmanu {
	char *name;
	u_char	manu;
	struct flashii *flashii;
} flashmanu[] = {
	{ "intel",	FLMANU_INTEL,		intel_flashii },
	{ NULL }
};

int
flashmatch(parent, cf, args)
	struct device *parent;
	void *cf;
	void *args;
{
	struct confargs *ca = args;

#ifdef MVME147
	if (cputyp == CPU_147)
		return (0);
#endif
#ifdef MVME167
	/*
	 * XXX: 166 has 4 byte-wide flash rams side-by-side, and
	 * isn't supported (yet).
	 */
	if (cputyp == CPU_166)
		return (0);
#endif

	if (badpaddr(ca->ca_paddr, 1))
		return (0);

	/*
	 * XXX: need to determine if it is flash or rom
	 */
	return (1);
}

void
flashattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct flashsoftc *sc = (struct flashsoftc *)self;
	struct confargs *ca = args;
	int manu, ident;

	sc->sc_paddr = ca->ca_paddr;
	sc->sc_vaddr = mapiodev(sc->sc_paddr, NBPG);

	switch (cputyp) {
#ifdef MVME162
	case CPU_162:
		mc_enableflashwrite(1);
		break;
#endif
	}

	/* read manufacturer and product identifier from flash */
	sc->sc_vaddr[0] = FLCMD_RESET;
	sc->sc_vaddr[0] = FLCMD_READII;
	sc->sc_manu = sc->sc_vaddr[0];
	sc->sc_ii = sc->sc_vaddr[1];
	sc->sc_vaddr[0] = FLCMD_RESET;

	for (manu = 0; flashmanu[manu].name; manu++)
		if (flashmanu[manu].manu == sc->sc_manu)
			break;
	if (flashmanu[manu].name == NULL) {
		printf(": unknown manu 0x%02x ident %02x\n",
		    sc->sc_manu, sc->sc_ii);
		return;
	}
	for (ident = 0; flashmanu[manu].flashii[ident].name; ident++)
		if (flashmanu[manu].flashii[ident].ii == sc->sc_ii)
			break;
	if (flashmanu[manu].flashii[ident].name == NULL) {
		printf(": unknown manu %s ident 0x%02x\n",
		    flashmanu[manu].name, sc->sc_ii);
		return;
	}
	sc->sc_len = flashmanu[manu].flashii[ident].size;
	sc->sc_zonesize = flashmanu[manu].flashii[ident].zonesize;
	printf(": %s %s len %d", flashmanu[manu].name,
	    flashmanu[manu].flashii[ident].name, sc->sc_len);

	unmapiodev(sc->sc_vaddr, NBPG);
	sc->sc_vaddr = mapiodev(sc->sc_paddr, sc->sc_len);
	if (sc->sc_vaddr == NULL) {
		sc->sc_len = 0;
		printf(" -- failed to map");
	}
	printf("\n"); 
}

/*ARGSUSED*/
int
flashopen(dev, flag, mode)
	dev_t dev;
	int flag, mode;
{
	if (minor(dev) >= flashcd.cd_ndevs ||
	    flashcd.cd_devs[minor(dev)] == NULL)
		return (ENODEV);
	return (0);
}

/*ARGSUSED*/
int
flashclose(dev, flag, mode)
	dev_t dev;
	int flag, mode;
{

	return (0);
}

/*ARGSUSED*/
int
flashioctl(dev, cmd, data, flag, p)
	dev_t   dev;
	caddr_t data;
	int     cmd, flag;
	struct proc *p;
{
	int unit = minor(dev);
	struct flashsoftc *sc = (struct flashsoftc *) flashcd.cd_devs[unit];
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
flashread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = minor(dev);
	struct flashsoftc *sc = (struct flashsoftc *) flashcd.cd_devs[unit];
	register vm_offset_t v;
	register int c;
	register struct iovec *iov;
	int error = 0;

	while (uio->uio_resid > 0 && error == 0) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("flashrw");
			continue;
		}

		v = uio->uio_offset;
		c = min(iov->iov_len, MAXPHYS);
		if (v + c > sc->sc_len)
			c = sc->sc_len - v;	/* till end of FLASH */
		if (c == 0)
			return (0);
		error = uiomove((caddr_t)sc->sc_vaddr + v, c, uio);
	}
	return (error);
}

/*ARGSUSED*/
int
flashwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	return (ENXIO);
}

int
flashmmap(dev, off, prot)
	dev_t dev;
	int off, prot;
{
	int unit = minor(dev);
	struct flashsoftc *sc = (struct flashsoftc *) flashcd.cd_devs[unit];

	/* allow access only in RAM */
	if (off > sc->sc_len)
		return (-1);
	return (m68k_btop(sc->sc_paddr + off));
}

int
flasherasezone(sc, addr)
	struct flashsoftc *sc;
	int addr;
{
	u_char	sr;

	sc->sc_vaddr[addr] = FLCMD_ESETUP;
	sc->sc_vaddr[addr] = FLCMD_ECONFIRM;

	/* XXX should use sleep/wakeup/timeout combination */
	do {
		sc->sc_vaddr[0] = FLCMD_READSTAT;
		sr = sc->sc_vaddr[0];
	} while (sr & FLSR_WSMS == 0);
	if (sr & FLSR_ES)
		return (-1);
	return (0);
}

/*
 * Should add some light retry code. If a write fails see if an
 * erase helps the situation... eventually flash rams become
 * useless but perhaps we can get just one more cycle out of it.
 */
int
flashwritebyte(sc, addr, val)
	struct flashsoftc *sc;
	int addr;
	u_char val;
{
	u_char sr;

	/*
	 * If a zero'd bit in the flash memory needs to become set,
	 * then the zone must be erased and rebuilt.
	 */
	if (val & ~sc->sc_vaddr[addr]) {
		int faddr = addr & ~(sc->sc_zonesize - 1);
		u_char *zone;
		int i;

		zone = (u_char *)malloc(sc->sc_zonesize, M_TEMP, M_WAITOK);
		if (!zone)
			return (-1);
		bcopy((caddr_t)&sc->sc_vaddr[faddr], zone, sc->sc_zonesize);

		if (flasherasezone(sc, faddr) == -1)
			return (-1);

		zone[addr - faddr] = val;
		for (i = 0; i < sc->sc_zonesize; i++) {
			sc->sc_vaddr[faddr + i] = FLCMD_WSETUP;
			sc->sc_vaddr[faddr + i] = zone[i];
			do {
				sc->sc_vaddr[0] = FLCMD_READSTAT;
				sr = sc->sc_vaddr[0];
			} while (sr & FLSR_WSMS == 0);
			if (sr & FLSR_BWS)
				return (-1);	/* write failed! */
		}
		free(zone, M_TEMP);
		return (0);
	}

	sc->sc_vaddr[addr] = FLCMD_WSETUP;
	sc->sc_vaddr[addr] = val;
	do {
		sc->sc_vaddr[0] = FLCMD_READSTAT;
		sr = sc->sc_vaddr[0];
	} while (sr & FLSR_WSMS == 0);
	if (sr & FLSR_BWS)
		return (-1);	/* write failed! */
	return (0);
}
