/*	$OpenBSD: intr.c,v 1.11 2015/02/11 13:05:44 miod Exp $	*/

/*
 * Copyright (c) 2002-2004 Michael Shalayeff
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

#define INTRDEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/atomic.h>
#include <sys/device.h>
#include <sys/evcount.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/intr.h>
#include <machine/iomod.h>
#include <machine/psl.h>
#include <machine/reg.h>

struct hppa_iv {
	char pri;
	char irq;
	char flags;
#define	HPPA_IV_SOFT	0x01
	char pad;
	u_int bit;
	int (*handler)(void *);
	void *arg;
	struct hppa_iv *share;
	struct hppa_iv *next;
	struct evcount *cnt;
};

struct hppa_iv *intr_list;
struct hppa_iv intr_table[CPU_NINTS] __attribute__ ((aligned(64))) = {
	{ IPL_SOFTCLOCK, 0, HPPA_IV_SOFT, 0, 0, NULL },
	{ IPL_SOFTNET  , 0, HPPA_IV_SOFT, 0, 0, NULL },
	{ 0 },
	{ 0 },
	{ IPL_SOFTTTY  , 0, HPPA_IV_SOFT, 0, 0, NULL }
};

volatile u_long imask[NIPL] = {
	1 << (IPL_SOFTCLOCK - 1),
	1 << (IPL_SOFTNET - 1),
	0,
	0,
	1 << (IPL_SOFTTTY - 1)
};

void
cpu_intr_init(void)
{
	u_long mask;
	int level;

	for (level = NIPL - 1; level > 0; level--) {
		imask[level - 1] |= imask[level];
#ifdef INTRDEBUG
		printf("IPL %i 0x%lx\n", level - 1, imask[level - 1]);
#endif
	}

	/* Prevent hardclock from happening early. */
	mask = mfctl(CR_ITMR);
	mtctl(mask - 1, CR_ITMR);

	/* Clear unwanted interrupts. */
	mask = mfctl(CR_EIRR);
	mtctl(mask & (1UL << 63), CR_EIRR);

	/* Time to enable interrupts. */
	curcpu()->ci_psw |= PSL_I;
	ssm(PSL_I, mask);
}

int
cpu_intr_findirq(void)
{
	int irq;

	for (irq = 0; irq < CPU_NINTS; irq++)
		if (intr_table[irq].handler == NULL &&
		    intr_table[irq].pri == 0)
			return irq;

	return -1;
}

void *
cpu_intr_map(void *v, int pri, int irq, int (*handler)(void *), void *arg,
    const char *name)
{
	struct hppa_iv *iv, *pv = v, *ivb = pv->next;
	struct evcount *cnt;

	if (irq < 0 || irq >= CPU_NINTS)
		return (NULL);

	cnt = (struct evcount *)malloc(sizeof *cnt, M_DEVBUF, M_NOWAIT);
	if (!cnt)
		return (NULL);

	iv = &ivb[irq];
	if (iv->handler) {
		if (!pv->share) {
			free(cnt, M_DEVBUF, 0);
			return (NULL);
		} else {
			iv = pv->share;
			pv->share = iv->share;
			iv->share = ivb[irq].share;
			ivb[irq].share = iv;
		}
	}

	evcount_attach(cnt, name, NULL);
	iv->pri = pri;
	iv->irq = irq;
	iv->flags = 0;
	iv->handler = handler;
	iv->arg = arg;
	iv->cnt = cnt;
	iv->next = intr_list;
	intr_list = iv;

	return (iv);
}

void *
cpu_intr_establish(int pri, int irq, int (*handler)(void *), void *arg,
    const char *name)
{
	struct hppa_iv *iv;
	struct evcount *cnt;

	if (irq < 0 || irq >= CPU_NINTS || intr_table[irq].handler)
		return (NULL);

	if ((intr_table[irq].flags & HPPA_IV_SOFT) != 0)
		return (NULL);

	cnt = (struct evcount *)malloc(sizeof *cnt, M_DEVBUF, M_NOWAIT);
	if (!cnt)
		return (NULL);

	imask[pri - 1] |= (1UL << irq);

	iv = &intr_table[irq];
	iv->pri = pri;
	iv->irq = irq;
	iv->bit = 1 << irq;
	iv->flags = 0;
	iv->handler = handler;
	iv->arg = arg;
	iv->cnt = cnt;
	iv->next = NULL;
	iv->share = NULL;

	evcount_attach(cnt, name, NULL);

	return (iv);
}

