/*	$OpenBSD: cpu.c,v 1.20 2002/12/08 17:21:43 mickey Exp $	*/

/*
 * Copyright (c) 1998-2002 Michael Shalayeff
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
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF MIND,
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
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
#include <machine/reg.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>

#include <hppa/dev/cpudevs.h>

struct cpu_softc {
	struct  device sc_dev;

	hppa_hpa_t sc_hpa;
	void *sc_ih;
};

int	cpumatch(struct device *, void *, void *);
void	cpuattach(struct device *, struct device *, void *);

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

int
cpu_hardclock(void *v)
{
	hardclock(v);
	return (1);
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
	extern u_int fpu_enable;

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

	printf (": %s ", p? p : cpu_typename);
	if (sc->sc_dev.dv_xname)
		(*cpu_desidhash)();

	if ((err = pdc_call((iodcio_t)pdc, 0, PDC_MODEL, PDC_MODEL_INFO,
			    &pdc_model)) < 0) {
#ifdef DEBUG
		printf("PDC_MODEL(%d) ", err);
#endif
	} else {
		static const char lvls[4][4] = { "0", "1", "1.5", "2" };

		printf("L%s-%c ", lvls[pdc_model.pa_lvl], "AB"[pdc_model.mc]);
	}

	printf ("%d", mhz / 100);
	if (mhz % 100 > 9)
		printf(".%02d", mhz % 100);
	printf("MHz, ");

	if (fpu_enable) {
		u_int32_t ver[2];

		mtctl(fpu_enable, CR_CCR);
		__asm volatile(
		    "fstds   %%fr0,0(%0)\n\t"
		    "copr,0,0\n\t"
		    "fstds   %%fr0,0(%0)"
		    :: "r" (&ver) : "memory");
		mtctl(0, CR_CCR);
		ver[0] = HPPA_FPUVER(ver[0]);
		printf("FPU %s rev %d",
		    hppa_mod_info(HPPA_TYPE_FPU, ver[0] >> 5), ver[0] & 0x1f);
	}

	/* if (pdc_model.sh)
		printf("shadows, "); */

	printf("\n%s: ", self->dv_xname);
	p = "";
	if (!pdc_cache.dc_conf.cc_sh) {
		printf("%uK(%db/l) Icache, ",
		    pdc_cache.ic_size / 1024, pdc_cache.ic_conf.cc_line * 16);
		p = "D";
	}
	/* TODO decode associativity */
	printf("%uK(%db/l) wr-%s %scoherent %scache, ",
	    pdc_cache.dc_size / 1024, pdc_cache.dc_conf.cc_line * 16,
	    pdc_cache.dc_conf.cc_wt? "thru" : "back",
	    pdc_cache.dc_conf.cc_cst? "" : "in", p);

	p = "";
	if (!pdc_cache.dt_conf.tc_sh) {
		printf("%u ITLB, ", pdc_cache.it_size);
		p = "D";
	}
	printf("%u %scoherent %sTLB",
	    pdc_cache.dt_size, pdc_cache.dt_conf.tc_cst? "" : "in", p);

	if (pdc_btlb.finfo.num_c)
		printf(", %u BTLB\n", pdc_btlb.finfo.num_c);
	else
		printf(", %u/%u D/I BTLBs\n",
		    pdc_btlb.finfo.num_i, pdc_btlb.finfo.num_d);

	/* sanity against lusers amongst config editors */
	if (ca->ca_irq == 31)
		sc->sc_ih = cpu_intr_establish(IPL_CLOCK, ca->ca_irq,
		    cpu_hardclock, NULL /*frame*/, &sc->sc_dev);
	else
		printf ("%s: bad irq %d\n", sc->sc_dev.dv_xname, ca->ca_irq);
}
