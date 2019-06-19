/*
 * Copyright © 2008-2015 Intel Corporation
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

#include <linux/oom.h>
#include <linux/sched/mm.h>
#include <linux/shmem_fs.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/pci.h>
#include <linux/dma-buf.h>
#include <linux/vmalloc.h>
#include <drm/drmP.h>
#include <drm/i915_drm.h>

#include "i915_drv.h"
#include "i915_trace.h"

#ifdef notyet
static bool shrinker_lock(struct drm_i915_private *i915, bool *unlock)
{
	switch (mutex_trylock_recursive(&i915->drm.struct_mutex)) {
	case MUTEX_TRYLOCK_RECURSIVE:
		*unlock = false;
		return true;

	case MUTEX_TRYLOCK_FAILED:
		*unlock = false;
		preempt_disable();
		do {
			cpu_relax();
			if (mutex_trylock(&i915->drm.struct_mutex)) {
				*unlock = true;
				break;
			}
		} while (!drm_need_resched());
		preempt_enable();
		return *unlock;

	case MUTEX_TRYLOCK_SUCCESS:
		*unlock = true;
		return true;
	}
}

static void shrinker_unlock(struct drm_i915_private *i915, bool unlock)
{
	if (!unlock)
		return;

	mutex_unlock(&i915->drm.struct_mutex);
}

static bool swap_available(void)
{
	return get_nr_swap_pages() > 0;
}

static bool can_release_pages(struct drm_i915_gem_object *obj)
{
	/* Consider only shrinkable ojects. */
	if (!i915_gem_object_is_shrinkable(obj))
		return false;

	/* Only report true if by unbinding the object and putting its pages
	 * we can actually make forward progress towards freeing physical
	 * pages.
	 *
	 * If the pages are pinned for any other reason than being bound
	 * to the GPU, simply unbinding from the GPU is not going to succeed
	 * in releasing our pin count on the pages themselves.
	 */
	if (atomic_read(&obj->mm.pages_pin_count) > obj->bind_count)
		return false;

	/* If any vma are "permanently" pinned, it will prevent us from
	 * reclaiming the obj->mm.pages. We only allow scanout objects to claim
	 * a permanent pin, along with a few others like the context objects.
	 * To simplify the scan, and to avoid walking the list of vma under the
	 * object, we just check the count of its permanently pinned.
	 */
	if (READ_ONCE(obj->pin_global))
		return false;

	/* We can only return physical pages to the system if we can either
	 * discard the contents (because the user has marked them as being
	 * purgeable) or if we can move their contents out to swap.
	 */
	return swap_available() || obj->mm.madv == I915_MADV_DONTNEED;
}

static bool unsafe_drop_pages(struct drm_i915_gem_object *obj)
{
	if (i915_gem_object_unbind(obj) == 0)
		__i915_gem_object_put_pages(obj, I915_MM_SHRINKER);
	return !i915_gem_object_has_pages(obj);
}
#endif

/**
 * i915_gem_shrink - Shrink buffer object caches
 * @i915: i915 device
 * @target: amount of memory to make available, in pages
 * @nr_scanned: optional output for number of pages scanned (incremental)
 * @flags: control flags for selecting cache types
 *
 * This function is the main interface to the shrinker. It will try to release
 * up to @target pages of main memory backing storage from buffer objects.
 * Selection of the specific caches can be done with @flags. This is e.g. useful
 * when purgeable objects should be removed from caches preferentially.
 *
 * Note that it's not guaranteed that released amount is actually available as
 * free system memory - the pages might still be in-used to due to other reasons
 * (like cpu mmaps) or the mm core has reused them before we could grab them.
 * Therefore code that needs to explicitly shrink buffer objects caches (e.g. to
 * avoid deadlocks in memory reclaim) must fall back to i915_gem_shrink_all().
 *
 * Also note that any kind of pinning (both per-vma address space pins and
 * backing storage pins at the buffer object level) result in the shrinker code
 * having to skip the object.
 *
 * Returns:
 * The number of pages of backing storage actually released.
 */
