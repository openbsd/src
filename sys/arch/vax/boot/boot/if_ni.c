/*	$OpenBSD: if_ni.c,v 1.1 2002/06/11 09:36:23 hugh Exp $ */
/*	$NetBSD: if_ni.c,v 1.2 2000/07/10 10:40:38 ragge Exp $ */
/*
 * Copyright (c) 2000 Ludd, University of Lule}, Sweden.
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
 *	This product includes software developed at Ludd, University of 
 *	Lule}, Sweden and its contributors.
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
 * Standalone routine for DEBNA Ethernet controller.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>

#include <../include/sid.h>
#include <../include/rpb.h>
#include <../include/pte.h>
#include <../include/macros.h>
#include <../include/mtpr.h>
#include <../include/scb.h>

#include <lib/libkern/libkern.h>

#include <lib/libsa/netif.h>
#include <lib/libsa/stand.h>
#include <lib/libsa/net.h>

#include <arch/vax/bi/bireg.h>

#include "vaxstand.h"

#undef NIDEBUG
/*
 * Tunable buffer parameters. Good idea to have them as power of 8; then
 * they will fit into a logical VAX page.
 */
#define NMSGBUF		8	/* Message queue entries */
#define NTXBUF		16	/* Transmit queue entries */
#define NTXFRAGS	1	/* Number of transmit buffer fragments */
#define NRXBUF		24	/* Receive queue entries */
#define NBDESCS		(NTXBUF + NRXBUF)
#define NQUEUES		3	/* RX + TX + MSG */
#define PKTHDR		18	/* Length of (control) packet header */
#define RXADD		18	/* Additional length of receive datagram */
#define TXADD		18	/*	""	transmit   ""	 */
#define MSGADD		134	/*	""	message	   ""	 */

#include <arch/vax/bi/if_nireg.h>


#define SPTSIZ	16384	/* 8MB */
#define roundpg(x)	(((int)x + VAX_PGOFSET) & ~VAX_PGOFSET)
#define ALLOC(x) \
	allocbase;xbzero((caddr_t)allocbase,x);allocbase+=roundpg(x);
#define nipqb	(&gvppqb->nc_pqb)
#define gvp	gvppqb
#define NI_WREG(csr, val) *(volatile long *)(niaddr + (csr)) = (val)
#define NI_RREG(csr)	*(volatile long *)(niaddr + (csr))
#define DELAY(x)	{volatile int i = x * 3;while (--i);}
#define WAITREG(csr,val) while (NI_RREG(csr) & val);

static int ni_get(struct iodesc *, void *, size_t, time_t);
static int ni_put(struct iodesc *, void *, size_t);

static int *syspte, allocbase, niaddr;
static struct ni_gvppqb *gvppqb;
static struct ni_fqb *fqb;
static struct ni_bbd *bbd;
static char enaddr[6];
static int beenhere = 0;

struct netif_driver ni_driver = {
	0, 0, 0, 0, ni_get, ni_put,
};

static void
xbzero(char *a, int s)
{
	while (s--)
		*a++ = 0;
}

static int
failtest(int reg, int mask, int test, char *str)
{
	int i = 100;

	do {
		DELAY(100000);
	} while (((NI_RREG(reg) & mask) != test) && --i);

	if (i == 0) {
		printf("ni: %s\n", str);
		return 1;
	}
	return 0;
}

static int
INSQTI(void *e, void *h)
{
	int ret;

	while ((ret = insqti(e, h)) == ILCK_FAILED)
		;
	return ret;
}

static void *
REMQHI(void *h)
{
	void *ret;

	while ((ret = remqhi(h)) == (void *)ILCK_FAILED)
		;
	return ret;
}

static void
puton(void *pkt, void *q, int args)
{
	INSQTI(pkt, q);

	WAITREG(NI_PCR, PCR_OWN);
	NI_WREG(NI_PCR, args);
	WAITREG(NI_PCR, PCR_OWN);
}

static void
remput(void *fq, void *pq, int args)
{
	struct ni_dg *data;
	int res;

	while ((data = REMQHI(fq)) == 0)
		;

	res = INSQTI(data, pq);
	if (res == Q_EMPTY) {
		WAITREG(NI_PCR, PCR_OWN);
		NI_WREG(NI_PCR, args);
	}
}

static void
insput(void *elem, void *q, int args)
{
	int res;

	res = INSQTI(elem, q);
	if (res == Q_EMPTY) {
		WAITREG(NI_PCR, PCR_OWN);
		NI_WREG(NI_PCR, args);
	}
}

