/*	$OpenBSD: ka650.c,v 1.20 2011/09/15 00:48:24 miod Exp $	*/
/*	$NetBSD: ka650.c,v 1.25 2001/04/27 15:02:37 ragge Exp $	*/
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
 * vax650-specific code.
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <uvm/uvm_extern.h>

#include <machine/cvax.h>
#include <machine/ka650.h>
#include <machine/clock.h>
#include <machine/cpu.h>
#include <machine/psl.h>
#include <machine/mtpr.h>
#include <machine/sid.h>
#include <machine/nexus.h>

struct	ka650_merr *ka650merr_ptr;
struct	ka650_cbd *ka650cbd_ptr;
struct	ka650_ipcr *ka650ipcr_ptr;
int	*KA650_CACHE_ptr;

#define	CACHEOFF	0
#define	CACHEON		1

static	void	ka650setcache(int);
static	void    uvaxIII_conf(void);
static	void    uvaxIII_memerr(void);
static	int     uvaxIII_mchk(caddr_t);

struct	cpu_dep	ka650_calls = {
	NULL,
	uvaxIII_mchk,
	uvaxIII_memerr,
	uvaxIII_conf,
	generic_clkread,
	generic_clkwrite,
	4,      /* ~VUPS */
	2,	/* SCB pages */
	cvax_halt,
	cvax_reboot,
	NULL,
	icr_hardclock
};

/*
 * uvaxIII_conf() is called by cpu_attach to do the cpu_specific setup.
 */
void
uvaxIII_conf()
{
	int syssub = GETSYSSUBT(vax_siedata);

	/*
	 * MicroVAX III: We map in memory error registers,
	 * cache control registers, SSC registers,
	 * interprocessor registers and cache diag space.
	 */
	ka650merr_ptr = (void *)vax_map_physmem(KA650_MERR, 1);
	ka650cbd_ptr = (void *)vax_map_physmem(KA650_CBD, 1);
	cvax_ssc_ptr = (void *)vax_map_physmem(CVAX_SSC, 3);
	ka650ipcr_ptr = (void *)vax_map_physmem(KA650_IPCR, 1);
	KA650_CACHE_ptr = (void *)vax_map_physmem(KA650_CACHE,
	    (KA650_CACHESIZE/VAX_NBPG));

	printf("cpu: KA6%d%d, CVAX microcode rev %d Firmware rev %d\n",
	    syssub == VAX_SIE_KA640 ? 4 : 5,
	    syssub == VAX_SIE_KA655 ? 5 : 0,
	    (vax_cpudata & 0xff), GETFRMREV(vax_siedata));
	ka650setcache(CACHEON);
	if (ptoa(physmem) > ka650merr_ptr->merr_qbmbr) {
		printf("physmem(0x%x) > qbmbr(0x%x)\n",
		    ptoa(physmem), (int)ka650merr_ptr->merr_qbmbr);
		panic("qbus map unprotected");
	}
	if (mfpr(PR_TODR) == 0)
		mtpr(1, PR_TODR);
}

void
uvaxIII_memerr()
{
	printf("memory err!\n");
#if 0 /* XXX Fix this */
	register char *cp = (char *)0;
	register int m;
	extern u_int cache2tag;

	if (ka650cbd.cbd_cacr & CACR_CPE) {
		printf("cache 2 tag parity error: ");
		if (time_second - cache2tag < 7) {
			ka650setcache(CACHEOFF);
			printf("caching disabled\n");
		} else {
			cache2tag = time_second;
			printf("flushing cache\n");
			ka650setcache(CACHEON);
		}
	}
	m = ka650merr.merr_errstat;
	ka650merr.merr_errstat = MEM_EMASK;
	if (m & MEM_CDAL) {
		cp = "Bus Parity";
	} else if (m & MEM_RDS) {
		cp = "Hard ECC";
	} else if (m & MEM_CRD) {
		cp = "Soft ECC";
	}
	if (cp) {
		printf("%sMemory %s Error: page 0x%x\n",
			(m & MEM_DMA) ? "DMA " : "", cp,
			(m & MEM_PAGE) >> MEM_PAGESHFT);
	}
#endif
}

