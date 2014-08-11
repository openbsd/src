/*	$OpenBSD: cn30xxpow.c,v 1.6 2014/08/11 18:29:56 miod Exp $	*/

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
#include <sys/types.h>
#include <sys/kernel.h>				/* hz */
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/octeonvar.h>

#include <octeon/dev/iobusvar.h>
#include <octeon/dev/cn30xxciureg.h>	/* XXX */
#include <octeon/dev/cn30xxpowreg.h>
#include <octeon/dev/cn30xxpowvar.h>

extern int ipflow_fastforward_disable_flags;

struct cn30xxpow_intr_handle {
	void				*pi_ih;
	struct cn30xxpow_softc		*pi_sc;
	int				pi_group;
	void				(*pi_cb)(void *, uint64_t *);
	void				*pi_data;

#ifdef OCTEON_ETH_DEBUG
#define	_EV_PER_N	32	/* XXX */
#define	_EV_IVAL_N	32	/* XXX */
	int				pi_first;
	struct timeval			pi_last;
#endif
};

void	cn30xxpow_bootstrap(struct octeon_config *);

#ifdef OCTEON_ETH_DEBUG
void	cn30xxpow_intr_rml(void *);

void	cn30xxpow_intr_debug_init(struct cn30xxpow_intr_handle *, int);
void	cn30xxpow_intr_work_debug_ival(struct cn30xxpow_softc *,
	    struct cn30xxpow_intr_handle *);
#endif
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

#ifdef OCTEON_ETH_DEBUG
void	cn30xxpow_dump(void);
#endif

/* XXX */
struct cn30xxpow_softc	cn30xxpow_softc;

#ifdef OCTEON_ETH_DEBUG
struct cn30xxpow_softc *__cn30xxpow_softc;
#endif

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

	mips_sync();
	result = octeon_cvmseg_read_8(scraddr);

	return (result & POW_IOBDMA_GET_WORK_RESULT_NO_WORK) ?
	    NULL :
	    (uint64_t *)PHYS_TO_CKSEG0(
		result & POW_IOBDMA_GET_WORK_RESULT_ADDR);
}

/* ---- status by coreid */

static inline uint64_t
cn30xxpow_status_by_coreid_pend_tag(int coreid)
{
	return cn30xxpow_ops_pow_status(coreid, 0, 0, 0);
}

static inline uint64_t
cn30xxpow_status_by_coreid_pend_wqp(int coreid)
{
	return cn30xxpow_ops_pow_status(coreid, 0, 0, 1);
}

static inline uint64_t
cn30xxpow_status_by_coreid_cur_tag_next(int coreid)
{
	return cn30xxpow_ops_pow_status(coreid, 0, 1, 0);
}

static inline uint64_t
cn30xxpow_status_by_coreid_cur_tag_prev(int coreid)
{
	return cn30xxpow_ops_pow_status(coreid, 1, 1, 0);
}

static inline uint64_t
cn30xxpow_status_by_coreid_cur_wqp_next(int coreid)
{
	return cn30xxpow_ops_pow_status(coreid, 0, 1, 1);
}

static inline uint64_t
cn30xxpow_status_by_coreid_cur_wqp_prev(int coreid)
{
	return cn30xxpow_ops_pow_status(coreid, 1, 1, 1);
}

/* ---- status by index */

static inline uint64_t
cn30xxpow_status_by_index_tag(int index)
{
	return cn30xxpow_ops_pow_memory(index, 0, 0);
}

static inline uint64_t
cn30xxpow_status_by_index_wqp(int index)
{
	return cn30xxpow_ops_pow_memory(index, 0, 1);
}

static inline uint64_t
cn30xxpow_status_by_index_desched(int index)
{
	return cn30xxpow_ops_pow_memory(index, 1, 0);
}

/* ---- status by qos level */

static inline uint64_t
cn30xxpow_status_by_qos_free_loc(int qos)
{
	return cn30xxpow_ops_pow_idxptr(qos, 0, 0);
}

