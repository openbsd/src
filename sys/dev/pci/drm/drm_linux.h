/*	$OpenBSD: drm_linux.h,v 1.24 2015/04/12 17:10:07 kettenis Exp $	*/
/*
 * Copyright (c) 2013, 2014 Mark Kettenis
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

typedef int irqreturn_t;
#define IRQ_NONE	0
#define IRQ_HANDLED	1

typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;

typedef int32_t s32;
typedef int64_t s64;

typedef uint16_t __le16;
typedef uint16_t __be16;
typedef uint32_t __le32;
typedef uint32_t __be32;

typedef bus_addr_t dma_addr_t;
typedef bus_addr_t phys_addr_t;

#define __force
#define __always_unused
#define __read_mostly
#define __iomem
#define __must_check
#define __init

#if BYTE_ORDER == BIG_ENDIAN
#define __BIG_ENDIAN
#else
#define __LITTLE_ENDIAN
#endif

#define le16_to_cpu(x) letoh16(x)
#define le32_to_cpu(x) letoh32(x)
#define cpu_to_le16(x) htole16(x)
#define cpu_to_le32(x) htole32(x)

#define be32_to_cpup(x) betoh32(*x)

#define lower_32_bits(n)	((u32)(n))
#define upper_32_bits(_val)	((u32)(((_val) >> 16) >> 16))
#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : (1ULL<<(n)) -1)

#define EXPORT_SYMBOL(x)
#define MODULE_FIRMWARE(x)
#define ARRAY_SIZE nitems

#define ERESTARTSYS	EINTR
#define ETIME		ETIMEDOUT
#define EREMOTEIO	EIO
#define EPROTO		EIO
#define ENOTSUPP	ENOTSUP

#define KERN_INFO
#define KERN_WARNING
#define KERN_NOTICE
#define KERN_DEBUG
#define KERN_CRIT
#define KERN_ERR

#define KBUILD_MODNAME "drm"

#ifndef pr_fmt
#define pr_fmt(fmt) fmt
#endif

#define printk(fmt, arg...)	printf(fmt, ## arg)
#define pr_warn(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#define pr_notice(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#define pr_crit(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#define pr_err(fmt, arg...)	printf(pr_fmt(fmt), ## arg)

#ifdef DRMDEBUG
#define pr_info(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#define pr_debug(fmt, arg...)	printf(pr_fmt(fmt), ## arg)
#else
#define pr_info(fmt, arg...)	do { } while(0)
#define pr_debug(fmt, arg...)	do { } while(0)
#endif

#define dev_warn(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *WARNING* " fmt, curproc->p_pid,	\
	    __func__ , ## arg)
#define dev_notice(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *NOTICE* " fmt, curproc->p_pid,	\
	    __func__ , ## arg)
#define dev_crit(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *ERROR* " fmt, curproc->p_pid,	\
	    __func__ , ## arg)
#define dev_err(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *ERROR* " fmt, curproc->p_pid,	\
	    __func__ , ## arg)

#ifdef DRMDEBUG
#define dev_info(dev, fmt, arg...)				\
	printf("drm: " fmt, ## arg)
#define dev_debug(dev, fmt, arg...)				\
	printf("drm:pid%d:%s *DEBUG* " fmt, curproc->p_pid,	\
	    __func__ , ## arg)
#else
#define dev_info(dev, fmt, arg...) 				\
	    do { } while(0)
#define dev_debug(dev, fmt, arg...) 				\
	    do { } while(0)
#endif

#define unlikely(x)	__builtin_expect(!!(x), 0)
#define likely(x)	__builtin_expect(!!(x), 1)

#define BUG()								\
do {									\
	panic("BUG at %s:%d", __FILE__, __LINE__);			\
} while (0)

#define BUG_ON(x) KASSERT(!(x))

#define BUILD_BUG_ON(x) CTASSERT(!(x))

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

#define container_of(ptr, type, member) ({                      \
	__typeof( ((type *)0)->member ) *__mptr = (ptr);        \
	(type *)( (char *)__mptr - offsetof(type,member) );})

#ifndef __DECONST
#define __DECONST(type, var)    ((type)(__uintptr_t)(const void *)(var))
#endif

typedef struct mutex spinlock_t;
#define DEFINE_SPINLOCK(x)	struct mutex x

static inline void
spin_lock_irqsave(struct mutex *mtxp, __unused unsigned long flags)
{
	mtx_enter(mtxp);
}
static inline void
spin_unlock_irqrestore(struct mutex *mtxp, __unused unsigned long flags)
{
	mtx_leave(mtxp);
}
#define spin_lock(mtxp)			mtx_enter(mtxp)
#define spin_unlock(mtxp)		mtx_leave(mtxp)
#define spin_lock_irq(mtxp)		mtx_enter(mtxp)
#define spin_unlock_irq(mtxp)		mtx_leave(mtxp)
#define assert_spin_locked(mtxp)	MUTEX_ASSERT_LOCKED(mtxp)
#define mutex_lock_interruptible(rwl)	-rw_enter(rwl, RW_WRITE | RW_INTR)
#define mutex_lock(rwl)			rw_enter_write(rwl)
#define mutex_unlock(rwl)		rw_exit_write(rwl)
#define mutex_is_locked(rwl)		(rw_status(rwl) == RW_WRITE)
#define down_read(rwl)			rw_enter_read(rwl)
#define up_read(rwl)			rw_exit_read(rwl)
#define down_write(rwl)			rw_enter_write(rwl)
#define up_write(rwl)			rw_exit_write(rwl)
#define read_lock(rwl)			rw_enter_read(rwl)
#define read_unlock(rwl)		rw_exit_read(rwl)
#define write_lock(rwl)			rw_enter_write(rwl)
#define write_unlock(rwl)		rw_exit_write(rwl)

struct wait_queue_head {
	struct mutex lock;
};
typedef struct wait_queue_head wait_queue_head_t;

static inline void
init_waitqueue_head(wait_queue_head_t *wq)
{
	mtx_init(&wq->lock, IPL_NONE);
}

#define wake_up(x)			wakeup(x)
#define wake_up_all(x)			wakeup(x)
#define wake_up_all_locked(x)		wakeup(x)

#define NSEC_PER_USEC	1000L
#define NSEC_PER_SEC	1000000000L
#define KHZ2PICOS(a)	(1000000000UL/(a))

extern struct timespec ns_to_timespec(const int64_t);
extern int64_t timeval_to_ns(const struct timeval *);
extern struct timeval ns_to_timeval(const int64_t);

#define jiffies_to_msecs(x)	(((int64_t)(x)) * 1000 / hz)
#define msecs_to_jiffies(x)	(((int64_t)(x)) * hz / 1000)
#define time_after(a,b)		((long)(b) - (long)(a) < 0)
#define time_after_eq(a,b)	((long)(b) - (long)(a) <= 0)

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

static inline int
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

#define GFP_ATOMIC	M_NOWAIT
#define GFP_KERNEL	(M_WAITOK | M_CANFAIL)
#define __GFP_NOWARN	0
#define __GFP_NORETRY	0

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
kfree(void *objp)
{
	free(objp, M_DRM, 0);
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

#define min_t(t, a, b) ({ \
	t __min_a = (a); \
	t __min_b = (b); \
	__min_a < __min_b ? __min_a : __min_b; })

#define max_t(t, a, b) ({ \
	t __max_a = (a); \
	t __max_b = (b); \
	__max_a > __max_b ? __max_a : __max_b; })

static inline uint64_t
div_u64(uint64_t x, uint32_t y)
{
	return (x / y);
}

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

static __inline uint16_t
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

#define console_lock()
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

struct pci_dev {
	uint16_t	vendor;
	uint16_t	device;
	uint16_t	subsystem_vendor;
	uint16_t	subsystem_device;
};
#define PCI_ANY_ID (uint16_t) (~0U)

#define PCI_VENDOR_ID_ASUSTEK	PCI_VENDOR_ASUSTEK
#define PCI_VENDOR_ID_ATI	PCI_VENDOR_ATI
#define PCI_VENDOR_ID_DELL	PCI_VENDOR_DELL
#define PCI_VENDOR_ID_HP	PCI_VENDOR_HP
#define PCI_VENDOR_ID_IBM	PCI_VENDOR_IBM
#define PCI_VENDOR_ID_INTEL	PCI_VENDOR_INTEL
#define PCI_VENDOR_ID_SONY	PCI_VENDOR_SONY
#define PCI_VENDOR_ID_VIA	PCI_VENDOR_VIATECH

#define PCI_DEVICE_ID_ATI_RADEON_QY	PCI_PRODUCT_ATI_RADEON_QY

#define memcpy_toio(d, s, n)	memcpy(d, s, n)
#define memcpy_fromio(d, s, n)	memcpy(d, s, n)
#define memset_io(d, b, n)	memset(d, b, n)

static inline u32
ioread32(const volatile void __iomem *addr)
{
	u32 r;
	memcpy(&r, (void *)addr, 4);
	return (r);
}

static inline void
iowrite32(u32 val, volatile void __iomem *addr)
{
	memcpy((void *)addr, &val, 4);
}

#define page_to_phys(page)	(VM_PAGE_TO_PHYS(page))
#define page_to_pfn(pp)		(VM_PAGE_TO_PHYS(pp) / PAGE_SIZE)
#define offset_in_page(off)	((off) & PAGE_MASK)

typedef int pgprot_t;
#define pgprot_val(v)	(v)
#define PAGE_KERNEL	0

void	*kmap(struct vm_page *);
void	 kunmap(void *addr);
void	*vmap(struct vm_page **, unsigned int, unsigned long, pgprot_t);
void	 vunmap(void *, size_t);

#define round_up(x, y) ((((x) + ((y) - 1)) / (y)) * (y))
#define round_down(x, y) (((x) / (y)) * (y))
#define roundup2(x, y) (((x)+((y)-1))&(~((y)-1))) /* if y is powers of two */
#define DIV_ROUND_UP(x, y)	(((x) + ((y) - 1)) / (y))
#define DIV_ROUND_CLOSEST(x, y)	(((x) + ((y) / 2)) / (y))

static inline unsigned long
roundup_pow_of_two(unsigned long x)
{
	return (1UL << flsl(x - 1));
}

#define PAGE_ALIGN(addr)	(((addr) + PAGE_MASK) & ~PAGE_MASK)
#define IS_ALIGNED(x, y)	(((x) & ((y) - 1)) == 0)

static __inline void
udelay(unsigned long usecs)
{
	DELAY(usecs);
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

#ifdef __macppc__
static __inline int
of_machine_is_compatible(const char *model)
{
	extern char *hw_prod;
	return (strcmp(model, hw_prod) == 0);
}
#endif

#if defined(__i386__) || defined(__amd64__)

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
in_atomic(void)
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
