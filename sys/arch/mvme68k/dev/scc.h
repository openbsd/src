/*	$OpenBSD: scc.h,v 1.4 2000/01/29 04:11:25 smurph Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed under OpenBSD by
 *	Theo de Raadt for Willowglen Singapore.
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
/*
 * SCC I/O register definitions
 */

#define PCLK_FREQ_147	5000000
#define PCLK_FREQ_162	10000000

/*
 * physical layout in memory of the SCC chips on the MVME147
 */
struct scc_147 {
	u_char cr;
	u_char dr;
};

/*
 * physical layout in memory of the SCC chips on the MVME162
 * (and possibly the MVME172 as well?)
 */
struct scc_162 {
	u_char xx1;
	u_char cr;
	u_char xx2;
	u_char dr;
};

struct sccregs {
	volatile u_char *s_cr;
	volatile u_char *s_dr;
	u_char s_val[16];
};

int mc_rev1_bug = 0;

#define ZREAD0(scc)	((*((scc)->s_cr)))
#define ZREAD(scc, n)	((*((scc)->s_cr)) = n, (*((scc)->s_cr)))
#if 1
#define ZREADD(scc)	mc_rev1_bug ? (ZWRITE0((scc), 8), ZREAD0((scc))) : ((*((scc)->s_dr)))
#else
#define ZREADD(scc)	((*((scc)->s_dr)))
#endif

#define ZWRITE0(scc, v)	((*((scc)->s_cr)) = (u_char)(v))
#define ZWRITE(scc, n, v) (ZWRITE0(scc, (u_char)n), \
	    ZWRITE0(scc, (scc)->s_val[n] = (u_char)(v)))
#if 1
#define ZWRITED(scc, v)	mc_rev1_bug ? ((ZWRITE0((scc), 8), ZWRITE0((scc), (u_char)(v)))) : \
        (((*((scc)->s_dr)) = (u_char)(v)))
#else
#define ZWRITED(scc, v)	((*((scc)->s_dr)) = (u_char)(v))
#endif

#define ZBIS(scc, n, v)	(ZWRITE(scc, n, (scc)->s_val[n] | (v)))
#define ZBIC(scc, n, v)	(ZWRITE(scc, n, (scc)->s_val[n] & ~(v)))

#define SCC_RXFULL	1	/* bits in rr0 */
#define SCC_TXRDY	4
#define SCC_DCD		8
#define SCC_CTS		0x20

#define SCC_RCVEN	1	/* bits in wr3 */

#define SCC_RTS		2	/* bits in wr5 */
#define SCC_DTR		0x80
