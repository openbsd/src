/*	$OpenBSD: nofn.c,v 1.12 2004/05/04 16:59:31 grange Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
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
 * Driver for the Hifn 7814/7851/7854 HIPP1 processor.
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

#include <crypto/cryptodev.h>
#include <crypto/cryptosoft.h>
#include <dev/rndvar.h>
#include <sys/md5k.h>
#include <crypto/sha1.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/nofnreg.h>
#include <dev/pci/nofnvar.h>

int nofn_match(struct device *, void *, void *);
void nofn_attach(struct device *, struct device *, void *);
int nofn_intr(void *);

void nofn_rng_enable(struct nofn_softc *);
void nofn_rng_disable(struct nofn_softc *);
void nofn_rng_tick(void *);
int nofn_rng_intr(struct nofn_softc *);
int nofn_rng_read(struct nofn_softc *);

int nofn_pk_process(struct cryptkop *);
void nofn_pk_enable(struct nofn_softc *);
void nofn_pk_feed(struct nofn_softc *);
struct nofn_softc *nofn_pk_find(struct cryptkop *);
void nofn_pk_write_reg(struct nofn_softc *, int, union nofn_pk_reg *);
void nofn_pk_read_reg(struct nofn_softc *, int, union nofn_pk_reg *);
void nofn_pk_zero_reg(struct nofn_softc *, int);
int nofn_modexp_start(struct nofn_softc *, struct nofn_pk_q *);
void nofn_modexp_finish(struct nofn_softc *, struct nofn_pk_q *);
int nofn_pk_sigbits(const u_int8_t *, u_int);

struct cfattach nofn_ca = {
	sizeof(struct nofn_softc), nofn_match, nofn_attach
};

struct cfdriver nofn_cd = {
	0, "nofn", DV_DULL
};

int
nofn_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_HIFN &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_HIFN_78XX)
		return (1);
	return (0);
}

void
nofn_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct nofn_softc *sc = (struct nofn_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_size_t bar0size = 0, bar3size = 0;
	u_int32_t cmd;

	sc->sc_dmat = pa->pa_dmat;

	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	cmd |= PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, cmd);
	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

	if (!(cmd & PCI_COMMAND_MEM_ENABLE)) {
		printf(": failed to enable memory mapping\n");
		goto fail;
	}

	if (!(cmd & PCI_COMMAND_MASTER_ENABLE)) {
		printf(": failed to enable bus mastering\n");
		goto fail;
	}

	if (pci_mapreg_map(pa, NOFN_BAR0_REGS, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_st, &sc->sc_sh, NULL, &bar0size, 0)) {
		printf(": can't map bar0 regs\n");
		goto fail;
	}

	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		bus_space_unmap(sc->sc_st, sc->sc_sh, bar0size);
		goto fail;
	}

	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, nofn_intr, sc,
	    self->dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto fail;
	}

	sc->sc_revid = REG_READ_4(sc, NOFN_REVID);

	switch (sc->sc_revid) {
	case REVID_7814_7854_1:
	case REVID_8154_1:/* XXX ? */
	case REVID_8065_1:/* XXX ? */
	case REVID_8165_1:/* XXX ? */
		if (pci_mapreg_map(pa, NOFN_BAR3_PK, PCI_MAPREG_TYPE_MEM, 0,
		    &sc->sc_pk_t, &sc->sc_pk_h, NULL, &bar3size, 0)) {
			printf(": can't map bar3 regs\n");
			goto fail;
		}
		nofn_rng_enable(sc);
		nofn_pk_enable(sc);
		break;
	case REVID_7851_1:
	case REVID_7851_2:
		break;
	default:
		printf(": unknown revid %x\n", sc->sc_revid);
		break;
	}

	printf(":");
	if (sc->sc_flags & NOFN_FLAGS_PK)
		printf(" PK");
	if (sc->sc_flags & NOFN_FLAGS_RNG)
		printf(" RNG");
	printf(", %s\n", intrstr);

	REG_WRITE_4(sc, NOFN_PCI_INT_MASK, sc->sc_intrmask);

	return;

fail:
	if (bar3size != 0)
		bus_space_unmap(sc->sc_pk_t, sc->sc_pk_h, bar3size);
	if (bar0size != 0)
		bus_space_unmap(sc->sc_st, sc->sc_sh, bar0size);
}

