/*	$OpenBSD: ubsec.c,v 1.2 2000/06/02 22:42:08 deraadt Exp $	*/

/*
 * Copyright (c) 2000 Jason L. Wright (jason@thought.net)
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

/*
 * Register definitions for 5601 BlueSteel Networks Ubiquitous Broadband
 * Security "uBSec" chip.  Definitions from revision 2.8 of the product
 * datasheet.
 */

#define BS_BAR		0x10	/* DMA and status base address register */

/*
 * DMA Control & Status Registers (offset from BS_BAR)
 */
#define	BS_MCR1		0x00	/* DMA Master Command Record 1 */
#define	BS_CTRL		0x04	/* DMA Control */
#define	BS_STAT		0x08	/* DMA Status */
#define	BS_ERR		0x0c	/* DMA Error Address */
#define	BS_MCR2		0x10	/* DMA Master Command Record 2 */

/* BS_CTRL - DMA Control */
#define	BS_CTRL_MCR2INT		0x40000000	/* enable intr MCR for MCR2 */
#define	BS_CTRL_MCR1INT		0x20000000	/* enable intr MCR for MCR1 */
#define	BS_CTRL_OFM		0x10000000	/* Output fragment mode */
#define	BS_CTRL_BE32		0x08000000	/* big-endian, 32bit bytes */
#define	BS_CTRL_BE64		0x04000000	/* big-endian, 64bit bytes */
#define	BS_CTRL_DMAERR		0x02000000	/* enable intr DMA error */
#define	BS_CTRL_RNG_M		0x01800000	/* RND mode */
#define	BS_CTRL_RNG_1		0x00000000	/* 1bit rn/one slow clock */
#define	BS_CTRL_RNG_4		0x00800000	/* 1bit rn/four slow clocks */
#define	BS_CTRL_RNG_8		0x01000000	/* 1bit rn/eight slow clocks */
#define	BS_CTRL_RNG_16		0x01800000	/* 1bit rn/16 slow clocks */
#define	BS_CTRL_FRAG_M		0x0000ffff	/* output fragment size mask */

/* BS_STAT - DMA Status */
#define	BS_STAT_MCR1_BUSY	0x80000000	/* MCR1 is busy */
#define	BS_STAT_MCR1_FULL	0x40000000	/* MCR1 is full */
#define	BS_STAT_MCR1_DONE	0x20000000	/* MCR1 is done */
#define	BS_STAT_DMAERR		0x10000000	/* DMA error */
#define	BS_STAT_MCR2_FULL	0x08000000	/* MCR2 is full */
#define	BS_STAT_MCR2_DONE	0x04000000	/* MCR2 is done */

/* BS_ERR - DMA Error Address */
#define	BS_ERR_READ		0x00000001	/* fault was on read */

#define UBSEC_CARD(sid)		(((sid) & 0xf0000000) >> 28)
#define UBSEC_SID(crd,ses)	(((crd) << 28) | ((ses) & 0x7ff))
#define	MAX_SCATTER		10

struct ubsec_pktctx {
	u_int8_t	pc_deskey[24];		/* 3DES key */
	u_int8_t	pc_hminner[20];		/* hmac inner state */
	u_int8_t	pc_hmouter[20];		/* hmac outer state */
	u_int8_t	pc_iv[8];		/* 3DES iv */
	u_int32_t	pc_flags;
};
#define	UBS_PKTCTX_COFFSET	0xffff0000	/* cryto to mac offset */
#define	UBS_PKTCTX_ENC_3DES	0x00008000	/* use 3des */
#define	UBS_PKTCTX_ENC_NONE	0x00000000	/* no encryption */
#define	UBS_PKTCTX_INBOUND	0x00004000	/* inbound packet */
#define	UBS_PKTCTX_AUTH		0x00003000	/* authentication mask */
#define	UBS_PKTCTX_AUTH_NONE	0x00000000	/* no authentication */
#define	UBS_PKTCTX_AUTH_MD5	0x00001000	/* use hmac-md5 */
#define	UBS_PKTCTX_AUTH_SHA1	0x00002000	/* use hmac-sha1 */

