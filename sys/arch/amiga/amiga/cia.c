/*	$NetBSD: cia.c,v 1.6 1995/02/12 19:34:17 chopps Exp $	*/

/*
 * Copyright (c) 1993 Markus Wild
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
 *      This product includes software developed by Markus Wild.
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
/*
 *  this file provides an interface to CIA-generated interrupts.
 *  Since the interrupt control register of a CIA is cleared
 *  when it's read, it is essential that different interrupt
 *  sources are managed from one central handler, or interrupts
 *  can get lost. 
 *
 *  if you write a handler dealing with a yet unused interrupt
 *  bit (handler == not_used), enter your interrupt handler 
 *  in the appropriate table below. If your handler must poll
 *  for an interrupt flag to come active, *always* call
 *  dispatch_cia_ints() afterwards with bits in the mask
 *  register your code didn't already deal with. 
 */
#include <sys/types.h>
#include <sys/cdefs.h>
#include <amiga/amiga/cia.h>
#include "par.h"
#include "kbd.h"

struct cia_intr_dispatch {
  u_char	mask;
  void		(*handler) __P ((int));
};

static void not_used __P((int));
void kbdintr  __P((int));
void parintr  __P((int));

/* handlers for CIA-A (IPL-2) */
static struct cia_intr_dispatch ciaa_ints[] = {
	{ CIA_ICR_TA,		not_used },
	{ CIA_ICR_TB,		not_used },
	{ CIA_ICR_ALARM,	not_used },
#if NKBD > 0
	{ CIA_ICR_SP,	kbdintr },
#else
	{ CIA_ICR_SP,	not_used },
#endif
#if NPAR > 0
	{ CIA_ICR_FLG,	parintr },
#else
	{ CIA_ICR_FLG,	not_used },
#endif
	{ 0,		0 },
};

/* handlers for CIA-B (IPL-6) */
static struct cia_intr_dispatch ciab_ints[] = {
	{ CIA_ICR_TA,	not_used },	/* used directly in locore.s */
	{ CIA_ICR_TB,	not_used },	/* "" */
	{ CIA_ICR_ALARM,	not_used },
	{ CIA_ICR_SP,	not_used },
	{ CIA_ICR_FLG,	not_used },
	{ 0,		0 },
};



void
dispatch_cia_ints(which, mask)
	int which;
	int mask;
{
	struct cia_intr_dispatch *disp;

	disp = (which == 0) ? ciaa_ints : ciab_ints;

	for (;disp->mask; disp++)
		if (mask & disp->mask)
			disp->handler(disp->mask);
}

void
ciaa_intr()
{
	dispatch_cia_ints (0, ciaa.icr);
}

/*
 * NOTE: ciab_intr() is *not* currently called. If you want to support
 * the FLG interrupt, which is used to indicate a disk-index
 * interrupt, you'll have to hack a call to ciab_intr() into
 * the lev6 interrupt handler in locore.s !
 */
void
ciab_intr()
{
	dispatch_cia_ints (1, ciab.icr);
}


static void
not_used (mask)
     int mask;
{
}