u_int	cache1tag;
u_int	cache1data;
u_int	cdalerr;
u_int	cache2tag;

int
uvaxIII_mchk(cmcf)
	caddr_t cmcf;
{
	struct cvax_mchk_frame *mcf = (struct cvax_mchk_frame *)cmcf;
	u_int type = mcf->cvax_summary;
	const char *descr;
	u_int i;

	printf("machine check %x", type);
	descr = cvax_mchk_descr(type);
	if (descr != NULL)
		printf(": %s", descr);
	printf("\n\tvap %x istate1 %x istate2 %x pc %x psl %x\n",
	    mcf->cvax_mrvaddr, mcf->cvax_istate1, mcf->cvax_istate2,
	    mcf->cvax_pc, mcf->cvax_psl);
	printf("dmaser=0x%b qbear=0x%x dmaear=0x%x\n",
	    ka650merr_ptr->merr_dser, DMASER_BITS, 
	    (int)ka650merr_ptr->merr_qbear,
	    (int)ka650merr_ptr->merr_dear);
	ka650merr_ptr->merr_dser = DSER_CLEAR;

	i = mfpr(PR_CAER);
	mtpr(CAER_MCC | CAER_DAT | CAER_TAG, PR_CAER);
	if (i & CAER_MCC) {
		printf("cache 1 ");
		if (i & CAER_DAT) {
			printf("data");
			i = cache1data;
			cache1data = time_second;
		}
		if (i & CAER_TAG) {
			printf("tag");
			i = cache1tag;
			cache1tag = time_second;
		}
	} else if ((i & CAER_MCD) || (ka650merr_ptr->merr_errstat & MEM_CDAL)) {
		printf("CDAL");
		i = cdalerr;
		cdalerr = time_second;
	}
	if (time_second - i < 7) {
		ka650setcache(CACHEOFF);
		printf(" parity error:	caching disabled\n");
	} else {
		printf(" parity error:	flushing cache\n");
		ka650setcache(CACHEON);
	}
	/*
	 * May be able to recover if type is 1-4, 0x80 or 0x81, but
	 * only if FPD is set in the saved PSL, or bit VCR in Istate2
	 * is clear.
	 */
	if ((type > 0 && type < 5) || type == 0x80 || type == 0x81) {
		if ((mcf->cvax_psl & PSL_FPD)
		    || !(mcf->cvax_istate2 & IS2_VCR)) {
			uvaxIII_memerr();
			return MCHK_RECOVERED;
		}
	}
	return MCHK_PANIC;
}

/*
 * Make sure both caches are off and not in diagnostic mode.  Clear the
 * 2nd level cache (by writing to each quadword entry), then enable it.
 * Enable 1st level cache too.
 */
void
ka650setcache(int state)
{
	int syssub = GETSYSSUBT(vax_siedata);
	int i;

	/*
	 * Before doing anything, disable the cache.
	 */
	mtpr(0, PR_CADR);
	if (syssub != VAX_SIE_KA640)
		ka650cbd_ptr->cbd_cacr = CACR_CPE;

	/*
	 * Check what we want to do, enable or disable.
	 */
	if (state == CACHEON) {
		mtpr(CADR_SEN2 | CADR_SEN1 | CADR_CENI | CADR_CEND, PR_CADR);
		if (syssub != VAX_SIE_KA640) {
			for (i = 0;
			    i < (KA650_CACHESIZE / sizeof(KA650_CACHE_ptr[0]));
			    i += 2)
				KA650_CACHE_ptr[i] = 0;
			ka650cbd_ptr->cbd_cacr = CACR_CEN;
		}
	}
}
