/*	$OpenBSD: cpu.c,v 1.5 2011/04/07 13:13:01 jsing Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>

#include <machine/pdc.h>
#include <machine/reg.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>

#include <arch/hppa/dev/cpudevs.h>

struct cpu_softc {
	struct device sc_dev;
};

int	cpumatch(struct device *, void *, void *);
void	cpuattach(struct device *, struct device *, void *);

int	cpu_hardclock(void *);

struct cfattach cpu_ca = {
	sizeof(struct cpu_softc), cpumatch, cpuattach
};

struct cfdriver cpu_cd = {
	NULL, "cpu", DV_DULL
};

int
cpumatch(struct device *parent, void *cfdata, void *aux)
{
	struct cfdata *cf = cfdata;
	struct confargs *ca = aux;

	if (ca->ca_type.iodc_type != HPPA_TYPE_NPROC ||
	    ca->ca_type.iodc_sv_model != HPPA_NPROC_HPPA)
		return 0;

	if (cf->cf_unit >= MAXCPUS)
		return 0;

	return 1;
}

int
cpu_hardclock(void *v)
{
	struct cpu_info *ci = curcpu();
	u_long __itmr, delta, eta;
	extern u_long cpu_hzticks;
	register_t eiem;
	int wrap;

	/*
	 * Invoke hardclock as many times as there has been cpu_hzticks
	 * ticks since the last interrupt.
	 */
	for (;;) {
		__itmr = mfctl(CR_ITMR);
		delta = __itmr - ci->ci_itmr;
		if (delta >= cpu_hzticks) {
			hardclock(v);
			ci->ci_itmr += cpu_hzticks;
		} else
			break;
	}

	/*
	 * Program the next clock interrupt, making sure it will
	 * indeed happen in the future. This is done with interrupts
	 * disabled to avoid a possible race.
	 */
	eta = ci->ci_itmr + cpu_hzticks;
	wrap = eta < ci->ci_itmr;	/* watch out for a wraparound */
	__asm __volatile("mfctl	%%cr15, %0": "=r" (eiem));
	__asm __volatile("mtctl	%r0, %cr15");
	mtctl(eta, CR_ITMR);
	__itmr = mfctl(CR_ITMR);

	/*
	 * If we were close enough to the next tick interrupt
	 * value, by the time we have programmed itmr, it might
	 * have passed the value, which would cause a complete
	 * cycle until the next interrupt occurs. On slow
	 * models, this would be a disaster (a complete cycle
	 * taking over two minutes on a 715/33).
	 *
	 * We expect that it will only be necessary to postpone
	 * the interrupt once. Thus, there are two cases:
	 * - We are expecting a wraparound: eta < cpu_itmr.
	 *   itmr is in tracks if either >= cpu_itmr or < eta.
	 * - We are not wrapping: eta > cpu_itmr.
	 *   itmr is in tracks if >= cpu_itmr and < eta (we need
	 *   to keep the >= cpu_itmr test because itmr might wrap
	 *   before eta does).
	 */
	if ((wrap && !(eta > __itmr || __itmr >= ci->ci_itmr)) ||
	    (!wrap && !(eta > __itmr && __itmr >= ci->ci_itmr))) {
		eta += cpu_hzticks;
		mtctl(eta, CR_ITMR);
	}
	__asm __volatile("mtctl	%0, %%cr15":: "r" (eiem));

	return (1);
}

void
cpuattach(struct device *parent, struct device *self, void *aux)
{
	/* machdep.c */
	extern struct pdc_model pdc_model;
	extern struct pdc_cache pdc_cache;
	extern u_int cpu_ticksnum, cpu_ticksdenom;
	extern u_int fpu_enable;

	/* struct cpu_softc *sc = (struct cpu_softc *)self; */
	/* struct confargs *ca = aux; */
	u_int mhz = 100 * cpu_ticksnum / cpu_ticksdenom;
	const char *p;

	printf (": %s ", cpu_typename);
	if (pdc_model.hvers) {
		static const char lvls[4][4] = { "0", "1", "1.5", "2" };

		printf("L%s-%c ", lvls[pdc_model.pa_lvl], "AB"[pdc_model.mc]);
	}

	printf ("%d", mhz / 100);
	if (mhz % 100 > 9)
		printf(".%02d", mhz % 100);
	printf("MHz");

	if (fpu_enable) {
		extern u_int fpu_version;
		u_int32_t ver[2];

		mtctl(fpu_enable, CR_CCR);
		__asm volatile(
		    "fstds   %%fr0,0(%0)\n\t"
		    "copr,0,0\n\t"
		    "fstds   %%fr0,0(%0)"
		    :: "r" (&ver) : "memory");
		mtctl(0, CR_CCR);
		fpu_version = HPPA_FPUVER(ver[0]);
		printf(", FPU %s rev %d",
		    hppa_mod_info(HPPA_TYPE_FPU, fpu_version >> 5),
		    fpu_version & 0x1f);
	}

	printf("\n%s: ", self->dv_xname);
	p = "";
	if (!pdc_cache.dc_conf.cc_sh) {
		printf("%uK(%db/l) Icache, ",
		    pdc_cache.ic_size / 1024, pdc_cache.ic_conf.cc_line * 16);
		p = "D";
	}
	printf("%uK(%db/l) wr-%s %scoherent %scache, ",
	    pdc_cache.dc_size / 1024, pdc_cache.dc_conf.cc_line * 16,
	    pdc_cache.dc_conf.cc_wt? "thru" : "back",
	    pdc_cache.dc_conf.cc_cst? "" : "in", p);

	p = "";
	if (!pdc_cache.dt_conf.tc_sh) {
		printf("%u ITLB, ", pdc_cache.it_size);
		p = "D";
	}
	printf("%u %scoherent %sTLB\n",
	    pdc_cache.dt_size, pdc_cache.dt_conf.tc_cst? "" : "in", p);

	cpu_intr_establish(IPL_CLOCK, 63, cpu_hardclock, NULL, "clock");
}
