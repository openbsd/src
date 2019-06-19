/*
 * Copyright © 2012-2014 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include <drm/drmP.h>
#include <drm/i915_drm.h>
#include "i915_drv.h"
#include "i915_trace.h"
#include "intel_drv.h"
#include <linux/mmu_context.h>
#include <linux/mmu_notifier.h>
#include <linux/mempolicy.h>
#include <linux/swap.h>
#include <linux/sched/mm.h>

#ifdef __linux__

struct i915_mm_struct {
	struct mm_struct *mm;
	struct drm_i915_private *i915;
	struct i915_mmu_notifier *mn;
	struct hlist_node node;
	struct kref kref;
	struct work_struct work;
};

#if defined(CONFIG_MMU_NOTIFIER)
#include <linux/interval_tree.h>

struct i915_mmu_notifier {
	spinlock_t lock;
	struct hlist_node node;
	struct mmu_notifier mn;
	struct rb_root_cached objects;
	struct workqueue_struct *wq;
};

struct i915_mmu_object {
	struct i915_mmu_notifier *mn;
	struct drm_i915_gem_object *obj;
	struct interval_tree_node it;
	struct list_head link;
	struct work_struct work;
	bool attached;
};

static void cancel_userptr(struct work_struct *work)
{
	struct i915_mmu_object *mo = container_of(work, typeof(*mo), work);
	struct drm_i915_gem_object *obj = mo->obj;
	struct work_struct *active;

	/* Cancel any active worker and force us to re-evaluate gup */
	mutex_lock(&obj->mm.lock);
	active = fetch_and_zero(&obj->userptr.work);
	mutex_unlock(&obj->mm.lock);
	if (active)
		goto out;

	i915_gem_object_wait(obj, I915_WAIT_ALL, MAX_SCHEDULE_TIMEOUT, NULL);

	mutex_lock(&obj->base.dev->struct_mutex);

	/* We are inside a kthread context and can't be interrupted */
	if (i915_gem_object_unbind(obj) == 0)
		__i915_gem_object_put_pages(obj, I915_MM_NORMAL);
	WARN_ONCE(i915_gem_object_has_pages(obj),
		  "Failed to release pages: bind_count=%d, pages_pin_count=%d, pin_global=%d\n",
		  obj->bind_count,
		  atomic_read(&obj->mm.pages_pin_count),
		  obj->pin_global);

	mutex_unlock(&obj->base.dev->struct_mutex);

out:
	i915_gem_object_put(obj);
}

static void add_object(struct i915_mmu_object *mo)
{
	if (mo->attached)
		return;

	interval_tree_insert(&mo->it, &mo->mn->objects);
	mo->attached = true;
}

static void del_object(struct i915_mmu_object *mo)
{
	if (!mo->attached)
		return;

	interval_tree_remove(&mo->it, &mo->mn->objects);
	mo->attached = false;
}

static int i915_gem_userptr_mn_invalidate_range_start(struct mmu_notifier *_mn,
						       struct mm_struct *mm,
						       unsigned long start,
						       unsigned long end,
						       bool blockable)
{
	struct i915_mmu_notifier *mn =
		container_of(_mn, struct i915_mmu_notifier, mn);
	struct i915_mmu_object *mo;
	struct interval_tree_node *it;
	LIST_HEAD(cancelled);

	if (RB_EMPTY_ROOT(&mn->objects.rb_root))
		return 0;

	/* interval ranges are inclusive, but invalidate range is exclusive */
	end--;

	spin_lock(&mn->lock);
	it = interval_tree_iter_first(&mn->objects, start, end);
	while (it) {
		if (!blockable) {
			spin_unlock(&mn->lock);
			return -EAGAIN;
		}
		/* The mmu_object is released late when destroying the
		 * GEM object so it is entirely possible to gain a
		 * reference on an object in the process of being freed
		 * since our serialisation is via the spinlock and not
		 * the struct_mutex - and consequently use it after it
		 * is freed and then double free it. To prevent that
		 * use-after-free we only acquire a reference on the
		 * object if it is not in the process of being destroyed.
		 */
		mo = container_of(it, struct i915_mmu_object, it);
		if (kref_get_unless_zero(&mo->obj->base.refcount))
			queue_work(mn->wq, &mo->work);

		list_add(&mo->link, &cancelled);
		it = interval_tree_iter_next(it, start, end);
	}
	list_for_each_entry(mo, &cancelled, link)
		del_object(mo);
	spin_unlock(&mn->lock);

	if (!list_empty(&cancelled))
		flush_workqueue(mn->wq);

	return 0;
}