void
cpu_intr(void *v)
{
	struct cpu_info *ci = curcpu();
	struct trapframe *frame = v;
	struct hppa_iv *iv;
	int pri, r, s, bit;
	u_long mask, tmp;
	void *arg;

	ci->ci_ipending |= mfctl(CR_EIRR);

	s = ci->ci_cpl;
	if (ci->ci_in_intr++)
		frame->tf_flags |= TFF_INTR;

	/* Process higher priority interrupts first. */
	for (pri = NIPL - 1; pri > s; pri--) {

		mask = imask[pri] ^ imask[pri - 1];

		while (ci->ci_ipending & mask) {

			bit = flsl(ci->ci_ipending & mask) - 1;
			iv = &intr_table[bit];

#ifdef INTRDEBUG
			if (iv->pri <= s)
				panic("irq %i: handler pri %i <= ipl %i\n",
				    bit, iv->pri, s);
#endif

			ci->ci_ipending &= ~(1UL << bit);
			mtctl(1UL << bit, CR_EIRR);
			ci->ci_ipending |= mfctl(CR_EIRR);

			uvmexp.intrs++;
			if (iv->flags & HPPA_IV_SOFT)
				uvmexp.softs++;

			ci->ci_cpl = iv->pri;
			mtctl(imask[ci->ci_cpl], CR_EIEM);
		       	ssm(PSL_I, tmp);

			for (r = iv->flags & HPPA_IV_SOFT;
			     iv && iv->handler; iv = iv->next) {
				/* No arg means pass the frame. */
				arg = iv->arg ? iv->arg : v;
				if ((iv->handler)(arg) == 1) {
					if (iv->cnt)
						iv->cnt->ec_count++;
					r |= 1;
				}
			}

			rsm(PSL_I, tmp);
		}
	}
	ci->ci_in_intr--;
	ci->ci_cpl = s;

	mtctl(imask[ci->ci_cpl], CR_EIEM);
	ssm(PSL_I, tmp);
}

void *
softintr_establish(int pri, void (*handler)(void *), void *arg)
{
	struct hppa_iv *iv;
	int irq;

	if (pri == IPL_TTY)
		pri = IPL_SOFTTTY;

	irq = pri - 1;
	iv = &intr_table[irq];
	if ((iv->flags & HPPA_IV_SOFT) == 0 || iv->pri != pri)
		return (NULL);

	if (iv->handler) {
		struct hppa_iv *nv;

		nv = malloc(sizeof *iv, M_DEVBUF, M_NOWAIT);
		if (!nv)
			return (NULL);
		while (iv->next)
			iv = iv->next;
		iv->next = nv;
		iv = nv;
	} else
		imask[pri - 1] |= (1 << irq);

	iv->pri = pri;
	iv->irq = 0;
	iv->bit = 1 << irq;
	iv->flags = HPPA_IV_SOFT;
	iv->handler = (int (*)(void *))handler;	/* XXX */
	iv->arg = arg;
	iv->cnt = NULL;
	iv->next = NULL;
	iv->share = NULL;

	return (iv);
}

void
softintr_disestablish(void *cookie)
{
	struct hppa_iv *iv = cookie;
	int irq = iv->pri - 1;

	if (&intr_table[irq] == cookie) {
		if (iv->next) {
			struct hppa_iv *nv = iv->next;

			iv->handler = nv->handler;
			iv->arg = nv->arg;
			iv->next = nv->next;
			free(nv, M_DEVBUF, 0);
			return;
		} else {
			iv->handler = NULL;
			iv->arg = NULL;
			return;
		}
	}

	for (iv = &intr_table[irq]; iv; iv = iv->next) {
		if (iv->next == cookie) {
			iv->next = iv->next->next;
			free(cookie, M_DEVBUF, 0);
			return;
		}
	}
}

void
softintr_schedule(void *cookie)
{
	struct hppa_iv *iv = cookie;

	softintr(1 << (iv->pri - 1));
}
