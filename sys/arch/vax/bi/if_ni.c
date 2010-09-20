/*	$OpenBSD: if_ni.c,v 1.15 2010/09/20 07:40:41 deraadt Exp $ */
/*	$NetBSD: if_ni.c,v 1.15 2002/05/22 16:03:14 wiz Exp $ */
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
 * Driver for DEBNA/DEBNT/DEBNK ethernet cards.
 * Things that is still to do:
 *	Collect statistics.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/sched.h>

#include <net/if.h>
#include <net/if_ether.h>
#include <net/if_dl.h>

#include <netinet/in.h>
#include <netinet/if_inarp.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/bus.h>
#ifdef __vax__
#include <machine/mtpr.h>
#include <machine/pte.h>
#endif

#include <dev/bi/bireg.h>
#include <dev/bi/bivar.h>

/*
 * Tunable buffer parameters. Good idea to have them as power of 8; then
 * they will fit into a logical VAX page.
 */
#define NMSGBUF		8	/* Message queue entries */
#define NTXBUF		16	/* Transmit queue entries */
#define NTXFRAGS	8	/* Number of transmit buffer fragments */
#define NRXBUF		24	/* Receive queue entries */
#define NBDESCS		(NTXBUF * NTXFRAGS + NRXBUF)
#define NQUEUES		3	/* RX + TX + MSG */
#define PKTHDR		18	/* Length of (control) packet header */
#define RXADD		18	/* Additional length of receive datagram */
#define TXADD		(10+NTXFRAGS*8) /*	""	transmit   ""	 */
#define MSGADD		134	/*		""	message	   ""	 */

#include <dev/bi/if_nireg.h>	/* XXX include earlier */

/*
 * Macros for (most cases of) insqti/remqhi.
 * Retry NRETRIES times to do the operation, if it still fails assume
 * a lost lock and panic.
 */
#define	NRETRIES	100
#define	INSQTI(e, h)	({						\
	int ret, i;							\
	for (i = 0; i < NRETRIES; i++) {				\
		if ((ret = insqti(e, h)) != ILCK_FAILED)		\
			break;						\
	}								\
	if (i == NRETRIES)						\
		panic("ni: insqti failed at %d", __LINE__);		\
	ret;								\
})
#define	REMQHI(h)	({						\
	int i;void *ret;						\
	for (i = 0; i < NRETRIES; i++) {				\
		if ((ret = remqhi(h)) != (void *)ILCK_FAILED)		\
			break;						\
	}								\
	if (i == NRETRIES)						\
		panic("ni: remqhi failed at %d", __LINE__);		\
	ret;								\
})


#define nipqb	(&sc->sc_gvppqb->nc_pqb)
#define gvp	sc->sc_gvppqb
#define fqb	sc->sc_fqb
#define bbd	sc->sc_bbd

struct	ni_softc {
	struct device	sc_dev;		/* Configuration common part	*/
	struct ethercom sc_ec;		/* Ethernet common part		*/
#define sc_if	sc_ec.ec_if		/* network-visible interface	*/
	bus_space_tag_t sc_iot;
	bus_addr_t	sc_ioh;
	bus_dma_tag_t	sc_dmat;
	struct ni_gvppqb *sc_gvppqb;	/* Port queue block		*/
	struct ni_gvppqb *sc_pgvppqb;	/* Phys address of PQB		*/
	struct ni_fqb	*sc_fqb;	/* Free Queue block		*/
	struct ni_bbd	*sc_bbd;	/* Buffer descriptors		*/
	u_int8_t	sc_enaddr[ETHER_ADDR_LEN];
};

static	int	nimatch(struct device *, struct cfdata *, void *);
static	void	niattach(struct device *, struct device *, void *);
static	void	niinit(struct ni_softc *);
static	void	nistart(struct ifnet *);
static	void	niintr(void *);
static	int	niioctl(struct ifnet *, u_long, caddr_t);
static	int	ni_add_rxbuf(struct ni_softc *, struct ni_dg *, int);
static	void	ni_setup(struct ni_softc *);
static	void	nitimeout(struct ifnet *);
static	void ni_getpgs(struct ni_softc *sc, int size, caddr_t *v, paddr_t *p);
static	int failtest(struct ni_softc *, int, int, int, char *);

