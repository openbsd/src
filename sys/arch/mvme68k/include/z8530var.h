/*	$OpenBSD: z8530var.h,v 1.1 2013/12/01 21:56:42 miod Exp $	*/
/*	$NetBSD: z8530var.h,v 1.1 1997/10/18 00:01:30 gwr Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)zsvar.h	8.1 (Berkeley) 6/11/93
 */

#include <dev/ic/z8530sc.h>

struct zsc_softc {
	struct device 		 zsc_dev;	/* required first: base device */
	struct zs_chanstate 	*zsc_cs[2];	/* channel A and B soft state */
	struct zs_chanstate	 zsc_cs_store[2];

	struct intrhand		 zsc_ih;
	void			*zsc_softih;	/* softintr cookie */
};

/*
 * Functions to read and write individual registers in a channel.
 */

uint8_t zs_read_reg(struct zs_chanstate *, uint8_t);
uint8_t zs_read_csr(struct zs_chanstate *);
uint8_t zs_read_data(struct zs_chanstate *);

void  zs_write_reg(struct zs_chanstate *, uint8_t, uint8_t);
void  zs_write_csr(struct zs_chanstate *, uint8_t);
void  zs_write_data(struct zs_chanstate *, uint8_t);

#define	splzs()		_splraise(PSL_S | PSL_IPL4)
#define	IPL_ZS		4