unsigned long
i915_gem_shrink(struct drm_i915_private *i915,
		unsigned long target,
		unsigned long *nr_scanned,
		unsigned flags)
{
	STUB();
	return -ENOSYS;
#ifdef notyet
	const struct {
		struct list_head *list;
		unsigned int bit;
	} phases[] = {
		{ &i915->mm.unbound_list, I915_SHRINK_UNBOUND },
		{ &i915->mm.bound_list, I915_SHRINK_BOUND },
		{ NULL, 0 },
	}, *phase;
	unsigned long count = 0;
	unsigned long scanned = 0;
	bool unlock;

	if (!shrinker_lock(i915, &unlock))
		return 0;

	/*
	 * When shrinking the active list, also consider active contexts.
	 * Active contexts are pinned until they are retired, and so can
	 * not be simply unbound to retire and unpin their pages. To shrink
	 * the contexts, we must wait until the gpu is idle.
	 *
	 * We don't care about errors here; if we cannot wait upon the GPU,
	 * we will free as much as we can and hope to get a second chance.
	 */
	if (flags & I915_SHRINK_ACTIVE)
		i915_gem_wait_for_idle(i915,
				       I915_WAIT_LOCKED,
				       MAX_SCHEDULE_TIMEOUT);

	trace_i915_gem_shrink(i915, target, flags);
	i915_retire_requests(i915);

	/*
	 * Unbinding of objects will require HW access; Let us not wake the
	 * device just to recover a little memory. If absolutely necessary,
	 * we will force the wake during oom-notifier.
	 */
	if ((flags & I915_SHRINK_BOUND) &&
	    !intel_runtime_pm_get_if_in_use(i915))
		flags &= ~I915_SHRINK_BOUND;

	/*
	 * As we may completely rewrite the (un)bound list whilst unbinding
	 * (due to retiring requests) we have to strictly process only
	 * one element of the list at the time, and recheck the list
	 * on every iteration.
	 *
	 * In particular, we must hold a reference whilst removing the
	 * object as we may end up waiting for and/or retiring the objects.
	 * This might release the final reference (held by the active list)
	 * and result in the object being freed from under us. This is
	 * similar to the precautions the eviction code must take whilst
	 * removing objects.
	 *
	 * Also note that although these lists do not hold a reference to
	 * the object we can safely grab one here: The final object
	 * unreferencing and the bound_list are both protected by the
	 * dev->struct_mutex and so we won't ever be able to observe an
	 * object on the bound_list with a reference count equals 0.
	 */
	for (phase = phases; phase->list; phase++) {
		struct list_head still_in_list;
		struct drm_i915_gem_object *obj;

		if ((flags & phase->bit) == 0)
			continue;

		INIT_LIST_HEAD(&still_in_list);

		/*
		 * We serialize our access to unreferenced objects through
		 * the use of the struct_mutex. While the objects are not
		 * yet freed (due to RCU then a workqueue) we still want
		 * to be able to shrink their pages, so they remain on
		 * the unbound/bound list until actually freed.
		 */
		spin_lock(&i915->mm.obj_lock);
		while (count < target &&
		       (obj = list_first_entry_or_null(phase->list,
						       typeof(*obj),
						       mm.link))) {
			list_move_tail(&obj->mm.link, &still_in_list);

			if (flags & I915_SHRINK_PURGEABLE &&
			    obj->mm.madv != I915_MADV_DONTNEED)
				continue;

			if (flags & I915_SHRINK_VMAPS &&
			    !is_vmalloc_addr(obj->mm.mapping))
				continue;

			if (!(flags & I915_SHRINK_ACTIVE) &&
			    (i915_gem_object_is_active(obj) ||
			     i915_gem_object_is_framebuffer(obj)))
				continue;

			if (!can_release_pages(obj))
				continue;

			spin_unlock(&i915->mm.obj_lock);

			if (unsafe_drop_pages(obj)) {
				/* May arrive from get_pages on another bo */
				mutex_lock_nested(&obj->mm.lock,
						  I915_MM_SHRINKER);
				if (!i915_gem_object_has_pages(obj)) {
					__i915_gem_object_invalidate(obj);
					count += obj->base.size >> PAGE_SHIFT;
				}
				mutex_unlock(&obj->mm.lock);
			}
			scanned += obj->base.size >> PAGE_SHIFT;

			spin_lock(&i915->mm.obj_lock);
		}
		list_splice_tail(&still_in_list, phase->list);
		spin_unlock(&i915->mm.obj_lock);
	}

	if (flags & I915_SHRINK_BOUND)
		intel_runtime_pm_put(i915);

	i915_retire_requests(i915);

	shrinker_unlock(i915, unlock);

	if (nr_scanned)
		*nr_scanned += scanned;
	return count;
#endif
}

/**
 * i915_gem_shrink_all - Shrink buffer object caches completely
 * @i915: i915 device
 *
 * This is a simple wraper around i915_gem_shrink() to aggressively shrink all
 * caches completely. It also first waits for and retires all outstanding
 * requests to also be able to release backing storage for active objects.
 *
 * This should only be used in code to intentionally quiescent the gpu or as a
 * last-ditch effort when memory seems to have run out.
 *
 * Returns:
 * The number of pages of backing storage actually released.
 */
