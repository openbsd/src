/*	$OpenBSD: intr.c,v 1.1 2020/06/13 22:58:42 kettenis Exp $	*/

/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
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
