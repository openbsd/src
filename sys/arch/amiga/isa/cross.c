/*	$OpenBSD: cross.c,v 1.2 1996/02/27 15:40:54 niklas Exp $	*/
/*	$NetBSD: cross.c,v 1.0 1994/07/08 23:32:17 niklas Exp $	*/

/*
 * Copyright (c) 1994 Niklas Hallqvist, Carsten Hammer
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
 *      This product includes software developed by Christian E. Hopps.
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
#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/syslog.h>

#include <machine/cpu.h>
#include <machine/pio.h>

#include <dev/isa/isavar.h>

#include <amiga/amiga/custom.h>
#include <amiga/amiga/device.h>
#include <amiga/amiga/isr.h>
#include <amiga/dev/zbusvar.h>
#include <amiga/isa/isa_machdep.h>
#include <amiga/isa/isa_intr.h>
#include <amiga/isa/crossvar.h>
#include <amiga/isa/crossreg.h>

int crossdebug = 0;

/* This static is OK because we only allow one ISA bus.  */
struct cross_device *crossp;

void crossattach __P((struct device *, struct device *, void *));
int crossmatch __P((struct device *, void *, void *));
int crossprint __P((void *auxp, char *));
void crossstb __P((struct device *, int, u_char));
u_char crossldb __P((struct device *, int));
void crossstw __P((struct device *, int, u_short));
u_short crossldw __P((struct device *, int));
void    *cross_establish_intr __P((int intr, int type, int level,
                                   int (*ih_fun) (void *), void *ih_arg,
				   char *ih_what));
void    cross_disestablish_intr __P((void *handler));

struct isa_intr_fcns cross_intr_fcns = {
        0 /* cross_intr_setup */,       cross_establish_intr,
        cross_disestablish_intr,        0 /* cross_iointr */
};

struct cfdriver crosscd = {
	NULL, "cross", crossmatch, crossattach, 
	DV_DULL, sizeof(struct cross_device), 0
};

int
crossmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct zbus_args *zap = aux;

	/*
	 * Check manufacturer and product id.
	 */
	if (zap->manid == 2011 && zap->prodid == 3)
		return(1);
	return(0);
}

void
crossattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	struct zbus_args *zap = auxp;
	struct cross_device *cdp = (struct cross_device *)dp;

	crossp = cdp;
	bcopy(zap, &cdp->cd_zargs, sizeof(struct zbus_args));
	cdp->cd_link.il_dev = dp;
	cdp->cd_link.il_ldb = crossldb;
	cdp->cd_link.il_stb = crossstb;
	cdp->cd_link.il_ldw = crossldw;
	cdp->cd_link.il_stw = crossstw;
	cdp->cd_imask = 1 << CROSS_MASTER;

	isa_intr_fcns = &cross_intr_fcns;
        isa_pio_fcns = &cross_pio_fcns;

	/* Enable interrupts lazily in crossaddint.  */
	CROSS_ENABLE_INTS(zap->va, 0);
	/* Default 16 bit tranfer  */
	*(volatile u_short *)(cdp->cd_zargs.va + CROSS_XLP_LATCH) = CROSS_SBHE; 

	printf(": pa 0x%08x va 0x%08x size 0x%x\n", zap->pa, zap->va,
            zap->size);


	/*
	 * attempt to configure the board.
	 */
	config_found(dp, &cdp->cd_link, crossprint);
}

int
crossprint(auxp, pnp)
	void *auxp;
	char *pnp;
{
	if (pnp == NULL)
		return(QUIET);
	return(UNCONF);
}


void
crossstb(dev, ia, b)
	struct device *dev;
	int ia;
	u_char b;
{
	/* generate A13-A19 for correct page */
	u_short upper_addressbits = ia >> 13;
	struct cross_device *cd = (struct cross_device *)dev;

	*(volatile u_short *)(cd->cd_zargs.va + CROSS_XLP_LATCH) =
	    upper_addressbits | CROSS_SBHE;
	*(volatile u_char *)(cd->cd_zargs.va + CROSS_MEMORY_OFFSET + 2 * ia) =
	    b;
}

u_char
crossldb(dev, ia)
	struct device *dev;
	int ia;
{
	/* generate A13-A19 for correct page */
	u_short upper_addressbits = ia >> 13;
	struct cross_device *cd = (struct cross_device *)dev;

	*(volatile u_short *)(cd->cd_zargs.va + CROSS_XLP_LATCH) =
	    upper_addressbits | CROSS_SBHE;
	return *(volatile u_char *)(cd->cd_zargs.va + CROSS_MEMORY_OFFSET +
            2 * ia);
}

