/*	$OpenBSD: locore.c,v 1.36 2010/04/21 03:11:28 deraadt Exp $	*/
/*	$NetBSD: locore.c,v 1.43 2000/03/26 11:39:45 ragge Exp $	*/
/*
 * Copyright (c) 1994, 1998 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

 /* All bugs are subject to removal without further notice */
		

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/sid.h>
#include <machine/param.h>
#include <machine/vmparam.h>
#include <machine/pcb.h>
#include <machine/pte.h>
#include <machine/pmap.h>
#include <machine/nexus.h>
#include <machine/rpb.h>
#include <machine/cca.h>

void	start(struct rpb *);
void	main(void);

extern	paddr_t avail_end;
extern	int physmem;
paddr_t	esym;
u_int	proc0paddr;
char	cpu_model[100];

/*
 * The strict cpu-dependent information is set up here, in
 * form of a pointer to a struct that is specific for each cpu.
 */
extern struct cpu_dep ka780_calls;
extern struct cpu_dep ka750_calls;
extern struct cpu_dep ka860_calls;
extern struct cpu_dep ka820_calls;
extern struct cpu_dep ka43_calls;
extern struct cpu_dep ka46_calls;
extern struct cpu_dep ka48_calls;
extern struct cpu_dep ka49_calls;
extern struct cpu_dep ka53_calls;
extern struct cpu_dep ka410_calls;
extern struct cpu_dep ka630_calls;
extern struct cpu_dep ka650_calls;
extern struct cpu_dep ka660_calls;
extern struct cpu_dep ka670_calls;
extern struct cpu_dep ka680_calls;
extern struct cpu_dep ka60_calls;
extern struct cpu_dep vxt_calls;

/*
 * Start is called from boot; the first routine that is called
 * in kernel. Kernel stack is setup somewhere in a safe place;
 * but we need to move it to a better known place. Memory
 * management is disabled, and no interrupt system is active.
 */
