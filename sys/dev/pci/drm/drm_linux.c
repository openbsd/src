/*	$OpenBSD: drm_linux.c,v 1.58 2020/03/15 10:14:49 claudio Exp $	*/
/*
 * Copyright (c) 2013 Jonathan Gray <jsg@openbsd.org>
 * Copyright (c) 2015, 2016 Mark Kettenis <kettenis@openbsd.org>
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

#include <drm/drmP.h>
#include <dev/pci/ppbreg.h>
#include <sys/event.h>
#include <sys/filedesc.h>
#include <sys/kthread.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <linux/dma-buf.h>
#include <linux/mod_devicetable.h>
#include <linux/acpi.h>
#include <linux/pagevec.h>
#include <linux/dma-fence-array.h>

#if defined(__amd64__) || defined(__i386__)
#include "bios.h"
#endif

void
tasklet_run(void *arg)
{
	struct tasklet_struct *ts = arg;

	clear_bit(TASKLET_STATE_SCHED, &ts->state);
	if (tasklet_trylock(ts)) {
		if (!atomic_read(&ts->count))
			ts->func(ts->data);
		tasklet_unlock(ts);
	}
}

struct mutex sch_mtx = MUTEX_INITIALIZER(IPL_SCHED);
volatile struct proc *sch_proc;
volatile void *sch_ident;
int sch_priority;

void
set_current_state(int state)
{
	if (sch_ident != curproc)
		mtx_enter(&sch_mtx);
	MUTEX_ASSERT_LOCKED(&sch_mtx);
	sch_ident = sch_proc = curproc;
	sch_priority = state;
}

void
__set_current_state(int state)
{
	KASSERT(state == TASK_RUNNING);
	if (sch_ident == curproc) {
		MUTEX_ASSERT_LOCKED(&sch_mtx);
		sch_ident = NULL;
		mtx_leave(&sch_mtx);
	}
}

void
schedule(void)
{
	schedule_timeout(MAX_SCHEDULE_TIMEOUT);
}

long
schedule_timeout(long timeout)
{
	struct sleep_state sls;
	long deadline;
	int wait, spl;

	MUTEX_ASSERT_LOCKED(&sch_mtx);
	KASSERT(!cold);

	sleep_setup(&sls, sch_ident, sch_priority, "schto");
	if (timeout != MAX_SCHEDULE_TIMEOUT)
		sleep_setup_timeout(&sls, timeout);

	wait = (sch_proc == curproc && timeout > 0);

	spl = MUTEX_OLDIPL(&sch_mtx);
	MUTEX_OLDIPL(&sch_mtx) = splsched();
	mtx_leave(&sch_mtx);

	sleep_setup_signal(&sls);

	if (timeout != MAX_SCHEDULE_TIMEOUT)
		deadline = ticks + timeout;
	sleep_finish_all(&sls, wait);
	if (timeout != MAX_SCHEDULE_TIMEOUT)
		timeout = deadline - ticks;

	mtx_enter(&sch_mtx);
	MUTEX_OLDIPL(&sch_mtx) = spl;
	sch_ident = curproc;

	return timeout > 0 ? timeout : 0;
}

int
wake_up_process(struct proc *p)
{
	atomic_cas_ptr(&sch_proc, p, NULL);
	return wakeup_proc(p, NULL);
}

void
flush_workqueue(struct workqueue_struct *wq)
{
	if (cold)
		return;

	taskq_barrier((struct taskq *)wq);
}

bool
flush_work(struct work_struct *work)
{
	if (cold)
		return false;

	taskq_barrier(work->tq);
	return false;
}

bool
flush_delayed_work(struct delayed_work *dwork)
{
	bool ret = false;

	if (cold)
		return false;

	while (timeout_pending(&dwork->to)) {
		tsleep(dwork, PWAIT, "fldwto", 1);
		ret = true;
	}

	taskq_barrier(dwork->tq ? dwork->tq : (struct taskq *)system_wq);
	return ret;
}

struct kthread {
	int (*func)(void *);
	void *data;
	struct proc *proc;
	volatile u_int flags;
#define KTHREAD_SHOULDSTOP	0x0000001
#define KTHREAD_STOPPED		0x0000002
#define KTHREAD_SHOULDPARK	0x0000004
#define KTHREAD_PARKED		0x0000008
	LIST_ENTRY(kthread) next;
};

LIST_HEAD(, kthread) kthread_list = LIST_HEAD_INITIALIZER(kthread_list);

void
kthread_func(void *arg)
{
	struct kthread *thread = arg;
	int ret;

	ret = thread->func(thread->data);
	thread->flags |= KTHREAD_STOPPED;
	kthread_exit(ret);
}

struct proc *
kthread_run(int (*func)(void *), void *data, const char *name)
{
	struct kthread *thread;

	thread = malloc(sizeof(*thread), M_DRM, M_WAITOK);
	thread->func = func;
	thread->data = data;
	thread->flags = 0;
	
	if (kthread_create(kthread_func, thread, &thread->proc, name)) {
		free(thread, M_DRM, sizeof(*thread));
		return ERR_PTR(-ENOMEM);
	}

	LIST_INSERT_HEAD(&kthread_list, thread, next);
	return thread->proc;
}

struct kthread *
kthread_lookup(struct proc *p)
{
	struct kthread *thread;

	LIST_FOREACH(thread, &kthread_list, next) {
		if (thread->proc == p)
			break;
	}
	KASSERT(thread);

	return thread;
}

int
kthread_should_park(void)
{
	struct kthread *thread = kthread_lookup(curproc);
	return (thread->flags & KTHREAD_SHOULDPARK);
}

void
kthread_parkme(void)
{
	struct kthread *thread = kthread_lookup(curproc);

	while (thread->flags & KTHREAD_SHOULDPARK) {
		thread->flags |= KTHREAD_PARKED;
		wakeup(thread);
		tsleep_nsec(thread, PPAUSE, "parkme", INFSLP);
		thread->flags &= ~KTHREAD_PARKED;
	}
}

void
kthread_park(struct proc *p)
{
	struct kthread *thread = kthread_lookup(p);

	while ((thread->flags & KTHREAD_PARKED) == 0) {
		thread->flags |= KTHREAD_SHOULDPARK;
		wake_up_process(thread->proc);
		tsleep_nsec(thread, PPAUSE, "park", INFSLP);
	}
}

void
kthread_unpark(struct proc *p)
{
	struct kthread *thread = kthread_lookup(p);

	thread->flags &= ~KTHREAD_SHOULDPARK;
	wakeup(thread);
}

int
kthread_should_stop(void)
{
	struct kthread *thread = kthread_lookup(curproc);
	return (thread->flags & KTHREAD_SHOULDSTOP);
}

void
kthread_stop(struct proc *p)
{
	struct kthread *thread = kthread_lookup(p);

	while ((thread->flags & KTHREAD_STOPPED) == 0) {
		thread->flags |= KTHREAD_SHOULDSTOP;
		wake_up_process(thread->proc);
		tsleep_nsec(thread, PPAUSE, "stop", INFSLP);
	}
	LIST_REMOVE(thread, next);
	free(thread, M_DRM, sizeof(*thread));
}

struct timespec
ns_to_timespec(const int64_t nsec)
{
	struct timespec ts;
	int32_t rem;

	if (nsec == 0) {
		ts.tv_sec = 0;
		ts.tv_nsec = 0;
		return (ts);
	}

	ts.tv_sec = nsec / NSEC_PER_SEC;
	rem = nsec % NSEC_PER_SEC;
	if (rem < 0) {
		ts.tv_sec--;
		rem += NSEC_PER_SEC;
	}
	ts.tv_nsec = rem;
	return (ts);
}

int64_t
timeval_to_ns(const struct timeval *tv)
{
	return ((int64_t)tv->tv_sec * NSEC_PER_SEC) +
		tv->tv_usec * NSEC_PER_USEC;
}

struct timeval
ns_to_timeval(const int64_t nsec)
{
	struct timeval tv;
	int32_t rem;

	if (nsec == 0) {
		tv.tv_sec = 0;
		tv.tv_usec = 0;
		return (tv);
	}

	tv.tv_sec = nsec / NSEC_PER_SEC;
	rem = nsec % NSEC_PER_SEC;
	if (rem < 0) {
		tv.tv_sec--;
		rem += NSEC_PER_SEC;
	}
	tv.tv_usec = rem / 1000;
	return (tv);
}

int64_t
timeval_to_ms(const struct timeval *tv)
{
	return ((int64_t)tv->tv_sec * 1000) + (tv->tv_usec / 1000);
}

int64_t
timeval_to_us(const struct timeval *tv)
{
	return ((int64_t)tv->tv_sec * 1000000) + tv->tv_usec;
}

extern char *hw_vendor, *hw_prod, *hw_ver;

#if NBIOS > 0
extern char smbios_board_vendor[];
extern char smbios_board_prod[];
extern char smbios_board_serial[];
#endif

bool
dmi_match(int slot, const char *str)
{
	switch (slot) {
	case DMI_SYS_VENDOR:
		if (hw_vendor != NULL &&
		    !strcmp(hw_vendor, str))
			return true;
		break;
	case DMI_PRODUCT_NAME:
		if (hw_prod != NULL &&
		    !strcmp(hw_prod, str))
			return true;
		break;
	case DMI_PRODUCT_VERSION:
		if (hw_ver != NULL &&
		    !strcmp(hw_ver, str))
			return true;
		break;
#if NBIOS > 0
	case DMI_BOARD_VENDOR:
		if (strcmp(smbios_board_vendor, str) == 0)
			return true;
		break;
	case DMI_BOARD_NAME:
		if (strcmp(smbios_board_prod, str) == 0)
			return true;
		break;
	case DMI_BOARD_SERIAL:
		if (strcmp(smbios_board_serial, str) == 0)
			return true;
		break;
#else
	case DMI_BOARD_VENDOR:
		if (hw_vendor != NULL &&
		    !strcmp(hw_vendor, str))
			return true;
		break;
	case DMI_BOARD_NAME:
		if (hw_prod != NULL &&
		    !strcmp(hw_prod, str))
			return true;
		break;
#endif
	case DMI_NONE:
	default:
		return false;
	}

	return false;
}

static bool
dmi_found(const struct dmi_system_id *dsi)
{
	int i, slot;

	for (i = 0; i < nitems(dsi->matches); i++) {
		slot = dsi->matches[i].slot;
		if (slot == DMI_NONE)
			break;
		if (!dmi_match(slot, dsi->matches[i].substr))
			return false;
	}

	return true;
}

const struct dmi_system_id *
dmi_first_match(const struct dmi_system_id *sysid)
{
	const struct dmi_system_id *dsi;

	for (dsi = sysid; dsi->matches[0].slot != 0 ; dsi++) {
		if (dmi_found(dsi))
			return dsi;
	}

	return NULL;
}

#if NBIOS > 0
extern char smbios_bios_date[];
#endif

const char *
dmi_get_system_info(int slot)
{
	WARN_ON(slot != DMI_BIOS_DATE);
#if NBIOS > 0
	if (slot == DMI_BIOS_DATE)
		return smbios_bios_date;
#endif
	return NULL;
}

int
dmi_check_system(const struct dmi_system_id *sysid)
{
	const struct dmi_system_id *dsi;
	int num = 0;

	for (dsi = sysid; dsi->matches[0].slot != 0 ; dsi++) {
		if (dmi_found(dsi)) {
			num++;
			if (dsi->callback && dsi->callback(dsi))
				break;
		}
	}
	return (num);
}

struct vm_page *
alloc_pages(unsigned int gfp_mask, unsigned int order)
{
	int flags = (gfp_mask & M_NOWAIT) ? UVM_PLA_NOWAIT : UVM_PLA_WAITOK;
	struct uvm_constraint_range *constraint = &no_constraint;
	struct pglist mlist;

	if (gfp_mask & M_CANFAIL)
		flags |= UVM_PLA_FAILOK;
	if (gfp_mask & M_ZERO)
		flags |= UVM_PLA_ZERO;
	if (gfp_mask & __GFP_DMA32)
		constraint = &dma_constraint;

	TAILQ_INIT(&mlist);
	if (uvm_pglistalloc(PAGE_SIZE << order, constraint->ucr_low,
	    constraint->ucr_high, PAGE_SIZE, 0, &mlist, 1, flags))
		return NULL;
	return TAILQ_FIRST(&mlist);
}

void
__free_pages(struct vm_page *page, unsigned int order)
{
	struct pglist mlist;
	int i;
	
	TAILQ_INIT(&mlist);
	for (i = 0; i < (1 << order); i++)
		TAILQ_INSERT_TAIL(&mlist, &page[i], pageq);
	uvm_pglistfree(&mlist);
}

void
__pagevec_release(struct pagevec *pvec)
{
	struct pglist mlist;
	int i;

	TAILQ_INIT(&mlist);
	for (i = 0; i < pvec->nr; i++)
		TAILQ_INSERT_TAIL(&mlist, pvec->pages[i], pageq);
	uvm_pglistfree(&mlist);
	pagevec_reinit(pvec);
}

void *
kmap(struct vm_page *pg)
{
	vaddr_t va;

#if defined (__HAVE_PMAP_DIRECT)
	va = pmap_map_direct(pg);
#else
	va = uvm_km_valloc_wait(phys_map, PAGE_SIZE);
	pmap_kenter_pa(va, VM_PAGE_TO_PHYS(pg), PROT_READ | PROT_WRITE);
	pmap_update(pmap_kernel());
#endif
	return (void *)va;
}

void
kunmap(void *addr)
{
	vaddr_t va = (vaddr_t)addr;

#if defined (__HAVE_PMAP_DIRECT)
	pmap_unmap_direct(va);
#else
	pmap_kremove(va, PAGE_SIZE);
	pmap_update(pmap_kernel());
	uvm_km_free_wakeup(phys_map, va, PAGE_SIZE);
#endif
}

void *
vmap(struct vm_page **pages, unsigned int npages, unsigned long flags,
     pgprot_t prot)
{
	vaddr_t va;
	paddr_t pa;
	int i;

	va = uvm_km_valloc(kernel_map, PAGE_SIZE * npages);
	if (va == 0)
		return NULL;
	for (i = 0; i < npages; i++) {
		pa = VM_PAGE_TO_PHYS(pages[i]) | prot;
		pmap_enter(pmap_kernel(), va + (i * PAGE_SIZE), pa,
		    PROT_READ | PROT_WRITE,
		    PROT_READ | PROT_WRITE | PMAP_WIRED);
		pmap_update(pmap_kernel());
	}

	return (void *)va;
}

void
vunmap(void *addr, size_t size)
{
	vaddr_t va = (vaddr_t)addr;

	pmap_remove(pmap_kernel(), va, va + size);
	pmap_update(pmap_kernel());
	uvm_km_free(kernel_map, va, size);
}

void
print_hex_dump(const char *level, const char *prefix_str, int prefix_type,
    int rowsize, int groupsize, const void *buf, size_t len, bool ascii)
{
	const uint8_t *cbuf = buf;
	int i;

	for (i = 0; i < len; i++) {
		if ((i % rowsize) == 0)
			printf("%s", prefix_str);
		printf("%02x", cbuf[i]);
		if ((i % rowsize) == (rowsize - 1))
			printf("\n");
		else
			printf(" ");
	}
}

void *
memchr_inv(const void *s, int c, size_t n)
{
	if (n != 0) {
		const unsigned char *p = s;

		do {
			if (*p++ != (unsigned char)c)
				return ((void *)(p - 1));
		}while (--n != 0);
	}
	return (NULL);
}

int
panic_cmp(struct rb_node *a, struct rb_node *b)
{
	panic(__func__);
}

#undef RB_ROOT
#define RB_ROOT(head)	(head)->rbh_root

RB_GENERATE(linux_root, rb_node, __entry, panic_cmp);

/*
 * This is a fairly minimal implementation of the Linux "idr" API.  It
 * probably isn't very efficient, and defenitely isn't RCU safe.  The
 * pre-load buffer is global instead of per-cpu; we rely on the kernel
 * lock to make this work.  We do randomize our IDs in order to make
 * them harder to guess.
 */

