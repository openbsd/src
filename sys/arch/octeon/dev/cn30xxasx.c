/*	$OpenBSD: cn30xxasx.c,v 1.6 2014/08/11 18:29:56 miod Exp $	*/

/*
 * Copyright (c) 2007 Internet Initiative Japan, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <machine/octeonvar.h>

#include <octeon/dev/cn30xxasxreg.h>
#include <octeon/dev/cn30xxasxvar.h>

#ifdef OCTEON_ETH_DEBUG
void	cn30xxasx_intr_rml(void *);
struct cn30xxasx_softc *__cn30xxasx_softc;
#endif

/* XXX */
void
cn30xxasx_init(struct cn30xxasx_attach_args *aa,
    struct cn30xxasx_softc **rsc)
{
	struct cn30xxasx_softc *sc;
	int status;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK | M_ZERO);
	if (sc == NULL)
		panic("can't allocate memory: %s", __func__);

	sc->sc_port = aa->aa_port;
	sc->sc_regt = aa->aa_regt;

	status = bus_space_map(sc->sc_regt, ASX0_BASE, ASX0_SIZE, 0,
	    &sc->sc_regh);
	if (status != 0)
		panic("can't map %s space", "asx register");

	*rsc = sc;

#ifdef OCTEON_ETH_DEBUG
	if (__cn30xxasx_softc == NULL)
		__cn30xxasx_softc = sc;
#endif
}

#define	_ASX_RD8(sc, off) \
	bus_space_read_8((sc)->sc_regt, (sc)->sc_regh, (off))
#define	_ASX_WR8(sc, off, v) \
	bus_space_write_8((sc)->sc_regt, (sc)->sc_regh, (off), (v))

int	cn30xxasx_enable_tx(struct cn30xxasx_softc *, int);
int	cn30xxasx_enable_rx(struct cn30xxasx_softc *, int);
#ifdef OCTEON_ETH_DEBUG
int	cn30xxasx_enable_intr(struct cn30xxasx_softc *, int);
#endif

int
cn30xxasx_enable(struct cn30xxasx_softc *sc, int enable)
{

#ifdef OCTEON_ETH_DEBUG
	cn30xxasx_enable_intr(sc, enable);
#endif
	cn30xxasx_enable_tx(sc, enable);
	cn30xxasx_enable_rx(sc, enable);
	return 0;
}

int
cn30xxasx_enable_tx(struct cn30xxasx_softc *sc, int enable)
{
	uint64_t asx_tx_port;

	asx_tx_port = _ASX_RD8(sc, ASX0_TX_PRT_EN_OFFSET);
	if (enable)
		SET(asx_tx_port, 1 << sc->sc_port);
	else
		CLR(asx_tx_port, 1 << sc->sc_port);
	_ASX_WR8(sc, ASX0_TX_PRT_EN_OFFSET, asx_tx_port);
	return 0;
}

int
cn30xxasx_enable_rx(struct cn30xxasx_softc *sc, int enable)
{
	uint64_t asx_rx_port;

	asx_rx_port = _ASX_RD8(sc, ASX0_RX_PRT_EN_OFFSET);
	if (enable)
		SET(asx_rx_port, 1 << sc->sc_port);
	else
		CLR(asx_rx_port, 1 << sc->sc_port);
	_ASX_WR8(sc, ASX0_RX_PRT_EN_OFFSET, asx_rx_port);
	return 0;
}

#if defined(OCTEON_ETH_DEBUG)
int	cn30xxasx_intr_rml_verbose;

void
cn30xxasx_intr_rml(void *arg)
{
	struct cn30xxasx_softc *sc = __cn30xxasx_softc;
	uint64_t reg;

	reg = cn30xxasx_int_summary(sc);
	if (cn30xxasx_intr_rml_verbose)
		printf("%s: ASX_INT_REG=0x%016llx\n", __func__, reg);
}

int
cn30xxasx_enable_intr(struct cn30xxasx_softc *sc, int enable)
{
	uint64_t asx_int_xxx = 0;

	SET(asx_int_xxx,
	    ASX0_INT_REG_TXPSH |
	    ASX0_INT_REG_TXPOP |
	    ASX0_INT_REG_OVRFLW);
	_ASX_WR8(sc, ASX0_INT_REG_OFFSET, asx_int_xxx);
	_ASX_WR8(sc, ASX0_INT_EN_OFFSET, enable ? asx_int_xxx : 0);
	return 0;
}
#endif

int
cn30xxasx_clk_set(struct cn30xxasx_softc *sc, int tx_setting, int rx_setting)
{
	_ASX_WR8(sc, ASX0_TX_CLK_SET0_OFFSET + 8 * sc->sc_port, tx_setting);
	_ASX_WR8(sc, ASX0_RX_CLK_SET0_OFFSET + 8 * sc->sc_port, rx_setting);
	return 0;
}

#ifdef OCTEON_ETH_DEBUG
uint64_t
cn30xxasx_int_summary(struct cn30xxasx_softc *sc)
{
	uint64_t summary;

	summary = _ASX_RD8(sc, ASX0_INT_REG_OFFSET);
	_ASX_WR8(sc, ASX0_INT_REG_OFFSET, summary);
	return summary;
}

#define	_ENTRY(x)	{ #x, x##_OFFSET }

struct cn30xxasx_dump_reg_ {
	const char *name;
	size_t	offset;
};

void		cn30xxasx_dump(void);

const struct cn30xxasx_dump_reg_ cn30xxasx_dump_regs_[] = {
	_ENTRY(ASX0_RX_PRT_EN),
	_ENTRY(ASX0_TX_PRT_EN),
	_ENTRY(ASX0_INT_REG),
	_ENTRY(ASX0_INT_EN),
	_ENTRY(ASX0_RX_CLK_SET0),
	_ENTRY(ASX0_RX_CLK_SET1),
	_ENTRY(ASX0_RX_CLK_SET2),
	_ENTRY(ASX0_PRT_LOOP),
	_ENTRY(ASX0_TX_CLK_SET0),
	_ENTRY(ASX0_TX_CLK_SET1),
	_ENTRY(ASX0_TX_CLK_SET2),
	_ENTRY(ASX0_COMP_BYP),
	_ENTRY(ASX0_TX_HI_WATER000),
	_ENTRY(ASX0_TX_HI_WATER001),
	_ENTRY(ASX0_TX_HI_WATER002),
	_ENTRY(ASX0_GMII_RX_CLK_SET),
	_ENTRY(ASX0_GMII_RX_DAT_SET),
	_ENTRY(ASX0_MII_RX_DAT_SET),
};

void
cn30xxasx_dump(void)
{
	struct cn30xxasx_softc *sc = __cn30xxasx_softc;
	const struct cn30xxasx_dump_reg_ *reg;
	uint64_t tmp;
	int i;

	for (i = 0; i < (int)nitems(cn30xxasx_dump_regs_); i++) {
		reg = &cn30xxasx_dump_regs_[i];
		tmp = _ASX_RD8(sc, reg->offset);
		printf("\t%-24s: %016llx\n", reg->name, tmp);
	}
}
#endif