struct ubsec_pktbuf {
	u_int32_t	pb_addr;		/* address of buffer start */
	u_int32_t	pb_next;		/* pointer to next pktbuf */
	u_int32_t	pb_len;
};
#define	UBS_PKTBUF_LEN		0x0000ffff	/* length mask */

struct ubsec_mcr {
	u_int32_t		mcr_flags;	/* flags/packet count */

	u_int32_t		mcr_cmdctxp;	/* command ctx pointer */
	struct ubsec_pktbuf	mcr_ipktbuf;	/* input chain header */
	struct ubsec_pktbuf	mcr_opktbuf;	/* output chain header */
};
#define	UBS_MCR_PACKETS		0x0000ffff	/* packets in this mcr */
#define	UBS_MCR_DONE		0x00010000	/* mcr has been processed */
#define	UBS_MCR_ERROR		0x00020000	/* error in processing */
#define	UBS_MCR_ERRORCODE	0xff000000	/* error type */

struct ubsec_q {
	SIMPLEQ_ENTRY(ubsec_q)		q_next;
	struct ubsec_softc		*q_sc;
	struct cryptop			*q_crp;
	struct ubsec_mcr		q_mcr;
	struct ubsec_pktctx		q_ctx;

	struct mbuf *		      	q_src_m;
	long				q_src_packp[MAX_SCATTER];
	int				q_src_packl[MAX_SCATTER];
	int				q_src_npa, q_src_l;
	struct ubsec_pktbuf		q_srcpkt[MAX_SCATTER-1];

	struct mbuf *			q_dst_m;
	long				q_dst_packp[MAX_SCATTER];
	int				q_dst_packl[MAX_SCATTER];
	int				q_dst_npa, q_dst_l;
	struct ubsec_pktbuf		q_dstpkt[MAX_SCATTER-1];
};

struct ubsec_softc {
	struct	device		sc_dv;		/* generic device */
	void			*sc_ih;		/* interrupt handler cookie */
	bus_space_handle_t	sc_sh;		/* memory handle */
	bus_space_tag_t		sc_st;		/* memory tag */
	bus_dma_tag_t		sc_dmat;	/* dma tag */
	int			sc_5601;	/* device is 5601 */
	int32_t			sc_cid;		/* crypto tag */
	u_int32_t		sc_intrmask;	/* interrupt mask */
	SIMPLEQ_HEAD(,ubsec_q)	sc_queue;	/* packet queue */
	int			sc_nqueue;	/* count enqueued */
	SIMPLEQ_HEAD(,ubsec_q)	sc_qchip;	/* on chip */
};

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
int	ubsec_crypto __P((struct ubsec_softc *, struct ubsec_q *q));

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

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_BLUESTEEL &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BLUESTEEL_5601) {
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

	printf(": %s\n", intrstr);
}

#define	READ_REG(sc,r) \
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (r))

#define WRITE_REG(sc,reg,val) \
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, reg, val)

int 
ubsec_intr(arg)
	void *arg;
{
	struct ubsec_softc *sc = arg;
	u_int32_t stat;
	struct ubsec_q *q;

	stat = READ_REG(sc, BS_STAT);

	if (stat & BS_STAT_MCR1_DONE) {
		SIMPLEQ_REMOVE_HEAD(&sc->sc_qchip, q, q_next);
		if (q) {
			/* XXX must generate macbuf ... */
			ubsec_callback(sc, q, NULL);
		}
	}

	if (stat & BS_STAT_DMAERR) {
		printf("%s: dmaerr\n", sc->sc_dv.dv_xname);
	}

	/* if MCR is non-full, put a new command in it */
	if ((stat & BS_STAT_MCR1_FULL) == 0) {
		SIMPLEQ_REMOVE_HEAD(&sc->sc_queue, q, q_next);
		--sc->sc_nqueue;
		if (q) {
			SIMPLEQ_INSERT_TAIL(&sc->sc_qchip, q, q_next);
			WRITE_REG(sc, BS_MCR1, (u_int32_t)&q->q_mcr);
		}
	}

	return (stat ? 1 : 0);
}

