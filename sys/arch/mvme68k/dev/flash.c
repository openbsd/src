/*	$OpenBSD: flash.c,v 1.22 2010/12/26 15:40:59 miod Exp $ */

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
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/conf.h>
#include <machine/cpu.h>
#include <machine/mioctl.h>

#include "mc.h"

#if NMC > 0
#include <mvme68k/dev/mcreg.h>
#endif

#include <mvme68k/dev/flashreg.h>

struct flashsoftc {
	struct device	 sc_dev;
	paddr_t		 sc_paddr;
	volatile u_char *sc_vaddr;
	u_char		 sc_manu;
	u_char		 sc_ii;
	int		 sc_len;
	int		 sc_zonesize;
};

void flashattach(struct device *, struct device *, void *);
int  flashmatch(struct device *, void *, void *);

struct cfattach flash_ca = {
	sizeof(struct flashsoftc), flashmatch, flashattach
};

struct cfdriver flash_cd = {
	NULL, "flash", DV_DULL
};

int flashwritebyte(struct flashsoftc *sc, int addr, u_char val);
int flasherasezone(struct flashsoftc *sc, int addr);
u_char *flashsavezone(struct flashsoftc *, int);
int flashwritezone(struct flashsoftc *, u_char *, int);

struct flashii intel_flashii[] = {
	{ "28F008SA",	FLII_INTEL_28F008SA,	1024*1024,	64*1024 },
	{ "28F008SA-L",	FLII_INTEL_28F008SA_L,	1024*1024,	64*1024 },
	{ "28F016SA",	FLII_INTEL_28F016SA,	1024*1024,	64*1024 },
	{ NULL },
};

struct flashmanu {
	char *name;
	u_char	manu;
	struct flashii *flashii;
} flashmanu[] = {
	{ "intel", FLMANU_INTEL, intel_flashii },
	{ NULL, 0, NULL }
};

