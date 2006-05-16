/*	$OpenBSD: if_le.c,v 1.4 2006/05/16 22:52:26 miod Exp $ */

/*
 * Copyright (c) 1995 Theo de Raadt
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 1993 Adam Glass
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
 *	This product includes software developed by Adam Glass.
 * 4. The name of the Author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Adam Glass ``AS IS'' AND
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
 */

#include <sys/cdefs.h>
#include "sboot.h"
#include "if_lereg.h"

struct {
	struct lereg1 *sc_r1;	/* LANCE registers */
	struct lereg2 *sc_r2;	/* RAM */
	int     next_rmd;
	int     next_tmd;
}       le_softc;

void
le_error(str, ler1)
	char   *str;
	struct lereg1 *ler1;
{
	/* ler1->ler1_rap = LE_CSRO done in caller */
	if (ler1->ler1_rdp & LE_C0_BABL) {
		printf("le0: been babbling, found by '%s'\n", str);
		callrom();
	}
	if (ler1->ler1_rdp & LE_C0_CERR) {
		ler1->ler1_rdp = LE_C0_CERR;
	}
	if (ler1->ler1_rdp & LE_C0_MISS) {
		ler1->ler1_rdp = LE_C0_MISS;
	}
	if (ler1->ler1_rdp & LE_C0_MERR) {
		printf("le0: memory error in '%s'\n", str);
		callrom();
	}
}

void
le_reset(myea)
	u_char *myea;
{
	struct lereg1 *ler1 = le_softc.sc_r1;
	struct lereg2 *ler2 = le_softc.sc_r2;
	unsigned int a;
	int     timo = 100000, stat, i;

	ler1->ler1_rap = LE_CSR0;
	ler1->ler1_rdp = LE_C0_STOP;	/* do nothing until we are finished */

	bzero(ler2, sizeof(*ler2));

	ler2->ler2_mode = LE_MODE_NORMAL;
	ler2->ler2_padr[0] = myea[1];
	ler2->ler2_padr[1] = myea[0];
	ler2->ler2_padr[2] = myea[3];
	ler2->ler2_padr[3] = myea[2];
	ler2->ler2_padr[4] = myea[5];
	ler2->ler2_padr[5] = myea[4];


	ler2->ler2_ladrf0 = 0;
	ler2->ler2_ladrf1 = 0;

	a = (u_int) ler2->ler2_rmd;
	ler2->ler2_rlen = LE_RLEN | (a >> 16);
	ler2->ler2_rdra = a & LE_ADDR_LOW_MASK;

	a = (u_int) ler2->ler2_tmd;
	ler2->ler2_tlen = LE_TLEN | (a >> 16);
	ler2->ler2_tdra = a & LE_ADDR_LOW_MASK;

	ler1->ler1_rap = LE_CSR1;
	a = (u_int) ler2;
	ler1->ler1_rdp = a & LE_ADDR_LOW_MASK;
	ler1->ler1_rap = LE_CSR2;
	ler1->ler1_rdp = a >> 16;

	for (i = 0; i < LERBUF; i++) {
		a = (u_int) & ler2->ler2_rbuf[i];
		ler2->ler2_rmd[i].rmd0 = a & LE_ADDR_LOW_MASK;
		ler2->ler2_rmd[i].rmd1_bits = LE_R1_OWN;
		ler2->ler2_rmd[i].rmd1_hadr = a >> 16;
		ler2->ler2_rmd[i].rmd2 = -LEMTU;
		ler2->ler2_rmd[i].rmd3 = 0;
	}
	for (i = 0; i < LETBUF; i++) {
		a = (u_int) & ler2->ler2_tbuf[i];
		ler2->ler2_tmd[i].tmd0 = a & LE_ADDR_LOW_MASK;
		ler2->ler2_tmd[i].tmd1_bits = 0;
		ler2->ler2_tmd[i].tmd1_hadr = a >> 16;
		ler2->ler2_tmd[i].tmd2 = 0;
		ler2->ler2_tmd[i].tmd3 = 0;
	}

	ler1->ler1_rap = LE_CSR3;
	ler1->ler1_rdp = LE_C3_BSWP;

	ler1->ler1_rap = LE_CSR0;
	ler1->ler1_rdp = LE_C0_INIT;
	do {
		if (--timo == 0) {
			printf("le0: init timeout, stat = 0x%x\n", stat);
			break;
		}
		stat = ler1->ler1_rdp;
	} while ((stat & LE_C0_IDON) == 0);

	ler1->ler1_rdp = LE_C0_IDON;
	le_softc.next_rmd = 0;
	le_softc.next_tmd = 0;
	ler1->ler1_rap = LE_CSR0;
	ler1->ler1_rdp = LE_C0_STRT;
}

