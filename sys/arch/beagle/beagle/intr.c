/* $OpenBSD: intr.c,v 1.1 2011/10/24 22:49:07 drahn Exp $ */
/*
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

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/param.h>

#include <arm/cpufunc.h>
#include <machine/cpu.h>
#include <machine/intr.h>

int arm_dflt_splraise(int);
int arm_dflt_spllower(int);
void arm_dflt_splx(int);
void arm_dflt_setipl(int);
void *arm_dflt_intr_establish(int irqno, int level, int (*func)(void *),
    void *cookie, char *name);
void arm_dflt_intr_disestablish(void *cookie);
const char *arm_dflt_intr_string(void *cookie);

void arm_dflt_intr(void *);
void arm_intr(void *);


#define SI_TO_IRQBIT(x) (1 << (x))
uint32_t arm_smask[NIPL];

struct arm_intr_func arm_intr_func = {
	arm_dflt_splraise,
	arm_dflt_spllower,
	arm_dflt_splx,
	arm_dflt_setipl,
	arm_dflt_intr_establish,
	arm_dflt_intr_disestablish,
	arm_dflt_intr_string
};

void (*arm_intr_dispatch)(void *) = arm_dflt_intr;

void
arm_intr(void *frame)
{
	/* XXX - change this to have irq_dispatch use function pointer */
	(*arm_intr_dispatch)(frame);
}
void
arm_dflt_intr(void *frame)
{
	panic("arm_dflt_intr() called");
}


void *arm_intr_establish(int irqno, int level, int (*func)(void *),
    void *cookie, char *name)
{
	return arm_intr_func.intr_establish(irqno, level, func, cookie, name);
}
void arm_intr_disestablish(void *cookie)
{
	arm_intr_func.intr_disestablish(cookie);
}
const char *arm_intr_string(void *cookie)
{
	return arm_intr_func.intr_string(cookie);
}

int
arm_dflt_splraise(int newcpl)
{
	struct cpu_info *ci = curcpu();
	int oldcpl;

	oldcpl = ci->ci_cpl;

	if (newcpl < oldcpl)
		newcpl = oldcpl;

	ci->ci_cpl = newcpl;

	return oldcpl;
}

int
arm_dflt_spllower(int newcpl)
{
	struct cpu_info *ci = curcpu();
	int oldcpl;

	oldcpl = ci->ci_cpl;

	splx(newcpl);

	return oldcpl;
}

void
arm_dflt_splx(int newcpl)
{
	struct cpu_info *ci = curcpu();

	if (ci->ci_ipending & arm_smask[newcpl])
		arm_do_pending_intr(newcpl);
	ci->ci_cpl = newcpl;
}

void
arm_dflt_setipl(int newcpl)
{
	struct cpu_info *ci = curcpu();

	ci->ci_cpl = newcpl;
}

void *arm_dflt_intr_establish(int irqno, int level, int (*func)(void *),
    void *cookie, char *name)
{
	panic("arm_dflt_intr_establish called");
}

void arm_dflt_intr_disestablish(void *cookie)
{
	panic("arm_dflt_intr_disestablish called");
}

const char *
arm_dflt_intr_string(void *cookie)
{
	panic("arm_dflt_intr_string called");
}

void
arm_setsoftintr(int si)
{
        struct cpu_info *ci = curcpu();
        int oldirqstate;

        /* XXX atomic? */
        oldirqstate = disable_interrupts(I32_bit);
        ci->ci_ipending |= SI_TO_IRQBIT(si);

        restore_interrupts(oldirqstate);

        /* Process unmasked pending soft interrupts. */
        if (ci->ci_ipending & arm_smask[ci->ci_cpl])
                arm_do_pending_intr(ci->ci_cpl);
}

void
arm_do_pending_intr(int pcpl)
{
	struct cpu_info *ci = curcpu();
	static int processing = 0;
	int oldirqstate;

	oldirqstate = disable_interrupts(I32_bit);

	if (processing == 1) {
		/* Don't use splx... we are here already! */
		arm_intr_func.setipl(pcpl);
		restore_interrupts(oldirqstate);
		return;
	}

#define DO_SOFTINT(si, ipl) \
	if ((ci->ci_ipending & arm_smask[pcpl]) &	\
	    SI_TO_IRQBIT(si)) {						\
		ci->ci_ipending &= ~SI_TO_IRQBIT(si);			\
		arm_intr_func.setipl(ipl);				\
		restore_interrupts(oldirqstate);			\
		softintr_dispatch(si);					\
		oldirqstate = disable_interrupts(I32_bit);		\
	}

	do {
		DO_SOFTINT(SI_SOFTTTY, IPL_SOFTTTY);
		DO_SOFTINT(SI_SOFTNET, IPL_SOFTNET);
		DO_SOFTINT(SI_SOFTCLOCK, IPL_SOFTCLOCK);
		DO_SOFTINT(SI_SOFT, IPL_SOFT);
	} while (ci->ci_ipending & arm_smask[pcpl]);
		
	/* Don't use splx... we are here already! */
	arm_intr_func.setipl(pcpl);
	processing = 0;
	restore_interrupts(oldirqstate);
}

void arm_set_intr_handler(int (*raise)(int), int (*lower)(int),
    void (*x)(int), void (*setipl)(int),
	void *(*intr_establish)(int irqno, int level, int (*func)(void *),
	    void *cookie, char *name),
	void (*intr_disestablish)(void *cookie),
	const char *(intr_string)(void *cookie),
	void (*intr_handle)(void *))
{
	arm_intr_func.raise		= raise;
	arm_intr_func.lower		= lower;
	arm_intr_func.x			= x;
	arm_intr_func.setipl		= setipl;
	arm_intr_func.intr_establish	= intr_establish;
	arm_intr_func.intr_disestablish	= intr_disestablish;
	arm_intr_func.intr_string	= intr_string;
	arm_intr_dispatch		= intr_handle;
}

void
arm_init_smask(void)
{
	static int inited = 0;
        int i;

	if (inited)
		return;
	inited = 1;

        for (i = IPL_NONE; i <= IPL_HIGH; i++)  {
                arm_smask[i] = 0;
                if (i < IPL_SOFT)
                        arm_smask[i] |= SI_TO_IRQBIT(SI_SOFT);
                if (i < IPL_SOFTCLOCK)
                        arm_smask[i] |= SI_TO_IRQBIT(SI_SOFTCLOCK);
                if (i < IPL_SOFTNET)
                        arm_smask[i] |= SI_TO_IRQBIT(SI_SOFTNET);
                if (i < IPL_SOFTTTY)
                        arm_smask[i] |= SI_TO_IRQBIT(SI_SOFTTTY);
        }
}

/* provide functions for asm */
#undef splraise
#undef spllower
#undef splx

int
splraise(int ipl)
{
	return arm_intr_func.raise(ipl);
}

int _spllower(int ipl); /* XXX - called from asm? */
int
_spllower(int ipl)
{
	return arm_intr_func.lower(ipl);
}
int
spllower(int ipl)
{
	return arm_intr_func.lower(ipl);
}

void
splx(int ipl)
{
	arm_intr_func.x(ipl);
}


#ifdef DIAGNOSTIC
void
arm_splassert_check(int wantipl, const char *func)
{
	struct cpu_info *ci = curcpu();
        int oldipl = ci->ci_cpl;

        if (oldipl < wantipl) {
                splassert_fail(wantipl, oldipl, func);
                /*
                 * If the splassert_ctl is set to not panic, raise the ipl
                 * in a feeble attempt to reduce damage.
                 */
                arm_intr_func.setipl(wantipl);
        }
}
#endif
