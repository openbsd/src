/*	$OpenBSD: kcov.c,v 1.9 2019/01/03 08:56:53 anton Exp $	*/

/*
 * Copyright (c) 2018 Anton Lindqvist <anton@openbsd.org>
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
#include <sys/proc.h>
#include <sys/kcov.h>
#include <sys/malloc.h>
#include <sys/stdint.h>
#include <sys/queue.h>

#include <uvm/uvm_extern.h>

/* #define KCOV_DEBUG */
#ifdef KCOV_DEBUG
#define DPRINTF(x...) do { if (kcov_debug) printf(x); } while (0)
#else
#define DPRINTF(x...)
#endif

struct kcov_dev {
	enum {
		KCOV_STATE_NONE,
		KCOV_STATE_READY,
		KCOV_STATE_TRACE,
		KCOV_STATE_DYING,
	}		 kd_state;
	int		 kd_mode;
	int		 kd_unit;	/* device minor */
	uintptr_t	*kd_buf;	/* traced coverage */
	size_t		 kd_nmemb;
	size_t		 kd_size;

	TAILQ_ENTRY(kcov_dev)	kd_entry;
};

void kcovattach(int);

int kd_init(struct kcov_dev *, unsigned long);
void kd_free(struct kcov_dev *);
struct kcov_dev *kd_lookup(int);

static inline int inintr(void);

TAILQ_HEAD(, kcov_dev) kd_list = TAILQ_HEAD_INITIALIZER(kd_list);

int kcov_cold = 1;

#ifdef KCOV_DEBUG
int kcov_debug = 1;
#endif

/*
 * Compiling the kernel with the `-fsanitize-coverage=trace-pc' option will
 * cause the following function to be called upon function entry and before
 * each block instructions that maps to a single line in the original source
 * code.
 *
 * If kcov is enabled for the current thread, the kernel program counter will
 * be stored in its corresponding coverage buffer.
 * The first element in the coverage buffer holds the index of next available
 * element.
 */
void
__sanitizer_cov_trace_pc(void)
{
	struct kcov_dev *kd;
	uint64_t idx;

	/*
	 * Do not trace before kcovopen() has been called at least once.
	 * At this point, all secondary CPUs have booted and accessing curcpu()
	 * is safe.
	 */
	if (kcov_cold)
		return;

	/* Do not trace in interrupts to prevent noisy coverage. */
	if (inintr())
		return;

	kd = curproc->p_kd;
	if (kd == NULL || kd->kd_mode != KCOV_MODE_TRACE_PC)
		return;

	idx = kd->kd_buf[0];
	if (idx < kd->kd_nmemb) {
		kd->kd_buf[idx + 1] = (uintptr_t)__builtin_return_address(0);
		kd->kd_buf[0] = idx + 1;
	}
}

void
kcovattach(int count)
{
}

int
kcovopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct kcov_dev *kd;

	if (kd_lookup(minor(dev)) != NULL)
		return (EBUSY);

	if (kcov_cold)
		kcov_cold = 0;

	DPRINTF("%s: unit=%d\n", __func__, minor(dev));

	kd = malloc(sizeof(*kd), M_SUBPROC, M_WAITOK | M_ZERO);
	kd->kd_unit = minor(dev);
	TAILQ_INSERT_TAIL(&kd_list, kd, kd_entry);
	return (0);
}

int
kcovclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct kcov_dev *kd;

	kd = kd_lookup(minor(dev));
	if (kd == NULL)
		return (EINVAL);

	DPRINTF("%s: unit=%d, state=%d, mode=%d\n",
	    __func__, kd->kd_unit, kd->kd_state, kd->kd_mode);

	if (kd->kd_state == KCOV_STATE_TRACE) {
		kd->kd_state = KCOV_STATE_DYING;
		kd->kd_mode = KCOV_MODE_NONE;
	} else {
		kd_free(kd);
	}

	return (0);
}