int idr_cmp(struct idr_entry *, struct idr_entry *);
SPLAY_PROTOTYPE(idr_tree, idr_entry, entry, idr_cmp);

struct pool idr_pool;
struct idr_entry *idr_entry_cache;

void
idr_init(struct idr *idr)
{
	static int initialized;

	if (!initialized) {
		pool_init(&idr_pool, sizeof(struct idr_entry), 0, IPL_TTY, 0,
		    "idrpl", NULL);
		initialized = 1;
	}
	SPLAY_INIT(&idr->tree);
}

void
idr_destroy(struct idr *idr)
{
	struct idr_entry *id;

	while ((id = SPLAY_MIN(idr_tree, &idr->tree))) {
		SPLAY_REMOVE(idr_tree, &idr->tree, id);
		pool_put(&idr_pool, id);
	}
}

void
idr_preload(unsigned int gfp_mask)
{
	int flags = (gfp_mask & GFP_NOWAIT) ? PR_NOWAIT : PR_WAITOK;

	KERNEL_ASSERT_LOCKED();

	if (idr_entry_cache == NULL)
		idr_entry_cache = pool_get(&idr_pool, flags);
}

int
idr_alloc(struct idr *idr, void *ptr, int start, int end,
    unsigned int gfp_mask)
{
	int flags = (gfp_mask & GFP_NOWAIT) ? PR_NOWAIT : PR_WAITOK;
	struct idr_entry *id;
	int begin;

	KERNEL_ASSERT_LOCKED();

	if (idr_entry_cache) {
		id = idr_entry_cache;
		idr_entry_cache = NULL;
	} else {
		id = pool_get(&idr_pool, flags);
		if (id == NULL)
			return -ENOMEM;
	}

	if (end <= 0)
		end = INT_MAX;

#ifdef notyet
	id->id = begin = start + arc4random_uniform(end - start);
#else
	id->id = begin = start;
#endif
	while (SPLAY_INSERT(idr_tree, &idr->tree, id)) {
		if (++id->id == end)
			id->id = start;
		if (id->id == begin) {
			pool_put(&idr_pool, id);
			return -ENOSPC;
		}
	}
	id->ptr = ptr;
	return id->id;
}

