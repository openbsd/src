/* $Id: pcc.c,v 1.1.1.1 1995/10/18 08:51:10 deraadt Exp $ */

/*
 *
 * Copyright (c) 1995 Charles D. Cranor
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
 *      This product includes software developed by Charles D. Cranor.
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

/*
 * peripheral channel controller
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/tty.h>
#include <sys/uio.h>
#include <sys/callout.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/fcntl.h>
#include <sys/device.h>
#include <machine/cpu.h>
#include <dev/cons.h>
#include <mvme68k/mvme68k/isr.h>
#include <mvme68k/dev/iio.h>
#include <mvme68k/dev/pccreg.h>

/*
 * Autoconfiguration stuff.
 */

struct pccsoftc {
	struct device	sc_dev;
	struct pcc	*sc_pcc;
};


void pccattach __P((struct device *, struct device *, void *));
int  pccmatch __P((struct device *, void *, void *));

struct cfdriver pcccd = {
	NULL, "pcc", pccmatch, pccattach,
	DV_DULL, sizeof(struct pccsoftc), 0
};

/*
 * globals
 */

struct pcc *sys_pcc = NULL;

struct {
	int	(*pcc_fn)();
	void	*arg;
	int	lvl;
} pcc_vecs[PCC_NVEC];

int
pccmatch(parent, vcf, args)
	struct device *parent;
	void *vcf, *args;
{
	struct cfdata *cf = vcf;

	return !badbaddr((caddr_t) IIO_CFLOC_ADDR(cf));
}

void
pccattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct pccsoftc *pccsc;

	if (sys_pcc)
		panic("pcc already attached!");

	iio_print(self->dv_cfdata);

	/*
	 * link into softc and set up interrupt vector base
	 */
	pccsc = (struct pccsoftc *) self;
	sys_pcc = pccsc->sc_pcc = (struct pcc *)IIO_CFLOC_ADDR(self->dv_cfdata);
	pccsc->sc_pcc->int_vectr = PCC_VECBASE;
	bzero(pcc_vecs, sizeof(pcc_vecs));

	printf(" rev %d intbvr 0x%x\n", pccsc->sc_pcc->pcc_rev,
	    pccsc->sc_pcc->int_vectr);
}


/*
 * pccintr: called from locore with the PC and evec from the trap frame.
 */
int
pccintr(pc, evec, frame)
	int pc;
	int evec;
	void *frame;
{
	int vec = (evec & 0xfff) >> 2; /* XXX should be m68k macro? */
	extern u_long intrcnt[]; /* XXX from locore */

	vec = vec & 0xf; /* XXX mask out */
	if (vec >= PCC_NVEC || pcc_vecs[vec].pcc_fn == NULL) 
		return(straytrap(pc, evec));

	cnt.v_intr++;
	intrcnt[pcc_vecs[vec].lvl]++;
  
	/* arg override?  only timer1 gets access to frame */
	if (vec != PCCV_TIMER1)
		frame = pcc_vecs[vec].arg;
	return((*pcc_vecs[vec].pcc_fn)(frame));
}


/*
 * pccintr_establish: establish pcc interrupt
 */
int
pccintr_establish(vec, hand, lvl, arg)
	u_long vec;
	int (*hand)(), lvl;
	void *arg;
{
	if (vec >= PCC_NVEC) {
		printf("pcc: illegal vector: 0x%x\n", vec);
		panic("pccintr_establish");
	}

	if (pcc_vecs[vec].pcc_fn) {
		printf("pcc: vector 0x%x in use: (0x%x,0x%x) (0x%x,0x%x)\n",
		    hand, arg, pcc_vecs[vec].pcc_fn, pcc_vecs[vec].arg);
		panic("pccintr_establish");
	}

	pcc_vecs[vec].pcc_fn = hand;
	pcc_vecs[vec].lvl = lvl;
	pcc_vecs[vec].arg = arg;
}