volatile int endwait, retry;	/* Used during autoconfig */

struct	cfattach ni_ca = {
	sizeof(struct ni_softc), nimatch, niattach
};

#define NI_WREG(csr, val) \
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, csr, val)
#define NI_RREG(csr) \
	bus_space_read_4(sc->sc_iot, sc->sc_ioh, csr)

#define WAITREG(csr,val) while (NI_RREG(csr) & val);
/*
 * Check for present device.
 */
int
nimatch(parent, cf, aux)
	struct	device *parent;
	struct	cfdata *cf;
	void	*aux;
{
	struct bi_attach_args *ba = aux;
	u_short type;

	type = bus_space_read_2(ba->ba_iot, ba->ba_ioh, BIREG_DTYPE);
	if (type != BIDT_DEBNA && type != BIDT_DEBNT && type != BIDT_DEBNK)
		return 0;

	if (cf->cf_loc[BICF_NODE] != BICF_NODE_DEFAULT &&
	    cf->cf_loc[BICF_NODE] != ba->ba_nodenr)
		return 0;

	return 1;
}

/*
 * Allocate a bunch of descriptor-safe memory.
 * We need to get the structures from the beginning of its own pages.
 */
static void
ni_getpgs(struct ni_softc *sc, int size, caddr_t *v, paddr_t *p)
{
	bus_dma_segment_t seg;
	int nsegs, error;

	if ((error = bus_dmamem_alloc(sc->sc_dmat, size, NBPG, 0, &seg, 1,
	    &nsegs, BUS_DMA_NOWAIT)) != 0)
		panic(" can't allocate memory: error %d", error);

	if ((error = bus_dmamem_map(sc->sc_dmat, &seg, nsegs, size, v,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0)
		panic(" can't map mem space: error %d", error);

	if (p)
		*p = seg.ds_addr;
	memset(*v, 0, size);
}

static int
failtest(struct ni_softc *sc, int reg, int mask, int test, char *str)
{
	int i = 100;

	do {
		DELAY(100000);
	} while (((NI_RREG(reg) & mask) != test) && --i);

	if (i == 0) {
		printf("%s: %s\n", sc->sc_dev.dv_xname, str);
		return 1;
	}
	return 0;
}


/*
 * Interface exists: make available by filling in network interface
 * record.  System will initialize the interface when it is ready
 * to accept packets.
 */
void
niattach(parent, self, aux)
	struct	device *parent, *self;
	void	*aux;
{
	struct bi_attach_args *ba = aux;
	struct ni_softc *sc = (struct ni_softc *)self;
	struct ifnet *ifp = (struct ifnet *)&sc->sc_if;
	struct ni_msg *msg;
	struct ni_ptdb *ptdb;
	caddr_t va;
	int i, j, s, res;
	u_short type;

	type = bus_space_read_2(ba->ba_iot, ba->ba_ioh, BIREG_DTYPE);
	printf(": DEBN%c\n", type == BIDT_DEBNA ? 'A' : type == BIDT_DEBNT ?
	    'T' : 'K');
	sc->sc_iot = ba->ba_iot;
	sc->sc_ioh = ba->ba_ioh;
	sc->sc_dmat = ba->ba_dmat;

	bi_intr_establish(ba->ba_icookie, ba->ba_ivec, niintr, sc);

	ni_getpgs(sc, sizeof(struct ni_gvppqb), (caddr_t *)&sc->sc_gvppqb, 
	    (paddr_t *)&sc->sc_pgvppqb);
	ni_getpgs(sc, sizeof(struct ni_fqb), (caddr_t *)&sc->sc_fqb, 0);
	ni_getpgs(sc, NBDESCS * sizeof(struct ni_bbd),
	    (caddr_t *)&sc->sc_bbd, 0);
	/*
	 * Zero the newly allocated memory.
	 */

	nipqb->np_veclvl = (ba->ba_ivec << 2) + 2;
	nipqb->np_node = ba->ba_intcpu;
	nipqb->np_vpqb = (u_int32_t)gvp;
#ifdef __vax__
	nipqb->np_spt = nipqb->np_gpt = mfpr(PR_SBR);
	nipqb->np_sptlen = nipqb->np_gptlen = mfpr(PR_SLR);
#else
#error Must fix support for non-vax.
#endif
	nipqb->np_bvplvl = 1;
	nipqb->np_vfqb = (u_int32_t)fqb;
	nipqb->np_vbdt = (u_int32_t)bbd;
	nipqb->np_nbdr = NBDESCS;

	/* Free queue block */
	nipqb->np_freeq = NQUEUES;
	fqb->nf_mlen = PKTHDR+MSGADD;
	fqb->nf_dlen = PKTHDR+TXADD;
	fqb->nf_rlen = PKTHDR+RXADD;

	strlcpy(ifp->if_xname, sc->sc_dev.dv_xname, sizeof ifp->if_xname);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_start = nistart;
	ifp->if_ioctl = niioctl;
	ifp->if_watchdog = nitimeout;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * Start init sequence.
	 */

	/* Reset the node */
	NI_WREG(BIREG_VAXBICSR, NI_RREG(BIREG_VAXBICSR) | BICSR_NRST);
	DELAY(500000);
	i = 20;
	while ((NI_RREG(BIREG_VAXBICSR) & BICSR_BROKE) && --i)
		DELAY(500000);
	if (i == 0) {
		printf("%s: BROKE bit set after reset\n", sc->sc_dev.dv_xname);
		return;
	}

	/* Check state */
	if (failtest(sc, NI_PSR, PSR_STATE, PSR_UNDEF, "not undefined state"))
		return;

	/* Clear owner bits */
	NI_WREG(NI_PSR, NI_RREG(NI_PSR) & ~PSR_OWN);
	NI_WREG(NI_PCR, NI_RREG(NI_PCR) & ~PCR_OWN);

	/* kick off init */
	NI_WREG(NI_PCR, (u_int32_t)sc->sc_pgvppqb | PCR_INIT | PCR_OWN);
	while (NI_RREG(NI_PCR) & PCR_OWN)
		DELAY(100000);

	/* Check state */
	if (failtest(sc, NI_PSR, PSR_INITED, PSR_INITED, "failed initialize"))
		return;

	NI_WREG(NI_PSR, NI_RREG(NI_PSR) & ~PSR_OWN);

	WAITREG(NI_PCR, PCR_OWN);
	NI_WREG(NI_PCR, PCR_OWN|PCR_ENABLE);
	WAITREG(NI_PCR, PCR_OWN);
	WAITREG(NI_PSR, PSR_OWN);

	/* Check state */
	if (failtest(sc, NI_PSR, PSR_STATE, PSR_ENABLED, "failed enable"))
		return;

	NI_WREG(NI_PSR, NI_RREG(NI_PSR) & ~PSR_OWN);

	/*
	 * The message queue packets must be located on the beginning
	 * of a page. A VAX page is 512 bytes, but it clusters 8 pages.
	 * This knowledge is used here when allocating pages.
	 * !!! How should this be done on MIPS and Alpha??? !!!
	 */
#if NBPG < 4096
#error pagesize too small
#endif
	s = splvm();
	/* Set up message free queue */
	ni_getpgs(sc, NMSGBUF * 512, &va, 0);
	for (i = 0; i < NMSGBUF; i++) {
		struct ni_msg *msg;

		msg = (void *)(va + i * 512);

		res = INSQTI(msg, &fqb->nf_mforw);
	}
	WAITREG(NI_PCR, PCR_OWN);
	NI_WREG(NI_PCR, PCR_FREEQNE|PCR_MFREEQ|PCR_OWN);
	WAITREG(NI_PCR, PCR_OWN);

	/* Set up xmit queue */
	ni_getpgs(sc, NTXBUF * 512, &va, 0);
	for (i = 0; i < NTXBUF; i++) {
		struct ni_dg *data;

		data = (void *)(va + i * 512);
		data->nd_status = 0;
		data->nd_len = TXADD;
		data->nd_ptdbidx = 1;
		data->nd_opcode = BVP_DGRAM;
		for (j = 0; j < NTXFRAGS; j++) {
			data->bufs[j]._offset = 0;
			data->bufs[j]._key = 1;
			bbd[i * NTXFRAGS + j].nb_key = 1;
			bbd[i * NTXFRAGS + j].nb_status = 0;
			data->bufs[j]._index = i * NTXFRAGS + j;
		}
		res = INSQTI(data, &fqb->nf_dforw);
	}
	WAITREG(NI_PCR, PCR_OWN);
	NI_WREG(NI_PCR, PCR_FREEQNE|PCR_DFREEQ|PCR_OWN);
	WAITREG(NI_PCR, PCR_OWN);

	/* recv buffers */
	ni_getpgs(sc, NRXBUF * 512, &va, 0);
	for (i = 0; i < NRXBUF; i++) {
		struct ni_dg *data;
		int idx;

		data = (void *)(va + i * 512);
		data->nd_len = RXADD;
		data->nd_opcode = BVP_DGRAMRX;
		data->nd_ptdbidx = 2;
		data->bufs[0]._key = 1;

		idx = NTXBUF * NTXFRAGS + i;
		if (ni_add_rxbuf(sc, data, idx))
			panic("niattach: ni_add_rxbuf: out of mbufs");

		res = INSQTI(data, &fqb->nf_rforw);
	}
	WAITREG(NI_PCR, PCR_OWN);
	NI_WREG(NI_PCR, PCR_FREEQNE|PCR_RFREEQ|PCR_OWN);
	WAITREG(NI_PCR, PCR_OWN);

	splx(s);

	/* Set initial parameters */
	msg = REMQHI(&fqb->nf_mforw);

	msg->nm_opcode = BVP_MSG;
	msg->nm_status = 0;
	msg->nm_len = sizeof(struct ni_param) + 6;
	msg->nm_opcode2 = NI_WPARAM;
	((struct ni_param *)&msg->nm_text[0])->np_flags = NP_PAD;

	endwait = retry = 0;
	res = INSQTI(msg, &gvp->nc_forw0);

retry:	WAITREG(NI_PCR, PCR_OWN);
	NI_WREG(NI_PCR, PCR_CMDQNE|PCR_CMDQ0|PCR_OWN);
	WAITREG(NI_PCR, PCR_OWN);
	i = 1000;
	while (endwait == 0 && --i)
		DELAY(10000);

	if (endwait == 0) {
		if (++retry < 3)
			goto retry;
		printf("%s: no response to set params\n", sc->sc_dev.dv_xname);
		return;
	}

	/* Clear counters */
	msg = REMQHI(&fqb->nf_mforw);
	msg->nm_opcode = BVP_MSG;
	msg->nm_status = 0;
	msg->nm_len = sizeof(struct ni_param) + 6;
	msg->nm_opcode2 = NI_RCCNTR;

	res = INSQTI(msg, &gvp->nc_forw0);

	WAITREG(NI_PCR, PCR_OWN);
	NI_WREG(NI_PCR, PCR_CMDQNE|PCR_CMDQ0|PCR_OWN);
	WAITREG(NI_PCR, PCR_OWN);

	/* Enable transmit logic */
	msg = REMQHI(&fqb->nf_mforw);

	msg->nm_opcode = BVP_MSG;
	msg->nm_status = 0;
	msg->nm_len = 18;
	msg->nm_opcode2 = NI_STPTDB;
	ptdb = (struct ni_ptdb *)&msg->nm_text[0];
	memset(ptdb, 0, sizeof(struct ni_ptdb));
	ptdb->np_index = 1;
	ptdb->np_fque = 1;

	res = INSQTI(msg, &gvp->nc_forw0);

	WAITREG(NI_PCR, PCR_OWN);
	NI_WREG(NI_PCR, PCR_CMDQNE|PCR_CMDQ0|PCR_OWN);
	WAITREG(NI_PCR, PCR_OWN);

	/* Wait for everything to finish */
	WAITREG(NI_PSR, PSR_OWN);

	printf("%s: address %s\n", sc->sc_dev.dv_xname,
	    ether_sprintf(sc->sc_enaddr));

	/*
	 * Attach the interface.
	 */
	if_attach(ifp);
	ether_ifattach(ifp, sc->sc_enaddr);
}

/*
 * Initialization of interface.
 */
void
niinit(sc)
	struct ni_softc *sc;
{
	struct ifnet *ifp = (struct ifnet *)&sc->sc_if;

	/*
	 * Set flags (so ni_setup() do the right thing).
	 */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Send setup messages so that the rx/tx locic starts.
	 */
	ni_setup(sc);

}

/*
 * Start output on interface.
 */
void
nistart(ifp)
	struct ifnet *ifp;
{
	struct ni_softc *sc = ifp->if_softc;
	struct ni_dg *data;
	struct ni_bbd *bdp;
	struct mbuf *m, *m0;
	int i, cnt, res, mlen;

	if (ifp->if_flags & IFF_OACTIVE)
		return;
#ifdef DEBUG
	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: nistart\n", sc->sc_dev.dv_xname);
#endif

	while (fqb->nf_dforw) {
		IFQ_POLL(&ifp->if_snd, m);
		if (m == 0)
			break;

		data = REMQHI(&fqb->nf_dforw);
		if ((int)data == Q_EMPTY) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m);

		/*
		 * Count number of mbufs in chain.
		 * Always do DMA directly from mbufs, therefore the transmit
		 * ring is really big.
		 */
		for (m0 = m, cnt = 0; m0; m0 = m0->m_next)
			if (m0->m_len)
				cnt++;
		if (cnt > NTXFRAGS)
			panic("nistart"); /* XXX */

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
		bdp = &bbd[(data->bufs[0]._index & 0x7fff)];
		for (m0 = m, i = 0, mlen = 0; m0; m0 = m0->m_next) {
			if (m0->m_len == 0)
				continue;
			bdp->nb_status = (mtod(m0, u_int32_t) & NIBD_OFFSET) |
			    NIBD_VALID;
			bdp->nb_pte = (u_int32_t)kvtopte(mtod(m0, void *));
			bdp->nb_len = m0->m_len;
			data->bufs[i]._offset = 0;
			data->bufs[i]._len = bdp->nb_len;
			data->bufs[i]._index |= NIDG_CHAIN;
			mlen += bdp->nb_len;
			bdp++;
			i++;
		}
		data->nd_opcode = BVP_DGRAM;
		data->nd_pad3 = 1;
		data->nd_ptdbidx = 1;
		data->nd_len = 10 + i * 8;
		data->bufs[i - 1]._index &= ~NIDG_CHAIN;
		if (mlen < 64)
			data->bufs[i - 1]._len = bdp[-1].nb_len += (64 - mlen);
		data->nd_cmdref = (u_int32_t)m;
#ifdef DEBUG
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: sending %d bytes (%d segments)\n",
			    sc->sc_dev.dv_xname, mlen, i);
#endif

		res = INSQTI(data, &gvp->nc_forw0);
		if (res == Q_EMPTY) {
			WAITREG(NI_PCR, PCR_OWN);
			NI_WREG(NI_PCR, PCR_CMDQNE|PCR_CMDQ0|PCR_OWN);
		}
	}
}

