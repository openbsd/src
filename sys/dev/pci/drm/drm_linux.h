/*	$OpenBSD: drm_linux.h,v 1.92 2018/10/31 08:50:25 kettenis Exp $	*/
/*
 * Copyright (c) 2013, 2014, 2015 Mark Kettenis
 * Copyright (c) 2017 Martin Pieuchot
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

#ifndef _DRM_LINUX_H_
#define _DRM_LINUX_H_

#include <sys/param.h>
#include <sys/atomic.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/stdint.h>
#include <sys/systm.h>
#include <sys/task.h>
#include <sys/time.h>
#include <sys/timeout.h>
#include <sys/tree.h>

#include <uvm/uvm_extern.h>

#include <ddb/db_var.h>

#include <dev/i2c/i2cvar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/pci/drm/linux_types.h>
#include <dev/pci/drm/drm_linux_atomic.h>
#include <dev/pci/drm/drm_linux_list.h>

/* The Linux code doesn't meet our usual standards! */
#ifdef __clang__
#pragma clang diagnostic ignored "-Wenum-conversion"
#pragma clang diagnostic ignored "-Winitializer-overrides"
#pragma clang diagnostic ignored "-Wtautological-compare"
#pragma clang diagnostic ignored "-Wunneeded-internal-declaration"
#pragma clang diagnostic ignored "-Wunused-const-variable"
#else
#pragma GCC diagnostic ignored "-Wformat-zero-length"
#endif

#define STUB() do { printf("%s: stub\n", __func__); } while(0)

typedef int irqreturn_t;
enum irqreturn {
	IRQ_NONE = 0,
	IRQ_HANDLED = 1
};

typedef int8_t   s8;
typedef uint8_t  u8;
typedef int16_t  s16;
typedef uint16_t u16;
typedef int32_t  s32;
typedef uint32_t u32;
typedef int64_t  s64;
typedef uint64_t u64;

#define U64_C(x) UINT64_C(x)
#define U64_MAX UINT64_MAX

typedef uint16_t __le16;
typedef uint16_t __be16;
typedef uint32_t __le32;
typedef uint32_t __be32;

typedef bus_addr_t dma_addr_t;
typedef bus_addr_t phys_addr_t;

typedef bus_addr_t resource_size_t;

typedef off_t loff_t;

#define __force
#define __always_unused	__unused
#define __read_mostly
#define __iomem
#define __must_check
#define __init
#define __exit

#ifndef __user
#define __user
#endif

#define __printf(x, y)

#define barrier()		__asm __volatile("" : : : "memory");

#define uninitialized_var(x) x

#if BYTE_ORDER == BIG_ENDIAN
#define __BIG_ENDIAN
#else
#define __LITTLE_ENDIAN
#endif

#define le16_to_cpu(x) letoh16(x)
#define le32_to_cpu(x) letoh32(x)
#define be16_to_cpu(x) betoh16(x)
#define be32_to_cpu(x) betoh32(x)
#define le16_to_cpup(x)	lemtoh16(x)
#define le32_to_cpup(x)	lemtoh32(x)
#define be16_to_cpup(x)	bemtoh16(x)
#define be32_to_cpup(x)	bemtoh32(x)
#define get_unaligned_le32(x)	lemtoh32(x)
#define cpu_to_le16(x) htole16(x)
#define cpu_to_le32(x) htole32(x)
#define cpu_to_be16(x) htobe16(x)
#define cpu_to_be32(x) htobe32(x)

static inline uint8_t
hweight8(uint32_t x)
{
	x = (x & 0x55) + ((x & 0xaa) >> 1);
	x = (x & 0x33) + ((x & 0xcc) >> 2);
	x = (x + (x >> 4)) & 0x0f;
	return (x);
}

static inline uint16_t
hweight16(uint32_t x)
{
	x = (x & 0x5555) + ((x & 0xaaaa) >> 1);
	x = (x & 0x3333) + ((x & 0xcccc) >> 2);
	x = (x + (x >> 4)) & 0x0f0f;
	x = (x + (x >> 8)) & 0x00ff;
	return (x);
}

static inline uint32_t
hweight32(uint32_t x)
{
	x = (x & 0x55555555) + ((x & 0xaaaaaaaa) >> 1);
	x = (x & 0x33333333) + ((x & 0xcccccccc) >> 2);
	x = (x + (x >> 4)) & 0x0f0f0f0f;
	x = (x + (x >> 8));
	x = (x + (x >> 16)) & 0x000000ff;
	return x;
}

static inline uint32_t
hweight64(uint64_t x)
{
	x = (x & 0x5555555555555555ULL) + ((x & 0xaaaaaaaaaaaaaaaaULL) >> 1);
	x = (x & 0x3333333333333333ULL) + ((x & 0xccccccccccccccccULL) >> 2);
	x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0fULL;
	x = (x + (x >> 8));
	x = (x + (x >> 16));
	x = (x + (x >> 32)) & 0x000000ff;
	return x;
}

#define lower_32_bits(n)	((u32)(n))
#define upper_32_bits(_val)	((u32)(((_val) >> 16) >> 16))
#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : (1ULL<<(n)) -1)
#define BIT(x)			(1UL << x)
#define BITS_TO_LONGS(x)	howmany((x), 8 * sizeof(long))

#define DECLARE_BITMAP(x, y)	unsigned long x[BITS_TO_LONGS(y)];
#define bitmap_empty(p, n)	(find_first_bit(p, n) == n)
#define GENMASK(h, l)		((~0U >> (32 - h -1)) & (~0U << l))

static inline void
bitmap_set(void *p, int b, u_int n)
{
	u_int end = b + n;

	for (; b < end; b++)
		__set_bit(b, p);
}

static inline void
bitmap_zero(void *p, u_int n)
{
	u_int *ptr = p;
	u_int b;

	for (b = 0; b < n; b += 32)
		ptr[b >> 5] = 0;
}

static inline void
bitmap_or(void *d, void *s1, void *s2, u_int n)
{
	u_int *dst = d;
	u_int *src1 = s1;
	u_int *src2 = s2;
	u_int b;

	for (b = 0; b < n; b += 32)
		dst[b >> 5] = src1[b >> 5] | src2[b >> 5];
}

static inline int
bitmap_weight(void *p, u_int n)
{
	u_int *ptr = p;
	u_int b;
	int sum = 0;

	for (b = 0; b < n; b += 32)
		sum += hweight32(ptr[b >> 5]);
	return sum;
}

#define DECLARE_HASHTABLE(name, bits) struct hlist_head name[1 << (bits)]

static inline void
__hash_init(struct hlist_head *table, u_int size)
{
	u_int i;

	for (i = 0; i < size; i++)
		INIT_HLIST_HEAD(&table[i]);
}

static inline bool
__hash_empty(struct hlist_head *table, u_int size)
{
	u_int i;

	for (i = 0; i < size; i++) {
		if (!hlist_empty(&table[i]))
			return false;
	}

	return true;
}

#define __hash(table, key)	&table[key % (nitems(table) - 1)]

#define hash_init(table)	__hash_init(table, nitems(table))
#define hash_add(table, node, key) \
	hlist_add_head(node, __hash(table, key))
#define hash_del(node)		hlist_del_init(node)
#define hash_empty(table)	__hash_empty(table, nitems(table))
#define hash_for_each_possible(table, obj, member, key) \
	hlist_for_each_entry(obj, __hash(table, key), member)
#define hash_for_each_safe(table, i, tmp, obj, member) 	\
	for (i = 0; i < nitems(table); i++)		\
	       hlist_for_each_entry_safe(obj, tmp, &table[i], member)

#define ACCESS_ONCE(x)		(x)

#define EXPORT_SYMBOL(x)

#define IS_ENABLED(x) x - 0

#define IS_BUILTIN(x) 1

struct device_node;

struct device_driver {
	struct device *dev;
};

#define dev_get_drvdata(x)	NULL
#define dev_set_drvdata(x, y)
#define dev_name(dev)		""

#define devm_kzalloc(x, y, z)	kzalloc(y, z)

struct module;

#define MODULE_AUTHOR(x)
#define MODULE_DESCRIPTION(x)
#define MODULE_LICENSE(x)
#define MODULE_FIRMWARE(x)
#define MODULE_DEVICE_TABLE(x, y)
#define MODULE_PARM_DESC(parm, desc)
#define module_param_named(name, value, type, perm)
#define module_param_named_unsafe(name, value, type, perm)
#define module_param_unsafe(name, type, perm)
#define module_init(x)
#define module_exit(x)

#define THIS_MODULE	NULL

#define ARRAY_SIZE nitems

#define ERESTARTSYS	EINTR
#define ETIME		ETIMEDOUT
#define EREMOTEIO	EIO
#define ENOTSUPP	ENOTSUP
#define ENODATA		ENOTSUP
#define ECHRNG		EINVAL

#define KERN_INFO	""
#define KERN_WARNING	""
#define KERN_NOTICE	""
#define KERN_DEBUG	""
#define KERN_CRIT	""
#define KERN_ERR	""

#define KBUILD_MODNAME "drm"

#define UTS_RELEASE	""

#define TASK_COMM_LEN	(MAXCOMLEN + 1)

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#define printk_once(fmt, arg...) ({		\
	static int __warned;			\
	if (!__warned) {			\
		printf(fmt, ## arg);		\
		__warned = 1;			\
	}					\
})

#define printk(fmt, arg...)	printf(fmt, ## arg)
#define pr_warn(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#define pr_warn_once(fmt, arg...)	printk_once(pr_fmt(fmt), ## arg)
#define pr_notice(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#define pr_crit(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#define pr_err(fmt, arg...)	printf(pr_fmt(fmt), ## arg)