static const struct mmu_notifier_ops i915_gem_userptr_notifier = {
	.invalidate_range_start = i915_gem_userptr_mn_invalidate_range_start,
};

static struct i915_mmu_notifier *
i915_mmu_notifier_create(struct mm_struct *mm)
{
	struct i915_mmu_notifier *mn;

	mn = kmalloc(sizeof(*mn), GFP_KERNEL);
	if (mn == NULL)
		return ERR_PTR(-ENOMEM);

	mtx_init(&mn->lock);
	mn->mn.ops = &i915_gem_userptr_notifier;
	mn->objects = RB_ROOT_CACHED;
	mn->wq = alloc_workqueue("i915-userptr-release",
				 WQ_UNBOUND | WQ_MEM_RECLAIM,
				 0);
	if (mn->wq == NULL) {
		kfree(mn);
		return ERR_PTR(-ENOMEM);
	}

	return mn;
}

static void
i915_gem_userptr_release__mmu_notifier(struct drm_i915_gem_object *obj)
{
	struct i915_mmu_object *mo;

	mo = obj->userptr.mmu_object;
	if (mo == NULL)
		return;

	spin_lock(&mo->mn->lock);
	del_object(mo);
	spin_unlock(&mo->mn->lock);
	kfree(mo);

	obj->userptr.mmu_object = NULL;
}

static struct i915_mmu_notifier *
i915_mmu_notifier_find(struct i915_mm_struct *mm)
{
	struct i915_mmu_notifier *mn;
	int err = 0;

	mn = mm->mn;
	if (mn)
		return mn;

	mn = i915_mmu_notifier_create(mm->mm);
	if (IS_ERR(mn))
		err = PTR_ERR(mn);

	down_write(&mm->mm->mmap_sem);
	mutex_lock(&mm->i915->mm_lock);
	if (mm->mn == NULL && !err) {
		/* Protected by mmap_sem (write-lock) */
		err = __mmu_notifier_register(&mn->mn, mm->mm);
		if (!err) {
			/* Protected by mm_lock */
			mm->mn = fetch_and_zero(&mn);
		}
	} else if (mm->mn) {
		/*
		 * Someone else raced and successfully installed the mmu
		 * notifier, we can cancel our own errors.
		 */
		err = 0;
	}
	mutex_unlock(&mm->i915->mm_lock);
	up_write(&mm->mm->mmap_sem);

	if (mn && !IS_ERR(mn)) {
		destroy_workqueue(mn->wq);
		kfree(mn);
	}

	return err ? ERR_PTR(err) : mm->mn;
}

static int
i915_gem_userptr_init__mmu_notifier(struct drm_i915_gem_object *obj,
				    unsigned flags)
{
	struct i915_mmu_notifier *mn;
	struct i915_mmu_object *mo;

	if (flags & I915_USERPTR_UNSYNCHRONIZED)
		return capable(CAP_SYS_ADMIN) ? 0 : -EPERM;

	if (WARN_ON(obj->userptr.mm == NULL))
		return -EINVAL;

	mn = i915_mmu_notifier_find(obj->userptr.mm);
	if (IS_ERR(mn))
		return PTR_ERR(mn);

	mo = kzalloc(sizeof(*mo), GFP_KERNEL);
	if (mo == NULL)
		return -ENOMEM;

	mo->mn = mn;
	mo->obj = obj;
	mo->it.start = obj->userptr.ptr;
	mo->it.last = obj->userptr.ptr + obj->base.size - 1;
	INIT_WORK(&mo->work, cancel_userptr);

	obj->userptr.mmu_object = mo;
	return 0;
}

