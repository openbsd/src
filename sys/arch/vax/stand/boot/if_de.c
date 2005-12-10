/*	$OpenBSD: if_de.c,v 1.2 2005/12/10 11:45:43 miod Exp $ */
/*	$NetBSD: if_de.c,v 1.2 2002/05/24 21:41:40 ragge Exp $	*/

/*
 * Copyright (c) 2000 Ludd, University of Lule}, Sweden. All rights reserved.
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
 *      This product includes software developed at Ludd, University of
 *      Lule}, Sweden and its contributors.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 *
 *	Standalone routine for the DEUNA Ethernet controller.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>

#include <lib/libsa/netif.h>
#include <lib/libsa/stand.h>

#include <arch/vax/qbus/if_dereg.h>

#include "arch/vax/include/sid.h"
#include "arch/vax/include/rpb.h"
#include "arch/vax/include/pte.h"

#include "vaxstand.h"

static int de_get(struct iodesc *, void *, size_t, time_t);
static int de_put(struct iodesc *, void *, size_t);
static void dewait(char *);

struct netif_driver de_driver = {
	0, 0, 0, 0, de_get, de_put,
};

#define NRCV 8		/* allocate 8 receive descriptors */
#define NXMT 4		/* and 4 transmit - must be >1 */

struct de_cdata {
	/* the following structures are always mapped in */
        struct  de_pcbb dc_pcbb;        /* port control block */
        struct  de_ring dc_xrent[NXMT]; /* transmit ring entries */
        struct  de_ring dc_rrent[NRCV]; /* receive ring entries */
        struct  de_udbbuf dc_udbbuf;    /* UNIBUS data buffer */
	char	dc_rbuf[NRCV][ETHER_MAX_LEN];
	char	dc_xbuf[NXMT][ETHER_MAX_LEN];
        /* end mapped area */
};

static volatile struct de_cdata *dc, *pdc;
static volatile char *addr;
static int crx, ctx;
#define DE_WCSR(csr, val) *(volatile u_short *)(addr + (csr)) = (val)
#define DE_WLOW(val) *(volatile u_char *)(addr + DE_PCSR0) = (val)
#define DE_WHIGH(val) *(volatile u_char *)(addr + DE_PCSR0 + 1) = (val)
#define DE_RCSR(csr) *(volatile u_short *)(addr + (csr))
#define LOWORD(x)       ((u_int)(x) & 0xffff)
#define HIWORD(x)       (((u_int)(x) >> 16) & 0x3)
#define	dereg(x)	((x) & 017777)

int
deopen(struct open_file *f, int adapt, int ctlr, int unit, int part)
{
	int i, cdata, *map, npgs;
	char eaddr[6];

	/* point to the device in memory */
	if (askname == 0) /* Override if autoboot */
		addr = (char *)bootrpb.csrphy;
	else {
		addr = (char *)csrbase + dereg(0174510);
		bootrpb.csrphy = (int)addr;
	}
#ifdef DEV_DEBUG
	printf("deopen: csrbase %x addr %p nexaddr %x\n", 
	    csrbase, addr, nexaddr);
#endif
	/* reset the device and wait for completion */
	DE_WCSR(DE_PCSR0, 0);
	{volatile int j = 100; while (--j);}
	DE_WCSR(DE_PCSR0, PCSR0_RSET);
	dewait("reset");

	/* Map in the control structures and buffers */
	dc = alloc(sizeof(struct de_cdata));
	(int)pdc = (int)dc & VAX_PGOFSET;
	map = (int *)nexaddr + 512;
	npgs = (sizeof(struct de_cdata) >> VAX_PGSHIFT) + 1;
	cdata = (int)dc >> VAX_PGSHIFT;
	for (i = 0; i < npgs; i++) {
		map[i] = PG_V | (cdata + i);
	}

	bzero((char *)dc, sizeof(struct de_cdata));
	
	/* Tell the DEUNA about our PCB */
	DE_WCSR(DE_PCSR2, LOWORD(pdc));
	DE_WCSR(DE_PCSR3, HIWORD(pdc));
	DE_WLOW(CMD_GETPCBB);
	dewait("pcbb");

	/* Get our address */
	dc->dc_pcbb.pcbb0 = FC_RDPHYAD;
	DE_WLOW(CMD_GETCMD);
	dewait("read physaddr");
	bcopy((char *)&dc->dc_pcbb.pcbb2, eaddr, 6);

	/* Create and link the descriptors */
	for (i=0; i < NRCV; i++) {
		volatile struct de_ring *rp = &dc->dc_rrent[i];

		rp->r_lenerr = 0;
		rp->r_segbl = LOWORD(&pdc->dc_rbuf[i][0]);
		rp->r_segbh = HIWORD(&pdc->dc_rbuf[i][0]);
		rp->r_flags = RFLG_OWN;
		rp->r_slen = ETHER_MAX_LEN;
	}
	for (i=0; i < NXMT; i++) {
		volatile struct de_ring *rp = &dc->dc_xrent[i];

		rp->r_segbl = LOWORD(&pdc->dc_xbuf[i][0]);
		rp->r_segbh = HIWORD(&pdc->dc_xbuf[i][0]);
		rp->r_tdrerr = 0;
		rp->r_flags = 0;
	}
	crx = ctx = 0;

	/* set the transmit and receive ring header addresses */
	dc->dc_pcbb.pcbb0 = FC_WTRING;
	dc->dc_pcbb.pcbb2 = LOWORD(&pdc->dc_udbbuf);
	dc->dc_pcbb.pcbb4 = HIWORD(&pdc->dc_udbbuf);

	dc->dc_udbbuf.b_tdrbl = LOWORD(&pdc->dc_xrent[0]);
	dc->dc_udbbuf.b_tdrbh = HIWORD(&pdc->dc_xrent[0]);
	dc->dc_udbbuf.b_telen = sizeof (struct de_ring) / sizeof(u_int16_t);
	dc->dc_udbbuf.b_trlen = NXMT;
	dc->dc_udbbuf.b_rdrbl = LOWORD(&pdc->dc_rrent[0]);
	dc->dc_udbbuf.b_rdrbh = HIWORD(&pdc->dc_rrent[0]);
	dc->dc_udbbuf.b_relen = sizeof (struct de_ring) / sizeof(u_int16_t);
	dc->dc_udbbuf.b_rrlen = NRCV;

	DE_WLOW(CMD_GETCMD);
	dewait("wtring");

	dc->dc_pcbb.pcbb0 = FC_WTMODE;
	dc->dc_pcbb.pcbb2 = MOD_DRDC|MOD_TPAD|MOD_HDX;
	DE_WLOW(CMD_GETCMD);
	dewait("wtmode");

	DE_WLOW(CMD_START);
	dewait("start");

	DE_WLOW(CMD_PDMD);
	dewait("initpoll");
	/* Should be running by now */

	net_devinit(f, &de_driver, eaddr);

	return 0;
}

