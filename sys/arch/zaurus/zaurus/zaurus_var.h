/*	$NetBSD: lubbock_var.h,v 1.1 2003/06/18 10:51:15 bsh Exp $ */

/*
 * Copyright (c) 2002, 2003  Genetec Corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Genetec Corporation may not be used to endorse or 
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORPORATION ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORPORATION
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _EVBARM_LUBBOCK_VAR_H
#define _EVBARM_LUBBOCK_VAR_H

#include <sys/conf.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/zaurus_reg.h>


/* 
 * Lubbock on-board IO bus
 */
#define N_OBIO_IRQ  8

struct obio_softc {
	struct device sc_dev;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_obioreg_ioh;
	void	*sc_ih;		/* interrupt handler for obio on pxaip */
	void	*sc_si;		/* software interrupt handler */
	int	sc_intr;
	int	sc_obio_intr_mask;
	int	sc_obio_intr_pending;
	int	sc_ipl;		/* Max ipl among sub interrupts */
	struct obio_handler {
		int	(* func)(void *);
		void	*arg;
		int	level;
	} sc_handler[N_OBIO_IRQ];
};

typedef void *obio_chipset_tag_t;

struct obio_attach_args {
	obio_chipset_tag_t	oba_sc;		
	bus_space_tag_t		oba_iot; 	/* Bus tag */
	bus_addr_t		oba_addr;	/* i/o address  */
	int			oba_intr;
};

/* on-board hex LED */
void hex_led_blank( uint32_t value, int blank );
#define hex_led(value) ioreg_write( LUBBOCK_OBIO_VBASE+LUBBOCK_HEXLED, (value) )
#define hex_led_p(value) ioreg_write( LUBBOCK_OBIO_PBASE+LUBBOCK_HEXLED, (value) )

#define d_led(value) ioreg16_write( LUBBOCK_OBIO_VBASE+LUBBOCK_LEDCTL, (value) )

/*
 * IRQ handler
 */
void *obio_intr_establish(struct obio_softc *, int, int, int (*)(void *), void *);
void obio_intr_disestablish(void *);

#define obio_read(offset)  ioreg_read(LUBBOCK_OBIO_VBASE+(offset))
#define obio_write(offset,value)  \
	ioreg_write(LUBBOCK_OBIO_VBASE+(offset), (value))


#define obio16_read(offset)  ioreg16_read(LUBBOCK_OBIO_VBASE+(offset))
#define obio16_write(offset,value)  \
	ioreg16_write(LUBBOCK_OBIO_VBASE+(offset), (value))

#define obio8_read(offset)  ioreg8_read(LUBBOCK_OBIO_VBASE+(offset))
#define obio8_write(offset,value)  \
	ioreg8_write(LUBBOCK_OBIO_VBASE+(offset), (value))


#endif /* _EVBARM_LUBBOCK_VAR_H */
