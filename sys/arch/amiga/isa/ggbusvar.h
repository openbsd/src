/*	$OpenBSD: ggbusvar.h,v 1.3 1996/06/04 13:40:14 niklas Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1996 Niklas Hallqvist
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

#ifndef _GGBUSVAR_H_
#define _GGBUSVAR_H_

/*
 * Interrupt handler chains.  ggbus_intr_establish() inserts a handler into
 * the list.  The handler is called with its (single) argument.
 */
struct intrhand {
	struct	intrhand *ih_next;
	int	(*ih_fun) __P((void *));
	void	*ih_arg;
	u_long	ih_count;
	int	ih_irq;
	char	*ih_what;

	struct	isr ih_isr;
	u_int16_t ih_mask;
	volatile u_int16_t *ih_status;
};

#define	ICU_LEN		16	/* number of ISA IRQs (XXX) */

struct ggbus_softc {
	struct	device sc_dev;

	struct	zbus_args sc_zargs;
	struct	intrhand *sc_ih[ICU_LEN];
	int	sc_intrsharetype[ICU_LEN];
	volatile u_int16_t *sc_status;

	struct amiga_bus_chipset sc_bc;
	struct amiga_isa_chipset sc_ic;
};

extern int ggdebug;

#endif
