/*	$OpenBSD: ubsecvar.h,v 1.2 2000/06/03 13:14:39 jason Exp $	*/

/*
 * Copyright (c) 2000 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

struct ubsec_softc {
	struct	device		sc_dv;		/* generic device */
	void			*sc_ih;		/* interrupt handler cookie */
	bus_space_handle_t	sc_sh;		/* memory handle */
	bus_space_tag_t		sc_st;		/* memory tag */
	bus_dma_tag_t		sc_dmat;	/* dma tag */
	int			sc_5601;	/* device is 5601 */
	int32_t			sc_cid;		/* crypto tag */
	u_int32_t		sc_intrmask;	/* interrupt mask */
	SIMPLEQ_HEAD(,ubsec_q)	sc_queue;	/* packet queue */
	int			sc_nqueue;	/* count enqueued */
	SIMPLEQ_HEAD(,ubsec_q)	sc_qchip;	/* on chip */
};
