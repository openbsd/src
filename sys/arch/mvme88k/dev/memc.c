/*	$NetBSD$ */

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
 *      This product includes software developed by Theo de Raadt
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
 * MEMC chip
 * XXX:
 * the databooks say that you should only ever access the higher-numbered
 * MEMC chip's control & status registers. this is strange. I disobey the
 * rules, hopefully there won't be any spanking.
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
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/device.h>
#include <machine/cpu.h>
#include <machine/autoconf.h>
#include <dev/cons.h>

#include <mvme88k/dev/memcreg.h>

struct memcsoftc {
	struct device	sc_dev;
	caddr_t		sc_vaddr;
	struct memcreg *sc_memc;
	struct intrhand	sc_ih;
};

void memcattach __P((struct device *, struct device *, void *));
int  memcmatch __P((struct device *, void *, void *));

struct cfdriver memccd = {
	NULL, "memc", memcmatch, memcattach,
	DV_DULL, sizeof(struct memcsoftc), 0
};

int
memcmatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = args;
	struct memcreg *memc = (struct memcreg *)ca->ca_vaddr;

	if (badvaddr(memc, 4) || memc->memc_chipid != MEMC_CHIPID)
		return (0);
	return (1);
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
	sc->sc_memc = (struct memcreg *)sc->sc_vaddr;

	printf(": rev %d, unsupported\n", sc->sc_memc->memc_chiprev);

#if 0
	sc->sc_nmiih.ih_fn = memcabort;
	sc->sc_nmiih.ih_arg = 0;
	sc->sc_nmiih.ih_ipl = 7;
	sc->sc_nmiih.ih_wantframe = 1;
	mcintr_establish(xxx, &sc->sc_nmiih);
#endif
}
