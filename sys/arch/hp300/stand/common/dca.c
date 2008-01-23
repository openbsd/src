/*	$OpenBSD: dca.c,v 1.7 2008/01/23 16:37:56 jsing Exp $	*/
/*	$NetBSD: dca.c,v 1.10 1996/10/06 01:42:48 mycroft Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1990, 1993
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
 *	@(#)dca.c	8.1 (Berkeley) 6/10/93
 */

#ifdef DCACONSOLE
#include <sys/param.h>

#include <hp300/dev/dcareg.h>

#include "samachdep.h"
#include "consdefs.h"

struct dcadevice *dcacnaddr = 0;

#define DCACONSCODE	9	/* XXX */

void
dcaprobe(struct consdev *cp)
{
	struct dcadevice *dca;

	dcacnaddr = (struct dcadevice *) sctoaddr(DCACONSCODE);
	if (badaddr((char *)dcacnaddr))
		return;

	dca = dcacnaddr;
	switch (dca->dca_id) {
	case DCAID0:
	case DCAID1:
		cp->cn_pri = CN_LOWPRI;
		break;
	case DCAID0 | DCACON:
	case DCAID1 | DCACON:
		cp->cn_pri = CN_HIGHPRI;
		break;
	default:
		break;
	}
}

void
dcainit(struct consdev *cp)
{
	struct dcadevice *dca = dcacnaddr;

	dca->dca_reset = 0xFF;
	DELAY(100);
	dca->dca_ic = 0;
	dca->dca_cfcr = CFCR_DLAB;
	dca->dca_data = DCABRD(9600) & 0xFF;
	dca->dca_ier = DCABRD(9600) >> 8;
	dca->dca_cfcr = CFCR_8BITS;
	dca->dca_fifo =
	    FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST | FIFO_TRIGGER_1;
	dca->dca_mcr = MCR_DTR | MCR_RTS;
}

/* ARGSUSED */
int
dcagetchar(dev_t dev)
{
#ifndef SMALL
	struct dcadevice *dca = dcacnaddr;
	short stat;
	int c;

	if (((stat = dca->dca_lsr) & LSR_RXRDY) == 0)
		return(0);
	c = dca->dca_data;
	return(c);
#else
	return(0);
#endif
}

/* ARGSUSED */
void
dcaputchar(dev_t dev, int c)
{
	struct dcadevice *dca = dcacnaddr;
	int timo;
	short stat;

	/* wait a reasonable time for the transmitter to come ready */
	timo = 50000;
	while (((stat = dca->dca_lsr) & LSR_TXRDY) == 0 && --timo)
		;
	dca->dca_data = c;
	/* wait for this transmission to complete */
	timo = 1000000;
	while (((stat = dca->dca_lsr) & LSR_TXRDY) == 0 && --timo)
		;
}
#endif
