/*	$OpenBSD: espvar.h,v 1.6 2004/12/10 18:23:23 martin Exp $	*/
/*	$NetBSD: espvar.h,v 1.16 1996/10/13 02:59:50 christos Exp $	*/

/*
 * Copyright (c) 1997 Allen Briggs.
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
 *	This product includes software developed by Peter Galbavy.
 * 4. The name of the author may not be used to endorse or promote products
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

struct esp_softc {
	struct ncr53c9x_softc	sc_ncr53c9x;	/* glue to MI code */
	bus_space_tag_t		sc_tag;
	bus_space_handle_t	sc_bsh;
	struct via2hand		sc_ih;

	volatile u_char *sc_reg;		/* the registers */

	u_char		irq_mask;		/* mask for clearing IRQ */

	int		sc_active;		/* Pseudo-DMA state vars */
	int		sc_datain;
	size_t		sc_dmasize;
	char		**sc_dmaaddr;
	size_t		*sc_dmalen;
	int	sc_tc;				/* only used in non-quick */
	u_int16_t	*sc_pdmaddr;		/* only used in quick */
	int		sc_pdmalen;		/* only used in quick */
	size_t		sc_prevdmasize;		/* only used in quick */
	int		sc_pad;			/* only used in quick */
};
