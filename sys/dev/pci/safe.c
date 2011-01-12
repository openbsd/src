/*	$OpenBSD: safe.c,v 1.31 2011/01/12 17:16:39 deraadt Exp $	*/

/*-
 * Copyright (c) 2003 Sam Leffler, Errno Consulting
 * Copyright (c) 2003 Global Technology Associates, Inc.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: /repoman/r/ncvs/src/sys/dev/safe/safe.c,v 1.1 2003/07/21 21:46:07 sam Exp $
 */

#include <sys/cdefs.h>

/*
 * SafeNet SafeXcel-1141 hardware crypto accelerator
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/device.h>
#include <sys/timeout.h>

#include <machine/bus.h>

#include <crypto/md5.h>
#include <crypto/sha1.h>
#include <crypto/cryptodev.h>
#include <crypto/cryptosoft.h>
#include <dev/rndvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/safereg.h>
#include <dev/pci/safevar.h>

#ifndef bswap32
#define	bswap32	NTOHL
#endif

#define	KASSERT_X(x,y)

/*
 * Prototypes and count for the pci_device structure
 */
int safe_probe(struct device *, void *, void *);
void safe_attach(struct device *, struct device *, void *);

struct cfattach safe_ca = {
	sizeof(struct safe_softc), safe_probe, safe_attach
};

struct cfdriver safe_cd = {
	0, "safe", DV_DULL
};

int safe_intr(void *);
int safe_newsession(u_int32_t *, struct cryptoini *);
int safe_freesession(u_int64_t);
int safe_process(struct cryptop *);
int safe_kprocess(struct cryptkop *);
int safe_kstart(struct safe_softc *);
void safe_kload_reg(struct safe_softc *, u_int32_t, u_int32_t,
    struct crparam *);
struct safe_softc *safe_kfind(struct cryptkop *);
void safe_kpoll(void *);
void safe_kfeed(struct safe_softc *);
int safe_ksigbits(struct crparam *cr);
void safe_callback(struct safe_softc *, struct safe_ringentry *);
void safe_feed(struct safe_softc *, struct safe_ringentry *);
void safe_mcopy(struct mbuf *, struct mbuf *, u_int);
void safe_rng_init(struct safe_softc *);
void safe_rng(void *);
int safe_dma_malloc(struct safe_softc *, bus_size_t,
	        struct safe_dma_alloc *, int);
#define	safe_dma_sync(_sc, _dma, _flags) \
	bus_dmamap_sync((_sc)->sc_dmat, (_dma)->dma_map, 0,		\
	    (_dma)->dma_map->dm_mapsize, (_flags))
void safe_dma_free(struct safe_softc *, struct safe_dma_alloc *);
int safe_dmamap_aligned(const struct safe_operand *);
int safe_dmamap_uniform(const struct safe_operand *);

void safe_reset_board(struct safe_softc *);
void safe_init_board(struct safe_softc *);
void safe_init_pciregs(struct safe_softc *);
void safe_cleanchip(struct safe_softc *);
static __inline u_int32_t safe_rng_read(struct safe_softc *);

int safe_free_entry(struct safe_softc *, struct safe_ringentry *);

#ifdef SAFE_DEBUG
int safe_debug;
#define	DPRINTF(_x)	if (safe_debug) printf _x

void safe_dump_dmastatus(struct safe_softc *, const char *);
void safe_dump_intrstate(struct safe_softc *, const char *);
void safe_dump_ringstate(struct safe_softc *, const char *);
void safe_dump_request(struct safe_softc *, const char *,
    struct safe_ringentry *);
void safe_dump_ring(struct safe_softc *sc, const char *tag);
#else
#define	DPRINTF(_x)
#endif

#define	READ_REG(sc,r) \
	bus_space_read_4((sc)->sc_st, (sc)->sc_sh, (r))

#define WRITE_REG(sc,reg,val) \
	bus_space_write_4((sc)->sc_st, (sc)->sc_sh, reg, val)

struct safe_stats safestats;

int safe_rnginterval = 1;		/* poll once a second */
int safe_rngbufsize = 16;		/* 64 bytes each poll  */
int safe_rngmaxalarm = 8;		/* max alarms before reset */

int
safe_probe(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SAFENET &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SAFENET_SAFEXCEL)
		return (1);
	return (0);
}

