/*	$OpenBSD: psc.c,v 1.5 2004/12/08 20:35:03 miod Exp $	*/
/*	$NetBSD: psc.c,v 1.5 1998/08/12 05:42:46 scottr Exp $	*/


/*-
 * Copyright (c) 1997 David Huang <khym@bga.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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
 *
 */

/*
 * This handles registration/unregistration of PSC (Peripheral
 * Subsystem Controller) interrupts. The PSC is used only on the
 * Centris/Quadra 660av and the Quadra 840av.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/psc.h>

int		psc_lev3_intr(void *);
static void	psc_lev3_noint(void *);
int		psc_lev4_intr(void *);
static int	psc_lev4_noint(void *);
int		psc_lev5_intr(void *);
static void	psc_lev5_noint(void *);
int		psc_lev6_intr(void *);
static void	psc_lev6_noint(void *);

void	(*psc3_ihandler)(void *) = psc_lev3_noint;
void	*psc3_iarg;

int (*psc4_itab[4])(void *) = {
	psc_lev4_noint,	/* 0 */
	psc_lev4_noint,	/* 1 */
	psc_lev4_noint,	/* 2 */
	psc_lev4_noint  /* 3 */
};

void *psc4_iarg[4] = {
	(void *)0, (void *)1, (void *)2, (void *)3
};

void (*psc5_itab[2])(void *) = {
	psc_lev5_noint, /* 0 */
	psc_lev5_noint  /* 1 */
};

void *psc5_iarg[2] = {
	(void *)0, (void *)1
};

void (*psc6_itab[3])(void *) = {
	psc_lev6_noint, /* 0 */
	psc_lev6_noint, /* 1 */
	psc_lev6_noint  /* 2 */
};

void *psc6_iarg[3] = {
	(void *)0, (void *)1, (void *)2
};

/*
 * Setup the interrupt vectors and disable most of the PSC interrupts
 */
void
psc_init()
{
	/*
	 * Only Quadra AVs have a PSC.
	 */
	if (current_mac_model->class == MACH_CLASSAV) {
		intr_establish(psc_lev3_intr, NULL, 3, "psc");
		intr_establish(psc_lev4_intr, NULL, 4, "psc");
		intr_establish(psc_lev5_intr, NULL, 5, "psc");
		intr_establish(psc_lev6_intr, NULL, 6, "psc");
		psc_reg1(PSC_LEV3_IER) = 0x01; /* disable level 3 interrupts */
		psc_reg1(PSC_LEV4_IER) = 0x09; /* disable level 4 interrupts */
		psc_reg1(PSC_LEV4_IER) = 0x86; /* except for SCC */
		psc_reg1(PSC_LEV5_IER) = 0x03; /* disable level 5 interrupts */
		psc_reg1(PSC_LEV6_IER) = 0x07; /* disable level 6 interrupts */
	}
}

int
add_psc_lev3_intr(handler, arg)
	void (*handler)(void *);
	void *arg;
{
	int s;

	s = splhigh();

	psc3_ihandler = handler;
	psc3_iarg = arg;

	splx(s);

	return 1;
}

int
remove_psc_lev3_intr()
{
	return add_psc_lev3_intr(psc_lev3_noint, (void *)0);
}

int
psc_lev3_intr(arg)
	void *arg;
{
	u_int8_t intbits;

	while ((intbits = psc_reg1(PSC_LEV3_ISR)) != psc_reg1(PSC_LEV3_ISR))
		;
	intbits &= 0x1 & psc_reg1(PSC_LEV3_IER);

	if (intbits)
		psc3_ihandler(psc3_iarg);

	return 0;
}

static void
psc_lev3_noint(arg)
	void *arg;
{
#ifdef DEBUG
	printf("psc_lev3_noint\n");
#endif
}

int
psc_lev4_intr(arg)
	void * arg;
{
	u_int8_t intbits, bitnum;
	u_int mask;

	while ((intbits = psc_reg1(PSC_LEV4_ISR)) != psc_reg1(PSC_LEV4_ISR))
		;
	intbits &= 0xf & psc_reg1(PSC_LEV4_IER);

	mask = 1;
	bitnum = 0;
	do {
		if (intbits & mask)
			psc4_itab[bitnum](psc4_iarg[bitnum]);
		mask <<= 1;
	} while (intbits >= mask && ++bitnum);

	return 0;
}

int
add_psc_lev4_intr(dev, handler, arg)
	int dev;
	int (*handler)(void *);
	void *arg;
{
	int s;

	if ((dev < 0) || (dev > 3))
		return 0;

	s = splhigh();

	psc4_itab[dev] = handler;
	psc4_iarg[dev] = arg;

	splx(s);

	return 1;
}

int
remove_psc_lev4_intr(dev)
	int dev;
{
	return add_psc_lev4_intr(dev, psc_lev4_noint, (void *)dev);
}

int
psc_lev4_noint(arg)
	void *arg;
{
#ifdef DEBUG
	printf("psc_lev4_noint: device %d\n", (int)arg);
#endif
	return 0;
}

int
psc_lev5_intr(arg)
	void *arg;
{
	u_int8_t intbits, bitnum;
	u_int mask;

	while ((intbits = psc_reg1(PSC_LEV5_ISR)) != psc_reg1(PSC_LEV5_ISR))
		;
	intbits &= 0x3 & psc_reg1(PSC_LEV5_IER);

	mask = 1;
	bitnum = 0;
	do {
		if (intbits & mask)
			psc5_itab[bitnum](psc5_iarg[bitnum]);
		mask <<= 1;
	} while (intbits >= mask && ++bitnum);

	return 0;
}

int
add_psc_lev5_intr(dev, handler, arg)
	int dev;
	void (*handler)(void *);
	void *arg;
{
	int s;

	if ((dev < 0) || (dev > 1))
		return 0;

	s = splhigh();

	psc5_itab[dev] = handler;
	psc5_iarg[dev] = arg;

	splx(s);

	return 1;
}

int
remove_psc_lev5_intr(dev)
	int dev;
{
	return add_psc_lev5_intr(dev, psc_lev5_noint, (void *)dev);
}

void
psc_lev5_noint(arg)
	void *arg;
{
#ifdef DEBUG
	printf("psc_lev5_noint: device %d\n", (int)arg);
#endif
}

int
psc_lev6_intr(arg)
	void *arg;
{
	u_int8_t intbits, bitnum;
	u_int mask;

	while ((intbits = psc_reg1(PSC_LEV6_ISR)) != psc_reg1(PSC_LEV6_ISR))
		;
	intbits &= 0x7 & psc_reg1(PSC_LEV6_IER);

	mask = 1;
	bitnum = 0;
	do {
		if (intbits & mask)
			psc6_itab[bitnum](psc6_iarg[bitnum]);
		mask <<= 1;
	} while (intbits >= mask && ++bitnum);

	return 0;
}

int
add_psc_lev6_intr(dev, handler, arg)
	int dev;
	void (*handler)(void *);
	void *arg;
{
	int s;

	if ((dev < 0) || (dev > 2))
		return 0;

	s = splhigh();

	psc6_itab[dev] = handler;
	psc6_iarg[dev] = arg;

	splx(s);

	return 1;
}

int
remove_psc_lev6_intr(dev)
	int dev;
{
	return add_psc_lev6_intr(dev, psc_lev6_noint, (void *)dev);
}

void
psc_lev6_noint(arg)
	void *arg;
{
#ifdef DEBUG
	printf("psc_lev6_noint: device %d\n", (int)arg);
#endif
}
