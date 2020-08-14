/*	$OpenBSD: kcov.c,v 1.23 2020/08/14 11:51:07 anton Exp $	*/

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
#include <sys/mutex.h>
#include <sys/pool.h>
#include <sys/stdint.h>
#include <sys/queue.h>

#include <uvm/uvm_extern.h>

#define KCOV_BUF_MEMB_SIZE	sizeof(uintptr_t)

#define KCOV_CMP_CONST		0x1
#define KCOV_CMP_SIZE(x)	((x) << 1)

#define KCOV_STATE_NONE		0
#define KCOV_STATE_READY	1
#define KCOV_STATE_TRACE	2
#define KCOV_STATE_DYING	3

struct kcov_dev {
	int		 kd_state;
	int		 kd_mode;
	int		 kd_unit;	/* device minor */
	uintptr_t	*kd_buf;	/* traced coverage */
	size_t		 kd_nmemb;
	size_t		 kd_size;

	struct kcov_remote *kd_kr;

	TAILQ_ENTRY(kcov_dev)	kd_entry;
};

/*
 * Remote coverage structure.
 *
 * Locking:
 * 	I	immutable after creation
 *	M	kr_mtx
 */
struct kcov_remote {
	struct kcov_dev *kr_kd;	/* [M] */
	void *kr_id;		/* [I] */
	int kr_subsystem;	/* [I] */
	int kr_nsections;	/* [M] # threads in remote section */
	int kr_state;		/* [M] */

	TAILQ_ENTRY(kcov_remote) kr_entry;	/* [M] */
};

void kcovattach(int);

int kd_init(struct kcov_dev *, unsigned long);
void kd_free(struct kcov_dev *);
struct kcov_dev *kd_lookup(int);

struct kcov_remote *kcov_remote_register_locked(int, void *);
int kcov_remote_attach(struct kcov_dev *, struct kio_remote_attach *);
void kcov_remote_detach(struct kcov_dev *, struct kcov_remote *);
void kr_free(struct kcov_remote *);
struct kcov_remote *kr_lookup(int, void *);

static struct kcov_dev *kd_curproc(int);
static uint64_t kd_claim(struct kcov_dev *, int);
static inline int inintr(void);

TAILQ_HEAD(, kcov_dev) kd_list = TAILQ_HEAD_INITIALIZER(kd_list);
TAILQ_HEAD(, kcov_remote) kr_list = TAILQ_HEAD_INITIALIZER(kr_list);

int kcov_cold = 1;
int kr_cold = 1;
struct mutex kr_mtx = MUTEX_INITIALIZER(IPL_MPFLOOR);
struct pool kr_pool;

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

	kd = kd_curproc(KCOV_MODE_TRACE_PC);
	if (kd == NULL)
		return;

	if ((idx = kd_claim(kd, 1)))
		kd->kd_buf[idx] = (uintptr_t)__builtin_return_address(0);
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

	kd = kd_curproc(KCOV_MODE_TRACE_CMP);
	if (kd == NULL)
		return;

	if ((idx = kd_claim(kd, 4))) {
		kd->kd_buf[idx] = type;
		kd->kd_buf[idx + 1] = arg1;
		kd->kd_buf[idx + 2] = arg2;
		kd->kd_buf[idx + 3] = pc;
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
	pool_init(&kr_pool, sizeof(struct kcov_remote), 0, IPL_MPFLOOR, PR_WAITOK,
	    "kcovpl", NULL);
	kr_cold = 0;
}

