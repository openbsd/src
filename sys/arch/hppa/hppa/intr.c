/*	$OpenBSD: intr.c,v 1.4 2002/12/17 21:54:25 mickey Exp $	*/

/*
 * Copyright (c) 2002 Michael Shalayeff
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
 *      This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <net/netisr.h>

#include <machine/autoconf.h>
#include <machine/frame.h>
#include <machine/reg.h>

void softnet(void);
void softtty(void);

struct hppa_iv {
	char pri;
	char bit;
	char flags;
#define	HPPA_IV_CALL	0x01
#define	HPPA_IV_SOFT	0x02
	char pad;
	int (*handler)(void *);
	void *arg;
	struct hppa_iv *next;
} __attribute__((__packed__));

register_t kpsw = PSL_Q | PSL_P | PSL_C | PSL_D;
volatile int cpl = IPL_NESTED;
volatile u_long ipending, imask[NIPL];
u_long cpu_mask;   
struct hppa_iv *intr_list, intr_store[8*CPU_NINTS], *intr_more = intr_store;
struct hppa_iv intr_table[CPU_NINTS] = {
	{ IPL_SOFTCLOCK, 0, HPPA_IV_SOFT, 0, (int (*)(void *))&softclock },
	{ IPL_SOFTNET  , 0, HPPA_IV_SOFT, 0, (int (*)(void *))&softnet },
	{ 0 }, { 0 },
	{ IPL_SOFTTTY  , 0, HPPA_IV_SOFT, 0, (int (*)(void *))&softtty }
};

#ifdef DIAGNOSTIC
void
splassert_check(int wantipl, const char *func)
{
	if (cpl < wantipl) {
		splassert_fail(wantipl, cpl, func);
	}
}
#endif

void
softnet(void)
{
	int ni;

	/* use atomic "load & clear" */
	__asm __volatile(
	    "ldcws	0(%2), %0": "=&r" (ni), "+m" (netisr): "r" (&netisr));
#define DONETISR(m,c) if (ni & (1 << (m))) c()
#include <net/netisr_dispatch.h>
}

void
softtty(void)
{

}

void
cpu_intr_init()
{
	u_long mask = cpu_mask | SOFTINT_MASK;
	int level;

	/* map the shared ints */
	while (intr_list) {
		struct hppa_iv *iv = intr_list;
		int bit = ffs(imask[(int)iv->pri]);
		intr_list = iv->next;

		if (!bit--) {
			bit = ffs(~mask);
			if (!bit--)
				panic("cpu_intr_init: out of bits");

			iv->next = NULL;
			iv->bit = 31 - bit;
			intr_table[bit] = *iv;
			mask |= (1 << bit);
			imask[(int)iv->pri] |= (1 << bit);
		} else {
			iv->bit = 31 - bit;
			iv->next = intr_table[bit].next;
			intr_table[bit].next = iv;
		}
	}

	/* match the init for intr_table */
	imask[IPL_SOFTCLOCK] = 1 << (IPL_SOFTCLOCK - 1);
	imask[IPL_SOFTNET  ] = 1 << (IPL_SOFTNET - 1);
	imask[IPL_SOFTTTY  ] = 1 << (IPL_SOFTTTY - 1);

	for (level = 0; level < NIPL - 1; level++)
		imask[level + 1] |= imask[level];

	printf("biomask 0x%x netmask 0x%x ttymask 0x%x\n",
	    imask[IPL_BIO], imask[IPL_NET], imask[IPL_TTY]);

	/* XXX the whacky trick is to prevent hardclock from happenning */
	mfctl(CR_ITMR, mask);
	mtctl(mask - 1, CR_ITMR);

	cold = 0;

	/* in spl*() we trust, clock is enabled in initclocks() */
	mtctl(cpu_mask, CR_EIEM);
	kpsw |= PSL_I;
	__asm __volatile("ssm %0, %%r0" :: "i" (PSL_I));
}

void *
cpu_intr_map(void *v, int pri, int irq, int (*handler)(void *), void *arg, struct device *dv)
{
	struct hppa_iv *iv, *pv = v, *ivb = pv->next;

	if (irq < 0 || irq >= CPU_NINTS || ivb[irq].handler)
		return (NULL);

	iv = &ivb[irq];
	iv->pri = pri;
	iv->bit = irq;
	iv->flags = 0;
	iv->handler = handler;
	iv->arg = arg;
	iv->next = intr_list;
	intr_list = iv;

	return (iv);
}

void *
cpu_intr_establish(int pri, int irq, int (*handler)(void *), void *arg, struct device *dv)
{
	struct hppa_iv *iv;

	if (irq < 0 || irq >= CPU_NINTS || intr_table[irq].handler)
		return (NULL);

	cpu_mask |= (1 << irq);
	imask[pri] |= (1 << irq);

	iv = &intr_table[irq];
	iv->pri = pri;
	iv->bit = 31 - irq;
	iv->flags = 0;
	iv->handler = handler;
	iv->arg = arg;

	if (pri == IPL_NESTED) {
		iv->flags = HPPA_IV_CALL;
		iv->next = intr_more;
		intr_more += CPU_NINTS;
	} else
		iv->next = NULL;

	return (iv);
}

void
cpu_intr(void *v)
{
	struct trapframe *frame = v;
	u_long mask;
	int s = cpl;

	while ((mask = ipending & ~imask[s])) {
		int r, bit = ffs(mask) - 1;
		struct hppa_iv *iv = &intr_table[bit];

		ipending &= ~(1L << bit);
		if (iv->flags & HPPA_IV_CALL)
			continue;

		cpl = iv->pri;
		mtctl(frame->tf_eiem, CR_EIEM);
		for (r = iv->flags & HPPA_IV_SOFT;
		    iv && iv->handler; iv = iv->next)
			/* no arg means pass the frame */
			r |= (iv->handler)(iv->arg? iv->arg : v) == 1;
#if 0	/* XXX this does not work, lasi gives us double ints */
		if (!r) {
			cpl = 0;
			printf("stray interrupt %d\n", bit);
		}
		mtctl(0, CR_EIEM);
#endif
	}
	cpl = s;
}