void *
idr_replace(struct idr *idr, void *ptr, int id)
{
	struct idr_entry find, *res;
	void *old;

	find.id = id;
	res = SPLAY_FIND(idr_tree, &idr->tree, &find);
	if (res == NULL)
		return ERR_PTR(-ENOENT);
	old = res->ptr;
	res->ptr = ptr;
	return old;
}

void *
idr_remove(struct idr *idr, int id)
{
	struct idr_entry find, *res;
	void *ptr = NULL;

	find.id = id;
	res = SPLAY_FIND(idr_tree, &idr->tree, &find);
	if (res) {
		SPLAY_REMOVE(idr_tree, &idr->tree, res);
		ptr = res->ptr;
		pool_put(&idr_pool, res);
	}
	return ptr;
}

void *
idr_find(struct idr *idr, int id)
{
	struct idr_entry find, *res;

	find.id = id;
	res = SPLAY_FIND(idr_tree, &idr->tree, &find);
	if (res == NULL)
		return NULL;
	return res->ptr;
}

void *
idr_get_next(struct idr *idr, int *id)
{
	struct idr_entry *res;

	SPLAY_FOREACH(res, idr_tree, &idr->tree) {
		if (res->id >= *id) {
			*id = res->id;
			return res->ptr;
		}
	}

	return NULL;
}

