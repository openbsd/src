/*	$OpenBSD: lofn.c,v 1.14 2002/05/08 19:09:25 jason Exp $	*/

/*
 * Copyright (c) 2001 Jason L. Wright (jason@thought.net)
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
#include <sys/proc.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

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
int lofn_kprocess_modexp(struct lofn_softc *, struct cryptkop *);

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
	u_int32_t cmd;

	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	cmd |= PCI_COMMAND_MEM_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, cmd);
	cmd = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

	if (!(cmd & PCI_COMMAND_MEM_ENABLE)) {
		printf(": failed to enable memory mapping\n");
		return;
	}

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
	WRITE_REG_0(sc, LOFN_REL_IER,
	    READ_REG_0(sc, LOFN_REL_IER) | LOFN_IER_RDY);
	WRITE_REG_0(sc, LOFN_REL_CFG2,
	    READ_REG_0(sc, LOFN_REL_CFG2) | LOFN_CFG2_RNGENA);

	/* Enable ALU */
	WRITE_REG_0(sc, LOFN_REL_CFG2,
	    READ_REG_0(sc, LOFN_REL_CFG2) | LOFN_CFG2_PRCENA);

	sc->sc_cid = crypto_get_driverid(0);
	if (sc->sc_cid < 0) {
		printf(": failed to register cid\n");
		return;
	}

	crypto_kregister(sc->sc_cid, CRK_MOD_EXP, 0, lofn_kprocess);

	printf(": %s\n", intrstr, sc->sc_sh);

	return;

fail:
	bus_space_unmap(sc->sc_st, sc->sc_sh, iosize);
}

int 
lofn_intr(vsc)
	void *vsc;
{
	struct lofn_softc *sc = vsc;
	u_int32_t sr;
	int r = 0, i;

	sr = READ_REG_0(sc, LOFN_REL_SR);

	if (sr & LOFN_SR_RNG_UF) {
		r = 1;
		printf("%s: rng underflow (disabling)\n", sc->sc_dv.dv_xname);
		WRITE_REG_0(sc, LOFN_REL_CFG2,
		    READ_REG_0(sc, LOFN_REL_CFG2) & (~LOFN_CFG2_RNGENA));
		WRITE_REG_0(sc, LOFN_REL_IER,
		    READ_REG_0(sc, LOFN_REL_IER) & (~LOFN_IER_RDY));
	} else if (sr & LOFN_SR_RNG_RDY) {
		r = 1;

		bus_space_read_region_4(sc->sc_st, sc->sc_sh, LOFN_REL_RNG,
		    sc->sc_rngbuf, LOFN_RNGBUF_SIZE);
		for (i = 0; i < LOFN_RNGBUF_SIZE; i++)
			add_true_randomness(sc->sc_rngbuf[i]);
	}

	return (r);
}

void
lofn_read_reg(sc, ridx, rp)
	struct lofn_softc *sc;
	int ridx;
	union lofn_reg *rp;
{
	bus_space_read_region_4(sc->sc_st, sc->sc_sh,
	    LOFN_REGADDR(LOFN_WIN_2, ridx, 0), rp->w, 1024/32);
}

void
lofn_write_reg(sc, ridx, rp)
	struct lofn_softc *sc;
	int ridx;
	union lofn_reg *rp;
{
	bus_space_write_region_4(sc->sc_st, sc->sc_sh,
	    LOFN_REGADDR(LOFN_WIN_2, ridx, 0), rp->w, 1024/32);
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

	if (krp == NULL || krp->krp_callback == NULL)
		return (EINVAL);
	if ((sc = lofn_kfind(krp)) == NULL)
		return (EINVAL);

	switch (krp->krp_op) {
	case CRK_MOD_EXP:
		return (lofn_kprocess_modexp(sc, krp));
	default:
		printf("%s: kprocess: invalid op 0x%x\n",
		    sc->sc_dv.dv_xname, krp->krp_op);
		krp->krp_status = EOPNOTSUPP;
		crypto_kdone(krp);
		return (0);
	}
}

/*
 * Start computation of cr[C] = (cr[M] ^ cr[E]) mod cr[N]
 */
int
lofn_kprocess_modexp(sc, krp)
	struct lofn_softc *sc;
	struct cryptkop *krp;
{
	int ip = 0, bits, err = 0;
	int mshift, eshift, nshift;

	if (krp->krp_param[LOFN_MODEXP_PAR_M].crp_nbits > 1024) {
		err = ERANGE;
		goto errout;
	}

	/* Poll until done... */
	while (1) {
		if (READ_REG(sc, LOFN_REL_SR) & LOFN_SR_DONE)
			break;
	}

	/* Zero out registers. */
	lofn_zero_reg(sc, 0);
	lofn_zero_reg(sc, 1);
	lofn_zero_reg(sc, 2);
	lofn_zero_reg(sc, 3);