unsigned long i915_gem_shrink_all(struct drm_i915_private *i915)
{
	unsigned long freed;

	intel_runtime_pm_get(i915);
	freed = i915_gem_shrink(i915, -1UL, NULL,
				I915_SHRINK_BOUND |
				I915_SHRINK_UNBOUND |
				I915_SHRINK_ACTIVE);
	intel_runtime_pm_put(i915);

	return freed;
}

#ifdef notyet
static unsigned long
i915_gem_shrinker_count(struct shrinker *shrinker, struct shrink_control *sc)
{
	struct drm_i915_private *i915 =
		container_of(shrinker, struct drm_i915_private, mm.shrinker);
	struct drm_i915_gem_object *obj;
	unsigned long num_objects = 0;
	unsigned long count = 0;

	spin_lock(&i915->mm.obj_lock);
	list_for_each_entry(obj, &i915->mm.unbound_list, mm.link)
		if (can_release_pages(obj)) {
			count += obj->base.size >> PAGE_SHIFT;
			num_objects++;
		}

	list_for_each_entry(obj, &i915->mm.bound_list, mm.link)
		if (!i915_gem_object_is_active(obj) && can_release_pages(obj)) {
			count += obj->base.size >> PAGE_SHIFT;
			num_objects++;
		}
	spin_unlock(&i915->mm.obj_lock);

	/* Update our preferred vmscan batch size for the next pass.
	 * Our rough guess for an effective batch size is roughly 2
	 * available GEM objects worth of pages. That is we don't want
	 * the shrinker to fire, until it is worth the cost of freeing an
	 * entire GEM object.
	 */
	if (num_objects) {
		unsigned long avg = 2 * count / num_objects;

		i915->mm.shrinker.batch =
			max((i915->mm.shrinker.batch + avg) >> 1,
			    128ul /* default SHRINK_BATCH */);
	}

	return count;
}

static unsigned long
i915_gem_shrinker_scan(struct shrinker *shrinker, struct shrink_control *sc)
{
	struct drm_i915_private *i915 =
		container_of(shrinker, struct drm_i915_private, mm.shrinker);
	unsigned long freed;
	bool unlock;

	sc->nr_scanned = 0;

	if (!shrinker_lock(i915, &unlock))
		return SHRINK_STOP;

	freed = i915_gem_shrink(i915,
				sc->nr_to_scan,
				&sc->nr_scanned,
				I915_SHRINK_BOUND |
				I915_SHRINK_UNBOUND |
				I915_SHRINK_PURGEABLE);
	if (sc->nr_scanned < sc->nr_to_scan)
		freed += i915_gem_shrink(i915,
					 sc->nr_to_scan - sc->nr_scanned,
					 &sc->nr_scanned,
					 I915_SHRINK_BOUND |
					 I915_SHRINK_UNBOUND);
	if (sc->nr_scanned < sc->nr_to_scan && current_is_kswapd()) {
		intel_runtime_pm_get(i915);
		freed += i915_gem_shrink(i915,
					 sc->nr_to_scan - sc->nr_scanned,
					 &sc->nr_scanned,
					 I915_SHRINK_ACTIVE |
					 I915_SHRINK_BOUND |
					 I915_SHRINK_UNBOUND);
		intel_runtime_pm_put(i915);
	}

	shrinker_unlock(i915, unlock);

	return sc->nr_scanned ? freed : SHRINK_STOP;
}

static bool
shrinker_lock_uninterruptible(struct drm_i915_private *i915, bool *unlock,
			      int timeout_ms)
{
	unsigned long timeout = jiffies + msecs_to_jiffies_timeout(timeout_ms);

	do {
		if (i915_gem_wait_for_idle(i915,
					   0, MAX_SCHEDULE_TIMEOUT) == 0 &&
		    shrinker_lock(i915, unlock))
			break;

		schedule_timeout_killable(1);
		if (fatal_signal_pending(current))
			return false;

		if (time_after(jiffies, timeout)) {
			pr_err("Unable to lock GPU to purge memory.\n");
			return false;
		}
	} while (1);

	return true;
}

