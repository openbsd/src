/*	$OpenBSD: ubsec.c,v 1.9 2000/06/13 05:15:19 jason Exp $	*/

/*
 * Copyright (c) 2000 Jason L. Wright (jason@thought.net)
 * Copyright (c) 2000 Theo de Raadt (deraadt@openbsd.org)
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * uBsec 5[56]01 hardware crypto accelerator
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <machine/pmap.h>
#include <sys/device.h>
#include <sys/queue.h>

#include <crypto/crypto.h>
#include <dev/rndvar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/ubsecreg.h>
#include <dev/pci/ubsecvar.h>

/*
 * Prototypes and count for the pci_device structure
 */
int ubsec_probe		__P((struct device *, void *, void *));
void ubsec_attach	__P((struct device *, struct device *, void *));

struct cfattach ubsec_ca = {
	sizeof(struct ubsec_softc), ubsec_probe, ubsec_attach,
};

struct cfdriver ubsec_cd = {
	0, "ubsec", DV_DULL
};

int	ubsec_intr __P((void *));
int	ubsec_newsession __P((u_int32_t *, struct cryptoini *));
int	ubsec_freesession __P((u_int64_t));
int	ubsec_process __P((struct cryptop *));
void	ubsec_callback __P((struct ubsec_softc *, struct ubsec_q *, u_int8_t *));
int	ubsec_feed __P((struct ubsec_softc *));

#define	READ_REG(sc,r) \
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (r))

#define WRITE_REG(sc,reg,val) \
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, reg, val)

int
ubsec_probe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_BLUESTEEL &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BLUESTEEL_5501)
		return (1);
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_BLUESTEEL &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BLUESTEEL_5601)
		return (1);
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_BROADCOM &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_5805)
		return (1);
	return (0);
}

void 
ubsec_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ubsec_softc *sc = (struct ubsec_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_addr_t iobase;
	bus_size_t iosize;
	u_int32_t cmd;

	SIMPLEQ_INIT(&sc->sc_queue);
	SIMPLEQ_INIT(&sc->sc_qchip);
	sc->sc_intrmask = BS_CTRL_MCR1INT | BS_CTRL_DMAERR;

	if ((PCI_VENDOR(pa->pa_id) == PCI_VENDOR_BLUESTEEL &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BLUESTEEL_5601) ||
	    (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_BROADCOM &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_5805)) {
		sc->sc_intrmask |= BS_CTRL_MCR2INT;
		sc->sc_5601 = 1;
	}

	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	cmd |= PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, cmd);
	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

	if (!(cmd & PCI_COMMAND_MEM_ENABLE)) {
		printf(": failed to enable memory mapping\n");
		return;
	}

	if (pci_mem_find(pc, pa->pa_tag, BS_BAR, &iobase, &iosize, NULL)) {
		printf(": can't find mem space\n");
		return;
	}
	if (bus_space_map(pa->pa_memt, iobase, iosize, 0, &sc->sc_sh)) {
		printf(": can't map mem space\n");
		return;
	}
	sc->sc_st = pa->pa_memt;

	sc->sc_dmat = pa->pa_dmat;

	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf(": couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, ubsec_intr, sc,
	    self->dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt\n");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}

	sc->sc_cid = crypto_get_driverid();
	if (sc->sc_cid < 0)
		return;

	crypto_register(sc->sc_cid, CRYPTO_3DES_CBC,
	    ubsec_newsession, ubsec_freesession, ubsec_process);
	crypto_register(sc->sc_cid, CRYPTO_DES_CBC, NULL, NULL, NULL);
	crypto_register(sc->sc_cid, CRYPTO_MD5_HMAC96, NULL, NULL, NULL);
	crypto_register(sc->sc_cid, CRYPTO_SHA1_HMAC96, NULL, NULL, NULL);

	WRITE_REG(sc, BS_CTRL, BS_CTRL_MCR1INT | BS_CTRL_DMAERR);

	printf(": %s\n", intrstr);
}