int
niopen(struct open_file *f, int adapt, int ctlr, int unit, int part)
{
	struct ni_dg *data;
	struct ni_msg *msg;
	struct ni_ptdb *ptdb;
	int i, va, res;

	if (beenhere++ && askname == 0)
		return 0;

	niaddr = nexaddr & ~(BI_NODESIZE - 1);
	bootrpb.csrphy = niaddr;
	if (adapt >= 0)
		bootrpb.adpphy = adapt;
	/*
	 * We need a bunch of memory, take it from our load
	 * address plus 1M.
	 */
	allocbase = RELOC + 1024 * 1024;
	/*
	 * First create a SPT for the first 8MB of physmem.
	 */
	syspte = (int *)ALLOC(SPTSIZ*4);
	for (i = 0; i < SPTSIZ; i++)
		syspte[i] = PG_V|PG_RW|i;


	gvppqb = (struct ni_gvppqb *)ALLOC(sizeof(struct ni_gvppqb));
	fqb = (struct ni_fqb *)ALLOC(sizeof(struct ni_fqb));
	bbd = (struct ni_bbd *)ALLOC(sizeof(struct ni_bbd) * NBDESCS);

	/* Init the PQB struct */
	nipqb->np_spt = nipqb->np_gpt = (int)syspte;
	nipqb->np_sptlen = nipqb->np_gptlen = SPTSIZ;
	nipqb->np_vpqb = (u_int32_t)gvp;
	nipqb->np_bvplvl = 1;
	nipqb->np_vfqb = (u_int32_t)fqb;
	nipqb->np_vbdt = (u_int32_t)bbd;
	nipqb->np_nbdr = NBDESCS;

	/* Free queue block */
	nipqb->np_freeq = NQUEUES;
	fqb->nf_mlen = PKTHDR+MSGADD;
	fqb->nf_dlen = PKTHDR+TXADD;
	fqb->nf_rlen = PKTHDR+RXADD;
#ifdef NIDEBUG
	printf("niopen: syspte %p gvp %p fqb %p bbd %p\n",
	    syspte, gvppqb, fqb, bbd);
#endif

	NI_WREG(BIREG_VAXBICSR, NI_RREG(BIREG_VAXBICSR) | BICSR_NRST);
	DELAY(500000);
	i = 20;
	while ((NI_RREG(BIREG_VAXBICSR) & BICSR_BROKE) && --i)
		DELAY(500000);
#ifdef NIDEBUG
	if (i == 0) {
		printf("ni: BROKE bit set after reset\n");
		return 1;
	}
#endif
	/* Check state */
	if (failtest(NI_PSR, PSR_STATE, PSR_UNDEF, "not undefined state"))
		return 1;

	/* Clear owner bits */
	NI_WREG(NI_PSR, NI_RREG(NI_PSR) & ~PSR_OWN);
	NI_WREG(NI_PCR, NI_RREG(NI_PCR) & ~PCR_OWN);

	/* kick off init */
	NI_WREG(NI_PCR, (int)gvppqb | PCR_INIT | PCR_OWN);
	while (NI_RREG(NI_PCR) & PCR_OWN)
		DELAY(100000);

	/* Check state */
	if (failtest(NI_PSR, PSR_INITED, PSR_INITED, "failed initialize"))
		return 1;

	NI_WREG(NI_PSR, NI_RREG(NI_PSR) & ~PSR_OWN);
	WAITREG(NI_PCR, PCR_OWN);
	NI_WREG(NI_PCR, PCR_OWN|PCR_ENABLE);
	WAITREG(NI_PCR, PCR_OWN);
	WAITREG(NI_PSR, PSR_OWN);

	/* Check state */
	if (failtest(NI_PSR, PSR_STATE, PSR_ENABLED, "failed enable"))
		return 1;

	NI_WREG(NI_PSR, NI_RREG(NI_PSR) & ~PSR_OWN);

#ifdef NIDEBUG
	printf("Set up message free queue\n");
#endif

	/* Set up message free queue */
	va = ALLOC(NMSGBUF * 512);
	for (i = 0; i < NMSGBUF; i++) {
		msg = (void *)(va + i * 512);

		res = INSQTI(msg, &fqb->nf_mforw);
	}
	WAITREG(NI_PCR, PCR_OWN);
	NI_WREG(NI_PCR, PCR_FREEQNE|PCR_MFREEQ|PCR_OWN);
	WAITREG(NI_PCR, PCR_OWN);

#ifdef NIDEBUG
	printf("Set up xmit queue\n");
#endif

	/* Set up xmit queue */
	va = ALLOC(NTXBUF * 512);
	for (i = 0; i < NTXBUF; i++) {
		struct ni_dg *data;

		data = (void *)(va + i * 512);
		data->nd_status = 0;
		data->nd_len = TXADD;
		data->nd_ptdbidx = 1;
		data->nd_opcode = BVP_DGRAM;
		data->bufs[0]._offset = 0;
		data->bufs[0]._key = 1;
		data->nd_cmdref = allocbase;
		bbd[i].nb_key = 1;
		bbd[i].nb_status = 0;
		bbd[i].nb_pte = (int)&syspte[allocbase>>9];
		allocbase += 2048;
		data->bufs[0]._index = i;

		res = INSQTI(data, &fqb->nf_dforw);
	}
	WAITREG(NI_PCR, PCR_OWN);
	NI_WREG(NI_PCR, PCR_FREEQNE|PCR_DFREEQ|PCR_OWN);
	WAITREG(NI_PCR, PCR_OWN);

#ifdef NIDEBUG
	printf("recv buffers\n");
#endif

	/* recv buffers */
	va = ALLOC(NRXBUF * 512);
	for (i = 0; i < NRXBUF; i++) {
		struct ni_dg *data;
		struct ni_bbd *bd;
		int idx;

		data = (void *)(va + i * 512);
		data->nd_cmdref = allocbase;
		data->nd_len = RXADD;
		data->nd_opcode = BVP_DGRAMRX;
		data->nd_ptdbidx = 2;
		data->bufs[0]._key = 1;

		idx = NTXBUF + i;
		bd = &bbd[idx];
		bd->nb_pte = (int)&syspte[allocbase>>9];
		allocbase += 2048;
		bd->nb_len = 2048;
		bd->nb_status = NIBD_VALID;
		bd->nb_key = 1;
		data->bufs[0]._offset = 0;
		data->bufs[0]._len = bd->nb_len;
		data->bufs[0]._index = idx;

		res = INSQTI(data, &fqb->nf_rforw);
	}
	WAITREG(NI_PCR, PCR_OWN);
	NI_WREG(NI_PCR, PCR_FREEQNE|PCR_RFREEQ|PCR_OWN);
	WAITREG(NI_PCR, PCR_OWN);

#ifdef NIDEBUG
	printf("Set initial parameters\n");
#endif

	/* Set initial parameters */
	msg = REMQHI(&fqb->nf_mforw);

	msg->nm_opcode = BVP_MSG;
	msg->nm_status = 0;
	msg->nm_len = sizeof(struct ni_param) + 6;
	msg->nm_opcode2 = NI_WPARAM;
	((struct ni_param *)&msg->nm_text[0])->np_flags = NP_PAD;

	puton(msg, &gvp->nc_forw0, PCR_CMDQNE|PCR_CMDQ0|PCR_OWN);


	while ((data = REMQHI(&gvp->nc_forwr)) == 0)
		;

	msg = (struct ni_msg *)data;
#ifdef NIDEBUG
	if (msg->nm_opcode2 != NI_WPARAM) {
		printf("ni: wrong response code %d\n", msg->nm_opcode2);
		insput(data, &fqb->nf_mforw, PCR_FREEQNE|PCR_MFREEQ|PCR_OWN);
	}
#endif
	bcopy(((struct ni_param *)&msg->nm_text[0])->np_dpa,
	    enaddr, ETHER_ADDR_LEN);
	insput(data, &fqb->nf_mforw, PCR_FREEQNE|PCR_MFREEQ|PCR_OWN);

#ifdef NIDEBUG
	printf("Clear counters\n");
#endif

	/* Clear counters */
	msg = REMQHI(&fqb->nf_mforw);
	msg->nm_opcode = BVP_MSG;
	msg->nm_status = 0;
	msg->nm_len = sizeof(struct ni_param) + 6;
	msg->nm_opcode2 = NI_RCCNTR;

	puton(msg, &gvp->nc_forw0, PCR_CMDQNE|PCR_CMDQ0|PCR_OWN);
	remput(&gvp->nc_forwr, &fqb->nf_mforw, PCR_FREEQNE|PCR_MFREEQ|PCR_OWN);

#ifdef NIDEBUG
	printf("Enable transmit logic\n");
#endif		

	/* Enable transmit logic */
	msg = REMQHI(&fqb->nf_mforw);

	msg->nm_opcode = BVP_MSG;
	msg->nm_status = 0;
	msg->nm_len = 18;
	msg->nm_opcode2 = NI_STPTDB;
	ptdb = (struct ni_ptdb *)&msg->nm_text[0];
	bzero(ptdb, sizeof(struct ni_ptdb));
	ptdb->np_index = 1;
	ptdb->np_fque = 1;

	puton(msg, &gvp->nc_forw0, PCR_CMDQNE|PCR_CMDQ0|PCR_OWN);
	remput(&gvp->nc_forwr, &fqb->nf_mforw, PCR_FREEQNE|PCR_MFREEQ|PCR_OWN);

#ifdef NIDEBUG
	printf("ni: hardware address %s\n", ether_sprintf(enaddr));
	printf("Setting receive parameters\n");
#endif
	msg = REMQHI(&fqb->nf_mforw);
	ptdb = (struct ni_ptdb *)&msg->nm_text[0];
	bzero(ptdb, sizeof(struct ni_ptdb));
	msg->nm_opcode = BVP_MSG;
	msg->nm_len = 18;
	ptdb->np_index = 2;
	ptdb->np_fque = 2;
	msg->nm_opcode2 = NI_STPTDB;
	ptdb->np_type = ETHERTYPE_IP;
	ptdb->np_flags = PTDB_UNKN|PTDB_BDC;
	memset(ptdb->np_mcast[0], 0xff, ETHER_ADDR_LEN);
	ptdb->np_adrlen = 1;
	msg->nm_len += 8;
	insput(msg, &gvp->nc_forw0, PCR_CMDQNE|PCR_CMDQ0|PCR_OWN);
	remput(&gvp->nc_forwr, &fqb->nf_mforw, PCR_FREEQNE|PCR_MFREEQ|PCR_OWN);

#ifdef NIDEBUG
	printf("finished\n");
#endif

	net_devinit(f, &ni_driver, enaddr);
	return 0;
}

