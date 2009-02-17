/*	$OpenBSD: if_le.c,v 1.11 2009/02/17 18:42:06 miod Exp $ */

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

#include <sys/param.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>

#include <machine/prom.h>

#include "stand.h"
#include "libsa.h"
#include "netif.h"
#include "config.h"

#include "if_lereg.h"

int     le_debug = 0;

void le_end(struct netif *);
void le_error(struct netif *, char *, volatile struct lereg1 *);
int le_get(struct iodesc *, void *, size_t, time_t);
void le_init(struct iodesc *, void *);
int le_match(struct netif *, void *);
int le_poll(struct iodesc *, void *, int);
int le_probe(struct netif *, void *);
int le_put(struct iodesc *, void *, size_t);
void le_reset(struct netif *, u_char *);

struct netif_stats le_stats;

struct netif_dif le0_dif = {
	0,			/* unit */
	1,			/* nsel */
	&le_stats,
	0,
	0,
};

struct netif_driver le_driver = {
	"le",			/* netif_bname */
	le_match,		/* match */
	le_probe,		/* probe */
	le_init,		/* init */
	le_get,			/* get */
	le_put,			/* put */
	le_end,			/* end */
	&le0_dif,		/* netif_ifs */
	1,			/* netif_nifs */
};

struct le_configuration {
	unsigned int phys_addr;
	int     used;
} le_config[] = {
	{ LANCE_REG_ADDR, 0 }
};

int     nle_config = sizeof(le_config) / (sizeof(le_config[0]));

struct {
	struct lereg1 *sc_r1;	/* LANCE registers */
	struct lereg2 *sc_r2;	/* RAM */
	int     next_rmd;
	int     next_tmd;
}       le_softc;

int
le_match(nif, machdep_hint)
	struct netif *nif;
	void   *machdep_hint;
{
	char   *name;
	int     i, val = 0;

	if (bugargs.cputyp != CPU_147)
		return (0);
	name = machdep_hint;
	if (name && !bcmp(le_driver.netif_bname, name, 2))
		val += 10;
	for (i = 0; i < nle_config; i++) {
		if (le_config[i].used)
			continue;
		if (le_debug)
			printf("le%d: le_match --> %d\n", i, val + 1);
		le_config[i].used++;
		return val + 1;
	}
	if (le_debug)
		printf("le%d: le_match --> 0\n", i);
	return 0;
}

int
le_probe(nif, machdep_hint)
	struct netif *nif;
	void   *machdep_hint;
{

	/* the set unit is the current unit */
	if (le_debug)
		printf("le%d: le_probe called\n", nif->nif_unit);

	if (bugargs.cputyp == CPU_147)
		return 0;
	return 1;
}

void
le_error(nif, str, ler1)
	struct netif *nif;
	char   *str;
	volatile struct lereg1 *ler1;
{
	/* ler1->ler1_rap = LE_CSRO done in caller */
	if (ler1->ler1_rdp & LE_C0_BABL)
		panic("le%d: been babbling, found by '%s'", nif->nif_unit, str);
	if (ler1->ler1_rdp & LE_C0_CERR) {
		le_stats.collision_error++;
		ler1->ler1_rdp = LE_C0_CERR;
	}
	if (ler1->ler1_rdp & LE_C0_MISS) {
		le_stats.missed++;
		ler1->ler1_rdp = LE_C0_MISS;
	}
	if (ler1->ler1_rdp & LE_C0_MERR) {
		printf("le%d: memory error in '%s'\n", nif->nif_unit, str);
		panic("memory error");
	}
}

