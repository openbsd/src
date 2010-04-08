/*	$OpenBSD: lofn.c,v 1.28 2010/04/08 00:23:53 tedu Exp $	*/

/*
 * Copyright (c) 2001-2002 Jason L. Wright (jason@thought.net)
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
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

/*
 * Driver for the Hifn 6500 assymmetric encryption processor.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/device.h>

#include <crypto/cryptodev.h>
#include <dev/rndvar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/lofnreg.h>
#include <dev/pci/lofnvar.h>

/*
 * Prototypes and count for the pci_device structure
 */
int lofn_probe(struct device *, void *, void *);
void lofn_attach(struct device *, struct device *, void *);

struct cfattach lofn_ca = {
	sizeof(struct lofn_softc), lofn_probe, lofn_attach,
};

struct cfdriver lofn_cd = {
	0, "lofn", DV_DULL
};

int lofn_intr(void *);
int lofn_norm_sigbits(const u_int8_t *, u_int);
void lofn_dump_reg(struct lofn_softc *, int);
void lofn_zero_reg(struct lofn_softc *, int);
void lofn_read_reg(struct lofn_softc *, int, union lofn_reg *);
void lofn_write_reg(struct lofn_softc *, int, union lofn_reg *);
int lofn_kprocess(struct cryptkop *);
struct lofn_softc *lofn_kfind(struct cryptkop *);
int lofn_modexp_start(struct lofn_softc *, struct lofn_q *);
void lofn_modexp_finish(struct lofn_softc *, struct lofn_q *);

void lofn_feed(struct lofn_softc *);

int
lofn_probe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_HIFN &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_HIFN_6500)
		return (1);
	return (0);
}

void 
lofn_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct lofn_softc *sc = (struct lofn_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_size_t iosize;
	int algs[CRK_ALGORITHM_MAX + 1];

	if (pci_mapreg_map(pa, LOFN_BAR0, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_st, &sc->sc_sh, NULL, &iosize, 0)) {
		printf(": can't map mem space\n");
		return;
	}

	sc->sc_dmat = pa->pa_dmat;

	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		goto fail;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, lofn_intr, sc,
	    self->dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto fail;
	}

	WRITE_REG_0(sc, LOFN_REL_RNC, LOFN_RNG_SCALAR);

	/* Enable RNG */
	WRITE_REG_0(sc, LOFN_REL_CFG2,
	    READ_REG_0(sc, LOFN_REL_CFG2) | LOFN_CFG2_RNGENA);
	sc->sc_ier |= LOFN_IER_RDY;
	WRITE_REG(sc, LOFN_REL_IER, sc->sc_ier);

	/* Enable ALU */
	WRITE_REG_0(sc, LOFN_REL_CFG2,
	    READ_REG_0(sc, LOFN_REL_CFG2) | LOFN_CFG2_PRCENA);

	SIMPLEQ_INIT(&sc->sc_queue);

	sc->sc_cid = crypto_get_driverid(0);
	if (sc->sc_cid < 0) {
		printf(": failed to register cid\n");
		return;
	}

	bzero(algs, sizeof(algs));
	algs[CRK_MOD_EXP] = CRYPTO_ALG_FLAG_SUPPORTED;

	crypto_kregister(sc->sc_cid, algs, lofn_kprocess);

	printf(": PK, %s\n", intrstr);

	return;

fail:
	bus_space_unmap(sc->sc_st, sc->sc_sh, iosize);
}

int 
lofn_intr(vsc)
	void *vsc;
{
	struct lofn_softc *sc = vsc;
	struct lofn_q *q;
	u_int32_t sr;
	int r = 0, i;

	sr = READ_REG_0(sc, LOFN_REL_SR);

	if (sc->sc_ier & LOFN_IER_RDY) {
		if (sr & LOFN_SR_RNG_UF) {
			r = 1;
			printf("%s: rng underflow (disabling)\n",
			    sc->sc_dv.dv_xname);
			WRITE_REG_0(sc, LOFN_REL_CFG2,
			    READ_REG_0(sc, LOFN_REL_CFG2) &
			    (~LOFN_CFG2_RNGENA));
			sc->sc_ier &= ~LOFN_IER_RDY;
			WRITE_REG_0(sc, LOFN_REL_IER, sc->sc_ier);
		} else if (sr & LOFN_SR_RNG_RDY) {
			r = 1;

			bus_space_read_region_4(sc->sc_st, sc->sc_sh,
			    LOFN_REL_RNG, sc->sc_rngbuf, LOFN_RNGBUF_SIZE);
			for (i = 0; i < LOFN_RNGBUF_SIZE; i++)
				add_true_randomness(sc->sc_rngbuf[i]);
		}
	}

	if (sc->sc_ier & LOFN_IER_DONE) {
		r = 1;
		if (sr & LOFN_SR_DONE && sc->sc_current != NULL) {
			q = sc->sc_current;
			sc->sc_current = NULL;
			q->q_finish(sc, q);
			free(q, M_DEVBUF);
			lofn_feed(sc);
		}
	}

	return (r);
}