int
idr_for_each(struct idr *idr, int (*func)(int, void *, void *), void *data)
{
	struct idr_entry *id;
	int ret;

	SPLAY_FOREACH(id, idr_tree, &idr->tree) {
		ret = func(id->id, id->ptr, data);
		if (ret)
			return ret;
	}

	return 0;
}

int
idr_cmp(struct idr_entry *a, struct idr_entry *b)
{
	return (a->id < b->id ? -1 : a->id > b->id);
}

SPLAY_GENERATE(idr_tree, idr_entry, entry, idr_cmp);

void
ida_init(struct ida *ida)
{
	ida->counter = 0;
}

void
ida_destroy(struct ida *ida)
{
}

void
ida_remove(struct ida *ida, int id)
{
}

int
ida_simple_get(struct ida *ida, unsigned int start, unsigned int end,
    int flags)
{
	if (end <= 0)
		end = INT_MAX;

	if (start > ida->counter)
		ida->counter = start;

	if (ida->counter >= end)
		return -ENOSPC;

	return ida->counter++;
}

void
ida_simple_remove(struct ida *ida, int id)
{
}

int
sg_alloc_table(struct sg_table *table, unsigned int nents, gfp_t gfp_mask)
{
	table->sgl = mallocarray(nents, sizeof(struct scatterlist),
	    M_DRM, gfp_mask);
	if (table->sgl == NULL)
		return -ENOMEM;
	table->nents = table->orig_nents = nents;
	return 0;
}

void
sg_free_table(struct sg_table *table)
{
	free(table->sgl, M_DRM,
	    table->orig_nents * sizeof(struct scatterlist));
}

size_t
sg_copy_from_buffer(struct scatterlist *sgl, unsigned int nents,
    const void *buf, size_t buflen)
{
	panic("%s", __func__);
}

int
i2c_master_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	void *cmd = NULL;
	int cmdlen = 0;
	int err, ret = 0;
	int op;

	iic_acquire_bus(&adap->ic, 0);

	while (num > 2) {
		op = (msgs->flags & I2C_M_RD) ? I2C_OP_READ : I2C_OP_WRITE;
		err = iic_exec(&adap->ic, op, msgs->addr, NULL, 0,
		    msgs->buf, msgs->len, 0);
		if (err) {
			ret = -err;
			goto fail;
		}
		msgs++;
		num--;
		ret++;
	}

	if (num > 1) {
		cmd = msgs->buf;
		cmdlen = msgs->len;
		msgs++;
		num--;
		ret++;
	}

	op = (msgs->flags & I2C_M_RD) ?
	    I2C_OP_READ_WITH_STOP : I2C_OP_WRITE_WITH_STOP;
	err = iic_exec(&adap->ic, op, msgs->addr, cmd, cmdlen,
	    msgs->buf, msgs->len, 0);
	if (err) {
		ret = -err;
		goto fail;
	}
	msgs++;
	ret++;

fail:
	iic_release_bus(&adap->ic, 0);

	return ret;
}

int
i2c_transfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	if (adap->algo)
		return adap->algo->master_xfer(adap, msgs, num);

	return i2c_master_xfer(adap, msgs, num);
}

int
i2c_bb_master_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	struct i2c_algo_bit_data *algo = adap->algo_data;
	struct i2c_adapter bb;

	memset(&bb, 0, sizeof(bb));
	bb.ic = algo->ic;
	bb.retries = adap->retries;
	return i2c_master_xfer(&bb, msgs, num);
}

uint32_t
i2c_bb_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

struct i2c_algorithm i2c_bit_algo = {
	.master_xfer = i2c_bb_master_xfer,
	.functionality = i2c_bb_functionality
};

int
i2c_bit_add_bus(struct i2c_adapter *adap)
{
	adap->algo = &i2c_bit_algo;
	adap->retries = 3;

	return 0;
}

#if defined(__amd64__) || defined(__i386__)

/*
 * This is a minimal implementation of the Linux vga_get/vga_put
 * interface.  In all likelyhood, it will only work for inteldrm(4) as
 * it assumes that if there is another active VGA device in the
 * system, it is sitting behind a PCI bridge.
 */

extern int pci_enumerate_bus(struct pci_softc *,
    int (*)(struct pci_attach_args *), struct pci_attach_args *);

pcitag_t vga_bridge_tag;
int vga_bridge_disabled;

int
vga_disable_bridge(struct pci_attach_args *pa)
{
	pcireg_t bhlc, bc;

	if (pa->pa_domain != 0)
		return 0;

	bhlc = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_BHLC_REG);
	if (PCI_HDRTYPE_TYPE(bhlc) != 1)
		return 0;

	bc = pci_conf_read(pa->pa_pc, pa->pa_tag, PPB_REG_BRIDGECONTROL);
	if ((bc & PPB_BC_VGA_ENABLE) == 0)
		return 0;
	bc &= ~PPB_BC_VGA_ENABLE;
	pci_conf_write(pa->pa_pc, pa->pa_tag, PPB_REG_BRIDGECONTROL, bc);

	vga_bridge_tag = pa->pa_tag;
	vga_bridge_disabled = 1;

	return 1;
}

void
vga_get_uninterruptible(struct pci_dev *pdev, int rsrc)
{
	KASSERT(pdev->pci->sc_bridgetag == NULL);
	pci_enumerate_bus(pdev->pci, vga_disable_bridge, NULL);
}

void
vga_put(struct pci_dev *pdev, int rsrc)
{
	pcireg_t bc;

	if (!vga_bridge_disabled)
		return;

	bc = pci_conf_read(pdev->pc, vga_bridge_tag, PPB_REG_BRIDGECONTROL);
	bc |= PPB_BC_VGA_ENABLE;
	pci_conf_write(pdev->pc, vga_bridge_tag, PPB_REG_BRIDGECONTROL, bc);

	vga_bridge_disabled = 0;
}

#endif

/*
 * ACPI types and interfaces.
 */

#ifdef __HAVE_ACPI
#include "acpi.h"
#endif