int
nofn_intr(vsc)
	void *vsc;
{
	struct nofn_softc *sc = vsc;
	u_int32_t stat;
	int r = 0;

	stat = REG_READ_4(sc, NOFN_PCI_INT_STAT) & sc->sc_intrmask;

	if (stat & PCIINTSTAT_RNGRDY)
		r |= nofn_rng_intr(sc);

	if (stat & PCIINTSTAT_PK) {
		struct nofn_pk_q *q;
		u_int32_t sr;

		r = 1;
		sr = PK_READ_4(sc, NOFN_PK_SR);
		if (sr & PK_SR_DONE && sc->sc_pk_current != NULL) {
			q = sc->sc_pk_current;
			sc->sc_pk_current = NULL;
			q->q_finish(sc, q);
			free(q, M_DEVBUF);
			nofn_pk_feed(sc);
		}
	}

	return (r);
}

int
nofn_rng_read(sc)
	struct nofn_softc *sc;
{
	u_int32_t buf[8], reg;
	int ret = 0, i;

	for (;;) {
		reg = PK_READ_4(sc, NOFN_PK_SR);
		if (reg & PK_SR_UFLOW) {
			ret = -1;
			printf("%s: rng underflow, disabling.\n",
			    sc->sc_dev.dv_xname);
			nofn_rng_disable(sc);
			break;
		}

		if ((reg & PK_SR_RRDY) == 0)
			break;

		ret = 1;
		bus_space_read_region_4(sc->sc_pk_t, sc->sc_pk_h,
		    NOFN_PK_RNGFIFO_BEGIN, buf, 8);
		if (sc->sc_rngskip > 0)
			sc->sc_rngskip -= 8;
		else
			for (i = 0; i < 8; i++)
				add_true_randomness(buf[i]);
	}

	return (ret);
}

int
nofn_rng_intr(sc)
	struct nofn_softc *sc;
{
	int r;

	r = nofn_rng_read(sc);
	if (r == 0)
		return (0);
	return (1);
}

void
nofn_rng_tick(vsc)
	void *vsc;
{
	struct nofn_softc *sc = vsc;
	int s, r;

	s = splnet();
	r = nofn_rng_read(sc);
	if (r != -1)
		timeout_add(&sc->sc_rngto, sc->sc_rngtick);
	splx(s);
}

void
nofn_rng_disable(sc)
	struct nofn_softc *sc;
{
	u_int32_t r;

	/* disable rng unit */
	r = PK_READ_4(sc, NOFN_PK_CFG2);
	r &= PK_CFG2_ALU_ENA; /* preserve */
	PK_WRITE_4(sc, NOFN_PK_CFG2, r);

	switch (sc->sc_revid) {
	case REVID_7814_7854_1:
		if (timeout_pending(&sc->sc_rngto))
			timeout_del(&sc->sc_rngto);
		break;
	case REVID_8154_1:
	case REVID_8065_1:
	case REVID_8165_1:
		/* disable rng interrupts */
		r = PK_READ_4(sc, NOFN_PK_IER);
		r &= PK_IER_DONE; /* preserve */
		PK_WRITE_4(sc, NOFN_PK_IER, r);

		sc->sc_intrmask &= ~PCIINTMASK_RNGRDY;
		REG_WRITE_4(sc, NOFN_PCI_INT_MASK, sc->sc_intrmask);
		break;
	default:
		printf("%s: nofn_rng_disable: unknown rev %x\n", 
		    sc->sc_dev.dv_xname, sc->sc_revid);
		break;
	}

	sc->sc_flags &= ~NOFN_FLAGS_RNG;
}

void
nofn_rng_enable(sc)
	struct nofn_softc *sc;
{
	u_int32_t r;

	/* setup scalar */
	PK_WRITE_4(sc, NOFN_PK_RNC, PK_RNC_SCALER);

	/* enable rng unit */
	r = PK_READ_4(sc, NOFN_PK_CFG2);
	r &= PK_CFG2_ALU_ENA; /* preserve */
	r |= PK_CFG2_RNG_ENA;
	PK_WRITE_4(sc, NOFN_PK_CFG2, r);

	/* 78xx chips cannot use interrupts to gather rng's */
	switch (sc->sc_revid) {
	case REVID_7814_7854_1:
		timeout_set(&sc->sc_rngto, nofn_rng_tick, sc);
		if (hz < 100)
			sc->sc_rngtick = 1;
		else
			sc->sc_rngtick = hz / 100;
		timeout_add(&sc->sc_rngto, sc->sc_rngtick);
		break;
	case REVID_8154_1:
	case REVID_8065_1:
	case REVID_8165_1:
		/* enable rng interrupts */
		r = PK_READ_4(sc, NOFN_PK_IER);
		r &= PK_IER_DONE; /* preserve */
		r |= PK_IER_RRDY;
		PK_WRITE_4(sc, NOFN_PK_IER, r);
		sc->sc_intrmask |= PCIINTMASK_RNGRDY;
		break;
	default:
		printf("%s: nofn_rng_enable: unknown rev %x\n", 
		    sc->sc_dev.dv_xname, sc->sc_revid);
		break;
	}