void
lofn_read_reg(sc, ridx, rp)
	struct lofn_softc *sc;
	int ridx;
	union lofn_reg *rp;
{
#if BYTE_ORDER == BIG_ENDIAN
	bus_space_read_region_4(sc->sc_st, sc->sc_sh,
	    LOFN_REGADDR(LOFN_WIN_0, ridx, 0), rp->w, 1024/32);
#else
	bus_space_read_region_4(sc->sc_st, sc->sc_sh,
	    LOFN_REGADDR(LOFN_WIN_2, ridx, 0), rp->w, 1024/32);
#endif
}

void
lofn_write_reg(sc, ridx, rp)
	struct lofn_softc *sc;
	int ridx;
	union lofn_reg *rp;
{
#if BYTE_ORDER == BIG_ENDIAN
	bus_space_write_region_4(sc->sc_st, sc->sc_sh,
	    LOFN_REGADDR(LOFN_WIN_0, ridx, 0), rp->w, 1024/32);
#else
	bus_space_write_region_4(sc->sc_st, sc->sc_sh,
	    LOFN_REGADDR(LOFN_WIN_2, ridx, 0), rp->w, 1024/32);
#endif
}

void
lofn_zero_reg(sc, ridx)
	struct lofn_softc *sc;
	int ridx;
{
	lofn_write_reg(sc, ridx, &sc->sc_zero);
}

void
lofn_dump_reg(sc, ridx)
	struct lofn_softc *sc;
	int ridx;
{
	int i;

	printf("reg %d bits %4u ", ridx,
	    READ_REG(sc, LOFN_LENADDR(LOFN_WIN_2, ridx)) & LOFN_LENMASK);

	for (i = 0; i < 1024/32; i++) {
		printf("%08X", READ_REG(sc, LOFN_REGADDR(LOFN_WIN_3, ridx, i)));
	}
	printf("\n");
}

struct lofn_softc *
lofn_kfind(krp)
	struct cryptkop *krp;
{
	struct lofn_softc *sc;
	int i;

	for (i = 0; i < lofn_cd.cd_ndevs; i++) {
		sc = lofn_cd.cd_devs[i];
		if (sc == NULL)
			continue;
		if (sc->sc_cid == krp->krp_hid)
			return (sc);
	}
	return (NULL);
}

int
lofn_kprocess(krp)
	struct cryptkop *krp;
{
	struct lofn_softc *sc;
	struct lofn_q *q;
	int s;

	if (krp == NULL || krp->krp_callback == NULL)
		return (EINVAL);
	if ((sc = lofn_kfind(krp)) == NULL) {
		krp->krp_status = EINVAL;
		crypto_kdone(krp);
		return (0);
	}

	q = (struct lofn_q *)malloc(sizeof(*q), M_DEVBUF, M_NOWAIT);
	if (q == NULL) {
		krp->krp_status = ENOMEM;
		crypto_kdone(krp);
		return (0);
	}

	switch (krp->krp_op) {
	case CRK_MOD_EXP:
		q->q_start = lofn_modexp_start;
		q->q_finish = lofn_modexp_finish;
		q->q_krp = krp;
		s = splnet();
		SIMPLEQ_INSERT_TAIL(&sc->sc_queue, q, q_next);
		lofn_feed(sc);
		splx(s);
		return (0);
	default:
		printf("%s: kprocess: invalid op 0x%x\n",
		    sc->sc_dv.dv_xname, krp->krp_op);
		krp->krp_status = EOPNOTSUPP;
		crypto_kdone(krp);
		free(q, M_DEVBUF);
		return (0);
	}
}

