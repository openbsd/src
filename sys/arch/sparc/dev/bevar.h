/*	$OpenBSD: bevar.h,v 1.3 1998/07/05 06:50:21 deraadt Exp $	*/

/*
 * Copyright (c) 1998 Theo de Raadt.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
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

struct besoftc {
	struct	device sc_dev;
	struct	sbusdev sc_sd;		/* sbus device */
	struct	arpcom sc_arpcom;
	struct	intrhand sc_ih;		/* interrupt vectoring */
	struct	dma_softc *sc_dma;	/* pointer to my dma */

	u_long	sc_laddr;		/* DMA address */
	struct	qecregs *sc_qr;		/* QEC registers */
	struct	be_bregs *sc_br;	/* registers */
	struct	be_cregs *sc_cr;	/* registers */
	struct	be_tregs *sc_tr;	/* registers */

	void	*sc_mem;
	int	sc_memsize;
	long	sc_addr;
	int	sc_conf3;
	u_int	sc_rev;

	int	sc_promisc;
};