void
niintr(void *arg)
{
	struct ni_softc *sc = arg;
	struct ni_dg *data;
	struct ni_msg *msg;
	struct ifnet *ifp = &sc->sc_if;
	struct ni_bbd *bd;
	struct mbuf *m;
	int idx, res;

	if ((NI_RREG(NI_PSR) & PSR_STATE) != PSR_ENABLED)
		return;

	if ((NI_RREG(NI_PSR) & PSR_ERR))
		printf("%s: PSR %x\n", sc->sc_dev.dv_xname, NI_RREG(NI_PSR));

	KERNEL_LOCK();
	/* Got any response packets?  */
	while ((NI_RREG(NI_PSR) & PSR_RSQ) && (data = REMQHI(&gvp->nc_forwr))) {

		switch (data->nd_opcode) {
		case BVP_DGRAMRX: /* Receive datagram */
			idx = data->bufs[0]._index;
			bd = &bbd[idx];
			m = (void *)data->nd_cmdref;
			m->m_pkthdr.len = m->m_len =
			    data->bufs[0]._len - ETHER_CRC_LEN;
			m->m_pkthdr.rcvif = ifp;
			if (ni_add_rxbuf(sc, data, idx)) {
				bd->nb_len = (m->m_ext.ext_size - 2);
				bd->nb_pte =
				    (long)kvtopte(m->m_ext.ext_buf);
				bd->nb_status = 2 | NIBD_VALID;
				bd->nb_key = 1;
			}
			data->nd_len = RXADD;
			data->nd_status = 0;
			res = INSQTI(data, &fqb->nf_rforw);
			if (res == Q_EMPTY) {
				WAITREG(NI_PCR, PCR_OWN);
				NI_WREG(NI_PCR, PCR_FREEQNE|PCR_RFREEQ|PCR_OWN);
			}
			if (m == (void *)data->nd_cmdref)
				break; /* Out of mbufs */

#if NBPFILTER > 0
			if (ifp->if_bpf)
				bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif
			(*ifp->if_input)(ifp, m);
			break;

		case BVP_DGRAM:
			m = (struct mbuf *)data->nd_cmdref;
			ifp->if_flags &= ~IFF_OACTIVE;
			m_freem(m);
			res = INSQTI(data, &fqb->nf_dforw);
			if (res == Q_EMPTY) {
				WAITREG(NI_PCR, PCR_OWN);
				NI_WREG(NI_PCR, PCR_FREEQNE|PCR_DFREEQ|PCR_OWN);
			}
			break;

		case BVP_MSGRX:
			msg = (struct ni_msg *)data;
			switch (msg->nm_opcode2) {
				case NI_WPARAM:
					memcpy(sc->sc_enaddr, ((struct ni_param *)&msg->nm_text[0])->np_dpa, ETHER_ADDR_LEN);
					endwait = 1;
					break;

				case NI_RCCNTR:
				case NI_CLPTDB:
				case NI_STPTDB:
					break;

				default:
					printf("Unkn resp %d\n", 
					    msg->nm_opcode2);
					break;
			}
			res = INSQTI(data, &fqb->nf_mforw);
			if (res == Q_EMPTY) {
				WAITREG(NI_PCR, PCR_OWN);
				NI_WREG(NI_PCR, PCR_FREEQNE|PCR_MFREEQ|PCR_OWN);
			}
			break;

		default:
			printf("Unknown opcode %d\n", data->nd_opcode);
			res = INSQTI(data, &fqb->nf_mforw);
			if (res == Q_EMPTY) {
				WAITREG(NI_PCR, PCR_OWN);
				NI_WREG(NI_PCR, PCR_FREEQNE|PCR_MFREEQ|PCR_OWN);
			}
		}
	}

	/* Try to kick on the start routine again */
	nistart(ifp);

	NI_WREG(NI_PSR, NI_RREG(NI_PSR) & ~(PSR_OWN|PSR_RSQ));
	KERNEL_UNLOCK();
}

