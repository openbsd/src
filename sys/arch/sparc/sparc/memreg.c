/*	$OpenBSD: memreg.c,v 1.7 1997/11/11 10:53:12 niklas Exp $	*/
/*	$NetBSD: memreg.c,v 1.21 1997/07/29 09:42:08 fair Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *	This product includes software developed by Harvard University.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 *	This product includes software developed by Harvard University.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)memreg.c	8.1 (Berkeley) 6/11/93
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/ctlreg.h>

#include <sparc/sparc/memreg.h>
#include <sparc/sparc/vaddrs.h>
#include <sparc/sparc/asm.h>
#include <sparc/sparc/cpuvar.h>

#include <machine/reg.h>	/* for trapframe */
#include <machine/trap.h>	/* for trap types */

int memregmatch __P((struct device *, void *, void *));
void memregattach __P((struct device *, struct device *, void *));

struct cfattach memreg_ca = {
	sizeof(struct device), memregmatch, memregattach
};

struct cfdriver memreg_cd = {
	0, "memreg", DV_DULL
};

void memerr __P((int, u_int, u_int, u_int, u_int));
#if defined(SUN4M)
void hardmemerr4m __P((int, u_int, u_int));
#endif

/*
 * The OPENPROM calls this "memory-error".
 */
int
memregmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	register struct cfdata *cf = vcf;
	register struct confargs *ca = aux;

	if (CPU_ISSUN4) {
		if (ca->ca_bustype == BUS_OBIO)
			return (strcmp(cf->cf_driver->cd_name,
			    ca->ca_ra.ra_name) == 0);
		return (0);
	}
	return (strcmp("memory-error", ca->ca_ra.ra_name) == 0);
}

/* ARGSUSED */
void
memregattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	register struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	if (CPU_ISSUN4) {
		if (par_err_reg == NULL)
			panic("memregattach");
		ra->ra_vaddr = (caddr_t)par_err_reg;
	} else {
		par_err_reg = ra->ra_vaddr ? (volatile u_int *)ra->ra_vaddr :
		    (volatile u_int *)mapiodev(ra->ra_reg, 0, sizeof(int));
	}
	printf("\n");
}

/*
 * Synchronous and asynchronous memory error handler.
 * (This is the level 15 interrupt, which is not vectored.)
 * Should kill the process that got its bits clobbered,
 * and take the page out of the page pool, but for now...
 */

void
memerr(issync, ser, sva, aer, ava)
	int issync;
	u_int ser, sva, aer, ava;
{
	/* XXX Ugh! Clean up this switch and all the ifdefs! */
	switch (cputyp) {
#if defined(SUN4)
	case CPU_SUN4:
		if (par_err_reg) {
			printf("mem err: ser=%b sva=0x%x\n",
				ser, SER_BITS, sva);
			printf("parity error register = %b\n",
				*par_err_reg, PER_BITS);
		} else {
			printf("mem err: ser=? sva=?\n");
			printf("parity error register not mapped yet!\n"); /* XXX */
		}
#ifdef DEBUG
		callrom();
#else
		panic("memory error");		/* XXX */
#endif
		break;
#endif /* Sun4 */

#if defined(SUN4C)
	case CPU_SUN4C:
		printf("%ssync mem arr: ser=%b sva=0x%x ",
		    issync ? "" : "a", ser, SER_BITS, sva);
		printf("aer=%b ava=0x%x\n", aer & 0xff, AER_BITS, ava);
		if (par_err_reg)
			printf("parity error register = %b\n",
			    *par_err_reg, PER_BITS);
#ifdef DEBUG
		callrom();
#else
		panic("memory error");		/* XXX */
#endif
		break;
#endif /* Sun4C */

#if defined(SUN4M)
	case CPU_SUN4M:
		hardmemerr4m(2, ser, sva);
		break;
#endif /* Sun4M */
	    default:
		break;
	}
}


#if defined(SUN4M)
/*
 * hardmemerr4m: called upon fatal memory error. Print a message and panic.
 * Note that issync is not really an indicator of whether or not the error
 * was synchronous; if it is set, it means that the fsr/faddr pair correspond
 * to the MMU's fault status register; if clear, they correspond to the
 * HyperSPARC asynchronous error register. If issync==2, then both decodings
 * of the error register are printed.
 */