void
safe_attach(struct device *parent, struct device *self, void *aux)
{
	struct safe_softc *sc = (struct safe_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_size_t iosize;
	bus_addr_t raddr;
	u_int32_t devinfo;
	int algs[CRYPTO_ALGORITHM_MAX + 1], i;

	/* XXX handle power management */

	SIMPLEQ_INIT(&sc->sc_pkq);
	sc->sc_dmat = pa->pa_dmat;

	/* 
	 * Setup memory-mapping of PCI registers.
	 */
	if (pci_mapreg_map(pa, SAFE_BAR, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_st, &sc->sc_sh, NULL, &iosize, 0)) {
		printf(": can't map register space\n");
		goto bad;
	}

	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		goto bad1;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
	sc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_NET, safe_intr, sc,
	    self->dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto bad2;
	}

	sc->sc_cid = crypto_get_driverid(0);
	if (sc->sc_cid < 0) {
		printf(": could not get crypto driver id\n");
		goto bad3;
	}

	sc->sc_chiprev = READ_REG(sc, SAFE_DEVINFO) &
		(SAFE_DEVINFO_REV_MAJ | SAFE_DEVINFO_REV_MIN);

	/*
	 * Allocate packet engine descriptors.
	 */
	if (safe_dma_malloc(sc,
	    SAFE_MAX_NQUEUE * sizeof (struct safe_ringentry),
	    &sc->sc_ringalloc, 0)) {
		printf(": cannot allocate PE descriptor ring\n");
		goto bad4;
	}
	/*
	 * Hookup the static portion of all our data structures.
	 */
	sc->sc_ring = (struct safe_ringentry *) sc->sc_ringalloc.dma_vaddr;
	sc->sc_ringtop = sc->sc_ring + SAFE_MAX_NQUEUE;
	sc->sc_front = sc->sc_ring;
	sc->sc_back = sc->sc_ring;
	raddr = sc->sc_ringalloc.dma_paddr;
	bzero(sc->sc_ring, SAFE_MAX_NQUEUE * sizeof(struct safe_ringentry));
	for (i = 0; i < SAFE_MAX_NQUEUE; i++) {
		struct safe_ringentry *re = &sc->sc_ring[i];

		re->re_desc.d_sa = raddr +
			offsetof(struct safe_ringentry, re_sa);
		re->re_sa.sa_staterec = raddr +
			offsetof(struct safe_ringentry, re_sastate);

		raddr += sizeof (struct safe_ringentry);
	}

	/*
	 * Allocate scatter and gather particle descriptors.
	 */
	if (safe_dma_malloc(sc, SAFE_TOTAL_SPART * sizeof (struct safe_pdesc),
	    &sc->sc_spalloc, 0)) {
		printf(": cannot allocate source particle descriptor ring\n");
		safe_dma_free(sc, &sc->sc_ringalloc);
		goto bad4;
	}
	sc->sc_spring = (struct safe_pdesc *) sc->sc_spalloc.dma_vaddr;
	sc->sc_springtop = sc->sc_spring + SAFE_TOTAL_SPART;
	sc->sc_spfree = sc->sc_spring;
	bzero(sc->sc_spring, SAFE_TOTAL_SPART * sizeof(struct safe_pdesc));

	if (safe_dma_malloc(sc, SAFE_TOTAL_DPART * sizeof (struct safe_pdesc),
	    &sc->sc_dpalloc, 0)) {
		printf(": cannot allocate destination particle "
			"descriptor ring\n");
		safe_dma_free(sc, &sc->sc_spalloc);
		safe_dma_free(sc, &sc->sc_ringalloc);
		goto bad4;
	}
	sc->sc_dpring = (struct safe_pdesc *) sc->sc_dpalloc.dma_vaddr;
	sc->sc_dpringtop = sc->sc_dpring + SAFE_TOTAL_DPART;
	sc->sc_dpfree = sc->sc_dpring;
	bzero(sc->sc_dpring, SAFE_TOTAL_DPART * sizeof(struct safe_pdesc));

	printf(":");

	devinfo = READ_REG(sc, SAFE_DEVINFO);
	if (devinfo & SAFE_DEVINFO_RNG)
		printf(" RNG");

	bzero(algs, sizeof(algs));
	if (devinfo & SAFE_DEVINFO_PKEY) {
		printf(" PK");
		algs[CRK_MOD_EXP] = CRYPTO_ALG_FLAG_SUPPORTED;
		crypto_kregister(sc->sc_cid, algs, safe_kprocess);
		timeout_set(&sc->sc_pkto, safe_kpoll, sc);
	}

	bzero(algs, sizeof(algs));
	if (devinfo & SAFE_DEVINFO_DES) {
		printf(" 3DES");
		algs[CRYPTO_3DES_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;
		algs[CRYPTO_DES_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;
	}
	if (devinfo & SAFE_DEVINFO_AES) {
		printf(" AES");
		algs[CRYPTO_AES_CBC] = CRYPTO_ALG_FLAG_SUPPORTED;
	}
	if (devinfo & SAFE_DEVINFO_MD5) {
		printf(" MD5");
		algs[CRYPTO_MD5_HMAC] = CRYPTO_ALG_FLAG_SUPPORTED;
	}
	if (devinfo & SAFE_DEVINFO_SHA1) {
		printf(" SHA1");
		algs[CRYPTO_SHA1_HMAC] = CRYPTO_ALG_FLAG_SUPPORTED;
	}
	crypto_register(sc->sc_cid, algs, safe_newsession,
	    safe_freesession, safe_process);
	/* XXX other supported algorithms? */

	printf(", %s\n", intrstr);

	safe_reset_board(sc);		/* reset h/w */
	safe_init_pciregs(sc);		/* init pci settings */
	safe_init_board(sc);		/* init h/w */

	if (devinfo & SAFE_DEVINFO_RNG) {
		safe_rng_init(sc);

		timeout_set(&sc->sc_rngto, safe_rng, sc);
		timeout_add_sec(&sc->sc_rngto, safe_rnginterval);
	}
	return;

bad4:
	/* XXX crypto_unregister_all(sc->sc_cid); */
bad3:
	pci_intr_disestablish(pa->pa_pc, sc->sc_ih);
bad2:
	/* pci_intr_unmap? */;
bad1:
	bus_space_unmap(sc->sc_st, sc->sc_sh, iosize);
bad:
	return;
}

int
safe_process(struct cryptop *crp)
{
	int err = 0, i, nicealign, uniform, s;
	struct safe_softc *sc;
	struct cryptodesc *crd1, *crd2, *maccrd, *enccrd;
	int bypass, oplen, ivsize, card;
	int16_t coffset;
	struct safe_session *ses;
	struct safe_ringentry *re;
	struct safe_sarec *sa;
	struct safe_pdesc *pd;
	u_int32_t cmd0, cmd1, staterec, iv[4];

	s = splnet();
	if (crp == NULL || crp->crp_callback == NULL) {
		safestats.st_invalid++;
		splx(s);
		return (EINVAL);
	}
	card = SAFE_CARD(crp->crp_sid);
	if (card >= safe_cd.cd_ndevs || safe_cd.cd_devs[card] == NULL) {
		safestats.st_invalid++;
		splx(s);
		return (EINVAL);
	}
	sc = safe_cd.cd_devs[card];

	if (SAFE_SESSION(crp->crp_sid) >= sc->sc_nsessions) {
		safestats.st_badsession++;
		splx(s);
		return (EINVAL);
	}

	if (sc->sc_front == sc->sc_back && sc->sc_nqchip != 0) {
		safestats.st_ringfull++;
		splx(s);
		return (ERESTART);
	}
	re = sc->sc_front;

	staterec = re->re_sa.sa_staterec;	/* save */
	/* NB: zero everything but the PE descriptor */
	bzero(&re->re_sa, sizeof(struct safe_ringentry) - sizeof(re->re_desc));
	re->re_sa.sa_staterec = staterec;	/* restore */

	re->re_crp = crp;
	re->re_sesn = SAFE_SESSION(crp->crp_sid);

	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		re->re_src_m = (struct mbuf *)crp->crp_buf;
		re->re_dst_m = (struct mbuf *)crp->crp_buf;
	} else if (crp->crp_flags & CRYPTO_F_IOV) {
		re->re_src_io = (struct uio *)crp->crp_buf;
		re->re_dst_io = (struct uio *)crp->crp_buf;
	} else {
		safestats.st_badflags++;
		err = EINVAL;
		goto errout;	/* XXX we don't handle contiguous blocks! */
	}

	sa = &re->re_sa;
	ses = &sc->sc_sessions[re->re_sesn];

	crd1 = crp->crp_desc;
	if (crd1 == NULL) {
		safestats.st_nodesc++;
		err = EINVAL;
		goto errout;
	}
	crd2 = crd1->crd_next;

	cmd0 = SAFE_SA_CMD0_BASIC;		/* basic group operation */
	cmd1 = 0;
	if (crd2 == NULL) {
		if (crd1->crd_alg == CRYPTO_MD5_HMAC ||
		    crd1->crd_alg == CRYPTO_SHA1_HMAC) {
			maccrd = crd1;
			enccrd = NULL;
			cmd0 |= SAFE_SA_CMD0_OP_HASH;
		} else if (crd1->crd_alg == CRYPTO_DES_CBC ||
		    crd1->crd_alg == CRYPTO_3DES_CBC ||
		    crd1->crd_alg == CRYPTO_AES_CBC) {
			maccrd = NULL;
			enccrd = crd1;
			cmd0 |= SAFE_SA_CMD0_OP_CRYPT;
		} else {
			safestats.st_badalg++;
			err = EINVAL;
			goto errout;
		}
	} else {
		if ((crd1->crd_alg == CRYPTO_MD5_HMAC ||
		    crd1->crd_alg == CRYPTO_SHA1_HMAC) &&
		   (crd2->crd_alg == CRYPTO_DES_CBC ||
		    crd2->crd_alg == CRYPTO_3DES_CBC ||
		    crd2->crd_alg == CRYPTO_AES_CBC) &&
		    ((crd2->crd_flags & CRD_F_ENCRYPT) == 0)) {
			maccrd = crd1;
			enccrd = crd2;
		} else if ((crd1->crd_alg == CRYPTO_DES_CBC ||
		    crd1->crd_alg == CRYPTO_3DES_CBC ||
		    crd1->crd_alg == CRYPTO_AES_CBC) &&
		    (crd2->crd_alg == CRYPTO_MD5_HMAC ||
		     crd2->crd_alg == CRYPTO_SHA1_HMAC) &&
		    (crd1->crd_flags & CRD_F_ENCRYPT)) {
			enccrd = crd1;
			maccrd = crd2;
		} else {
			safestats.st_badalg++;
			err = EINVAL;
			goto errout;
		}
		cmd0 |= SAFE_SA_CMD0_OP_BOTH;
	}

	if (enccrd) {
		if (enccrd->crd_alg == CRYPTO_DES_CBC) {
			cmd0 |= SAFE_SA_CMD0_DES;
			cmd1 |= SAFE_SA_CMD1_CBC;
			ivsize = 2*sizeof(u_int32_t);
		} else if (enccrd->crd_alg == CRYPTO_3DES_CBC) {
			cmd0 |= SAFE_SA_CMD0_3DES;
			cmd1 |= SAFE_SA_CMD1_CBC;
			ivsize = 2*sizeof(u_int32_t);
		} else if (enccrd->crd_alg == CRYPTO_AES_CBC) {
			cmd0 |= SAFE_SA_CMD0_AES;
			cmd1 |= SAFE_SA_CMD1_CBC;
			if (ses->ses_klen == 128)
			     cmd1 |=  SAFE_SA_CMD1_AES128;
			else if (ses->ses_klen == 192)
			     cmd1 |=  SAFE_SA_CMD1_AES192;
			else
			     cmd1 |=  SAFE_SA_CMD1_AES256;
			ivsize = 4*sizeof(u_int32_t);
		} else {
			cmd0 |= SAFE_SA_CMD0_CRYPT_NULL;
			ivsize = 0;
		}

		/*
		 * Setup encrypt/decrypt state.  When using basic ops
		 * we can't use an inline IV because hash/crypt offset
		 * must be from the end of the IV to the start of the
		 * crypt data and this leaves out the preceding header
		 * from the hash calculation.  Instead we place the IV
		 * in the state record and set the hash/crypt offset to
		 * copy both the header+IV.
		 */
		if (enccrd->crd_flags & CRD_F_ENCRYPT) {
			cmd0 |= SAFE_SA_CMD0_OUTBOUND;

			if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
				bcopy(enccrd->crd_iv, iv, ivsize);
			else
				arc4random_buf(iv, ivsize);

			if ((enccrd->crd_flags & CRD_F_IV_PRESENT) == 0) {
				if (crp->crp_flags & CRYPTO_F_IMBUF)
					m_copyback(re->re_src_m,
					    enccrd->crd_inject, ivsize, iv,
					    M_NOWAIT);
				else if (crp->crp_flags & CRYPTO_F_IOV)
					cuio_copyback(re->re_src_io,
					    enccrd->crd_inject, ivsize, iv);
			}
			for (i = 0; i < ivsize / sizeof(iv[0]); i++)
				re->re_sastate.sa_saved_iv[i] = htole32(iv[i]);
			cmd0 |= SAFE_SA_CMD0_IVLD_STATE | SAFE_SA_CMD0_SAVEIV;
		} else {
			cmd0 |= SAFE_SA_CMD0_INBOUND;

			if (enccrd->crd_flags & CRD_F_IV_EXPLICIT)
				bcopy(enccrd->crd_iv, iv, ivsize);
			else if (crp->crp_flags & CRYPTO_F_IMBUF)
				m_copydata(re->re_src_m, enccrd->crd_inject,
				    ivsize, (caddr_t)iv);
			else if (crp->crp_flags & CRYPTO_F_IOV)
				cuio_copydata(re->re_src_io, enccrd->crd_inject,
				    ivsize, (caddr_t)iv);
			for (i = 0; i < ivsize / sizeof(iv[0]); i++)
				re->re_sastate.sa_saved_iv[i] = htole32(iv[i]);
			cmd0 |= SAFE_SA_CMD0_IVLD_STATE;
		}
		/*
		 * For basic encryption use the zero pad algorithm.
		 * This pads results to an 8-byte boundary and
		 * suppresses padding verification for inbound (i.e.
		 * decrypt) operations.
		 *
		 * NB: Not sure if the 8-byte pad boundary is a problem.
		 */
		cmd0 |= SAFE_SA_CMD0_PAD_ZERO;

		/* XXX assert key bufs have the same size */
		for (i = 0; i < sizeof(sa->sa_key)/sizeof(sa->sa_key[0]); i++)
			sa->sa_key[i] = ses->ses_key[i];
	}

	if (maccrd) {
		if (maccrd->crd_alg == CRYPTO_MD5_HMAC) {
			cmd0 |= SAFE_SA_CMD0_MD5;
			cmd1 |= SAFE_SA_CMD1_HMAC;	/* NB: enable HMAC */
		} else if (maccrd->crd_alg == CRYPTO_SHA1_HMAC) {
			cmd0 |= SAFE_SA_CMD0_SHA1;
			cmd1 |= SAFE_SA_CMD1_HMAC;	/* NB: enable HMAC */
		} else {
			cmd0 |= SAFE_SA_CMD0_HASH_NULL;
		}
		/*
		 * Digest data is loaded from the SA and the hash
		 * result is saved to the state block where we
		 * retrieve it for return to the caller.
		 */
		/* XXX assert digest bufs have the same size */
		for (i = 0;
		     i < sizeof(sa->sa_outdigest)/sizeof(sa->sa_outdigest[i]);
		     i++) {
			sa->sa_indigest[i] = ses->ses_hminner[i];
			sa->sa_outdigest[i] = ses->ses_hmouter[i];
		}

		cmd0 |= SAFE_SA_CMD0_HSLD_SA | SAFE_SA_CMD0_SAVEHASH;
		re->re_flags |= SAFE_QFLAGS_COPYOUTICV;
	}

	if (enccrd && maccrd) {
		/*
		 * The offset from hash data to the start of
		 * crypt data is the difference in the skips.
		 */
		bypass = maccrd->crd_skip;
		coffset = enccrd->crd_skip - maccrd->crd_skip;
		if (coffset < 0) {
			DPRINTF(("%s: hash does not precede crypt; "
				"mac skip %u enc skip %u\n",
				__func__, maccrd->crd_skip, enccrd->crd_skip));
			safestats.st_skipmismatch++;
			err = EINVAL;
			goto errout;
		}
		oplen = enccrd->crd_skip + enccrd->crd_len;
		if (maccrd->crd_skip + maccrd->crd_len != oplen) {
			DPRINTF(("%s: hash amount %u != crypt amount %u\n",
				__func__, maccrd->crd_skip + maccrd->crd_len,
				oplen));
			safestats.st_lenmismatch++;
			err = EINVAL;
			goto errout;
		}
#ifdef SAFE_DEBUG
		if (safe_debug) {
			printf("mac: skip %d, len %d, inject %d\n",
			    maccrd->crd_skip, maccrd->crd_len,
			    maccrd->crd_inject);
			printf("enc: skip %d, len %d, inject %d\n",
			    enccrd->crd_skip, enccrd->crd_len,
			    enccrd->crd_inject);
			printf("bypass %d coffset %d oplen %d\n",
				bypass, coffset, oplen);
		}
#endif
		if (coffset & 3) {	/* offset must be 32-bit aligned */
			DPRINTF(("%s: coffset %u misaligned\n",
				__func__, coffset));
			safestats.st_coffmisaligned++;
			err = EINVAL;
			goto errout;
		}
		coffset >>= 2;
		if (coffset > 255) {	/* offset must be <256 dwords */
			DPRINTF(("%s: coffset %u too big\n",
				__func__, coffset));
			safestats.st_cofftoobig++;
			err = EINVAL;
			goto errout;
		}
		/*
		 * Tell the hardware to copy the header to the output.
		 * The header is defined as the data from the end of
		 * the bypass to the start of data to be encrypted. 
		 * Typically this is the inline IV.  Note that you need
		 * to do this even if src+dst are the same; it appears
		 * that w/o this bit the crypted data is written
		 * immediately after the bypass data.
		 */
		cmd1 |= SAFE_SA_CMD1_HDRCOPY;
		/*
		 * Disable IP header mutable bit handling.  This is
		 * needed to get correct HMAC calculations.
		 */
		cmd1 |= SAFE_SA_CMD1_MUTABLE;
	} else {
		if (enccrd) {
			bypass = enccrd->crd_skip;
			oplen = bypass + enccrd->crd_len;
		} else {
			bypass = maccrd->crd_skip;
			oplen = bypass + maccrd->crd_len;
		}
		coffset = 0;
	}
	/* XXX verify multiple of 4 when using s/g */
	if (bypass > 96) {		/* bypass offset must be <= 96 bytes */
		DPRINTF(("%s: bypass %u too big\n", __func__, bypass));
		safestats.st_bypasstoobig++;
		err = EINVAL;
		goto errout;
	}

	if (bus_dmamap_create(sc->sc_dmat, SAFE_MAX_DMA, SAFE_MAX_PART,
	    SAFE_MAX_DSIZE, SAFE_MAX_DSIZE, BUS_DMA_ALLOCNOW | BUS_DMA_NOWAIT,
	    &re->re_src_map)) {
		safestats.st_nomap++;
		err = ENOMEM;
		goto errout;
	}
	if (crp->crp_flags & CRYPTO_F_IMBUF) {
		if (bus_dmamap_load_mbuf(sc->sc_dmat, re->re_src_map,
		    re->re_src_m, BUS_DMA_NOWAIT)) {
			bus_dmamap_destroy(sc->sc_dmat, re->re_src_map);
			re->re_src_map = NULL;
			safestats.st_noload++;
			err = ENOMEM;
			goto errout;
		}
	} else if (crp->crp_flags & CRYPTO_F_IOV) {
		if (bus_dmamap_load_uio(sc->sc_dmat, re->re_src_map,
		    re->re_src_io, BUS_DMA_NOWAIT) != 0) {
			bus_dmamap_destroy(sc->sc_dmat, re->re_src_map);
			re->re_src_map = NULL;
			safestats.st_noload++;
			err = ENOMEM;
			goto errout;
		}
	}
	nicealign = safe_dmamap_aligned(&re->re_src);
	uniform = safe_dmamap_uniform(&re->re_src);

	DPRINTF(("src nicealign %u uniform %u nsegs %u\n",
		nicealign, uniform, re->re_src_nsegs));
	if (re->re_src_nsegs > 1) {
		re->re_desc.d_src = sc->sc_spalloc.dma_paddr +
			((caddr_t) sc->sc_spfree - (caddr_t) sc->sc_spring);
		for (i = 0; i < re->re_src_nsegs; i++) {
			/* NB: no need to check if there's space */
			pd = sc->sc_spfree;
			if (++(sc->sc_spfree) == sc->sc_springtop)
				sc->sc_spfree = sc->sc_spring;

			KASSERT_X((pd->pd_flags&3) == 0 ||
				(pd->pd_flags&3) == SAFE_PD_DONE,
				("bogus source particle descriptor; flags %x",
				pd->pd_flags));
			pd->pd_addr = re->re_src_segs[i].ds_addr;
			pd->pd_ctrl = SAFE_PD_READY |
			    ((re->re_src_segs[i].ds_len << SAFE_PD_LEN_S)
			    & SAFE_PD_LEN_M);
		}
		cmd0 |= SAFE_SA_CMD0_IGATHER;
	} else {
		/*
		 * No need for gather, reference the operand directly.
		 */
		re->re_desc.d_src = re->re_src_segs[0].ds_addr;
	}

	if (enccrd == NULL && maccrd != NULL) {
		/*
		 * Hash op; no destination needed.
		 */
	} else {
		if (crp->crp_flags & CRYPTO_F_IOV) {
			if (!nicealign) {
				safestats.st_iovmisaligned++;
				err = EINVAL;
				goto errout;
			}
			if (uniform != 1) {
				/*
				 * Source is not suitable for direct use as
				 * the destination.  Create a new scatter/gather
				 * list based on the destination requirements
				 * and check if that's ok.
				 */
				if (bus_dmamap_create(sc->sc_dmat,
				    SAFE_MAX_DMA, SAFE_MAX_PART,
				    SAFE_MAX_DSIZE, SAFE_MAX_DSIZE,
				    BUS_DMA_ALLOCNOW | BUS_DMA_NOWAIT,
				    &re->re_dst_map)) {
					safestats.st_nomap++;
					err = ENOMEM;
					goto errout;
				}
				if (bus_dmamap_load_uio(sc->sc_dmat,
				    re->re_dst_map, re->re_dst_io,
				    BUS_DMA_NOWAIT) != 0) {
					bus_dmamap_destroy(sc->sc_dmat,
						re->re_dst_map);
					re->re_dst_map = NULL;
					safestats.st_noload++;
					err = ENOMEM;
					goto errout;
				}
				uniform = safe_dmamap_uniform(&re->re_dst);
				if (!uniform) {
					/*
					 * There's no way to handle the DMA
					 * requirements with this uio.  We
					 * could create a separate DMA area for
					 * the result and then copy it back,
					 * but for now we just bail and return
					 * an error.  Note that uio requests
					 * > SAFE_MAX_DSIZE are handled because
					 * the DMA map and segment list for the
					 * destination will result in a
					 * destination particle list that does
					 * the necessary scatter DMA.
					 */ 
					safestats.st_iovnotuniform++;
					err = EINVAL;
					goto errout;
				}
			} else
				re->re_dst = re->re_src;
		} else if (crp->crp_flags & CRYPTO_F_IMBUF) {
			if (nicealign && uniform == 1) {
				/*
				 * Source layout is suitable for direct
				 * sharing of the DMA map and segment list.
				 */
				re->re_dst = re->re_src;
			} else if (nicealign && uniform == 2) {
				/*
				 * The source is properly aligned but requires a
				 * different particle list to handle DMA of the
				 * result.  Create a new map and do the load to
				 * create the segment list.  The particle
				 * descriptor setup code below will handle the
				 * rest.
				 */
				if (bus_dmamap_create(sc->sc_dmat,
				    SAFE_MAX_DMA, SAFE_MAX_PART,
				    SAFE_MAX_DSIZE, SAFE_MAX_DSIZE,
				    BUS_DMA_ALLOCNOW | BUS_DMA_NOWAIT,
				    &re->re_dst_map)) {
					safestats.st_nomap++;
					err = ENOMEM;
					goto errout;
				}
				if (bus_dmamap_load_mbuf(sc->sc_dmat,
				    re->re_dst_map, re->re_dst_m,
				    BUS_DMA_NOWAIT) != 0) {
					bus_dmamap_destroy(sc->sc_dmat,
						re->re_dst_map);
					re->re_dst_map = NULL;
					safestats.st_noload++;
					err = ENOMEM;
					goto errout;
				}
			} else {		/* !(aligned and/or uniform) */
				int totlen, len;
				struct mbuf *m, *top, **mp;

				/*
				 * DMA constraints require that we allocate a
				 * new mbuf chain for the destination.  We
				 * allocate an entire new set of mbufs of
				 * optimal/required size and then tell the
				 * hardware to copy any bits that are not
				 * created as a byproduct of the operation.
				 */
				if (!nicealign)
					safestats.st_unaligned++;
				if (!uniform)
					safestats.st_notuniform++;
				totlen = re->re_src_mapsize;
				if (re->re_src_m->m_flags & M_PKTHDR) {
					len = MHLEN;
					MGETHDR(m, M_DONTWAIT, MT_DATA);
				} else {
					len = MLEN;
					MGET(m, M_DONTWAIT, MT_DATA);
				}
				if (m == NULL) {
					safestats.st_nombuf++;
					err = sc->sc_nqchip ? ERESTART : ENOMEM;
					goto errout;
				}
				if (len == MHLEN) {
					err = m_dup_pkthdr(m, re->re_src_m);
					if (err) {
						m_free(m);
						goto errout;
					}
				}
				if (totlen >= MINCLSIZE) {
					MCLGET(m, M_DONTWAIT);
					if ((m->m_flags & M_EXT) == 0) {
						m_free(m);
						safestats.st_nomcl++;
						err = sc->sc_nqchip ?
							ERESTART : ENOMEM;
						goto errout;
					}
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
							safestats.st_nombuf++;
							err = sc->sc_nqchip ?
							    ERESTART : ENOMEM;
							goto errout;
						}
						len = MLEN;
					}
					if (top && totlen >= MINCLSIZE) {
						MCLGET(m, M_DONTWAIT);
						if ((m->m_flags & M_EXT) == 0) {
							*mp = m;
							m_freem(top);
							safestats.st_nomcl++;
							err = sc->sc_nqchip ?
							    ERESTART : ENOMEM;
							goto errout;
						}
						len = MCLBYTES;
					}
					m->m_len = len = min(totlen, len);
					totlen -= len;
					*mp = m;
					mp = &m->m_next;
				}
				re->re_dst_m = top;
				if (bus_dmamap_create(sc->sc_dmat, 
				    SAFE_MAX_DMA, SAFE_MAX_PART,
				    SAFE_MAX_DSIZE, SAFE_MAX_DSIZE,
				    BUS_DMA_ALLOCNOW | BUS_DMA_NOWAIT,
				    &re->re_dst_map) != 0) {
					safestats.st_nomap++;
					err = ENOMEM;
					goto errout;
				}
				if (bus_dmamap_load_mbuf(sc->sc_dmat,
				    re->re_dst_map, re->re_dst_m,
				    BUS_DMA_NOWAIT) != 0) {
					bus_dmamap_destroy(sc->sc_dmat,
					re->re_dst_map);
					re->re_dst_map = NULL;
					safestats.st_noload++;
					err = ENOMEM;
					goto errout;
				}
				if (re->re_src_mapsize > oplen) {
					/*
					 * There's data following what the
					 * hardware will copy for us.  If this
					 * isn't just the ICV (that's going to
					 * be written on completion), copy it
					 * to the new mbufs
					 */
					if (!(maccrd &&
					    (re->re_src_mapsize-oplen) == 12 &&
					    maccrd->crd_inject == oplen))
						safe_mcopy(re->re_src_m,
							   re->re_dst_m,
							   oplen);
					else
						safestats.st_noicvcopy++;
				}
			}
		} else {
			safestats.st_badflags++;
			err = EINVAL;
			goto errout;
		}

		if (re->re_dst_nsegs > 1) {
			re->re_desc.d_dst = sc->sc_dpalloc.dma_paddr +
			    ((caddr_t) sc->sc_dpfree - (caddr_t) sc->sc_dpring);
			for (i = 0; i < re->re_dst_nsegs; i++) {
				pd = sc->sc_dpfree;
				KASSERT_X((pd->pd_flags&3) == 0 ||
					(pd->pd_flags&3) == SAFE_PD_DONE,
					("bogus dest particle descriptor; flags %x",
						pd->pd_flags));
				if (++(sc->sc_dpfree) == sc->sc_dpringtop)
					sc->sc_dpfree = sc->sc_dpring;
				pd->pd_addr = re->re_dst_segs[i].ds_addr;
				pd->pd_ctrl = SAFE_PD_READY;
			}
			cmd0 |= SAFE_SA_CMD0_OSCATTER;
		} else {
			/*
			 * No need for scatter, reference the operand directly.
			 */
			re->re_desc.d_dst = re->re_dst_segs[0].ds_addr;
		}
	}

	/*
	 * All done with setup; fillin the SA command words
	 * and the packet engine descriptor.  The operation
	 * is now ready for submission to the hardware.
	 */
	sa->sa_cmd0 = cmd0 | SAFE_SA_CMD0_IPCI | SAFE_SA_CMD0_OPCI;
	sa->sa_cmd1 = cmd1
	    | (coffset << SAFE_SA_CMD1_OFFSET_S)
	    | SAFE_SA_CMD1_SAREV1	/* Rev 1 SA data structure */
	    | SAFE_SA_CMD1_SRPCI;

	/*
	 * NB: the order of writes is important here.  In case the
	 * chip is scanning the ring because of an outstanding request
	 * it might nab this one too.  In that case we need to make
	 * sure the setup is complete before we write the length
	 * field of the descriptor as it signals the descriptor is
	 * ready for processing.
	 */
	re->re_desc.d_csr = SAFE_PE_CSR_READY | SAFE_PE_CSR_SAPCI;
	if (maccrd)
		re->re_desc.d_csr |= SAFE_PE_CSR_LOADSA | SAFE_PE_CSR_HASHFINAL;
	re->re_desc.d_len = oplen
			  | SAFE_PE_LEN_READY
			  | (bypass << SAFE_PE_LEN_BYPASS_S)
			  ;

	safestats.st_ipackets++;
	safestats.st_ibytes += oplen;

	if (++(sc->sc_front) == sc->sc_ringtop)
		sc->sc_front = sc->sc_ring;

	/* XXX honor batching */
	safe_feed(sc, re);
	splx(s);
	return (0);