/*
 * Process an ioctl request.
 */
int
niioctl(ifp, cmd, data)
	register struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct ni_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch(ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			niinit(sc);
			arp_ifinit(ifp, ifa);
			break;
#endif
		}
		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running,
			 * stop it.
			 */
			ifp->if_flags &= ~IFF_RUNNING;
			ni_setup(sc);
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
			   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface it marked up and it is stopped, then
			 * start it.
			 */
			niinit(sc);
		} else if ((ifp->if_flags & IFF_UP) != 0) {
			/*
			 * Send a new setup packet to match any new changes.
			 * (Like IFF_PROMISC etc)
			 */
			ni_setup(sc);
		}
		break;

	default:
		error = ether_ioctl(ifp, &sc->sc_ec, cmd, data);
	}

	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			ni_setup(sc);
		error = 0;
	}

	splx(s);
	return (error);
}

/*
 * Add a receive buffer to the indicated descriptor.
 */
int
ni_add_rxbuf(struct ni_softc *sc, struct ni_dg *data, int idx) 
{
	struct ni_bbd *bd = &bbd[idx];
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return (ENOBUFS);
	}

	m->m_data += 2;
	bd->nb_len = (m->m_ext.ext_size - 2);
	bd->nb_pte = (long)kvtopte(m->m_ext.ext_buf);
	bd->nb_status = 2 | NIBD_VALID;
	bd->nb_key = 1;

	data->bufs[0]._offset = 0;
	data->bufs[0]._len = bd->nb_len;
	data->bufs[0]._index = idx;
	data->nd_cmdref = (long)m;

	return (0);
}