void
hardmemerr4m(issync, fsr, faddr)
	int issync;
	u_int fsr, faddr;
{
	switch (issync) {
	    case 1:
		if ((fsr & SFSR_FT) == SFSR_FT_NONE)
		    return;
		printf("mem err: sfsr=%b sfaddr=0x%x\n", fsr, SFSR_BITS, faddr);
		break;
	    case 0:
		if (!(fsr & AFSR_AFO))
		    return;
		printf("async (HS) mem err: afsr=%b afaddr=0x%x physaddr=0x%x%x\n",
		       fsr, AFSR_BITS, faddr,
		       (fsr & AFSR_AFA) >> AFSR_AFA_RSHIFT, faddr);
		break;
	    default:	/* unknown; print both decodings*/
		printf("unknown mem err: if sync, fsr=%b fva=0x%x; ",
		       fsr, SFSR_BITS, faddr);
		printf("if async, fsr=%b fa=0x%x pa=0x%x%x", fsr,
		       AFSR_BITS, faddr, (fsr & AFSR_AFA) >> AFSR_AFA_RSHIFT,
		       faddr);
		break;
	}
	panic("hard memory error");
}

/*
 * Memerr4m: handle a non-trivial memory fault. These include HyperSPARC
 * asynchronous faults, SuperSPARC store-buffer copyback failures, and
 * data faults without a valid faulting VA. We try to retry the operation
 * once, and then fail if we get called again.
 */

static int addrold = (int)0xdeadbeef; /* We pick an unlikely address */
static int addroldtop = (int)0xdeadbeef;
static int oldtype = -1;

void
memerr4m(type, sfsr, sfva, afsr, afva, tf)
	register unsigned type;
	register u_int sfsr;
	register u_int sfva;
	register u_int afsr;
	register u_int afva;
	register struct trapframe *tf;
{
	if ((afsr & AFSR_AFO) != 0) {	/* HS async fault! */

		printf("HyperSPARC async cache memory failure at phys 0x%x%x. "
		       "Ignoring.\n", (afsr & AFSR_AFA) >> AFSR_AFA_RSHIFT,
		       afva);

		if (afva == addrold && (afsr & AFSR_AFA) == addroldtop)
			hardmemerr4m(0, afsr, afva);
			/* NOTREACHED */

		oldtype = -1;
		addrold = afva;
		addroldtop = afsr & AFSR_AFA;

	} else if (type == T_STOREBUFFAULT && cpuinfo.cpu_vers == 4) {

		/*
		 * On Supersparc, we try to reenable the store buffers
		 * to force a retry.
		 */
		printf("store buffer copy-back failure at 0x%x. Retrying...\n",
		       sfva);

		if (oldtype == T_STOREBUFFAULT || addrold == sfva)
			hardmemerr4m(1, sfsr, sfva);
			/* NOTREACHED */

		oldtype = T_STOREBUFFAULT;
		addrold = sfva;

		/* reenable store buffer */
		sta(SRMMU_PCR, ASI_SRMMU,
		    lda(SRMMU_PCR, ASI_SRMMU) | VIKING_PCR_SB);

	} else if (type == T_DATAFAULT && !(sfsr & SFSR_FAV)) { /* bizarre */
		/* XXX: Should handle better. See SuperSPARC manual pg. 9-35 */

		printf("warning: got data fault with no faulting address."
		       " Ignoring.\n");

		if (oldtype == T_DATAFAULT)
			hardmemerr4m(1, sfsr, sfva);
			/* NOTREACHED */

		oldtype = T_DATAFAULT;
	} else if (type == 0) {	/* NMI */
		printf("ERROR: got NMI with sfsr=0x%b, sfva=0x%x, ",
		       sfsr, SFSR_BITS, sfva);
		printf("afsr=0x%b, afaddr=0x%x. Retrying...\n",
		       afsr, AFSR_BITS, afva);
		if (oldtype == 0 || addrold == sfva)
			hardmemerr4m(1, sfsr, sfva);	/* XXX: async? */
			/* NOTREACHED */

		oldtype = 0;
		addrold = sfva;
	} else 	/* something we don't know about?!? */ {
		printf("unknown fatal memory error, type=%d, sfsr=%b, sfva=0x%x",
		       type, sfsr, SFSR_BITS, sfva);
		printf(", afsr=%b, afaddr=0x%x\n", afsr, AFSR_BITS, afva);
		panic("memerr4m");
	}

	return;
}
#endif /* 4m */