#if NACPI > 0

#include <dev/acpi/acpireg.h>
#include <dev/acpi/acpivar.h>
#include <dev/acpi/amltypes.h>
#include <dev/acpi/dsdt.h>

acpi_status
acpi_get_table(const char *sig, int instance,
    struct acpi_table_header **hdr)
{
	struct acpi_softc *sc = acpi_softc;
	struct acpi_q *entry;

	KASSERT(instance == 1);

	if (sc == NULL)
		return AE_NOT_FOUND;

	SIMPLEQ_FOREACH(entry, &sc->sc_tables, q_next) {
		if (memcmp(entry->q_table, sig, strlen(sig)) == 0) {
			*hdr = entry->q_table;
			return 0;
		}
	}

	return AE_NOT_FOUND;
}

acpi_status
acpi_get_handle(acpi_handle node, const char *name, acpi_handle *rnode)
{
	node = aml_searchname(node, name);
	if (node == NULL)
		return AE_NOT_FOUND;

	*rnode = node;
	return 0;
}

acpi_status
acpi_get_name(acpi_handle node, int type,  struct acpi_buffer *buffer)
{
	KASSERT(buffer->length != ACPI_ALLOCATE_BUFFER);
	KASSERT(type == ACPI_FULL_PATHNAME);
	strlcpy(buffer->pointer, aml_nodename(node), buffer->length);
	return 0;
}

acpi_status
acpi_evaluate_object(acpi_handle node, const char *name,
    struct acpi_object_list *params, struct acpi_buffer *result)
{
	struct aml_value args[4], res;
	union acpi_object *obj;
	uint8_t *data;
	int i;

	KASSERT(params->count <= nitems(args));

	for (i = 0; i < params->count; i++) {
		args[i].type = params->pointer[i].type;
		switch (args[i].type) {
		case AML_OBJTYPE_INTEGER:
			args[i].v_integer = params->pointer[i].integer.value;
			break;
		case AML_OBJTYPE_BUFFER:
			args[i].length = params->pointer[i].buffer.length;
			args[i].v_buffer = params->pointer[i].buffer.pointer;
			break;
		default:
			printf("%s: arg type 0x%02x", __func__, args[i].type);
			return AE_BAD_PARAMETER;
		}
	}

	if (name) {
		node = aml_searchname(node, name);
		if (node == NULL)
			return AE_NOT_FOUND;
	}
	if (aml_evalnode(acpi_softc, node, params->count, args, &res)) {
		aml_freevalue(&res);
		return AE_ERROR;
	}

	KASSERT(result->length == ACPI_ALLOCATE_BUFFER);

	result->length = sizeof(union acpi_object);
	switch (res.type) {
	case AML_OBJTYPE_BUFFER:
		result->length += res.length;
		result->pointer = malloc(result->length, M_DRM, M_WAITOK);
		obj = (union acpi_object *)result->pointer;
		data = (uint8_t *)(obj + 1);
		obj->type = res.type;
		obj->buffer.length = res.length;
		obj->buffer.pointer = data;
		memcpy(data, res.v_buffer, res.length);
		break;
	default:
		printf("%s: return type 0x%02x", __func__, res.type);
		aml_freevalue(&res);
		return AE_ERROR;
	}

	aml_freevalue(&res);
	return 0;
}

SLIST_HEAD(, notifier_block) drm_linux_acpi_notify_list =
	SLIST_HEAD_INITIALIZER(drm_linux_acpi_notify_list);

int
drm_linux_acpi_notify(struct aml_node *node, int notify, void *arg)
{
	struct acpi_bus_event event;
	struct notifier_block *nb;

	event.device_class = ACPI_VIDEO_CLASS;
	event.type = notify;

	SLIST_FOREACH(nb, &drm_linux_acpi_notify_list, link)
		nb->notifier_call(nb, 0, &event);
	return 0;
}

int
register_acpi_notifier(struct notifier_block *nb)
{
	SLIST_INSERT_HEAD(&drm_linux_acpi_notify_list, nb, link);
	return 0;
}

int
unregister_acpi_notifier(struct notifier_block *nb)
{
	struct notifier_block *tmp;

	SLIST_FOREACH(tmp, &drm_linux_acpi_notify_list, link) {
		if (tmp == nb) {
			SLIST_REMOVE(&drm_linux_acpi_notify_list, nb,
			    notifier_block, link);
			return 0;
		}
	}

	return -ENOENT;
}

const char *
acpi_format_exception(acpi_status status)
{
	switch (status) {
	case AE_NOT_FOUND:
		return "not found";
	case AE_BAD_PARAMETER:
		return "bad parameter";
	default:
		return "unknown";
	}
}

#endif

void
backlight_do_update_status(void *arg)
{
	backlight_update_status(arg);
}

struct backlight_device *
backlight_device_register(const char *name, void *kdev, void *data,
    const struct backlight_ops *ops, struct backlight_properties *props)
{
	struct backlight_device *bd;

	bd = malloc(sizeof(*bd), M_DRM, M_WAITOK);
	bd->ops = ops;
	bd->props = *props;
	bd->data = data;

	task_set(&bd->task, backlight_do_update_status, bd);
	
	return bd;
}

void
backlight_device_unregister(struct backlight_device *bd)
{
	free(bd, M_DRM, sizeof(*bd));
}

void
backlight_schedule_update_status(struct backlight_device *bd)
{
	task_add(systq, &bd->task);
}

void
drm_sysfs_hotplug_event(struct drm_device *dev)
{
	KNOTE(&dev->note, NOTE_CHANGE);
}

unsigned int drm_fence_count;

unsigned int
dma_fence_context_alloc(unsigned int num)
{
	return __sync_add_and_fetch(&drm_fence_count, num) - num;
}

struct default_wait_cb {
	struct dma_fence_cb base;
	struct proc *proc;
};

static void
dma_fence_default_wait_cb(struct dma_fence *fence, struct dma_fence_cb *cb)
{
	struct default_wait_cb *wait =
	    container_of(cb, struct default_wait_cb, base);
	wake_up_process(wait->proc);
}

