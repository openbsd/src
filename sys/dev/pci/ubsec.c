/*	$OpenBSD: ubsec.c,v 1.82 2002/01/28 17:10:11 jason Exp $	*/

/*
 * Copyright (c) 2000 Jason L. Wright (jason@thought.net)
 * Copyright (c) 2000 Theo de Raadt (deraadt@openbsd.org)
 * Copyright (c) 2001 Patrik Lindergren (patrik@ipunplugged.com)
 * 
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

#undef UBSEC_DEBUG

/*
 * uBsec 5[56]01, 58xx hardware crypto accelerator
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/device.h>
#include <sys/queue.h>

#include <uvm/uvm_extern.h>

#include <crypto/cryptodev.h>
#include <crypto/cryptosoft.h>
#include <dev/rndvar.h>
#include <sys/md5k.h>
#include <crypto/sha1.h>

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
void ubsec_reset_board	__P((struct ubsec_softc *));
void ubsec_init_board	__P((struct ubsec_softc *));
void ubsec_init_pciregs	__P((struct pci_attach_args *pa));
void ubsec_cleanchip	__P((struct ubsec_softc *));
void ubsec_totalreset	__P((struct ubsec_softc *));
int  ubsec_free_q	__P((struct ubsec_softc*, struct ubsec_q *));

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
void	ubsec_callback __P((struct ubsec_softc *, struct ubsec_q *));
int	ubsec_feed __P((struct ubsec_softc *));
void	ubsec_mcopy __P((struct mbuf *, struct mbuf *, int, int));
void	ubsec_callback2 __P((struct ubsec_softc *, struct ubsec_q2 *));
int	ubsec_feed2 __P((struct ubsec_softc *));
void	ubsec_rng __P((void *));
int	ubsec_dma_malloc __P((struct ubsec_softc *, bus_size_t,
    struct ubsec_dma_alloc *, int));
void	ubsec_dma_free __P((struct ubsec_softc *, struct ubsec_dma_alloc *));
int	ubsec_dmamap_aligned __P((bus_dmamap_t));

#define	READ_REG(sc,r) \
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (r))

#define WRITE_REG(sc,reg,val) \
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, reg, val)

#define	SWAP32(x) (x) = htole32(ntohl((x)))
#define	HTOLE32(x) (x) = htole32(x)


struct ubsec_stats ubsecstats;

int
ubsec_probe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_BLUESTEEL &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BLUESTEEL_5501 ||
	     PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BLUESTEEL_5601))
		return (1);
	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_BROADCOM &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_5805 ||
	     PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_5820 ||
	     PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_5821))
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
	struct ubsec_dma *dmap;
	bus_size_t iosize;
	u_int32_t cmd, i;

	SIMPLEQ_INIT(&sc->sc_queue);
	SIMPLEQ_INIT(&sc->sc_qchip);
	SIMPLEQ_INIT(&sc->sc_queue2);
	SIMPLEQ_INIT(&sc->sc_qchip2);

	sc->sc_statmask = BS_STAT_MCR1_DONE | BS_STAT_DMAERR;
	if ((PCI_VENDOR(pa->pa_id) == PCI_VENDOR_BLUESTEEL &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BLUESTEEL_5601) ||
	    (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_BROADCOM &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_5805))
		sc->sc_flags |= UBS_FLAGS_KEY;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_BROADCOM &&
	    (PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_5820 ||
	     PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_BROADCOM_5821))
		sc->sc_flags |= UBS_FLAGS_KEY | UBS_FLAGS_LONGCTX;

	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	cmd |= PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, cmd);
	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

	if (!(cmd & PCI_COMMAND_MEM_ENABLE)) {
		printf(": failed to enable memory mapping\n");
		return;
	}

	if (!(cmd & PCI_COMMAND_MASTER_ENABLE)) {
		printf(": failed to enable bus mastering\n");
		return;
	}

	if (pci_mapreg_map(pa, BS_BAR, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_st, &sc->sc_sh, NULL, &iosize, 0)) {
		printf(": can't find mem space\n");
		return;
	}
	sc->sc_dmat = pa->pa_dmat;

	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		bus_space_unmap(sc->sc_st, sc->sc_sh, iosize);
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, ubsec_intr, sc,
	    self->dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		bus_space_unmap(sc->sc_st, sc->sc_sh, iosize);
		return;
	}

	sc->sc_cid = crypto_get_driverid(0);
	if (sc->sc_cid < 0) {
		pci_intr_disestablish(pc, sc->sc_ih);
		bus_space_unmap(sc->sc_st, sc->sc_sh, iosize);
		return;
	}

	SIMPLEQ_INIT(&sc->sc_freequeue);
	dmap = sc->sc_dmaa;
	for (i = 0; i < UBS_MAX_NQUEUE; i++, dmap++) {
		struct ubsec_q *q;

		q = (struct ubsec_q *)malloc(sizeof(struct ubsec_q),
		    M_DEVBUF, M_NOWAIT);
		if (q == NULL) {
			printf("Warning can't allocate queue buffers for ubsec\n");
			break;
		}

		if (ubsec_dma_malloc(sc, sizeof(struct ubsec_dmachunk),
		    &dmap->d_alloc, 0)) {
			printf("Warning can't allocate dma buffers for ubsec\n");
			free(q, M_DEVBUF);
			break;
		}
		dmap->d_dma = (struct ubsec_dmachunk *)dmap->d_alloc.dma_vaddr;

		q->q_dma = dmap;
		sc->sc_queuea[i] = q;

		SIMPLEQ_INSERT_TAIL(&sc->sc_freequeue, q, q_next);
	}

	crypto_register(sc->sc_cid, CRYPTO_3DES_CBC, 0, 0,
	    ubsec_newsession, ubsec_freesession, ubsec_process);
	crypto_register(sc->sc_cid, CRYPTO_DES_CBC, 0, 0, NULL, NULL, NULL);
	crypto_register(sc->sc_cid, CRYPTO_MD5_HMAC, 0, 0, NULL, NULL, NULL);
	crypto_register(sc->sc_cid, CRYPTO_SHA1_HMAC, 0, 0, NULL, NULL, NULL);

	/*
	 * Reset Broadcom chip
	 */
	ubsec_reset_board(sc);

	/*
	 * Init Broadcom specific PCI settings
	 */
	ubsec_init_pciregs(pa);

	/*
	 * Init Broadcom chip
	 */
	ubsec_init_board(sc);

	
