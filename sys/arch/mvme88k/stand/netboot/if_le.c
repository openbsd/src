/*	$OpenBSD: if_le.c,v 1.9 2013/05/12 21:00:56 miod Exp $ */

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
/*-
 * Copyright (c) 1982, 1992, 1993
 *      The Regents of the University of California.  All rights reserved.
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
 *      @(#)if_le.c     8.2 (Berkeley) 10/30/93
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

void le_wrcsr(u_int16_t, u_int16_t);
u_int16_t le_rdcsr(u_int16_t);
void le_end(struct netif *);
void le_error(struct netif *, const char *, u_int16_t);
int le_get(struct iodesc *, void *, size_t, time_t);
void le_init(struct iodesc *, void *);
int le_match(struct netif *, void *);
int le_poll(struct iodesc *, void *, int);
int le_probe(struct netif *, void *);
int le_put(struct iodesc *, void *, size_t);
void le_reset(struct netif *, u_char *);

struct netif_driver le_driver = {
	"le",			/* netif_bname */
	le_match,		/* match */
	le_probe,		/* probe */
	le_init,		/* init */
	le_get,			/* get */
	le_put,			/* put */
	le_end,			/* end */
	NULL,			/* netif_ifs - will be filled later */
	0,			/* netif_nifs - will be filled later */
};

struct {
	struct lereg1 *sc_r1;	/* LANCE registers */
	struct vlereg1 *sc_vr1;	/* MVME376 registers */
	struct lereg2 *sc_r2;	/* RAM */
	int     next_rmd;
	int     next_tmd;
} le_softc;

int
le_match(struct netif *nif, void *machdep_hint)
{
	const char *name = machdep_hint;

	if (name == NULL) {
		if (le_config[nif->nif_unit].clun == bugargs.ctrl_lun ||
		    (int)bugargs.ctrl_lun < 0)
			return 1;
	} else {
		if (bcmp(le_driver.netif_bname, name, 2) == 0) {
			if (nif->nif_unit == name[2] - '0')
				return 1;
		}
	}

	return 0;
}

int
le_probe(struct netif *nif, void *machdep_hint)
{
	return 0;
}

void
le_wrcsr(u_int16_t port, u_int16_t val)
{
	if (le_softc.sc_r1 != NULL) {
		le_softc.sc_r1->ler1_rap = port;
		le_softc.sc_r1->ler1_rdp = val;
	} else {
		le_softc.sc_vr1->ler1_rap = port;
		le_softc.sc_vr1->ler1_rdp = val;
	}
}

u_int16_t
le_rdcsr(u_int16_t port)
{
	u_int16_t val;

	if (le_softc.sc_r1 != NULL) {
		le_softc.sc_r1->ler1_rap = port;
		val = le_softc.sc_r1->ler1_rdp;
	} else {
		le_softc.sc_vr1->ler1_rap = port;
		val = le_softc.sc_vr1->ler1_rdp;
	}
	return (val);
}

void
le_error(struct netif *nif, const char *str, u_int16_t stat)
{
	struct netif_driver *drv = nif->nif_driver;

	if (stat & LE_C0_BABL)
		panic("le%d: been babbling, found by '%s'", nif->nif_unit, str);
	if (stat & LE_C0_CERR) {
		drv->netif_ifs[nif->nif_unit].dif_stats->collision_error++;
		le_wrcsr(LE_CSR0, LE_C0_CERR);
	}
	if (stat & LE_C0_MISS) {
		drv->netif_ifs[nif->nif_unit].dif_stats->missed++;
		le_wrcsr(LE_CSR0, LE_C0_MISS);
	}
	if (stat & LE_C0_MERR) {
		panic("le%d: memory error in '%s'", nif->nif_unit, str);
	}
}