int
lofn_modexp_start(sc, q)
	struct lofn_softc *sc;
	struct lofn_q *q;
{
	struct cryptkop *krp = q->q_krp;
	int ip = 0, err = 0;
	int mshift, eshift, nshift;
	int mbits, ebits, nbits;

	if (krp->krp_param[LOFN_MODEXP_PAR_M].crp_nbits > 1024) {
		err = ERANGE;
		goto errout;
	}

	/* Zero out registers. */
	lofn_zero_reg(sc, 0);
	lofn_zero_reg(sc, 1);
	lofn_zero_reg(sc, 2);
	lofn_zero_reg(sc, 3);

	/* Write out N... */
	nbits = lofn_norm_sigbits(krp->krp_param[LOFN_MODEXP_PAR_N].crp_p,
	    krp->krp_param[LOFN_MODEXP_PAR_N].crp_nbits);
	if (nbits > 1024) {
		err = E2BIG;
		goto errout;
	}
	if (nbits < 5) {
		err = ERANGE;
		goto errout;
	}
	bzero(&sc->sc_tmp, sizeof(sc->sc_tmp));
	bcopy(krp->krp_param[LOFN_MODEXP_PAR_N].crp_p, &sc->sc_tmp,
	    (nbits + 7) / 8);
	lofn_write_reg(sc, 2, &sc->sc_tmp);

	nshift = 1024 - nbits;
	WRITE_REG(sc, LOFN_LENADDR(LOFN_WIN_2, 2), 1024);
	if (nshift != 0) {
		WRITE_REG(sc, LOFN_REL_INSTR + ip,
		    LOFN_INSTR2(0, OP_CODE_SL, 2, 2, nshift));
		ip += 4;

		WRITE_REG(sc, LOFN_REL_INSTR + ip,
		    LOFN_INSTR2(0, OP_CODE_TAG, 2, 2, nbits));
		ip += 4;
	}

	/* Write out M... */
	mbits = lofn_norm_sigbits(krp->krp_param[LOFN_MODEXP_PAR_M].crp_p,
	    krp->krp_param[LOFN_MODEXP_PAR_M].crp_nbits);
	if (mbits > 1024 || mbits > nbits) {
		err = E2BIG;
		goto errout;
	}
	bzero(&sc->sc_tmp, sizeof(sc->sc_tmp));
	bcopy(krp->krp_param[LOFN_MODEXP_PAR_M].crp_p, &sc->sc_tmp,
	    (mbits + 7) / 8);
	lofn_write_reg(sc, 0, &sc->sc_tmp);

	mshift = 1024 - nbits;
	WRITE_REG(sc, LOFN_LENADDR(LOFN_WIN_2, 0), 1024);
	if (mshift != 0) {
		WRITE_REG(sc, LOFN_REL_INSTR + ip,
		    LOFN_INSTR2(0, OP_CODE_SL, 0, 0, mshift));
		ip += 4;

		WRITE_REG(sc, LOFN_REL_INSTR + ip,
		    LOFN_INSTR2(0, OP_CODE_TAG, 0, 0, nbits));
		ip += 4;
	}

	/* Write out E... */
	ebits = lofn_norm_sigbits(krp->krp_param[LOFN_MODEXP_PAR_E].crp_p,
	    krp->krp_param[LOFN_MODEXP_PAR_E].crp_nbits);
	if (ebits > 1024 || ebits > nbits) {
		err = E2BIG;
		goto errout;
	}
	if (ebits < 1) {
		err = ERANGE;
		goto errout;
	}
	bzero(&sc->sc_tmp, sizeof(sc->sc_tmp));
	bcopy(krp->krp_param[LOFN_MODEXP_PAR_E].crp_p, &sc->sc_tmp,
	    (ebits + 7) / 8);
	lofn_write_reg(sc, 1, &sc->sc_tmp);

