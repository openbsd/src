/*	$NetBSD: isr.c,v 1.1 1996/02/14 02:56:48 thorpej Exp $	*/

/*
 * Copyright (c) 1995, 1996 Jason R. Thorpe.
 * All rights reserved.
 *
 * Portions:
 * Copyright (c) 1994 Gordon W. Ross.
 * Copyright (c) 1993 Adam Glass.
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
 *    must display the following acknowledgements:
 *	This product includes software developed by Adam Glass.
 *	This product includes software developed for the NetBSD Project
 *	by Jason R. Thorpe.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Link and dispatch interrupts.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/vmmeter.h>
#include <machine/cpu.h>
#include <net/netisr.h>

#include <hp300/hp300/isr.h>

typedef LIST_HEAD(, isr) isr_list_t;
isr_list_t isr_list[NISR];

extern	int intrcnt[];		/* from locore.s */

void
isrinit()
{
	int i;

	/* Initialize the ISR lists. */
	for (i = 0; i < NISR; ++i) {
		LIST_INIT(&isr_list[i]);
	}
}

/*
 * Establish an interrupt handler.
 * Called by driver attach functions.
 */
void
isrlink(func, arg, ipl, priority)
	int (*func) __P((void *));
	void *arg;
	int ipl;
	int priority;
{
	struct isr *newisr, *curisr;
	isr_list_t *list;

	if ((ipl < 0) || (ipl >= NISR))
		panic("isrlink: bad ipl %d", ipl);

	newisr = (struct isr *)malloc(sizeof(struct isr), M_DEVBUF, M_NOWAIT);
	if (newisr == NULL)
		panic("isrlink: can't allocate space for isr");

	/* Fill in the new entry. */
	newisr->isr_func = func;
	newisr->isr_arg = arg;
	newisr->isr_ipl = ipl;
	newisr->isr_priority = priority;

	/*
	 * Some devices are particularly sensitive to interrupt
	 * handling latency.  The DCA, for example, can lose many
	 * characters if its interrupt isn't handled with reasonable
	 * speed.
	 *
	 * To work around this problem, each device can give itself a
	 * "priority".  An unbuffered DCA would give itself a higher
	 * priority than a SCSI device, for example.
	 *
	 * This is necessary because of the flat spl scheme employed by
	 * the hp300.  Each device can be set from ipl 3 to ipl 5, which
	 * in turn means that splbio, splnet, and spltty must all be at
	 * spl5.
	 *
	 * Don't blame me...I just work here.
	 */

	/*
	 * Get the appropriate ISR list.  If the list is empty, no
	 * additional work is necessary; we simply insert ourselves
	 * at the head of the list.
	 */
	list = &isr_list[ipl];
	if (list->lh_first == NULL) {
		LIST_INSERT_HEAD(list, newisr, isr_link);
		return;
	}

	/*
	 * A little extra work is required.  We traverse the list
	 * and place ourselves after any ISRs with our current (or
	 * higher) priority.
	 */
	for (curisr = list->lh_first; curisr->isr_link.le_next != NULL;
	    curisr = curisr->isr_link.le_next) {
		if (newisr->isr_priority > curisr->isr_priority) {
			LIST_INSERT_BEFORE(curisr, newisr, isr_link);
			return;
		}
	}

	/*
	 * We're the least important entry, it seems.  We just go
	 * on the end.
	 */
	LIST_INSERT_AFTER(curisr, newisr, isr_link);
}

/*
 * This is the dispatcher called by the low-level
 * assembly language interrupt routine.
 */
void
isrdispatch(evec)
	int evec;		/* format | vector offset */
{
	struct isr *isr;
	isr_list_t *list;
	int handled, ipl, vec;
	static int straycount, unexpected;

	vec = (evec & 0xfff) >> 2;
	if ((vec < ISRLOC) || (vec >= (ISRLOC + NISR)))
		panic("isrdispatch: bad vec 0x%x\n");
	ipl = vec - ISRLOC;

	intrcnt[ipl]++;
	cnt.v_intr++;

	list = &isr_list[ipl];
	if (list->lh_first == NULL) {
		printf("intrhand: ipl %d unexpected\n", ipl);
		if (++unexpected > 10)
			panic("isrdispatch: too many unexpected interrupts");
		return;
	}

	/* Give all the handlers a chance. */
	for (isr = list->lh_first ; isr != NULL; isr = isr->isr_link.le_next)
		handled |= (*isr->isr_func)(isr->isr_arg);

	if (handled)
		straycount = 0;
	else if (++straycount > 50)
		panic("isrdispatch: too many stray interrupts");
	else
		printf("isrdispatch: stray level %d interrupt\n", ipl);
}

/*
 * XXX Why on earth isn't this in a common file?!
 */
void
netintr()
{
#ifdef INET
	if (netisr & (1 << NETISR_ARP)) {
		netisr &= ~(1 << NETISR_ARP);
		arpintr();
	}
	if (netisr & (1 << NETISR_IP)) {
		netisr &= ~(1 << NETISR_IP);
		ipintr();
	}
#endif
#ifdef NS
	if (netisr & (1 << NETISR_NS)) {
		netisr &= ~(1 << NETISR_NS);
		nsintr();
	}
#endif
#ifdef ISO
	if (netisr & (1 << NETISR_ISO)) {
		netisr &= ~(1 << NETISR_ISO);
		clnlintr();
	}
#endif
#ifdef CCITT
	if (netisr & (1 << NETISR_CCITT)) {
		netisr &= ~(1 << NETISR_CCITT);
		ccittintr();
	}
#endif
#include "ppp.h"
#if NPPP > 0
	if (netisr & (1 << NETISR_PPP)) {
		netisr &= ~(1 << NETISR_PPP);
		pppintr();
	}
#endif
}
