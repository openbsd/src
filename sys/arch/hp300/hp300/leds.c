/*	$OpenBSD: leds.c,v 1.8 2005/01/08 22:13:24 miod Exp $	*/
/*	$NetBSD: leds.c,v 1.1 1997/05/05 20:54:35 thorpej Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: machdep.c 1.74 92/12/20$
 *
 *	@(#)machdep.c	8.10 (Berkeley) 4/20/94
 */

#include <sys/param.h>

#include <uvm/uvm_extern.h>

#include <hp300/hp300/leds.h>

extern caddr_t	ledbase;	/* kva of LED page */
u_int8_t	*ledaddr;	/* actual address of LEDs */
static int	inledcontrol;	/* mutex */

/*
 * Map the LED page and setup the KVA to access it.
 */
void
ledinit()
{

	pmap_enter(pmap_kernel(), (vaddr_t)ledbase, (paddr_t)LED_ADDR,
	    VM_PROT_READ|VM_PROT_WRITE, VM_PROT_READ|VM_PROT_WRITE|PMAP_WIRED);
	pmap_update(pmap_kernel());
	ledaddr = (u_int8_t *) ((long)ledbase | (LED_ADDR & PGOFSET));
}

/*
 * Do lights:
 *	`ons' is a mask of LEDs to turn on,
 *	`offs' is a mask of LEDs to turn off,
 *	`togs' is a mask of LEDs to toggle.
 * Note we don't use splclock/splx for mutual exclusion.
 * They are expensive and we really don't need to be that precise.
 * Besides we would like to be able to profile this routine.
 */
void
ledcontrol(ons, offs, togs)
	int ons, offs, togs;
{
	static u_int8_t currentleds;
	u_int8_t leds;

	if (inledcontrol)
		return;

	inledcontrol = 1;
	leds = currentleds;
	if (ons)
		leds |= ons;
	if (offs)
		leds &= ~offs;
	if (togs)
		leds ^= togs;
	currentleds = leds;
	*ledaddr = ~leds;
	inledcontrol = 0;
}