static void
i915_mmu_notifier_free(struct i915_mmu_notifier *mn,
		       struct mm_struct *mm)
{
	if (mn == NULL)
		return;

	mmu_notifier_unregister(&mn->mn, mm);
	destroy_workqueue(mn->wq);
	kfree(mn);
}

#else

static void
i915_gem_userptr_release__mmu_notifier(struct drm_i915_gem_object *obj)
{
}

static int
i915_gem_userptr_init__mmu_notifier(struct drm_i915_gem_object *obj,
				    unsigned flags)
{
	if ((flags & I915_USERPTR_UNSYNCHRONIZED) == 0)
		return -ENODEV;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	return 0;
}

static void
i915_mmu_notifier_free(struct i915_mmu_notifier *mn,
		       struct mm_struct *mm)
{
}

#endif

static struct i915_mm_struct *
__i915_mm_struct_find(struct drm_i915_private *dev_priv, struct mm_struct *real)
{
	struct i915_mm_struct *mm;

	/* Protected by dev_priv->mm_lock */
	hash_for_each_possible(dev_priv->mm_structs, mm, node, (unsigned long)real)
		if (mm->mm == real)
			return mm;

	return NULL;
}

static int
i915_gem_userptr_init__mm_struct(struct drm_i915_gem_object *obj)
{
	struct drm_i915_private *dev_priv = to_i915(obj->base.dev);
	struct i915_mm_struct *mm;
	int ret = 0;

	/* During release of the GEM object we hold the struct_mutex. This
	 * precludes us from calling mmput() at that time as that may be
	 * the last reference and so call exit_mmap(). exit_mmap() will
	 * attempt to reap the vma, and if we were holding a GTT mmap
	 * would then call drm_gem_vm_close() and attempt to reacquire
	 * the struct mutex. So in order to avoid that recursion, we have
	 * to defer releasing the mm reference until after we drop the
	 * struct_mutex, i.e. we need to schedule a worker to do the clean
	 * up.
	 */
	mutex_lock(&dev_priv->mm_lock);
	mm = __i915_mm_struct_find(dev_priv, current->mm);
	if (mm == NULL) {
		mm = kmalloc(sizeof(*mm), GFP_KERNEL);
		if (mm == NULL) {
			ret = -ENOMEM;
			goto out;
		}

		kref_init(&mm->kref);
		mm->i915 = to_i915(obj->base.dev);

		mm->mm = current->mm;
		mmgrab(current->mm);

		mm->mn = NULL;

		/* Protected by dev_priv->mm_lock */
		hash_add(dev_priv->mm_structs,
			 &mm->node, (unsigned long)mm->mm);
	} else
		kref_get(&mm->kref);

	obj->userptr.mm = mm;
out:
	mutex_unlock(&dev_priv->mm_lock);
	return ret;
}

static void
__i915_mm_struct_free__worker(struct work_struct *work)
{
	struct i915_mm_struct *mm = container_of(work, typeof(*mm), work);
	i915_mmu_notifier_free(mm->mn, mm->mm);
	mmdrop(mm->mm);
	kfree(mm);
}

static void
__i915_mm_struct_free(struct kref *kref)
{
	struct i915_mm_struct *mm = container_of(kref, typeof(*mm), kref);

	/* Protected by dev_priv->mm_lock */
	hash_del(&mm->node);
	mutex_unlock(&mm->i915->mm_lock);

	INIT_WORK(&mm->work, __i915_mm_struct_free__worker);
	queue_work(mm->i915->mm.userptr_wq, &mm->work);
}