long
dma_fence_default_wait(struct dma_fence *fence, bool intr, signed long timeout)
{
	long ret = timeout ? timeout : 1;
	int err;
	struct default_wait_cb cb;
	bool was_set;

	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		return ret;

	mtx_enter(fence->lock);

	was_set = test_and_set_bit(DMA_FENCE_FLAG_ENABLE_SIGNAL_BIT,
	    &fence->flags);

	if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
		goto out;

	if (!was_set && fence->ops->enable_signaling) {
		if (!fence->ops->enable_signaling(fence)) {
			dma_fence_signal_locked(fence);
			goto out;
		}
	}

	if (timeout == 0) {
		ret = 0;
		goto out;
	}

	cb.base.func = dma_fence_default_wait_cb;
	cb.proc = curproc;
	list_add(&cb.base.node, &fence->cb_list);

	while (!test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags)) {
		err = msleep(curproc, fence->lock, intr ? PCATCH : 0, "dmafence",
		    timeout);
		if (err == EINTR || err == ERESTART) {
			ret = -ERESTARTSYS;
			break;
		} else if (err == EWOULDBLOCK) {
			ret = 0;
			break;
		}
	}

	if (!list_empty(&cb.base.node))
		list_del(&cb.base.node);
out:
	mtx_leave(fence->lock);
	
	return ret;
}

static bool
dma_fence_test_signaled_any(struct dma_fence **fences, uint32_t count,
    uint32_t *idx)
{
	int i;

	for (i = 0; i < count; ++i) {
		struct dma_fence *fence = fences[i];
		if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags)) {
			if (idx)
				*idx = i;
			return true;
		}
	}
	return false;
}

long
dma_fence_wait_any_timeout(struct dma_fence **fences, uint32_t count,
    bool intr, long timeout, uint32_t *idx)
{
	struct default_wait_cb *cb;
	int i, err;
	int ret = timeout;

	if (timeout == 0) {
		for (i = 0; i < count; i++) {
			if (dma_fence_is_signaled(fences[i])) {
				if (idx)
					*idx = i;
				return 1;
			}
		}
		return 0;
	}

	cb = mallocarray(count, sizeof(*cb), M_DRM, M_WAITOK|M_CANFAIL|M_ZERO);
	if (cb == NULL)
		return -ENOMEM;
	
	for (i = 0; i < count; i++) {
		struct dma_fence *fence = fences[i];
		cb[i].proc = curproc;
		if (dma_fence_add_callback(fence, &cb[i].base,
		    dma_fence_default_wait_cb)) {
			if (idx)
				*idx = i;
			goto cb_cleanup;
		}
	}

	while (ret > 0) {
		if (dma_fence_test_signaled_any(fences, count, idx))
			break;

		err = tsleep(curproc, intr ? PCATCH : 0,
		    "dfwat", timeout);
		if (err == EINTR || err == ERESTART) {
			ret = -ERESTARTSYS;
			break;
		} else if (err == EWOULDBLOCK) {
			ret = 0;
			break;
		}
	}

cb_cleanup:
	while (i-- > 0)
		dma_fence_remove_callback(fences[i], &cb[i].base);
	free(cb, M_DRM, count * sizeof(*cb));
	return ret;
}

static const char *
dma_fence_array_get_driver_name(struct dma_fence *fence)
{
	return "dma_fence_array";
}

static const char *
dma_fence_array_get_timeline_name(struct dma_fence *fence)
{
	return "unbound";
}

static void
irq_dma_fence_array_work(struct irq_work *wrk)
{
	struct dma_fence_array *dfa = container_of(wrk, typeof(*dfa), work);

	dma_fence_signal(&dfa->base);
	dma_fence_put(&dfa->base);
}

static void
dma_fence_array_cb_func(struct dma_fence *f, struct dma_fence_cb *cb)
{
	struct dma_fence_array_cb *array_cb =
	    container_of(cb, struct dma_fence_array_cb, cb);
	struct dma_fence_array *dfa = array_cb->array;
	
	if (atomic_dec_and_test(&dfa->num_pending))
		irq_work_queue(&dfa->work);
	else
		dma_fence_put(&dfa->base);
}

static bool
dma_fence_array_enable_signaling(struct dma_fence *fence)
{
	struct dma_fence_array *dfa = to_dma_fence_array(fence);
	struct dma_fence_array_cb *cb = (void *)(&dfa[1]);
	int i;

	for (i = 0; i < dfa->num_fences; ++i) {
		cb[i].array = dfa;
		dma_fence_get(&dfa->base);
		if (dma_fence_add_callback(dfa->fences[i], &cb[i].cb,
		    dma_fence_array_cb_func)) {
			dma_fence_put(&dfa->base);
			if (atomic_dec_and_test(&dfa->num_pending))
				return false;
		}
	}
	
	return true;
}

static bool dma_fence_array_signaled(struct dma_fence *fence)
{
	struct dma_fence_array *dfa = to_dma_fence_array(fence);

	return atomic_read(&dfa->num_pending) <= 0;
}

static void dma_fence_array_release(struct dma_fence *fence)
{
	struct dma_fence_array *dfa = to_dma_fence_array(fence);
	int i;

	for (i = 0; i < dfa->num_fences; ++i)
		dma_fence_put(dfa->fences[i]);

	free(dfa->fences, M_DRM, 0);
	dma_fence_free(fence);
}

struct dma_fence_array *
dma_fence_array_create(int num_fences, struct dma_fence **fences, u64 context,
    unsigned seqno, bool signal_on_any)
{
	struct dma_fence_array *dfa = malloc(sizeof(*dfa) +
	    (num_fences * sizeof(struct dma_fence_array_cb)),
	    M_DRM, M_WAITOK|M_CANFAIL|M_ZERO);
	if (dfa == NULL)
		return NULL;

	mtx_init(&dfa->lock, IPL_TTY);
	dma_fence_init(&dfa->base, &dma_fence_array_ops, &dfa->lock,
	    context, seqno);
	init_irq_work(&dfa->work, irq_dma_fence_array_work);

	dfa->num_fences = num_fences;
	atomic_set(&dfa->num_pending, signal_on_any ? 1 : num_fences);
	dfa->fences = fences;

	return dfa;
}

const struct dma_fence_ops dma_fence_array_ops = {
	.get_driver_name = dma_fence_array_get_driver_name,
	.get_timeline_name = dma_fence_array_get_timeline_name,
	.enable_signaling = dma_fence_array_enable_signaling,
	.signaled = dma_fence_array_signaled,
	.release = dma_fence_array_release,
};

int
dmabuf_read(struct file *fp, struct uio *uio, int fflags)
{
	return (ENXIO);
}

int
dmabuf_write(struct file *fp, struct uio *uio, int fflags)
{
	return (ENXIO);
}

int
dmabuf_ioctl(struct file *fp, u_long com, caddr_t data, struct proc *p)
{
	return (ENOTTY);
}

