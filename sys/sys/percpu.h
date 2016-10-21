/*	$OpenBSD: percpu.h,v 1.1 2016/10/21 06:27:50 dlg Exp $ */

/*
 * Copyright (c) 2016 David Gwynne <dlg@openbsd.org>
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

#ifndef _SYS_PERCPU_H_
#define _SYS_PERCPU_H_

#ifndef CACHELINESIZE
#define CACHELINESIZE 64
#endif

#ifndef __upunused /* this should go in param.h */
#ifdef MULTIPROCESSOR
#define __upunused
#else
#define __upunused __attribute__((__unused__))
#endif
#endif

struct cpumem {
	void		*mem;
};

struct cpumem_iter {
	unsigned int	cpu;
} __upunused;

struct counters_ref {
	uint64_t	 g;
	uint64_t	*c;
};

#ifdef _KERNEL

#include <sys/atomic.h>

struct pool;

struct cpumem	*cpumem_get(struct pool *);
void		 cpumem_put(struct pool *, struct cpumem *);

struct cpumem	*cpumem_malloc(size_t, int);
struct cpumem	*cpumem_realloc(struct cpumem *, size_t, int);
void		 cpumem_free(struct cpumem *, int, size_t);

#ifdef MULTIPROCESSOR
static inline void *
cpumem_enter(struct cpumem *cm)
{
	return (cm[cpu_number()].mem);
}

static inline void
cpumem_leave(struct cpumem *cm, void *mem)
{
	/* KDASSERT? */
}

void		*cpumem_first(struct cpumem_iter *, struct cpumem *);
void		*cpumem_next(struct cpumem_iter *, struct cpumem *);

#define CPUMEM_BOOT_MEMORY(_name, _sz)					\
static struct {								\
	unsigned char	mem[_sz];					\
	struct cpumem	cpumem;						\
} __aligned(CACHELINESIZE) _name##_boot_cpumem = {			\
	.cpumem = { _name##_boot_cpumem.mem }				\
}

#define CPUMEM_BOOT_INITIALIZER(_name)					\
	{ &_name##_boot_cpumem.cpumem }

#else /* MULTIPROCESSOR */
static inline void *
cpumem_enter(struct cpumem *cm)
{
	return (cm);
}

static inline void
cpumem_leave(struct cpumem *cm, void *mem)
{
	/* KDASSERT? */
}

static inline void *
cpumem_first(struct cpumem_iter *i, struct cpumem *cm)
{
	return (cm);
}

static inline void *
cpumem_next(struct cpumem_iter *i, struct cpumem *cm)
{
	return (NULL);
}

#define CPUMEM_BOOT_MEMORY(_name, _sz)					\
static struct {								\
	unsigned char	mem[_sz];					\
} _name##_boot_cpumem

#define CPUMEM_BOOT_INITIALIZER(_name)					\
	{ (struct cpumem *)&_name##_boot_cpumem.mem }

#endif /* MULTIPROCESSOR */

#define CPUMEM_FOREACH(_var, _iter, _cpumem)				\
	for ((_var) = cpumem_first((_iter), (_cpumem));			\
	    (_var) != NULL;						\
	    (_var) = cpumem_next((_iter), (_cpumem)))

struct cpumem	*counters_alloc(unsigned int, int);
struct cpumem	*counters_realloc(struct cpumem *, unsigned int, int);
void		 counters_free(struct cpumem *, int, unsigned int);
void		 counters_read(struct cpumem *, uint64_t *, unsigned int);
void		 counters_zero(struct cpumem *, unsigned int);

#ifdef MULTIPROCESSOR
static inline uint64_t *
counters_enter(struct counters_ref *ref, struct cpumem *cm)
{
	ref->c = cpumem_enter(cm);
	ref->g = ++(*ref->c); /* make the generation number odd */
	return (ref->c + 1);
}

static inline void
counters_leave(struct counters_ref *ref, struct cpumem *cm)
{
	membar_producer();
	(*ref->c) = ++ref->g; /* make the generation number even again */
	cpumem_leave(cm, ref->c);
}
#define COUNTERS_BOOT_MEMORY(_name, _n)					\
	CPUMEM_BOOT_MEMORY(_name, ((_n) + 1) * sizeof(uint64_t))
#else
static inline uint64_t *
counters_enter(struct counters_ref *r, struct cpumem *cm)
{
	r->c = cpumem_enter(cm);
	return (r->c);
}

static inline void
counters_leave(struct counters_ref *r, struct cpumem *cm)
{
	cpumem_leave(cm, r->c);
}

#define COUNTERS_BOOT_MEMORY(_name, _n)					\
	CPUMEM_BOOT_MEMORY(_name, (_n) * sizeof(uint64_t))
#endif

#define COUNTERS_BOOT_INITIALIZER(_name)	CPUMEM_BOOT_INITIALIZER(_name)

#endif /* _KERNEL */
#endif /* _SYS_PERCPU_H_ */