	sc->sc_flags |= NOFN_FLAGS_RNG;
}

void
nofn_pk_enable(sc)
	struct nofn_softc *sc;
{
	u_int32_t r;
	int algs[CRK_ALGORITHM_MAX + 1];

	if ((sc->sc_cid = crypto_get_driverid(0)) < 0) {
		printf(": failed to register cid\n");
		return;
	}

	SIMPLEQ_INIT(&sc->sc_pk_queue);
	sc->sc_pk_current = NULL;

	bzero(algs, sizeof(algs));
	algs[CRK_MOD_EXP] = CRYPTO_ALG_FLAG_SUPPORTED;
	crypto_kregister(sc->sc_cid, algs, nofn_pk_process);

	/* enable ALU */
	r = PK_READ_4(sc, NOFN_PK_CFG2);
	r &= PK_CFG2_RNG_ENA; /* preserve */
	r |= PK_CFG2_ALU_ENA;
	PK_WRITE_4(sc, NOFN_PK_CFG2, r);

	sc->sc_intrmask |= PCIINTMASK_PK;
	sc->sc_flags |= NOFN_FLAGS_PK;
}

void
nofn_pk_feed(sc)
	struct nofn_softc *sc;
{
	struct nofn_pk_q *q;
	u_int32_t r;

	/* Queue is empty and nothing being processed, turn off interrupt */
	if (SIMPLEQ_EMPTY(&sc->sc_pk_queue) &&
	    sc->sc_pk_current == NULL) {
		r = PK_READ_4(sc, NOFN_PK_IER);
		r &= PK_IER_RRDY; /* preserve */
		PK_WRITE_4(sc, NOFN_PK_IER, r);
		return;
	}

	/* Operation already pending, wait. */
	if (sc->sc_pk_current != NULL)
		return;

	while (!SIMPLEQ_EMPTY(&sc->sc_pk_queue)) {
		q = SIMPLEQ_FIRST(&sc->sc_pk_queue);
		if (q->q_start(sc, q) == 0) {
			sc->sc_pk_current = q;
			SIMPLEQ_REMOVE_HEAD(&sc->sc_pk_queue, q_next);

			r = PK_READ_4(sc, NOFN_PK_IER);
			r &= PK_IER_RRDY; /* preserve */
			r |= PK_IER_DONE;
			PK_WRITE_4(sc, NOFN_PK_IER, r);
			break;
		} else {
			SIMPLEQ_REMOVE_HEAD(&sc->sc_pk_queue, q_next);
			free(q, M_DEVBUF);
		}
	}
}

int
nofn_pk_process(krp)
	struct cryptkop *krp;
{
	struct nofn_softc *sc;
	struct nofn_pk_q *q;
	int s;

	if (krp == NULL || krp->krp_callback == NULL)
		return (EINVAL);
	if ((sc = nofn_pk_find(krp)) == NULL) {
		krp->krp_status = EINVAL;
		crypto_kdone(krp);
		return (0);
	}

	q = (struct nofn_pk_q *)malloc(sizeof(*q), M_DEVBUF, M_NOWAIT);
	if (q == NULL) {
		krp->krp_status = ENOMEM;
		crypto_kdone(krp);
		return (0);
	}

	switch (krp->krp_op) {
	case CRK_MOD_EXP:
		q->q_start = nofn_modexp_start;
		q->q_finish = nofn_modexp_finish;
		q->q_krp = krp;
		s = splnet();
		SIMPLEQ_INSERT_TAIL(&sc->sc_pk_queue, q, q_next);
		nofn_pk_feed(sc);
		splx(s);
		return (0);
	default:
		printf("%s: kprocess: invalid op 0x%x\n",
		    sc->sc_dev.dv_xname, krp->krp_op);
		krp->krp_status = EOPNOTSUPP;
		crypto_kdone(krp);
		free(q, M_DEVBUF);
		return (0);
	}
}

struct nofn_softc *
nofn_pk_find(krp)
	struct cryptkop *krp;
{
	struct nofn_softc *sc;
	int i;

	for (i = 0; i < nofn_cd.cd_ndevs; i++) {
		sc = nofn_cd.cd_devs[i];
		if (sc == NULL)
			continue;
		if (sc->sc_cid == krp->krp_hid)
			return (sc);
	}
	return (NULL);
}