int 
ubsec_intr(arg)
	void *arg;
{
	struct ubsec_softc *sc = arg;
	volatile u_int32_t stat, a;
	struct ubsec_q *q;
	int npkts = 0;

	stat = READ_REG(sc, BS_STAT);

	stat &= (BS_STAT_MCR1_DONE | BS_STAT_MCR2_DONE | BS_STAT_DMAERR);
	if (stat == 0)
		return (0);

	WRITE_REG(sc, BS_STAT, stat);		/* IACK */

	if (stat & BS_STAT_MCR1_DONE) {
		while (!SIMPLEQ_EMPTY(&sc->sc_qchip)) {
			q = SIMPLEQ_FIRST(&sc->sc_qchip);
			if ((q->q_mcr.mcr_flags & UBS_MCR_DONE) == 0)
				break;
			npkts++;
			SIMPLEQ_REMOVE_HEAD(&sc->sc_qchip, q, q_next);
#ifdef UBSEC_DEBUG
			printf("intr: callback q %08x flags %04x\n", q,
			    q->q_mcr.mcr_flags);
#endif
			ubsec_callback(sc, q, NULL);
		}
#ifdef UBSEC_DEBUG
		if (npkts > 1)
			printf("intr: %d pkts\n", npkts);
#endif
	}

	if (stat & BS_STAT_DMAERR) {
		a = READ_REG(sc, BS_ERR);
		printf("%s: dmaerr %s@%08x\n", sc->sc_dv.dv_xname,
		       (a & BS_ERR_READ) ? "read" : "write",
		       a & ~BS_ERR_READ);
	}

	ubsec_feed(sc);
	return (1);
}

int
ubsec_feed(sc)
	struct ubsec_softc *sc;
{
	struct ubsec_q *q;

	while (!SIMPLEQ_EMPTY(&sc->sc_queue)) {
		if (READ_REG(sc, BS_STAT) & BS_STAT_MCR1_FULL)
			break;
		q = SIMPLEQ_FIRST(&sc->sc_queue);
		WRITE_REG(sc, BS_MCR1, (u_int32_t)vtophys(&q->q_mcr));
#ifdef UBSEC_DEBUG
		printf("feed: q->chip %08x %08x\n", q, (u_int32_t)vtophys(&q->q_mcr));
#endif
		SIMPLEQ_REMOVE_HEAD(&sc->sc_queue, q, q_next);
		--sc->sc_nqueue;
		SIMPLEQ_INSERT_TAIL(&sc->sc_qchip, q, q_next);
	}
	return (0);
}

/*
 * Allocate a new 'session' and return an encoded session id.  'sidp'
 * contains our registration id, and should contain an encoded session
 * id on successful allocation.
 * XXX No allocation actually done here, all sessions are the same.
 */
int
ubsec_newsession(sidp, cri)
	u_int32_t *sidp;
	struct cryptoini *cri;
{
	struct cryptoini *c;
	struct ubsec_softc *sc = NULL;
	char mac = 0, cry = 0;
	int i;

	if (sidp == NULL || cri == NULL)
		return (EINVAL);

	for (i = 0; i < ubsec_cd.cd_ndevs; i++) {
		sc = ubsec_cd.cd_devs[i];
		if (sc == NULL || sc->sc_cid == (*sidp))
			break;
	}
	if (sc == NULL)
		return (EINVAL);

	for (c = cri; c != NULL; c = c->cri_next) {
		if (c->cri_alg == CRYPTO_MD5_HMAC96 ||
		    c->cri_alg == CRYPTO_SHA1_HMAC96) {
			if (mac)
				return (EINVAL);
			mac = 1;
		} else if (c->cri_alg == CRYPTO_DES_CBC ||
		    c->cri_alg == CRYPTO_3DES_CBC) {
			if (cry)
				return (EINVAL);
			cry = 1;
		} else
			return (EINVAL);
	}
	if (mac == 0 && cry == 0)
		return (EINVAL);

	*sidp = UBSEC_SID(sc->sc_dv.dv_unit, 0);

	return (0);
}

/*
 * Deallocate a session.
 * XXX Nothing to do yet.
 */
int
ubsec_freesession(sid)
	u_int64_t sid;
{
	return (0);
}

int
ubsec_process(crp)
	struct cryptop *crp;
{
	struct ubsec_q *q;
	int card, err, i, j, s;
	struct ubsec_softc *sc;
	struct cryptodesc *crd1, *crd2, *maccrd, *enccrd;
	int encoffset = 0, macoffset = 0, sskip, dskip;
	int16_t coffset;