/*
 * Create setup packet and put in queue for sending.
 */
void
ni_setup(struct ni_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	struct ni_msg *msg;
	struct ni_ptdb *ptdb;
	struct ether_multi *enm;
	struct ether_multistep step;
	int i, res;

	msg = REMQHI(&fqb->nf_mforw);
	if ((int)msg == Q_EMPTY)
		return; /* What to do? */

	ptdb = (struct ni_ptdb *)&msg->nm_text[0];
	memset(ptdb, 0, sizeof(struct ni_ptdb));

	msg->nm_opcode = BVP_MSG;
	msg->nm_len = 18;
	ptdb->np_index = 2; /* definition type index */
	ptdb->np_fque = 2; /* Free queue */
	if (ifp->if_flags & IFF_RUNNING) {
		msg->nm_opcode2 = NI_STPTDB;
		ptdb->np_type = ETHERTYPE_IP;
		ptdb->np_flags = PTDB_UNKN|PTDB_BDC;
		if (ifp->if_flags & IFF_PROMISC)
			ptdb->np_flags |= PTDB_PROMISC;
		memset(ptdb->np_mcast[0], 0xff, ETHER_ADDR_LEN); /* Broadcast */
		ptdb->np_adrlen = 1;
		msg->nm_len += 8;
		ifp->if_flags &= ~IFF_ALLMULTI;
		if ((ifp->if_flags & IFF_PROMISC) == 0) {
			ETHER_FIRST_MULTI(step, &sc->sc_ec, enm);
			i = 1;
			while (enm != NULL) {
				if (memcmp(enm->enm_addrlo, enm->enm_addrhi, 6)) {
					ifp->if_flags |= IFF_ALLMULTI;
					ptdb->np_flags |= PTDB_AMC;
					break;
				}
				msg->nm_len += 8;
				ptdb->np_adrlen++;
				memcpy(ptdb->np_mcast[i++], enm->enm_addrlo,
				    ETHER_ADDR_LEN);
				ETHER_NEXT_MULTI(step, enm);
			}
		}
	} else
		msg->nm_opcode2 = NI_CLPTDB;

	res = INSQTI(msg, &gvp->nc_forw0);
	if (res == Q_EMPTY) {
		WAITREG(NI_PCR, PCR_OWN);
		NI_WREG(NI_PCR, PCR_CMDQNE|PCR_CMDQ0|PCR_OWN);
	}
}

/*
 * Check for dead transmit logic. Not uncommon.
 */
void
nitimeout(ifp)
	struct ifnet *ifp;
{
#if 0
	struct ni_softc *sc = ifp->if_softc;

	if (sc->sc_inq == 0)
		return;

	printf("%s: xmit logic died, resetting...\n", sc->sc_dev.dv_xname);
	/*
	 * Do a reset of interface, to get it going again.
	 * Will it work by just restart the transmit logic?
	 */
	niinit(sc);
#endif
}