errout:
	if ((re->re_dst_m != NULL) && (re->re_src_m != re->re_dst_m))
		m_freem(re->re_dst_m);

	if (re->re_dst_map != NULL && re->re_dst_map != re->re_src_map) {
		bus_dmamap_unload(sc->sc_dmat, re->re_dst_map);
		bus_dmamap_destroy(sc->sc_dmat, re->re_dst_map);
	}
	if (re->re_src_map != NULL) {
		bus_dmamap_unload(sc->sc_dmat, re->re_src_map);
		bus_dmamap_destroy(sc->sc_dmat, re->re_src_map);
	}
	crp->crp_etype = err;
	crypto_done(crp);
	splx(s);
	return (err);
}

/*
 * Resets the board.  Values in the regesters are left as is
 * from the reset (i.e. initial values are assigned elsewhere).
 */
void
safe_reset_board(struct safe_softc *sc)
{
	u_int32_t v;

	/*
	 * Reset the device.  The manual says no delay
	 * is needed between marking and clearing reset.
	 */
	v = READ_REG(sc, SAFE_PE_DMACFG) &
	    ~(SAFE_PE_DMACFG_PERESET | SAFE_PE_DMACFG_PDRRESET |
	    SAFE_PE_DMACFG_SGRESET);
	WRITE_REG(sc, SAFE_PE_DMACFG, v
	    | SAFE_PE_DMACFG_PERESET
	    | SAFE_PE_DMACFG_PDRRESET
	    | SAFE_PE_DMACFG_SGRESET);
	WRITE_REG(sc, SAFE_PE_DMACFG, v);
}

