/*	$OpenBSD: busswitch.c,v 1.1 1999/09/27 18:43:22 smurph Exp $ */

/*
 * Copyright (c) 1999 Steve Murphree, Jr.
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
 *	This product includes software developed under OpenBSD by
 *	Theo de Raadt for Willowglen Singapore.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <machine/psl.h>
#include <machine/autoconf.h>
#include <machine/bugio.h>
#include <machine/cpu.h>
#include <machine/mioctl.h>
#include <machine/vmparam.h>

#include <mvme88k/dev/busswitchreg.h>

struct busswitchsoftc {
	struct device		sc_dev;
	void *		sc_paddr;
	void *		sc_vaddr;
	int		sc_len;
   struct busswitchreg * sc_busswitch;
};

void	busswitchattach	__P((struct device *, struct device *, void *));
int	busswitchmatch __P((struct device *, void *, void *));

struct cfattach busswitch_ca = { 
        sizeof(struct busswitchsoftc), busswitchmatch, busswitchattach
}; 
 
struct cfdriver busswitch_cd = {
        NULL, "busswitch", DV_DULL, 0
};

int
busswitchmatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = args;
   struct busswitchreg *busswitch;
   /* Don't match if wrong cpu */
	if (cputyp != CPU_197) return (0);
	
   busswitch = (struct busswitchreg *)(IIOV(ca->ca_paddr));
	if (badvaddr(busswitch, 4) <= 0){
	    printf("==> busswitch: failed address check.\n");
	    return (0);
	}
	return (1);
}

void
busswitchattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct confargs *ca = args;
	struct busswitchsoftc	*sc = (struct busswitchsoftc *)self;

	sc->sc_paddr = ca->ca_paddr;
	sc->sc_vaddr = ca->ca_vaddr;

	/*
   printf(": rev %d\n", sc->sc_busswitch->chiprev);
   */
   printf(": rev %d\n", 0);
}

int
busswitch_print(args, bus)
	void *args;
	const char *bus;
{
	struct confargs *ca = args;

	if (ca->ca_offset != -1)
		printf(" offset 0x%x", ca->ca_offset);
	if (ca->ca_ipl > 0)
		printf(" ipl %d", ca->ca_ipl);
	return (UNCONF);
}

int
busswitch_scan(parent, child, args)
	struct device *parent;
	void *child, *args;
{
	struct cfdata *cf = child;
	struct busswitchsoftc *sc = (struct busswitchsoftc *)parent;
	struct confargs *ca = args;
	struct confargs oca;

	if (parent->dv_cfdata->cf_driver->cd_indirect) {
      printf(" indirect devices not supported\n");
      return 0;
   }

	bzero(&oca, sizeof oca);
	oca.ca_offset = cf->cf_loc[0];
	oca.ca_ipl = cf->cf_loc[1];
	if ((oca.ca_offset != (void*)-1) && ISIIOVA(sc->sc_vaddr + oca.ca_offset)) {
		oca.ca_vaddr = sc->sc_vaddr + oca.ca_offset;
		oca.ca_paddr = sc->sc_paddr + oca.ca_offset;
	} else {
		oca.ca_vaddr = (void *)-1;
		oca.ca_paddr = (void *)-1;
	}
	oca.ca_bustype = BUS_BUSSWITCH;
	oca.ca_master = (void *)sc->sc_busswitch;
	oca.ca_name = cf->cf_driver->cd_name;
	if ((*cf->cf_attach->ca_match)(parent, cf, &oca) == 0)
		return (0);
	config_attach(parent, cf, &oca, busswitch_print);
	return (1);
}

