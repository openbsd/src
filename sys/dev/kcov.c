/*	$OpenBSD: kcov.c,v 1.1 2018/08/19 11:42:33 anton Exp $	*/

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

/* kcov descriptor */
struct kd {
	enum {
		KCOV_MODE_DISABLED,
		KCOV_MODE_INIT,
		KCOV_MODE_TRACE_PC,
	}		 kd_mode;
	int		 kd_unit;	/* device minor */
	pid_t		 kd_pid;	/* process being traced */
	uintptr_t	*kd_buf;	/* traced coverage */
	size_t		 kd_nmemb;
	size_t		 kd_size;

	TAILQ_ENTRY(kd)	 kd_entry;
};

void kcovattach(int);

int kd_alloc(struct kd *, unsigned long);
struct kd *kd_lookup(int);

static inline struct kd *kd_lookup_pid(pid_t);
static inline int inintr(void);

TAILQ_HEAD(, kd) kd_list = TAILQ_HEAD_INITIALIZER(kd_list);

#ifdef KCOV_DEBUG
int kcov_debug = 1;
#endif

/*
 * Compiling the kernel with the `-fsanitize-coverage=trace-pc' option will
 * cause the following function to be called upon function entry and before
 * each block instructions that maps to a single line in the original source
 * code.
 *
 * If kcov is enabled for the current process, the executed address will be
 * stored in the corresponding coverage buffer.
 * The first element in the coverage buffer holds the index of next available
 * element.
 */
void
__sanitizer_cov_trace_pc(void)
{
	extern int cold;
	struct kd *kd;
	uint64_t idx;

	/* Do not trace during boot. */
	if (cold)
		return;

	/* Do not trace in interrupts to prevent noisy coverage. */
	if (inintr())
		return;

	kd = kd_lookup_pid(curproc->p_p->ps_pid);
	if (kd == NULL)
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
#ifdef KCOV
	struct kd *kd;

	if (kd_lookup(minor(dev)) != NULL)
		return (EBUSY);

	DPRINTF("%s: unit=%d\n", __func__, minor(dev));

	kd = malloc(sizeof(*kd), M_SUBPROC, M_WAITOK | M_ZERO);
	kd->kd_unit = minor(dev);
	TAILQ_INSERT_TAIL(&kd_list, kd, kd_entry);
	return (0);
#else
	return (ENXIO);
#endif
}

int
kcovclose(dev_t dev, int flag, int mode, struct proc *p)
{
	struct kd *kd;

	kd = kd_lookup(minor(dev));
	if (kd == NULL)
		return (EINVAL);

	DPRINTF("%s: unit=%d\n", __func__, minor(dev));

	TAILQ_REMOVE(&kd_list, kd, kd_entry);
	free(kd->kd_buf, M_SUBPROC, kd->kd_size);
	free(kd, M_SUBPROC, sizeof(struct kd));
	return (0);
}

int
kcovioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct kd *kd;
	int error = 0;

	kd = kd_lookup(minor(dev));
	if (kd == NULL)
		return (ENXIO);

	switch (cmd) {
	case KIOSETBUFSIZE:
		if (kd->kd_mode != KCOV_MODE_DISABLED) {
			error = EBUSY;
			break;
		}
		error = kd_alloc(kd, *((unsigned long *)data));
		if (error == 0)
			kd->kd_mode = KCOV_MODE_INIT;
		break;
	case KIOENABLE:
		if (kd->kd_mode != KCOV_MODE_INIT) {
			error = EBUSY;
			break;
		}
		kd->kd_mode = KCOV_MODE_TRACE_PC;
		kd->kd_pid = p->p_p->ps_pid;
		break;
	case KIODISABLE:
		/* Only the enabled process may disable itself. */
		if (kd->kd_pid != p->p_p->ps_pid ||
		    kd->kd_mode != KCOV_MODE_TRACE_PC) {
			error = EBUSY;
			break;
		}
		kd->kd_mode = KCOV_MODE_INIT;
		kd->kd_pid = 0;
		break;
	default:
		error = EINVAL;
		DPRINTF("%s: %lu: unknown command\n", __func__, cmd);
	}

	DPRINTF("%s: unit=%d, mode=%d, pid=%d, error=%d\n",
		    __func__, kd->kd_unit, kd->kd_mode, kd->kd_pid, error);

	return (error);
}

paddr_t
kcovmmap(dev_t dev, off_t offset, int prot)
{
	struct kd *kd;
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
	struct kd *kd;

	kd = kd_lookup_pid(p->p_p->ps_pid);
	if (kd == NULL)
		return;

	kd->kd_mode = KCOV_MODE_INIT;
	kd->kd_pid = 0;
}

struct kd *
kd_lookup(int unit)
{
	struct kd *kd;

	TAILQ_FOREACH(kd, &kd_list, kd_entry) {
		if (kd->kd_unit == unit)
			return (kd);
	}
	return (NULL);
}

int
kd_alloc(struct kd *kd, unsigned long nmemb)
{
	size_t size;

	KASSERT(kd->kd_buf == NULL);

	if (nmemb == 0 || nmemb > KCOV_BUF_MAX_NMEMB)
		return (EINVAL);

	size = roundup(nmemb * sizeof(uintptr_t), PAGE_SIZE);
	kd->kd_buf = malloc(size, M_SUBPROC, M_WAITOK | M_ZERO);
	/* The first element is reserved to hold the number of used elements. */
	kd->kd_nmemb = nmemb - 1;
	kd->kd_size = size;
	return (0);
}

static inline struct kd *
kd_lookup_pid(pid_t pid)
{
	struct kd *kd;

	TAILQ_FOREACH(kd, &kd_list, kd_entry) {
		if (kd->kd_pid == pid && kd->kd_mode == KCOV_MODE_TRACE_PC)
			return (kd);
	}
	return (NULL);
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