#ifdef DRMDEBUG
#define pr_info(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#define pr_info_once(fmt, arg...)	printk_once(pr_fmt(fmt), ## arg)
#define pr_debug(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#else
#define pr_info(fmt, arg...)	do { } while(0)
#define pr_info_once(fmt, arg...)	do { } while(0)
#define pr_debug(fmt, arg...)	do { } while(0)
#endif

#define dev_warn(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *WARNING* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)
#define dev_notice(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *NOTICE* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)
#define dev_crit(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *ERROR* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)
#define dev_err(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *ERROR* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)

#ifdef DRMDEBUG
#define dev_info(dev, fmt, arg...)				\
	printf("drm: " fmt, ## arg)
#define dev_debug(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *DEBUG* " fmt, curproc->p_p->ps_pid,	\
	    __func__ , ## arg)
#else
#define dev_info(dev, fmt, arg...) 				\
	    do { } while(0)
#define dev_debug(dev, fmt, arg...) 				\
	    do { } while(0)
#endif

enum {
	DUMP_PREFIX_NONE,
	DUMP_PREFIX_ADDRESS,
	DUMP_PREFIX_OFFSET
};

void print_hex_dump(const char *, const char *, int, int, int,
	 const void *, size_t, bool);

#define scnprintf(str, size, fmt, arg...) snprintf(str, size, fmt, ## arg)

#define unlikely(x)	__builtin_expect(!!(x), 0)
#define likely(x)	__builtin_expect(!!(x), 1)

#define BUG()								\
do {									\
	panic("BUG at %s:%d", __FILE__, __LINE__);			\
} while (0)

#ifndef DIAGNOSTIC
#define BUG_ON(x)	((void)(x))
#else
#define BUG_ON(x)	KASSERT(!(x))
#endif

#define BUILD_BUG()
#define BUILD_BUG_ON(x) CTASSERT(!(x))
#define BUILD_BUG_ON_NOT_POWER_OF_2(x)
#define BUILD_BUG_ON_MSG(x, y)

#define WARN(condition, fmt...) ({ 					\
	int __ret = !!(condition);					\
	if (__ret)							\
		printf(fmt);						\
	unlikely(__ret);						\
})

#define WARN_ONCE(condition, fmt...) ({					\
	static int __warned;						\
	int __ret = !!(condition);					\
	if (__ret && !__warned) {					\
		printf(fmt);						\
		__warned = 1;						\
	}								\
	unlikely(__ret);						\
})

#define _WARN_STR(x) #x

#define WARN_ON(condition) ({						\
	int __ret = !!(condition);					\
	if (__ret)							\
		printf("WARNING %s failed at %s:%d\n",			\
		    _WARN_STR(condition), __FILE__, __LINE__);		\
	unlikely(__ret);						\
})

#define WARN_ON_ONCE(condition) ({					\
	static int __warned;						\
	int __ret = !!(condition);					\
	if (__ret && !__warned) {					\
		printf("WARNING %s failed at %s:%d\n",			\
		    _WARN_STR(condition), __FILE__, __LINE__);		\
		__warned = 1;						\
	}								\
	unlikely(__ret);						\
})

#define TP_PROTO(x...) x

#define DEFINE_EVENT(template, name, proto, args) \
static inline void trace_##name(proto) {}

#define DEFINE_EVENT_PRINT(template, name, proto, args, print) \
static inline void trace_##name(proto) {}

#define TRACE_EVENT(name, proto, args, tstruct, assign, print) \
static inline void trace_##name(proto) {}

#define TRACE_EVENT_CONDITION(name, proto, args, cond, tstruct, assign, print) \
static inline void trace_##name(proto) {}

#define DECLARE_EVENT_CLASS(name, proto, args, tstruct, assign, print) \
static inline void trace_##name(proto) {}

#define IS_ERR_VALUE(x) unlikely((x) >= (unsigned long)-ELAST)

static inline void *
ERR_PTR(long error)
{
	return (void *) error;
}

static inline long
PTR_ERR(const void *ptr)
{
	return (long) ptr;
}

static inline long
IS_ERR(const void *ptr)
{
        return IS_ERR_VALUE((unsigned long)ptr);
}

static inline long
IS_ERR_OR_NULL(const void *ptr)
{
        return !ptr || IS_ERR_VALUE((unsigned long)ptr);
}

static inline void *
ERR_CAST(const void *ptr)
{
	return (void *)ptr;
}

static inline int
PTR_ERR_OR_ZERO(const void *ptr)
{
	return IS_ERR(ptr)? PTR_ERR(ptr) : 0;
}

#define swap(a, b) \
	do { __typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while(0)

#define container_of(ptr, type, member) ({                      \
	const __typeof( ((type *)0)->member ) *__mptr = (ptr);        \
	(type *)( (char *)__mptr - offsetof(type,member) );})

#ifndef __DECONST
#define __DECONST(type, var)    ((type)(__uintptr_t)(const void *)(var))
#endif

typedef struct rwlock rwlock_t;
typedef struct mutex spinlock_t;
#define DEFINE_SPINLOCK(x)	struct mutex x
#define DEFINE_MUTEX(x)		struct rwlock x

static inline void
_spin_lock_irqsave(struct mutex *mtxp, __unused unsigned long flags
    LOCK_FL_VARS)
{
	_mtx_enter(mtxp LOCK_FL_ARGS);
}
static inline void
_spin_unlock_irqrestore(struct mutex *mtxp, __unused unsigned long flags
    LOCK_FL_VARS)
{
	_mtx_leave(mtxp LOCK_FL_ARGS);
}
#define spin_lock_irqsave(m, fl)	\
	_spin_lock_irqsave(m, fl LOCK_FILE_LINE)
#define spin_unlock_irqrestore(m, fl)	\
	_spin_unlock_irqrestore(m, fl LOCK_FILE_LINE)


#define spin_lock(mtxp)			mtx_enter(mtxp)
#define spin_unlock(mtxp)		mtx_leave(mtxp)
#define spin_lock_irq(mtxp)		mtx_enter(mtxp)
#define spin_unlock_irq(mtxp)		mtx_leave(mtxp)
#define assert_spin_locked(mtxp)	MUTEX_ASSERT_LOCKED(mtxp)
#define mutex_lock_interruptible(rwl)	-rw_enter(rwl, RW_WRITE | RW_INTR)
#define mutex_lock(rwl)			rw_enter_write(rwl)
#define mutex_lock_nest_lock(rwl, sub)	rw_enter_write(rwl)
#define mutex_trylock(rwl)		(rw_enter(rwl, RW_WRITE | RW_NOSLEEP) == 0)
#define mutex_unlock(rwl)		rw_exit_write(rwl)
#define mutex_is_locked(rwl)		(rw_status(rwl) == RW_WRITE)
#define mutex_destroy(rwl)
#define down_read(rwl)			rw_enter_read(rwl)
#define down_read_trylock(rwl)		(rw_enter(rwl, RW_READ | RW_NOSLEEP) == 0)
#define up_read(rwl)			rw_exit_read(rwl)
#define down_write(rwl)			rw_enter_write(rwl)
#define up_write(rwl)			rw_exit_write(rwl)
#define read_lock(rwl)			rw_enter_read(rwl)
#define read_unlock(rwl)		rw_exit_read(rwl)
#define write_lock(rwl)			rw_enter_write(rwl)
#define write_unlock(rwl)		rw_exit_write(rwl)

#define might_lock(lock)
#define lockdep_assert_held(lock)	do { (void)(lock); } while(0)

#define IRQF_SHARED	0

#define local_irq_save(x)		(x) = splhigh()
#define local_irq_restore(x)		splx((x))

#define request_irq(irq, hdlr, flags, name, dev)	(0)
#define free_irq(irq, dev)
#define synchronize_irq(x)

typedef struct wait_queue wait_queue_t;
struct wait_queue {
	unsigned int flags;
	void *private;
	int (*func)(wait_queue_t *, unsigned, int, void *);
};

extern struct mutex sch_mtx;
extern void *sch_ident;
extern int sch_priority;

struct wait_queue_head {
	struct mutex lock;
	unsigned int count;
	struct wait_queue *_wq;
};
typedef struct wait_queue_head wait_queue_head_t;

#define MAX_SCHEDULE_TIMEOUT (INT32_MAX)

static inline void
init_waitqueue_head(wait_queue_head_t *wq)
{
	mtx_init(&wq->lock, IPL_TTY);
	wq->count = 0;
	wq->_wq = NULL;
}

static inline void
__add_wait_queue(wait_queue_head_t *head, wait_queue_t *new)
{
	head->_wq = new;
}

static inline void
__remove_wait_queue(wait_queue_head_t *head, wait_queue_t *old)
{
	head->_wq = NULL;
}

#define __wait_event_intr_timeout(wq, condition, timo, prio)		\
({									\
	long ret = timo;						\
	do {								\
		int deadline, __error;					\
									\
		KASSERT(!cold);						\
									\
		mtx_enter(&sch_mtx);					\
		atomic_inc_int(&(wq).count);				\
		deadline = ticks + ret;					\
		__error = msleep(&wq, &sch_mtx, prio, "drmweti", ret);	\
		ret = deadline - ticks;					\
		atomic_dec_int(&(wq).count);				\
		if (__error == ERESTART || __error == EINTR) {		\
			ret = -ERESTARTSYS;				\
			mtx_leave(&sch_mtx);				\
			break;						\
		}							\
		if (timo && (ret <= 0 || __error == EWOULDBLOCK)) { 	\
			mtx_leave(&sch_mtx);				\
			ret = ((condition)) ? 1 : 0;			\
			break;						\
 		}							\
		mtx_leave(&sch_mtx);					\
	} while (ret > 0 && !(condition));				\
	ret;								\
})

/*
 * Sleep until `condition' gets true.
 */
#define wait_event(wq, condition) 		\
do {						\
	if (!(condition))			\
		__wait_event_intr_timeout(wq, condition, 0, 0); \
} while (0)