int
ubsec_crypto(sc, q)
	struct ubsec_softc *sc;
	struct ubsec_q *q;
{
	int s;
	u_int32_t stat;

	s = splnet();
	stat = READ_REG(sc, BS_STAT);
	if ((stat & BS_STAT_MCR1_FULL) == 0) {
		WRITE_REG(sc, BS_MCR1, (u_int32_t)&q->q_mcr);
		SIMPLEQ_INSERT_TAIL(&sc->sc_qchip, q, q_next);
	} else {
		SIMPLEQ_INSERT_TAIL(&sc->sc_queue, q, q_next);
		sc->sc_nqueue++;
	}
	splx(s);
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
	int card, err, i;
	struct ubsec_softc *sc;
	struct cryptodesc *crd1, *crd2, *maccrd, *enccrd;

	if (crp == NULL || crp->crp_callback == NULL)
		return (EINVAL);

	card = UBSEC_CARD(crp->crp_sid);
	if (card >= ubsec_cd.cd_ndevs || ubsec_cd.cd_devs[card] == NULL) {
		err = EINVAL;
		goto errout;
	}

	sc = ubsec_cd.cd_devs[card];

	q = (struct ubsec_q *)malloc(sizeof(struct ubsec_q),
	    M_DEVBUF, M_NOWAIT);
	if (q == NULL) {
		err = ENOMEM;
		goto errout;
	}
	bzero(q, sizeof(struct ubsec_q));

	q->q_mcr.mcr_flags = 1 & UBS_MCR_PACKETS;
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
		if (maccrd->crd_alg == CRYPTO_MD5_HMAC96)
			q->q_ctx.pc_flags |= UBS_PKTCTX_AUTH_MD5;
		else
			q->q_ctx.pc_flags |= UBS_PKTCTX_AUTH_SHA1;

		/* XXX not right */
		bcopy(maccrd->crd_key, &q->q_ctx.pc_hminner[0],
		    maccrd->crd_klen >> 3);

	}

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
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			err = ENOMEM;
			goto errout;
		}
		len = MHLEN;
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

#if 0
	/* XXX time to incorporate the information in crd_skip... */

	cmd->crypt_header_skip = enccrd->crd_skip;
	cmd->crypt_process_len = enccrd->crd_len;
	cmd->mac_header_skip = maccrd->crd_skip;
	cmd->mac_process_len = maccrd->crd_len;
#endif
	for (i = 0; i < q->q_src_npa; i++) {
		struct ubsec_pktbuf *pb;

		if (i == 0)
			pb = &q->q_mcr.mcr_ipktbuf;
		else
			pb = &q->q_srcpkt[i - 1];

		pb->pb_addr = q->q_src_packp[i];
		pb->pb_len = q->q_src_packl[i];

		if ((i + 1) == q->q_src_npa)
			pb->pb_next = 0;
		else
			pb->pb_next = vtophys(&q->q_srcpkt[i]);
	}

	for (i = 0; i < q->q_dst_npa; i++) {
		struct ubsec_pktbuf *pb;

		if (i == 0)
			pb = &q->q_mcr.mcr_opktbuf;
		else
			pb = &q->q_dstpkt[i - 1];

		pb->pb_addr = q->q_dst_packp[i];
		pb->pb_len = q->q_dst_packl[i];

		if ((i + 1) == q->q_dst_npa)
			pb->pb_next = 0;
		else
			pb->pb_next = vtophys(&q->q_dstpkt[i]);
	}

	/* queues it, or sends it to the chip */
	ubsec_crypto(sc, q);
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
