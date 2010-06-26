/*	$OpenBSD: fooip.c,v 1.10 2010/06/26 23:24:43 guenther Exp $ */

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

/*
 * A sample framework for writing an IP module driver.
 */
#include <sys/param.h>
#include <sys/conf.h>
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
#include <machine/cpu.h>
#include <mvme68k/dev/ipicreg.h>

struct fooipregs {
	volatile u_char		fooip_reg1;
	volatile u_char		fooip_vec;
};

struct fooipsoftc {
	struct device		sc_dev;
	struct ipicsoftc	*sc_ipicsc;
	struct intrhand		sc_ih;

	int			sc_slot;
	struct fooipregs	*sc_regs;
};

void fooipattach(struct device *, struct device *, void *);
int  fooipmatch(struct device *, void *, void *);

struct cfattach fooip_ca = {
	sizeof(struct fooipsoftc), fooipmatch, fooipattach
};

struct cfdriver fooip_cd = {
	NULL, "fooip", DV_DULL
};

int  fooipintr(void *);

int
fooipmatch(parent, cf, args)
	struct device *parent;
	void *cf;
	void *args;
{
	return (1);
}

void
fooipattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct fooipsoftc *sc = (struct fooipsoftc *)self;
	struct confargs *ca = args;

	sc->sc_ipicsc = (struct ipicsoftc *)parent;
	sc->sc_regs = (struct fooipregs *)(ca->ca_vaddr +
	    IPIC_IP_REGOFFSET);
	sc->sc_slot = ca->ca_offset;

	/* this device uses only one interrupt */
	sc->sc_ih.ih_fn = fooipintr;
	sc->sc_ih.ih_arg = sc;
	sc->sc_ih.ih_ipl = ca->ca_ipl;
	ipicintr_establish(ca->ca_vec, &sc->sc_ih, self->dv_xname);

	sc->sc_regs->fooip_vec = ca->ca_vec;
	sc->sc_ipicsc->sc_ipic->ipic_irq[sc->sc_slot][0] = ca->ca_ipl |
	    IPIC_IRQ_ICLR | IPIC_IRQ_IEN;

	printf("\n");
}

int
fooipintr(arg)
	void *arg;
{
	struct fooipsoftc *sc = arg;

	if (sc->sc_ipicsc->sc_ipic->ipic_irq[sc->sc_slot][0] & IPIC_IRQ_INT) {
		/* clear interrupt on device */
		return (1);
	}
	return (0);
}