#define wait_event_interruptible_locked(wq, condition) 		\
({						\
	int __ret = 0;				\
	if (!(condition))			\
		__ret = __wait_event_intr_timeout(wq, condition, 0, PCATCH); \
	__ret;					\
})

/*
 * Sleep until `condition' gets true or `timo' expires.
 *
 * Returns 0 if `condition' is still false when `timo' expires or
 * the remaining (>=1) ticks otherwise.
 */
#define wait_event_timeout(wq, condition, timo)	\
({						\
	long __ret = timo;			\
	if (!(condition))			\
		__ret = __wait_event_intr_timeout(wq, condition, timo, 0); \
	__ret;					\
})

/*
 * Sleep until `condition' gets true, `timo' expires or the process
 * receives a signal.
 *
 * Returns -ERESTARTSYS if interrupted by a signal.
 * Returns 0 if `condition' is still false when `timo' expires or
 * the remaining (>=1) ticks otherwise.
 */
#define wait_event_interruptible_timeout(wq, condition, timo) \
({						\
	long __ret = timo;			\
	if (!(condition))			\
		__ret = __wait_event_intr_timeout(wq, condition, timo, PCATCH);\
	__ret;					\
})

static inline void
_wake_up(wait_queue_head_t *wq LOCK_FL_VARS)
{
	_mtx_enter(&wq->lock LOCK_FL_ARGS);
	if (wq->_wq != NULL && wq->_wq->func != NULL)
		wq->_wq->func(wq->_wq, 0, wq->_wq->flags, NULL);
	else {
		mtx_enter(&sch_mtx);
		wakeup(wq);
		mtx_leave(&sch_mtx);
	}
	_mtx_leave(&wq->lock LOCK_FL_ARGS);
}

#define wake_up_process(task)			\
do {						\
	mtx_enter(&sch_mtx);			\
	wakeup(task);				\
	mtx_leave(&sch_mtx);			\
} while (0)

#define wake_up(wq)			_wake_up(wq LOCK_FILE_LINE)
#define wake_up_all(wq)			_wake_up(wq LOCK_FILE_LINE)

static inline void
wake_up_all_locked(wait_queue_head_t *wq)
{
	if (wq->_wq != NULL && wq->_wq->func != NULL)
		wq->_wq->func(wq->_wq, 0, wq->_wq->flags, NULL);
	else {
		mtx_enter(&sch_mtx);
		wakeup(wq);
		mtx_leave(&sch_mtx);
	}
}

#define wake_up_interruptible(wq)	_wake_up(wq LOCK_FILE_LINE)
#define waitqueue_active(wq)		((wq)->count > 0)

struct completion {
	u_int done;
	wait_queue_head_t wait;
};

#define INIT_COMPLETION(x) ((x).done = 0)

static inline void
init_completion(struct completion *x)
{
	x->done = 0;
	mtx_init(&x->wait.lock, IPL_NONE);
}

static inline u_long
_wait_for_completion_interruptible_timeout(struct completion *x, u_long timo
    LOCK_FL_VARS)
{
	int ret;

	_mtx_enter(&x->wait.lock LOCK_FL_ARGS);
	while (x->done == 0) {
		ret = msleep(x, &x->wait.lock, PCATCH, "wfcit", timo);
		if (ret) {
			_mtx_leave(&x->wait.lock LOCK_FL_ARGS);
			return (ret == EWOULDBLOCK) ? 0 : -ret;
		}
	}

	return 1;
}
#define wait_for_completion_interruptible_timeout(x, timo)	\
	_wait_for_completion_interruptible_timeout(x, timo LOCK_FILE_LINE)

static inline void
_complete_all(struct completion *x LOCK_FL_VARS)
{
	_mtx_enter(&x->wait.lock LOCK_FL_ARGS);
	x->done = 1;
	_mtx_leave(&x->wait.lock LOCK_FL_ARGS);
	wakeup(x);
}
#define complete_all(x)	_complete_all(x LOCK_FILE_LINE)

struct workqueue_struct;

#define system_wq (struct workqueue_struct *)systq
#define system_long_wq (struct workqueue_struct *)systq

static inline struct workqueue_struct *
alloc_ordered_workqueue(const char *name, int flags)
{
	struct taskq *tq = taskq_create(name, 1, IPL_TTY, 0);
	return (struct workqueue_struct *)tq;
}

static inline struct workqueue_struct *
create_singlethread_workqueue(const char *name)
{
	struct taskq *tq = taskq_create(name, 1, IPL_TTY, 0);
	return (struct workqueue_struct *)tq;
}

static inline void
destroy_workqueue(struct workqueue_struct *wq)
{
	taskq_destroy((struct taskq *)wq);
}

struct work_struct {
	struct task task;
	struct taskq *tq;
};

typedef void (*work_func_t)(struct work_struct *);

static inline void
INIT_WORK(struct work_struct *work, work_func_t func)
{
	work->tq = systq;
	task_set(&work->task, (void (*)(void *))func, work);
}

#define INIT_WORK_ONSTACK(x, y)	INIT_WORK((x), (y))

static inline bool
queue_work(struct workqueue_struct *wq, struct work_struct *work)
{
	work->tq = (struct taskq *)wq;
	return task_add(work->tq, &work->task);
}

static inline void
cancel_work_sync(struct work_struct *work)
{
	task_del(work->tq, &work->task);
}

struct delayed_work {
	struct work_struct work;
	struct timeout to;
	struct taskq *tq;
};

#define system_power_efficient_wq ((struct workqueue_struct *)systq)

static inline struct delayed_work *
to_delayed_work(struct work_struct *work)
{
	return container_of(work, struct delayed_work, work);
}

static void
__delayed_work_tick(void *arg)
{
	struct delayed_work *dwork = arg;

	task_add(dwork->tq, &dwork->work.task);
}

static inline void
INIT_DELAYED_WORK(struct delayed_work *dwork, work_func_t func)
{
	INIT_WORK(&dwork->work, func);
	timeout_set(&dwork->to, __delayed_work_tick, &dwork->work);
}

static inline bool
schedule_work(struct work_struct *work)
{
	return task_add(work->tq, &work->task);
}

static inline bool
schedule_delayed_work(struct delayed_work *dwork, int jiffies)
{
	dwork->tq = systq;
	return timeout_add(&dwork->to, jiffies);
}

static inline bool
queue_delayed_work(struct workqueue_struct *wq,
    struct delayed_work *dwork, int jiffies)
{
	dwork->tq = (struct taskq *)wq;
	return timeout_add(&dwork->to, jiffies);
}

static inline bool
mod_delayed_work(struct workqueue_struct *wq,
    struct delayed_work *dwork, int jiffies)
{
	dwork->tq = (struct taskq *)wq;
	return (timeout_add(&dwork->to, jiffies) == 0);
}

static inline bool
cancel_delayed_work(struct delayed_work *dwork)
{
	if (timeout_del(&dwork->to))
		return true;
	return task_del(dwork->tq, &dwork->work.task);
}

static inline bool
cancel_delayed_work_sync(struct delayed_work *dwork)
{
	if (timeout_del(&dwork->to))
		return true;
	return task_del(dwork->tq, &dwork->work.task);
}

void flush_workqueue(struct workqueue_struct *);
void flush_work(struct work_struct *);
void flush_delayed_work(struct delayed_work *);
#define flush_scheduled_work()	flush_workqueue(system_wq)

#define destroy_work_on_stack(x)

typedef void *async_cookie_t;
#define async_schedule(func, data)	(func)((data), NULL)

#define local_irq_disable()	intr_disable()
#define local_irq_enable()	intr_enable()

#define setup_timer(x, y, z)	timeout_set((x), (void (*)(void *))(y), (void *)(z))
#define mod_timer(x, y)		timeout_add((x), (y - jiffies))
#define mod_timer_pinned(x, y)	timeout_add((x), (y - jiffies))
#define del_timer_sync(x)	timeout_del((x))
#define timer_pending(x)	timeout_pending((x))

#define cond_resched()		sched_pause(yield)
#define drm_need_resched() \
    (curcpu()->ci_schedstate.spc_schedflags & SPCF_SHOULDYIELD)

#define TASK_UNINTERRUPTIBLE	0
#define TASK_INTERRUPTIBLE	PCATCH
#define TASK_RUNNING		-1

#define signal_pending_state(x, y) CURSIG(curproc)
#define signal_pending(y) CURSIG(curproc)

#define NSEC_PER_USEC	1000L
#define NSEC_PER_MSEC	1000000L
#define NSEC_PER_SEC	1000000000L
#define KHZ2PICOS(a)	(1000000000UL/(a))

extern struct timespec ns_to_timespec(const int64_t);
extern int64_t timeval_to_ns(const struct timeval *);
extern int64_t timeval_to_us(const struct timeval *);
extern struct timeval ns_to_timeval(const int64_t);

static inline struct timespec
timespec_sub(struct timespec t1, struct timespec t2)
{
	struct timespec diff;

	timespecsub(&t1, &t2, &diff);
	return diff;
}

#define time_in_range(x, min, max) ((x) >= (min) && (x) <= (max))

extern volatile unsigned long jiffies;
#define jiffies_64 jiffies /* XXX */
#undef HZ
#define HZ	hz

#define MAX_JIFFY_OFFSET	((INT_MAX >> 1) - 1)

static inline unsigned long
round_jiffies_up(unsigned long j)
{
	return roundup(j, hz);
}

static inline unsigned long
round_jiffies_up_relative(unsigned long j)
{
	return roundup(j, hz);
}