#ifndef UBSEC_NO_RNG
	if (sc->sc_flags & UBS_FLAGS_KEY) {
		sc->sc_statmask |= BS_STAT_MCR2_DONE;

		if (ubsec_dma_malloc(sc, sizeof(struct ubsec_mcr),
		    &sc->sc_rng.rng_q.q_mcr, 0))
			goto skip_rng;

		if (ubsec_dma_malloc(sc, sizeof(struct ubsec_ctx_rngbypass),
		    &sc->sc_rng.rng_q.q_ctx, 0)) {
			ubsec_dma_free(sc, &sc->sc_rng.rng_q.q_mcr);
			goto skip_rng;
		}

		if (ubsec_dma_malloc(sc, sizeof(u_int32_t) *
		    UBSEC_RNG_BUFSIZ, &sc->sc_rng.rng_buf, 0)) {
			ubsec_dma_free(sc, &sc->sc_rng.rng_q.q_ctx);
			ubsec_dma_free(sc, &sc->sc_rng.rng_q.q_mcr);
			goto skip_rng;
		}

		timeout_set(&sc->sc_rngto, ubsec_rng, sc);
		if (hz >= 100)
			sc->sc_rnghz = hz / 100;
		else
			sc->sc_rnghz = 1;
		timeout_add(&sc->sc_rngto, sc->sc_rnghz);
		printf(", rng");
skip_rng:
	;
	}
#endif /* UBSEC_NO_RNG */

	printf(": %s\n", intrstr);
}

/*
 * UBSEC Interrupt routine
 */
