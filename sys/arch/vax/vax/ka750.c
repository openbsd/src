/*	$NetBSD: ka750.c,v 1.8 1995/12/13 18:50:34 ragge Exp $	*/

/*-
 * Copyright (c) 1982, 1986, 1988 The Regents of the University of California.
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	@(#)ka750.c	7.4 (Berkeley) 5/9/91
 *      @(#)autoconf.c  7.20 (Berkeley) 5/9/91
 */

#include "sys/param.h"
#include "sys/types.h"
#include "sys/device.h"
#include "vm/vm.h"
#include "vm/vm_kern.h"

#include "machine/ka750.h"
#include "machine/pte.h"
#include "machine/cpu.h"
#include "machine/mtpr.h"
#include "vax/uba/ubavar.h"
#include "vax/uba/ubareg.h"

/*
 * ka750_conf() is called by cpu_attach to do the cpu_specific setup.
 */
void
ka750_conf(parent, self, aux)
	struct	device *parent, *self;
	void	*aux;
{
	extern char cpu_model[];

	strcpy(cpu_model,"VAX 11/750");
	printf(": 11/750, hardware rev %d, ucode rev %d\n",
	    V750HARDW(cpu_type), V750UCODE(cpu_type));
	printf("%s: ", self->dv_xname);
	if (mfpr(PR_ACCS) & 255) {
		printf("FPA present, enabling.\n");
		mtpr(0x8000, PR_ACCS);
	} else
		printf("no FPA\n");
}

/*
 * ka750_clock() makes the 11/750 interrupt clock and todr
 * register start counting.
 */
int
ka750_clock()
{

	mtpr(-10000, PR_NICR); /* Load in count register */
	mtpr(0x800000d1, PR_ICCS); /* Start clock and enable interrupt */
	if (mfpr(PR_TODR)) {
		/* todr running */
		return 0;
	} else {
		/* Start TODR register. */
		mtpr(1, PR_TODR);
		return 1;
	}

}


extern volatile caddr_t mcraddr[];

struct	mcr750 {
	int	mc_err;			/* error bits */
	int	mc_inh;			/* inhibit crd */
	int	mc_inf;			/* info bits */
};

#define	M750_ICRD	0x10000000	/* inhibit crd interrupts, in [1] */
#define	M750_UNCORR	0xc0000000	/* uncorrectable error, in [0] */
#define	M750_CORERR	0x20000000	/* correctable error, in [0] */

#define	M750_INH(mcr)	((mcr)->mc_inh = 0)
#define	M750_ENA(mcr)	((mcr)->mc_err = (M750_UNCORR|M750_CORERR), \
			 (mcr)->mc_inh = M750_ICRD)
#define	M750_ERR(mcr)	((mcr)->mc_err & (M750_UNCORR|M750_CORERR))

#define	M750_SYN(err)	((err) & 0x7f)
#define	M750_ADDR(err)	(((err) >> 9) & 0x7fff)

/* enable crd interrupts */
ka750_memenable(sa,self)
	struct sbi_attach_args *sa;
	struct device *self;
{
	extern int nmcr;
	int k,l,m,cardinfo;
	struct mcr750 *mcr=(struct mcr750 *)sa->nexaddr;
	
	mcraddr[self->dv_unit]=(caddr_t)sa->nexaddr;

	/* We will use this info for error reporting - later! */
	cardinfo=mcr->mc_inf;
	switch((cardinfo>>24)&3){
	case 0: printf(": L0011 ");
		break;
	case 1: printf(": L0016 ");
		m=cardinfo&0xaaaa;
		for(k=l=0;k<16;k++){if(m&1)l++;m>>=1;}
		printf("with %d M8750",l);
		break;
	case 3: printf(": L0022 ");
		m=cardinfo&0x5555;
		for(k=l=0;k<16;k++){if(m&1)l++;m>>=1;}
		printf("with %d M7199",l);
		m=cardinfo&0xaaaa;
		if(m){
		for(k=l=0;k<16;k++){if(m&1)l++;m>>=1;}
		printf(" and %d M8750",l);
		}
		break;
	}
	printf("\n");


	M750_ENA((struct mcr750 *)mcraddr[0]);
}

/* log crd errors */
ka750_memerr()
{
	register struct mcr750 *mcr = (struct mcr750 *)mcraddr[0];
	register int err;

	if (M750_ERR(mcr)) {
		err = mcr->mc_err;	/* careful with i/o space refs */
		printf("mcr0: %s", err & M750_UNCORR ?
		    "hard error" : "soft ecc");
		printf(" addr %x syn %x\n", M750_ADDR(err), M750_SYN(err));
		M750_INH(mcr);
	}
}

char *mc750[]={"0","1","2","3","4","5","6","7","8","9","10","11","12","13",
	"14","15"};

struct mc750frame {
	int	mc5_bcnt;		/* byte count == 0x28 */
	int	mc5_summary;		/* summary parameter (as above) */
	int	mc5_va;			/* virtual address register */
	int	mc5_errpc;		/* error pc */
	int	mc5_mdr;
	int	mc5_svmode;		/* saved mode register */
	int	mc5_rdtimo;		/* read lock timeout */
	int	mc5_tbgpar;		/* tb group parity error register */
	int	mc5_cacherr;		/* cache error register */
	int	mc5_buserr;		/* bus error register */
	int	mc5_mcesr;		/* machine check status register */
	int	mc5_pc;			/* trapped pc */
	int	mc5_psl;		/* trapped psl */
};

#define MC750_TBERR	2		/* type code of cp tbuf par */
#define	MC750_TBPAR	4		/* tbuf par bit in mcesr */

ka750_mchk(cmcf)
	caddr_t cmcf;
{
	register struct mc750frame *mcf = (struct mc750frame *)cmcf;
	register int type = mcf->mc5_summary;
	int mcsr = mfpr(PR_MCSR);

	printf("machine check %x: %s%s\n", type, mc750[type&0xf],
	    (type&0xf0) ? " abort" : " fault"); 
	printf(
"\tva %x errpc %x mdr %x smr %x rdtimo %x tbgpar %x cacherr %x\n",
	    mcf->mc5_va, mcf->mc5_errpc, mcf->mc5_mdr, mcf->mc5_svmode,
	    mcf->mc5_rdtimo, mcf->mc5_tbgpar, mcf->mc5_cacherr);
	printf("\tbuserr %x mcesr %x pc %x psl %x mcsr %x\n",
	    mcf->mc5_buserr, mcf->mc5_mcesr, mcf->mc5_pc, mcf->mc5_psl,
	    mcsr);
	mtpr(0, PR_TBIA);
	mtpr(0xf, PR_MCESR);
	if (type == MC750_TBERR && (mcf->mc5_mcesr&0xe) == MC750_TBPAR) {
		printf("tbuf par: flushing and returning\n");
		return (0);
	}
	return (-1);
}

ka750_steal_pages()
{
	extern	vm_offset_t avail_start, virtual_avail;
	extern	struct nexus *nexus;
	int	junk;

	/*
	 * We take away the pages we need, one for SCB and the rest
	 * for UBA vectors == 1 + 2 will alloc all needed space.
	 * We also set up virtual area for SBI.
	 */
	MAPPHYS(junk, V750PGS, VM_PROT_READ|VM_PROT_WRITE);
	MAPVIRT(nexus, btoc(NEX750SZ));
	pmap_map((vm_offset_t)nexus, NEX750, NEX750 + NEX750SZ,
	    VM_PROT_READ|VM_PROT_WRITE);

	return 0;
}

