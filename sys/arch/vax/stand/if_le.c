/*	$OpenBSD: if_le.c,v 1.1 1998/02/03 11:48:27 maja Exp $ */
/*	$NetBSD: if_le.c,v 1.2 1997/03/22 12:47:31 ragge Exp $ */
/*
 * Copyright (c) 1997 Ludd, University of Lule}, Sweden.
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
 */

/*
 * Standalone routine for MicroVAX LANCE chip. 
 */

#include <sys/param.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>

#include <lib/libsa/netif.h>

#include <dev/ic/am7990reg.h>

#define	ETHER_MIN_LEN	64
#define	ETHER_MAX_LEN	1518
#define	ETHER_ADDR_LEN	6

/*
 * The following are incorrect. Why doesn't DEC follow its own specs???
 */
#define TLEN    1
#define NTBUF   (1 << TLEN)
#define RLEN    3
#define NRBUF   (1 << RLEN)
#define BUFSIZE 1518

#define	QW_ALLOC(x)	((alloc((x) + 7) + 7) & ~7)

int le_probe(), le_match(), le_get(), le_put();
void le_init();

struct netif_stats le_stats;

struct netif_dif le_ifs[] = {
/*	dif_unit	dif_nsel	dif_stats	dif_private	*/
{	0,		1,		&le_stats,	},
};

struct netif_stats le_stats;

struct netif_driver le_driver = {
	"le", le_match, le_probe, le_init, le_get, le_put, 0, le_ifs, 1,
};

/*
 * Init block & buffer descriptors according to DEC system
 * specification documentation.
 */
struct initblock {
	short	ib_mode;
	char	ib_padr[6]; /* Ethernet address */
	int	ib_ladrf1;
	int	ib_ladrf2;
	int	ib_rdr; /* Receive address */
	int	ib_tdr; /* Transmit address */
} *initblock = NULL;

struct nireg {
	volatile short ni_rdp;       /* data port */
	volatile short ni_pad0;
	volatile short ni_rap;       /* register select port */
} *nireg = (struct nireg *)0x200e0000;


volatile struct	buffdesc {
	int	bd_adrflg;
	short	bd_bcnt;
	short	bd_mcnt;
} *rdesc, *tdesc;

/* Flags in the address field */
#define	BR_OWN	0x80000000
#define	BR_ERR	0x40000000
#define	BR_FRAM	0x20000000
#define	BR_OFLO	0x10000000
#define	BR_CRC	0x08000000
#define	BR_BUFF	0x04000000
#define	BR_STP	0x02000000
#define	BR_ENP	0x01000000

#define	BT_OWN	0x80000000
#define	BT_ERR	0x40000000
#define	BT_MORE	0x10000000
#define	BT_ONE	0x08000000
#define	BT_DEF	0x04000000
#define	BT_STP	0x02000000
#define	BT_ENP	0x01000000

int	next_rdesc, next_tdesc;

#define	LEWRCSR(port, val) { \
	nireg->ni_rap = (port); \
	nireg->ni_rdp = (val); \
}

#define	LERDCSR(port) \
	(nireg->ni_rap = port, nireg->ni_rdp)
 
int
le_match(nif, machdep_hint)
	struct netif *nif;
	void *machdep_hint;
{
	return strcmp(machdep_hint, "le") == 0;
}

le_probe(nif, machdep_hint)
	struct netif *nif;
	void *machdep_hint;
{
	return 0;
}