	eshift = 1024 - nbits;
	WRITE_REG(sc, LOFN_LENADDR(LOFN_WIN_2, 1), 1024);
	if (eshift != 0) {
		WRITE_REG(sc, LOFN_REL_INSTR + ip,
		    LOFN_INSTR2(0, OP_CODE_SL, 1, 1, eshift));
		ip += 4;

		WRITE_REG(sc, LOFN_REL_INSTR + ip,
		    LOFN_INSTR2(0, OP_CODE_TAG, 1, 1, nbits));
		ip += 4;
	}

	if (nshift == 0) {
		WRITE_REG(sc, LOFN_REL_INSTR + ip,
		    LOFN_INSTR(OP_DONE, OP_CODE_MODEXP, 3, 0, 1, 2));
		ip += 4;
	} else {
		WRITE_REG(sc, LOFN_REL_INSTR + ip,
		    LOFN_INSTR(0, OP_CODE_MODEXP, 3, 0, 1, 2));
		ip += 4;

		WRITE_REG(sc, LOFN_REL_INSTR + ip,
		    LOFN_INSTR2(0, OP_CODE_SR, 3, 3, nshift));
		ip += 4;

		WRITE_REG(sc, LOFN_REL_INSTR + ip,
		    LOFN_INSTR2(OP_DONE, OP_CODE_TAG, 3, 3, nbits));
		ip += 4;
	}

	/* Start microprogram */
	WRITE_REG(sc, LOFN_REL_CR, 0);

	return (0);

errout:
	bzero(&sc->sc_tmp, sizeof(sc->sc_tmp));
	lofn_zero_reg(sc, 0);
	lofn_zero_reg(sc, 1);
	lofn_zero_reg(sc, 2);
	lofn_zero_reg(sc, 3);
	krp->krp_status = err;
	crypto_kdone(krp);
	return (1);
}

void
lofn_modexp_finish(sc, q)
	struct lofn_softc *sc;
	struct lofn_q *q;
{
	struct cryptkop *krp = q->q_krp;
	int reglen, crplen;

	lofn_read_reg(sc, 3, &sc->sc_tmp);

	reglen = ((READ_REG(sc, LOFN_LENADDR(LOFN_WIN_2, 3)) & LOFN_LENMASK) +
	    7) / 8;
	crplen = (krp->krp_param[krp->krp_iparams].crp_nbits + 7) / 8;

	if (crplen <= reglen)
		bcopy(sc->sc_tmp.b, krp->krp_param[krp->krp_iparams].crp_p,
		    reglen);
	else {
		bcopy(sc->sc_tmp.b, krp->krp_param[krp->krp_iparams].crp_p,
		    reglen);
		bzero(krp->krp_param[krp->krp_iparams].crp_p + reglen,
		    crplen - reglen);
	}
	bzero(&sc->sc_tmp, sizeof(sc->sc_tmp));
	lofn_zero_reg(sc, 0);
	lofn_zero_reg(sc, 1);
	lofn_zero_reg(sc, 2);
	lofn_zero_reg(sc, 3);
	crypto_kdone(krp);
}

/*
 * Return the number of significant bits of a big number.
 */
int
lofn_norm_sigbits(const u_int8_t *p, u_int pbits)
{
	u_int plen = (pbits + 7) / 8;
	int i, sig = plen * 8;
	u_int8_t c;

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
lofn_feed(sc)
	struct lofn_softc *sc;
{
	struct lofn_q *q;

	/* Queue is empty and nothing being processed, turn off interrupt */
	if (SIMPLEQ_EMPTY(&sc->sc_queue) &&
	    sc->sc_current == NULL) {
		sc->sc_ier &= ~LOFN_IER_DONE;
		WRITE_REG(sc, LOFN_REL_IER, sc->sc_ier);
		return;
	}

	/* Operation already pending, wait. */
	if (sc->sc_current != NULL)
		return;

	while (!SIMPLEQ_EMPTY(&sc->sc_queue)) {
		q = SIMPLEQ_FIRST(&sc->sc_queue);
		if (q->q_start(sc, q) == 0) {
			sc->sc_current = q;
			SIMPLEQ_REMOVE_HEAD(&sc->sc_queue, q_next);
			sc->sc_ier |= LOFN_IER_DONE;
			WRITE_REG(sc, LOFN_REL_IER, sc->sc_ier);
			break;
		} else {
			SIMPLEQ_REMOVE_HEAD(&sc->sc_queue, q_next);
			free(q, M_DEVBUF);
		}
	}
}
