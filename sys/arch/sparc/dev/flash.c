/*	$OpenBSD: flash.c,v 1.7 2015/02/10 23:50:29 miod Exp $	*/

/*
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Driver for the flash memory found on FORCE CPU-5V boards.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/syslog.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>
#include <sparc/cpu.h>
#include <sparc/sparc/cpuvar.h>

int	flashmatch(struct device *, void *, void *);
void	flashattach(struct device *, struct device *, void *);

int	flashopen(dev_t, int, int, struct proc *p);
int	flashclose(dev_t, int, int, struct proc *p);
int	flashread(dev_t, struct uio *, int);
int	flashwrite(dev_t, struct uio *, int);
int	flashrw(dev_t, struct uio *, int);
int	flashioctl(dev_t, u_long, caddr_t, int, struct proc *);

/*
 * We see the flash-memory in 512k windows.  The current window is
 * changed in the sysconfig registers (FMPCR1), see scf.c.
 */
#define	FLASH_REGS_SIZE		0x80000

struct flash_regs {
	u_int8_t		regs[0x80000];
};

struct flash_softc {
	struct device		sc_dv;
	struct flash_regs	*sc_regs;
	int			sc_node;
	int			sc_open;
};

struct cfattach flash_ca = {
	sizeof (struct flash_softc), flashmatch, flashattach
};

struct cfdriver flash_cd = {
	NULL, "flash", DV_DULL
};

int
flashmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	if (strcmp("flash-memory", ra->ra_name))
		return (0);
	return (1);
}

void    
flashattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct confargs *ca = aux;
	struct flash_softc *sc = (struct flash_softc *)self;

	/* map registers */
	if (ca->ca_ra.ra_nreg != 1) {
		printf(": expected 1 register, got %d\n", ca->ca_ra.ra_nreg);
		return;
	}
	sc->sc_regs = mapiodev(&(ca->ca_ra.ra_reg[0]), 0,
			ca->ca_ra.ra_reg[0].rr_len);

	sc->sc_node = ca->ca_ra.ra_node;

	printf(": window 0x%x\n", ca->ca_ra.ra_reg[0].rr_len);
}

int
flashopen(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	struct flash_softc *sc;
	int card = 0;

	if (card >= flash_cd.cd_ndevs)
		return (ENXIO);
	sc = flash_cd.cd_devs[card];
	if (sc == NULL)
		return (ENXIO);
	if (sc->sc_open)
		return (EBUSY);
	sc->sc_open = 1;
	return (0);
}

int
flashclose(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	struct flash_softc *sc = flash_cd.cd_devs[0];
	sc->sc_open = 0;
	return (0);
}

int
flashwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	return (flashrw(dev, uio, flags));
}

int
flashread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	return (flashrw(dev, uio, flags));
}

int
flashrw(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	struct flash_softc *sc = flash_cd.cd_devs[0];
	size_t cnt;
	off_t off;

	off = uio->uio_offset;
	if (off < 0 || off >= FLASH_REGS_SIZE)
		return (EFAULT);

	cnt = uio->uio_resid;
	if (cnt > (FLASH_REGS_SIZE - off))
		cnt = FLASH_REGS_SIZE - off;

	return (uiomove(&sc->sc_regs->regs[0] + off, cnt, uio));
}

int
flashioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	return (EINVAL);
}
