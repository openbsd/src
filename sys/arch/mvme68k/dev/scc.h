/*	$NetBSD: scc.h,v 1.1.1.1 1995/07/25 23:12:07 chuck Exp $	*/

/*
 * Copyright (c) 1993 Paul Mackerras.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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
/*
 * SCC I/O register definitions
 */

#define PCLK_FREQ	8333333		/* XXX */

struct scc {
	unsigned char cr;
	unsigned char dr;
};

struct sccregs {
	volatile struct scc *s_adr;
	unsigned char s_val[16];
};

#define ZREAD0(scc)	((scc)->s_adr->cr)
#define ZREAD(scc, n)	((scc)->s_adr->cr = n, (scc)->s_adr->cr)
#define ZREADD(scc)	((scc)->s_adr->dr)

#define ZWRITE0(scc, v)	((scc)->s_adr->cr = v)
#define ZWRITE(scc, n, v) (ZWRITE0(scc, n), ZWRITE0(scc, (scc)->s_val[n] = v))
#define ZWRITED(scc, v)	((scc)->s_adr->dr = v)

#define ZBIS(scc, n, v)	(ZWRITE(scc, n, (scc)->s_val[n] | (v)))
#define ZBIC(scc, n, v)	(ZWRITE(scc, n, (scc)->s_val[n] & ~(v)))

#define SCC_RXFULL	1	/* bits in rr0 */
#define SCC_TXRDY	4
#define SCC_DCD		8
#define SCC_CTS		0x20

#define SCC_RCVEN	1	/* bits in wr3 */

#define SCC_RTS		2	/* bits in wr5 */
#define SCC_DTR		0x80