static void
i915_gem_userptr_release__mm_struct(struct drm_i915_gem_object *obj)
{
	if (obj->userptr.mm == NULL)
		return;

	kref_put_mutex(&obj->userptr.mm->kref,
		       __i915_mm_struct_free,
		       &to_i915(obj->base.dev)->mm_lock);
	obj->userptr.mm = NULL;
}

struct get_pages_work {
	struct work_struct work;
	struct drm_i915_gem_object *obj;
	struct task_struct *task;
};

static struct sg_table *
__i915_gem_userptr_alloc_pages(struct drm_i915_gem_object *obj,
			       struct vm_page **pvec, int num_pages)
{
	unsigned int max_segment = i915_sg_segment_size();
	struct sg_table *st;
	unsigned int sg_page_sizes;
	int ret;

	st = kmalloc(sizeof(*st), GFP_KERNEL);
	if (!st)
		return ERR_PTR(-ENOMEM);

alloc_table:
	ret = __sg_alloc_table_from_pages(st, pvec, num_pages,
					  0, num_pages << PAGE_SHIFT,
					  max_segment,
					  GFP_KERNEL);
	if (ret) {
		kfree(st);
		return ERR_PTR(ret);
	}

	ret = i915_gem_gtt_prepare_pages(obj, st);
	if (ret) {
		sg_free_table(st);

		if (max_segment > PAGE_SIZE) {
			max_segment = PAGE_SIZE;
			goto alloc_table;
		}

		kfree(st);
		return ERR_PTR(ret);
	}

	sg_page_sizes = i915_sg_page_sizes(st->sgl);

	__i915_gem_object_set_pages(obj, st, sg_page_sizes);

	return st;
}

static int
__i915_gem_userptr_set_active(struct drm_i915_gem_object *obj,
			      bool value)
{
	int ret = 0;

	/* During mm_invalidate_range we need to cancel any userptr that
	 * overlaps the range being invalidated. Doing so requires the
	 * struct_mutex, and that risks recursion. In order to cause
	 * recursion, the user must alias the userptr address space with
	 * a GTT mmapping (possible with a MAP_FIXED) - then when we have
	 * to invalidate that mmaping, mm_invalidate_range is called with
	 * the userptr address *and* the struct_mutex held.  To prevent that
	 * we set a flag under the i915_mmu_notifier spinlock to indicate
	 * whether this object is valid.
	 */
#if defined(CONFIG_MMU_NOTIFIER)
	if (obj->userptr.mmu_object == NULL)
		return 0;

	spin_lock(&obj->userptr.mmu_object->mn->lock);
	/* In order to serialise get_pages with an outstanding
	 * cancel_userptr, we must drop the struct_mutex and try again.
	 */
	if (!value)
		del_object(obj->userptr.mmu_object);
	else if (!work_pending(&obj->userptr.mmu_object->work))
		add_object(obj->userptr.mmu_object);
	else
		ret = -EAGAIN;
	spin_unlock(&obj->userptr.mmu_object->mn->lock);
#endif

	return ret;
}

