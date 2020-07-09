/*	$OpenBSD: intr.c,v 1.2 2020/06/14 16:12:09 kettenis Exp $	*/

/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
 * Copyright (c) 2011 Dale Rahn <drahn@openbsd.org>
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
#include <sys/systm.h>

#include <machine/intr.h>

/* Dummy implementations. */
void	dummy_hvi(struct trapframe *);
void	*dummy_intr_establish(uint32_t, int, int,
	    int (*)(void *), void *, const char *);
void	dummy_setipl(int);

/*
 * The function pointers are overridden when the driver for the real
 * interrupt controller attaches.
 */
void	(*_hvi)(struct trapframe *) = dummy_hvi;
void	*(*_intr_establish)(uint32_t, int, int,
	    int (*)(void *), void *, const char *) = dummy_intr_establish;
void	(*_setipl)(int) = dummy_setipl;

void
hvi_intr(struct trapframe *frame)
{
	(*_hvi)(frame);
}

void *
intr_establish(uint32_t girq, int type, int level,
    int (*func)(void *), void *arg, const char *name)
{
	return (*_intr_establish)(girq, type, level, func, arg, name);
}

#define SI_TO_IRQBIT(x) (1 << (x))
uint32_t intr_smask[NIPL];

void
intr_init(void)
{
	int i;

	for (i = IPL_NONE; i <= IPL_HIGH; i++)  {
		intr_smask[i] = 0;
		if (i < IPL_SOFT)
			intr_smask[i] |= SI_TO_IRQBIT(SIR_SOFT);
		if (i < IPL_SOFTCLOCK)
			intr_smask[i] |= SI_TO_IRQBIT(SIR_CLOCK);
		if (i < IPL_SOFTNET)
			intr_smask[i] |= SI_TO_IRQBIT(SIR_NET);
		if (i < IPL_SOFTTTY)
			intr_smask[i] |= SI_TO_IRQBIT(SIR_TTY);
	}
}

void
intr_do_pending(int new)
{
	struct cpu_info *ci = curcpu();
	u_long msr;

	msr = intr_disable();

#define DO_SOFTINT(si, ipl) \
	if ((ci->ci_ipending & intr_smask[new]) & SI_TO_IRQBIT(si)) {	\
		ci->ci_ipending &= ~SI_TO_IRQBIT(si);			\
		_setipl(ipl);						\
		intr_restore(msr);					\
		softintr_dispatch(si);					\
		msr = intr_disable();					\
	}

	do {
		DO_SOFTINT(SIR_TTY, IPL_SOFTTTY);
		DO_SOFTINT(SIR_NET, IPL_SOFTNET);
		DO_SOFTINT(SIR_CLOCK, IPL_SOFTCLOCK);
		DO_SOFTINT(SIR_SOFT, IPL_SOFT);
	} while (ci->ci_ipending & intr_smask[new]);

	intr_restore(msr);
}

int
splraise(int new)
{
	struct cpu_info *ci = curcpu();
	int old = ci->ci_cpl;

	if (new > old)
		(*_setipl)(new);
	return old;
}

int
spllower(int new)
{
	struct cpu_info *ci = curcpu();
	int old = ci->ci_cpl;

	if (new < old)
		(*_setipl)(new);
	return old;
}

void
splx(int new)
{
	struct cpu_info *ci = curcpu();

	if (ci->ci_ipending & intr_smask[new])
		intr_do_pending(new);

	if (ci->ci_cpl != new)
		(*_setipl)(new);
}

#ifdef DIAGNOSTIC
void
splassert_check(int wantipl, const char *func)
{
	int oldipl = curcpu()->ci_cpl;

	if (oldipl < wantipl) {
		splassert_fail(wantipl, oldipl, func);
		/*
		 * If the splassert_ctl is set to not panic, raise the ipl
		 * in a feeble attempt to reduce damage.
		 */
		(*_setipl)(wantipl);
	}

	if (wantipl == IPL_NONE && curcpu()->ci_idepth != 0) {
		splassert_fail(-1, curcpu()->ci_idepth, func);
	}
}
#endif

void
dummy_hvi(struct trapframe *frame)
{
	panic("Unhandled Hypervisor Virtualization interrupt");
}

void *
dummy_intr_establish(uint32_t girq, int type, int level,
	    int (*func)(void *), void *arg, const char *name)
{
	return NULL;
}

void
dummy_setipl(int new)
{
	struct cpu_info *ci = curcpu();
	ci->ci_cpl = new;
}
