/*	$OpenBSD: lp.c,v 1.4 2000/03/26 23:31:59 deraadt Exp $ */

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
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/device.h>
#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <mvme68k/dev/pccreg.h>

struct lpsoftc {
	struct device	sc_dev;
	struct intrhand	sc_ih;
	struct pccreg	*sc_pcc;
};

void lpattach __P((struct device *, struct device *, void *));
int  lpmatch __P((struct device *, void *, void *));

struct cfattach lp_ca = {
	sizeof(struct lpsoftc), lpmatch, lpattach
};

struct cfdriver lp_cd = {
	NULL, "lp", DV_DULL, 0
};

int lpintr __P((void *));

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

	sc->sc_pcc = (struct pccreg *)ca->ca_master;

	printf(": unsupported\n");

	sc->sc_ih.ih_fn = lpintr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_ipl = ca->ca_ipl;
	pccintr_establish(PCCV_PRINTER, &sc->sc_ih);

	sc->sc_pcc->pcc_lpirq = ca->ca_ipl | PCC_IRQ_IEN | PCC_LPIRQ_ACK;
}

int
lpintr(dev)
	void *dev;
{
	struct lpsoftc *sc = dev; 

	return (0);
}

/*ARGSUSED*/
int
lpopen(dev, flag, mode)
	dev_t dev;
	int flag, mode;
{

	return (0);
}

/*ARGSUSED*/
int
lpclose(dev, flag, mode)
	dev_t dev;
	int flag, mode;
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
}

lpioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
}