static void
__i915_gem_userptr_get_pages_worker(struct work_struct *_work)
{
	struct get_pages_work *work = container_of(_work, typeof(*work), work);
	struct drm_i915_gem_object *obj = work->obj;
	const int npages = obj->base.size >> PAGE_SHIFT;
	struct vm_page **pvec;
	int pinned, ret;

	ret = -ENOMEM;
	pinned = 0;

	pvec = kvmalloc_array(npages, sizeof(struct vm_page *), GFP_KERNEL);
	if (pvec != NULL) {
		struct mm_struct *mm = obj->userptr.mm->mm;
		unsigned int flags = 0;

		if (!i915_gem_object_is_readonly(obj))
			flags |= FOLL_WRITE;

		ret = -EFAULT;
		if (mmget_not_zero(mm)) {
			down_read(&mm->mmap_sem);
			while (pinned < npages) {
				ret = get_user_pages_remote
					(work->task, mm,
					 obj->userptr.ptr + pinned * PAGE_SIZE,
					 npages - pinned,
					 flags,
					 pvec + pinned, NULL, NULL);
				if (ret < 0)
					break;

				pinned += ret;
			}
			up_read(&mm->mmap_sem);
			mmput(mm);
		}
	}

	mutex_lock(&obj->mm.lock);
	if (obj->userptr.work == &work->work) {
		struct sg_table *pages = ERR_PTR(ret);

		if (pinned == npages) {
			pages = __i915_gem_userptr_alloc_pages(obj, pvec,
							       npages);
			if (!IS_ERR(pages)) {
				pinned = 0;
				pages = NULL;
			}
		}

		obj->userptr.work = ERR_CAST(pages);
		if (IS_ERR(pages))
			__i915_gem_userptr_set_active(obj, false);
	}
	mutex_unlock(&obj->mm.lock);

	release_pages(pvec, pinned);
	kvfree(pvec);

	i915_gem_object_put(obj);
	put_task_struct(work->task);
	kfree(work);
}

static struct sg_table *
__i915_gem_userptr_get_pages_schedule(struct drm_i915_gem_object *obj)
{
	struct get_pages_work *work;

	/* Spawn a worker so that we can acquire the
	 * user pages without holding our mutex. Access
	 * to the user pages requires mmap_sem, and we have
	 * a strict lock ordering of mmap_sem, struct_mutex -
	 * we already hold struct_mutex here and so cannot
	 * call gup without encountering a lock inversion.
	 *
	 * Userspace will keep on repeating the operation
	 * (thanks to EAGAIN) until either we hit the fast
	 * path or the worker completes. If the worker is
	 * cancelled or superseded, the task is still run
	 * but the results ignored. (This leads to
	 * complications that we may have a stray object
	 * refcount that we need to be wary of when
	 * checking for existing objects during creation.)
	 * If the worker encounters an error, it reports
	 * that error back to this function through
	 * obj->userptr.work = ERR_PTR.
	 */
	work = kmalloc(sizeof(*work), GFP_KERNEL);
	if (work == NULL)
		return ERR_PTR(-ENOMEM);

	obj->userptr.work = &work->work;

	work->obj = i915_gem_object_get(obj);

	work->task = current;
	get_task_struct(work->task);

	INIT_WORK(&work->work, __i915_gem_userptr_get_pages_worker);
	queue_work(to_i915(obj->base.dev)->mm.userptr_wq, &work->work);

	return ERR_PTR(-EAGAIN);
}

static int i915_gem_userptr_get_pages(struct drm_i915_gem_object *obj)
{
	const int num_pages = obj->base.size >> PAGE_SHIFT;
	struct mm_struct *mm = obj->userptr.mm->mm;
	struct vm_page **pvec;
	struct sg_table *pages;
	bool active;
	int pinned;

	/* If userspace should engineer that these pages are replaced in
	 * the vma between us binding this page into the GTT and completion
	 * of rendering... Their loss. If they change the mapping of their
	 * pages they need to create a new bo to point to the new vma.
	 *
	 * However, that still leaves open the possibility of the vma
	 * being copied upon fork. Which falls under the same userspace
	 * synchronisation issue as a regular bo, except that this time
	 * the process may not be expecting that a particular piece of
	 * memory is tied to the GPU.
	 *
	 * Fortunately, we can hook into the mmu_notifier in order to
	 * discard the page references prior to anything nasty happening
	 * to the vma (discard or cloning) which should prevent the more
	 * egregious cases from causing harm.
	 */

	if (obj->userptr.work) {
		/* active flag should still be held for the pending work */
		if (IS_ERR(obj->userptr.work))
			return PTR_ERR(obj->userptr.work);
		else
			return -EAGAIN;
	}

	pvec = NULL;
	pinned = 0;

	if (mm == current->mm) {
		pvec = kvmalloc_array(num_pages, sizeof(struct vm_page *),
				      GFP_KERNEL |
				      __GFP_NORETRY |
				      __GFP_NOWARN);
		if (pvec) /* defer to worker if malloc fails */
			pinned = __get_user_pages_fast(obj->userptr.ptr,
						       num_pages,
						       !i915_gem_object_is_readonly(obj),
						       pvec);
	}

	active = false;
	if (pinned < 0) {
		pages = ERR_PTR(pinned);
		pinned = 0;
	} else if (pinned < num_pages) {
		pages = __i915_gem_userptr_get_pages_schedule(obj);
		active = pages == ERR_PTR(-EAGAIN);
	} else {
		pages = __i915_gem_userptr_alloc_pages(obj, pvec, num_pages);
		active = !IS_ERR(pages);
	}
	if (active)
		__i915_gem_userptr_set_active(obj, true);

	if (IS_ERR(pages))
		release_pages(pvec, pinned);
	kvfree(pvec);

	return PTR_ERR_OR_ZERO(pages);
}

