/* 	$OpenBSD: kern_compat.h,v 1.8 2018/01/14 09:32:09 mpi Exp $ */

#ifndef _KERN_COMPAT_H_
#define _KERN_COMPAT_H_

#include <sys/socket.h>
#include <sys/domain.h>
#include <sys/queue.h>
#include <sys/mutex.h>
#include <sys/task.h>
#include <sys/select.h>
#include <arpa/inet.h>

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "srp_compat.h"

#define DIAGNOSTIC
#define INET
#define INET6

#define KASSERT(x)		assert(x)
#define KERNEL_ASSERT_LOCKED()	/* nothing */
#define KERNEL_LOCK()		/* nothing */
#define KERNEL_UNLOCK()		/* nothing */

#define panic(x...) errx(1, x)

#define malloc(size, bucket, flag)		calloc(1, size)
#define mallocarray(nelems, size, bucket, flag)	calloc(nelems, size)
#define free(x, bucket, size)			free(x)

struct pool {
	size_t pr_size;
};

#define	pool_init(a, b, c, d, e, f, g)	do { (a)->pr_size = (b); } while (0)
#define pool_setipl(pp, ipl)		/* nothing */
#define pool_get(pp, flags)		malloc((pp)->pr_size, 0, 0)
#define	pool_put(pp, rp)		free((rp), 0, 0)

#define	log(lvl, x...)	fprintf(stderr, x)

#define min(a, b) (a < b ? a : b)
#define max(a, b) (a < b ? b : a)

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

#define rtref(_rt)		((_rt)->rt_refcnt++)
#define rtfree(_rt)		(assert(--(_rt)->rt_refcnt >= 0))

#ifndef IPL_NONE
#define IPL_NONE 0
#endif
#define __mtx_enter(_mtx)	/* nothing */
#define __mtx_leave(_mtx)	/* nothing */

#define task_add(_tq, _t)	((_t)->t_func((_t)->t_arg))

extern struct domain *domains[];

#define IPL_SOFTNET	0

#define rw_init(rwl, name)
#define rw_enter_write(rwl)
#define rw_exit_write(rwl)
#define rw_assert_wrlock(rwl)

#define SET(t, f)	((t) |= (f))
#define CLR(t, f)	((t) &= ~(f))
#define ISSET(t, f)	((t) & (f))

struct rtentry;

int	 rt_hash(struct rtentry *, struct sockaddr *, uint32_t *);

#endif /* _KERN_COMPAT_H_ */
