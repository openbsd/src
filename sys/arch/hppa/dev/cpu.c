/*	$OpenBSD: cpu.c,v 1.4 1999/08/14 03:58:55 mickey Exp $	*/

/*
 * Copyright (c) 1998,1999 Michael Shalayeff
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
	void *sc_ih;
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
	/* probe any 1.0, 1.1 or 2.0 */
	if (cf->cf_unit > 0 ||
	    ca->ca_type.iodc_type != HPPA_TYPE_NPROC ||
	    ca->ca_type.iodc_sv_model != HPPA_NPROC_HPPA)
		return 0;

	return 1;
}

void
cpuattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
{
	/* machdep.c */
	extern struct pdc_cache pdc_cache;
	extern struct pdc_btlb pdc_btlb;
	extern u_int cpu_ticksnum, cpu_ticksdenom;

	struct pdc_model pdc_model PDC_ALIGNMENT;
	struct pdc_cpuid pdc_cpuid PDC_ALIGNMENT;
	u_int pdc_cversion[32] PDC_ALIGNMENT;
	register struct cpu_softc *sc = (struct cpu_softc *)self;
	register struct confargs *ca = aux;
	const char *p = NULL;
	u_int mhz = 100 * cpu_ticksnum / cpu_ticksdenom;
	int err;

	bzero (&pdc_cpuid, sizeof(pdc_cpuid));
	if (pdc_call((iodcio_t)pdc, 0, PDC_MODEL, PDC_MODEL_CPUID,
		     &pdc_cpuid, sc->sc_dev.dv_unit, 0, 0, 0) >= 0) {

		/* patch for old 8200 */
		if (pdc_cpuid.version == HPPA_CPU_PCXUP &&
		    pdc_cpuid.revision > 0x0d)
			pdc_cpuid.version = HPPA_CPU_PCXUP1;
			
		p = hppa_mod_info(HPPA_TYPE_CPU, pdc_cpuid.version);
	}
	/* otherwise try to guess on component version numbers */
	else if (pdc_call((iodcio_t)pdc, 0, PDC_MODEL, PDC_MODEL_COMP,
		     &pdc_cversion, sc->sc_dev.dv_unit) >= 0) {
		/* XXX p = hppa_mod_info(HPPA_TYPE_CPU,pdc_cversion[0]); */
	}

	printf (": %s v%d.%d, ", p? p : "PA7000",
		pdc_cpuid.revision >> 4, pdc_cpuid.revision & 0xf);

	if ((err = pdc_call((iodcio_t)pdc, 0, PDC_MODEL, PDC_MODEL_INFO,
			    &pdc_model)) < 0) {
#ifdef DEBUG
		printf("WARNING: PDC_MODEL failed (%d)\n", err);
#endif
	} else {
		static const char lvls[4][4] = { "0", "1", "1.5", "2" };

		printf("level %s, category %c, ",
		       lvls[pdc_model.pa_lvl], "AB"[pdc_model.mc]);
	}

	printf ("%d", mhz / 100);
	if (mhz % 100 > 9)
		printf(".%02d", mhz % 100);

	printf(" MHz clock\n%s: %s", self->dv_xname,
	       pdc_model.sh? "shadows, ": "");

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

	if (pdc_btlb.finfo.num_c)
		printf(", %u shared BTLB", pdc_btlb.finfo.num_c);
	else {
		printf(", %u/%u D/I BTLBs",
		       pdc_btlb.finfo.num_i,
		       pdc_btlb.finfo.num_d);
	}

	printf("\n");

	if (ca->ca_irq == 31) {
		sc->sc_ih = cpu_intr_establish(IPL_CLOCK, ca->ca_irq,
					       clock_intr, NULL /*trapframe*/,
					       sc->sc_dev.dv_xname);
	} else {
		printf ("%s: bad irq number %d\n", sc->sc_dev.dv_xname,
			ca->ca_irq);
	}
}