static void
i915_gem_userptr_put_pages(struct drm_i915_gem_object *obj,
			   struct sg_table *pages)
{
	struct sgt_iter sgt_iter;
	struct vm_page *page;

	BUG_ON(obj->userptr.work != NULL);
	__i915_gem_userptr_set_active(obj, false);

	if (obj->mm.madv != I915_MADV_WILLNEED)
		obj->mm.dirty = false;

	i915_gem_gtt_finish_pages(obj, pages);

	for_each_sgt_page(page, sgt_iter, pages) {
		if (obj->mm.dirty)
			set_page_dirty(page);

		mark_page_accessed(page);
		put_page(page);
	}
	obj->mm.dirty = false;

	sg_free_table(pages);
	kfree(pages);
}

static void
i915_gem_userptr_release(struct drm_i915_gem_object *obj)
{
	i915_gem_userptr_release__mmu_notifier(obj);
	i915_gem_userptr_release__mm_struct(obj);
}

static int
i915_gem_userptr_dmabuf_export(struct drm_i915_gem_object *obj)
{
	if (obj->userptr.mmu_object)
		return 0;

	return i915_gem_userptr_init__mmu_notifier(obj, 0);
}

static const struct drm_i915_gem_object_ops i915_gem_userptr_ops = {
	.flags = I915_GEM_OBJECT_HAS_STRUCT_PAGE |
		 I915_GEM_OBJECT_IS_SHRINKABLE,
	.get_pages = i915_gem_userptr_get_pages,
	.put_pages = i915_gem_userptr_put_pages,
	.dmabuf_export = i915_gem_userptr_dmabuf_export,
	.release = i915_gem_userptr_release,
};

/*
 * Creates a new mm object that wraps some normal memory from the process
 * context - user memory.
 *
 * We impose several restrictions upon the memory being mapped
 * into the GPU.
 * 1. It must be page aligned (both start/end addresses, i.e ptr and size).
 * 2. It must be normal system memory, not a pointer into another map of IO
 *    space (e.g. it must not be a GTT mmapping of another object).
 * 3. We only allow a bo as large as we could in theory map into the GTT,
 *    that is we limit the size to the total size of the GTT.
 * 4. The bo is marked as being snoopable. The backing pages are left
 *    accessible directly by the CPU, but reads and writes by the GPU may
 *    incur the cost of a snoop (unless you have an LLC architecture).
 *
 * Synchronisation between multiple users and the GPU is left to userspace
 * through the normal set-domain-ioctl. The kernel will enforce that the
 * GPU relinquishes the VMA before it is returned back to the system
 * i.e. upon free(), munmap() or process termination. However, the userspace
 * malloc() library may not immediately relinquish the VMA after free() and
 * instead reuse it whilst the GPU is still reading and writing to the VMA.
 * Caveat emptor.
 *
 * Also note, that the object created here is not currently a "first class"
 * object, in that several ioctls are banned. These are the CPU access
 * ioctls: mmap(), pwrite and pread. In practice, you are expected to use
 * direct access via your pointer rather than use those ioctls. Another
 * restriction is that we do not allow userptr surfaces to be pinned to the
 * hardware and so we reject any attempt to create a framebuffer out of a
 * userptr.
 *
 * If you think this is a good interface to use to pass GPU memory between
 * drivers, please use dma-buf instead. In fact, wherever possible use
 * dma-buf instead.
 */