void
nofn_pk_read_reg(sc, ridx, rp)
	struct nofn_softc *sc;
	int ridx;
	union nofn_pk_reg *rp;
{
#if BYTE_ORDER == BIG_ENDIAN
	bus_space_read_region_4(sc->sc_pk_t, sc->sc_pk_h,
	    NOFN_PK_REGADDR(NOFN_PK_WIN_0, ridx, 0), rp->w, 1024/32);
#else
	bus_space_read_region_4(sc->sc_pk_t, sc->sc_pk_h,
	    NOFN_PK_REGADDR(NOFN_PK_WIN_2, ridx, 0), rp->w, 1024/32);
#endif
}

void
nofn_pk_write_reg(sc, ridx, rp)
	struct nofn_softc *sc;
	int ridx;
	union nofn_pk_reg *rp;
{
#if BYTE_ORDER == BIG_ENDIAN
	bus_space_write_region_4(sc->sc_pk_t, sc->sc_pk_h,
	    NOFN_PK_REGADDR(NOFN_PK_WIN_0, ridx, 0), rp->w, 1024/32);
#else
	bus_space_write_region_4(sc->sc_pk_t, sc->sc_pk_h,
	    NOFN_PK_REGADDR(NOFN_PK_WIN_2, ridx, 0), rp->w, 1024/32);
#endif
}

void
nofn_pk_zero_reg(sc, ridx)
	struct nofn_softc *sc;
	int ridx;
{
	nofn_pk_write_reg(sc, ridx, &sc->sc_pk_zero);
}

int
nofn_modexp_start(sc, q)
	struct nofn_softc *sc;
	struct nofn_pk_q *q;
{
	struct cryptkop *krp = q->q_krp;
	int ip = 0, err = 0;
	int mshift, eshift, nshift;
	int mbits, ebits, nbits;

	if (krp->krp_param[NOFN_MODEXP_PAR_M].crp_nbits > 1024) {
		err = ERANGE;
		goto errout;
	}

	/* Zero out registers. */
	nofn_pk_zero_reg(sc, 0);
	nofn_pk_zero_reg(sc, 1);
	nofn_pk_zero_reg(sc, 2);
	nofn_pk_zero_reg(sc, 3);

	/* Write out N... */
	nbits = nofn_pk_sigbits(krp->krp_param[NOFN_MODEXP_PAR_N].crp_p,
	    krp->krp_param[NOFN_MODEXP_PAR_N].crp_nbits);
	if (nbits > 1024) {
		err = E2BIG;
		goto errout;
	}
	if (nbits < 5) {
		err = ERANGE;
		goto errout;
	}
	bzero(&sc->sc_pk_tmp, sizeof(sc->sc_pk_tmp));
	bcopy(krp->krp_param[NOFN_MODEXP_PAR_N].crp_p, &sc->sc_pk_tmp,
	    (nbits + 7) / 8);
	nofn_pk_write_reg(sc, 2, &sc->sc_pk_tmp);

	nshift = 1024 - nbits;
	PK_WRITE_4(sc, NOFN_PK_LENADDR(2), 1024);
	if (nshift != 0) {
		PK_WRITE_4(sc, NOFN_PK_INSTR_BEGIN + ip,
		    NOFN_PK_INSTR2(0, PK_OPCODE_SL, 2, 2, nshift));
		ip += 4;

		PK_WRITE_4(sc, NOFN_PK_INSTR_BEGIN + ip,
		    NOFN_PK_INSTR2(0, PK_OPCODE_TAG, 2, 2, nbits));
		ip += 4;
	}

	/* Write out M... */
	mbits = nofn_pk_sigbits(krp->krp_param[NOFN_MODEXP_PAR_M].crp_p,
	    krp->krp_param[NOFN_MODEXP_PAR_M].crp_nbits);
	if (mbits > 1024 || mbits > nbits) {
		err = E2BIG;
		goto errout;
	}
	bzero(&sc->sc_pk_tmp, sizeof(sc->sc_pk_tmp));
	bcopy(krp->krp_param[NOFN_MODEXP_PAR_M].crp_p, &sc->sc_pk_tmp,
	    (mbits + 7) / 8);
	nofn_pk_write_reg(sc, 0, &sc->sc_pk_tmp);

	mshift = 1024 - nbits;
	PK_WRITE_4(sc, NOFN_PK_LENADDR(0), 1024);
	if (mshift != 0) {
		PK_WRITE_4(sc, NOFN_PK_INSTR_BEGIN + ip,
		    NOFN_PK_INSTR2(0, PK_OPCODE_SL, 0, 0, mshift));
		ip += 4;

		PK_WRITE_4(sc, NOFN_PK_INSTR_BEGIN + ip,
		    NOFN_PK_INSTR2(0, PK_OPCODE_TAG, 0, 0, nbits));
		ip += 4;
	}