/*
 * Initialize registers we need to touch only once.
 */
void
safe_init_board(struct safe_softc *sc)
{
	u_int32_t v, dwords;

	v = READ_REG(sc, SAFE_PE_DMACFG);
	v &= ~(SAFE_PE_DMACFG_PEMODE | SAFE_PE_DMACFG_ESPACKET);
	v |= SAFE_PE_DMACFG_FSENA		/* failsafe enable */
	  |  SAFE_PE_DMACFG_GPRPCI		/* gather ring on PCI */
	  |  SAFE_PE_DMACFG_SPRPCI		/* scatter ring on PCI */
	  |  SAFE_PE_DMACFG_ESDESC		/* endian-swap descriptors */
	  |  SAFE_PE_DMACFG_ESPDESC		/* endian-swap part. desc's */
	  |  SAFE_PE_DMACFG_ESSA		/* endian-swap SA data */
	  ;
	WRITE_REG(sc, SAFE_PE_DMACFG, v);

	WRITE_REG(sc, SAFE_CRYPTO_CTRL, SAFE_CRYPTO_CTRL_PKEY |
	    SAFE_CRYPTO_CTRL_3DES | SAFE_CRYPTO_CTRL_RNG);

#if BYTE_ORDER == LITTLE_ENDIAN
	WRITE_REG(sc, SAFE_ENDIAN, SAFE_ENDIAN_TGT_PASS|SAFE_ENDIAN_DMA_PASS);
#elif BYTE_ORDER == BIG_ENDIAN
	WRITE_REG(sc, SAFE_ENDIAN, SAFE_ENDIAN_TGT_PASS|SAFE_ENDIAN_DMA_SWAB);
#endif

	if (sc->sc_chiprev == SAFE_REV(1,0)) {
		/*
		 * Avoid large PCI DMA transfers.  Rev 1.0 has a bug where
		 * "target mode transfers" done while the chip is DMA'ing
		 * >1020 bytes cause the hardware to lockup.  To avoid this
		 * we reduce the max PCI transfer size and use small source
		 * particle descriptors (<= 256 bytes).
		 */
		WRITE_REG(sc, SAFE_DMA_CFG, 256);
		printf("%s: Reduce max DMA size to %u words for rev %u.%u WAR\n",
		    sc->sc_dev.dv_xname,
		    (READ_REG(sc, SAFE_DMA_CFG)>>2) & 0xff,
		    SAFE_REV_MAJ(sc->sc_chiprev),
		    SAFE_REV_MIN(sc->sc_chiprev));
	}

	/* NB: operands+results are overlaid */
	WRITE_REG(sc, SAFE_PE_PDRBASE, sc->sc_ringalloc.dma_paddr);
	WRITE_REG(sc, SAFE_PE_RDRBASE, sc->sc_ringalloc.dma_paddr);
	/*
	 * Configure ring entry size and number of items in the ring.
	 */
	KASSERT_X((sizeof(struct safe_ringentry) % sizeof(u_int32_t)) == 0,
	    ("PE ring entry not 32-bit aligned!"));
	dwords = sizeof(struct safe_ringentry) / sizeof(u_int32_t);
	WRITE_REG(sc, SAFE_PE_RINGCFG,
	    (dwords << SAFE_PE_RINGCFG_OFFSET_S) | SAFE_MAX_NQUEUE);
	WRITE_REG(sc, SAFE_PE_RINGPOLL, 0);	/* disable polling */

	WRITE_REG(sc, SAFE_PE_GRNGBASE, sc->sc_spalloc.dma_paddr);
	WRITE_REG(sc, SAFE_PE_SRNGBASE, sc->sc_dpalloc.dma_paddr);
	WRITE_REG(sc, SAFE_PE_PARTSIZE,
	    (SAFE_TOTAL_DPART<<16) | SAFE_TOTAL_SPART);
	/*
	 * NB: destination particles are fixed size.  We use
	 *     an mbuf cluster and require all results go to
	 *     clusters or smaller.
	 */
	WRITE_REG(sc, SAFE_PE_PARTCFG, SAFE_MAX_DSIZE);

	WRITE_REG(sc, SAFE_HI_CLR, SAFE_INT_PE_CDONE | SAFE_INT_PE_DDONE |
	    SAFE_INT_PE_ERROR | SAFE_INT_PE_ODONE);

	/* it's now safe to enable PE mode, do it */
	WRITE_REG(sc, SAFE_PE_DMACFG, v | SAFE_PE_DMACFG_PEMODE);

	/*
	 * Configure hardware to use level-triggered interrupts and
	 * to interrupt after each descriptor is processed.
	 */
	DELAY(1000);
	WRITE_REG(sc, SAFE_HI_CFG, SAFE_HI_CFG_LEVEL);
	DELAY(1000);
	WRITE_REG(sc, SAFE_HI_MASK, SAFE_INT_PE_DDONE | SAFE_INT_PE_ERROR);
	DELAY(1000);
	WRITE_REG(sc, SAFE_HI_DESC_CNT, 1);
	DELAY(1000);
}