int
kcovopen(dev_t dev, int flag, int mode, struct proc *p)
{
	struct kcov_dev *kd;

	if (kd_lookup(minor(dev)) != NULL)
		return (EBUSY);

	if (kcov_cold)
		kcov_cold = 0;

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

	if (kd->kd_state == KCOV_STATE_TRACE && kd->kd_kr == NULL) {
		/*
		 * Another thread is currently using the kcov descriptor,
		 * postpone freeing to kcov_exit().
		 */
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
		/* Remote coverage is mutually exclusive. */
		if (kd->kd_kr == NULL)
			p->p_kd = kd;
		break;
	case KIODISABLE:
		/* Only the enabled thread may disable itself. */
		if ((p->p_kd != kd && kd->kd_kr == NULL) ||
		    kd->kd_state != KCOV_STATE_TRACE) {
			error = EBUSY;
			break;
		}
		kd->kd_state = KCOV_STATE_READY;
		kd->kd_mode = KCOV_MODE_NONE;
		p->p_kd = NULL;
		break;
	case KIOREMOTEATTACH:
		error = kcov_remote_attach(kd,
		    (struct kio_remote_attach *)data);
		break;
	default:
		error = ENOTTY;
	}

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
	struct kcov_remote *kr;

	TAILQ_REMOVE(&kd_list, kd, kd_entry);

	kr = kd->kd_kr;
	if (kr != NULL)
		kcov_remote_detach(kd, kr);

	if (kd->kd_buf != NULL)
		km_free(kd->kd_buf, kd->kd_size, &kv_any, &kp_zero);
	free(kd, M_SUBPROC, sizeof(*kd));
}

static struct kcov_dev *
kd_curproc(int mode)
{
	struct kcov_dev *kd;

	/*
	 * Do not trace if the kernel has panicked. This could happen if curproc
	 * had kcov enabled while panicking.
	 */
	if (__predict_false(panicstr || db_active))
		return (NULL);

	/*
	 * Do not trace before kcovopen() has been called at least once.
	 * At this point, all secondary CPUs have booted and accessing curcpu()
	 * is safe.
	 */
	if (__predict_false(kcov_cold))
		return (NULL);

	/* Do not trace in interrupts to prevent noisy coverage. */
	if (inintr())
		return (NULL);

	kd = curproc->p_kd;
	if (__predict_true(kd == NULL) || kd->kd_mode != mode)
		return (NULL);
	return (kd);

}

/*
 * Claim stride number of elements in the coverage buffer. Returns the index of
 * the first claimed element. If the claim cannot be fulfilled, zero is
 * returned.
 */