int
ni_get(struct iodesc *desc, void *pkt, size_t maxlen, time_t timeout)
{
	struct ni_dg *data;
	struct ni_bbd *bd;
	int nsec = getsecs() + timeout;
	int len, idx;

loop:	while ((data = REMQHI(&gvp->nc_forwr)) == 0 && (nsec > getsecs()))
		;

	if (nsec <= getsecs())
		return 0;

	switch (data->nd_opcode) {
	case BVP_DGRAMRX:
		idx = data->bufs[0]._index;
		bd = &bbd[idx];
		len = data->bufs[0]._len;
		if (len > maxlen)
			len = maxlen;
		bcopy((caddr_t)data->nd_cmdref, pkt, len);
		bd->nb_pte = (int)&syspte[data->nd_cmdref>>9];
		data->bufs[0]._len = bd->nb_len = 2048;
		data->bufs[0]._offset = 0;
		data->bufs[0]._key = 1;
		bd->nb_status = NIBD_VALID;
		bd->nb_key = 1;
		data->nd_len = RXADD;
		data->nd_status = 0;
		insput(data, &fqb->nf_rforw,
		    PCR_FREEQNE|PCR_RFREEQ|PCR_OWN);
		return len;

	case BVP_DGRAM:
		insput(data, &fqb->nf_dforw, PCR_FREEQNE|PCR_DFREEQ|PCR_OWN);
		break;
	default:
		insput(data, &fqb->nf_mforw, PCR_FREEQNE|PCR_MFREEQ|PCR_OWN);
		break;
	}

	NI_WREG(NI_PSR, NI_RREG(NI_PSR) & ~(PSR_OWN|PSR_RSQ));
	goto loop;
}