int
le_poll(pkt, len)
	void   *pkt;
	int     len;
{
	struct lereg1 *ler1 = le_softc.sc_r1;
	struct lereg2 *ler2 = le_softc.sc_r2;
	unsigned int a;
	int     length;
	struct lermd *rmd;

	ler1->ler1_rap = LE_CSR0;
	if ((ler1->ler1_rdp & LE_C0_RINT) != 0)
		ler1->ler1_rdp = LE_C0_RINT;
	rmd = &ler2->ler2_rmd[le_softc.next_rmd];
	if (rmd->rmd1_bits & LE_R1_OWN) {
		return (0);
	}
	if (ler1->ler1_rdp & LE_C0_ERR)
		le_error("le_poll", ler1);
	if (rmd->rmd1_bits & LE_R1_ERR) {
		printf("le0_poll: rmd status 0x%x\n", rmd->rmd1_bits);
		length = 0;
		goto cleanup;
	}
	if ((rmd->rmd1_bits & (LE_R1_STP | LE_R1_ENP)) != (LE_R1_STP | LE_R1_ENP)) {
		printf("le_poll: chained packet\n");
		callrom();
	}
	length = rmd->rmd3;
	if (length >= LEMTU) {
		length = 0;
		printf("csr0 when bad things happen: %x\n", ler1->ler1_rdp);
		callrom();
		goto cleanup;
	}
	if (!length)
		goto cleanup;
	length -= 4;
	if (length > 0)
		bcopy((char *) &ler2->ler2_rbuf[le_softc.next_rmd], pkt, length);

cleanup:
	a = (u_int) & ler2->ler2_rbuf[le_softc.next_rmd];
	rmd->rmd0 = a & LE_ADDR_LOW_MASK;
	rmd->rmd1_hadr = a >> 16;
	rmd->rmd2 = -LEMTU;
	le_softc.next_rmd =
	    (le_softc.next_rmd == (LERBUF - 1)) ? 0 : (le_softc.next_rmd + 1);
	rmd->rmd1_bits = LE_R1_OWN;
	return length;
}

