/*	$OpenBSD: kcov.c,v 1.15 2019/05/19 08:55:27 anton Exp $	*/

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

#define KCOV_BUF_MEMB_SIZE	sizeof(uintptr_t)

#define KCOV_CMP_CONST		0x1
#define KCOV_CMP_SIZE(x)	((x) << 1)

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
	if (idx + 1 <= kd->kd_nmemb) {
		kd->kd_buf[idx + 1] = (uintptr_t)__builtin_return_address(0);
		kd->kd_buf[0] = idx + 1;
	}
}

/*
 * Compiling the kernel with the `-fsanitize-coverage=trace-cmp' option will
 * cause the following function to be called upon integer comparisons and switch
 * statements.
 *
 * If kcov is enabled for the current thread, the comparison will be stored in
 * its corresponding coverage buffer.
 */
void
trace_cmp(uint64_t type, uint64_t arg1, uint64_t arg2, uintptr_t pc)
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
	if (kd == NULL || kd->kd_mode != KCOV_MODE_TRACE_CMP)
		return;

	idx = kd->kd_buf[0];
	if (idx * 4 + 4 <= kd->kd_nmemb) {
		kd->kd_buf[idx * 4 + 1] = type;
		kd->kd_buf[idx * 4 + 2] = arg1;
		kd->kd_buf[idx * 4 + 3] = arg2;
		kd->kd_buf[idx * 4 + 4] = pc;
		kd->kd_buf[0] = idx + 1;
	}
}

void
__sanitizer_cov_trace_cmp1(uint8_t arg1, uint8_t arg2)
{
	trace_cmp(KCOV_CMP_SIZE(0), arg1, arg2,
	    (uintptr_t)__builtin_return_address(0));
}

void
__sanitizer_cov_trace_cmp2(uint16_t arg1, uint16_t arg2)
{
	trace_cmp(KCOV_CMP_SIZE(1), arg1, arg2,
	    (uintptr_t)__builtin_return_address(0));
}

void
__sanitizer_cov_trace_cmp4(uint32_t arg1, uint32_t arg2)
{
	trace_cmp(KCOV_CMP_SIZE(2), arg1, arg2,
	    (uintptr_t)__builtin_return_address(0));
}

void
__sanitizer_cov_trace_cmp8(uint64_t arg1, uint64_t arg2)
{
	trace_cmp(KCOV_CMP_SIZE(3), arg1, arg2,
	    (uintptr_t)__builtin_return_address(0));
}

void
__sanitizer_cov_trace_const_cmp1(uint8_t arg1, uint8_t arg2)
{
	trace_cmp(KCOV_CMP_SIZE(0) | KCOV_CMP_CONST, arg1, arg2,
	    (uintptr_t)__builtin_return_address(0));
}

void
__sanitizer_cov_trace_const_cmp2(uint16_t arg1, uint16_t arg2)
{
	trace_cmp(KCOV_CMP_SIZE(1) | KCOV_CMP_CONST, arg1, arg2,
	    (uintptr_t)__builtin_return_address(0));
}

void
__sanitizer_cov_trace_const_cmp4(uint32_t arg1, uint32_t arg2)
{
	trace_cmp(KCOV_CMP_SIZE(2) | KCOV_CMP_CONST, arg1, arg2,
	    (uintptr_t)__builtin_return_address(0));
}

void
__sanitizer_cov_trace_const_cmp8(uint64_t arg1, uint64_t arg2)
{
	trace_cmp(KCOV_CMP_SIZE(3) | KCOV_CMP_CONST, arg1, arg2,
	    (uintptr_t)__builtin_return_address(0));
}

void
__sanitizer_cov_trace_switch(uint64_t val, uint64_t *cases)
{
	uint64_t i, nbits, ncases, type;
	uintptr_t pc;

	pc = (uintptr_t)__builtin_return_address(0);
	ncases = cases[0];
	nbits = cases[1];

	switch (nbits) {
	case 8:
		type = KCOV_CMP_SIZE(0);
		break;
	case 16:
		type = KCOV_CMP_SIZE(1);
		break;
	case 32:
		type = KCOV_CMP_SIZE(2);
		break;
	case 64:
		type = KCOV_CMP_SIZE(3);
		break;
	default:
		return;
	}
	type |= KCOV_CMP_CONST;

	for (i = 0; i < ncases; i++)
		trace_cmp(type, cases[i + 2], val, pc);
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
		if (mode != KCOV_MODE_TRACE_PC && mode != KCOV_MODE_TRACE_CMP) {
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

	if (offset < 0 || offset >= kd->kd_nmemb * KCOV_BUF_MEMB_SIZE)
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

	size = roundup(nmemb * KCOV_BUF_MEMB_SIZE, PAGE_SIZE);
	buf = km_alloc(size, &kv_any, &kp_zero, &kd_waitok);
	if (buf == NULL)
		return (ENOMEM);
	/* km_malloc() can sleep, ensure the race was won. */
	if (kd->kd_state != KCOV_STATE_NONE) {
		km_free(buf, size, &kv_any, &kp_zero);
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
	if (kd->kd_buf != NULL)
		km_free(kd->kd_buf, kd->kd_size, &kv_any, &kp_zero);
	free(kd, M_SUBPROC, sizeof(*kd));
}

static inline int
inintr(void)
{
#if defined(__amd64__) || defined(__arm__) || defined(__arm64__) || \
    defined(__i386__)
	return (curcpu()->ci_idepth > 0);
#else
	return (0);
#endif
}