int
de_get(struct iodesc *desc, void *pkt, size_t maxlen, time_t timeout)
{
	volatile int to = 100000 * timeout;
	int len, csr0;

	if ((csr0 = DE_RCSR(DE_PCSR0)) & PCSR0_INTR)
		DE_WHIGH(csr0 >> 8);
retry:
	if (to-- == 0)
		return 0;

	if (dc->dc_rrent[crx].r_flags & RFLG_OWN)
		goto retry;

	if (dc->dc_rrent[crx].r_flags & RFLG_ERRS)
		len = 0;
	else
		len = dc->dc_rrent[crx].r_lenerr & RERR_MLEN;

	if (len > maxlen)
		len = maxlen;
	if (len)
		bcopy((char *)&dc->dc_rbuf[crx][0], pkt, len);

	dc->dc_rrent[crx].r_flags = RFLG_OWN;
	dc->dc_rrent[crx].r_lenerr = 0;
#ifdef DEV_DEBUG
	printf("Got packet: len %d idx %d maxlen %ld\n", len, crx, maxlen);
#endif
	if (++crx == NRCV)
		crx = 0;

	if (len == 0)
		goto retry;
	return len;
}


int
de_put(struct iodesc *desc, void *pkt, size_t len)
{
	volatile int to = 100000;
	int csr0;

	if ((csr0 = DE_RCSR(DE_PCSR0)) & PCSR0_INTR)
		DE_WHIGH(csr0 >> 8);
#ifdef DEV_DEBUG
	printf("de_put: len %ld\n", len);
#endif
retry:
	if (to-- == 0)
		return -1;

	if (dc->dc_xrent[ctx].r_flags & RFLG_OWN)
		goto retry;

	bcopy(pkt, (char *)&dc->dc_xbuf[ctx][0], len);

	dc->dc_xrent[ctx].r_slen = len;
	dc->dc_xrent[ctx].r_tdrerr = 0;
	dc->dc_xrent[ctx].r_flags = XFLG_OWN|XFLG_STP|XFLG_ENP;

	DE_WLOW(CMD_PDMD);
	dewait("start");

	if (++ctx == NXMT)
		ctx = 0;
	return len;
}

int
declose(struct open_file *f)
{
	DE_WCSR(DE_PCSR0, PCSR0_RSET);
	dewait("close");
	return 0;
}

void
dewait(char *fn)
{
	int csr0;

#ifdef DEV_DEBUG
	printf("dewait: %s...", fn);
#endif
	while ((DE_RCSR(DE_PCSR0) & PCSR0_INTR) == 0)
		;
	csr0 = DE_RCSR(DE_PCSR0);
	DE_WHIGH(csr0 >> 8);
#ifdef DEV_DEBUG
	if (csr0 & PCSR0_PCEI)
		printf("failed! CSR0 %x", csr0);
	else
		printf("done");
	printf(", PCSR1 %x\n", DE_RCSR(DE_PCSR1));
#endif
}