static uint64_t
kd_claim(struct kcov_dev *kd, int stride)
{
	uint64_t idx, was;

	idx = kd->kd_buf[0];
	for (;;) {
		if (idx * stride + stride > kd->kd_nmemb)
			return (0);

		was = atomic_cas_ulong(&kd->kd_buf[0], idx, idx + 1);
		if (was == idx)
			return (idx * stride + 1);
		idx = was;
	}
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

void
kcov_remote_enter(int subsystem, void *id)
{
	struct kcov_dev *kd;
	struct kcov_remote *kr;

	/*
	 * We could end up here while executing a timeout triggered from a
	 * softclock interrupt context. At this point, the current thread might
	 * have kcov enabled and this is therefore not a remote section in the
	 * sense that we're not executing in the context of another thread.
	 */
	if (inintr())
		return;

	mtx_enter(&kr_mtx);
	kr = kr_lookup(subsystem, id);
	if (kr == NULL || kr->kr_state != KCOV_STATE_READY)
		goto out;
	kd = kr->kr_kd;
	if (kd != NULL && kd->kd_state == KCOV_STATE_TRACE) {
		kr->kr_nsections++;
		KASSERT(curproc->p_kd == NULL);
		curproc->p_kd = kd;
	}
out:
	mtx_leave(&kr_mtx);
}

void
kcov_remote_leave(int subsystem, void *id)
{
	struct kcov_remote *kr;

	/* See kcov_remote_enter(). */
	if (inintr())
		return;

	mtx_enter(&kr_mtx);
	kr = kr_lookup(subsystem, id);
	/*
	 * The remote could have been absent when the same thread called
	 * kcov_remote_enter() earlier, allowing the remote to be registered
	 * while thread was inside the remote section. Therefore ensure we don't
	 * give back a reference we didn't acquire.
	 */
	if (kr == NULL || curproc->p_kd == NULL || curproc->p_kd != kr->kr_kd)
		goto out;
	curproc->p_kd = NULL;
	if (--kr->kr_nsections == 0 && kr->kr_state == KCOV_STATE_DYING)
		wakeup(kr);
out:
	mtx_leave(&kr_mtx);
}

void
kcov_remote_register(int subsystem, void *id)
{
	mtx_enter(&kr_mtx);
	kcov_remote_register_locked(subsystem, id);
	mtx_leave(&kr_mtx);
}

void
kcov_remote_unregister(int subsystem, void *id)
{
	struct kcov_remote *kr;

	mtx_enter(&kr_mtx);
	kr = kr_lookup(subsystem, id);
	if (kr != NULL)
		kr_free(kr);
	mtx_leave(&kr_mtx);
}

struct kcov_remote *
kcov_remote_register_locked(int subsystem, void *id)
{
	struct kcov_remote *kr, *tmp;

	/* Do not allow registrations before the pool is initialized. */
	KASSERT(kr_cold == 0);

	/*
	 * Temporarily release the mutex since the allocation could end up
	 * sleeping.
	 */
	mtx_leave(&kr_mtx);
	kr = pool_get(&kr_pool, PR_WAITOK | PR_ZERO);
	kr->kr_subsystem = subsystem;
	kr->kr_id = id;
	kr->kr_state = KCOV_STATE_NONE;
	mtx_enter(&kr_mtx);

	for (;;) {
		tmp = kr_lookup(subsystem, id);
		if (tmp == NULL)
			break;
		if (tmp->kr_state != KCOV_STATE_DYING) {
			pool_put(&kr_pool, tmp);
			return (NULL);
		}
		/*
		 * The remote could already be deregistered while another
		 * thread is currently inside a kcov remote section.
		 */
		KASSERT(tmp->kr_state == KCOV_STATE_DYING);
		msleep_nsec(tmp, &kr_mtx, PWAIT, "kcov", INFSLP);
	}
	TAILQ_INSERT_TAIL(&kr_list, kr, kr_entry);
	return (kr);
}

int
kcov_remote_attach(struct kcov_dev *kd, struct kio_remote_attach *arg)
{
	struct kcov_remote *kr = NULL;
	int error = 0;

	if (kd->kd_state != KCOV_STATE_READY)
		return (EBUSY);

	mtx_enter(&kr_mtx);
	if (arg->subsystem == KCOV_REMOTE_COMMON)
		kr = kcov_remote_register_locked(KCOV_REMOTE_COMMON,
		    curproc->p_p);
	if (kr == NULL) {
		error = EINVAL;
		goto out;
	}
	if (kr->kr_state != KCOV_STATE_NONE) {
		error = EBUSY;
		goto out;
	}

	kr->kr_state = KCOV_STATE_READY;
	kr->kr_kd = kd;
	kd->kd_kr = kr;

out:
	mtx_leave(&kr_mtx);
	return (error);
}

void
kcov_remote_detach(struct kcov_dev *kd, struct kcov_remote *kr)
{
	mtx_enter(&kr_mtx);
	KASSERT(kd == kr->kr_kd);
	if (kr->kr_subsystem == KCOV_REMOTE_COMMON) {
		kr_free(kr);
	} else {
		kr->kr_state = KCOV_STATE_NONE;
		kd->kd_kr = NULL;
		kr->kr_kd = NULL;
	}
	mtx_leave(&kr_mtx);
}

void
kr_free(struct kcov_remote *kr)
{
	MUTEX_ASSERT_LOCKED(&kr_mtx);

	kr->kr_state = KCOV_STATE_DYING;
	if (kr->kr_kd != NULL)
		kr->kr_kd->kd_kr = NULL;
	kr->kr_kd = NULL;
	if (kr->kr_nsections > 0)
		msleep_nsec(kr, &kr_mtx, PWAIT, "kcov", INFSLP);
	TAILQ_REMOVE(&kr_list, kr, kr_entry);
	pool_put(&kr_pool, kr);
}

struct kcov_remote *
kr_lookup(int subsystem, void *id)
{
	struct kcov_remote *kr;

	MUTEX_ASSERT_LOCKED(&kr_mtx);

	TAILQ_FOREACH(kr, &kr_list, kr_entry) {
		if (kr->kr_subsystem == subsystem && kr->kr_id == id)
			return (kr);
	}
	return (NULL);
}
