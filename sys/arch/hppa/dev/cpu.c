/*	$OpenBSD: cpu.c,v 1.2 1999/02/17 03:19:07 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
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
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <machine/cpufunc.h>
#include <machine/pdc.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>

#include <hppa/dev/cpudevs.h>

struct cpu_softc {
	struct  device sc_dev;

	hppa_hpa_t sc_hpa;
};

int	cpumatch __P((struct device *, void *, void *));
void	cpuattach __P((struct device *, struct device *, void *));

struct cfattach cpu_ca = {
	sizeof(struct cpu_softc), cpumatch, cpuattach
};

struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL
};

int
cpumatch(parent, cfdata, aux)   
	struct device *parent;
	void *cfdata;
	void *aux;
{
	struct confargs *ca = aux;
	struct cfdata *cf = cfdata;

	/* there will be only one for now XXX */
	if (cf->cf_unit > 0 ||
	    ca->ca_type.iodc_type != HPPA_TYPE_CPU)
		return 0;

	return 1;
}

void
cpuattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	extern u_int cpu_ticksnum, cpu_ticksdenom;
	/* register struct cpu_softc *sc = (struct cpu_softc *)self; */
	register struct confargs *ca = aux;
	struct pdc_cache pdc_cache PDC_ALIGNMENT;
	struct pdc_btlb pdc_btlb PDC_ALIGNMENT;
	u_int mhz;
	int pdcerr;
	char *p;

	/* this is finally from "HP-UX Multiprocessing rev 1.3" paper
	 * identifying PA revisions! */
	switch (ca->ca_type.iodc_sv_model) {
	case  0:  p = "1.0";	break;
	case  4:  p = "1.1";	break;
	case  8:  p = "2.0";	break;
	default:  p = "?.?";	break;
	}

	mhz = 100 * cpu_ticksnum / cpu_ticksdenom;
	printf(": PA%s %d", mhz / 100);
	if (mhz % 100 > 9)
		printf(".%02d", mhz % 100);

	printf(" MHz clock\n%s: ", self->dv_xname);

	/*
	 * get cache parameters from the PDC
	 */
	if ((pdcerr = pdc_call((iodcio_t)pdc, 0, PDC_CACHE, PDC_CACHE_DFLT,
			       &pdc_cache)) < 0) {
#ifdef DEBUG
                printf("Warning: PDC_CACHE call Ret'd %d\n", pdcerr);
#endif
	} else {
		if (pdc_cache.dc_conf.cc_sh)
			printf("%uK cache", pdc_cache.dc_size / 1024);
		else
			printf("%uK/%uK D/I caches",
			       pdc_cache.dc_size / 1024,
			       pdc_cache.ic_size / 1024);
		if (pdc_cache.dt_conf.tc_sh)
			printf(", %u shared TLB", pdc_cache.dt_size);
		else
			printf(", %u/%u D/I TLBs",
			       pdc_cache.dt_size, pdc_cache.it_size);
	}

	if ((pdcerr = pdc_call((iodcio_t)pdc, 0, PDC_BLOCK_TLB,
			       PDC_BTLB_DEFAULT, &pdc_btlb)) < 0) {
#ifdef DEBUG
                printf("Warning: PDC_BTLB call Ret'd %d\n", pdcerr);
#endif
	} else {
		if (pdc_btlb.finfo.num_c)
			printf(", %u shared BTLB", pdc_btlb.finfo.num_c);
		else {
			printf(", %u/%u D/I BTLBs",
			       pdc_btlb.finfo.num_i,
			       pdc_btlb.finfo.num_d);
		}
	}

	printf("\n");
}
