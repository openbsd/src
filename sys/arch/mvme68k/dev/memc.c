/*	$OpenBSD: memc.c,v 1.5 2000/03/26 23:31:59 deraadt Exp $ */

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

/*
 * MEMC/MCECC chips
 * these chips are rather similar in appearance except that the MEMC
 * does parity while the MCECC does ECC.
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
#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <dev/cons.h>

#include <mvme68k/dev/memcreg.h>

struct memcsoftc {
	struct device	sc_dev;
	void *		sc_vaddr;
	struct memcreg *sc_memc;
	struct intrhand	sc_ih;
};

void memcattach __P((struct device *, struct device *, void *));
int  memcmatch __P((struct device *, void *, void *));

struct cfattach memc_ca = {
	sizeof(struct memcsoftc), memcmatch, memcattach
};

struct cfdriver memc_cd = {
	NULL, "memc", DV_DULL, 0
};

int memcintr __P((struct frame *frame));

int
memcmatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = args;
	struct memcreg *memc = (struct memcreg *)ca->ca_vaddr;

	if (badvaddr(memc, 1))
		return (0);
	if (memc->memc_chipid==MEMC_CHIPID || memc->memc_chipid==MCECC_CHIPID)
		return (1);
	return (0);
}

void
memcattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct confargs *ca = args;
	struct memcsoftc *sc = (struct memcsoftc *)self;

	/*
	 * since we know ourself to land in intiobase land,
	 * we must adjust our address
	 */
	sc->sc_memc = (struct memcreg *)ca->ca_vaddr;

	printf(": %s rev %d",
	    (sc->sc_memc->memc_chipid == MEMC_CHIPID) ? "MEMC040" : "MCECC",
	    sc->sc_memc->memc_chiprev);

#if 0
	sc->sc_ih.ih_fn = memcintr;
	sc->sc_ih.ih_arg = 0;
	sc->sc_ih.ih_ipl = 7;
	sc->sc_ih.ih_wantframe = 1;
	mcintr_establish(xxx, &sc->sc_ih);
#endif

	switch (sc->sc_memc->memc_chipid) {
	case MEMC_CHIPID:
		break;
	case MCECC_CHIPID:
		break;
	}

	printf("\n");
}

int
memcintr(frame)
	struct frame *frame;
{
	return (0);
}
