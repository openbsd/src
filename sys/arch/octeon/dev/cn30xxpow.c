/*	$OpenBSD: cn30xxpow.c,v 1.14 2017/11/05 05:17:55 visa Exp $	*/

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
#include <sys/kernel.h>				/* hz */
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/octeonvar.h>

#include <octeon/dev/iobusvar.h>
#include <octeon/dev/cn30xxciureg.h>	/* XXX */
#include <octeon/dev/cn30xxpowreg.h>
#include <octeon/dev/cn30xxpowvar.h>

struct cn30xxpow_intr_handle {
	void				*pi_ih;
	struct cn30xxpow_softc		*pi_sc;
	int				pi_group;
	void				(*pi_cb)(void *, uint64_t *);
	void				*pi_data;
};

void	cn30xxpow_bootstrap(struct octeon_config *);

void	cn30xxpow_init(struct cn30xxpow_softc *);
void	cn30xxpow_init_regs(struct cn30xxpow_softc *);
int	cn30xxpow_tag_sw_poll(void);
void	cn30xxpow_tag_sw_wait(void);
void	cn30xxpow_config_int_pc(struct cn30xxpow_softc *, int);
void	cn30xxpow_config_int(struct cn30xxpow_softc *, int,
	    uint64_t, uint64_t, uint64_t);
void	cn30xxpow_intr_work(struct cn30xxpow_softc *,
	    struct cn30xxpow_intr_handle *, int);
int	cn30xxpow_intr(void *);

/* XXX */
struct cn30xxpow_softc	cn30xxpow_softc;

/*
 * XXX: parameter tuning is needed: see files.octeon
 */
#ifndef OCTEON_ETH_RING_MAX
#define OCTEON_ETH_RING_MAX 512
#endif
#ifndef OCTEON_ETH_RING_MIN
#define OCTEON_ETH_RING_MIN 1
#endif

#ifdef OCTEON_ETH_INTR_FEEDBACK_RING
int max_recv_cnt = OCTEON_ETH_RING_MAX;
int min_recv_cnt = OCTEON_ETH_RING_MIN;
int recv_cnt = OCTEON_ETH_RING_MIN;
int int_rate = 1;
#else
/* infinity */
int max_recv_cnt = 0;
int min_recv_cnt = 0;
int recv_cnt = 0;
#endif

/* -------------------------------------------------------------------------- */

/* ---- operation primitive functions */

/* 5.11.1 Load Operations */

/* 5.11.2 IOBDMA Operations */

/* 5.11.3 Store Operations */

/* -------------------------------------------------------------------------- */

/* ---- utility functions */

void
cn30xxpow_work_request_async(uint64_t scraddr, uint64_t wait)
{
        cn30xxpow_ops_get_work_iobdma(scraddr, wait);
}

uint64_t *
cn30xxpow_work_response_async(uint64_t scraddr)
{
	uint64_t result;

	octeon_synciobdma();
	result = octeon_cvmseg_read_8(scraddr);

	return (result & POW_IOBDMA_GET_WORK_RESULT_NO_WORK) ?
	    NULL :
	    (uint64_t *)PHYS_TO_XKPHYS(
		result & POW_IOBDMA_GET_WORK_RESULT_ADDR, CCA_CACHED);
}

/* ---- tag switch */

/*
 * "RDHWR rt, $30" returns:
 *	0 => pending bit is set
 *	1 => pending bit is clear
 */

/* return 1 if pending bit is clear (ready) */
int
cn30xxpow_tag_sw_poll(void)
{
	uint64_t result;

	__asm volatile (
		"	.set	push		\n"
		"	.set	noreorder	\n"
		"	.set	arch=mips64r2	\n"
		"	rdhwr	%[result], $30	\n"
		"	.set	pop		\n"
		: [result]"=r"(result)
	);
	return (int)result;
}

void
cn30xxpow_tag_sw_wait(void)
{

	while (cn30xxpow_tag_sw_poll() == 0)
		continue;
}

/* -------------------------------------------------------------------------- */

/* ---- initialization and configuration */

void
cn30xxpow_bootstrap(struct octeon_config *mcp)
{
	struct cn30xxpow_softc *sc = &cn30xxpow_softc;

	sc->sc_regt = mcp->mc_iobus_bust;
	/* XXX */

	cn30xxpow_init(sc);
}

void
cn30xxpow_config_int(struct cn30xxpow_softc *sc, int group,
   uint64_t tc_thr, uint64_t ds_thr, uint64_t iq_thr)
{
	uint64_t wq_int_thr;

	wq_int_thr =
	    POW_WQ_INT_THRX_TC_EN |
	    (tc_thr << POW_WQ_INT_THRX_TC_THR_SHIFT) |
	    (ds_thr << POW_WQ_INT_THRX_DS_THR_SHIFT) |
	    (iq_thr << POW_WQ_INT_THRX_IQ_THR_SHIFT);
	_POW_WR8(sc, POW_WQ_INT_THR0_OFFSET + (group * 8), wq_int_thr);
}