/*
 * Init PCI registers
 */
void
safe_init_pciregs(struct safe_softc *sc)
{
}

int
safe_dma_malloc(struct safe_softc *sc, bus_size_t size,
    struct safe_dma_alloc *dma,	int mapflags)
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
safe_dma_free(struct safe_softc *sc, struct safe_dma_alloc *dma)
{
	bus_dmamap_unload(sc->sc_dmat, dma->dma_map);
	bus_dmamem_unmap(sc->sc_dmat, dma->dma_vaddr, dma->dma_size);
	bus_dmamem_free(sc->sc_dmat, &dma->dma_seg, dma->dma_nseg);
	bus_dmamap_destroy(sc->sc_dmat, dma->dma_map);
}


#define	SAFE_RNG_MAXWAIT	1000

void
safe_rng_init(struct safe_softc *sc)
{
	u_int32_t w, v;
	int i;

	WRITE_REG(sc, SAFE_RNG_CTRL, 0);
	/* use default value according to the manual */
	WRITE_REG(sc, SAFE_RNG_CNFG, 0x834);	/* magic from SafeNet */
	WRITE_REG(sc, SAFE_RNG_ALM_CNT, 0);

	/*
	 * There is a bug in rev 1.0 of the 1140 that when the RNG
	 * is brought out of reset the ready status flag does not
	 * work until the RNG has finished its internal initialization.
	 *
	 * So in order to determine the device is through its
	 * initialization we must read the data register, using the
	 * status reg in the read in case it is initialized.  Then read
	 * the data register until it changes from the first read.
	 * Once it changes read the data register until it changes
	 * again.  At this time the RNG is considered initialized. 
	 * This could take between 750ms - 1000ms in time.
	 */
	i = 0;
	w = READ_REG(sc, SAFE_RNG_OUT);
	do {
		v = READ_REG(sc, SAFE_RNG_OUT);
		if (v != w) {
			w = v;
			break;
		}
		DELAY(10);
	} while (++i < SAFE_RNG_MAXWAIT);

	/* Wait Until data changes again */
	i = 0;
	do {
		v = READ_REG(sc, SAFE_RNG_OUT);
		if (v != w)
			break;
		DELAY(10);
	} while (++i < SAFE_RNG_MAXWAIT);
}

static __inline u_int32_t
safe_rng_read(struct safe_softc *sc)
{
	int i;

	i = 0;
	while (READ_REG(sc, SAFE_RNG_STAT) != 0 && ++i < SAFE_RNG_MAXWAIT)
		;
	return (READ_REG(sc, SAFE_RNG_OUT));
}

void
safe_rng(void *arg)
{
	struct safe_softc *sc = arg;
	u_int32_t buf[SAFE_RNG_MAXBUFSIZ];	/* NB: maybe move to softc */
	u_int maxwords;
	int i;

	safestats.st_rng++;
	/*
	 * Fetch the next block of data.
	 */
	maxwords = safe_rngbufsize;
	if (maxwords > SAFE_RNG_MAXBUFSIZ)
		maxwords = SAFE_RNG_MAXBUFSIZ;
retry:
	for (i = 0; i < maxwords; i++)
		buf[i] = safe_rng_read(sc);
	/*
	 * Check the comparator alarm count and reset the h/w if
	 * it exceeds our threshold.  This guards against the
	 * hardware oscillators resonating with external signals.
	 */
	if (READ_REG(sc, SAFE_RNG_ALM_CNT) > safe_rngmaxalarm) {
		u_int32_t freq_inc, w;

		DPRINTF(("%s: alarm count %u exceeds threshold %u\n", __func__,
			READ_REG(sc, SAFE_RNG_ALM_CNT), safe_rngmaxalarm));
		safestats.st_rngalarm++;
		WRITE_REG(sc, SAFE_RNG_CTRL, 
		    READ_REG(sc, SAFE_RNG_CTRL) | SAFE_RNG_CTRL_SHORTEN);
		freq_inc = 18;
		for (i = 0; i < 64; i++) {
			w = READ_REG(sc, SAFE_RNG_CNFG);
			freq_inc = ((w + freq_inc) & 0x3fL);
			w = ((w & ~0x3fL) | freq_inc);
			WRITE_REG(sc, SAFE_RNG_CNFG, w);

			WRITE_REG(sc, SAFE_RNG_ALM_CNT, 0);

			(void) safe_rng_read(sc);
			DELAY(25);

			if (READ_REG(sc, SAFE_RNG_ALM_CNT) == 0) {
				WRITE_REG(sc, SAFE_RNG_CTRL,
				    READ_REG(sc, SAFE_RNG_CTRL) &
				    ~SAFE_RNG_CTRL_SHORTEN);
				goto retry;
			}
			freq_inc = 1;
		}
		WRITE_REG(sc, SAFE_RNG_CTRL,
		    READ_REG(sc, SAFE_RNG_CTRL) & ~SAFE_RNG_CTRL_SHORTEN);
	} else
		WRITE_REG(sc, SAFE_RNG_ALM_CNT, 0);

	for (i = 0; i < maxwords; i++)
		add_true_randomness(buf[i]);

	timeout_add_sec(&sc->sc_rngto, safe_rnginterval);
}

/*
 * Allocate a new 'session' and return an encoded session id.  'sidp'
 * contains our registration id, and should contain an encoded session
 * id on successful allocation.
 */