	if (crp == NULL || crp->crp_callback == NULL)
		return (EINVAL);

	card = UBSEC_CARD(crp->crp_sid);
	if (card >= ubsec_cd.cd_ndevs || ubsec_cd.cd_devs[card] == NULL) {
		err = EINVAL;
		goto errout;
	}

	sc = ubsec_cd.cd_devs[card];

	s = splnet();
	if (sc->sc_nqueue == UBS_MAX_NQUEUE) {
		splx(s);
		err = ENOMEM;
		goto errout;
	}
	splx(s);

	q = (struct ubsec_q *)malloc(sizeof(struct ubsec_q),
	    M_DEVBUF, M_NOWAIT);
	if (q == NULL) {
		err = ENOMEM;
		goto errout;
	}
	bzero(q, sizeof(struct ubsec_q));

	q->q_mcr.mcr_pkts = 1;
	q->q_mcr.mcr_flags = 0;
	q->q_mcr.mcr_cmdctxp = vtophys(&q->q_ctx);
	q->q_sc = sc;
	q->q_crp = crp;

	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		q->q_src_m = (struct mbuf *)crp->crp_buf;
		q->q_dst_m = (struct mbuf *)crp->crp_buf;
	} else {
		err = EINVAL;
		goto errout;	/* XXX only handle mbufs right now */
	}

	crd1 = crp->crp_desc;
	if (crd1 == NULL) {
		err = EINVAL;
		goto errout;
	}
	crd2 = crd1->crd_next;

	if (crd2 == NULL) {
		if (crd1->crd_alg == CRYPTO_MD5_HMAC96 ||
		    crd1->crd_alg == CRYPTO_SHA1_HMAC96) {
			maccrd = crd1;
			enccrd = NULL;
		} else if (crd1->crd_alg == CRYPTO_DES_CBC ||
			 crd1->crd_alg == CRYPTO_3DES_CBC) {
			maccrd = NULL;
			enccrd = crd1;
		} else {
			err = EINVAL;
			goto errout;
		}
	} else {
		if ((crd1->crd_alg == CRYPTO_MD5_HMAC96 ||
		    crd1->crd_alg == CRYPTO_SHA1_HMAC96) &&
		    (crd2->crd_alg == CRYPTO_DES_CBC ||
			crd2->crd_alg == CRYPTO_3DES_CBC) &&
		    ((crd2->crd_flags & CRD_F_ENCRYPT) == 0)) {
			maccrd = crd1;
			enccrd = crd2;
		} else if ((crd1->crd_alg == CRYPTO_DES_CBC ||
		    crd1->crd_alg == CRYPTO_3DES_CBC) &&
		    (crd2->crd_alg == CRYPTO_MD5_HMAC96 ||
			crd2->crd_alg == CRYPTO_SHA1_HMAC96) &&
		    (crd1->crd_flags & CRD_F_ENCRYPT)) {
			enccrd = crd1;
			maccrd = crd2;
		} else {
			/*
			 * We cannot order the ubsec as requested
			 */
			err = EINVAL;
			goto errout;
		}
	}

	if (enccrd) {
		encoffset = enccrd->crd_skip;
		q->q_ctx.pc_flags |= UBS_PKTCTX_ENC_3DES;

		if (enccrd->crd_flags & CRD_F_ENCRYPT) {
			if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
				bcopy(enccrd->crd_iv, q->q_ctx.pc_iv,
				    sizeof(q->q_ctx.pc_iv));
			else
				get_random_bytes(q->q_ctx.pc_iv,
				    sizeof(q->q_ctx.pc_iv));

			if ((enccrd->crd_flags & CRD_F_IV_PRESENT) == 0)
				m_copyback(q->q_src_m, enccrd->crd_inject,
				    sizeof(q->q_ctx.pc_iv), q->q_ctx.pc_iv);
		} else {
			q->q_ctx.pc_flags |= UBS_PKTCTX_INBOUND;

			if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
				bcopy(enccrd->crd_iv, q->q_ctx.pc_iv,
				    sizeof(q->q_ctx.pc_iv));
			else
				m_copydata(q->q_src_m, enccrd->crd_inject,
				    sizeof(q->q_ctx.pc_iv), q->q_ctx.pc_iv);
		}

		if (enccrd->crd_alg == CRYPTO_DES_CBC) {
			/* Cheat: des == 3des with two of the keys the same */
			bcopy(enccrd->crd_key, &q->q_ctx.pc_deskey[0], 8);
			bcopy(enccrd->crd_key, &q->q_ctx.pc_deskey[8], 8);
			bcopy(enccrd->crd_key, &q->q_ctx.pc_deskey[16], 8);
		} else
			bcopy(enccrd->crd_key, &q->q_ctx.pc_deskey[0], 24);

	}

	if (maccrd) {
		macoffset = maccrd->crd_skip;
		if (maccrd->crd_alg == CRYPTO_MD5_HMAC96)
			q->q_ctx.pc_flags |= UBS_PKTCTX_AUTH_MD5;
		else
			q->q_ctx.pc_flags |= UBS_PKTCTX_AUTH_SHA1;

		/* XXX not right */
		bcopy(maccrd->crd_key, &q->q_ctx.pc_hminner[0],
		    maccrd->crd_klen >> 3);

	}

	if (enccrd && maccrd) {
		dskip = sskip = (macoffset > encoffset) ? encoffset : macoffset;
		coffset = macoffset - encoffset;
		if (coffset < 0)
			coffset = -coffset;
	} else {
		dskip = sskip = macoffset + encoffset;
		coffset = 0;
	}
	q->q_ctx.pc_flags |= (coffset << 16);

	q->q_src_l = mbuf2pages(q->q_src_m, &q->q_src_npa, q->q_src_packp,
	    q->q_src_packl, MAX_SCATTER, &err);
	if (q->q_src_l == 0) {
		err = ENOMEM;
		goto errout;
	}

	if (err == 0) {
		int totlen, len;
		struct mbuf *m, *top, **mp;

		totlen = q->q_dst_l = q->q_src_l;
		if (q->q_src_m->m_flags & M_PKTHDR) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			M_COPY_PKTHDR(m, q->q_src_m);
			len = MHLEN;
		} else {
			MGET(m, M_DONTWAIT, MT_DATA);
			len = MLEN;
		}
		if (m == NULL) {
			err = ENOMEM;
			goto errout;
		}
		if (totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				len = MCLBYTES;
		}
		m->m_len = len;
		top = NULL;
		mp = &top;

		while (totlen > 0) {
			if (top) {
				MGET(m, M_DONTWAIT, MT_DATA);
				if (m == NULL) {
					m_freem(top);
					err = ENOMEM;
					goto errout;
				}
				len = MLEN;
			}
			if (top && totlen >= MINCLSIZE) {
				MCLGET(m, M_DONTWAIT);
				if (m->m_flags & M_EXT)
					len = MCLBYTES;
			}
			m->m_len = len = min(totlen, len);
			totlen -= len;
			*mp = m;
			mp = &m->m_next;
		}
		q->q_dst_m = top;
	} else
		q->q_dst_m = q->q_src_m;

	q->q_dst_l = mbuf2pages(q->q_dst_m, &q->q_dst_npa, q->q_dst_packp,
	    q->q_dst_packl, MAX_SCATTER, NULL);

	q->q_mcr.mcr_pktlen = q->q_dst_l - sskip;