/*
 * interrupt threshold configuration
 *
 * => DS / IQ
 *    => ...
 * => time counter threshold
 *    => unit is 1msec
 *    => each group can set timeout
 * => temporary disable bit
 *    => use CIU generic timer
 */

void
cn30xxpow_config(struct cn30xxpow_softc *sc, int group)
{

	cn30xxpow_config_int(sc, group,
	    0x0f,		/* TC */
	    0x00,		/* DS */
	    0x00);		/* IQ */
}

void *
cn30xxpow_intr_establish(int group, int level,
    void (*cb)(void *, uint64_t *), void (*fcb)(int*, int *, uint64_t, void *),
    void *data, char *what)
{
	struct cn30xxpow_intr_handle *pow_ih;

	KASSERT(group >= 0);
	KASSERT(group < 16);

	pow_ih = malloc(sizeof(*pow_ih), M_DEVBUF, M_NOWAIT);
	if (pow_ih == NULL)
		return NULL;

	pow_ih->pi_ih = octeon_intr_establish(
	    ffs64(CIU_INTX_SUM0_WORKQ_0) - 1 + group,
	    level,
	    cn30xxpow_intr, pow_ih, what);
	KASSERT(pow_ih->pi_ih != NULL);

	pow_ih->pi_sc = &cn30xxpow_softc;
	pow_ih->pi_group = group;
	pow_ih->pi_cb = cb;
	pow_ih->pi_data = data;

	return pow_ih;
}

void
cn30xxpow_init(struct cn30xxpow_softc *sc)
{
	cn30xxpow_init_regs(sc);

	sc->sc_int_pc_base = 10000;
	cn30xxpow_config_int_pc(sc, sc->sc_int_pc_base);
}

void
cn30xxpow_init_regs(struct cn30xxpow_softc *sc)
{
	int status;

	status = bus_space_map(sc->sc_regt, POW_BASE, POW_SIZE, 0,
	    &sc->sc_regh);
	if (status != 0)
		panic("can't map %s space", "pow register");
}

/* -------------------------------------------------------------------------- */

/* ---- interrupt handling */

/*
 * Interrupt handling by fixed count.
 *
 * XXX the fixed count (MAX_RX_CNT) could be changed dynamically?
 *
 * XXX this does not utilize "tag switch" very well
 */
/*
 * usually all packet recieve
 */
#define MAX_RX_CNT 0x7fffffff 

void
cn30xxpow_intr_work(struct cn30xxpow_softc *sc,
    struct cn30xxpow_intr_handle *pow_ih, int max_recv_cnt)
{
	uint64_t *work;
	uint64_t count = 0;
	uint32_t coreid = octeon_get_coreid();
	int recv_cnt = MAX_RX_CNT;

	_POW_WR8(sc, POW_PP_GRP_MSK_OFFSET(coreid), 1ULL << pow_ih->pi_group);

	if (max_recv_cnt > 0)
		recv_cnt = max_recv_cnt - 1;

	cn30xxpow_tag_sw_wait();
	cn30xxpow_work_request_async(OCTEON_CVMSEG_OFFSET(csm_pow_intr),
	    POW_NO_WAIT);

	for (count = 0; count < recv_cnt; count++) {
		work = (uint64_t *)cn30xxpow_work_response_async(
		    OCTEON_CVMSEG_OFFSET(csm_pow_intr));
		if (work == NULL)
			return;
		cn30xxpow_tag_sw_wait();
		cn30xxpow_work_request_async(
		    OCTEON_CVMSEG_OFFSET(csm_pow_intr), POW_NO_WAIT);
		(*pow_ih->pi_cb)(pow_ih->pi_data, work);
	}

	work = (uint64_t *)cn30xxpow_work_response_async(
	    OCTEON_CVMSEG_OFFSET(csm_pow_intr));
	if (work == NULL)
		return;

	(*pow_ih->pi_cb)(pow_ih->pi_data, work);
}

int
cn30xxpow_intr(void *data)
{
	struct cn30xxpow_intr_handle *pow_ih = data;
	struct cn30xxpow_softc *sc = pow_ih->pi_sc;
	uint64_t wq_int_mask = 0x1ULL << pow_ih->pi_group;

	cn30xxpow_intr_work(sc, pow_ih, recv_cnt);

	_POW_WR8(sc, POW_WQ_INT_OFFSET, wq_int_mask << POW_WQ_INT_WQ_INT_SHIFT);
	return 1;
}