int
safe_newsession(u_int32_t *sidp, struct cryptoini *cri)
{
	struct cryptoini *c, *encini = NULL, *macini = NULL;
	struct safe_softc *sc = NULL;
	struct safe_session *ses = NULL;
	MD5_CTX md5ctx;
	SHA1_CTX sha1ctx;
	int i, sesn;

	if (sidp == NULL || cri == NULL)
		return (EINVAL);
	for (i = 0; i < safe_cd.cd_ndevs; i++) {
		sc = safe_cd.cd_devs[i];
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
		    c->cri_alg == CRYPTO_3DES_CBC ||
		    c->cri_alg == CRYPTO_AES_CBC) {
			if (encini)
				return (EINVAL);
			encini = c;
		} else
			return (EINVAL);
	}
	if (encini == NULL && macini == NULL)
		return (EINVAL);
	if (encini) {			/* validate key length */
		switch (encini->cri_alg) {
		case CRYPTO_DES_CBC:
			if (encini->cri_klen != 64)
				return (EINVAL);
			break;
		case CRYPTO_3DES_CBC:
			if (encini->cri_klen != 192)
				return (EINVAL);
			break;
		case CRYPTO_AES_CBC:
			if (encini->cri_klen != 128 &&
			    encini->cri_klen != 192 &&
			    encini->cri_klen != 256)
				return (EINVAL);
			break;
		}
	}

	if (sc->sc_sessions == NULL) {
		ses = sc->sc_sessions = (struct safe_session *)malloc(
		    sizeof(struct safe_session), M_DEVBUF, M_NOWAIT);
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
			ses = (struct safe_session *)malloc((sesn + 1) *
			    sizeof(struct safe_session), M_DEVBUF, M_NOWAIT);
			if (ses == NULL)
				return (ENOMEM);
			bcopy(sc->sc_sessions, ses, sesn *
			    sizeof(struct safe_session));
			explicit_bzero(sc->sc_sessions, sesn *
			    sizeof(struct safe_session));
			free(sc->sc_sessions, M_DEVBUF);
			sc->sc_sessions = ses;
			ses = &sc->sc_sessions[sesn];
			sc->sc_nsessions++;
		}
	}

	bzero(ses, sizeof(struct safe_session));
	ses->ses_used = 1;

	if (encini) {
		ses->ses_klen = encini->cri_klen;
		bcopy(encini->cri_key, ses->ses_key, ses->ses_klen / 8);

		for (i = 0;
		     i < sizeof(ses->ses_key)/sizeof(ses->ses_key[0]); i++)
			ses->ses_key[i] = htole32(ses->ses_key[i]);
	}

	if (macini) {
		for (i = 0; i < macini->cri_klen / 8; i++)
			macini->cri_key[i] ^= HMAC_IPAD_VAL;

		if (macini->cri_alg == CRYPTO_MD5_HMAC) {
			MD5Init(&md5ctx);
			MD5Update(&md5ctx, macini->cri_key,
			    macini->cri_klen / 8);
			MD5Update(&md5ctx, hmac_ipad_buffer,
			    HMAC_MD5_BLOCK_LEN - (macini->cri_klen / 8));
			bcopy(md5ctx.state, ses->ses_hminner,
			    sizeof(md5ctx.state));
		} else {
			SHA1Init(&sha1ctx);
			SHA1Update(&sha1ctx, macini->cri_key,
			    macini->cri_klen / 8);
			SHA1Update(&sha1ctx, hmac_ipad_buffer,
			    HMAC_SHA1_BLOCK_LEN - (macini->cri_klen / 8));
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
			    HMAC_MD5_BLOCK_LEN - (macini->cri_klen / 8));
			bcopy(md5ctx.state, ses->ses_hmouter,
			    sizeof(md5ctx.state));
		} else {
			SHA1Init(&sha1ctx);
			SHA1Update(&sha1ctx, macini->cri_key,
			    macini->cri_klen / 8);
			SHA1Update(&sha1ctx, hmac_opad_buffer,
			    HMAC_SHA1_BLOCK_LEN - (macini->cri_klen / 8));
			bcopy(sha1ctx.state, ses->ses_hmouter,
			    sizeof(sha1ctx.state));
		}

		for (i = 0; i < macini->cri_klen / 8; i++)
			macini->cri_key[i] ^= HMAC_OPAD_VAL;

		/* PE is little-endian, insure proper byte order */
		for (i = 0;
		     i < sizeof(ses->ses_hminner)/sizeof(ses->ses_hminner[0]);
		     i++) {
			ses->ses_hminner[i] = htole32(ses->ses_hminner[i]);
			ses->ses_hmouter[i] = htole32(ses->ses_hmouter[i]);
		}
	}

	*sidp = SAFE_SID(sc->sc_dev.dv_unit, sesn);
	return (0);
}

/*
 * Deallocate a session.
 */
int
safe_freesession(u_int64_t tid)
{
	struct safe_softc *sc;
	int session, ret, card;
	u_int32_t sid = ((u_int32_t) tid) & 0xffffffff;

	card = SAFE_CARD(sid);
	if (card >= safe_cd.cd_ndevs || safe_cd.cd_devs[card] == NULL)
		return (EINVAL);
	sc = safe_cd.cd_devs[card];

	if (sc == NULL)
		return (EINVAL);

	session = SAFE_SESSION(sid);
	if (session < sc->sc_nsessions) {
		explicit_bzero(&sc->sc_sessions[session],
		    sizeof(sc->sc_sessions[session]));
		ret = 0;
	} else
		ret = EINVAL;
	return (ret);
}

/*
 * Is the operand suitable aligned for direct DMA.  Each
 * segment must be aligned on a 32-bit boundary and all
 * but the last segment must be a multiple of 4 bytes.
 */
int
safe_dmamap_aligned(const struct safe_operand *op)
{
	int i;

	for (i = 0; i < op->map->dm_nsegs; i++) {
		if (op->map->dm_segs[i].ds_addr & 3)
			return (0);
		if (i != (op->map->dm_nsegs - 1) &&
		    (op->map->dm_segs[i].ds_len & 3))
			return (0);
	}
	return (1);
}

/*
 * Clean up after a chip crash.
 * It is assumed that the caller in splnet()
 */
void
safe_cleanchip(struct safe_softc *sc)
{

	if (sc->sc_nqchip != 0) {
		struct safe_ringentry *re = sc->sc_back;

		while (re != sc->sc_front) {
			if (re->re_desc.d_csr != 0)
				safe_free_entry(sc, re);
			if (++re == sc->sc_ringtop)
				re = sc->sc_ring;
		}
		sc->sc_back = re;
		sc->sc_nqchip = 0;
	}
}

/*
 * free a safe_q
 * It is assumed that the caller is within splnet().
 */
int
safe_free_entry(struct safe_softc *sc, struct safe_ringentry *re)
{
	struct cryptop *crp;

	/*
	 * Free header MCR
	 */
	if ((re->re_dst_m != NULL) && (re->re_src_m != re->re_dst_m))
		m_freem(re->re_dst_m);

	crp = (struct cryptop *)re->re_crp;
	
	re->re_desc.d_csr = 0;
	
	crp->crp_etype = EFAULT;
	crypto_done(crp);
	return (0);
}

/*
 * safe_feed() - post a request to chip
 */