#ifdef UBSEC_DEBUG
	printf("src skip: %d\n", sskip);
#endif
	for (i = j = 0; i < q->q_src_npa; i++) {
		struct ubsec_pktbuf *pb;

#ifdef UBSEC_DEBUG
		printf("  src[%d->%d]: %d@%x\n", i, j,
		    q->q_src_packl[i], q->q_src_packp[i]);
#endif
		if (sskip) {
			if (sskip >= q->q_src_packl[i]) {
				sskip -= q->q_src_packl[i];
				continue;
			}
			q->q_src_packp[i] += sskip;
			q->q_src_packl[i] -= sskip;
			sskip = 0;
		}

		if (j == 0)
			pb = &q->q_mcr.mcr_ipktbuf;
		else
			pb = &q->q_srcpkt[j - 1];

#ifdef UBSEC_DEBUG
		printf("  pb v %08x p %08x\n", pb, vtophys(pb));
#endif
		pb->pb_addr = q->q_src_packp[i];
		pb->pb_len = q->q_src_packl[i];

		if ((i + 1) == q->q_src_npa)
			pb->pb_next = 0;
		else
			pb->pb_next = vtophys(&q->q_srcpkt[j]);
		j++;
	}
#ifdef UBSEC_DEBUG
	printf("  buf[%x]: %d@%x -> %x\n", vtophys(&q->q_mcr),
	    q->q_mcr.mcr_ipktbuf.pb_len,
	    q->q_mcr.mcr_ipktbuf.pb_addr,
	    q->q_mcr.mcr_ipktbuf.pb_next);
	for (i = 0; i < j - 1; i++) {
		printf("  buf[%x]: %d@%x -> %x\n", vtophys(&q->q_srcpkt[i]),
		    q->q_srcpkt[i].pb_len,
		    q->q_srcpkt[i].pb_addr,
		    q->q_srcpkt[i].pb_next);
	}