#define jiffies_to_msecs(x)	(((uint64_t)(x)) * 1000 / hz)
#define jiffies_to_usecs(x)	(((uint64_t)(x)) * 1000000 / hz)
#define msecs_to_jiffies(x)	(((uint64_t)(x)) * hz / 1000)
#define nsecs_to_jiffies64(x)	(((uint64_t)(x)) * hz / 1000000000)
#define get_jiffies_64()	jiffies
#define time_after(a,b)		((long)(b) - (long)(a) < 0)
#define time_after_eq(a,b)	((long)(b) - (long)(a) <= 0)
#define get_seconds()		time_second
#define getrawmonotonic(x)	nanouptime(x)

static inline void
set_normalized_timespec(struct timespec *ts, time_t sec, int64_t nsec)
{
	while (nsec > NSEC_PER_SEC) {
		nsec -= NSEC_PER_SEC;
		sec++;
	}

	ts->tv_sec = sec;
	ts->tv_nsec = nsec;
}

static inline int64_t
timespec_to_ns(const struct timespec *ts)
{
	return ((ts->tv_sec * NSEC_PER_SEC) + ts->tv_nsec);
}

static inline unsigned long
timespec_to_jiffies(const struct timespec *ts)
{
	long long to_ticks;

	to_ticks = (long long)hz * ts->tv_sec + ts->tv_nsec / (tick * 1000);
	if (to_ticks > INT_MAX)
		to_ticks = INT_MAX;

	return ((int)to_ticks);
}

static inline int
timespec_valid(const struct timespec *ts)
{
	if (ts->tv_sec < 0 || ts->tv_sec > 100000000 ||
	    ts->tv_nsec < 0 || ts->tv_nsec >= 1000000000)
		return (0);
	return (1);
}

typedef struct timeval ktime_t;

static inline struct timeval
ktime_get(void)
{
	struct timeval tv;
	
	getmicrouptime(&tv);
	return tv;
}

static inline struct timeval
ktime_get_monotonic_offset(void)
{
	struct timeval tv = {0, 0};
	return tv;
}

static inline int64_t
ktime_to_us(struct timeval tv)
{
	return timeval_to_us(&tv);
}

static inline int64_t
ktime_to_ns(struct timeval tv)
{
	return timeval_to_ns(&tv);
}

static inline int64_t
ktime_get_raw_ns(void)
{
	return ktime_to_ns(ktime_get());
}

#define ktime_to_timeval(tv) (tv)

static inline struct timeval
ktime_sub(struct timeval a, struct timeval b)
{
	struct timeval res;
	timersub(&a, &b, &res);
	return res;
}

static inline struct timeval
ktime_add_ns(struct timeval tv, int64_t ns)
{
	return ns_to_timeval(timeval_to_ns(&tv) + ns);
}

static inline struct timeval
ktime_sub_ns(struct timeval tv, int64_t ns)
{
	return ns_to_timeval(timeval_to_ns(&tv) - ns);
}

static inline int64_t
ktime_us_delta(struct timeval a, struct timeval b)
{
	return ktime_to_us(ktime_sub(a, b));
}

#define ktime_mono_to_real(x) (x)
#define ktime_get_real() ktime_get()

#define do_gettimeofday(tv) getmicrouptime(tv)

#define GFP_ATOMIC	M_NOWAIT
#define GFP_NOWAIT	M_NOWAIT
#define GFP_KERNEL	(M_WAITOK | M_CANFAIL)
#define GFP_USER	(M_WAITOK | M_CANFAIL)
#define GFP_TEMPORARY	(M_WAITOK | M_CANFAIL)
#define GFP_HIGHUSER	0
#define GFP_DMA32	0
#define __GFP_NOWARN	0
#define __GFP_NORETRY	0
#define __GFP_ZERO	M_ZERO

static inline void *
kmalloc(size_t size, int flags)
{
	return malloc(size, M_DRM, flags);
}

static inline void *
kmalloc_array(size_t n, size_t size, int flags)
{
	if (n == 0 || SIZE_MAX / n < size)
		return NULL;
	return malloc(n * size, M_DRM, flags);
}

static inline void *
kcalloc(size_t n, size_t size, int flags)
{
	if (n == 0 || SIZE_MAX / n < size)
		return NULL;
	return malloc(n * size, M_DRM, flags | M_ZERO);
}

static inline void *
kzalloc(size_t size, int flags)
{
	return malloc(size, M_DRM, flags | M_ZERO);
}

static inline void
kfree(const void *objp)
{
	free((void *)objp, M_DRM, 0);
}

static inline void *
kmemdup(const void *src, size_t len, int flags)
{
	void *p = malloc(len, M_DRM, flags);
	if (p)
		memcpy(p, src, len);
	return (p);
}

static inline char *
kasprintf(int flags, const char *fmt, ...)
{
	char *buf;
	size_t len;
	va_list ap;

	va_start(ap, fmt);
	len = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	buf = kmalloc(len, flags);
	if (buf) {
		va_start(ap, fmt);
		vsnprintf(buf, len, fmt, ap);
		va_end(ap);
	}

	return buf;
}

static inline void *
vmalloc(unsigned long size)
{
	return malloc(size, M_DRM, M_WAITOK | M_CANFAIL);
}

static inline void *
vzalloc(unsigned long size)
{
	return malloc(size, M_DRM, M_WAITOK | M_CANFAIL | M_ZERO);
}

static inline void
vfree(void *objp)
{
	free(objp, M_DRM, 0);
}

struct kref {
	uint32_t refcount;
};

static inline void
kref_init(struct kref *ref)
{
	ref->refcount = 1;
}

static inline void
kref_get(struct kref *ref)
{
	atomic_inc_int(&ref->refcount);
}

static inline int
kref_get_unless_zero(struct kref *ref)
{
	if (ref->refcount != 0) {
		atomic_inc_int(&ref->refcount);
		return (1);
	} else {
		return (0);
	}
}

static inline void
kref_put(struct kref *ref, void (*release)(struct kref *ref))
{
	if (atomic_dec_int_nv(&ref->refcount) == 0)
		release(ref);
}

static inline void
kref_sub(struct kref *ref, unsigned int v, void (*release)(struct kref *ref))
{
	if (atomic_sub_int_nv(&ref->refcount, v) == 0)
		release(ref);
}

static inline int
kref_put_mutex(struct kref *kref, void (*release)(struct kref *kref),
    struct rwlock *lock)
{
	if (!atomic_add_unless(&kref->refcount, -1, 1)) {
		rw_enter_write(lock);
		if (likely(atomic_dec_and_test(&kref->refcount))) {
			release(kref);
			return 1;
		}
		rw_exit_write(lock);
		return 0;
	}

	return 0;
}

struct kobject {
	struct kref kref;
	struct kobj_type *type;
};

struct kobj_type {
	void (*release)(struct kobject *);
};

static inline void
kobject_init(struct kobject *obj, struct kobj_type *type)
{
	kref_init(&obj->kref);
	obj->type = type;
}

static inline int
kobject_init_and_add(struct kobject *obj, struct kobj_type *type,
    struct kobject *parent, const char *fmt, ...)
{
	kobject_init(obj, type);
	return (0);
}

static inline struct kobject *
kobject_get(struct kobject *obj)
{
	if (obj != NULL)
		kref_get(&obj->kref);
	return (obj);
}

static inline void
kobject_release(struct kref *ref)
{
	struct kobject *obj = container_of(ref, struct kobject, kref);
	if (obj->type && obj->type->release)
		obj->type->release(obj);
}

static inline void
kobject_put(struct kobject *obj)
{
	if (obj != NULL)
		kref_put(&obj->kref, kobject_release);
}

static inline void
kobject_del(struct kobject *obj)
{
}

#define	DEFINE_WAIT(wait)		wait_queue_head_t *wait = NULL

static inline void
prepare_to_wait(wait_queue_head_t *wq, wait_queue_head_t **wait, int state)
{
	if (*wait == NULL) {
		mtx_enter(&sch_mtx);
		*wait = wq;
	}
	MUTEX_ASSERT_LOCKED(&sch_mtx);
	sch_ident = wq;
	sch_priority = state;
}

static inline void
finish_wait(wait_queue_head_t *wq, wait_queue_head_t **wait)
{
	if (*wait) {
		MUTEX_ASSERT_LOCKED(&sch_mtx);
		sch_ident = NULL;
		mtx_leave(&sch_mtx);
	}
}

static inline void
set_current_state(int state)
{
	if (sch_ident != curproc)
		mtx_enter(&sch_mtx);
	MUTEX_ASSERT_LOCKED(&sch_mtx);
	sch_ident = curproc;
	sch_priority = state;
}

static inline void
__set_current_state(int state)
{
	KASSERT(state == TASK_RUNNING);
	if (sch_ident == curproc) {
		MUTEX_ASSERT_LOCKED(&sch_mtx);
		sch_ident = NULL;
		mtx_leave(&sch_mtx);
	}
}

static inline long
schedule_timeout(long timeout)
{
	int err;
	long deadline;

	if (cold) {
		delay((timeout * 1000000) / hz);
		return 0;
	}

	if (timeout == MAX_SCHEDULE_TIMEOUT) {
		err = msleep(sch_ident, &sch_mtx, sch_priority, "schto", 0);
		sch_ident = curproc;
		return timeout;
	}

	deadline = ticks + timeout;
	err = msleep(sch_ident, &sch_mtx, sch_priority, "schto", timeout);
	timeout = deadline - ticks;
	if (timeout < 0)
		timeout = 0;
	sch_ident = curproc;
	return timeout;
}

struct seq_file;

static inline void
seq_printf(struct seq_file *m, const char *fmt, ...) {};

#define preempt_enable()
#define preempt_disable()

#define FENCE_TRACE(fence, fmt, args...) do {} while(0)

struct fence {
	struct kref refcount;
	const struct fence_ops *ops;
	unsigned long flags;
	unsigned int context;
	unsigned int seqno;
	struct mutex *lock;
	struct list_head cb_list;
};

