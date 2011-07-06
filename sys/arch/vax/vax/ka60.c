/*	$OpenBSD: ka60.c,v 1.2 2011/07/06 20:42:05 miod Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mt. Xinu.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ka650.c	7.7 (Berkeley) 12/16/90
 */

/*
 * VAXstation 3500 (KA60) specific code. Based on the KA650 specific code.
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <uvm/uvm_extern.h>

#include <machine/cvax.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/mtpr.h>
#include <machine/nexus.h>
#include <machine/psl.h>
#include <machine/sid.h>
#include <machine/rpb.h>
#include <machine/cca.h>
#include <machine/scb.h>

#include <vax/mbus/mbusreg.h>
#include <vax/mbus/mbusvar.h>
#include <vax/mbus/fwioreg.h>

int	ka60_clkread(time_t);
void	ka60_clkwrite(void);
void	ka60_clrf(void);
void	ka60_conf(void);
void	ka60_halt(void);
void	ka60_hardclock(struct clockframe *);
void	ka60_init(void);
int	ka60_mchk(caddr_t);
void	ka60_memerr(void);
void	ka60_reboot(int);

struct	cpu_dep	ka60_calls = {
	ka60_init,
	ka60_mchk,
	ka60_memerr,
	ka60_conf,
	ka60_clkread,
	ka60_clkwrite,
	3,	/* ~VUPS */
	2,	/* SCB pages */
#if 0	/* this ought to work, dammit! */
	cvax_halt,
	cvax_reboot,
#else
	ka60_halt,
	ka60_reboot,
#endif
	ka60_clrf,
	ka60_hardclock
};

void	ka60_memwrtmo(void *);

struct cca *cca;
unsigned int cca_size;

unsigned int ka60cpus = 1;
uint32_t *ka60_iocsr;

/*
 * Early system initialization, while still running physical.
 *
 * The PROM will have enabled the L2 cache, but each individual
 * CPU still has its own L1 cache disabled.
 *
 * L1 cache configuration is similar to KA650, without external
 * configuration registers.
 */
void
ka60_init()
{
	unsigned int mid;
	paddr_t fbicaddr;
	uint32_t modtype, fbicrange;
	int i;

	/*
	 * Enable CPU cache.
	 */
	mtpr(CADR_SEN2 | CADR_SEN1 | CADR_CENI | CADR_CEND, PR_CADR);

	cca = (struct cca *)rpb.cca_addr;	/* physical!!! */
	if (cca == NULL) {
		/*
		 * If things are *that* wrong, stick to 2 cpus and a
		 * monoprocessor kernel, really.  We could try looking
		 * for a CCA signature from the top of memory downwards,
		 * or count CPU boards to get the correct number of
		 * processors, but is it really worth doing? I don't
		 * think we are in Kansas anymore anyway...
		 */
		ka60cpus = 2;
	} else {
		cca_size = vax_atop(cca->cca_size);

		/*
		 * Count the other processors.
		 */
		for (i = 0; i < cca->cca_nproc; i++)
			if (cca->cca_console & (1 << i))
				ka60cpus++;
	}

	snprintf(cpu_model, sizeof cpu_model, "VAXstation 35%d0", ka60cpus);

	/*
	 * Silence memory write timeout errors now.
	 */
	scb_vecalloc(0x60, ka60_memwrtmo, NULL, 0, NULL);

	/*
	 * We need to find out which M-bus slot contains the I/O
	 * module.  This could not have been done before because
	 * we were not able to handle machine check (and thus run
	 * badaddr() on each slot), and this has to be done before
	 * consinit() may try to talk to the serial ports.
	 *
	 * Note that there might be multiple I/O modules in the system.
	 * We do not know which I/O module the PROM will prefer; however
	 * since only one module should be configured to map the SSC at
	 * its preferred address, it is possible to find out which one
	 * has been selected.
	 */

	for (mid = 0; mid < MBUS_SLOT_MAX; mid++) {
		fbicaddr = MBUS_SLOT_BASE(mid) + FBIC_BASE;
		if (badaddr((caddr_t)(fbicaddr + FBIC_MODTYPE), 4) != 0)
			continue;
		modtype = *(uint32_t *)(fbicaddr + FBIC_MODTYPE);
		if ((modtype & MODTYPE_CLASS_MASK) >> MODTYPE_CLASS_SHIFT !=
		    CLASS_IO)
			continue;

		mbus_ioslot = mid;

		fbicrange = *(uint32_t *)(fbicaddr + FBIC_RANGE);
		if (fbicrange ==
		    ((HOST_TO_MBUS(CVAX_SSC) & RANGE_MATCH) | RANGE_ENABLE))
			break;
	}

	if ((int)mbus_ioslot < 0) {
		/*
		 * This shouldn't happen. Try mid #5 (enclosure slot #4) as a
		 * supposedly sane default.
		 */
		mbus_ioslot = 5;
	}
}