int
ubsec_intr(arg)
	void *arg;
{
	struct ubsec_softc *sc = arg;
	volatile u_int32_t stat;
	struct ubsec_q *q;
	struct ubsec_dma *dmap;
	int npkts = 0, i;

	stat = READ_REG(sc, BS_STAT);

	stat &= sc->sc_statmask;
	if (stat == 0)
		return (0);

	WRITE_REG(sc, BS_STAT, stat);		/* IACK */

	/*
	 * Check to see if we have any packets waiting for us
	 */
	if ((stat & BS_STAT_MCR1_DONE)) {
		while (!SIMPLEQ_EMPTY(&sc->sc_qchip)) {
			q = SIMPLEQ_FIRST(&sc->sc_qchip);
			dmap = q->q_dma;

			if ((dmap->d_dma->d_mcr.mcr_flags & htole16(UBS_MCR_DONE)) == 0)
				break;

			SIMPLEQ_REMOVE_HEAD(&sc->sc_qchip, q, q_next);

			npkts = q->q_nstacked_mcrs;
			/*
			 * search for further sc_qchip ubsec_q's that share
			 * the same MCR, and complete them too, they must be
			 * at the top.
			 */
			for (i = 0; i < npkts; i++) {
				if(q->q_stacked_mcr[i]) {
					ubsec_callback(sc, q->q_stacked_mcr[i]);
					ubsecstats.hst_opackets++;
				} else {
					break;
				}
			}
			ubsec_callback(sc, q);
			ubsecstats.hst_opackets++;
		}

		/*
		 * Don't send any more packet to chip if there has been
		 * a DMAERR.
		 */
		if (!(stat & BS_STAT_DMAERR))
			ubsec_feed(sc);
	}

#ifndef UBSEC_NO_RNG
	/*
	 * Check to see if we have any Random number waiting for us
	 */
	if ((sc->sc_flags & UBS_FLAGS_KEY) && (stat & BS_STAT_MCR2_DONE)) {
		struct ubsec_q2 *q2;
		struct ubsec_mcr *mcr;

		while (!SIMPLEQ_EMPTY(&sc->sc_qchip2)) {
			q2 = SIMPLEQ_FIRST(&sc->sc_qchip2);

			bus_dmamap_sync(sc->sc_dmat, q2->q_mcr.dma_map,
			    0, q2->q_mcr.dma_map->dm_mapsize,
			    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

			mcr = (struct ubsec_mcr *)q2->q_mcr.dma_vaddr;
			if ((mcr->mcr_flags & htole16(UBS_MCR_DONE)) == 0) {
				bus_dmamap_sync(sc->sc_dmat,
				    q2->q_mcr.dma_map, 0,
				    q2->q_mcr.dma_map->dm_mapsize,
				    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
				break;
			}
			SIMPLEQ_REMOVE_HEAD(&sc->sc_qchip2, q2, q_next);
			ubsec_callback2(sc, q2);
			/*
			 * Don't send any more packet to chip if there has been
			 * a DMAERR.
			 */
			if (!(stat & BS_STAT_DMAERR))
				ubsec_feed2(sc);
		}
	}
#endif /* UBSEC_NO_RNG */

	/*
	 * Check to see if we got any DMA Error
	 */
	if (stat & BS_STAT_DMAERR) {
#ifdef UBSEC_DEBUG
		volatile u_int32_t a = READ_REG(sc, BS_ERR);

		printf("%s: dmaerr %s@%08x\n", sc->sc_dv.dv_xname,
		    (a & BS_ERR_READ) ? "read" : "write", a & BS_ERR_ADDR);
#endif /* UBSEC_DEBUG */
		ubsecstats.hst_dmaerr++;
		ubsec_totalreset(sc);
		ubsec_feed(sc);
	}

	return (1);
}

/*
 * ubsec_feed() - aggregate and post requests to chip
 *		  It is assumed that the caller set splnet()
 */
int
ubsec_feed(sc)
	struct ubsec_softc *sc;
{
#ifdef UBSEC_DEBUG
	static int max;
#endif /* UBSEC_DEBUG */
	struct ubsec_q *q, *q2;
	int npkts, i;
	void *v;
	u_int32_t stat;

	npkts = sc->sc_nqueue;
	if (npkts > UBS_MAX_AGGR)
		npkts = UBS_MAX_AGGR;
	if (npkts < 2)
		goto feed1;

	if ((stat = READ_REG(sc, BS_STAT)) & (BS_STAT_MCR1_FULL | BS_STAT_DMAERR)) {
		if(stat & BS_STAT_DMAERR) {
			ubsec_totalreset(sc);
			ubsecstats.hst_dmaerr++;
		}
		return (0);
	}

#ifdef UBSEC_DEBUG
	printf("merging %d records\n", npkts);

	/* XXX temporary aggregation statistics reporting code */
	if (max < npkts) {
		max = npkts;
		printf("%s: new max aggregate %d\n", sc->sc_dv.dv_xname, max);
	}
#endif /* UBSEC_DEBUG */

	q = SIMPLEQ_FIRST(&sc->sc_queue);
	SIMPLEQ_REMOVE_HEAD(&sc->sc_queue, q, q_next);
	--sc->sc_nqueue;

	bus_dmamap_sync(sc->sc_dmat, q->q_src_map,
	    0, q->q_src_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
	if (q->q_dst_map != NULL)
		bus_dmamap_sync(sc->sc_dmat, q->q_dst_map,
		    0, q->q_dst_map->dm_mapsize, BUS_DMASYNC_PREREAD);

	q->q_nstacked_mcrs = npkts - 1;		/* Number of packets stacked */

	for (i = 0; i < q->q_nstacked_mcrs; i++) {
		q2 = SIMPLEQ_FIRST(&sc->sc_queue);
		bus_dmamap_sync(sc->sc_dmat, q2->q_src_map,
		    0, q2->q_src_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
		if (q2->q_dst_map != NULL)
			bus_dmamap_sync(sc->sc_dmat, q2->q_dst_map,
			    0, q2->q_dst_map->dm_mapsize, BUS_DMASYNC_PREREAD);
		SIMPLEQ_REMOVE_HEAD(&sc->sc_queue, q2, q_next);
		--sc->sc_nqueue;

		v = ((void *)&q2->q_dma->d_dma->d_mcr) + sizeof(struct ubsec_mcr) -
		    sizeof(struct ubsec_mcr_add);
		bcopy(v, &q->q_dma->d_dma->d_mcradd[i], sizeof(struct ubsec_mcr_add));
		q->q_stacked_mcr[i] = q2;
	}
	q->q_dma->d_dma->d_mcr.mcr_pkts = htole16(npkts);
	SIMPLEQ_INSERT_TAIL(&sc->sc_qchip, q, q_next);
	bus_dmamap_sync(sc->sc_dmat, q->q_dma->d_alloc.dma_map,
	    0, q->q_dma->d_alloc.dma_map->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	WRITE_REG(sc, BS_MCR1, q->q_dma->d_alloc.dma_paddr +
	    offsetof(struct ubsec_dmachunk, d_mcr));
	return (0);

feed1:
	while (!SIMPLEQ_EMPTY(&sc->sc_queue)) {
		if ((stat = READ_REG(sc, BS_STAT)) & (BS_STAT_MCR1_FULL | BS_STAT_DMAERR)) {
			if(stat & BS_STAT_DMAERR) {
				ubsec_totalreset(sc);
				ubsecstats.hst_dmaerr++;
			}
			break;
		}

		q = SIMPLEQ_FIRST(&sc->sc_queue);

		bus_dmamap_sync(sc->sc_dmat, q->q_src_map,
		    0, q->q_src_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
		if (q->q_dst_map != NULL)
			bus_dmamap_sync(sc->sc_dmat, q->q_dst_map,
			    0, q->q_dst_map->dm_mapsize, BUS_DMASYNC_PREREAD);
		bus_dmamap_sync(sc->sc_dmat, q->q_dma->d_alloc.dma_map,
		    0, q->q_dma->d_alloc.dma_map->dm_mapsize,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		WRITE_REG(sc, BS_MCR1, q->q_dma->d_alloc.dma_paddr +
		    offsetof(struct ubsec_dmachunk, d_mcr));
#ifdef UBSEC_DEBUG
		printf("feed: q->chip %p %08x\n", q, (u_int32_t)vtophys(&q->q_dma->d_dma->d_mcr));
#endif /* UBSEC_DEBUG */
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
 */
int
ubsec_newsession(sidp, cri)
	u_int32_t *sidp;
	struct cryptoini *cri;
{
	struct cryptoini *c, *encini = NULL, *macini = NULL;
	struct ubsec_softc *sc = NULL;
	struct ubsec_session *ses = NULL;
	MD5_CTX md5ctx;
	SHA1_CTX sha1ctx;
	int i, sesn;

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
		if (c->cri_alg == CRYPTO_MD5_HMAC ||
		    c->cri_alg == CRYPTO_SHA1_HMAC) {
			if (macini)
				return (EINVAL);
			macini = c;
		} else if (c->cri_alg == CRYPTO_DES_CBC ||
		    c->cri_alg == CRYPTO_3DES_CBC) {
			if (encini)
				return (EINVAL);
			encini = c;
		} else
			return (EINVAL);
	}
	if (encini == NULL && macini == NULL)
		return (EINVAL);

	if (sc->sc_sessions == NULL) {
		ses = sc->sc_sessions = (struct ubsec_session *)malloc(
		    sizeof(struct ubsec_session), M_DEVBUF, M_NOWAIT);
		if (ses == NULL)
			return (ENOMEM);
		sesn = 0;
		sc->sc_nsessions = 1;
	} else {
		for (sesn = 0; sesn < sc->sc_nsessions; sesn++) {
			if (sc->sc_sessions[sesn].ses_used == 0) {
				ses = &sc->sc_sessions[sesn];
				break;
			}
		}

		if (ses == NULL) {
			sesn = sc->sc_nsessions;
			ses = (struct ubsec_session *)malloc((sesn + 1) *
			    sizeof(struct ubsec_session), M_DEVBUF, M_NOWAIT);
			if (ses == NULL)
				return (ENOMEM);
			bcopy(sc->sc_sessions, ses, sesn *
			    sizeof(struct ubsec_session));
			bzero(sc->sc_sessions, sesn *
			    sizeof(struct ubsec_session));
			free(sc->sc_sessions, M_DEVBUF);
			sc->sc_sessions = ses;
			ses = &sc->sc_sessions[sesn];
			sc->sc_nsessions++;
		}
	}

	bzero(ses, sizeof(struct ubsec_session));
	ses->ses_used = 1;
	if (encini) {
		/* get an IV, network byte order */
		get_random_bytes(ses->ses_iv, sizeof(ses->ses_iv));

		/* Go ahead and compute key in ubsec's byte order */
		if (encini->cri_alg == CRYPTO_DES_CBC) {
			bcopy(encini->cri_key, &ses->ses_deskey[0], 8);
			bcopy(encini->cri_key, &ses->ses_deskey[2], 8);
			bcopy(encini->cri_key, &ses->ses_deskey[4], 8);
		} else
			bcopy(encini->cri_key, ses->ses_deskey, 24);

		SWAP32(ses->ses_deskey[0]);
		SWAP32(ses->ses_deskey[1]);
		SWAP32(ses->ses_deskey[2]);
		SWAP32(ses->ses_deskey[3]);
		SWAP32(ses->ses_deskey[4]);
		SWAP32(ses->ses_deskey[5]);
	}

	if (macini) {
		for (i = 0; i < macini->cri_klen / 8; i++)
			macini->cri_key[i] ^= HMAC_IPAD_VAL;

		if (macini->cri_alg == CRYPTO_MD5_HMAC) {
			MD5Init(&md5ctx);
			MD5Update(&md5ctx, macini->cri_key,
			    macini->cri_klen / 8);
			MD5Update(&md5ctx, hmac_ipad_buffer,
			    HMAC_BLOCK_LEN - (macini->cri_klen / 8));
			bcopy(md5ctx.state, ses->ses_hminner,
			    sizeof(md5ctx.state));
		} else {
			SHA1Init(&sha1ctx);
			SHA1Update(&sha1ctx, macini->cri_key,
			    macini->cri_klen / 8);
			SHA1Update(&sha1ctx, hmac_ipad_buffer,
			    HMAC_BLOCK_LEN - (macini->cri_klen / 8));
			bcopy(sha1ctx.state, ses->ses_hminner,
			    sizeof(sha1ctx.state));
		}

		for (i = 0; i < macini->cri_klen / 8; i++)
			macini->cri_key[i] ^= (HMAC_IPAD_VAL ^ HMAC_OPAD_VAL);

		if (macini->cri_alg == CRYPTO_MD5_HMAC) {
			MD5Init(&md5ctx);
			MD5Update(&md5ctx, macini->cri_key,
			    macini->cri_klen / 8);
			MD5Update(&md5ctx, hmac_opad_buffer,
			    HMAC_BLOCK_LEN - (macini->cri_klen / 8));
			bcopy(md5ctx.state, ses->ses_hmouter,
			    sizeof(md5ctx.state));
		} else {
			SHA1Init(&sha1ctx);
			SHA1Update(&sha1ctx, macini->cri_key,
			    macini->cri_klen / 8);
			SHA1Update(&sha1ctx, hmac_opad_buffer,
			    HMAC_BLOCK_LEN - (macini->cri_klen / 8));
			bcopy(sha1ctx.state, ses->ses_hmouter,
			    sizeof(sha1ctx.state));
		}

		for (i = 0; i < macini->cri_klen / 8; i++)
			macini->cri_key[i] ^= HMAC_OPAD_VAL;
	}

	*sidp = UBSEC_SID(sc->sc_dv.dv_unit, sesn);
	return (0);
}

/*
 * Deallocate a session.
 */
int
ubsec_freesession(tid)
	u_int64_t tid;
{
	struct ubsec_softc *sc;
	int card, session;
	u_int32_t sid = ((u_int32_t) tid) & 0xffffffff;

	card = UBSEC_CARD(sid);
	if (card >= ubsec_cd.cd_ndevs || ubsec_cd.cd_devs[card] == NULL)
		return (EINVAL);
	sc = ubsec_cd.cd_devs[card];
	session = UBSEC_SESSION(sid);
	bzero(&sc->sc_sessions[session], sizeof(sc->sc_sessions[session]));
	return (0);
}

int
ubsec_process(crp)
	struct cryptop *crp;
{
	struct ubsec_q *q = NULL;
	int card, err, i, j, s, nicealign;
	struct ubsec_softc *sc;
	struct cryptodesc *crd1, *crd2, *maccrd, *enccrd;
	int encoffset = 0, macoffset = 0, cpskip, cpoffset;
	int sskip, dskip, stheend, dtheend;
	int16_t coffset;
	struct ubsec_session *ses;
	struct ubsec_pktctx ctx;
	struct ubsec_dma *dmap = NULL;

	if (crp == NULL || crp->crp_callback == NULL) {
		ubsecstats.hst_invalid++;
		return (EINVAL);
	}
	card = UBSEC_CARD(crp->crp_sid);
	if (card >= ubsec_cd.cd_ndevs || ubsec_cd.cd_devs[card] == NULL) {
		ubsecstats.hst_invalid++;
		return (EINVAL);
	}

	sc = ubsec_cd.cd_devs[card];

	s = splnet();

	if (SIMPLEQ_EMPTY(&sc->sc_freequeue)) {
		ubsecstats.hst_queuefull++;
		splx(s);
		err = ENOMEM;
		goto errout2;
	}

	q = SIMPLEQ_FIRST(&sc->sc_freequeue);
	SIMPLEQ_REMOVE_HEAD(&sc->sc_freequeue, q, q_next);
	splx(s);

	dmap = q->q_dma; /* Save dma pointer */
	bzero(q, sizeof(struct ubsec_q));
	bzero(&ctx, sizeof(ctx));

	q->q_sesn = UBSEC_SESSION(crp->crp_sid);
	q->q_dma = dmap;
	ses = &sc->sc_sessions[q->q_sesn];

	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		q->q_src_m = (struct mbuf *)crp->crp_buf;
		q->q_dst_m = (struct mbuf *)crp->crp_buf;
	} else if (crp->crp_flags & CRYPTO_F_IOV) {
		q->q_src_io = (struct uio *)crp->crp_buf;
		q->q_dst_io = (struct uio *)crp->crp_buf;
	} else {
		err = EINVAL;
		goto errout;	/* XXX we don't handle contiguous blocks! */
	}

	bzero(&dmap->d_dma->d_mcr, sizeof(struct ubsec_mcr));

	dmap->d_dma->d_mcr.mcr_pkts = htole16(1);
	dmap->d_dma->d_mcr.mcr_flags = 0;
	q->q_crp = crp;

	crd1 = crp->crp_desc;
	if (crd1 == NULL) {
		err = EINVAL;
		goto errout;
	}
	crd2 = crd1->crd_next;

	if (crd2 == NULL) {
		if (crd1->crd_alg == CRYPTO_MD5_HMAC ||
		    crd1->crd_alg == CRYPTO_SHA1_HMAC) {
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
		if ((crd1->crd_alg == CRYPTO_MD5_HMAC ||
		    crd1->crd_alg == CRYPTO_SHA1_HMAC) &&
		    (crd2->crd_alg == CRYPTO_DES_CBC ||
			crd2->crd_alg == CRYPTO_3DES_CBC) &&
		    ((crd2->crd_flags & CRD_F_ENCRYPT) == 0)) {
			maccrd = crd1;
			enccrd = crd2;
		} else if ((crd1->crd_alg == CRYPTO_DES_CBC ||
		    crd1->crd_alg == CRYPTO_3DES_CBC) &&
		    (crd2->crd_alg == CRYPTO_MD5_HMAC ||
			crd2->crd_alg == CRYPTO_SHA1_HMAC) &&
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
		ctx.pc_flags |= htole16(UBS_PKTCTX_ENC_3DES);

		if (enccrd->crd_flags & CRD_F_ENCRYPT) {
			q->q_flags |= UBSEC_QFLAGS_COPYOUTIV;

			if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
				bcopy(enccrd->crd_iv, ctx.pc_iv, 8);
			else {
				ctx.pc_iv[0] = ses->ses_iv[0];
				ctx.pc_iv[1] = ses->ses_iv[1];
			}

			if ((enccrd->crd_flags & CRD_F_IV_PRESENT) == 0) {
				if (crp->crp_flags & CRYPTO_F_IMBUF)
					m_copyback(q->q_src_m,
					    enccrd->crd_inject,
					    8, (caddr_t)ctx.pc_iv);
				else if (crp->crp_flags & CRYPTO_F_IOV)
					cuio_copyback(q->q_src_io,
					    enccrd->crd_inject,
					    8, (caddr_t)ctx.pc_iv);
			}
		} else {
			ctx.pc_flags |= htole16(UBS_PKTCTX_INBOUND);

			if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
				bcopy(enccrd->crd_iv, ctx.pc_iv, 8);
			else if (crp->crp_flags & CRYPTO_F_IMBUF)
				m_copydata(q->q_src_m, enccrd->crd_inject,
				    8, (caddr_t)ctx.pc_iv);
			else if (crp->crp_flags & CRYPTO_F_IOV)
				cuio_copydata(q->q_src_io,
				    enccrd->crd_inject, 8,
				    (caddr_t)ctx.pc_iv);
		}

		ctx.pc_deskey[0] = ses->ses_deskey[0];
		ctx.pc_deskey[1] = ses->ses_deskey[1];
		ctx.pc_deskey[2] = ses->ses_deskey[2];
		ctx.pc_deskey[3] = ses->ses_deskey[3];
		ctx.pc_deskey[4] = ses->ses_deskey[4];
		ctx.pc_deskey[5] = ses->ses_deskey[5];
		SWAP32(ctx.pc_iv[0]);
		SWAP32(ctx.pc_iv[1]);
	}

	if (maccrd) {
		macoffset = maccrd->crd_skip;

		if (maccrd->crd_alg == CRYPTO_MD5_HMAC)
			ctx.pc_flags |= htole16(UBS_PKTCTX_AUTH_MD5);
		else
			ctx.pc_flags |= htole16(UBS_PKTCTX_AUTH_SHA1);

		for (i = 0; i < 5; i++) {
			ctx.pc_hminner[i] = ses->ses_hminner[i];
			ctx.pc_hmouter[i] = ses->ses_hmouter[i];

			HTOLE32(ctx.pc_hminner[i]);
			HTOLE32(ctx.pc_hmouter[i]);
		}
	}

	if (enccrd && maccrd) {
		/*
		 * ubsec cannot handle packets where the end of encryption
		 * and authentication are not the same, or where the
		 * encrypted part begins before the authenticated part.
		 */
		if (((encoffset + enccrd->crd_len) !=
		    (macoffset + maccrd->crd_len)) ||
		    (enccrd->crd_skip < maccrd->crd_skip)) {
			err = EINVAL;
			goto errout;
		}
		sskip = maccrd->crd_skip;
		cpskip = dskip = enccrd->crd_skip;
		stheend = maccrd->crd_len;
		dtheend = enccrd->crd_len;
		coffset = enccrd->crd_skip - maccrd->crd_skip;
		cpoffset = cpskip + dtheend;
#ifdef UBSEC_DEBUG
		printf("mac: skip %d, len %d, inject %d\n",
 		    maccrd->crd_skip, maccrd->crd_len, maccrd->crd_inject);
		printf("enc: skip %d, len %d, inject %d\n",
		    enccrd->crd_skip, enccrd->crd_len, enccrd->crd_inject);
		printf("src: skip %d, len %d\n", sskip, stheend);
		printf("dst: skip %d, len %d\n", dskip, dtheend);
		printf("ubs: coffset %d, pktlen %d, cpskip %d, cpoffset %d\n",
		    coffset, stheend, cpskip, cpoffset);
#endif
	} else {
		cpskip = dskip = sskip = macoffset + encoffset;
		dtheend = stheend = (enccrd)?enccrd->crd_len:maccrd->crd_len;
		cpoffset = cpskip + dtheend;
		coffset = 0;
	}
	ctx.pc_offset = htole16(coffset >> 2);

	if (bus_dmamap_create(sc->sc_dmat, 0xfff0, UBS_MAX_SCATTER,
		0xfff0, 0, BUS_DMA_NOWAIT, &q->q_src_map) != 0) {
		err = ENOMEM;
		goto errout;
	}
	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		if (bus_dmamap_load_mbuf(sc->sc_dmat, q->q_src_map,
		    q->q_src_m, BUS_DMA_NOWAIT) != 0) {
			bus_dmamap_destroy(sc->sc_dmat, q->q_src_map);
			q->q_src_map = NULL;
			err = ENOMEM;
			goto errout;
		}
	} else if (crp->crp_flags & CRYPTO_F_IOV) {
		if (bus_dmamap_load_uio(sc->sc_dmat, q->q_src_map,
		    q->q_src_io, BUS_DMA_NOWAIT) != 0) {
			bus_dmamap_destroy(sc->sc_dmat, q->q_src_map);
			q->q_src_map = NULL;
			err = ENOMEM;
			goto errout;
		}
	}
	nicealign = ubsec_dmamap_aligned(q->q_src_map);

	dmap->d_dma->d_mcr.mcr_pktlen = htole16(stheend);

#ifdef UBSEC_DEBUG
	printf("src skip: %d\n", sskip);
#endif
	for (i = j = 0; i < q->q_src_map->dm_nsegs; i++) {
		struct ubsec_pktbuf *pb;
		bus_size_t packl = q->q_src_map->dm_segs[i].ds_len;
		bus_addr_t packp = q->q_src_map->dm_segs[i].ds_addr;

		if (sskip >= packl) {
			sskip -= packl;
			continue;
		}

		packl -= sskip;
		packp += sskip;
		sskip = 0;

		if (packl > 0xfffc) {
			err = EIO;
			goto errout;
		}

		if (j == 0)
			pb = &dmap->d_dma->d_mcr.mcr_ipktbuf;
		else
			pb = &dmap->d_dma->d_sbuf[j - 1];

		pb->pb_addr = htole32(packp);

		if (stheend) {
			if (packl > stheend) {
				pb->pb_len = htole32(stheend);
				stheend = 0;
			} else {
				pb->pb_len = htole32(packl);
				stheend -= packl;
			}
		} else
			pb->pb_len = htole32(packl);

		if ((i + 1) == q->q_src_map->dm_nsegs)
			pb->pb_next = 0;
		else
			pb->pb_next = htole32(dmap->d_alloc.dma_paddr +
			    offsetof(struct ubsec_dmachunk, d_sbuf[j]));
		j++;
	}

	if (enccrd == NULL && maccrd != NULL) {
		dmap->d_dma->d_mcr.mcr_opktbuf.pb_addr = 0;
		dmap->d_dma->d_mcr.mcr_opktbuf.pb_len = 0;
		dmap->d_dma->d_mcr.mcr_opktbuf.pb_next = htole32(dmap->d_alloc.dma_paddr +
		    offsetof(struct ubsec_dmachunk, d_macbuf[0]));
#ifdef UBSEC_DEBUG
		printf("opkt: %x %x %x\n",
		    dmap->d_mcr->mcr_opktbuf.pb_addr,
		    dmap->d_mcr->mcr_opktbuf.pb_len,
		    dmap->d_mcr->mcr_opktbuf.pb_next);
#endif
	} else {
		if (crp->crp_flags & CRYPTO_F_IOV) {
			if (!nicealign) {
				err = EINVAL;
				goto errout;
			}
			if (bus_dmamap_create(sc->sc_dmat, 0xfff0,
			    UBS_MAX_SCATTER, 0xfff0, 0, BUS_DMA_NOWAIT,
			    &q->q_dst_map) != 0) {
				err = ENOMEM;
				goto errout;
			}
			if (bus_dmamap_load_uio(sc->sc_dmat, q->q_dst_map,
			    q->q_dst_io, BUS_DMA_NOWAIT) != 0) {
				bus_dmamap_destroy(sc->sc_dmat, q->q_dst_map);
				q->q_dst_map = NULL;
				goto errout;
			}
		} else if (crp->crp_flags & CRYPTO_F_IMBUF) {
			if (nicealign) {
				q->q_dst_m = q->q_src_m;
				q->q_dst_map = q->q_src_map;
			} else {
				int totlen, len;
				struct mbuf *m, *top, **mp;

				totlen = q->q_src_map->dm_mapsize;
				if (q->q_src_m->m_flags & M_PKTHDR) {
					len = MHLEN;
					MGETHDR(m, M_DONTWAIT, MT_DATA);
				} else {
					len = MLEN;
					MGET(m, M_DONTWAIT, MT_DATA);
				}
				if (m == NULL) {
					err = ENOMEM;
					goto errout;
				}
				if (len == MHLEN)
					M_DUP_PKTHDR(m, q->q_src_m);
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
				ubsec_mcopy(q->q_src_m, q->q_dst_m,
				    cpskip, cpoffset);
				if (bus_dmamap_create(sc->sc_dmat, 0xfff0,
				    UBS_MAX_SCATTER, 0xfff0, 0, BUS_DMA_NOWAIT,
				    &q->q_dst_map) != 0) {
					err = ENOMEM;
					goto errout;
				}
				if (bus_dmamap_load_mbuf(sc->sc_dmat,
				    q->q_dst_map, q->q_dst_m,
				    BUS_DMA_NOWAIT) != 0) {
					bus_dmamap_destroy(sc->sc_dmat,
					q->q_dst_map);
					q->q_dst_map = NULL;
					err = ENOMEM;
					goto errout;
				}
			}
		} else {
			err = EINVAL;
			goto errout;
		}

#ifdef UBSEC_DEBUG
		printf("dst skip: %d\n", dskip);
#endif
		for (i = j = 0; i < q->q_dst_map->dm_nsegs; i++) {
			struct ubsec_pktbuf *pb;
			bus_size_t packl = q->q_dst_map->dm_segs[i].ds_len;
			bus_addr_t packp = q->q_dst_map->dm_segs[i].ds_addr;

			if (dskip >= packl) {
				dskip -= packl;
				continue;
			}

			packl -= dskip;
			packp += dskip;
			dskip = 0;

			if (packl > 0xfffc) {
				err = EIO;
				goto errout;
			}

			if (j == 0)
				pb = &dmap->d_dma->d_mcr.mcr_opktbuf;
			else
				pb = &dmap->d_dma->d_dbuf[j - 1];

			pb->pb_addr = htole32(packp);

			if (dtheend) {
				if (packl > dtheend) {
					pb->pb_len = htole32(dtheend);
					dtheend = 0;
				} else {
					pb->pb_len = htole32(packl);
					dtheend -= packl;
				}
			} else
				pb->pb_len = htole32(packl);

			if ((i + 1) == q->q_dst_map->dm_nsegs) {
				if (maccrd)
					pb->pb_next = htole32(dmap->d_alloc.dma_paddr +
					    offsetof(struct ubsec_dmachunk, d_macbuf[0]));
				else
					pb->pb_next = 0;
			} else
				pb->pb_next = htole32(dmap->d_alloc.dma_paddr +
				    offsetof(struct ubsec_dmachunk, d_dbuf[j]));
			j++;
		}
	}

	dmap->d_dma->d_mcr.mcr_cmdctxp = htole32(dmap->d_alloc.dma_paddr +
	    offsetof(struct ubsec_dmachunk, d_ctx));

	if (sc->sc_flags & UBS_FLAGS_LONGCTX) {
		struct ubsec_pktctx_long *ctxl;

		ctxl = (struct ubsec_pktctx_long *)(dmap->d_alloc.dma_vaddr +
		    offsetof(struct ubsec_dmachunk, d_ctx));
		
		/* transform small context into long context */
		ctxl->pc_len = htole16(sizeof(struct ubsec_pktctx_long));
		ctxl->pc_type = htole16(UBS_PKTCTX_TYPE_IPSEC);
		ctxl->pc_flags = ctx.pc_flags;
		ctxl->pc_offset = ctx.pc_offset;
		for (i = 0; i < 6; i++)
			ctxl->pc_deskey[i] = ctx.pc_deskey[i];
		for (i = 0; i < 5; i++)
			ctxl->pc_hminner[i] = ctx.pc_hminner[i];
		for (i = 0; i < 5; i++)
			ctxl->pc_hmouter[i] = ctx.pc_hmouter[i];   
		ctxl->pc_iv[0] = ctx.pc_iv[0];
		ctxl->pc_iv[1] = ctx.pc_iv[1];
	} else
		bcopy(&ctx, dmap->d_alloc.dma_vaddr +
		    offsetof(struct ubsec_dmachunk, d_ctx),
		    sizeof(struct ubsec_pktctx));

	s = splnet();
	SIMPLEQ_INSERT_TAIL(&sc->sc_queue, q, q_next);
	sc->sc_nqueue++;
	ubsecstats.hst_ipackets++;
	ubsecstats.hst_ibytes += dmap->d_alloc.dma_map->dm_mapsize;
	ubsec_feed(sc);
	splx(s);
	return (0);

errout:
	if (q != NULL) {
		if ((q->q_dst_m != NULL) && (q->q_src_m != q->q_dst_m))
			m_freem(q->q_dst_m);

		if (q->q_dst_map != NULL && q->q_dst_map != q->q_src_map) {
			bus_dmamap_unload(sc->sc_dmat, q->q_dst_map);
			bus_dmamap_destroy(sc->sc_dmat, q->q_dst_map);
		}
		if (q->q_src_map != NULL) {
			bus_dmamap_unload(sc->sc_dmat, q->q_src_map);
			bus_dmamap_destroy(sc->sc_dmat, q->q_src_map);
		}

		s = splnet();
		SIMPLEQ_INSERT_TAIL(&sc->sc_freequeue, q, q_next);
		splx(s);
	}
	if (err == EINVAL)
		ubsecstats.hst_invalid++;
	else
		ubsecstats.hst_nomem++;
errout2:
	crp->crp_etype = err;
	crp->crp_callback(crp);
	return (0);
}

void
ubsec_callback(sc, q)
	struct ubsec_softc *sc;
	struct ubsec_q *q;
{
	struct cryptop *crp = (struct cryptop *)q->q_crp;
	struct cryptodesc *crd;
	struct ubsec_dma *dmap = q->q_dma;

	bus_dmamap_sync(sc->sc_dmat, dmap->d_alloc.dma_map, 0,
	    dmap->d_alloc.dma_map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	if (q->q_dst_map != NULL && q->q_dst_map != q->q_src_map) {
		bus_dmamap_sync(sc->sc_dmat, q->q_dst_map,
		    0, q->q_dst_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, q->q_dst_map);
		bus_dmamap_destroy(sc->sc_dmat, q->q_dst_map);
	}
	bus_dmamap_sync(sc->sc_dmat, q->q_src_map,
	    0, q->q_src_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, q->q_src_map);
	bus_dmamap_destroy(sc->sc_dmat, q->q_src_map);

	if ((crp->crp_flags & CRYPTO_F_IMBUF) && (q->q_src_m != q->q_dst_m)) {
		m_freem(q->q_src_m);
		crp->crp_buf = (caddr_t)q->q_dst_m;
	}
	ubsecstats.hst_obytes += ((struct mbuf *)crp->crp_buf)->m_len;

	/* copy out IV for future use */
	if (q->q_flags & UBSEC_QFLAGS_COPYOUTIV) {
		for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
			if (crd->crd_alg != CRYPTO_DES_CBC &&
			    crd->crd_alg != CRYPTO_3DES_CBC)
				continue;
			if (crp->crp_flags & CRYPTO_F_IMBUF)
				m_copydata((struct mbuf *)crp->crp_buf,
				    crd->crd_skip + crd->crd_len - 8, 8,
				    (caddr_t)sc->sc_sessions[q->q_sesn].ses_iv);
			else if (crp->crp_flags & CRYPTO_F_IOV) {
				cuio_copydata((struct uio *)crp->crp_buf,
				    crd->crd_skip + crd->crd_len - 8, 8,
				    (caddr_t)sc->sc_sessions[q->q_sesn].ses_iv);
			}
			break;
		}
	}

	for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
		if (crd->crd_alg != CRYPTO_MD5_HMAC &&
		    crd->crd_alg != CRYPTO_SHA1_HMAC)
			continue;
		if (crp->crp_flags & CRYPTO_F_IMBUF)
			m_copyback((struct mbuf *)crp->crp_buf,
			    crd->crd_inject, 12,
			    (caddr_t)dmap->d_dma->d_macbuf);
		else if (crp->crp_flags & CRYPTO_F_IOV && crp->crp_mac)
			bcopy((caddr_t)dmap->d_dma->d_macbuf,
			    crp->crp_mac, 12);
		break;
	}
	SIMPLEQ_INSERT_TAIL(&sc->sc_freequeue, q, q_next);
	crypto_done(crp);
}

void
ubsec_mcopy(srcm, dstm, hoffset, toffset)
	struct mbuf *srcm, *dstm;
	int hoffset, toffset;
{
	int i, j, dlen, slen;
	caddr_t dptr, sptr;

	j = 0;
	sptr = srcm->m_data;
	slen = srcm->m_len;
	dptr = dstm->m_data;
	dlen = dstm->m_len;

	while (1) {
		for (i = 0; i < min(slen, dlen); i++) {
			if (j < hoffset || j >= toffset)
				*dptr++ = *sptr++;
			slen--;
			dlen--;
			j++;
		}
		if (slen == 0) {
			srcm = srcm->m_next;
			if (srcm == NULL)
				return;
			sptr = srcm->m_data;
			slen = srcm->m_len;
		}
		if (dlen == 0) {
			dstm = dstm->m_next;
			if (dstm == NULL)
				return;
			dptr = dstm->m_data;
			dlen = dstm->m_len;
		}
	}
}

#ifndef UBSEC_NO_RNG
/*
 * feed the key generator, must be called at splnet() or higher.
 */
int
ubsec_feed2(sc)
	struct ubsec_softc *sc;
{
	struct ubsec_q2 *q;

	while (!SIMPLEQ_EMPTY(&sc->sc_queue2)) {
		if (READ_REG(sc, BS_STAT) & BS_STAT_MCR2_FULL)
			break;
		q = SIMPLEQ_FIRST(&sc->sc_queue2);

		bus_dmamap_sync(sc->sc_dmat, q->q_mcr.dma_map, 0,
		    q->q_mcr.dma_map->dm_mapsize,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		bus_dmamap_sync(sc->sc_dmat, q->q_ctx.dma_map, 0,
		    q->q_ctx.dma_map->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		WRITE_REG(sc, BS_MCR2, q->q_mcr.dma_paddr);
		SIMPLEQ_REMOVE_HEAD(&sc->sc_queue2, q, q_next);
		--sc->sc_nqueue2;
		SIMPLEQ_INSERT_TAIL(&sc->sc_qchip2, q, q_next);
	}
	return (0);
}

/*
 * Callback for handling random numbers
 */
void
ubsec_callback2(sc, q)
	struct ubsec_softc *sc;
	struct ubsec_q2 *q;
{
	struct ubsec_ctx_keyop *ctx;

	ctx = (struct ubsec_ctx_keyop *)q->q_ctx.dma_vaddr;
	bus_dmamap_sync(sc->sc_dmat, q->q_ctx.dma_map, 0,
	    q->q_ctx.dma_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);

	switch (letoh16(ctx->ctx_op)) {
	case UBS_CTXOP_RNGBYPASS: {
		struct ubsec_q2_rng *rng = (struct ubsec_q2_rng *)q;
		u_int32_t *p;
		int i;

		bus_dmamap_sync(sc->sc_dmat, rng->rng_buf.dma_map, 0,
		    rng->rng_buf.dma_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		p = (u_int32_t *)rng->rng_buf.dma_vaddr;
		for (i = 0; i < UBSEC_RNG_BUFSIZ; p++, i++)
			add_true_randomness(letoh32(*p));
		rng->rng_used = 0;
		timeout_add(&sc->sc_rngto, sc->sc_rnghz);
		break;
	}
	default:
		printf("%s: unknown ctx op: %x\n", sc->sc_dv.dv_xname,
		    letoh16(ctx->ctx_op));
		break;
	}
}

void
ubsec_rng(vsc)
	void *vsc;
{
	struct ubsec_softc *sc = vsc;
	struct ubsec_q2_rng *rng = &sc->sc_rng;
	struct ubsec_mcr *mcr;
	struct ubsec_ctx_rngbypass *ctx;
	int s;

	s = splnet();
	if (rng->rng_used) {
		splx(s);
		return;
	}
	sc->sc_nqueue2++;
	if (sc->sc_nqueue2 >= UBS_MAX_NQUEUE)
		goto out;

	mcr = (struct ubsec_mcr *)rng->rng_q.q_mcr.dma_vaddr;
	ctx = (struct ubsec_ctx_rngbypass *)rng->rng_q.q_ctx.dma_vaddr;

	mcr->mcr_pkts = htole16(1);
	mcr->mcr_flags = 0;
	mcr->mcr_cmdctxp = htole32(rng->rng_q.q_ctx.dma_paddr);
	mcr->mcr_ipktbuf.pb_addr = mcr->mcr_ipktbuf.pb_next = 0;
	mcr->mcr_ipktbuf.pb_len = 0;
	mcr->mcr_reserved = mcr->mcr_pktlen = 0;
	mcr->mcr_opktbuf.pb_addr = htole32(rng->rng_buf.dma_paddr);
	mcr->mcr_opktbuf.pb_len = htole32(((sizeof(u_int32_t) * UBSEC_RNG_BUFSIZ)) &
	    UBS_PKTBUF_LEN);
	mcr->mcr_opktbuf.pb_next = 0;

	ctx->rbp_len = htole16(sizeof(struct ubsec_ctx_rngbypass));
	ctx->rbp_op = htole16(UBS_CTXOP_RNGBYPASS);

	bus_dmamap_sync(sc->sc_dmat, rng->rng_buf.dma_map, 0,
	    rng->rng_buf.dma_map->dm_mapsize, BUS_DMASYNC_PREREAD);

	SIMPLEQ_INSERT_TAIL(&sc->sc_queue2, (struct ubsec_q2 *)rng, q_next);
	rng->rng_used = 1;
	ubsec_feed2(sc);
	splx(s);

	return;

out:
	/*
	 * Something weird happened, generate our own call back.
	 */
	sc->sc_nqueue2--;
	splx(s);
	timeout_add(&sc->sc_rngto, sc->sc_rnghz);
}
#endif /* UBSEC_NO_RNG */

int
ubsec_dma_malloc(sc, size, dma, mapflags)
	struct ubsec_softc *sc;
	bus_size_t size;
	struct ubsec_dma_alloc *dma;
	int mapflags;
{
	int r;

	if ((r = bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0,
	    &dma->dma_seg, 1, &dma->dma_nseg, BUS_DMA_NOWAIT)) != 0)
		goto fail_0;

	if ((r = bus_dmamem_map(sc->sc_dmat, &dma->dma_seg, dma->dma_nseg,
	    size, &dma->dma_vaddr, mapflags | BUS_DMA_NOWAIT)) != 0)
		goto fail_1;

	if ((r = bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT, &dma->dma_map)) != 0)
		goto fail_2;

	if ((r = bus_dmamap_load(sc->sc_dmat, dma->dma_map, dma->dma_vaddr,
	    size, NULL, BUS_DMA_NOWAIT)) != 0)
		goto fail_3;

	dma->dma_paddr = dma->dma_map->dm_segs[0].ds_addr;
	dma->dma_size = size;
	return (0);

fail_3:
	bus_dmamap_destroy(sc->sc_dmat, dma->dma_map);
fail_2:
	bus_dmamem_unmap(sc->sc_dmat, dma->dma_vaddr, size);
fail_1:
	bus_dmamem_free(sc->sc_dmat, &dma->dma_seg, dma->dma_nseg);
fail_0:
	dma->dma_map = NULL;
	return (r);
}

void
ubsec_dma_free(sc, dma)
	struct ubsec_softc *sc;
	struct ubsec_dma_alloc *dma;
{
	bus_dmamap_unload(sc->sc_dmat, dma->dma_map);
	bus_dmamem_unmap(sc->sc_dmat, dma->dma_vaddr, dma->dma_size);
	bus_dmamem_free(sc->sc_dmat, &dma->dma_seg, dma->dma_nseg);
	bus_dmamap_destroy(sc->sc_dmat, dma->dma_map);
}

/*
 * Resets the board.  Values in the regesters are left as is
 * from the reset (i.e. initial values are assigned elsewhere).
 */
void
ubsec_reset_board(sc)
	struct ubsec_softc *sc;
{
    volatile u_int32_t ctrl;

    ctrl = READ_REG(sc, BS_CTRL);
    ctrl |= BS_CTRL_RESET;
    WRITE_REG(sc, BS_CTRL, ctrl);

    /*
     * Wait aprox. 30 PCI clocks = 900 ns = 0.9 us
     */
    DELAY(10);
}

/*
 * Init Broadcom registers
 */
void
   ubsec_init_board(sc)
   struct ubsec_softc *sc;
{
	WRITE_REG(sc, BS_CTRL,
	    READ_REG(sc, BS_CTRL) | BS_CTRL_MCR1INT | BS_CTRL_DMAERR |
	    ((sc->sc_flags & UBS_FLAGS_KEY) ? BS_CTRL_MCR2INT : 0));
}

/*
 * Init Broadcom PCI registers
 */
void
   ubsec_init_pciregs(pa)
   struct pci_attach_args *pa;
{
	pci_chipset_tag_t pc = pa->pa_pc;
	u_int32_t misc;

#if 0
	misc = pci_conf_read(pc, pa->pa_tag, BS_RTY_TOUT);
	misc = (misc & ~(UBS_PCI_RTY_MASK << UBS_PCI_RTY_SHIFT))
	    | ((UBS_DEF_RTY & 0xff) << UBS_PCI_RTY_SHIFT);
	misc = (misc & ~(UBS_PCI_TOUT_MASK << UBS_PCI_TOUT_SHIFT))
	    | ((UBS_DEF_TOUT & 0xff) << UBS_PCI_TOUT_SHIFT);
	pci_conf_write(pc, pa->pa_tag, BS_RTY_TOUT, misc);
#endif

	/*
	 * This will set the cache line size to 1, this will
	 * force the BCM58xx chip just to do burst read/writes.
	 * Cache line read/writes are to slow
	 */
	misc = pci_conf_read(pc, pa->pa_tag, PCI_BHLC_REG);
	misc = (misc & ~(PCI_CACHELINE_MASK << PCI_CACHELINE_SHIFT))
	    | ((UBS_DEF_CACHELINE & 0xff) << PCI_CACHELINE_SHIFT);
	pci_conf_write(pc, pa->pa_tag, PCI_BHLC_REG, misc);
}

/*
 * Clean up after a chip crash.
 * It is assumed that the caller in splnet()
 */
void
ubsec_cleanchip(sc)
	struct ubsec_softc *sc;
{
	struct ubsec_q *q;

	while (!SIMPLEQ_EMPTY(&sc->sc_qchip)) {
		q = SIMPLEQ_FIRST(&sc->sc_qchip);
		SIMPLEQ_REMOVE_HEAD(&sc->sc_qchip, q, q_next);
		ubsec_free_q(sc, q);
	}
}

/*
 * free a ubsec_q
 * It is assumed that the caller is within splnet()
 */
int
ubsec_free_q(struct ubsec_softc *sc, struct ubsec_q *q)
{
	struct ubsec_q *q2;
	struct cryptop *crp;
	int npkts;
	int i;

	npkts = q->q_nstacked_mcrs;

	for (i = 0; i < npkts; i++) {
		if(q->q_stacked_mcr[i]) {
			q2 = q->q_stacked_mcr[i];

			if ((q2->q_dst_m != NULL) && (q2->q_src_m != q2->q_dst_m)) 
				m_freem(q2->q_dst_m);

			crp = (struct cryptop *)q2->q_crp;
			
			SIMPLEQ_INSERT_TAIL(&sc->sc_freequeue, q2, q_next);
			
			crp->crp_etype = EFAULT;
			crp->crp_callback(crp);
		} else {
			break;
		}
	}

	/*
	 * Free header MCR
	 */
	if ((q->q_dst_m != NULL) && (q->q_src_m != q->q_dst_m))
		m_freem(q->q_dst_m);

	crp = (struct cryptop *)q->q_crp;
	
	SIMPLEQ_INSERT_TAIL(&sc->sc_freequeue, q, q_next);
	
	crp->crp_etype = EFAULT;
	crp->crp_callback(crp);
	return(0);
}

/*
 * Routine to reset the chip and clean up.
 * It is assumed that the caller is in splnet()
 */
void
ubsec_totalreset(sc)
	struct ubsec_softc *sc;
{
	ubsec_reset_board(sc);
	ubsec_init_board(sc);
	ubsec_cleanchip(sc);
}

int
ubsec_dmamap_aligned(map)
	bus_dmamap_t map;
{
	int i;

	for (i = 0; i < map->dm_nsegs; i++) {
		if (map->dm_segs[i].ds_addr & 3)
			return (0);
		if ((i != (map->dm_nsegs - 1)) &&
		    (map->dm_segs[i].ds_len & 3))
			return (0);
	}
	return (1);
}