void
le_reset(struct netif *nif, u_char *myea)
{
	struct lereg2 *ler2 = le_softc.sc_r2;
	unsigned int a;
	int     timo = 100000, i;
	u_int16_t stat;

	if (le_debug)
		printf("le%d: le_reset called\n", nif->nif_unit);
	
	if (le_softc.sc_vr1 != NULL) {
		le_softc.sc_vr1->ler1_csr = 0;
		CDELAY;
		le_softc.sc_vr1->ler1_csr = HW_RS | 0x0f;
	}
	le_wrcsr(LE_CSR0, LE_C0_STOP);

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

	a = (u_int)ler2->ler2_rmd;
	ler2->ler2_rlen = LE_RLEN | LE_ADDR_HIGH(a);
	ler2->ler2_rdra = LE_ADDR_LOW(a);

	a = (u_int)ler2->ler2_tmd;
	ler2->ler2_tlen = LE_TLEN | LE_ADDR_HIGH(a);
	ler2->ler2_tdra = LE_ADDR_LOW(a);

	a = (u_int)ler2;
	le_wrcsr(LE_CSR1, LE_ADDR_LOW(a));
	le_wrcsr(LE_CSR2, LE_ADDR_HIGH(a));

	for (i = 0; i < LERBUF; i++) {
		a = (u_int)&ler2->ler2_rbuf[i];
		ler2->ler2_rmd[i].rmd0 = LE_ADDR_LOW(a);
		ler2->ler2_rmd[i].rmd1_bits = LE_R1_OWN;
		ler2->ler2_rmd[i].rmd1_hadr = LE_ADDR_HIGH(a);
		ler2->ler2_rmd[i].rmd2 = -LEMTU;
		ler2->ler2_rmd[i].rmd3 = 0;
	}
	for (i = 0; i < LETBUF; i++) {
		a = (u_int)&ler2->ler2_tbuf[i];
		ler2->ler2_tmd[i].tmd0 = LE_ADDR_LOW(a);
		ler2->ler2_tmd[i].tmd1_bits = 0;
		ler2->ler2_tmd[i].tmd1_hadr = LE_ADDR_HIGH(a);
		ler2->ler2_tmd[i].tmd2 = 0;
		ler2->ler2_tmd[i].tmd3 = 0;
	}

	le_wrcsr(LE_CSR3, LE_C3_BSWP);

	le_wrcsr(LE_CSR0, LE_C0_INIT);
	do {
		stat = le_rdcsr(LE_CSR0);
		if (--timo == 0) {
			printf("le%d: init timeout, stat = 0x%x\n",
			    nif->nif_unit, stat);
			break;
		}
	} while ((stat & LE_C0_IDON) == 0);

	le_wrcsr(LE_CSR0, LE_C0_IDON);
	le_softc.next_rmd = 0;
	le_softc.next_tmd = 0;
	le_wrcsr(LE_CSR0, LE_C0_STRT);
}