void
safe_feed(struct safe_softc *sc, struct safe_ringentry *re)
{
	bus_dmamap_sync(sc->sc_dmat, re->re_src_map,
	    0, re->re_src_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
	if (re->re_dst_map != NULL)
		bus_dmamap_sync(sc->sc_dmat, re->re_dst_map, 0,
		    re->re_dst_map->dm_mapsize, BUS_DMASYNC_PREREAD);
	/* XXX have no smaller granularity */
	safe_dma_sync(sc, &sc->sc_ringalloc, 
		BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	safe_dma_sync(sc, &sc->sc_spalloc, BUS_DMASYNC_PREWRITE);
	safe_dma_sync(sc, &sc->sc_dpalloc, BUS_DMASYNC_PREWRITE);

#ifdef SAFE_DEBUG
	if (safe_debug) {
		safe_dump_ringstate(sc, __func__);
		safe_dump_request(sc, __func__, re);
	}
#endif
	sc->sc_nqchip++;
	if (sc->sc_nqchip > safestats.st_maxqchip)
		safestats.st_maxqchip = sc->sc_nqchip;
	/* poke h/w to check descriptor ring, any value can be written */
	WRITE_REG(sc, SAFE_HI_RD_DESCR, 0);
}

/*
 * Is the operand suitable for direct DMA as the destination
 * of an operation.  The hardware requires that each ``particle''
 * but the last in an operation result have the same size.  We
 * fix that size at SAFE_MAX_DSIZE bytes.  This routine returns
 * 0 if some segment is not a multiple of this size, 1 if all
 * segments are exactly this size, or 2 if segments are at worst
 * a multple of this size.
 */
int
safe_dmamap_uniform(const struct safe_operand *op)
{
	int result = 1, i;

	if (op->map->dm_nsegs <= 0)
		return (result);

	for (i = 0; i < op->map->dm_nsegs-1; i++) {
		if (op->map->dm_segs[i].ds_len % SAFE_MAX_DSIZE)
			return (0);
		if (op->map->dm_segs[i].ds_len != SAFE_MAX_DSIZE)
			result = 2;
	}
	return (result);
}

/*
 * Copy all data past offset from srcm to dstm.
 */
void
safe_mcopy(struct mbuf *srcm, struct mbuf *dstm, u_int offset)
{
	u_int j, dlen, slen;
	caddr_t dptr, sptr;

	/*
	 * Advance src and dst to offset.
	 */
	for (j = offset; srcm->m_len <= j;) {
		j -= srcm->m_len;
		srcm = srcm->m_next;
		if (srcm == NULL)
			return;
	}
	sptr = mtod(srcm, caddr_t) + j;
	slen = srcm->m_len - j;

	for (j = offset; dstm->m_len <= j;) {
		j -= dstm->m_len;
		dstm = dstm->m_next;
		if (dstm == NULL)
			return;
	}
	dptr = mtod(dstm, caddr_t) + j;
	dlen = dstm->m_len - j;

	/*
	 * Copy everything that remains.
	 */
	for (;;) {
		j = min(slen, dlen);
		bcopy(sptr, dptr, j);
		if (slen == j) {
			srcm = srcm->m_next;
			if (srcm == NULL)
				return;
			sptr = srcm->m_data;
			slen = srcm->m_len;
		} else
			sptr += j, slen -= j;
		if (dlen == j) {
			dstm = dstm->m_next;
			if (dstm == NULL)
				return;
			dptr = dstm->m_data;
			dlen = dstm->m_len;
		} else
			dptr += j, dlen -= j;
	}
}

void
safe_callback(struct safe_softc *sc, struct safe_ringentry *re)
{
	struct cryptop *crp = (struct cryptop *)re->re_crp;
	struct cryptodesc *crd;

	safestats.st_opackets++;
	safestats.st_obytes += (re->re_dst_map == NULL) ?
	    re->re_src_mapsize : re->re_dst_mapsize;

	safe_dma_sync(sc, &sc->sc_ringalloc,
		BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	if (re->re_desc.d_csr & SAFE_PE_CSR_STATUS) {
		printf("%s: csr 0x%x cmd0 0x%x cmd1 0x%x\n",
		    sc->sc_dev.dv_xname, re->re_desc.d_csr,
			re->re_sa.sa_cmd0, re->re_sa.sa_cmd1);
		safestats.st_peoperr++;
		crp->crp_etype = EIO;		/* something more meaningful? */
	}
	if (re->re_dst_map != NULL && re->re_dst_map != re->re_src_map) {
		bus_dmamap_sync(sc->sc_dmat, re->re_dst_map, 0,
		    re->re_dst_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, re->re_dst_map);
		bus_dmamap_destroy(sc->sc_dmat, re->re_dst_map);
	}
	bus_dmamap_sync(sc->sc_dmat, re->re_src_map, 0,
	    re->re_src_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, re->re_src_map);
	bus_dmamap_destroy(sc->sc_dmat, re->re_src_map);

	/* 
	 * If result was written to a different mbuf chain, swap
	 * it in as the return value and reclaim the original.
	 */
	if ((crp->crp_flags & CRYPTO_F_IMBUF) && re->re_src_m != re->re_dst_m) {
		m_freem(re->re_src_m);
		crp->crp_buf = (caddr_t)re->re_dst_m;
	}

	if (re->re_flags & SAFE_QFLAGS_COPYOUTICV) {
		/* copy out ICV result */
		for (crd = crp->crp_desc; crd; crd = crd->crd_next) {
			if (!(crd->crd_alg == CRYPTO_MD5_HMAC ||
			    crd->crd_alg == CRYPTO_SHA1_HMAC))
				continue;
			if (crd->crd_alg == CRYPTO_SHA1_HMAC) {
				/*
				 * SHA-1 ICV's are byte-swapped; fix 'em up
				 * before copy them to their destination.
				 */
				bswap32(re->re_sastate.sa_saved_indigest[0]);
				bswap32(re->re_sastate.sa_saved_indigest[1]);
				bswap32(re->re_sastate.sa_saved_indigest[2]);
			}
			if (crp->crp_flags & CRYPTO_F_IMBUF) {
				m_copyback((struct mbuf *)crp->crp_buf,
					crd->crd_inject, 12,
					(caddr_t)re->re_sastate.sa_saved_indigest,
					M_NOWAIT);
			} else if (crp->crp_flags & CRYPTO_F_IOV && crp->crp_mac) {
				bcopy((caddr_t)re->re_sastate.sa_saved_indigest,
					crp->crp_mac, 12);
			}
			break;
		}
	}

	crypto_done(crp);
}

/*
 * SafeXcel Interrupt routine
 */
int
safe_intr(void *arg)
{
	struct safe_softc *sc = arg;
	volatile u_int32_t stat;

	stat = READ_REG(sc, SAFE_HM_STAT);
	if (stat == 0)			/* shared irq, not for us */
		return (0);

	WRITE_REG(sc, SAFE_HI_CLR, stat);	/* IACK */

	if ((stat & SAFE_INT_PE_DDONE)) {
		/*
		 * Descriptor(s) done; scan the ring and
		 * process completed operations.
		 */
		while (sc->sc_back != sc->sc_front) {
			struct safe_ringentry *re = sc->sc_back;
#ifdef SAFE_DEBUG
			if (safe_debug) {
				safe_dump_ringstate(sc, __func__);
				safe_dump_request(sc, __func__, re);
			}
#endif
			/*
			 * safe_process marks ring entries that were allocated
			 * but not used with a csr of zero.  This insures the
			 * ring front pointer never needs to be set backwards
			 * in the event that an entry is allocated but not used
			 * because of a setup error.
			 */
			if (re->re_desc.d_csr != 0) {
				if (!SAFE_PE_CSR_IS_DONE(re->re_desc.d_csr))
					break;
				if (!SAFE_PE_LEN_IS_DONE(re->re_desc.d_len))
					break;
				sc->sc_nqchip--;
				safe_callback(sc, re);
			}
			if (++(sc->sc_back) == sc->sc_ringtop)
				sc->sc_back = sc->sc_ring;
		}
	}

	return (1);
}

struct safe_softc *
safe_kfind(struct cryptkop *krp)
{
	struct safe_softc *sc;
	int i;

	for (i = 0; i < safe_cd.cd_ndevs; i++) {
		sc = safe_cd.cd_devs[i];
		if (sc == NULL)
			continue;
		if (sc->sc_cid == krp->krp_hid)
			return (sc);
	}
	return (NULL);
}

int
safe_kprocess(struct cryptkop *krp)
{
	struct safe_softc *sc;
	struct safe_pkq *q;
	int s;

	if ((sc = safe_kfind(krp)) == NULL) {
		krp->krp_status = EINVAL;
		goto err;
	}

	if (krp->krp_op != CRK_MOD_EXP) {
		krp->krp_status = EOPNOTSUPP;
		goto err;
	}

	q = (struct safe_pkq *)malloc(sizeof(*q), M_DEVBUF, M_NOWAIT);
	if (q == NULL) {
		krp->krp_status = ENOMEM;
		goto err;
	}
	q->pkq_krp = krp;

	s = splnet();
	SIMPLEQ_INSERT_TAIL(&sc->sc_pkq, q, pkq_next);
	safe_kfeed(sc);
	splx(s);
	return (0);

err:
	crypto_kdone(krp);
	return (0);
}

#define	SAFE_CRK_PARAM_BASE	0
#define	SAFE_CRK_PARAM_EXP	1
#define	SAFE_CRK_PARAM_MOD	2

int
safe_kstart(struct safe_softc *sc)
{
	struct cryptkop *krp = sc->sc_pkq_cur->pkq_krp;
	int exp_bits, mod_bits, base_bits;
	u_int32_t op, a_off, b_off, c_off, d_off;

	if (krp->krp_iparams < 3 || krp->krp_oparams != 1) {
		krp->krp_status = EINVAL;
		return (1);
	}

	base_bits = safe_ksigbits(&krp->krp_param[SAFE_CRK_PARAM_BASE]);
	if (base_bits > 2048)
		goto too_big;
	if (base_bits <= 0)		/* 5. base not zero */
		goto too_small;

	exp_bits = safe_ksigbits(&krp->krp_param[SAFE_CRK_PARAM_EXP]);
	if (exp_bits > 2048)
		goto too_big;
	if (exp_bits <= 0)		/* 1. exponent word length > 0 */
		goto too_small;		/* 4. exponent not zero */

	mod_bits = safe_ksigbits(&krp->krp_param[SAFE_CRK_PARAM_MOD]);
	if (mod_bits > 2048)
		goto too_big;
	if (mod_bits <= 32)		/* 2. modulus word length > 1 */
		goto too_small;		/* 8. MSW of modulus != zero */
	if (mod_bits < exp_bits)	/* 3 modulus len >= exponent len */
		goto too_small;
	if ((krp->krp_param[SAFE_CRK_PARAM_MOD].crp_p[0] & 1) == 0)
		goto bad_domain;	/* 6. modulus is odd */
	if (mod_bits > krp->krp_param[krp->krp_iparams].crp_nbits)
		goto too_small;		/* make sure result will fit */

	/* 7. modulus > base */
	if (mod_bits < base_bits)
		goto too_small;
	if (mod_bits == base_bits) {
		u_int8_t *basep, *modp;
		int i;

		basep = krp->krp_param[SAFE_CRK_PARAM_BASE].crp_p +
		    ((base_bits + 7) / 8) - 1;
		modp = krp->krp_param[SAFE_CRK_PARAM_MOD].crp_p +
		    ((mod_bits + 7) / 8) - 1;
		
		for (i = 0; i < (mod_bits + 7) / 8; i++, basep--, modp--) {
			if (*modp < *basep)
				goto too_small;
			if (*modp > *basep)
				break;
		}
	}

	/* And on the 9th step, he rested. */

	WRITE_REG(sc, SAFE_PK_A_LEN, (exp_bits + 31) / 32);
	WRITE_REG(sc, SAFE_PK_B_LEN, (mod_bits + 31) / 32);
	if (mod_bits > 1024) {
		op = SAFE_PK_FUNC_EXP4;
		a_off = 0x000;
		b_off = 0x100;
		c_off = 0x200;
		d_off = 0x300;
	} else {
		op = SAFE_PK_FUNC_EXP16;
		a_off = 0x000;
		b_off = 0x080;
		c_off = 0x100;
		d_off = 0x180;
	}
	sc->sc_pk_reslen = b_off - a_off;
	sc->sc_pk_resoff = d_off;

	/* A is exponent, B is modulus, C is base, D is result */
	safe_kload_reg(sc, a_off, b_off - a_off,
	    &krp->krp_param[SAFE_CRK_PARAM_EXP]);
	WRITE_REG(sc, SAFE_PK_A_ADDR, a_off >> 2);
	safe_kload_reg(sc, b_off, b_off - a_off,
	    &krp->krp_param[SAFE_CRK_PARAM_MOD]);
	WRITE_REG(sc, SAFE_PK_B_ADDR, b_off >> 2);
	safe_kload_reg(sc, c_off, b_off - a_off,
	    &krp->krp_param[SAFE_CRK_PARAM_BASE]);
	WRITE_REG(sc, SAFE_PK_C_ADDR, c_off >> 2);
	WRITE_REG(sc, SAFE_PK_D_ADDR, d_off >> 2);

	WRITE_REG(sc, SAFE_PK_FUNC, op | SAFE_PK_FUNC_RUN);

	return (0);

too_big:
	krp->krp_status = E2BIG;
	return (1);
too_small:
	krp->krp_status = ERANGE;
	return (1);
bad_domain:
	krp->krp_status = EDOM;
	return (1);
}

int
safe_ksigbits(struct crparam *cr)
{
	u_int plen = (cr->crp_nbits + 7) / 8;
	int i, sig = plen * 8;
	u_int8_t c, *p = cr->crp_p;

	for (i = plen - 1; i >= 0; i--) {
		c = p[i];
		if (c != 0) {
			while ((c & 0x80) == 0) {
				sig--;
				c <<= 1;
			}
			break;
		}
		sig -= 8;
	}
	return (sig);
}

void
safe_kfeed(struct safe_softc *sc)
{
	if (SIMPLEQ_EMPTY(&sc->sc_pkq) && sc->sc_pkq_cur == NULL)
		return;
	if (sc->sc_pkq_cur != NULL)
		return;
	while (!SIMPLEQ_EMPTY(&sc->sc_pkq)) {
		struct safe_pkq *q = SIMPLEQ_FIRST(&sc->sc_pkq);

		sc->sc_pkq_cur = q;
		SIMPLEQ_REMOVE_HEAD(&sc->sc_pkq, pkq_next);
		if (safe_kstart(sc) != 0) {
			crypto_kdone(q->pkq_krp);
			free(q, M_DEVBUF);
			sc->sc_pkq_cur = NULL;
		} else {
			/* op started, start polling */
			timeout_add(&sc->sc_pkto, 1);
			break;
		}
	}
}

void
safe_kpoll(void *vsc)
{
	struct safe_softc *sc = vsc;
	struct safe_pkq *q;
	struct crparam *res;
	int s, i;
	u_int32_t buf[64];

	s = splnet();
	if (sc->sc_pkq_cur == NULL)
		goto out;
	if (READ_REG(sc, SAFE_PK_FUNC) & SAFE_PK_FUNC_RUN) {
		/* still running, check back later */
		timeout_add(&sc->sc_pkto, 1);
		goto out;
	}

	q = sc->sc_pkq_cur;
	res = &q->pkq_krp->krp_param[q->pkq_krp->krp_iparams];
	bzero(buf, sizeof(buf));
	bzero(res->crp_p, (res->crp_nbits + 7) / 8);
	for (i = 0; i < sc->sc_pk_reslen >> 2; i++)
		buf[i] = letoh32(READ_REG(sc, SAFE_PK_RAM_START +
		    sc->sc_pk_resoff + (i << 2)));
	bcopy(buf, res->crp_p, (res->crp_nbits + 7) / 8);
	res->crp_nbits = sc->sc_pk_reslen * 8;
	res->crp_nbits = safe_ksigbits(res);

	for (i = SAFE_PK_RAM_START; i < SAFE_PK_RAM_END; i += 4)
		WRITE_REG(sc, i, 0);

	explicit_bzero(&buf, sizeof(buf));
	crypto_kdone(q->pkq_krp);
	free(q, M_DEVBUF);
	sc->sc_pkq_cur = NULL;

	safe_kfeed(sc);
out:
	splx(s);
}

void
safe_kload_reg(struct safe_softc *sc, u_int32_t off, u_int32_t len,
    struct crparam *n)
{
	u_int32_t buf[64], i;

	bzero(buf, sizeof(buf));
	bcopy(n->crp_p, buf, (n->crp_nbits + 7) / 8);

	for (i = 0; i < len >> 2; i++)
		WRITE_REG(sc, SAFE_PK_RAM_START + off + (i << 2),
		    htole32(buf[i]));
}

#ifdef SAFE_DEBUG

void
safe_dump_dmastatus(struct safe_softc *sc, const char *tag)
{
	printf("%s: ENDIAN 0x%x SRC 0x%x DST 0x%x STAT 0x%x\n", tag,
	    READ_REG(sc, SAFE_DMA_ENDIAN), READ_REG(sc, SAFE_DMA_SRCADDR),
	    READ_REG(sc, SAFE_DMA_DSTADDR), READ_REG(sc, SAFE_DMA_STAT));
}

void
safe_dump_intrstate(struct safe_softc *sc, const char *tag)
{
	printf("%s: HI_CFG 0x%x HI_MASK 0x%x HI_DESC_CNT 0x%x HU_STAT 0x%x HM_STAT 0x%x\n",
	    tag, READ_REG(sc, SAFE_HI_CFG), READ_REG(sc, SAFE_HI_MASK),
	    READ_REG(sc, SAFE_HI_DESC_CNT), READ_REG(sc, SAFE_HU_STAT),
	    READ_REG(sc, SAFE_HM_STAT));
}

void
safe_dump_ringstate(struct safe_softc *sc, const char *tag)
{
	u_int32_t estat = READ_REG(sc, SAFE_PE_ERNGSTAT);

	/* NB: assume caller has lock on ring */
	printf("%s: ERNGSTAT %x (next %u) back %u front %u\n",
	    tag, estat, (estat >> SAFE_PE_ERNGSTAT_NEXT_S),
	    sc->sc_back - sc->sc_ring, sc->sc_front - sc->sc_ring);
}

void
safe_dump_request(struct safe_softc *sc, const char* tag, struct safe_ringentry *re)
{
	int ix, nsegs;

	ix = re - sc->sc_ring;
	printf("%s: %p (%u): csr %x src %x dst %x sa %x len %x\n", tag,
	    re, ix, re->re_desc.d_csr, re->re_desc.d_src, re->re_desc.d_dst,
	    re->re_desc.d_sa, re->re_desc.d_len);
	if (re->re_src_nsegs > 1) {
		ix = (re->re_desc.d_src - sc->sc_spalloc.dma_paddr) /
		    sizeof(struct safe_pdesc);
		for (nsegs = re->re_src_nsegs; nsegs; nsegs--) {
			printf(" spd[%u] %p: %p", ix,
			    &sc->sc_spring[ix],
			    (caddr_t)sc->sc_spring[ix].pd_addr);
			printf("\n");
			if (++ix == SAFE_TOTAL_SPART)
				ix = 0;
		}
	}
	if (re->re_dst_nsegs > 1) {
		ix = (re->re_desc.d_dst - sc->sc_dpalloc.dma_paddr) /
		    sizeof(struct safe_pdesc);
		for (nsegs = re->re_dst_nsegs; nsegs; nsegs--) {
			printf(" dpd[%u] %p: %p\n", ix,
			    &sc->sc_dpring[ix],
			    (caddr_t) sc->sc_dpring[ix].pd_addr);
			if (++ix == SAFE_TOTAL_DPART)
				ix = 0;
		}
	}
	printf("sa: cmd0 %08x cmd1 %08x staterec %x\n",
	    re->re_sa.sa_cmd0, re->re_sa.sa_cmd1, re->re_sa.sa_staterec);
	printf("sa: key %x %x %x %x %x %x %x %x\n", re->re_sa.sa_key[0],
	    re->re_sa.sa_key[1], re->re_sa.sa_key[2], re->re_sa.sa_key[3],
	    re->re_sa.sa_key[4], re->re_sa.sa_key[5], re->re_sa.sa_key[6],
	    re->re_sa.sa_key[7]);
	printf("sa: indigest %x %x %x %x %x\n", re->re_sa.sa_indigest[0],
	    re->re_sa.sa_indigest[1], re->re_sa.sa_indigest[2],
	    re->re_sa.sa_indigest[3], re->re_sa.sa_indigest[4]);
	printf("sa: outdigest %x %x %x %x %x\n", re->re_sa.sa_outdigest[0],
	    re->re_sa.sa_outdigest[1], re->re_sa.sa_outdigest[2],
	    re->re_sa.sa_outdigest[3], re->re_sa.sa_outdigest[4]);
	printf("sr: iv %x %x %x %x\n",
	    re->re_sastate.sa_saved_iv[0], re->re_sastate.sa_saved_iv[1],
	    re->re_sastate.sa_saved_iv[2], re->re_sastate.sa_saved_iv[3]);
	printf("sr: hashbc %u indigest %x %x %x %x %x\n",
	    re->re_sastate.sa_saved_hashbc,
	    re->re_sastate.sa_saved_indigest[0],
	    re->re_sastate.sa_saved_indigest[1],
	    re->re_sastate.sa_saved_indigest[2],
	    re->re_sastate.sa_saved_indigest[3],
	    re->re_sastate.sa_saved_indigest[4]);
}

void
safe_dump_ring(struct safe_softc *sc, const char *tag)
{
	printf("\nSafeNet Ring State:\n");
	safe_dump_intrstate(sc, tag);
	safe_dump_dmastatus(sc, tag);
	safe_dump_ringstate(sc, tag);
	if (sc->sc_nqchip) {
		struct safe_ringentry *re = sc->sc_back;
		do {
			safe_dump_request(sc, tag, re);
			if (++re == sc->sc_ringtop)
				re = sc->sc_ring;
		} while (re != sc->sc_front);
	}
}

#endif /* SAFE_DEBUG */