enum fence_flag_bits {
	FENCE_FLAG_SIGNALED_BIT,
	FENCE_FLAG_ENABLE_SIGNAL_BIT,
	FENCE_FLAG_USER_BITS,
};

struct fence_ops {
	const char * (*get_driver_name)(struct fence *);
	const char * (*get_timeline_name)(struct fence *);
	bool (*enable_signaling)(struct fence *);
	bool (*signaled)(struct fence *);
	long (*wait)(struct fence *, bool, long);
	void (*release)(struct fence *);
};

struct fence_cb;
typedef void (*fence_func_t)(struct fence *fence, struct fence_cb *cb);

struct fence_cb {
	struct list_head node;
	fence_func_t func;
};

unsigned int fence_context_alloc(unsigned int);

static inline struct fence *
fence_get(struct fence *fence)
{
	if (fence)
		kref_get(&fence->refcount);
	return fence;
}

static inline struct fence *
fence_get_rcu(struct fence *fence)
{
	if (fence)
		kref_get(&fence->refcount);
	return fence;
}

static inline void
fence_release(struct kref *ref)
{
	struct fence *fence = container_of(ref, struct fence, refcount);
	if (fence->ops && fence->ops->release)
		fence->ops->release(fence);
	else
		free(fence, M_DRM, 0);
}

static inline void
fence_put(struct fence *fence)
{
	if (fence)
		kref_put(&fence->refcount, fence_release);
}