int
le_poll(struct iodesc *desc, void *pkt, int len)
{
	struct netif *nif = desc->io_netif;
	struct lereg2 *ler2 = le_softc.sc_r2;
	unsigned int a;
	int     length;
	struct lermd *rmd;
	u_int16_t stat;

	stat = le_rdcsr(LE_CSR0);
	if ((stat & LE_C0_RINT) != 0)
		le_wrcsr(LE_CSR0, LE_C0_RINT);
	rmd = &ler2->ler2_rmd[le_softc.next_rmd];
	if (rmd->rmd1_bits & LE_R1_OWN) {
		return (0);
	}
	if (stat & LE_C0_ERR)
		le_error(nif, "le_poll", stat);
	if (rmd->rmd1_bits & LE_R1_ERR) {
		printf("le%d_poll: rmd status 0x%x\n",
		    nif->nif_unit, rmd->rmd1_bits);
		length = 0;
		goto cleanup;
	}
	if ((rmd->rmd1_bits & (LE_R1_STP | LE_R1_ENP)) !=
	    (LE_R1_STP | LE_R1_ENP))
		panic("le_poll: chained packet");

	length = rmd->rmd3;
	if (length >= LEMTU) {
		length = 0;
		panic("csr0 when bad things happen: %x", stat);
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
	a = (u_int)&ler2->ler2_rbuf[le_softc.next_rmd];
	rmd->rmd0 = LE_ADDR_LOW(a);
	rmd->rmd1_hadr = LE_ADDR_HIGH(a);
	rmd->rmd2 = -LEMTU;
	le_softc.next_rmd =
	    (le_softc.next_rmd == (LERBUF - 1)) ? 0 : (le_softc.next_rmd + 1);
	rmd->rmd1_bits = LE_R1_OWN;
	return length;
}

int
le_put(struct iodesc *desc, void *pkt, size_t len)
{
	struct netif *nif = desc->io_netif;
	struct netif_driver *drv = nif->nif_driver;
	volatile struct lereg2 *ler2 = le_softc.sc_r2;
	volatile struct letmd *tmd;
	int     timo = 100000;
	unsigned int a;
	u_int16_t stat;

	stat = le_rdcsr(LE_CSR0);
	if (stat & LE_C0_ERR)
		le_error(nif, "le_put(way before xmit)", stat);
	tmd = &ler2->ler2_tmd[le_softc.next_tmd];
	while (tmd->tmd1_bits & LE_T1_OWN) {
		printf("le%d: output buffer busy\n", nif->nif_unit);
	}
	bcopy(pkt, (void *)ler2->ler2_tbuf[le_softc.next_tmd], len);
	if (len < 64)
		tmd->tmd2 = -64;
	else
		tmd->tmd2 = -len;
	tmd->tmd3 = 0;
	stat = le_rdcsr(LE_CSR0);
	if (stat & LE_C0_ERR)
		le_error(nif, "le_put(before xmit)", stat);
	tmd->tmd1_bits = LE_T1_STP | LE_T1_ENP | LE_T1_OWN;
	a = (u_int)&ler2->ler2_tbuf[le_softc.next_tmd];
	tmd->tmd0 = LE_ADDR_LOW(a);
	tmd->tmd1_hadr = LE_ADDR_HIGH(a);
	le_wrcsr(LE_CSR0, LE_C0_TDMD);
	stat = le_rdcsr(LE_CSR0);
	if (stat & LE_C0_ERR)
		le_error(nif, "le_put(after xmit)", stat);
	do {
		stat = le_rdcsr(LE_CSR0);
		if (--timo == 0) {
			printf("le%d: transmit timeout, stat = 0x%x\n",
			    nif->nif_unit, stat);
			if (stat & LE_C0_ERR)
				le_error(nif, "le_put(timeout)", stat);
			break;
		}
	} while ((stat & LE_C0_TINT) == 0);
	le_wrcsr(LE_CSR0, LE_C0_TINT);
	stat = le_rdcsr(LE_CSR0);
	if (stat & LE_C0_ERR) {
		if ((stat &
		     (LE_C0_BABL | LE_C0_CERR | LE_C0_MISS | LE_C0_MERR)) !=
		    LE_C0_CERR)
			printf("le_put: xmit error, buf %d\n",
			    le_softc.next_tmd);
		le_error(nif, "le_put(xmit error)", stat);
	}
	le_softc.next_tmd = 0;
/*	(le_softc.next_tmd == (LETBUF - 1)) ? 0 : le_softc.next_tmd + 1;*/
	if (tmd->tmd1_bits & LE_T1_DEF)
		drv->netif_ifs[nif->nif_unit].dif_stats->deferred++;
	if (tmd->tmd1_bits & LE_T1_ONE)
		drv->netif_ifs[nif->nif_unit].dif_stats->collisions++;
	if (tmd->tmd1_bits & LE_T1_MORE)
		drv->netif_ifs[nif->nif_unit].dif_stats->collisions += 2;
	if (tmd->tmd1_bits & LE_T1_ERR) {
		printf("le%d: transmit error, error = 0x%x\n",
		    nif->nif_unit, tmd->tmd3);
		return -1;
	}
	if (le_debug) {
		printf("le%d: le_put() successful: sent %d\n",
		    nif->nif_unit, len);
		printf("le%d: le_put(): tmd1_bits: %x tmd3: %x\n",
		    nif->nif_unit,
		    (unsigned int) tmd->tmd1_bits,
		    (unsigned int) tmd->tmd3);
	}
	return len;
}

int
le_get(struct iodesc *desc, void *pkt, size_t len, time_t timeout)
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
le_init(struct iodesc *desc, void *machdep_hint)
{
	struct netif *nif = desc->io_netif;

	if (le_debug)
		printf("le%d: le_init called\n", nif->nif_unit);
	bcopy(le_config[nif->nif_unit].eaddr, desc->myea, 6);
	bzero(&le_softc, sizeof(le_softc));
	/* no on-board le on mvme88k */
	le_softc.sc_r1 = NULL;
	le_softc.sc_vr1 = (struct vlereg1 *)le_config[nif->nif_unit].phys_addr;
	if (le_config[nif->nif_unit].buf_size != 0) {
		le_softc.sc_r2 =
		    (struct lereg2 *)le_config[nif->nif_unit].buf_addr;
	} else {
		le_softc.sc_r2 = (struct lereg2 *)(HEAP_START - LEMEMSIZE);
	}
	le_reset(nif, desc->myea);
#if 0
	printf("device: %s%d attached to %s\n", nif->nif_driver->netif_bname,
	    nif->nif_unit, ether_sprintf(desc->myea));
#endif
	bugargs.ctrl_lun = le_config[nif->nif_unit].clun;
	bugargs.dev_lun = 0;
	bugargs.ctrl_addr = le_config[nif->nif_unit].phys_addr;
}

void
le_end(struct netif *nif)
{
	if (le_debug)
		printf("le%d: le_end called\n", nif->nif_unit);
	le_wrcsr(LE_CSR0, LE_C0_STOP);
}

void nvram_cmd(struct vlereg1 *, u_char, u_short);
u_int16_t nvram_read(struct vlereg1 *, u_char);

/* send command to the nvram controller */
void
nvram_cmd(struct vlereg1 *reg1, u_char cmd, u_short addr)
{
	int i;

	for (i = 0; i < 8; i++) {
		reg1->ler1_ear = ((cmd | (addr << 1)) >> i); 
		CDELAY; 
	} 
}

/* read nvram one bit at a time */
u_int16_t
nvram_read(struct vlereg1 *reg1, u_char nvram_addr)
{
	u_short val = 0, mask = 0x04000;
	u_int16_t wbit;

	ENABLE_NVRAM(reg1->ler1_csr);
	nvram_cmd(reg1, NVRAM_RCL, 0);
	DISABLE_NVRAM(reg1->ler1_csr);
	CDELAY;
	ENABLE_NVRAM(reg1->ler1_csr);
	nvram_cmd(reg1, NVRAM_READ, nvram_addr);
	for (wbit = 0; wbit < 15; wbit++) {
		if (reg1->ler1_ear & 0x01)
			val |= mask;
		else
			val &= ~mask;
		mask = mask >> 1;
		CDELAY;
	}
	if (reg1->ler1_ear & 0x01)
		val |= 0x8000;
	else
		val &= 0x7fff;
	CDELAY;
	DISABLE_NVRAM(reg1->ler1_csr);
	return (val);
}

void
le_read_etheraddr(u_int phys_addr, u_char *enaddr)
{
	struct vlereg1 *reg1 = (struct vlereg1 *)phys_addr;
	u_int16_t ival[3];
	int i;

	for (i = 0; i < 3; i++) {
		ival[i] = nvram_read(reg1, i);
	}
	memcpy(enaddr, &ival[0], 6);
}
