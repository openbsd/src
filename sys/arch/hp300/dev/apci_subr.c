/*	$OpenBSD: apci_subr.c,v 1.2 2008/06/26 05:42:10 ray Exp $	*/

/*-
 * Copyright (c) 1996, 1997, 1999 The NetBSD Foundation, Inc.
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
 * Copyright (c) 1997 Michael Smith.  All rights reserved.
 * Copyright (c) 1982, 1986, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *      @(#)dca.c       8.2 (Berkeley) 1/12/94
 */

/*
 * Routines shared by the apci and dnkbd drivers.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/tty.h>
#include <sys/kernel.h>

#include <hp300/dev/apcireg.h>
#include <hp300/dev/apcivar.h>
#include <hp300/dev/dcareg.h>

const struct speedtab apcispeedtab[] = {
	{ 0,		0		},
	{ 50,		APCIBRD(50)	},
	{ 75,		APCIBRD(75)	},
	{ 110,		APCIBRD(110)	},
	{ 134,		APCIBRD(134)	},
	{ 150,		APCIBRD(150)	},
	{ 200,		APCIBRD(200)	},
	{ 300,		APCIBRD(300)	},
	{ 600,		APCIBRD(600)	},
	{ 1200,		APCIBRD(1200)	},
	{ 1800,		APCIBRD(1800)	},
	{ 2400,		APCIBRD(2400)	},
	{ 4800,		APCIBRD(4800)	},
	{ 9600,		APCIBRD(9600)	},
	{ 19200,	APCIBRD(19200)	},
	{ 38400,	APCIBRD(38400)	},
	{ -1,		-1		},
};

int
apciinit(struct apciregs *apci, u_int16_t rate, u_int16_t cfcr)
{
	int s, stat;

	s = splhigh();

	rate = ttspeedtab(rate, apcispeedtab);

	apci->ap_cfcr = CFCR_DLAB;
	apci->ap_data = rate & 0xff;
	apci->ap_ier = (rate >> 8) & 0xff;
	apci->ap_cfcr = cfcr;
	apci->ap_ier = IER_ERXRDY | IER_ETXRDY;
	apci->ap_fifo =
	    FIFO_ENABLE | FIFO_RCV_RST | FIFO_XMT_RST | FIFO_TRIGGER_1;
	apci->ap_mcr = MCR_DTR | MCR_RTS;
	delay(100);
	stat = apci->ap_iir;
	splx(s);

	return (stat);
}