void
le_init(desc, machdep_hint)
	struct iodesc *desc;
	void *machdep_hint;
{
	int stat, i, *ea;
	volatile int to = 100000;

	*(int *)0x20080014 = 0; /* Be sure we do DMA in low 16MB */
	next_rdesc = next_tdesc = 0;

	LEWRCSR(LE_CSR0, LE_C0_STOP);
	while (to--)
		;

	if (initblock == NULL) {
		ea = (void *)0x20090000; /* XXX ethernetadressen */
		for (i = 0; i < 6; i++)
			desc->myea[i] = ea[i] & 0377;

		initblock = (void *)alloc(sizeof(struct initblock));
		initblock->ib_mode = LE_MODE_NORMAL;
		bcopy(desc->myea, initblock->ib_padr, 6);
		initblock->ib_ladrf1 = 0;
		initblock->ib_ladrf2 = 0;

		(int)rdesc = QW_ALLOC(sizeof(struct buffdesc) * NRBUF);
		initblock->ib_rdr = (RLEN << 29) | (int)rdesc;
		(int)tdesc = QW_ALLOC(sizeof(struct buffdesc) * NTBUF);
		initblock->ib_tdr = (TLEN << 29) | (int)tdesc;

		for (i = 0; i < NRBUF; i++) {
			rdesc[i].bd_adrflg = alloc(BUFSIZE) | BR_OWN;
			rdesc[i].bd_bcnt = -BUFSIZE;
			rdesc[i].bd_mcnt = 0;
		}

		for (i = 0; i < NTBUF; i++) {
			tdesc[i].bd_adrflg = alloc(BUFSIZE);
			tdesc[i].bd_bcnt = 0xf000;
			tdesc[i].bd_mcnt = 0;
		}
	}

	LEWRCSR(LE_CSR1, (int)initblock & 0xffff);
	LEWRCSR(LE_CSR2, ((int)initblock >> 16) & 0xff);

	LEWRCSR(LE_CSR0, LE_C0_INIT);

	to = 100000;
	while (to--)
		if (LERDCSR(LE_CSR0) & LE_C0_IDON)
			break;

	LEWRCSR(LE_CSR0, LE_C0_INEA | LE_C0_STRT | LE_C0_IDON);
}

int
le_get(desc, pkt, maxlen, timeout)
	struct iodesc *desc;
	void *pkt;
	int maxlen;
	time_t timeout;
{
	int csr, len;
	volatile int to = 100000 * timeout;

retry:
	if (to-- == 0)
		return 0;

	csr = LERDCSR(LE_CSR0);
	LEWRCSR(LE_CSR0, csr & (LE_C0_BABL|LE_C0_MISS|LE_C0_MERR|LE_C0_RINT));

	if (rdesc[next_rdesc].bd_adrflg & BR_OWN)
		goto retry;

        if (rdesc[next_rdesc].bd_adrflg & BR_ERR)
                len = 0;
        else {
		if ((len = rdesc[next_rdesc].bd_mcnt - 4) > maxlen)
			len = maxlen;

		bcopy((void *)(rdesc[next_rdesc].bd_adrflg&0xffffff),pkt,len);
	}

	rdesc[next_rdesc].bd_mcnt = 0;
	rdesc[next_rdesc].bd_adrflg |= BR_OWN;
	if (++next_rdesc >= NRBUF)
		next_rdesc = 0;

	if (len == 0)
		goto retry;
	return len;
}

int
le_put(desc, pkt, len)
	struct iodesc *desc;
	void *pkt;
	int len;
{
	volatile int to = 100000;
	int csr;

retry:
	if (--to == 0)
		return -1;

	csr = LERDCSR(LE_CSR0);
	LEWRCSR(LE_CSR0, csr & (LE_C0_MISS|LE_C0_CERR|LE_C0_TINT));

	if (tdesc[next_tdesc].bd_adrflg & BT_OWN)
		goto retry;

	bcopy(pkt, (void *)(tdesc[next_tdesc].bd_adrflg & 0xffffff), len);
	tdesc[next_tdesc].bd_bcnt =
	    (len < ETHER_MIN_LEN ? -ETHER_MIN_LEN : -len);
	tdesc[next_tdesc].bd_mcnt = 0;
	tdesc[next_tdesc].bd_adrflg |= BT_OWN | BT_STP | BT_ENP;

	LEWRCSR(LE_CSR0, LE_C0_TDMD);

	to = 100000;
	while (((LERDCSR(LE_CSR0) & LE_C0_TINT) == 0) && --to)
		;

	LEWRCSR(LE_CSR0, LE_C0_TINT);
	if (++next_tdesc >= NTBUF)
		next_tdesc = 0;

	if (to)
		return len;

	return -1;
}