/* ---- status by desched group */

static inline uint64_t
cn30xxpow_status_by_grp_nosched_des(int grp)
{
	return cn30xxpow_ops_pow_idxptr(grp, 0, 1);
}

/* ---- status by memory input queue */

static inline uint64_t
cn30xxpow_status_by_queue_remote_head(int queue)
{
	return cn30xxpow_ops_pow_idxptr(queue, 1, 0);
}

static inline uint64_t
cn30xxpow_status_by_queue_remote_tail(int queue)
{
	return cn30xxpow_ops_pow_idxptr(queue, 1, 0);
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

#ifdef OCTEON_ETH_DEBUG
	__cn30xxpow_softc = sc;
#endif

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
	KASSERT(pow_ih != NULL);	/* XXX handle failure */

	pow_ih->pi_ih = octeon_intr_establish(
	    ffs64(CIU_INTX_SUM0_WORKQ_0) - 1 + group,
	    level,
	    cn30xxpow_intr, pow_ih, what);
	KASSERT(pow_ih->pi_ih != NULL);

	pow_ih->pi_sc = &cn30xxpow_softc;	/* XXX */
	pow_ih->pi_group = group;
	pow_ih->pi_cb = cb;
	pow_ih->pi_data = data;

#ifdef OCTEON_ETH_DEBUG
	cn30xxpow_intr_debug_init(pow_ih, group);
#endif
	return pow_ih;
}

#ifdef OCTEON_ETH_DEBUG

void
cn30xxpow_intr_debug_init(struct cn30xxpow_intr_handle *pow_ih, int group)
{
	pow_ih->pi_first = 1;

}
#endif

void
cn30xxpow_init(struct cn30xxpow_softc *sc)
{
	cn30xxpow_init_regs(sc);

	sc->sc_int_pc_base = 10000;
	cn30xxpow_config_int_pc(sc, sc->sc_int_pc_base);

#ifdef OCTEON_ETH_DEBUG
	cn30xxpow_error_int_enable(sc, 1);
#endif
}

void
cn30xxpow_init_regs(struct cn30xxpow_softc *sc)
{
	int status;

	status = bus_space_map(sc->sc_regt, POW_BASE, POW_SIZE, 0,
	    &sc->sc_regh);
	if (status != 0)
		panic("can't map %s space", "pow register");

#ifdef OCTEON_ETH_DEBUG
	_POW_WR8(sc, POW_ECC_ERR_OFFSET,
	    POW_ECC_ERR_IOP_IE | POW_ECC_ERR_RPE_IE |
	    POW_ECC_ERR_DBE_IE | POW_ECC_ERR_SBE_IE);
#endif
}

/* -------------------------------------------------------------------------- */

/* ---- interrupt handling */

#ifdef OCTEON_ETH_DEBUG
void
cn30xxpow_intr_work_debug_ival(struct cn30xxpow_softc *sc,
    struct cn30xxpow_intr_handle *pow_ih)
{
	struct timeval now;
	struct timeval ival;

	microtime(&now);
	if (__predict_false(pow_ih->pi_first == 1)) {
		pow_ih->pi_first = 0;
		goto stat_done;
	}
	timersub(&now, &pow_ih->pi_last, &ival);
	if (ival.tv_sec != 0)
		goto stat_done;	/* XXX */

stat_done:
	pow_ih->pi_last = now;	/* struct copy */
}
#endif

#ifdef OCTEON_ETH_DEBUG
#define _POW_INTR_WORK_DEBUG_IVAL(sc, ih) \
	    cn30xxpow_intr_work_debug_ival((sc), (ih))
#else
#define _POW_INTR_WORK_DEBUG_IVAL(sc, ih) \
	    do {} while (0)