	/* Write out E... */
	ebits = nofn_pk_sigbits(krp->krp_param[NOFN_MODEXP_PAR_E].crp_p,
	    krp->krp_param[NOFN_MODEXP_PAR_E].crp_nbits);
	if (ebits > 1024 || ebits > nbits) {
		err = E2BIG;
		goto errout;
	}
	if (ebits < 1) {
		err = ERANGE;
		goto errout;
	}
	bzero(&sc->sc_pk_tmp, sizeof(sc->sc_pk_tmp));
	bcopy(krp->krp_param[NOFN_MODEXP_PAR_E].crp_p, &sc->sc_pk_tmp,
	    (ebits + 7) / 8);
	nofn_pk_write_reg(sc, 1, &sc->sc_pk_tmp);

	eshift = 1024 - nbits;
	PK_WRITE_4(sc, NOFN_PK_LENADDR(1), 1024);
	if (eshift != 0) {
		PK_WRITE_4(sc, NOFN_PK_INSTR_BEGIN + ip,
		    NOFN_PK_INSTR2(0, PK_OPCODE_SL, 1, 1, eshift));
		ip += 4;

		PK_WRITE_4(sc, NOFN_PK_INSTR_BEGIN + ip,
		    NOFN_PK_INSTR2(0, PK_OPCODE_TAG, 1, 1, nbits));
		ip += 4;
	}

	if (nshift == 0) {
		PK_WRITE_4(sc, NOFN_PK_INSTR_BEGIN + ip,
		    NOFN_PK_INSTR(PK_OP_DONE, PK_OPCODE_MODEXP, 3, 0, 1, 2));
		ip += 4;
	} else {
		PK_WRITE_4(sc, NOFN_PK_INSTR_BEGIN + ip,
		    NOFN_PK_INSTR(0, PK_OPCODE_MODEXP, 3, 0, 1, 2));
		ip += 4;

		PK_WRITE_4(sc, NOFN_PK_INSTR_BEGIN + ip,
		    NOFN_PK_INSTR2(0, PK_OPCODE_SR, 3, 3, nshift));
		ip += 4;

		PK_WRITE_4(sc, NOFN_PK_INSTR_BEGIN + ip,
		    NOFN_PK_INSTR2(PK_OP_DONE, PK_OPCODE_TAG, 3, 3, nbits));
		ip += 4;
	}

	/* Start microprogram */
	PK_WRITE_4(sc, NOFN_PK_CR, 0 << PK_CR_OFFSET_S);

	return (0);

errout:
	bzero(&sc->sc_pk_tmp, sizeof(sc->sc_pk_tmp));
	nofn_pk_zero_reg(sc, 0);
	nofn_pk_zero_reg(sc, 1);
	nofn_pk_zero_reg(sc, 2);
	nofn_pk_zero_reg(sc, 3);
	krp->krp_status = err;
	crypto_kdone(krp);
	return (1);
}

void
nofn_modexp_finish(sc, q)
	struct nofn_softc *sc;
	struct nofn_pk_q *q;
{
	struct cryptkop *krp = q->q_krp;
	int reglen, crplen;

	nofn_pk_read_reg(sc, 3, &sc->sc_pk_tmp);

	reglen = ((PK_READ_4(sc, NOFN_PK_LENADDR(3)) & NOFN_PK_LENMASK) + 7)
	    / 8;
	crplen = (krp->krp_param[krp->krp_iparams].crp_nbits + 7) / 8;

	if (crplen <= reglen)
		bcopy(sc->sc_pk_tmp.b, krp->krp_param[krp->krp_iparams].crp_p,
		    reglen);
	else {
		bcopy(sc->sc_pk_tmp.b, krp->krp_param[krp->krp_iparams].crp_p,
		    reglen);
		bzero(krp->krp_param[krp->krp_iparams].crp_p + reglen,
		    crplen - reglen);
	}
	bzero(&sc->sc_pk_tmp, sizeof(sc->sc_pk_tmp));
	nofn_pk_zero_reg(sc, 0);
	nofn_pk_zero_reg(sc, 1);
	nofn_pk_zero_reg(sc, 2);
	nofn_pk_zero_reg(sc, 3);
	crypto_kdone(krp);
}

/*
 * Return the number of significant bits of a little endian big number.
 */
int
nofn_pk_sigbits(p, pbits)
	const u_int8_t *p;
	u_int pbits;
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