/*
 * Early system initialization, while running virtual, and before
 * devices are probed.
 */
void
ka60_conf()
{
	printf("cpu0: KA60\n");

	cvax_ssc_ptr = (void *)vax_map_physmem(CVAX_SSC, 3);

	/*
	 * Remap the CCA now we're running virtual.
	 */
	if (cca != NULL)
		cca = (void *)vax_map_physmem((paddr_t)cca, cca_size);

	/*
	 * Map the IOCSR register of the main I/O module, and enable
	 * CPU clock.  We'll need this mapping for reset as well.
	 */
	ka60_iocsr = (uint32_t *)vax_map_physmem(MBUS_SLOT_BASE(mbus_ioslot) +
	    FWIO_IOCSR_OFFSET, 1);
	if (ka60_iocsr == 0)
		panic("can not map IOCSR");

	*ka60_iocsr |= FWIO_IOCSR_CLKIEN | FWIO_IOCSR_MRUN | FWIO_IOCSR_CNSL;
}

/*
 * Corrected memory error trap.
 */
void
ka60_memerr()
{
	printf("cpu0: corrected memory error\n");
	/*
	 * Need to peek at the M-bus error logs, display anything
	 * interesting, and clear them.
	 */
}

/*
 * Machine check trap.
 */
int
ka60_mchk(caddr_t mcef)
{
	struct cvax_mchk_frame *mcf = (struct cvax_mchk_frame *)mcef;
	u_int type = mcf->cvax_summary;
	const char *descr;

	printf("machine check %x", type);
	descr = cvax_mchk_descr(type);
	if (descr != NULL)
		printf(": %s", descr);
	printf("\n\tvap %x istate1 %x istate2 %x pc %x psl %x\n",
	    mcf->cvax_mrvaddr, mcf->cvax_istate1, mcf->cvax_istate2,
	    mcf->cvax_pc, mcf->cvax_psl);

	return MCHK_PANIC;
}

/*
 * Clock routines.  They need to access the TODR through the SSC.
 */
int
ka60_clkread(time_t base)
{
	unsigned klocka = cvax_ssc_ptr->ssc_todr;

	/*
	 * Sanity check.
	 */
	if (klocka < TODRBASE) {
		if (klocka == 0) {
			printf("TODR stopped");
			cvax_ssc_ptr->ssc_todr = 1;	/* spin it */
		} else
			printf("TODR too small");
		return CLKREAD_BAD;
	}

	time.tv_sec = yeartonum(numtoyear(base)) + (klocka - TODRBASE) / 100;
	return CLKREAD_OK;
}

void
ka60_clkwrite()
{
	unsigned tid = time.tv_sec, bastid;

	bastid = tid - yeartonum(numtoyear(tid));
	cvax_ssc_ptr->ssc_todr = (bastid * 100) + TODRBASE;
}

void
ka60_halt()
{
	printf("system halted.\n");
	asm("halt");
}

void
ka60_reboot(arg)
	int arg;
{
	printf("resetting system...\n");
	delay(500000);
	*ka60_iocsr |= FWIO_IOCSR_RSTWS;
}

/*
 * Probing empty M-bus slots causes this vector to be triggered.
 *
 * We get one after the first spl0(), if probing for the console
 * slot caused us to look at empty slots, and then one per empty
 * slot during autoconf.
 *
 * There shouldn't be any such error after autoconf, though.
 */
void
ka60_memwrtmo(void *arg)
{
	/* do nothing */
}

void
ka60_clrf(void)
{
	/*
	 * Restore the memory write timeout vector.
	 */
	scb_vecalloc(0x60, scb_stray, (void *)0x60, SCB_ISTACK, NULL);
}

/*
 * SSC clock interrupts come at level 0x16, which is not enough for
 * our needs, so raise the level here before invoking hardclock().
 */
void
ka60_hardclock(struct clockframe *cf)
{
	int s;

	s = splclock();
	hardclock(cf);
	splx(s);
}