void
start(struct rpb *prpb)
{
	extern vaddr_t scratch;
	int preserve_cca = 0;

	mtpr(AST_NO, PR_ASTLVL); /* Turn off ASTs */

	findcpu(); /* Set up the CPU identifying variables */

	if (vax_confdata & 0x80)
		strlcpy(cpu_model, "MicroVAX ", sizeof cpu_model);
	else
		strlcpy(cpu_model, "VAXstation ", sizeof cpu_model);

	switch (vax_boardtype) {
#if VAX780
	case VAX_BTYP_780:
		dep_call = &ka780_calls;
		strlcpy(cpu_model,"VAX 11/780", sizeof cpu_model);
		if (vax_cpudata & 0x100)
			cpu_model[9] = '5';
		break;
#endif
#if VAX750
	case VAX_BTYP_750:
		dep_call = &ka750_calls;
		strlcpy(cpu_model, "VAX 11/750", sizeof cpu_model);
		break;
#endif
#if VAX8600
	case VAX_BTYP_790:
		dep_call = &ka860_calls;
		strlcpy(cpu_model,"VAX 8600", sizeof cpu_model);
		if (vax_cpudata & 0x100)
			cpu_model[6] = '5';
		break;
#endif
#if VAX410
	case VAX_BTYP_420: /* They are very similar */
		dep_call = &ka410_calls;
		strlcat(cpu_model, "3100", sizeof cpu_model);
		switch ((vax_siedata >> 8) & 0xff) {
		case 0x00:
			strlcat(cpu_model, "/m{30,40}", sizeof cpu_model);
			break;
		case 0x01:
			strlcat(cpu_model, "/m{38,48}", sizeof cpu_model);
			break;
		case 0x02:
			strlcat(cpu_model, "/m{10,20}{,e}", sizeof cpu_model);
			break;
		}
		break;

	case VAX_BTYP_410:
		dep_call = &ka410_calls;
		strlcat(cpu_model, "2000", sizeof cpu_model);
		break;
#endif
#if VAX43
	case VAX_BTYP_43:
		dep_call = &ka43_calls;
		strlcat(cpu_model, "3100/m76", sizeof cpu_model);
		break;
#endif
#if VAX46
	case VAX_BTYP_46:
		dep_call = &ka46_calls;
		switch(vax_siedata & 0xff) {
		case VAX_VTYP_47:
			strlcpy(cpu_model, "MicroVAX 3100 m80", sizeof cpu_model);
			break;
		case VAX_VTYP_46:
			strlcpy(cpu_model, "VAXstation 4000/60", sizeof cpu_model);
			break;
		default:
			strlcat(cpu_model, " - Unknown Mariah", sizeof cpu_model);
		}
		break;
#endif
#ifdef VXT
	case VAX_BTYP_VXT:
		dep_call = &vxt_calls;
		strlcpy(cpu_model, "VXT2000", sizeof cpu_model);
		break;
#endif
#if VAX48
	case VAX_BTYP_48:
		dep_call = &ka48_calls;
		switch ((vax_siedata >> 8) & 0xff) {
		case VAX_STYP_45:
			strlcpy(cpu_model, "MicroVAX 3100/m{30,40}", sizeof cpu_model);
			break;
		case VAX_STYP_48:
			strlcpy(cpu_model, "VAXstation 4000/VLC", sizeof cpu_model);
			break;
		default:
			strlcat(cpu_model, " - Unknown SOC", sizeof cpu_model);
		}
		break;
#endif
#if VAX49
	case VAX_BTYP_49:
		dep_call = &ka49_calls;
		strlcpy(cpu_model, "VAXstation 4000/90", sizeof cpu_model);
		break;
#endif
#if VAX53
	case VAX_BTYP_1303:	
		dep_call = &ka53_calls;
		switch ((vax_siedata >> 8) & 0xff) {
		case VAX_STYP_50:
			strlcpy(cpu_model, "MicroVAX 3100 model 85 or 90", sizeof cpu_model);
			break;
		case VAX_STYP_51:
			strlcpy(cpu_model, "MicroVAX 3100 model 90 or 95", sizeof cpu_model);
			break;
		case VAX_STYP_52:
			strlcpy(cpu_model, "VAX 4000 100", sizeof cpu_model);
			break;
		case VAX_STYP_53:
			strlcpy(cpu_model, "VAX 4000 105A", sizeof cpu_model);
			break;
		default:
			strlcpy(cpu_model, "VAX - Unknown Cheetah Class", sizeof cpu_model);
		}
		break;
#endif
#if VAX630
	case VAX_BTYP_630:
		dep_call = &ka630_calls;
		strlcpy(cpu_model,"MicroVAX II", sizeof cpu_model);
		break;
#endif
#if VAX650
	case VAX_BTYP_650:
		dep_call = &ka650_calls;
		strlcpy(cpu_model,"MicroVAX ", sizeof cpu_model);
		switch ((vax_siedata >> 8) & 255) {
		case VAX_SIE_KA640:
			strlcat(cpu_model, "3300/3400", sizeof cpu_model);
			break;

		case VAX_SIE_KA650:
			strlcat(cpu_model, "3500/3600", sizeof cpu_model);
			break;

		case VAX_SIE_KA655:
			strlcat(cpu_model, "3800/3900", sizeof cpu_model);
			break;

		default:
			strlcat(cpu_model, "III", sizeof cpu_model);
			break;
		}
		break;
#endif
#if VAX660
	case VAX_BTYP_660:
		dep_call = &ka660_calls;
		strlcpy(cpu_model,"VAX 4000 200", sizeof cpu_model);
		break;
#endif
#if VAX670
	case VAX_BTYP_670:
		dep_call = &ka670_calls;
		strlcpy(cpu_model,"VAX 4000 300", sizeof cpu_model);
		break;
#endif
#if VAX680
	case VAX_BTYP_1301:
		dep_call = &ka680_calls;
		strlcpy(cpu_model,"VAX 4000 ", sizeof cpu_model);
		switch ((vax_siedata >> 8) & 0xff) {
		case VAX_STYP_675:
			strlcat(cpu_model,"400", sizeof cpu_model);
			break;
		case VAX_STYP_680:
			strlcat(cpu_model,"500", sizeof cpu_model);
			break;
		case VAX_STYP_690:
			strlcat(cpu_model,"600", sizeof cpu_model);
			break;
		default:
			strlcat(cpu_model,"- Unknown Omega Class", sizeof cpu_model);
		}
		break;
	case VAX_BTYP_1305:
		dep_call = &ka680_calls;
		strlcpy(cpu_model,"VAX 4000 ", sizeof cpu_model);
		switch ((vax_siedata >> 8) & 0xff) {
		case VAX_STYP_681:
			strlcat(cpu_model,"500A", sizeof cpu_model);
			break;
		case VAX_STYP_691:
			strlcat(cpu_model,"605A", sizeof cpu_model);
			break;
		case VAX_STYP_694:
			if (vax_cpudata & 0x1000)
				strlcat(cpu_model,"705A", sizeof cpu_model);
			else
				strlcat(cpu_model,"700A", sizeof cpu_model);
			break;
		default:
			strlcat(cpu_model,"- Unknown Legacy Class", sizeof cpu_model);
		}
		break;
#endif
#if VAX8200
	case VAX_BTYP_8000:
		mastercpu = mfpr(PR_BINID);
		dep_call = &ka820_calls;
		strlcpy(cpu_model, "VAX 8200", sizeof cpu_model);
		break;
#endif
#ifdef VAX60
	case VAX_BTYP_60:
		dep_call = &ka60_calls;
		preserve_cca = 1;
		/* cpu_model will be set in ka60_init */
		break;
#endif
	default:
		/* CPU not supported, just give up */
		asm("halt");
	}

	/*
	 * Machines older than MicroVAX II have their boot blocks
	 * loaded directly or the boot program loaded from console
	 * media, so we need to figure out their memory size.
	 * This is not easily done on MicroVAXen, so we get it from
	 * VMB instead.
	 *
	 * In post-1.4 a RPB is always provided from the boot blocks.
	 */
#if 1 /* compat with old bootblocks */
	if (prpb == 0) {
		bzero((caddr_t)proc0paddr + REDZONEADDR, sizeof(struct rpb));
		prpb = (struct rpb *)(proc0paddr + REDZONEADDR);
		prpb->pfncnt = avail_end >> VAX_PGSHIFT;
		prpb->rpb_base = (void *)-1;    /* RPB is fake */
	} else
#endif
	bcopy(prpb, (caddr_t)proc0paddr + REDZONEADDR, sizeof(struct rpb));
	if (prpb->pfncnt)
		avail_end = prpb->pfncnt << VAX_PGSHIFT;
	else
		while (badaddr((caddr_t)avail_end, 4) == 0)
			avail_end += VAX_NBPG * 128;
	boothowto = prpb->rpb_bootr5;

	physmem = atop(avail_end);

	/*
	 * If we need to use the Console Communication Area, make sure
	 * we will not stomp over it.
	 *
	 * On KA60 systems, the PROM apparently forgets to keep the CCA
	 * out of the reported memory size.  It's no real surprise, as
	 * the memory bitmap pointed to by the CCA reports all physical
	 * memory (including itself and the CCA) as available!
	 * (which means the bitmap is not worth looking at either)
	 */

	if (preserve_cca) {
		if (prpb->cca_addr != 0 && avail_end > prpb->cca_addr) {
			struct cca *cca = (struct cca *)prpb->cca_addr;

			/*
			 * XXX Should validate the CCA image here.
			 */

			avail_end = prpb->cca_addr;
			if (cca->cca_bitmap != 0 && avail_end > cca->cca_bitmap)
				avail_end = cca->cca_bitmap;
		}
	}

        avail_end = trunc_page(avail_end); /* be sure */

	proc0.p_addr = (struct user *)proc0paddr; /* XXX */
	bzero((struct user *)proc0paddr, sizeof(struct user));

	/* Clear the used parts of the uarea except for the pcb */
	bzero(&proc0.p_addr->u_stats, sizeof(struct user) - sizeof(struct pcb));

	pmap_bootstrap();

	/* Now running virtual. set red zone for proc0 */
	*kvtopte((u_int)proc0.p_addr + REDZONEADDR) &= ~PG_V;

	((struct pcb *)proc0paddr)->framep = (void *)scratch;

	/*
	 * Change mode down to userspace is done by faking a stack
	 * frame that is setup in cpu_set_kpc(). Not done by returning
	 * from main anymore.
	 */
	main();
	/* NOTREACHED */
}
