/*	$OpenBSD: intr.c,v 1.1 2004/11/25 18:32:10 miod Exp $	*/
/*	$NetBSD: intr.c,v 1.2 1998/08/25 04:03:56 scottr Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass, Gordon W. Ross, and Jason R. Thorpe.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Link and dispatch interrupts.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/vmmeter.h>

#include <uvm/uvm_extern.h>

#include <net/netisr.h>

#include <machine/cpu.h>
#include <machine/intr.h>

#define	NISR	8
#define	ISRLOC	0x18

static int intr_noint (void *);
void netintr (void);

static int ((*intr_func[NISR]) (void *)) = {
	intr_noint,
	intr_noint,
	intr_noint,
	intr_noint,
	intr_noint,
	intr_noint,
	intr_noint,
	intr_noint
};
static void *intr_arg[NISR] = {
	(void *)0,
	(void *)1,
	(void *)2,
	(void *)3,
	(void *)4,
	(void *)5,
	(void *)6,
	(void *)7
};

#ifdef DEBUG
int	intr_debug = 0;
#endif

extern	int intrcnt[];		/* from locore.s */

/*
 * Establish an autovectored interrupt handler.
 * Called by driver attach functions.
 *
 * XXX Warning!  DO NOT use Macintosh ROM traps from an interrupt handler
 * established by this routine, either directly or indirectly, without
 * properly saving and restoring all registers.  If not, chaos _will_
 * ensue!  (sar 19980806)
 */
void
intr_establish(func, arg, ipl)
	int (*func) (void *);
	void *arg;
	int ipl;
{
	if ((ipl < 0) || (ipl >= NISR))
		panic("intr_establish: bad ipl %d", ipl);

#ifdef DIAGNOSTIC
	if (intr_func[ipl] != intr_noint)
		printf("intr_establish: attempt to share ipl %d\n", ipl);
#endif

	intr_func[ipl] = func;
	intr_arg[ipl] = arg;
}

/*
 * Disestablish an interrupt handler.
 */
void
intr_disestablish(ipl)
	int ipl;
{
	if ((ipl < 0) || (ipl >= NISR))
		panic("intr_disestablish: bad ipl %d", ipl);

	intr_func[ipl] = intr_noint;
	intr_arg[ipl] = (void *)ipl;
}

/*
 * This is the dispatcher called by the low-level
 * assembly language interrupt routine.
 *
 * XXX Note: see the warning in intr_establish()
 */
void
intr_dispatch(evec)
	int evec;		/* format | vector offset */
{
	int ipl, vec;

	vec = (evec & 0xfff) >> 2;
#ifdef DIAGNOSTIC
	if ((vec < ISRLOC) || (vec >= (ISRLOC + NISR)))
		panic("intr_dispatch: bad vec 0x%x\n", vec);
#endif
	ipl = vec - ISRLOC;

	intrcnt[ipl]++;
	uvmexp.intrs++;

	(void)(*intr_func[ipl])(intr_arg[ipl]);
}

/*
 * Default interrupt handler:  do nothing.
 */
static int
intr_noint(arg)
	void *arg;
{
#ifdef DEBUG
	if (intr_debug)
		printf("intr_noint: ipl %d\n", (int)arg);
#endif
	return 0;
}

int netisr;

void
netintr()
{
	int s, isr;

	for (;;) {
		s = splimp();
		isr = netisr;
		netisr = 0;
		splx(s);
		
		if (isr == 0)
			return;

#define DONETISR(bit, fn) do {		\
	if (isr & (1 << bit))		\
		(fn)();			\
} while (0)

#include <net/netisr_dispatch.h>

#undef  DONETISR
	}
}
