/*	$OpenBSD: ggbus.c,v 1.2 1996/02/27 15:40:56 niklas Exp $	*/
/*	$NetBSD: ggbus.c,v 1.1 1994/07/08 23:32:17 niklas Exp $	*/

/*
 * Copyright (c) 1994, 1995 Niklas Hallqvist
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
 *      This product includes software developed by Niklas Hallqvist.
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
#include <amiga/isa/ggbusvar.h>
#include <amiga/isa/ggbusreg.h>

int ggdebug = 0;
int ggstrayints = 0;

/* This is OK because we only allow one ISA bus.  */
struct ggbus_device *ggbusp;

void	ggbusattach __P((struct device *, struct device *, void *));
int	ggbusmatch __P((struct device *, void *, void *));
int	ggbusprint __P((void *auxp, char *));
void	ggbusstb __P((struct device *, int, u_char));
u_char	ggbusldb __P((struct device *, int));
void	ggbusstw __P((struct device *, int, u_short));
u_short ggbusldw __P((struct device *, int));
void	*ggbus_establish_intr __P((int intr, int type, int level,
				   int (*ih_fun) (void *), void *ih_arg,
				   char *ih_what));
void	ggbus_disestablish_intr __P((void *handler));

struct isa_intr_fcns ggbus_intr_fcns = {
	0 /* ggbus_intr_setup */,	ggbus_establish_intr,
	ggbus_disestablish_intr,	0 /* ggbus_iointr */
};

struct cfdriver ggbuscd = {
	NULL, "ggbus", ggbusmatch, ggbusattach, 
	DV_DULL, sizeof(struct ggbus_device), 0
};

int
ggbusmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct zbus_args *zap = aux;

	/*
	 * Check manufacturer and product id.
	 */
	if (zap->manid == 2150 && zap->prodid == 1)
		return(1);
	return(0);
}

void
ggbusattach(pdp, dp, auxp)
	struct device *pdp, *dp;
	void *auxp;
{
	struct zbus_args *zap = auxp;
	struct ggbus_device *gdp = (struct ggbus_device *)dp;

	ggbusp = gdp;
	bcopy(zap, &gdp->gd_zargs, sizeof(struct zbus_args));
	gdp->gd_link.il_dev = dp;
	if (gdp->gd_zargs.serno >= 2)
	  {
	    gdp->gd_link.il_ldb = ggbusldb;
	    gdp->gd_link.il_stb = ggbusstb;
	    gdp->gd_link.il_ldw = ggbusldw;
	    gdp->gd_link.il_stw = ggbusstw;
	  }
	else
	  {
	    gdp->gd_link.il_ldb = 0;
	    gdp->gd_link.il_stb = 0;
	    gdp->gd_link.il_ldw = 0;
	    gdp->gd_link.il_stw = 0;
	  }
	gdp->gd_imask = 0;

	isa_intr_fcns = &ggbus_intr_fcns;
	isa_pio_fcns = &ggbus_pio_fcns;

	if (gdp->gd_zargs.serno >= 2)
	  {
	    /* XXX turn on wait states unconditionally for now. */
	    GG2_ENABLE_WAIT(zap->va);
	    GG2_ENABLE_INTS(zap->va);
	  }

	printf(": pa 0x%08x va 0x%08x size 0x%x\n", zap->pa, zap->va, zap->size);

	/*
	 * attempt to configure the board.
	 */
	config_found(dp, &gdp->gd_link, ggbusprint);
}

int
ggbusprint(auxp, pnp)
	void *auxp;
	char *pnp;
{
	if (pnp == NULL)
		return(QUIET);
	return(UNCONF);
}


void
ggbusstb(dev, ia, b)
	struct device *dev;
	int ia;
	u_char b;
{
	struct ggbus_device *gd = (struct ggbus_device *)dev;

	*(volatile u_char *)(gd->gd_zargs.va + GG2_MEMORY_OFFSET + 2 * ia + 1) = b;
}

u_char
ggbusldb(dev, ia)
	struct device *dev;
	int ia;
{
	struct ggbus_device *gd = (struct ggbus_device *)dev;
	u_char retval =
	    *(volatile u_char *)(gd->gd_zargs.va + GG2_MEMORY_OFFSET + 2 * ia + 1);

#ifdef DEBUG
	if (ggdebug)
		printf("ldb 0x%x => 0x%x\n", ia, retval);
#endif
	return retval;
}

void
ggbusstw(dev, ia, w)
	struct device *dev;
	int ia;
	u_short w;
{
	struct ggbus_device *gd = (struct ggbus_device *)dev;

	*(volatile u_short *)(gd->gd_zargs.va + GG2_MEMORY_OFFSET + 2 * ia) = swap(w);
}

u_short
ggbusldw(dev, ia)
	struct device *dev;
	int ia;
{
	struct ggbus_device *gd = (struct ggbus_device *)dev;
	u_short retval =
	    swap(*(volatile u_short *)(gd->gd_zargs.va + GG2_MEMORY_OFFSET + 2 * ia));

#ifdef DEBUG
	if (ggdebug)
		printf("ldw 0x%x => 0x%x\n", ia, retval);
#endif
	return retval;
}

static ggbus_int_map[] = {
    0, 0, 0, 0, GG2_IRQ3, GG2_IRQ4, GG2_IRQ5, GG2_IRQ6, GG2_IRQ7, 0,
    GG2_IRQ9, GG2_IRQ10, GG2_IRQ11, GG2_IRQ12, 0, GG2_IRQ14, GG2_IRQ15
};

struct ggintr_desc {
	struct	isr gid_isr;
	int	gid_mask;
	int	(*gid_fun)(void *);
	void	*gid_arg;
};

static struct ggintr_desc *ggid[16];	/* XXX */

int
ggbusintr(gid)
	struct ggintr_desc *gid;
{
	return (GG2_GET_STATUS (ggbusp->gd_zargs.va) & gid->gid_mask) ?
	    (*gid->gid_fun)(gid->gid_arg) : 0;
}

void *
ggbus_establish_intr(intr, type, level, ih_fun, ih_arg, ih_what)
	int intr;
	int type;
	int level;
	int (*ih_fun)(void *);
	void *ih_arg;
	char *ih_what;
{
	if (ggid[intr]) {
		log(LOG_WARNING, "ISA interrupt %d already handled\n", intr);
		return 0;
	}
	MALLOC(ggid[intr], struct ggintr_desc *, sizeof(struct ggintr_desc),
	    M_DEVBUF, M_WAITOK);
	ggid[intr]->gid_isr.isr_intr = ggbusintr;
	ggid[intr]->gid_isr.isr_arg = ggid[intr];
	ggid[intr]->gid_isr.isr_ipl = 6;
	ggid[intr]->gid_isr.isr_mapped_ipl = level;
	ggid[intr]->gid_mask = 1 << ggbus_int_map[intr + 1];
	ggid[intr]->gid_fun = ih_fun;
	ggid[intr]->gid_arg = ih_arg;
	add_isr(&ggid[intr]->gid_isr);
	return &ggid[intr];
}

void
ggbus_disestablish_intr(handler)
	void  *handler;
{
	struct ggintr_desc **gid = handler;

	remove_isr(&(*gid)->gid_isr);
	FREE(*gid, M_DEVBUF);
	*gid = 0;
}