int
flashmatch(parent, cf, args)
	struct device *parent;
	void *cf;
	void *args;
{
	struct confargs *ca = args;

	switch (cputyp) {
#ifdef MVME147
	case CPU_147:
		return (0);
#endif
#ifdef MVME165
	case CPU_165:
		return (0);
#endif
#ifdef MVME167
	case CPU_166:
	case CPU_167:
		/*
		 * XXX: 166 has 4 byte-wide flash rams side-by-side, and
		 * isn't supported (yet).
		 */
		return (0);
#endif
#ifdef MVME177
	case CPU_176:
	case CPU_177:
		/*
		 * XXX: 177 has no flash.
		 */
		return (0);
#endif
#if defined(MVME162) || defined(MVME172)
	case CPU_162:
	case CPU_172:
		if (badpaddr(ca->ca_paddr, 1))
			return (0);

		if (!mc_hasflash())
			return 0;
		return (1);
#endif
	default:
		return (0);
	}
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
	sc->sc_vaddr = (volatile u_char *)mapiodev(sc->sc_paddr, NBPG);

	switch (cputyp) {
#ifdef MVME162
	case CPU_162:
		mc_enableflashwrite(1);
		break;
#endif
#ifdef MVME172
	case CPU_172:
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

	sc->sc_vaddr[0] = FLCMD_CLEARSTAT;
	sc->sc_vaddr[0] = FLCMD_RESET;

	unmapiodev((vaddr_t)sc->sc_vaddr, NBPG);
	sc->sc_vaddr = (volatile u_char *)mapiodev(sc->sc_paddr, sc->sc_len);
	if (sc->sc_vaddr == NULL) {
		sc->sc_len = 0;
		printf(" -- failed to map");
	}
	printf("\n");
}

u_char *
flashsavezone(sc, start)
	struct flashsoftc *sc;
	int start;
{
	u_char *zone;

	zone = (u_char *)malloc(sc->sc_zonesize, M_TEMP, M_WAITOK);
	sc->sc_vaddr[0] = FLCMD_RESET;
	bcopy((u_char *)&sc->sc_vaddr[start], zone, sc->sc_zonesize);
	return (zone);
}

int
flashwritezone(sc, zone, start)
	struct flashsoftc *sc;
	u_char *zone;
	int start;
{
	u_char sr;
	int i;

	for (i = 0; i < sc->sc_zonesize; i++) {
		if (zone[i] == 0xff)
			continue;
		sc->sc_vaddr[start + i] = FLCMD_WSETUP;
		sc->sc_vaddr[start + i] = zone[i];
		do {
			sc->sc_vaddr[0] = FLCMD_READSTAT;
			sr = sc->sc_vaddr[0];
		} while ((sr & FLSR_WSMS) == 0);
		if (sr & FLSR_BWS)
			return (i);	/* write failed on this byte! */
		sc->sc_vaddr[0] = FLCMD_RESET;
	}
	free(zone, M_TEMP);
	return (0);
}

int
flasherasezone(sc, addr)
	struct flashsoftc *sc;
	int addr;
{
	u_char	sr;

	printf("erasing zone at %d\n", addr);

	sc->sc_vaddr[addr] = FLCMD_ESETUP;
	sc->sc_vaddr[addr] = FLCMD_ECONFIRM;

	sc->sc_vaddr[0] = FLCMD_READSTAT;
	sr = sc->sc_vaddr[0];
	while ((sr & FLSR_WSMS) == 0) {
		sc->sc_vaddr[0] = FLCMD_READSTAT;
		sr = sc->sc_vaddr[0];
	}
	printf("sr=%2x\n", sr);

	sc->sc_vaddr[0] = FLCMD_RESET;
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

	sc->sc_vaddr[addr] = FLCMD_CLEARSTAT;
	sr = sc->sc_vaddr[0];
	sc->sc_vaddr[addr] = FLCMD_WSETUP;
	sc->sc_vaddr[addr] = val;
	delay(9);
	do {
		sr = sc->sc_vaddr[addr];
	} while ((sr & FLSR_WSMS) == 0);
	printf("write status %2x\n", sr);

	sc->sc_vaddr[0] = FLCMD_RESET;
	if (sr & FLSR_BWS)
		return (-1);	/* write failed! */
	return (0);
}


/*ARGSUSED*/
int
flashopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{

	if (minor(dev) >= flash_cd.cd_ndevs ||
	    flash_cd.cd_devs[minor(dev)] == NULL)
		return (ENODEV);

	return (0);
}

/*ARGSUSED*/
int
flashclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{

	return (0);
}

/*ARGSUSED*/
int
flashioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = minor(dev);
	struct flashsoftc *sc = (struct flashsoftc *) flash_cd.cd_devs[unit];
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
	struct flashsoftc *sc = (struct flashsoftc *) flash_cd.cd_devs[unit];
	vaddr_t v;
	int c;
	struct iovec *iov;
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
		error = uiomove((u_char *)sc->sc_vaddr + v, c, uio);
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
	int unit = minor(dev);
	struct flashsoftc *sc = (struct flashsoftc *) flash_cd.cd_devs[unit];
	vaddr_t v;
	int c, i, r;
	struct iovec *iov;
	int error = 0;
	u_char *cmpbuf;
	int neederase = 0, needwrite = 0;
	int zonestart, zoneoff;

	cmpbuf = (u_char *)malloc(sc->sc_zonesize, M_TEMP, M_WAITOK);

	while (uio->uio_resid > 0 && error == 0) {
		iov = uio->uio_iov;
		if (iov->iov_len == 0) {
			uio->uio_iov++;
			uio->uio_iovcnt--;
			if (uio->uio_iovcnt < 0)
				panic("flashrw");
			continue;
		}

		/* 
		 * constrain to be at most a zone in size, and
		 * aligned to be within that one zone only.
		*/
		v = uio->uio_offset;
		zonestart = v & ~(sc->sc_zonesize - 1);
		zoneoff = v & (sc->sc_zonesize - 1);
		c = min(iov->iov_len, MAXPHYS);
		if (v + c > sc->sc_len)
			c = sc->sc_len - v;	/* till end of FLASH */
		if (c > sc->sc_zonesize - zoneoff)
			c = sc->sc_zonesize - zoneoff; /* till end of zone */
		if (c == 0)
			return (0);
		error = uiomove((u_char *)cmpbuf, c, uio);

		/*
		 * compare to see if we are going to need a block erase
		 * operation.
		 */
		sc->sc_vaddr[0] = FLCMD_RESET;
		for (i = 0; i < c; i++) {
			u_char x = sc->sc_vaddr[v + i];
			if (cmpbuf[i] & ~x)
				neederase = 1;
			if (cmpbuf[i] != x)
				needwrite = 1;
		}
		if (needwrite && !neederase) {
			/*
			 * we don't need to erase. all the bytes being
			 * written (thankfully) set bits.
			 */
			for (i = 0; i < c; i++) {
				if (cmpbuf[i] == sc->sc_vaddr[v + i])
					continue;
				r = flashwritebyte(sc, v + i, cmpbuf[i]);
				if (r == 0)
					continue;
				/*
				 * this doesn't make sense. we
				 * thought we didn't need to erase,
				 * but a write failed. let's try an
				 * erase operation..
				 */
				printf("%s: failed write at %d, trying erase\n",
				    sc->sc_dev.dv_xname, i);
				goto tryerase;
			}
		} else if (neederase) {
			u_char *mem;

tryerase:
			mem = flashsavezone(sc, zonestart);
			for (i = 0; i < c; i++)
				mem[zoneoff + i] = cmpbuf[i];
			flasherasezone(sc, zonestart);
			r = flashwritezone(sc, mem, zonestart);
			if (r) {
				printf("%s: failed at offset %x\n",
				    sc->sc_dev.dv_xname, r);
				free(mem, M_TEMP);
				error = EIO;
			}
		}
	}

	free(cmpbuf, M_TEMP);
	return (error);
}

paddr_t
flashmmap(dev, off, prot)
	dev_t dev;
	off_t off;
	int prot;
{
	int unit = minor(dev);
	struct flashsoftc *sc = (struct flashsoftc *) flash_cd.cd_devs[unit];

	/* allow access only in RAM */
	if (off < 0 || off >= round_page(sc->sc_len))
		return (-1);
	return (sc->sc_paddr + off);
}
