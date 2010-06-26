/*	$OpenBSD: lp.c,v 1.11 2010/06/26 23:24:43 guenther Exp $ */

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
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/conf.h>
#include <machine/cpu.h>

#include <mvme68k/dev/pccreg.h>

struct lpsoftc {
	struct device	sc_dev;
	struct intrhand	sc_ih;
};

void lpattach(struct device *, struct device *, void *);
int  lpmatch(struct device *, void *, void *);

struct cfattach lp_ca = {
	sizeof(struct lpsoftc), lpmatch, lpattach
};

struct cfdriver lp_cd = {
	NULL, "lp", DV_DULL
};

int lpintr(void *);

/*
 * a PCC chip always has an lp attached to it.
 */
int
lpmatch(parent, cf, args)
	struct device *parent;
	void *cf;
	void *args;
{
	return (1);
}

void
lpattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct lpsoftc *sc = (struct lpsoftc *)self;
	struct confargs *ca = args;

	printf(": unsupported\n");

	sc->sc_ih.ih_fn = lpintr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_ipl = ca->ca_ipl;
	pccintr_establish(PCCV_PRINTER, &sc->sc_ih, self->dv_xname);

	sys_pcc->pcc_lpirq = ca->ca_ipl | PCC_IRQ_IEN | PCC_LPIRQ_ACK;
}

int
lpintr(dev)
	void *dev;
{
	return (0);
}

/*ARGSUSED*/
int
lpopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{

	return (0);
}

/*ARGSUSED*/
int
lpclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{

	return (0);
}

/*ARGSUSED*/
int
lpwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	return (EOPNOTSUPP);
}

int
lpioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	return (EOPNOTSUPP);
}