void
le_reset(nif, myea)
	struct netif *nif;
	u_char *myea;
{
	struct lereg1 *ler1 = le_softc.sc_r1;
	struct lereg2 *ler2 = le_softc.sc_r2;
	unsigned int a;
	int     timo = 100000, stat, i;

	if (le_debug)
		printf("le%d: le_reset called\n", nif->nif_unit);
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
			printf("le%d: init timeout, stat = 0x%x\n",
			    nif->nif_unit, stat);
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
le_poll(desc, pkt, len)
	struct iodesc *desc;
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
		le_error(desc->io_netif, "le_poll", ler1);
	if (rmd->rmd1_bits & LE_R1_ERR) {
		printf("le%d_poll: rmd status 0x%x\n", desc->io_netif->nif_unit,
		    rmd->rmd1_bits);
		length = 0;
		goto cleanup;
	}
	if ((rmd->rmd1_bits & (LE_R1_STP | LE_R1_ENP)) != (LE_R1_STP | LE_R1_ENP))
		panic("le_poll: chained packet");

	length = rmd->rmd3;
	if (length >= LEMTU) {
		length = 0;
		panic("csr0 when bad things happen: %x", ler1->ler1_rdp);
		goto cleanup;
	}
	if (!length)
		goto cleanup;
	length -= 4;
	if (length > 0) {

		/*
	         * if buffer is smaller than the packet truncate it.
	         * (is this wise?)
	         */
		if (length > len)
			length = len;

		bcopy((void *)&ler2->ler2_rbuf[le_softc.next_rmd], pkt, length);
	}
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
le_put(desc, pkt, len)
	struct	iodesc *desc;
	void	*pkt;
	size_t	len;
{
	volatile struct lereg1 *ler1 = le_softc.sc_r1;
	volatile struct lereg2 *ler2 = le_softc.sc_r2;
	volatile struct letmd *tmd;
	int     timo = 100000, stat, i;
	unsigned int a;

	ler1->ler1_rap = LE_CSR0;
	if (ler1->ler1_rdp & LE_C0_ERR)
		le_error(desc->io_netif, "le_put(way before xmit)", ler1);
	tmd = &ler2->ler2_tmd[le_softc.next_tmd];
	while (tmd->tmd1_bits & LE_T1_OWN) {
		printf("le%d: output buffer busy\n", desc->io_netif->nif_unit);
	}
	bcopy(pkt, (void *)ler2->ler2_tbuf[le_softc.next_tmd], len);
	if (len < 64)
		tmd->tmd2 = -64;
	else
		tmd->tmd2 = -len;
	tmd->tmd3 = 0;
	if (ler1->ler1_rdp & LE_C0_ERR)
		le_error(desc->io_netif, "le_put(before xmit)", ler1);
	tmd->tmd1_bits = LE_T1_STP | LE_T1_ENP | LE_T1_OWN;
	a = (u_int) & ler2->ler2_tbuf[le_softc.next_tmd];
	tmd->tmd0 = a & LE_ADDR_LOW_MASK;
	tmd->tmd1_hadr = a >> 16;
	ler1->ler1_rdp = LE_C0_TDMD;
	if (ler1->ler1_rdp & LE_C0_ERR)
		le_error(desc->io_netif, "le_put(after xmit)", ler1);
	do {
		if (--timo == 0) {
			printf("le%d: transmit timeout, stat = 0x%x\n",
			    desc->io_netif->nif_unit, stat);
			if (ler1->ler1_rdp & LE_C0_ERR)
				le_error(desc->io_netif, "le_put(timeout)", ler1);
			break;
		}
		stat = ler1->ler1_rdp;
	} while ((stat & LE_C0_TINT) == 0);
	ler1->ler1_rdp = LE_C0_TINT;
	if (ler1->ler1_rdp & LE_C0_ERR) {
		if ((ler1->ler1_rdp & (LE_C0_BABL | LE_C0_CERR | LE_C0_MISS |
		    LE_C0_MERR)) !=
		    LE_C0_CERR)
			printf("le_put: xmit error, buf %d\n", le_softc.next_tmd);
		le_error(desc->io_netif, "le_put(xmit error)", ler1);
	}
	le_softc.next_tmd = 0;
/*	(le_softc.next_tmd == (LETBUF - 1)) ? 0 : le_softc.next_tmd + 1;*/
	if (tmd->tmd1_bits & LE_T1_DEF)
		le_stats.deferred++;
	if (tmd->tmd1_bits & LE_T1_ONE)
		le_stats.collisions++;
	if (tmd->tmd1_bits & LE_T1_MORE)
		le_stats.collisions += 2;
	if (tmd->tmd1_bits & LE_T1_ERR) {
		printf("le%d: transmit error, error = 0x%x\n", desc->io_netif->nif_unit,
		    tmd->tmd3);
		return -1;
	}
	if (le_debug) {
		printf("le%d: le_put() successful: sent %d\n",
		    desc->io_netif->nif_unit, len);
		printf("le%d: le_put(): tmd1_bits: %x tmd3: %x\n",
		    desc->io_netif->nif_unit,
		    (unsigned int) tmd->tmd1_bits,
		    (unsigned int) tmd->tmd3);
	}
	return len;
}

int
le_get(desc, pkt, len, timeout)
	struct	iodesc *desc;
	void	*pkt;
	size_t	len;
	time_t	timeout;
{
	time_t  t;
	int     cc;

	t = getsecs();
	cc = 0;
	while (((getsecs() - t) < timeout) && !cc) {
		cc = le_poll(desc, pkt, len);
	}
	return cc;
}
/*
 * init le device.   return 0 on failure, 1 if ok.
 */
void
le_init(desc, machdep_hint)
	struct iodesc *desc;
	void   *machdep_hint;
{
	struct netif *nif = desc->io_netif;

	if (le_debug)
		printf("le%d: le_init called\n", desc->io_netif->nif_unit);
	machdep_common_ether(desc->myea);
	bzero(&le_softc, sizeof(le_softc));
	le_softc.sc_r1 =
	    (struct lereg1 *) le_config[desc->io_netif->nif_unit].phys_addr;
	le_softc.sc_r2 = (struct lereg2 *)(STAGE2_RELOC - LEMEMSIZE);
	le_reset(desc->io_netif, desc->myea);
	printf("device: %s%d attached to %s\n", nif->nif_driver->netif_bname,
	    nif->nif_unit, ether_sprintf(desc->myea));
}

void
le_end(nif)
	struct netif *nif;
{
	struct lereg1 *ler1 = le_softc.sc_r1;

	if (le_debug)
		printf("le%d: le_end called\n", nif->nif_unit);
	ler1->ler1_rap = LE_CSR0;
	ler1->ler1_rdp = LE_C0_STOP;
}
