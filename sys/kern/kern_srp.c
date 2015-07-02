/*	$OpenBSD: kern_srp.c,v 1.1 2015/07/02 01:34:00 dlg Exp $ */

/*
 * Copyright (c) 2014 Jonathan Matthew <jmatthew@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/atomic.h>

#include <sys/srp.h>

void	srp_v_gc_start(struct srp_gc *, struct srp *, void *);

void
srp_gc_init(struct srp_gc *srp_gc, void (*dtor)(void *, void *), void *cookie)
{
	srp_gc->srp_gc_dtor = dtor;
	srp_gc->srp_gc_cookie = cookie;
	srp_gc->srp_gc_refcount = 1;
}

void
srp_init(struct srp *srp)
{
	srp->ref = NULL;
}

void
srp_update_locked(struct srp_gc *srp_gc, struct srp *srp, void *nv)
{
	void *ov;

	if (nv != NULL)
		atomic_inc_int(&srp_gc->srp_gc_refcount);

	/*
	 * this doesn't have to be as careful as the caller has already
	 * prevented concurrent updates, eg. by holding the kernel lock.
	 * can't be mixed with non-locked updates though.
	 */

	ov = srp->ref;
	srp->ref = nv;
	if (ov != NULL)
		srp_v_gc_start(srp_gc, srp, ov);
}

void *
srp_get_locked(struct srp *srp)
{
	return (srp->ref);
}

#ifdef MULTIPROCESSOR
#include <machine/cpu.h>
#include <sys/pool.h>

struct srp_gc_ctx {
	struct srp_gc		*srp_gc;
	struct timeout		tick;
	struct srp_hazard	hzrd;
};

int	srp_v_referenced(struct srp *, void *);
void	srp_v_gc(void *);

struct pool srp_gc_ctx_pool;

void
srp_startup(void)
{
	pool_init(&srp_gc_ctx_pool, sizeof(struct srp_gc_ctx), 0, 0,
	    PR_WAITOK, "srpgc", NULL);

	/* items are allocated in a process, but freed from a timeout */
	pool_setipl(&srp_gc_ctx_pool, IPL_SOFTCLOCK);
}

int
srp_v_referenced(struct srp *srp, void *v)
{
	struct cpu_info *ci;
	CPU_INFO_ITERATOR cii;
	u_int i;
	struct srp_hazard *hzrd;

	CPU_INFO_FOREACH(cii, ci) {
		for (i = 0; i < nitems(ci->ci_srp_hazards); i++) {
			hzrd = &ci->ci_srp_hazards[i];

			if (hzrd->sh_p != srp)
				continue;
			membar_consumer();
			if (hzrd->sh_v != v)
				continue;

			return (1);
		}
	}

	return (0);
}

void
srp_v_dtor(struct srp_gc *srp_gc, void *v)
{
	(*srp_gc->srp_gc_dtor)(srp_gc->srp_gc_cookie, v);

	if (atomic_dec_int_nv(&srp_gc->srp_gc_refcount) == 0)
		wakeup_one(&srp_gc->srp_gc_refcount);
}

void
srp_v_gc_start(struct srp_gc *srp_gc, struct srp *srp, void *v)
{
	struct srp_gc_ctx *ctx;

	if (!srp_v_referenced(srp, v)) {
		/* we win */
		srp_v_dtor(srp_gc, v);
		return;
	}

	/* in use, try later */

	ctx = pool_get(&srp_gc_ctx_pool, PR_WAITOK);
	ctx->srp_gc = srp_gc;
	ctx->hzrd.sh_p = srp;
	ctx->hzrd.sh_v = v;

	timeout_set(&ctx->tick, srp_v_gc, ctx);
	timeout_add(&ctx->tick, 1);
}

void
srp_v_gc(void *x)
{
	struct srp_gc_ctx *ctx = x;

	if (srp_v_referenced(ctx->hzrd.sh_p, ctx->hzrd.sh_v)) {
		/* oh well, try again later */
		timeout_add(&ctx->tick, 1);
		return;
	}

	srp_v_dtor(ctx->srp_gc, ctx->hzrd.sh_v);
	pool_put(&srp_gc_ctx_pool, ctx);
}

void
srp_update(struct srp_gc *srp_gc, struct srp *srp, void *v)
{
	if (v != NULL)
		atomic_inc_int(&srp_gc->srp_gc_refcount);

	v = atomic_swap_ptr(&srp->ref, v);
	if (v != NULL)
		srp_v_gc_start(srp_gc, srp, v);
}

void
srp_finalize(struct srp_gc *srp_gc)
{
	struct sleep_state sls;
	u_int r;

	r = atomic_dec_int_nv(&srp_gc->srp_gc_refcount);
	while (r > 0) {
		sleep_setup(&sls, &srp_gc->srp_gc_refcount, PWAIT, "srpfini");
		r = srp_gc->srp_gc_refcount;
		sleep_finish(&sls, r);
	}
}

void *
srp_enter(struct srp *srp)
{
	struct cpu_info *ci = curcpu();
	struct srp_hazard *hzrd;
	void *v;
	u_int i;

	for (i = 0; i < nitems(ci->ci_srp_hazards); i++) {
		hzrd = &ci->ci_srp_hazards[i];
		if (hzrd->sh_p == NULL)
			break;
	}
	if (__predict_false(i == nitems(ci->ci_srp_hazards)))
		panic("%s: not enough srp hazard records", __func__);

	hzrd->sh_p = srp;
	membar_producer();

	/*
	 * ensure we update this cpu's hazard pointer to a value that's still
	 * current after the store finishes, otherwise the gc task may already
	 * be destroying it
	 */
	do {
		v = srp->ref;
		hzrd->sh_v = v;
		membar_consumer();
	} while (__predict_false(v != srp->ref));

	return (v);
}

void
srp_leave(struct srp *srp, void *v)
{
	struct cpu_info *ci = curcpu();
	struct srp_hazard *hzrd;
	u_int i;

	for (i = 0; i < nitems(ci->ci_srp_hazards); i++) {
		hzrd = &ci->ci_srp_hazards[i];
		if (hzrd->sh_p == srp) {
			hzrd->sh_p = NULL;
			hzrd->sh_v = NULL;
			return;
		}
	}

	panic("%s: unexpected ref %p via %p", __func__, v, srp);
}

#else /* MULTIPROCESSOR */

void
srp_startup(void)
{

}

void
srp_finalize(struct srp_gc *srp_gc)
{
	KASSERT(srp_gc->srp_gc_refcount == 1);

	srp_gc->srp_gc_refcount--;
}

void
srp_v_gc_start(struct srp_gc *srp_gc, struct srp *srp, void *v)
{
	(*srp_gc->srp_gc_dtor)(srp_gc->srp_gc_cookie, v);
	srp_gc->srp_gc_refcount--;
}

#endif /* MULTIPROCESSOR */