int
le_put(pkt, len)
	u_char *pkt;
	size_t  len;
{
	struct lereg1 *ler1 = le_softc.sc_r1;
	struct lereg2 *ler2 = le_softc.sc_r2;
	struct letmd *tmd;
	int     timo = 100000, stat, i;
	unsigned int a;

	ler1->ler1_rap = LE_CSR0;
	if (ler1->ler1_rdp & LE_C0_ERR)
		le_error("le_put(way before xmit)", ler1);
	tmd = &ler2->ler2_tmd[le_softc.next_tmd];
	while (tmd->tmd1_bits & LE_T1_OWN) {
		printf("le0: output buffer busy\n");
	}
	bcopy(pkt, (char *) ler2->ler2_tbuf[le_softc.next_tmd], len);
	if (len < 64)
		tmd->tmd2 = -64;
	else
		tmd->tmd2 = -len;
	tmd->tmd3 = 0;
	if (ler1->ler1_rdp & LE_C0_ERR)
		le_error("le_put(before xmit)", ler1);
	tmd->tmd1_bits = LE_T1_STP | LE_T1_ENP | LE_T1_OWN;
	a = (u_int) & ler2->ler2_tbuf[le_softc.next_tmd];
	tmd->tmd0 = a & LE_ADDR_LOW_MASK;
	tmd->tmd1_hadr = a >> 16;
	ler1->ler1_rdp = LE_C0_TDMD;
	if (ler1->ler1_rdp & LE_C0_ERR)
		le_error("le_put(after xmit)", ler1);
	do {
		if (--timo == 0) {
			printf("le0: transmit timeout, stat = 0x%x\n",
			    stat);
			if (ler1->ler1_rdp & LE_C0_ERR)
				le_error("le_put(timeout)", ler1);
			break;
		}
		stat = ler1->ler1_rdp;
	} while ((stat & LE_C0_TINT) == 0);
	ler1->ler1_rdp = LE_C0_TINT;
	if (ler1->ler1_rdp & LE_C0_ERR) {
		if ((ler1->ler1_rdp & (LE_C0_BABL | LE_C0_CERR | LE_C0_MISS | LE_C0_MERR)) !=
		    LE_C0_CERR)
			printf("le_put: xmit error, buf %d\n", le_softc.next_tmd);
		le_error("le_put(xmit error)", ler1);
	}
	le_softc.next_tmd = 0;
/*	(le_softc.next_tmd == (LETBUF - 1)) ? 0 : le_softc.next_tmd + 1;*/
	if (tmd->tmd1_bits & LE_T1_ERR) {
		printf("le0: transmit error, error = 0x%x\n",
		    tmd->tmd3);
		return -1;
	}
	return len;
}

int
le_get(pkt, len, timeout)
	u_char *pkt;
	size_t  len;
	u_long  timeout;
{
	int     cc;
	int     now, then;
	int     stopat = time() + timeout;
	then = 0;

	cc = 0;
	while ((now = time()) < stopat && !cc) {
		cc = le_poll(pkt, len);
		if (then != now) {
#ifdef LE_DEBUG
			printf("%d  \r", stopat - now);
#endif
			then = now;
		}
		if (cc && (pkt[0] != myea[0] || pkt[1] != myea[1] ||
			pkt[2] != myea[2] || pkt[3] != myea[3] ||
			pkt[4] != myea[4] || pkt[5] != myea[5])) {
			cc = 0;	/* ignore broadcast / multicast */
#ifdef LE_DEBUG
			printf("reject (%d sec left)\n", stopat - now);
#endif
		}
	}
#ifdef LE_DEBUG
	printf("\n");
#endif
	return cc;
}

void
le_init()
{
	caddr_t addr;
	int    *ea = (int *) LANCE_ADDR;
	u_long *eram = (u_long *) ERAM_ADDR;
	u_long  e = *ea;
	if ((e & 0x2fffff00) == 0x2fffff00) {
		printf("ERROR: ethernet address not set!  Use LSAD.\n");
		callrom();
	}
	myea[0] = 0x08;
	myea[1] = 0x00;
	myea[2] = 0x3e;
	e = e >> 8;
	myea[5] = e & 0xff;
	e = e >> 8;
	myea[4] = e & 0xff;
	e = e >> 8;
	myea[3] = e;
	printf("le0: ethernet address: %x:%x:%x:%x:%x:%x\n",
	    myea[0], myea[1], myea[2], myea[3], myea[4], myea[5]);
	bzero(&le_softc, sizeof(le_softc));
	le_softc.sc_r1 = (struct lereg1 *) LANCE_REG_ADDR;
	le_softc.sc_r2 = (struct lereg2 *) (*eram - (1024 * 1024));
	le_reset(myea);
}

void
le_end()
{
	struct lereg1 *ler1 = le_softc.sc_r1;

	ler1->ler1_rap = LE_CSR0;
	ler1->ler1_rdp = LE_C0_STOP;
}