void
crossstw(dev, ia, w)
	struct device *dev;
	int ia;
	u_short w;
{
	/* generate A13-A19 for correct page */
	u_short upper_addressbits = ia >> 13;
	struct cross_device *cd = (struct cross_device *)dev;
 	
	*(volatile u_short *)(cd->cd_zargs.va + CROSS_XLP_LATCH) =
	    upper_addressbits | CROSS_SBHE;
#ifdef DEBUG
	if (crossdebug)
		printf("outw 0x%x,0x%x\n", ia, w);
#endif
	*(volatile u_short *)(cd->cd_zargs.va + CROSS_MEMORY_OFFSET + 2 * ia) =
	    w;
}

u_short
crossldw(dev, ia)
	struct device *dev;
	int ia;
{
	/* generate A13-A19 for correct page */
	u_short upper_addressbits = ia >> 13;
	struct cross_device *cd = (struct cross_device *)dev;
	u_short retval;

	*(volatile u_short *)(cd->cd_zargs.va + CROSS_XLP_LATCH) =
	    upper_addressbits | CROSS_SBHE;
	retval = *(volatile u_short *)(cd->cd_zargs.va + CROSS_MEMORY_OFFSET +
	    2 * ia);
#ifdef DEBUG
	if (crossdebug)
		printf("ldw 0x%x => 0x%x\n", ia, retval);
#endif
	return retval;
}

static cross_int_map[] = {
    0, 0, 0, 0, CROSS_IRQ3, CROSS_IRQ4, CROSS_IRQ5, CROSS_IRQ6, CROSS_IRQ7, 0,
    CROSS_IRQ9, CROSS_IRQ10, CROSS_IRQ11, CROSS_IRQ12, 0, CROSS_IRQ14,
    CROSS_IRQ15
};

#if 0
/* XXX We don't care about the priority yet, although we ought to.  */
void
crossaddint(dev, irq, func, arg, pri)
	struct device *dev;
	int irq;
	int (*func)();
	void *arg;
	int pri;
{
	struct cross_device *cd = (struct cross_device *)dev;
	int s = splhigh();
	int bit = cross_int_map[irq + 1];

	if (!bit) {
		log(LOG_WARNING, "Registration of unknown ISA interrupt %d\n",
		    irq);
		goto out;
	}
	if (cd->cd_imask & 1 << bit) {
		log(LOG_WARNING, "ISA interrupt %d already handled\n", irq);
		goto out;
	}
	cd->cd_imask |= (1 << bit);
        CROSS_ENABLE_INTS (cd->cd_zargs.va, cd->cd_imask);
	cd->cd_ifunc[bit] = func;
	cd->cd_ipri[bit] = pri;
	cd->cd_iarg[bit] = arg;
out:
	splx(s);
}

void
crossremint(dev, irq)
	struct device *dev;
	int irq;
{
	struct cross_device *cd = (struct cross_device *)dev;
	int s = splhigh();
	int bit = cross_int_map[irq + 1];

	cd->cd_imask &= ~(1 << bit);
        CROSS_ENABLE_INTS (cd->cd_zargs.va, cd->cd_imask);
	splx(s);
}
#endif
struct crossintr_desc {
	struct	isr cid_isr;
	int	cid_mask;
        int     (*cid_fun)(void *);
        void    *cid_arg;
};

static struct crossintr_desc *crid[16];	/* XXX */

int
crossintr(cid)
	struct crossintr_desc *cid;
{
	return (CROSS_GET_STATUS (crossp->cd_zargs.va) & cid->cid_mask) ?
	    (*cid->cid_fun)(cid->cid_arg) : 0;
}

void *
cross_establish_intr(intr, type, level, ih_fun, ih_arg, ih_what)
        int intr;
        int type;
        int level;
        int (*ih_fun)(void *);
        void *ih_arg;
	char *ih_what;
{
	if (crid[intr]) {
		log(LOG_WARNING, "ISA interrupt %d already handled\n", intr);
		return 0;
	}
	MALLOC(crid[intr], struct crossintr_desc *,
	    sizeof(struct crossintr_desc), M_DEVBUF, M_WAITOK);
	crid[intr]->cid_isr.isr_intr = crossintr;
	crid[intr]->cid_isr.isr_arg = crid[intr];
	crid[intr]->cid_isr.isr_ipl = 6;
	crid[intr]->cid_isr.isr_mapped_ipl = level;
	crid[intr]->cid_mask = 1 << cross_int_map[intr + 1];
	crid[intr]->cid_fun = ih_fun;
	crid[intr]->cid_arg = ih_arg;
	add_isr (&crid[intr]->cid_isr);
	crossp->cd_imask |= 1 << cross_int_map[intr + 1];
        CROSS_ENABLE_INTS (crossp->cd_zargs.va, crossp->cd_imask);
	return &crid[intr];
}

void
cross_disestablish_intr(handler)
        void  *handler;
{
        struct crossintr_desc **cid = handler;

	remove_isr(&(*cid)->cid_isr);
	crossp->cd_imask &= ~(*cid)->cid_mask;
	FREE(*cid, M_DEVBUF);
	*cid = 0;
        CROSS_ENABLE_INTS (crossp->cd_zargs.va, crossp->cd_imask);
}