static int
i915_gem_shrinker_oom(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct drm_i915_private *i915 =
		container_of(nb, struct drm_i915_private, mm.oom_notifier);
	struct drm_i915_gem_object *obj;
	unsigned long unevictable, bound, unbound, freed_pages;

	freed_pages = i915_gem_shrink_all(i915);

	/* Because we may be allocating inside our own driver, we cannot
	 * assert that there are no objects with pinned pages that are not
	 * being pointed to by hardware.
	 */
	unbound = bound = unevictable = 0;
	spin_lock(&i915->mm.obj_lock);
	list_for_each_entry(obj, &i915->mm.unbound_list, mm.link) {
		if (!can_release_pages(obj))
			unevictable += obj->base.size >> PAGE_SHIFT;
		else
			unbound += obj->base.size >> PAGE_SHIFT;
	}
	list_for_each_entry(obj, &i915->mm.bound_list, mm.link) {
		if (!can_release_pages(obj))
			unevictable += obj->base.size >> PAGE_SHIFT;
		else
			bound += obj->base.size >> PAGE_SHIFT;
	}
	spin_unlock(&i915->mm.obj_lock);

	if (freed_pages || unbound || bound)
		pr_info("Purging GPU memory, %lu pages freed, "
			"%lu pages still pinned.\n",
			freed_pages, unevictable);
	if (unbound || bound)
		pr_err("%lu and %lu pages still available in the "
		       "bound and unbound GPU page lists.\n",
		       bound, unbound);

	*(unsigned long *)ptr += freed_pages;
	return NOTIFY_DONE;
}

static int
i915_gem_shrinker_vmap(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct drm_i915_private *i915 =
		container_of(nb, struct drm_i915_private, mm.vmap_notifier);
	struct i915_vma *vma, *next;
	unsigned long freed_pages = 0;
	bool unlock;
	int ret;

	if (!shrinker_lock_uninterruptible(i915, &unlock, 5000))
		return NOTIFY_DONE;

	/* Force everything onto the inactive lists */
	ret = i915_gem_wait_for_idle(i915,
				     I915_WAIT_LOCKED,
				     MAX_SCHEDULE_TIMEOUT);
	if (ret)
		goto out;

	intel_runtime_pm_get(i915);
	freed_pages += i915_gem_shrink(i915, -1UL, NULL,
				       I915_SHRINK_BOUND |
				       I915_SHRINK_UNBOUND |
				       I915_SHRINK_ACTIVE |
				       I915_SHRINK_VMAPS);
	intel_runtime_pm_put(i915);

	/* We also want to clear any cached iomaps as they wrap vmap */
	list_for_each_entry_safe(vma, next,
				 &i915->ggtt.vm.inactive_list, vm_link) {
		unsigned long count = vma->node.size >> PAGE_SHIFT;
		if (vma->iomap && i915_vma_unbind(vma) == 0)
			freed_pages += count;
	}

out:
	shrinker_unlock(i915, unlock);

	*(unsigned long *)ptr += freed_pages;
	return NOTIFY_DONE;
}
#endif

/**
 * i915_gem_shrinker_register - Register the i915 shrinker
 * @i915: i915 device
 *
 * This function registers and sets up the i915 shrinker and OOM handler.
 */
void i915_gem_shrinker_register(struct drm_i915_private *i915)
{
#ifdef notyet
	i915->mm.shrinker.scan_objects = i915_gem_shrinker_scan;
	i915->mm.shrinker.count_objects = i915_gem_shrinker_count;
	i915->mm.shrinker.seeks = DEFAULT_SEEKS;
	i915->mm.shrinker.batch = 4096;
	WARN_ON(register_shrinker(&i915->mm.shrinker));

	i915->mm.oom_notifier.notifier_call = i915_gem_shrinker_oom;
	WARN_ON(register_oom_notifier(&i915->mm.oom_notifier));

	i915->mm.vmap_notifier.notifier_call = i915_gem_shrinker_vmap;
	WARN_ON(register_vmap_purge_notifier(&i915->mm.vmap_notifier));
#endif
}

/**
 * i915_gem_shrinker_unregister - Unregisters the i915 shrinker
 * @i915: i915 device
 *
 * This function unregisters the i915 shrinker and OOM handler.
 */
void i915_gem_shrinker_unregister(struct drm_i915_private *i915)
{
	STUB();
#ifdef notyet
	WARN_ON(unregister_vmap_purge_notifier(&i915->mm.vmap_notifier));
	WARN_ON(unregister_oom_notifier(&i915->mm.oom_notifier));
	unregister_shrinker(&i915->mm.shrinker);
#endif
}

void i915_gem_shrinker_taints_mutex(struct rwlock *mutex)
{
#ifdef notyet
	if (!IS_ENABLED(CONFIG_LOCKDEP))
		return;

	fs_reclaim_acquire(GFP_KERNEL);
	mutex_lock(mutex);
	mutex_unlock(mutex);
	fs_reclaim_release(GFP_KERNEL);
#endif
}
