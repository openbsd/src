/*
 * Copyright (c) Joel Sing <jsing@openbsd.org>
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

#ifndef _MACHINE_INTR_H_
#define _MACHINE_INTR_H_

#define CPU_NINTS       64

#define NIPL            12

#define IPL_NONE        0
#define IPL_SOFTCLOCK   1
#define IPL_SOFTNET     2
#define IPL_BIO         3
#define IPL_NET         4
#define IPL_SOFTTTY     5
#define IPL_TTY         6
#define IPL_VM          7
#define IPL_AUDIO       8
#define IPL_CLOCK       9
#define IPL_STATCLOCK   10
#define IPL_SCHED       10
#define IPL_HIGH        10
#define IPL_IPI         11

#define	IST_NONE        0
#define	IST_PULSE       1
#define	IST_EDGE        2
#define	IST_LEVEL       3

#define SOFTINT_MASK ((1 << (IPL_SOFTCLOCK - 1)) | \
    (1 << (IPL_SOFTNET - 1)) | (1 << (IPL_SOFTTTY - 1)))

#if !defined(_LOCORE) && defined(_KERNEL)
#define  softintr(mask)  atomic_setbits_long(&curcpu()->ci_ipending, mask)

void	cpu_intr_init(void);
void	cpu_intr(void *);

void	*softintr_establish(int, void (*)(void *), void *);
void	softintr_disestablish(void *);
void	softintr_schedule(void *);

int	splraise(int cpl);
int	spllower(int cpl);

#define	splsoftclock()	splraise(IPL_SOFTCLOCK)
#define	splsoftnet()	splraise(IPL_SOFTNET)
#define	splbio()	splraise(IPL_BIO)
#define	splnet()	splraise(IPL_NET)
#define	splsofttty()	splraise(IPL_SOFTTTY)
#define	spltty()	splraise(IPL_TTY)
#define	splvm()		splraise(IPL_VM)
#define	splaudio()	splraise(IPL_AUDIO)
#define	splclock()	splraise(IPL_CLOCK)
#define	splsched()	splraise(IPL_SCHED)
#define	splstatclock()	splraise(IPL_STATCLOCK)
#define	splhigh()	splraise(IPL_HIGH)
#define	spl0()		spllower(IPL_NONE)
#define	splx(c)		spllower(c)

#define	setsoftast()		(astpending = 1)

#ifdef DIAGNOSTIC   
extern int splassert_ctl;
void splassert_fail(int, int, const char *);
void splassert_check(int, const char *);
#define splassert(__wantipl) do {			\
	if (splassert_ctl > 0) {			\
		splassert_check(__wantipl, __func__);	\
	}						\
} while (0)
#define splsoftassert(__wantipl) splassert(__wantipl)
#else
#define splassert(__wantipl)		do { /* nada */ } while (0)
#define splsoftassert(__wantipl)	do { /* nada */ } while (0)
#endif /* DIAGNOSTIC */

#endif

#endif