#endif

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
	int recv_cnt = MAX_RX_CNT;

	/* s = splhigh(); */
	_POW_WR8(sc, POW_PP_GRP_MSK0_OFFSET, 1ULL << pow_ih->pi_group);

	if (max_recv_cnt > 0)
		recv_cnt = max_recv_cnt - 1;

	_POW_INTR_WORK_DEBUG_IVAL(sc, pow_ih);

	cn30xxpow_tag_sw_wait();
	cn30xxpow_work_request_async(OCTEON_CVMSEG_OFFSET(csm_pow_intr),
	    POW_NO_WAIT);

	for (count = 0; count < recv_cnt; count++) {
		work = (uint64_t *)cn30xxpow_work_response_async(
		    OCTEON_CVMSEG_OFFSET(csm_pow_intr));
		if (work == NULL)
			goto done;
		cn30xxpow_tag_sw_wait();
		cn30xxpow_work_request_async(
		    OCTEON_CVMSEG_OFFSET(csm_pow_intr), POW_NO_WAIT);
		(*pow_ih->pi_cb)(pow_ih->pi_data, work);
	}

	work = (uint64_t *)cn30xxpow_work_response_async(
	    OCTEON_CVMSEG_OFFSET(csm_pow_intr));
	if (work == NULL)
		goto done;

	(*pow_ih->pi_cb)(pow_ih->pi_data, work);
	count++;

done:
	;
	/* KASSERT(work == NULL); */
	/* KASSERT(count > 0); */

	/* _POW_WR8(sc, POW_PP_GRP, 0)ULL; */
	/* splx(s); */
}

int
cn30xxpow_intr(void *data)
{
	struct cn30xxpow_intr_handle *pow_ih = data;
	struct cn30xxpow_softc *sc = pow_ih->pi_sc;
	uint64_t wq_int_mask = 0x1ULL << pow_ih->pi_group;

#if 0
	if (ipflow_fastforward_disable_flags == 0)
		cn30xxpow_intr_work(sc, pow_ih, -1);
	else
		cn30xxpow_intr_work(sc, pow_ih, recv_cnt);
#else
	cn30xxpow_intr_work(sc, pow_ih, recv_cnt);
#endif

	_POW_WR8(sc, POW_WQ_INT_OFFSET, wq_int_mask << POW_WQ_INT_WQ_INT_SHIFT);
	return 1;
}

/* -------------------------------------------------------------------------- */

/* ---- debug configuration */

#ifdef OCTEON_ETH_DEBUG

void
cn30xxpow_error_int_enable(void *data, int enable)
{
	struct cn30xxpow_softc *sc = data;
	uint64_t pow_error_int_xxx;

	pow_error_int_xxx =
	    POW_ECC_ERR_IOP | POW_ECC_ERR_RPE |
	    POW_ECC_ERR_DBE | POW_ECC_ERR_SBE;
	_POW_WR8(sc, POW_ECC_ERR_OFFSET, pow_error_int_xxx);
	_POW_WR8(sc, POW_ECC_ERR_OFFSET, enable ? pow_error_int_xxx : 0);
}

uint64_t
cn30xxpow_error_int_summary(void *data)
{
	struct cn30xxpow_softc *sc = data;
	uint64_t summary;

	summary = _POW_RD8(sc, POW_ECC_ERR_OFFSET);
	_POW_WR8(sc, POW_ECC_ERR_OFFSET, summary);
	return summary;
}

#endif

/* -------------------------------------------------------------------------- */

/* ---- debug counter */

#ifdef OCTEON_ETH_DEBUG
int	cn30xxpow_intr_rml_verbose;

void
cn30xxpow_intr_rml(void *arg)
{
	struct cn30xxpow_softc *sc;
	uint64_t reg;

	sc = __cn30xxpow_softc;
	KASSERT(sc != NULL);
	reg = cn30xxpow_error_int_summary(sc);
	if (cn30xxpow_intr_rml_verbose)
		printf("%s: POW_ECC_ERR=0x%016llx\n", __func__, reg);
}
#endif

/* -------------------------------------------------------------------------- */

