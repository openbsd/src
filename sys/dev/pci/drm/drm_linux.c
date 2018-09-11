/*	$OpenBSD: drm_linux.c,v 1.32 2018/09/11 20:25:58 kettenis Exp $	*/
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

#include <dev/pci/drm/drmP.h>
#include <dev/pci/ppbreg.h>
#include <sys/event.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/stat.h>
#include <sys/unistd.h>

struct mutex sch_mtx = MUTEX_INITIALIZER(IPL_SCHED);
void *sch_ident;
int sch_priority;

void
flush_barrier(void *arg)
{
	int *barrier = arg;

	*barrier = 1;
	wakeup(barrier);
}

void
flush_workqueue(struct workqueue_struct *wq)
{
	struct sleep_state sls;
	struct task task;
	int barrier = 0;

	if (cold)
		return;

	task_set(&task, flush_barrier, &barrier);
	task_add((struct taskq *)wq, &task);
	while (!barrier) {
		sleep_setup(&sls, &barrier, PWAIT, "flwqbar");
		sleep_finish(&sls, !barrier);
	}
}

void
flush_work(struct work_struct *work)
{
	struct sleep_state sls;
	struct task task;
	int barrier = 0;

	if (cold)
		return;

	task_set(&task, flush_barrier, &barrier);
	task_add(work->tq, &task);
	while (!barrier) {
		sleep_setup(&sls, &barrier, PWAIT, "flwkbar");
		sleep_finish(&sls, !barrier);
	}
}

void
flush_delayed_work(struct delayed_work *dwork)
{
	struct sleep_state sls;
	struct task task;
	int barrier = 0;

	if (cold)
		return;

	while (timeout_pending(&dwork->to))
		tsleep(&barrier, PWAIT, "fldwto", 1);

	task_set(&task, flush_barrier, &barrier);
	task_add(dwork->tq ? dwork->tq : systq, &task);
	while (!barrier) {
		sleep_setup(&sls, &barrier, PWAIT, "fldwbar");
		sleep_finish(&sls, !barrier);
	}
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
timeval_to_us(const struct timeval *tv)
{
	return ((int64_t)tv->tv_sec * 1000000) + tv->tv_usec;
}

extern char *hw_vendor, *hw_prod, *hw_ver;

bool
dmi_match(int slot, const char *str)
{
	switch (slot) {
	case DMI_SYS_VENDOR:
	case DMI_BOARD_VENDOR:
		if (hw_vendor != NULL &&
		    !strcmp(hw_vendor, str))
			return true;
		break;
	case DMI_PRODUCT_NAME:
	case DMI_BOARD_NAME:
		if (hw_prod != NULL &&
		    !strcmp(hw_prod, str))
			return true;
		break;
	case DMI_PRODUCT_VERSION:
	case DMI_BOARD_VERSION:
		if (hw_ver != NULL &&
		    !strcmp(hw_ver, str))
			return true;
		break;
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
	struct pglist mlist;

	if (gfp_mask & M_CANFAIL)
		flags |= UVM_PLA_FAILOK;
	if (gfp_mask & M_ZERO)
		flags |= UVM_PLA_ZERO;

	TAILQ_INIT(&mlist);
	if (uvm_pglistalloc(PAGE_SIZE << order, dma_constraint.ucr_low,
	    dma_constraint.ucr_high, PAGE_SIZE, 0, &mlist, 1, flags))
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

void
idr_remove(struct idr *idr, int id)
{
	struct idr_entry find, *res;

	find.id = id;
	res = SPLAY_FIND(idr_tree, &idr->tree, &find);
	if (res) {
		SPLAY_REMOVE(idr_tree, &idr->tree, res);
		pool_put(&idr_pool, res);
	}
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

	res = idr_find(idr, *id);
	if (res == NULL)
		res = SPLAY_MIN(idr_tree, &idr->tree);
	else
		res = SPLAY_NEXT(idr_tree, &idr->tree, res);
	if (res == NULL)
		return NULL;
	*id = res->id;
	return res->ptr;
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

acpi_status
acpi_get_table_with_size(const char *sig, int instance,
    struct acpi_table_header **hdr, acpi_size *size)
{
	struct acpi_softc *sc = acpi_softc;
	struct acpi_q *entry;

	KASSERT(instance == 1);

	if (sc == NULL)
		return AE_NOT_FOUND;

	SIMPLEQ_FOREACH(entry, &sc->sc_tables, q_next) {
		if (memcmp(entry->q_table, sig, strlen(sig)) == 0) {
			*hdr = entry->q_table;
			*size = (*hdr)->length;
			return 0;
		}
	}

	return AE_NOT_FOUND;
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
fence_context_alloc(unsigned int num)
{
	return __sync_add_and_fetch(&drm_fence_count, num) - num;
}

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
	fp->f_offset = *offset = newoff;
	return (0);
}

struct fileops dmabufops = {
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