static inline int
fence_signal(struct fence *fence)
{
	if (fence == NULL)
		return -EINVAL;

	if (test_and_set_bit(FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return -EINVAL;

	if (test_bit(FENCE_FLAG_ENABLE_SIGNAL_BIT, &fence->flags)) {
		struct fence_cb *cur, *tmp;

		mtx_enter(fence->lock);
		list_for_each_entry_safe(cur, tmp, &fence->cb_list, node) {
			list_del_init(&cur->node);
			cur->func(fence, cur);
		}
		mtx_leave(fence->lock);
	}

	return 0;
}

static inline int
fence_signal_locked(struct fence *fence)
{
	struct fence_cb *cur, *tmp;

	if (fence == NULL)
		return -EINVAL;

	if (test_and_set_bit(FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return -EINVAL;

	list_for_each_entry_safe(cur, tmp, &fence->cb_list, node) {
		list_del_init(&cur->node);
		cur->func(fence, cur);
	}

	return 0;
}

static inline bool
fence_is_signaled(struct fence *fence)
{
	if (test_bit(FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return true;

	if (fence->ops->signaled && fence->ops->signaled(fence)) {
		fence_signal(fence);
		return true;
	}

	return false;
}

static inline long
fence_wait_timeout(struct fence *fence, bool intr, signed long timeout)
{
	if (timeout < 0)
		return -EINVAL;

	if (timeout == 0)
		return fence_is_signaled(fence);

	return fence->ops->wait(fence, intr, timeout);
}

static inline long
fence_wait(struct fence *fence, bool intr)
{
	return fence_wait_timeout(fence, intr, MAX_SCHEDULE_TIMEOUT);
}

static inline void
fence_enable_sw_signaling(struct fence *fence)
{
	if (!test_and_set_bit(FENCE_FLAG_ENABLE_SIGNAL_BIT, &fence->flags) &&
	    !test_bit(FENCE_FLAG_SIGNALED_BIT, &fence->flags)) {
		mtx_enter(fence->lock);
		if (!fence->ops->enable_signaling(fence))
			fence_signal_locked(fence);
		mtx_leave(fence->lock);
	}
}

static inline void
fence_init(struct fence *fence, const struct fence_ops *ops,
    struct mutex *lock, unsigned context, unsigned seqno)
{
	fence->ops = ops;
	fence->lock = lock;
	fence->context = context;
	fence->seqno = seqno;
	fence->flags = 0;
	kref_init(&fence->refcount);
	INIT_LIST_HEAD(&fence->cb_list);
}

static inline int
fence_add_callback(struct fence *fence, struct fence_cb *cb,
    fence_func_t func)
{
	int ret = 0;
	bool was_set;

	if (WARN_ON(!fence || !func))
		return -EINVAL;

	if (test_bit(FENCE_FLAG_SIGNALED_BIT, &fence->flags)) {
		INIT_LIST_HEAD(&cb->node);
		return -ENOENT;
	}

	mtx_enter(fence->lock);

	was_set = test_and_set_bit(FENCE_FLAG_ENABLE_SIGNAL_BIT, &fence->flags);

	if (test_bit(FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		ret = -ENOENT;
	else if (!was_set) {
		if (!fence->ops->enable_signaling(fence)) {
			fence_signal_locked(fence);
			ret = -ENOENT;
		}
	}

	if (!ret) {
		cb->func = func;
		list_add_tail(&cb->node, &fence->cb_list);
	} else
		INIT_LIST_HEAD(&cb->node);
	mtx_leave(fence->lock);

	return ret;
}

static inline bool
fence_remove_callback(struct fence *fence, struct fence_cb *cb)
{
	bool ret;

	mtx_enter(fence->lock);

	ret = !list_empty(&cb->node);
	if (ret)
		list_del_init(&cb->node);

	mtx_leave(fence->lock);

	return ret;
}

struct idr_entry {
	SPLAY_ENTRY(idr_entry) entry;
	int id;
	void *ptr;
};

struct idr {
	SPLAY_HEAD(idr_tree, idr_entry) tree;
};

void idr_init(struct idr *);
void idr_preload(unsigned int);
int idr_alloc(struct idr *, void *, int, int, unsigned int);
#define idr_preload_end()
void *idr_find(struct idr *, int);
void *idr_replace(struct idr *, void *ptr, int);
void idr_remove(struct idr *, int);
void idr_destroy(struct idr *);
int idr_for_each(struct idr *, int (*)(int, void *, void *), void *);
void *idr_get_next(struct idr *, int *);

#define idr_for_each_entry(idp, entry, id) \
	for (id = 0; ((entry) = idr_get_next(idp, &(id))) != NULL; id++)


struct ida {
	int counter;
};

void ida_init(struct ida *);
void ida_destroy(struct ida *);
int ida_simple_get(struct ida *, unsigned int, unsigned nt, int);
void ida_remove(struct ida *, int);

struct notifier_block {
	void *notifier_call;
};

#define register_reboot_notifier(x)
#define unregister_reboot_notifier(x)

#define SYS_RESTART 0

#define min_t(t, a, b) ({ \
	t __min_a = (a); \
	t __min_b = (b); \
	__min_a < __min_b ? __min_a : __min_b; })

#define max_t(t, a, b) ({ \
	t __max_a = (a); \
	t __max_b = (b); \
	__max_a > __max_b ? __max_a : __max_b; })

#define clamp_t(t, x, a, b) min_t(t, max_t(t, x, a), b)
#define clamp(x, a, b) clamp_t(__typeof(x), x, a, b)

#define min3(x, y, z) MIN(x, MIN(y, z))

#define do_div(n, base) ({				\
	uint32_t __base = (base);			\
	uint32_t __rem = ((uint64_t)(n)) % __base;	\
	(n) = ((uint64_t)(n)) / __base;			\
	__rem;						\
})

static inline uint64_t
div_u64(uint64_t x, uint32_t y)
{
	return (x / y);
}

static inline int64_t
div_s64(int64_t x, int64_t y)
{
	return (x / y);
}

static inline uint64_t
div64_u64(uint64_t x, uint64_t y)
{
	return (x / y);
}

static inline uint64_t
div64_u64_rem(uint64_t x, uint64_t y, uint64_t *rem)
{
	*rem = x % y;
	return (x / y);
}

static inline int64_t
div64_s64(int64_t x, int64_t y)
{
	return (x / y);
}

#define mult_frac(x, n, d) (((x) * (n)) / (d))
#define order_base_2(x) drm_order(x)

static inline int64_t
abs64(int64_t x)
{
	return (x < 0 ? -x : x);
}

static inline unsigned long
__copy_to_user(void *to, const void *from, unsigned len)
{
	if (copyout(from, to, len))
		return len;
	return 0;
}

static inline unsigned long
copy_to_user(void *to, const void *from, unsigned len)
{
	return __copy_to_user(to, from, len);
}

static inline unsigned long
__copy_from_user(void *to, const void *from, unsigned len)
{
	if (copyin(from, to, len))
		return len;
	return 0;
}

static inline unsigned long
copy_from_user(void *to, const void *from, unsigned len)
{
	return __copy_from_user(to, from, len);
}

#define get_user(x, ptr)	-copyin(ptr, &(x), sizeof(x))
#define put_user(x, ptr)	-copyout(&(x), ptr, sizeof(x))

#define console_lock()
#define console_trylock()	1
#define console_unlock()

#ifndef PCI_MEM_START
#define PCI_MEM_START	0
#endif

#ifndef PCI_MEM_END
#define PCI_MEM_END	0xffffffff
#endif

enum dmi_field {
        DMI_NONE,
        DMI_BIOS_VENDOR,
        DMI_BIOS_VERSION,
        DMI_BIOS_DATE,
        DMI_SYS_VENDOR,
        DMI_PRODUCT_NAME,
        DMI_PRODUCT_VERSION,
        DMI_PRODUCT_SERIAL,
        DMI_PRODUCT_UUID,
        DMI_BOARD_VENDOR,
        DMI_BOARD_NAME,
        DMI_BOARD_VERSION,
        DMI_BOARD_SERIAL,
        DMI_BOARD_ASSET_TAG,
        DMI_CHASSIS_VENDOR,
        DMI_CHASSIS_TYPE,
        DMI_CHASSIS_VERSION,
        DMI_CHASSIS_SERIAL,
        DMI_CHASSIS_ASSET_TAG,
        DMI_STRING_MAX,
};

struct dmi_strmatch {
	unsigned char slot;
	char substr[79];
};

struct dmi_system_id {
        int (*callback)(const struct dmi_system_id *);
        const char *ident;
        struct dmi_strmatch matches[4];
};
#define	DMI_MATCH(a, b) {(a), (b)}
#define	DMI_EXACT_MATCH(a, b) {(a), (b)}
int dmi_check_system(const struct dmi_system_id *);
bool dmi_match(int, const char *);

struct resource {
	u_long	start;
};

struct pci_bus {
	pci_chipset_tag_t pc;
	unsigned char	number;
	pcitag_t	*bridgetag;
};

struct pci_dev {
	struct pci_bus	_bus;
	struct pci_bus	*bus;

	unsigned int	devfn;
	uint16_t	vendor;
	uint16_t	device;
	uint16_t	subsystem_vendor;
	uint16_t	subsystem_device;
	uint8_t		revision;

	pci_chipset_tag_t pc;
	pcitag_t	tag;
	struct pci_softc *pci;

	int		irq;
	int		msi_enabled;
};
#define PCI_ANY_ID (uint16_t) (~0U)

#define PCI_VENDOR_ID_APPLE	PCI_VENDOR_APPLE
#define PCI_VENDOR_ID_ASUSTEK	PCI_VENDOR_ASUSTEK
#define PCI_VENDOR_ID_ATI	PCI_VENDOR_ATI
#define PCI_VENDOR_ID_DELL	PCI_VENDOR_DELL
#define PCI_VENDOR_ID_HP	PCI_VENDOR_HP
#define PCI_VENDOR_ID_IBM	PCI_VENDOR_IBM
#define PCI_VENDOR_ID_INTEL	PCI_VENDOR_INTEL
#define PCI_VENDOR_ID_SONY	PCI_VENDOR_SONY
#define PCI_VENDOR_ID_VIA	PCI_VENDOR_VIATECH

#define PCI_DEVICE_ID_ATI_RADEON_QY	PCI_PRODUCT_ATI_RADEON_QY

#define PCI_DEVFN(slot, func)	((slot) << 3 | (func))
#define PCI_SLOT(devfn)		((devfn) >> 3)
#define PCI_FUNC(devfn)		((devfn) & 0x7)

#define pci_dev_put(x)

#define PCI_EXP_DEVSTA		0x0a
#define PCI_EXP_DEVSTA_TRPND	0x0020
#define PCI_EXP_LNKCAP		0x0c
#define PCI_EXP_LNKCAP_CLKPM	0x00040000
#define PCI_EXP_LNKCTL		0x10
#define PCI_EXP_LNKCTL_HAWD	0x0200
#define PCI_EXP_LNKCTL2		0x30

static inline int
pci_read_config_dword(struct pci_dev *pdev, int reg, u32 *val)
{
	*val = pci_conf_read(pdev->pc, pdev->tag, reg);
	return 0;
} 

static inline int
pci_read_config_word(struct pci_dev *pdev, int reg, u16 *val)
{
	uint32_t v;

	v = pci_conf_read(pdev->pc, pdev->tag, (reg & ~0x2));
	*val = (v >> ((reg & 0x2) * 8));
	return 0;
} 

static inline int
pci_read_config_byte(struct pci_dev *pdev, int reg, u8 *val)
{
	uint32_t v;

	v = pci_conf_read(pdev->pc, pdev->tag, (reg & ~0x3));
	*val = (v >> ((reg & 0x3) * 8));
	return 0;
} 

static inline int
pci_write_config_dword(struct pci_dev *pdev, int reg, u32 val)
{
	pci_conf_write(pdev->pc, pdev->tag, reg, val);
	return 0;
} 

static inline int
pci_write_config_word(struct pci_dev *pdev, int reg, u16 val)
{
	uint32_t v;

	v = pci_conf_read(pdev->pc, pdev->tag, (reg & ~0x2));
	v &= ~(0xffff << ((reg & 0x2) * 8));
	v |= (val << ((reg & 0x2) * 8));
	pci_conf_write(pdev->pc, pdev->tag, (reg & ~0x2), v);
	return 0;
} 

static inline int
pci_write_config_byte(struct pci_dev *pdev, int reg, u8 val)
{
	uint32_t v;

	v = pci_conf_read(pdev->pc, pdev->tag, (reg & ~0x3));
	v &= ~(0xff << ((reg & 0x3) * 8));
	v |= (val << ((reg & 0x3) * 8));
	pci_conf_write(pdev->pc, pdev->tag, (reg & ~0x3), v);
	return 0;
}

static inline int
pci_bus_read_config_word(struct pci_bus *bus, unsigned int devfn,
    int reg, u16 *val)
{
	pcitag_t tag = pci_make_tag(bus->pc, bus->number,
	    PCI_SLOT(devfn), PCI_FUNC(devfn));
	uint32_t v;

	v = pci_conf_read(bus->pc, tag, (reg & ~0x2));
	*val = (v >> ((reg & 0x2) * 8));
	return 0;
}

static inline int
pci_bus_read_config_byte(struct pci_bus *bus, unsigned int devfn,
    int reg, u8 *val)
{
	pcitag_t tag = pci_make_tag(bus->pc, bus->number,
	    PCI_SLOT(devfn), PCI_FUNC(devfn));
	uint32_t v;

	v = pci_conf_read(bus->pc, tag, (reg & ~0x3));
	*val = (v >> ((reg & 0x3) * 8));
	return 0;
}

static inline int
pci_pcie_cap(struct pci_dev *pdev)
{
	int pos;
	if (!pci_get_capability(pdev->pc, pdev->tag, PCI_CAP_PCIEXPRESS,
	    &pos, NULL))
		return -EINVAL;
	return pos;
}

static inline bool
pci_is_root_bus(struct pci_bus *pbus)
{
	return (pbus->bridgetag == NULL);
}

static inline int
pcie_capability_read_dword(struct pci_dev *pdev, int off, u32 *val)
{
	int pos;
	if (!pci_get_capability(pdev->pc, pdev->tag, PCI_CAP_PCIEXPRESS,
	    &pos, NULL)) {
		*val = 0;
		return -EINVAL;
	}
	*val = pci_conf_read(pdev->pc, pdev->tag, pos + off);
	return 0;
}

#define pci_set_master(x)
#define pci_clear_master(x)

#define pci_save_state(x)
#define pci_restore_state(x)

#define pci_enable_msi(x)
#define pci_disable_msi(x)

typedef enum {
	PCI_D0,
	PCI_D1,
	PCI_D2,
	PCI_D3hot,
	PCI_D3cold
} pci_power_t;

#define pci_save_state(x)
#define pci_enable_device(x)	0
#define pci_disable_device(x)
#define pci_set_power_state(d, s)

static inline int
vga_client_register(struct pci_dev *a, void *b, void *c, void *d)
{
	return -ENODEV;
}

#if defined(__amd64__) || defined(__i386__)

#define AGP_USER_MEMORY			0
#define AGP_USER_CACHED_MEMORY		BUS_DMA_COHERENT

#define PCI_DMA_BIDIRECTIONAL	0

static inline dma_addr_t
pci_map_page(struct pci_dev *pdev, struct vm_page *page, unsigned long offset, size_t size, int direction)
{
	return VM_PAGE_TO_PHYS(page);
}

static inline void
pci_unmap_page(struct pci_dev *pdev, dma_addr_t dma_address, size_t size, int direction)
{
}

static inline int
pci_dma_mapping_error(struct pci_dev *pdev, dma_addr_t dma_addr)
{
	return 0;
}

#define dma_set_coherent_mask(x, y)

#define VGA_RSRC_LEGACY_IO	0x01

void vga_get_uninterruptible(struct pci_dev *, int);
void vga_put(struct pci_dev *, int);

#endif

#define vga_switcheroo_register_client(a, b, c)	0
#define vga_switcheroo_unregister_client(a)
#define vga_switcheroo_process_delayed_switch()
#define vga_switcheroo_fini_domain_pm_ops(x)

struct i2c_algorithm;

#define I2C_FUNC_I2C			0
#define I2C_FUNC_SMBUS_EMUL		0
#define I2C_FUNC_SMBUS_READ_BLOCK_DATA	0
#define I2C_FUNC_SMBUS_BLOCK_PROC_CALL	0
#define I2C_FUNC_10BIT_ADDR		0

struct i2c_adapter {
	struct i2c_controller ic;

	char name[48];
	const struct i2c_algorithm *algo;
	void *algo_data;
	int retries;

	void *data;
};

#define I2C_NAME_SIZE	20

struct i2c_msg {
	uint16_t addr;
	uint16_t flags;
	uint16_t len;
	uint8_t *buf;
};

#define I2C_M_RD	0x0001
#define I2C_M_NOSTART	0x0002

struct i2c_algorithm {
	int (*master_xfer)(struct i2c_adapter *, struct i2c_msg *, int);
	u32 (*functionality)(struct i2c_adapter *);
};

extern struct i2c_algorithm i2c_bit_algo;

struct i2c_algo_bit_data {
	struct i2c_controller ic;
};

int i2c_transfer(struct i2c_adapter *, struct i2c_msg *, int);
#define i2c_add_adapter(x) 0
#define i2c_del_adapter(x)

static inline void *
i2c_get_adapdata(struct i2c_adapter *adap)
{
	return adap->data;
}

static inline void
i2c_set_adapdata(struct i2c_adapter *adap, void *data)
{
	adap->data = data;
}

int i2c_bit_add_bus(struct i2c_adapter *);

#define memcpy_toio(d, s, n)	memcpy(d, s, n)
#define memcpy_fromio(d, s, n)	memcpy(d, s, n)
#define memset_io(d, b, n)	memset(d, b, n)

static inline u32
ioread32(const volatile void __iomem *addr)
{
	return (*(volatile uint32_t *)addr);
}

static inline u64
ioread64(const volatile void __iomem *addr)
{
	return (*(volatile uint64_t *)addr);
}

static inline void
iowrite32(u32 val, volatile void __iomem *addr)
{
	*(volatile uint32_t *)addr = val;
}

static inline void
iowrite64(u64 val, volatile void __iomem *addr)
{
	*(volatile uint64_t *)addr = val;
}

#define readl(p) ioread32(p)
#define writel(v, p) iowrite32(v, p)
#define readq(p) ioread64(p)
#define writeq(v, p) iowrite64(v, p)

#define page_to_phys(page)	(VM_PAGE_TO_PHYS(page))
#define page_to_pfn(pp)		(VM_PAGE_TO_PHYS(pp) / PAGE_SIZE)
#define offset_in_page(off)	((off) & PAGE_MASK)
#define set_page_dirty(page)	atomic_clearbits_int(&page->pg_flags, PG_CLEAN)

#define VERIFY_READ	0x1
#define VERIFY_WRITE	0x2
static inline int
access_ok(int type, const void *addr, unsigned long size)
{
	return true;
}

#define CAP_SYS_ADMIN	0x1
static inline int
capable(int cap)
{
	KASSERT(cap == CAP_SYS_ADMIN);
	return suser(curproc);
}

typedef int pgprot_t;
#define pgprot_val(v)	(v)
#define PAGE_KERNEL	0

static inline pgprot_t
pgprot_writecombine(pgprot_t prot)
{
#if PMAP_WC != 0
	return prot | PMAP_WC;
#else
	return prot | PMAP_NOCACHE;
#endif
}

static inline pgprot_t
pgprot_noncached(pgprot_t prot)
{
#if PMAP_DEVICE != 0
	return prot | PMAP_DEVICE;
#else
	return prot | PMAP_NOCACHE;
#endif
}

void	*kmap(struct vm_page *);
void	 kunmap(void *addr);
void	*vmap(struct vm_page **, unsigned int, unsigned long, pgprot_t);
void	 vunmap(void *, size_t);

#define round_up(x, y) ((((x) + ((y) - 1)) / (y)) * (y))
#define round_down(x, y) (((x) / (y)) * (y))
#define roundup2(x, y) (((x)+((y)-1))&(~((y)-1))) /* if y is powers of two */
#define DIV_ROUND_UP(x, y)	(((x) + ((y) - 1)) / (y))
#define DIV_ROUND_UP_ULL(x, y)	DIV_ROUND_UP(x, y)
#define DIV_ROUND_CLOSEST(x, y)	(((x) + ((y) / 2)) / (y))
#define DIV_ROUND_CLOSEST_ULL(x, y)	DIV_ROUND_CLOSEST(x, y)

/*
 * Compute the greatest common divisor of a and b.
 * from libc getopt_long.c
 */
static inline unsigned long
gcd(unsigned long a, unsigned long b)
{
	unsigned long c;

	c = a % b;
	while (c != 0) {
		a = b;
		b = c;
		c = a % b;
	}

	return (b);
}


static inline unsigned long
roundup_pow_of_two(unsigned long x)
{
	return (1UL << flsl(x - 1));
}

#define is_power_of_2(x)	(x != 0 && (((x) - 1) & (x)) == 0)

#define PAGE_ALIGN(addr)	(((addr) + PAGE_MASK) & ~PAGE_MASK)
#define IS_ALIGNED(x, y)	(((x) & ((y) - 1)) == 0)

static __inline void
udelay(unsigned long usecs)
{
	DELAY(usecs);
}

static __inline void
ndelay(unsigned long nsecs)
{
	DELAY(max(nsecs / 1000, 1));
}

static __inline void
usleep_range(unsigned long min, unsigned long max)
{
	DELAY(min);
}

static __inline void
mdelay(unsigned long msecs)
{
	int loops = msecs;
	while (loops--)
		DELAY(1000);
}

static __inline void
cpu_relax(void)
{
	CPU_BUSY_CYCLE();
	if (cold) {
		delay(tick);
		jiffies++;
	}
}

#define cpu_relax_lowlatency() CPU_BUSY_CYCLE()
#define cpu_has_pat	1
#define cpu_has_clflush	1

struct lock_class_key {
};

typedef struct {
	unsigned int sequence;
} seqcount_t;

static inline void
__seqcount_init(seqcount_t *s, const char *name,
    struct lock_class_key *key)
{
	s->sequence = 0;
}

static inline unsigned int
read_seqcount_begin(const seqcount_t *s)
{
	unsigned int r;
	for (;;) {
		r = s->sequence;
		if ((r & 1) == 0)
			break;
		cpu_relax();
	}
	membar_consumer();
	return r;
}

static inline int
read_seqcount_retry(const seqcount_t *s, unsigned start)
{
	membar_consumer();
	return (s->sequence != start);
}

static inline void
write_seqcount_begin(seqcount_t *s)
{
	s->sequence++;
	membar_producer();
}

static inline void
write_seqcount_end(seqcount_t *s)
{
	membar_producer();
	s->sequence++;
}

static inline uint32_t ror32(uint32_t word, unsigned int shift)
{
	return (word >> shift) | (word << (32 - shift));
}

static inline int
irqs_disabled(void)
{
	return (cold);
}

static inline int
in_dbg_master(void)
{
#ifdef DDB
	return (db_is_active);
#endif
	return (0);
}

#define oops_in_progress in_dbg_master()

static inline int
power_supply_is_system_supplied(void)
{
	/* XXX return 0 if on battery */
	return (1);
}

#define pm_qos_update_request(x, y)
#define pm_qos_remove_request(x)
#define pm_runtime_mark_last_busy(x)
#define pm_runtime_use_autosuspend(x)
#define pm_runtime_put_autosuspend(x)
#define pm_runtime_set_autosuspend_delay(x, y)
#define pm_runtime_set_active(x)
#define pm_runtime_allow(x)

static inline int
pm_runtime_get_sync(struct device *dev)
{
	return 0;
}

#define _U      0x01
#define _L      0x02
#define _N      0x04
#define _S      0x08
#define _P      0x10
#define _C      0x20
#define _X      0x40
#define _B      0x80

static inline int
isascii(int c)
{
	return ((unsigned int)c <= 0177);
}

static inline int
isprint(int c)
{
	if (c == -1)
		return (0);
	if ((unsigned char)c >= 040 && (unsigned char)c <= 0176)
		return (1);
	return (0);
}

#ifdef __macppc__
static __inline int
of_machine_is_compatible(const char *model)
{
	extern char *hw_prod;
	return (strcmp(model, hw_prod) == 0);
}
#endif

typedef unsigned int gfp_t;

struct vm_page *alloc_pages(unsigned int, unsigned int);
void	__free_pages(struct vm_page *, unsigned int);

static inline struct vm_page *
alloc_page(unsigned int gfp_mask)
{
	return alloc_pages(gfp_mask, 0);
}

static inline void
__free_page(struct vm_page *page)
{
	return __free_pages(page, 0);
}

static inline unsigned int
get_order(size_t size)
{
	return flsl((size - 1) >> PAGE_SHIFT);
}

#define ilog2(x) ((sizeof(x) <= 4) ? (fls(x) - 1) : (flsl(x) - 1))

#if defined(__i386__) || defined(__amd64__)

#define _PAGE_PRESENT	PG_V
#define _PAGE_RW	PG_RW
#define _PAGE_PAT	PG_PAT
#define _PAGE_PWT	PG_WT
#define _PAGE_PCD	PG_N

static inline void
pagefault_disable(void)
{
	KASSERT(curcpu()->ci_inatomic == 0);
	curcpu()->ci_inatomic = 1;
}

static inline void
pagefault_enable(void)
{
	KASSERT(curcpu()->ci_inatomic == 1);
	curcpu()->ci_inatomic = 0;
}

static inline int
pagefault_disabled(void)
{
	return curcpu()->ci_inatomic;
}

static inline void *
kmap_atomic(struct vm_page *pg)
{
	vaddr_t va;

#if defined (__HAVE_PMAP_DIRECT)
	va = pmap_map_direct(pg);
#else
	extern vaddr_t pmap_tmpmap_pa(paddr_t);
	va = pmap_tmpmap_pa(VM_PAGE_TO_PHYS(pg));
#endif
	return (void *)va;
}

static inline void
kunmap_atomic(void *addr)
{
#if defined (__HAVE_PMAP_DIRECT)
	pmap_unmap_direct((vaddr_t)addr);
#else
	extern void pmap_tmpunmap_pa(void);
	pmap_tmpunmap_pa();
#endif
}

static inline unsigned long
__copy_to_user_inatomic(void *to, const void *from, unsigned len)
{
	struct cpu_info *ci = curcpu();
	int inatomic = ci->ci_inatomic;
	int error;

	ci->ci_inatomic = 1;
	error = copyout(from, to, len);
	ci->ci_inatomic = inatomic;

	return (error ? len : 0);
}

static inline unsigned long
__copy_from_user_inatomic(void *to, const void *from, unsigned len)
{
	struct cpu_info *ci = curcpu();
	int inatomic = ci->ci_inatomic;
	int error;

	ci->ci_inatomic = 1;
	error = copyin(from, to, len);
	ci->ci_inatomic = inatomic;

	return (error ? len : 0);
}

static inline unsigned long
__copy_from_user_inatomic_nocache(void *to, const void *from, unsigned len)
{
	return __copy_from_user_inatomic(to, from, len);
}

#endif

struct fb_var_screeninfo {
	int pixclock;
};

struct fb_info {
	struct fb_var_screeninfo var;
	void *par;
};

#define FB_BLANK_UNBLANK	0
#define FB_BLANK_NORMAL		1
#define FB_BLANK_HSYNC_SUSPEND	2
#define FB_BLANK_VSYNC_SUSPEND	3
#define FB_BLANK_POWERDOWN	4

#define FBINFO_STATE_RUNNING	0
#define FBINFO_STATE_SUSPENDED	1

#define framebuffer_alloc(flags, device) \
	kzalloc(sizeof(struct fb_info), GFP_KERNEL)

struct address_space;
#define unmap_mapping_range(mapping, holebegin, holeend, even_cows)

/*
 * ACPI types and interfaces.
 */

typedef size_t acpi_size;
typedef int acpi_status;

struct acpi_table_header;

#define ACPI_SUCCESS(x) ((x) == 0)

#define AE_NOT_FOUND	0x0005

acpi_status acpi_get_table_with_size(const char *, int, struct acpi_table_header **, acpi_size *);

#define acpi_video_register()
#define acpi_video_unregister()

struct backlight_device;

struct backlight_properties {
	int type;
	int max_brightness;
	int brightness;
	int power;
};

struct backlight_ops {
	int (*update_status)(struct backlight_device *);
	int (*get_brightness)(struct backlight_device *);
};

struct backlight_device {
	const struct backlight_ops *ops;
	struct backlight_properties props;
	struct task task;
	void *data;
};

#define bl_get_data(bd)	(bd)->data

#define BACKLIGHT_RAW		0
#define BACKLIGHT_FIRMWARE	1

struct backlight_device *backlight_device_register(const char *, void *,
     void *, const struct backlight_ops *, struct backlight_properties *);
void backlight_device_unregister(struct backlight_device *);

static inline void
backlight_update_status(struct backlight_device *bd)
{
	bd->ops->update_status(bd);
}

void backlight_schedule_update_status(struct backlight_device *);

#define MIPI_DSI_V_SYNC_START			0x01
#define MIPI_DSI_V_SYNC_END			0x11
#define MIPI_DSI_H_SYNC_START			0x21
#define MIPI_DSI_H_SYNC_END			0x31
#define MIPI_DSI_COLOR_MODE_OFF			0x02
#define MIPI_DSI_COLOR_MODE_ON			0x12
#define MIPI_DSI_SHUTDOWN_PERIPHERAL		0x22
#define MIPI_DSI_TURN_ON_PERIPHERAL		0x32
#define MIPI_DSI_GENERIC_SHORT_WRITE_0_PARAM	0x03
#define MIPI_DSI_GENERIC_SHORT_WRITE_1_PARAM	0x13
#define MIPI_DSI_GENERIC_SHORT_WRITE_2_PARAM	0x23
#define MIPI_DSI_GENERIC_READ_REQUEST_0_PARAM	0x04
#define MIPI_DSI_GENERIC_READ_REQUEST_1_PARAM	0x14
#define MIPI_DSI_GENERIC_READ_REQUEST_2_PARAM	0x24
#define MIPI_DSI_DCS_SHORT_WRITE		0x05
#define MIPI_DSI_DCS_SHORT_WRITE_PARAM		0x15
#define MIPI_DSI_DCS_READ			0x06
#define MIPI_DSI_SET_MAXIMUM_RETURN_PACKET_SIZE	0x37
#define MIPI_DSI_END_OF_TRANSMISSION		0x08
#define MIPI_DSI_NULL_PACKET			0x09
#define MIPI_DSI_BLANKING_PACKET		0x19
#define MIPI_DSI_GENERIC_LONG_WRITE		0x29
#define MIPI_DSI_DCS_LONG_WRITE			0x39
#define MIPI_DSI_LOOSELY_PACKED_PIXEL_STREAM_YCBCR20	0x0c
#define MIPI_DSI_PACKED_PIXEL_STREAM_YCBCR24	0x1c
#define MIPI_DSI_PACKED_PIXEL_STREAM_YCBCR16	0x2c
#define MIPI_DSI_PACKED_PIXEL_STREAM_30		0x0d
#define MIPI_DSI_PACKED_PIXEL_STREAM_36		0x1d
#define MIPI_DSI_PACKED_PIXEL_STREAM_YCBCR12	0x3d
#define MIPI_DSI_PACKED_PIXEL_STREAM_16		0x0e
#define MIPI_DSI_PACKED_PIXEL_STREAM_18		0x1e
#define MIPI_DSI_PIXEL_STREAM_3BYTE_18		0x2e
#define MIPI_DSI_PACKED_PIXEL_STREAM_24		0x3e

#define MIPI_DCS_NOP				0x00
#define MIPI_DCS_SOFT_RESET			0x01
#define MIPI_DCS_GET_POWER_MODE			0x0a
#define MIPI_DCS_GET_PIXEL_FORMAT		0x0c
#define MIPI_DCS_ENTER_SLEEP_MODE		0x10
#define MIPI_DCS_EXIT_SLEEP_MODE		0x11
#define MIPI_DCS_SET_DISPLAY_OFF		0x28
#define MIPI_DCS_SET_DISPLAY_ON			0x29
#define MIPI_DCS_SET_COLUMN_ADDRESS		0x2a
#define MIPI_DCS_SET_PAGE_ADDRESS		0x2b
#define MIPI_DCS_SET_TEAR_OFF			0x34
#define MIPI_DCS_SET_TEAR_ON			0x35
#define MIPI_DCS_SET_PIXEL_FORMAT		0x3a

struct pwm_device;

static inline struct pwm_device *
pwm_get(struct device *dev, const char *consumer)
{
	return ERR_PTR(-ENODEV);
}

static inline void
pwm_put(struct pwm_device *pwm)
{
}

static inline unsigned int
pwm_get_duty_cycle(const struct pwm_device *pwm)
{
	return 0;
}

static inline int
pwm_config(struct pwm_device *pwm, int duty_ns, int period_ns)
{
	return -EINVAL;
}

static inline int
pwm_enable(struct pwm_device *pwm)
{
	return -EINVAL;
}

static inline void
pwm_disable(struct pwm_device *pwm)
{
}

struct scatterlist {
	dma_addr_t dma_address;
	unsigned int offset;
	unsigned int length;
};

struct sg_table {
	struct scatterlist *sgl;
	unsigned int nents;
	unsigned int orig_nents;
};

struct sg_page_iter {
	struct scatterlist *sg;
	unsigned int sg_pgoffset;
	unsigned int __nents;
};

int sg_alloc_table(struct sg_table *, unsigned int, gfp_t);
void sg_free_table(struct sg_table *);

#define sg_mark_end(x)

static __inline void
__sg_page_iter_start(struct sg_page_iter *iter, struct scatterlist *sgl,
    unsigned int nents, unsigned long pgoffset)
{
	iter->sg = sgl;
	iter->sg_pgoffset = pgoffset - 1;
	iter->__nents = nents;
}

static inline bool
__sg_page_iter_next(struct sg_page_iter *iter)
{
	iter->sg_pgoffset++;
	while (iter->__nents > 0 && 
	    iter->sg_pgoffset >= (iter->sg->length / PAGE_SIZE)) {
		iter->sg_pgoffset -= (iter->sg->length / PAGE_SIZE);
		iter->sg++;
		iter->__nents--;
	}

	return (iter->__nents > 0);
}

static inline paddr_t
sg_page_iter_dma_address(struct sg_page_iter *iter)
{
	return iter->sg->dma_address + (iter->sg_pgoffset << PAGE_SHIFT);
}

static inline struct vm_page *
sg_page_iter_page(struct sg_page_iter *iter)
{
	return PHYS_TO_VM_PAGE(sg_page_iter_dma_address(iter));
}

static inline struct vm_page *
sg_page(struct scatterlist *sgl)
{
	return PHYS_TO_VM_PAGE(sgl->dma_address);
}

#define sg_dma_address(sg)	((sg)->dma_address)
#define sg_dma_len(sg)		((sg)->length)

#define for_each_sg_page(sgl, iter, nents, pgoffset) \
  __sg_page_iter_start((iter), (sgl), (nents), (pgoffset)); \
  while (__sg_page_iter_next(iter))

size_t sg_copy_from_buffer(struct scatterlist *, unsigned int,
    const void *, size_t);

struct firmware {
	size_t size;
	const u8 *data;
};

static inline int
request_firmware(const struct firmware **fw, const char *name,
    struct device *device)
{
	int r;
	struct firmware *f = malloc(sizeof(struct firmware), M_DRM,
	    M_WAITOK | M_ZERO);
	*fw = f;
	r = loadfirmware(name, __DECONST(u_char **, &f->data), &f->size);
	if (r != 0)
		return -r;
	else
		return 0;
}

#define request_firmware_nowait(a, b, c, d, e, f, g) -EINVAL

static inline void
release_firmware(const struct firmware *fw)
{
	if (fw)
		free(__DECONST(u_char *, fw->data), M_DEVBUF, fw->size);
	free(__DECONST(struct firmware *, fw), M_DRM, sizeof(*fw));
}

void *memchr_inv(const void *, int, size_t);

struct dma_buf_ops;

struct dma_buf {
	const struct dma_buf_ops *ops;
	void *priv;
	size_t size;
	struct file *file;
};

struct dma_buf_attachment;

void	get_dma_buf(struct dma_buf *);
struct dma_buf *dma_buf_get(int);
void	dma_buf_put(struct dma_buf *);
int	dma_buf_fd(struct dma_buf *, int);

struct dma_buf_ops {
	void (*release)(struct dma_buf *);
};

struct dma_buf_export_info {
	const struct dma_buf_ops *ops;
	size_t size;
	int flags;
	void *priv;
};

#define DEFINE_DMA_BUF_EXPORT_INFO(x)  struct dma_buf_export_info x 

struct dma_buf *dma_buf_export(const struct dma_buf_export_info *);

#define dma_buf_attach(x, y) NULL
#define dma_buf_detach(x, y) panic("dma_buf_detach")

#endif
