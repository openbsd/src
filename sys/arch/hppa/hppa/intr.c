/*	$OpenBSD: intr.c,v 1.3 2002/09/23 06:11:47 mickey Exp $	*/

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

/* #define INTRDEBUG */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/reg.h>

#ifdef DDB
#include <machine/db_machdep.h>
#endif

#ifdef INTRDEBUG
#include <ddb/db_output.h>
#endif

/* all the interrupts, minus cpu clock, which is the last */
struct cpu_intr_vector {
	struct evcnt evcnt;
	int pri;
	int (*handler)(void *);
	void *arg;
} cpu_intr_vectors[CPU_NINTS];

void *
cpu_intr_establish(pri, irq, handler, arg, dv)
	int pri, irq;
	int (*handler)(void *);
	void *arg;
	struct device *dv;
{
	register struct cpu_intr_vector *iv;

	if (0 <= irq && irq < CPU_NINTS && cpu_intr_vectors[irq].handler)
		return NULL;

	iv = &cpu_intr_vectors[irq];
	iv->pri = pri;
	iv->handler = handler;
	iv->arg = arg;
	evcnt_attach(dv, dv->dv_xname, &iv->evcnt);

	return iv;
}

void
cpu_intr(frame)
	struct trapframe *frame;
{
	u_int32_t eirr = 0, r;
	register struct cpu_intr_vector *iv;
	register int bit;

	do {
		mfctl(CR_EIRR, r);
		eirr |= r;
#ifdef INTRDEBUG
		if (eirr & 0x7fffffff)
			db_printf ("cpu_intr: 0x%08x & 0x%08x\n",
			    eirr, frame->tf_eiem);
#endif
		eirr &= frame->tf_eiem;
		bit = ffs(eirr) - 1;
		if (bit >= 0) {
			mtctl(1 << bit, CR_EIRR);
			eirr &= ~(1 << bit);
			/* ((struct iomod *)cpu_gethpa(0))->io_eir = 0; */
			if (bit != 31) {
#ifdef INTRDEBUG
				db_printf ("cpu_intr: 0x%08x\n", (1 << bit));
#endif
				frame->tf_flags |= TFF_INTR;
			} else
				frame->tf_flags &= ~TFF_INTR;

			iv = &cpu_intr_vectors[bit];
			if (iv->handler) {
				register int s, r;

				iv->evcnt.ev_count++;
				s = splraise(iv->pri);
				/* no arg means pass the frame */
				r = (iv->handler)(iv->arg? iv->arg:frame);
				splx(s);
#ifdef INTRDEBUG
				if (!r)
					db_printf ("%s: can't handle interrupt\n",
						   iv->evcnt.ev_name);
#endif
			}
#ifdef INTRDEBUG
			else
				db_printf ("cpu_intr: stray interrupt %d\n", bit);
#endif
		}
	} while (eirr);
}