int
i915_gem_userptr_ioctl(struct drm_device *dev,
		       void *data,
		       struct drm_file *file)
{
	struct drm_i915_private *dev_priv = to_i915(dev);
	struct drm_i915_gem_userptr *args = data;
	struct drm_i915_gem_object *obj;
	int ret;
	u32 handle;

	if (!HAS_LLC(dev_priv) && !HAS_SNOOP(dev_priv)) {
		/* We cannot support coherent userptr objects on hw without
		 * LLC and broken snooping.
		 */
		return -ENODEV;
	}

	if (args->flags & ~(I915_USERPTR_READ_ONLY |
			    I915_USERPTR_UNSYNCHRONIZED))
		return -EINVAL;

	if (!args->user_size)
		return -EINVAL;

	if (offset_in_page(args->user_ptr | args->user_size))
		return -EINVAL;

	if (!access_ok(args->flags & I915_USERPTR_READ_ONLY ? VERIFY_READ : VERIFY_WRITE,
		       (char __user *)(unsigned long)args->user_ptr, args->user_size))
		return -EFAULT;

	if (args->flags & I915_USERPTR_READ_ONLY) {
		struct i915_hw_ppgtt *ppgtt;

		/*
		 * On almost all of the older hw, we cannot tell the GPU that
		 * a page is readonly.
		 */
		ppgtt = dev_priv->kernel_context->ppgtt;
		if (!ppgtt || !ppgtt->vm.has_read_only)
			return -ENODEV;
	}

	obj = i915_gem_object_alloc(dev_priv);
	if (obj == NULL)
		return -ENOMEM;

	drm_gem_private_object_init(dev, &obj->base, args->user_size);
	i915_gem_object_init(obj, &i915_gem_userptr_ops);
	obj->read_domains = I915_GEM_DOMAIN_CPU;
	obj->write_domain = I915_GEM_DOMAIN_CPU;
	i915_gem_object_set_cache_coherency(obj, I915_CACHE_LLC);

	obj->userptr.ptr = args->user_ptr;
	if (args->flags & I915_USERPTR_READ_ONLY)
		i915_gem_object_set_readonly(obj);

	/* And keep a pointer to the current->mm for resolving the user pages
	 * at binding. This means that we need to hook into the mmu_notifier
	 * in order to detect if the mmu is destroyed.
	 */
	ret = i915_gem_userptr_init__mm_struct(obj);
	if (ret == 0)
		ret = i915_gem_userptr_init__mmu_notifier(obj, args->flags);
	if (ret == 0)
		ret = drm_gem_handle_create(file, &obj->base, &handle);

	/* drop reference from allocate - handle holds it now */
	i915_gem_object_put(obj);
	if (ret)
		return ret;

	args->handle = handle;
	return 0;
}

#else

int
i915_gem_userptr_ioctl(struct drm_device *dev, void *data, struct drm_file *file)
{
	return -ENODEV;
}

#endif

int i915_gem_init_userptr(struct drm_i915_private *dev_priv)
{
	rw_init(&dev_priv->mm_lock, "userptr");
	hash_init(dev_priv->mm_structs);

	dev_priv->mm.userptr_wq =
		alloc_workqueue("i915-userptr-acquire",
				WQ_HIGHPRI | WQ_UNBOUND,
				0);
	if (!dev_priv->mm.userptr_wq)
		return -ENOMEM;

	return 0;
}

void i915_gem_cleanup_userptr(struct drm_i915_private *dev_priv)
{
	destroy_workqueue(dev_priv->mm.userptr_wq);
}