int
kcovioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct kcov_dev *kd;
	int mode;
	int error = 0;

	kd = kd_lookup(minor(dev));
	if (kd == NULL)
		return (ENXIO);

	switch (cmd) {
	case KIOSETBUFSIZE:
		error = kd_init(kd, *((unsigned long *)data));
		break;
	case KIOENABLE:
		/* Only one kcov descriptor can be enabled per thread. */
		if (p->p_kd != NULL || kd->kd_state != KCOV_STATE_READY) {
			error = EBUSY;
			break;
		}
		mode = *((int *)data);
		if (mode != KCOV_MODE_TRACE_PC) {
			error = EINVAL;
			break;
		}
		kd->kd_state = KCOV_STATE_TRACE;
		kd->kd_mode = mode;
		p->p_kd = kd;
		break;
	case KIODISABLE:
		/* Only the enabled thread may disable itself. */
		if (p->p_kd != kd || kd->kd_state != KCOV_STATE_TRACE) {
			error = EBUSY;
			break;
		}
		kd->kd_state = KCOV_STATE_READY;
		kd->kd_mode = KCOV_MODE_NONE;
		p->p_kd = NULL;
		break;
	default:
		error = ENOTTY;
	}

	DPRINTF("%s: unit=%d, state=%d, mode=%d, error=%d\n",
	    __func__, kd->kd_unit, kd->kd_state, kd->kd_mode, error);

	return (error);
}

paddr_t
kcovmmap(dev_t dev, off_t offset, int prot)
{
	struct kcov_dev *kd;
	paddr_t pa;
	vaddr_t va;

	kd = kd_lookup(minor(dev));
	if (kd == NULL)
		return (paddr_t)(-1);

	if (offset < 0 || offset >= kd->kd_nmemb * sizeof(uintptr_t))
		return (paddr_t)(-1);

	va = (vaddr_t)kd->kd_buf + offset;
	if (pmap_extract(pmap_kernel(), va, &pa) == FALSE)
		return (paddr_t)(-1);
	return (pa);
}

void
kcov_exit(struct proc *p)
{
	struct kcov_dev *kd;

	kd = p->p_kd;
	if (kd == NULL)
		return;

	DPRINTF("%s: unit=%d, state=%d, mode=%d\n",
	    __func__, kd->kd_unit, kd->kd_state, kd->kd_mode);

	if (kd->kd_state == KCOV_STATE_DYING) {
		kd_free(kd);
	} else {
		kd->kd_state = KCOV_STATE_READY;
		kd->kd_mode = KCOV_MODE_NONE;
	}
	p->p_kd = NULL;
}

struct kcov_dev *
kd_lookup(int unit)
{
	struct kcov_dev *kd;

	TAILQ_FOREACH(kd, &kd_list, kd_entry) {
		if (kd->kd_unit == unit)
			return (kd);
	}
	return (NULL);
}

int
kd_init(struct kcov_dev *kd, unsigned long nmemb)
{
	void *buf;
	size_t size;

	KASSERT(kd->kd_buf == NULL);

	if (kd->kd_state != KCOV_STATE_NONE)
		return (EBUSY);

	if (nmemb == 0 || nmemb > KCOV_BUF_MAX_NMEMB)
		return (EINVAL);

	size = roundup(nmemb * sizeof(uintptr_t), PAGE_SIZE);
	buf = malloc(size, M_SUBPROC, M_WAITOK | M_ZERO);
	/* malloc() can sleep, ensure the race was won. */
	if (kd->kd_state != KCOV_STATE_NONE) {
		free(buf, M_SUBPROC, size);
		return (EBUSY);
	}
	kd->kd_buf = buf;
	/* The first element is reserved to hold the number of used elements. */
	kd->kd_nmemb = nmemb - 1;
	kd->kd_size = size;
	kd->kd_state = KCOV_STATE_READY;
	return (0);
}

void
kd_free(struct kcov_dev *kd)
{
	DPRINTF("%s: unit=%d, state=%d, mode=%d\n",
	    __func__, kd->kd_unit, kd->kd_state, kd->kd_mode);

	TAILQ_REMOVE(&kd_list, kd, kd_entry);
	free(kd->kd_buf, M_SUBPROC, kd->kd_size);
	free(kd, M_SUBPROC, sizeof(*kd));
}

static inline int
inintr(void)
{
#if defined(__amd64__) || defined(__i386__)
	return (curcpu()->ci_idepth > 0);
#else
	return (0);
#endif
}