int
ni_put(struct iodesc *desc, void *pkt, size_t len)
{
	struct ni_dg *data;
	struct ni_bbd *bdp;

	data = REMQHI(&fqb->nf_dforw);
#ifdef NIDEBUG
	if (data == 0) {
		printf("ni_put: driver problem, data == 0\n");
		return -1;
	}
#endif
	bdp = &bbd[(data->bufs[0]._index & 0x7fff)];
	bdp->nb_status = NIBD_VALID;
	bdp->nb_len = (len < 64 ? 64 : len);
	bcopy(pkt, (caddr_t)data->nd_cmdref, len);
	data->bufs[0]._offset = 0;
	data->bufs[0]._len = bdp->nb_len;
	data->nd_opcode = BVP_DGRAM;
	data->nd_pad3 = 1;
	data->nd_ptdbidx = 1;
	data->nd_len = 18;
	insput(data, &gvp->nc_forw0, PCR_CMDQNE|PCR_CMDQ0|PCR_OWN);
	return len;
}

int
niclose(struct open_file *f)
{
	if (beenhere) {
		WAITREG(NI_PCR, PCR_OWN);
		NI_WREG(NI_PCR, PCR_OWN|PCR_SHUTDOWN);
		WAITREG(NI_PCR, PCR_OWN);
	}
	return 0;
}