	/* Write out M... */
	bits = lofn_norm_sigbits(krp->krp_param[LOFN_MODEXP_PAR_M].crp_p,
	    krp->krp_param[LOFN_MODEXP_PAR_M].crp_nbits);
	if (bits > 1024) {
		err = E2BIG;
		goto errout;
	}
	bzero(&sc->sc_tmp, sizeof(sc->sc_tmp));
	bcopy(krp->krp_param[LOFN_MODEXP_PAR_M].crp_p, &sc->sc_tmp,
	    (bits + 7) / 8);
	lofn_write_reg(sc, 0, &sc->sc_tmp);

	mshift = 1024 - bits;
	WRITE_REG(sc, LOFN_LENADDR(LOFN_WIN_2, 0), 1024);
	if (mshift != 0) {
		WRITE_REG(sc, LOFN_REL_INSTR + ip,
		    LOFN_INSTR2(0, OP_CODE_SL, 0, 0, mshift));
		ip += 4;

		WRITE_REG(sc, LOFN_REL_INSTR + ip,
		    LOFN_INSTR2(0, OP_CODE_TAG, 0, 0, bits));
		ip += 4;
	}

	/* Write out E... */
	bits = lofn_norm_sigbits(krp->krp_param[LOFN_MODEXP_PAR_E].crp_p,
	    krp->krp_param[LOFN_MODEXP_PAR_E].crp_nbits);
	if (bits > 1024) {
		err = E2BIG;
		goto errout;
	}
	if (bits < 1) {
		err = ERANGE;
		goto errout;
	}
	bzero(&sc->sc_tmp, sizeof(sc->sc_tmp));
	bcopy(krp->krp_param[LOFN_MODEXP_PAR_E].crp_p, &sc->sc_tmp,
	    (bits + 7) / 8);
	lofn_write_reg(sc, 1, &sc->sc_tmp);

	eshift = 1024 - bits;
	WRITE_REG(sc, LOFN_LENADDR(LOFN_WIN_2, 1), 1024);
	if (eshift != 0) {
		WRITE_REG(sc, LOFN_REL_INSTR + ip,
		    LOFN_INSTR2(0, OP_CODE_SL, 1, 1, eshift));
		ip += 4;

		WRITE_REG(sc, LOFN_REL_INSTR + ip,
		    LOFN_INSTR2(0, OP_CODE_TAG, 0, 0, bits));
		ip += 4;
	}

	/* Write out N... */
	bits = lofn_norm_sigbits(krp->krp_param[LOFN_MODEXP_PAR_N].crp_p,
	    krp->krp_param[LOFN_MODEXP_PAR_N].crp_nbits);
	if (bits > 1024) {
		err = E2BIG;
		goto errout;
	}
	if (bits < 5) {
		err = ERANGE;
		goto errout;
	}
	bzero(&sc->sc_tmp, sizeof(sc->sc_tmp));
	bcopy(krp->krp_param[LOFN_MODEXP_PAR_N].crp_p, &sc->sc_tmp,
	    (bits + 7) / 8);
	lofn_write_reg(sc, 2, &sc->sc_tmp);

	nshift = 1024 - bits;
	WRITE_REG(sc, LOFN_LENADDR(LOFN_WIN_2, 2), 1024);
	if (nshift != 0) {
		WRITE_REG(sc, LOFN_REL_INSTR + ip,
		    LOFN_INSTR2(0, OP_CODE_SL, 2, 2, nshift));
		ip += 4;

		WRITE_REG(sc, LOFN_REL_INSTR + ip,
		    LOFN_INSTR2(0, OP_CODE_TAG, 0, 0, bits));
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
		    LOFN_INSTR2(OP_DONE, OP_CODE_SR, 3, 3, nshift));
		ip += 4;
	}

	WRITE_REG(sc, LOFN_REL_CR, 0);

	while (1) {
		if (READ_REG(sc, LOFN_REL_SR) & LOFN_SR_DONE)
			break;
	}

	lofn_read_reg(sc, 3, &sc->sc_tmp);
	bcopy(sc->sc_tmp.b, krp->krp_param[LOFN_MODEXP_PAR_C].crp_p,
	    (krp->krp_param[LOFN_MODEXP_PAR_C].crp_nbits + 7) / 8);
	crypto_kdone(krp);
	return (0);

errout:
	bzero(&sc->sc_tmp, sizeof(sc->sc_tmp));
	lofn_zero_reg(sc, 0);
	lofn_zero_reg(sc, 1);
	lofn_zero_reg(sc, 2);
	lofn_zero_reg(sc, 3);
	krp->krp_status = err;
	crypto_kdone(krp);
	return (0);
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
