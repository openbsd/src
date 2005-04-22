/*	$OpenBSD: apci.c,v 1.5 2005/04/22 00:42:16 miod Exp $	*/
/*	$NetBSD: apci.c,v 1.2 1997/10/04 17:20:15 thorpej Exp $	*/

/*-
 * Copyright (c) 1997, 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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

#ifdef APCICONSOLE
#include <sys/param.h>
#include <dev/cons.h>

#include <hp300/dev/frodoreg.h>		/* for APCI offsets */
#include <hp300/dev/apcireg.h>		/* for register map */
#include <hp300/dev/dcareg.h>		/* for register bits */

#include "consdefs.h"
#include "samachdep.h"

struct apciregs *apcicnaddr = 0;

void
apciprobe(cp)
	struct consdev *cp;
{

	cp->cn_pri = CN_DEAD;

	/*
	 * Only a 425e can have an APCI console.  On all other 4xx models,
	 * the "first" serial port is mapped to the DCA at select code 9.
	 */
	if (machineid != HP_425 || mmuid != MMUID_425_E)
		return;

#ifdef FORCEAPCICONSOLE
	cp->cn_pri = CN_REMOTE;
#else
	cp->cn_pri = CN_NORMAL;
#endif
	curcons_scode = -2;
}

void
apciinit(cp)
	struct consdev *cp;
{
	struct apciregs *apci = (struct apciregs *)apcicnaddr;

	apci->ap_cfcr = CFCR_DLAB;
	apci->ap_data = APCIBRD(9600) & 0xff;
	apci->ap_ier  = (APCIBRD(9600) >> 8) & 0xff;
	apci->ap_cfcr = CFCR_8BITS;
	apci->ap_fifo =
	    FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST | FIFO_TRIGGER_1;
	apci->ap_mcr = MCR_DTR | MCR_RTS;
}

/* ARGSUSED */
#ifndef SMALL
int
apcigetchar(dev)
	dev_t dev;
{
	register struct apciregs *apci = apcicnaddr;
	short stat;
	int c;

	if (((stat = apci->ap_lsr) & LSR_RXRDY) == 0)
		return (0);
	c = apci->ap_data;
	return (c);
}
#else
int
apcigetchar(dev)
	dev_t dev;
{
	return (0);
}
#endif

/* ARGSUSED */
void
apciputchar(dev, c)
	dev_t dev;
	int c;
{
	struct apciregs *apci = apcicnaddr;
	int timo;
	short stat;

	/* wait a reasonable time for the transmitter to come ready */
	timo = 50000;
	while (((stat = apci->ap_lsr) & LSR_TXRDY) == 0 && --timo)
		;
	apci->ap_data = c;
	/* wait for this transmission to complete */
	timo = 1000000;
	while (((stat = apci->ap_lsr) & LSR_TXRDY) == 0 && --timo)
		;
}
#endif
