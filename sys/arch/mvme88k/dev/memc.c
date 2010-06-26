/*	$OpenBSD: memc.c,v 1.14 2010/06/26 23:24:43 guenther Exp $ */

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
 * MEMC/MCECC chips
 * these chips are rather similar in appearance except that the MEMC
 * does parity while the MCECC does ECC.
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

#include <dev/cons.h>

#include <mvme88k/dev/memcreg.h>

struct memcsoftc {
	struct device	sc_dev;
	struct memcreg *sc_memc;
	struct intrhand	sc_ih;
};

void memcattach(struct device *, struct device *, void *);
int  memcmatch(struct device *, void *, void *);

struct cfattach memc_ca = {
	sizeof(struct memcsoftc), memcmatch, memcattach
};

struct cfdriver memc_cd = {
	NULL, "memc", DV_DULL
};

#if 0
int memcintr(void *);
#endif

int
memcmatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct confargs *ca = args;
	struct memcreg *memc = (struct memcreg *)ca->ca_paddr;

	if (badaddr((vaddr_t)memc, 4))
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

	sc->sc_memc = (struct memcreg *)ca->ca_paddr;

	printf(": %s rev %d",
	    (sc->sc_memc->memc_chipid == MEMC_CHIPID) ? "MEMC040" : "MCECC",
	    sc->sc_memc->memc_chiprev);

#if 0
	sc->sc_ih.ih_fn = memcintr;
	sc->sc_ih.ih_arg = 0;
	sc->sc_ih.ih_wantframe = 1;
	sc->sc_ih.ih_ipl = 7;
	mcintr_establish(xxx, &sc->sc_ih, self->dv_xname);
#endif

	switch (sc->sc_memc->memc_chipid) {
	case MEMC_CHIPID:
		break;
	case MCECC_CHIPID:
		break;
	}

	printf("\n");
}

#if 0
int
memcintr(eframe)
	void *eframe;
{
	return (0);
}
#endif