#endif

#ifdef UBSEC_DEBUG
	printf("dst skip: %d\n", dskip);
#endif
	for (i = j = 0; i < q->q_dst_npa; i++) {
		struct ubsec_pktbuf *pb;

#ifdef UBSEC_DEBUG
		printf("  dst[%d->%d]: %d@%x\n", i, j,
		    q->q_dst_packl[i], q->q_dst_packp[i]);
#endif
		if (dskip) {
			if (dskip >= q->q_dst_packl[i]) {
				dskip -= q->q_dst_packl[i];
				continue;
			}
			q->q_dst_packp[i] += dskip;
			q->q_dst_packl[i] -= dskip;
			dskip = 0;
		}

		if (j == 0)
			pb = &q->q_mcr.mcr_opktbuf;
		else
			pb = &q->q_dstpkt[j - 1];

#ifdef UBSEC_DEBUG
		printf("  pb v %08x p %08x\n", pb, vtophys(pb));
#endif
		pb->pb_addr = q->q_dst_packp[i];
		pb->pb_len = q->q_dst_packl[i];

		if ((i + 1) == q->q_dst_npa)
			pb->pb_next = 0;
		else
			pb->pb_next = vtophys(&q->q_dstpkt[j]);
		j++;
	}
#ifdef UBSEC_DEBUG
	printf("  buf[%d, %x]: %d@%x -> %x\n", 0,
	    vtophys(&q->q_mcr),
	    q->q_mcr.mcr_opktbuf.pb_len,
	    q->q_mcr.mcr_opktbuf.pb_addr,
	    q->q_mcr.mcr_opktbuf.pb_next);
	for (i = 0; i < j - 1; i++) {
		printf("  buf[%d, %x]: %d@%x -> %x\n", i+1,
		    vtophys(&q->q_dstpkt[i]),
		    q->q_dstpkt[i].pb_len,
		    q->q_dstpkt[i].pb_addr,
		    q->q_dstpkt[i].pb_next);
	}
#endif

	s = splnet();
	SIMPLEQ_INSERT_TAIL(&sc->sc_queue, q, q_next);
	sc->sc_nqueue++;
	ubsec_feed(sc);
	splx(s);
	return (0);

errout:
	if (q != NULL) {
		if (q->q_src_m != q->q_dst_m)
			m_freem(q->q_dst_m);
		free(q, M_DEVBUF);
	}
	crp->crp_etype = err;
	crp->crp_callback(crp);
	return (0);
}

void
ubsec_callback(sc, q, macbuf)
	struct ubsec_softc *sc;
	struct ubsec_q *q;
	u_int8_t *macbuf;
{
	struct cryptop *crp = (struct cryptop *)q->q_crp;
	struct cryptodesc *crd;

	if ((crp->crp_flags & CRYPTO_F_IMBUF) && (q->q_src_m != q->q_dst_m)) {
		m_freem(q->q_src_m);
		crp->crp_buf = (caddr_t)q->q_dst_m;
	}

	if (macbuf != NULL) {
		printf("copying macbuf\n");
		for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
			if (crd->crd_alg != CRYPTO_MD5_HMAC96 &&
			    crd->crd_alg != CRYPTO_SHA1_HMAC96)
				continue;
			m_copyback((struct mbuf *)crp->crp_buf,
			    crd->crd_inject, 12, macbuf);
			break;
		}
	}

	free(q, M_DEVBUF);
	crp->crp_callback(crp);
}