int
dmabuf_poll(struct file *fp, int events, struct proc *p)
{
	return (0);
}

int
dmabuf_kqfilter(struct file *fp, struct knote *kn)
{
	return (EINVAL);
}

int
dmabuf_stat(struct file *fp, struct stat *st, struct proc *p)
{
	struct dma_buf *dmabuf = fp->f_data;

	memset(st, 0, sizeof(*st));
	st->st_size = dmabuf->size;
	st->st_mode = S_IFIFO;	/* XXX */
	return (0);
}

int
dmabuf_close(struct file *fp, struct proc *p)
{
	struct dma_buf *dmabuf = fp->f_data;

	fp->f_data = NULL;
	KERNEL_LOCK();
	dmabuf->ops->release(dmabuf);
	KERNEL_UNLOCK();
	free(dmabuf, M_DRM, sizeof(struct dma_buf));
	return (0);
}

int
dmabuf_seek(struct file *fp, off_t *offset, int whence, struct proc *p)
{
	struct dma_buf *dmabuf = fp->f_data;
	off_t newoff;

	if (*offset != 0)
		return (EINVAL);

	switch (whence) {
	case SEEK_SET:
		newoff = 0;
		break;
	case SEEK_END:
		newoff = dmabuf->size;
		break;
	default:
		return (EINVAL);
	}
	mtx_enter(&fp->f_mtx);
	fp->f_offset = newoff;
	mtx_leave(&fp->f_mtx);
	*offset = newoff;
	return (0);
}

const struct fileops dmabufops = {
	.fo_read	= dmabuf_read,
	.fo_write	= dmabuf_write,
	.fo_ioctl	= dmabuf_ioctl,
	.fo_poll	= dmabuf_poll,
	.fo_kqfilter	= dmabuf_kqfilter,
	.fo_stat	= dmabuf_stat,
	.fo_close	= dmabuf_close,
	.fo_seek	= dmabuf_seek,
};

struct dma_buf *
dma_buf_export(const struct dma_buf_export_info *info)
{
	struct proc *p = curproc;
	struct dma_buf *dmabuf;
	struct file *fp;

	fp = fnew(p);
	if (fp == NULL)
		return ERR_PTR(-ENFILE);
	fp->f_type = DTYPE_DMABUF;
	fp->f_ops = &dmabufops;
	dmabuf = malloc(sizeof(struct dma_buf), M_DRM, M_WAITOK | M_ZERO);
	dmabuf->priv = info->priv;
	dmabuf->ops = info->ops;
	dmabuf->size = info->size;
	dmabuf->file = fp;
	fp->f_data = dmabuf;
	return dmabuf;
}

struct dma_buf *
dma_buf_get(int fd)
{
	struct proc *p = curproc;
	struct filedesc *fdp = p->p_fd;
	struct file *fp;

	if ((fp = fd_getfile(fdp, fd)) == NULL)
		return ERR_PTR(-EBADF);

	if (fp->f_type != DTYPE_DMABUF) {
		FRELE(fp, p);
		return ERR_PTR(-EINVAL);
	}

	return fp->f_data;
}

void
dma_buf_put(struct dma_buf *dmabuf)
{
	KASSERT(dmabuf);
	KASSERT(dmabuf->file);

	FRELE(dmabuf->file, curproc);
}

int
dma_buf_fd(struct dma_buf *dmabuf, int flags)
{
	struct proc *p = curproc;
	struct filedesc *fdp = p->p_fd;
	struct file *fp = dmabuf->file;
	int fd, cloexec, error;

	cloexec = (flags & O_CLOEXEC) ? UF_EXCLOSE : 0;

	fdplock(fdp);
restart:
	if ((error = fdalloc(p, 0, &fd)) != 0) {
		if (error == ENOSPC) {
			fdexpand(p);
			goto restart;
		}
		fdpunlock(fdp);
		return -error;
	}

	fdinsert(fdp, fd, cloexec, fp);
	fdpunlock(fdp);

	return fd;
}

void
get_dma_buf(struct dma_buf *dmabuf)
{
	FREF(dmabuf->file);
}

enum pci_bus_speed
pcie_get_speed_cap(struct pci_dev *pdev)
{
	pci_chipset_tag_t	pc = pdev->pc;
	pcitag_t		tag = pdev->tag;
	int			pos ;
	pcireg_t		xcap, lnkcap = 0, lnkcap2 = 0;
	pcireg_t		id;
	enum pci_bus_speed	cap = PCI_SPEED_UNKNOWN;
	int			bus, device, function;

	if (!pci_get_capability(pc, tag, PCI_CAP_PCIEXPRESS,
	    &pos, NULL)) 
		return PCI_SPEED_UNKNOWN;

	id = pci_conf_read(pc, tag, PCI_ID_REG);
	pci_decompose_tag(pc, tag, &bus, &device, &function);

	/* we've been informed via and serverworks don't make the cut */
	if (PCI_VENDOR(id) == PCI_VENDOR_VIATECH ||
	    PCI_VENDOR(id) == PCI_VENDOR_RCC)
		return PCI_SPEED_UNKNOWN;

	lnkcap = pci_conf_read(pc, tag, pos + PCI_PCIE_LCAP);
	xcap = pci_conf_read(pc, tag, pos + PCI_PCIE_XCAP);
	if (PCI_PCIE_XCAP_VER(xcap) >= 2)
		lnkcap2 = pci_conf_read(pc, tag, pos + PCI_PCIE_LCAP2);

	lnkcap &= 0x0f;
	lnkcap2 &= 0xfe;

	if (lnkcap2) { /* PCIE GEN 3.0 */
		if (lnkcap2 & 0x02)
			cap = PCIE_SPEED_2_5GT;
		if (lnkcap2 & 0x04)
			cap = PCIE_SPEED_5_0GT;
		if (lnkcap2 & 0x08)
			cap = PCIE_SPEED_8_0GT;
		if (lnkcap2 & 0x10)
			cap = PCIE_SPEED_16_0GT;
	} else {
		if (lnkcap & 0x01)
			cap = PCIE_SPEED_2_5GT;
		if (lnkcap & 0x02)
			cap = PCIE_SPEED_5_0GT;
	}

	DRM_INFO("probing pcie caps for device %d:%d:%d 0x%04x:0x%04x = %x/%x\n",
	    bus, device, function, PCI_VENDOR(id), PCI_PRODUCT(id), lnkcap,
	    lnkcap2);
	return cap;
}