/* ---- debug dump */

#ifdef OCTEON_ETH_DEBUG

void	cn30xxpow_dump_reg(void);
void	cn30xxpow_dump_ops(void);

void
cn30xxpow_dump(void)
{
	cn30xxpow_dump_reg();
	cn30xxpow_dump_ops();
}

/* ---- register dump */

struct cn30xxpow_dump_reg_entry {
	const char *name;
	size_t offset;
};

#define	_ENTRY(x)	{ #x, x##_OFFSET }
#define	_ENTRY_0_7(x) \
	_ENTRY(x## 0), _ENTRY(x## 1), _ENTRY(x## 2), _ENTRY(x## 3), \
	_ENTRY(x## 4), _ENTRY(x## 5), _ENTRY(x## 6), _ENTRY(x## 7)
#define	_ENTRY_0_15(x) \
	_ENTRY(x## 0), _ENTRY(x## 1), _ENTRY(x## 2), _ENTRY(x## 3), \
	_ENTRY(x## 4), _ENTRY(x## 5), _ENTRY(x## 6), _ENTRY(x## 7), \
	_ENTRY(x## 8), _ENTRY(x## 9), _ENTRY(x##10), _ENTRY(x##11), \
	_ENTRY(x##12), _ENTRY(x##13), _ENTRY(x##14), _ENTRY(x##15)

const struct cn30xxpow_dump_reg_entry cn30xxpow_dump_reg_entries[] = {
	_ENTRY		(POW_PP_GRP_MSK0),
	_ENTRY		(POW_PP_GRP_MSK1),
	_ENTRY_0_15	(POW_WQ_INT_THR),
	_ENTRY_0_15	(POW_WQ_INT_CNT),
	_ENTRY_0_7	(POW_QOS_THR),
	_ENTRY_0_7	(POW_QOS_RND),
	_ENTRY		(POW_WQ_INT),
	_ENTRY		(POW_WQ_INT_PC),
	_ENTRY		(POW_NW_TIM),
	_ENTRY		(POW_ECC_ERR),
	_ENTRY		(POW_NOS_CNT),
	_ENTRY_0_15	(POW_WS_PC),
	_ENTRY_0_7	(POW_WA_PC),
	_ENTRY_0_7	(POW_IQ_CNT),
	_ENTRY		(POW_WA_COM_PC),
	_ENTRY		(POW_IQ_COM_CNT),
	_ENTRY		(POW_TS_PC),
	_ENTRY		(POW_DS_PC),
	_ENTRY		(POW_BIST_STAT)
};

#undef _ENTRY

void
cn30xxpow_dump_reg(void)
{
	struct cn30xxpow_softc *sc = __cn30xxpow_softc;
	const struct cn30xxpow_dump_reg_entry *entry;
	uint64_t tmp;
	int i;

	for (i = 0; i < (int)nitems(cn30xxpow_dump_reg_entries); i++) {
		entry = &cn30xxpow_dump_reg_entries[i];
		tmp = _POW_RD8(sc, entry->offset);
		printf("\t%-24s: %16llx\n", entry->name, tmp);
	}
}

/* ---- operations dump */

struct cn30xxpow_dump_ops_entry {
	const char *name;
	uint64_t (*func)(int);
};

void	cn30xxpow_dump_ops_coreid(int);
void	cn30xxpow_dump_ops_index(int);
void	cn30xxpow_dump_ops_qos(int);
void	cn30xxpow_dump_ops_grp(int);
void	cn30xxpow_dump_ops_queue(int);
void	cn30xxpow_dump_ops_common(const struct cn30xxpow_dump_ops_entry *,
	    size_t, const char *, int);

#define	_ENTRY_COMMON(name, prefix, x, y) \
	{ #name "_" #x, cn30xxpow_status_by_##name##_##x }

const struct cn30xxpow_dump_ops_entry cn30xxpow_dump_ops_coreid_entries[] = {
#define	_ENTRY(x, y)	_ENTRY_COMMON(coreid, POW_STATUS_LOAD_RESULT, x, y)
	_ENTRY(pend_tag, PEND_TAG),
	_ENTRY(pend_wqp, PEND_WQP),
	_ENTRY(cur_tag_next, CUR_TAG_NEXT),
	_ENTRY(cur_tag_prev, CUR_TAG_PREV),
	_ENTRY(cur_wqp_next, CUR_WQP_NEXT),
	_ENTRY(cur_wqp_prev, CUR_WQP_PREV)
#undef _ENTRY
};

const struct cn30xxpow_dump_ops_entry cn30xxpow_dump_ops_index_entries[] = {
#define	_ENTRY(x, y)	_ENTRY_COMMON(index, POW_MEMORY_LOAD_RESULT, x, y)
	_ENTRY(tag, TAG),
	_ENTRY(wqp, WQP),
	_ENTRY(desched, DESCHED)
#undef _ENTRY
};

const struct cn30xxpow_dump_ops_entry cn30xxpow_dump_ops_qos_entries[] = {
#define	_ENTRY(x, y)	_ENTRY_COMMON(qos, POW_IDXPTR_LOAD_RESULT_QOS, x, y)
	_ENTRY(free_loc, FREE_LOC)
#undef _ENTRY
};

const struct cn30xxpow_dump_ops_entry cn30xxpow_dump_ops_grp_entries[] = {
#define	_ENTRY(x, y)	_ENTRY_COMMON(grp, POW_IDXPTR_LOAD_RESULT_GRP, x, y)
	_ENTRY(nosched_des, NOSCHED_DES)
#undef _ENTRY
};

const struct cn30xxpow_dump_ops_entry cn30xxpow_dump_ops_queue_entries[] = {
#define	_ENTRY(x, y)	_ENTRY_COMMON(queue, POW_IDXPTR_LOAD_RESULT_QUEUE, x, y)
	_ENTRY(remote_head, REMOTE_HEAD),
	_ENTRY(remote_tail, REMOTE_TAIL)
#undef _ENTRY
};

void
cn30xxpow_dump_ops(void)
{
	int i;

	/* XXX */
	for (i = 0; i < 2/* XXX */; i++)
		cn30xxpow_dump_ops_coreid(i);

	/* XXX */
	cn30xxpow_dump_ops_index(0);

	for (i = 0; i < 8; i++)
		cn30xxpow_dump_ops_qos(i);

	for (i = 0; i < 16; i++)
		cn30xxpow_dump_ops_grp(i);

	for (i = 0; i < 16; i++)
		cn30xxpow_dump_ops_queue(i);
}

void
cn30xxpow_dump_ops_coreid(int coreid)
{
	cn30xxpow_dump_ops_common(cn30xxpow_dump_ops_coreid_entries,
	    nitems(cn30xxpow_dump_ops_coreid_entries), "coreid", coreid);
}

void
cn30xxpow_dump_ops_index(int index)
{
	cn30xxpow_dump_ops_common(cn30xxpow_dump_ops_index_entries,
	    nitems(cn30xxpow_dump_ops_index_entries), "index", index);
}

void
cn30xxpow_dump_ops_qos(int qos)
{
	cn30xxpow_dump_ops_common(cn30xxpow_dump_ops_qos_entries,
	    nitems(cn30xxpow_dump_ops_qos_entries), "qos", qos);
}

void
cn30xxpow_dump_ops_grp(int grp)
{
	cn30xxpow_dump_ops_common(cn30xxpow_dump_ops_grp_entries,
	    nitems(cn30xxpow_dump_ops_grp_entries), "grp", grp);
}

void
cn30xxpow_dump_ops_queue(int queue)
{
	cn30xxpow_dump_ops_common(cn30xxpow_dump_ops_queue_entries,
	    nitems(cn30xxpow_dump_ops_queue_entries), "queue", queue);
}

void
cn30xxpow_dump_ops_common(const struct cn30xxpow_dump_ops_entry *entries,
    size_t nentries, const char *by_what, int arg)
{
	const struct cn30xxpow_dump_ops_entry *entry;
	uint64_t tmp;
	int i;

	printf("%s=%d\n", by_what, arg);
	for (i = 0; i < (int)nentries; i++) {
		entry = &entries[i];
		tmp = (*entry->func)(arg);
		printf("\t%-24s: %16llx\n", entry->name, tmp);
	}
}

#endif

/* -------------------------------------------------------------------------- */

/* ---- test */

#ifdef OCTEON_POW_TEST
/*
 * Standalone test entries; meant to be called from ddb.
 */

void		cn30xxpow_test(void);
void		cn30xxpow_test_dump_wqe(paddr_t);

void		cn30xxpow_test_1(void);

struct test_wqe {
	uint64_t word0;
	uint64_t word1;
	uint64_t word2;
	uint64_t word3;
} __packed;
struct test_wqe test_wqe;

void
cn30xxpow_test(void)
{
	cn30xxpow_test_1();
}

void
cn30xxpow_test_1(void)
{
	struct test_wqe *wqe = &test_wqe;
	int qos, grp, queue, tt;
	uint32_t tag;
	paddr_t ptr;

	qos = 7;			/* XXX */
	grp = queue = 15;		/* XXX */
	tt = POW_TAG_TYPE_ORDERED;	/* XXX */
	tag = UINT32_C(0x01234567);	/* XXX */

	/* => make sure that the queue is empty */

	cn30xxpow_dump_ops_qos(qos);
	cn30xxpow_dump_ops_grp(grp);
	printf("\n");

	/*
	 * Initialize WQE.
	 *
	 * word0:next is used by hardware.
	 *
	 * word1:qos, word1:grp, word1:tt, word1:tag must match with arguments
	 * of the following ADDWQ transaction.
	 */

	(void)memset(wqe, 0, sizeof(*wqe));
	wqe->word0 =
	    __BITS64_SET(POW_WQE_WORD0_NEXT, 0);
	wqe->word1 =
	    __BITS64_SET(POW_WQE_WORD1_QOS, qos) |
	    __BITS64_SET(POW_WQE_WORD1_GRP, grp) |
	    __BITS64_SET(POW_WQE_WORD1_TT, tt) |
	    __BITS64_SET(POW_WQE_WORD1_TAG, tag);

	printf("calling ADDWQ\n");
	cn30xxpow_ops_addwq(MIPS_KSEG0_TO_PHYS(wqe), qos, grp, tt, tag);

	cn30xxpow_dump_ops_qos(qos);
	cn30xxpow_dump_ops_grp(grp);
	printf("\n");

	/* => make sure that a WQE is added to the queue */

	printf("calling GET_WORK_LOAD\n");
	ptr = cn30xxpow_ops_get_work_load(0);

	cn30xxpow_dump_ops_qos(qos);
	cn30xxpow_dump_ops_grp(grp);
	printf("\n");

	cn30xxpow_test_dump_wqe(ptr);

	/* => make sure that the WQE is in-flight (and scheduled) */

	printf("calling SWTAG(NULL)\n");
	cn30xxpow_ops_swtag(POW_TAG_TYPE_NULL, tag);

	cn30xxpow_dump_ops_qos(qos);
	cn30xxpow_dump_ops_grp(grp);
	printf("\n");

	/* => make sure that the WQE is un-scheduled (completed) */
}

void
cn30xxpow_test_dump_wqe(paddr_t ptr)
{
	uint64_t word0, word1;

	printf("wqe\n");

	word0 = *(uint64_t *)PHYS_TO_CKSEG0(ptr);
	printf("\t%-24s: %16llx\n", "word0", word0);

	word1 = *(uint64_t *)PHYS_TO_CKSEG0(ptr + 8);
	printf("\t%-24s: %16llx\n", "word1", word1);
}
#endif