enum pcie_link_width
pcie_get_width_cap(struct pci_dev *pdev)
{
	pci_chipset_tag_t	pc = pdev->pc;
	pcitag_t		tag = pdev->tag;
	int			pos ;
	pcireg_t		lnkcap = 0;
	pcireg_t		id;
	int			bus, device, function;

	if (!pci_get_capability(pc, tag, PCI_CAP_PCIEXPRESS,
	    &pos, NULL)) 
		return PCIE_LNK_WIDTH_UNKNOWN;

	id = pci_conf_read(pc, tag, PCI_ID_REG);
	pci_decompose_tag(pc, tag, &bus, &device, &function);

	lnkcap = pci_conf_read(pc, tag, pos + PCI_PCIE_LCAP);

	DRM_INFO("probing pcie width for device %d:%d:%d 0x%04x:0x%04x = %x\n",
	    bus, device, function, PCI_VENDOR(id), PCI_PRODUCT(id), lnkcap);

	if (lnkcap)
		return (lnkcap & 0x3f0) >> 4;
	return PCIE_LNK_WIDTH_UNKNOWN;
}

int
default_wake_function(struct wait_queue_entry *wqe, unsigned int mode,
    int sync, void *key)
{
	wakeup(wqe);
	if (wqe->proc)
		wake_up_process(wqe->proc);
	return 0;
}

int
autoremove_wake_function(struct wait_queue_entry *wqe, unsigned int mode,
    int sync, void *key)
{
	default_wake_function(wqe, mode, sync, key);
	list_del_init(&wqe->entry);
	return 0;
}

struct mutex wait_bit_mtx = MUTEX_INITIALIZER(IPL_TTY);

int
wait_on_bit(unsigned long *word, int bit, unsigned mode)
{
	int err;

	if (!test_bit(bit, word))
		return 0;

	mtx_enter(&wait_bit_mtx);
	while (test_bit(bit, word)) {
		err = msleep_nsec(word, &wait_bit_mtx, PWAIT | mode, "wtb",
		    INFSLP);
		if (err) {
			mtx_leave(&wait_bit_mtx);
			return 1;
		}
	}
	mtx_leave(&wait_bit_mtx);
	return 0;
}

int
wait_on_bit_timeout(unsigned long *word, int bit, unsigned mode, int timo)
{
	int err;

	if (!test_bit(bit, word))
		return 0;

	mtx_enter(&wait_bit_mtx);
	while (test_bit(bit, word)) {
		err = msleep(word, &wait_bit_mtx, PWAIT | mode, "wtb", timo);
		if (err) {
			mtx_leave(&wait_bit_mtx);
			return 1;
		}
	}
	mtx_leave(&wait_bit_mtx);
	return 0;
}

void
wake_up_bit(void *word, int bit)
{
	mtx_enter(&wait_bit_mtx);
	wakeup(word);
	mtx_leave(&wait_bit_mtx);
}

struct workqueue_struct *system_wq;
struct workqueue_struct *system_unbound_wq;
struct workqueue_struct *system_long_wq;
struct taskq *taskletq;

void
drm_linux_init(void)
{
	if (system_wq == NULL) {
		system_wq = (struct workqueue_struct *)
		    taskq_create("drmwq", 4, IPL_HIGH, 0);
	}
	if (system_unbound_wq == NULL) {
		system_unbound_wq = (struct workqueue_struct *)
		    taskq_create("drmubwq", 4, IPL_HIGH, 0);
	}
	if (system_long_wq == NULL) {
		system_long_wq = (struct workqueue_struct *)
		    taskq_create("drmlwq", 4, IPL_HIGH, 0);
	}

	if (taskletq == NULL)
		taskletq = taskq_create("drmtskl", 1, IPL_HIGH, 0);
}

#define PCIE_ECAP_RESIZE_BAR	0x15
#define RBCAP0			0x04
#define RBCTRL0			0x08
#define RBCTRL_BARINDEX_MASK	0x07
#define RBCTRL_BARSIZE_MASK	0x1f00
#define RBCTRL_BARSIZE_SHIFT	8

/* size in MB is 1 << nsize */
int
pci_resize_resource(struct pci_dev *pdev, int bar, int nsize)
{
	pcireg_t	reg;
	uint32_t	offset, capid;

	KASSERT(bar == 0);

	offset = PCI_PCIE_ECAP;

	/* search PCI Express Extended Capabilities */
	do {
		reg = pci_conf_read(pdev->pc, pdev->tag, offset);
		capid = PCI_PCIE_ECAP_ID(reg);
		if (capid == PCIE_ECAP_RESIZE_BAR)
			break;
		offset = PCI_PCIE_ECAP_NEXT(reg);
	} while (capid != 0);

	if (capid == 0) {
		printf("%s: could not find resize bar cap!\n", __func__);
		return -ENOTSUP;
	}

	reg = pci_conf_read(pdev->pc, pdev->tag, offset + RBCAP0);

	if ((reg & (1 << (nsize + 4))) == 0) {
		printf("%s size not supported\n", __func__);
		return -ENOTSUP;
	}

	reg = pci_conf_read(pdev->pc, pdev->tag, offset + RBCTRL0);
	if ((reg & RBCTRL_BARINDEX_MASK) != 0) {
		printf("%s BAR index not 0\n", __func__);
		return -EINVAL;
	}

	reg &= ~RBCTRL_BARSIZE_MASK;
	reg |= (nsize << RBCTRL_BARSIZE_SHIFT) & RBCTRL_BARSIZE_MASK;

	pci_conf_write(pdev->pc, pdev->tag, offset + RBCTRL0, reg);

	return 0;
}

TAILQ_HEAD(, shrinker) shrinkers = TAILQ_HEAD_INITIALIZER(shrinkers);

int
register_shrinker(struct shrinker *shrinker)
{
	TAILQ_INSERT_TAIL(&shrinkers, shrinker, next);
	return 0;
}

void
unregister_shrinker(struct shrinker *shrinker)
{
	TAILQ_REMOVE(&shrinkers, shrinker, next);
}

void
drmbackoff(long npages)
{
	struct shrink_control sc;
	struct shrinker *shrinker;
	u_long ret;

	shrinker = TAILQ_FIRST(&shrinkers);
	while (shrinker && npages > 0) {
		sc.nr_to_scan = npages;
		ret = shrinker->scan_objects(shrinker, &sc);
		npages -= ret;
		shrinker = TAILQ_NEXT(shrinker, next);
	}
}
