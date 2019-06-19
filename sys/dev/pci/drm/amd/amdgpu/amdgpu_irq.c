/*
 * Copyright 2008 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
 * Copyright 2009 Jerome Glisse.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie
 *          Alex Deucher
 *          Jerome Glisse
 */

/**
 * DOC: Interrupt Handling
 *
 * Interrupts generated within GPU hardware raise interrupt requests that are
 * passed to amdgpu IRQ handler which is responsible for detecting source and
 * type of the interrupt and dispatching matching handlers. If handling an
 * interrupt requires calling kernel functions that may sleep processing is
 * dispatched to work handlers.
 *
 * If MSI functionality is not disabled by module parameter then MSI
 * support will be enabled.
 *
 * For GPU interrupt sources that may be driven by another driver, IRQ domain
 * support is used (with mapping between virtual and hardware IRQs).
 */

#include <linux/irq.h>
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/amdgpu_drm.h>
#include "amdgpu.h"
#include "amdgpu_ih.h"
#include "atom.h"
#include "amdgpu_connectors.h"
#include "amdgpu_trace.h"

#include <linux/pm_runtime.h>

#ifdef CONFIG_DRM_AMD_DC
#include "amdgpu_dm_irq.h"
#endif

#define AMDGPU_WAIT_IDLE_TIMEOUT 200

/**
 * amdgpu_hotplug_work_func - work handler for display hotplug event
 *
 * @work: work struct pointer
 *
 * This is the hotplug event work handler (all ASICs).
 * The work gets scheduled from the IRQ handler if there
 * was a hotplug interrupt.  It walks through the connector table
 * and calls hotplug handler for each connector. After this, it sends
 * a DRM hotplug event to alert userspace.
 *
 * This design approach is required in order to defer hotplug event handling
 * from the IRQ handler to a work handler because hotplug handler has to use
 * mutexes which cannot be locked in an IRQ handler (since &mutex_lock may
 * sleep).
 */
static void amdgpu_hotplug_work_func(struct work_struct *work)
{
	struct amdgpu_device *adev = container_of(work, struct amdgpu_device,
						  hotplug_work);
	struct drm_device *dev = adev->ddev;
	struct drm_mode_config *mode_config = &dev->mode_config;
	struct drm_connector *connector;

	mutex_lock(&mode_config->mutex);
	list_for_each_entry(connector, &mode_config->connector_list, head)
		amdgpu_connector_hotplug(connector);
	mutex_unlock(&mode_config->mutex);
	/* Just fire off a uevent and let userspace tell us what to do */
	drm_helper_hpd_irq_event(dev);
}

/**
 * amdgpu_irq_reset_work_func - execute GPU reset
 *
 * @work: work struct pointer
 *
 * Execute scheduled GPU reset (Cayman+).
 * This function is called when the IRQ handler thinks we need a GPU reset.
 */
static void amdgpu_irq_reset_work_func(struct work_struct *work)
{
	struct amdgpu_device *adev = container_of(work, struct amdgpu_device,
						  reset_work);

	if (!amdgpu_sriov_vf(adev))
		amdgpu_device_gpu_recover(adev, NULL, false);
}

/**
 * amdgpu_irq_disable_all - disable *all* interrupts
 *
 * @adev: amdgpu device pointer
 *
 * Disable all types of interrupts from all sources.
 */
void amdgpu_irq_disable_all(struct amdgpu_device *adev)
{
	unsigned long irqflags;
	unsigned i, j, k;
	int r;

	spin_lock_irqsave(&adev->irq.lock, irqflags);
	for (i = 0; i < AMDGPU_IH_CLIENTID_MAX; ++i) {
		if (!adev->irq.client[i].sources)
			continue;

		for (j = 0; j < AMDGPU_MAX_IRQ_SRC_ID; ++j) {
			struct amdgpu_irq_src *src = adev->irq.client[i].sources[j];

			if (!src || !src->funcs->set || !src->num_types)
				continue;

			for (k = 0; k < src->num_types; ++k) {
				atomic_set(&src->enabled_types[k], 0);
				r = src->funcs->set(adev, src, k,
						    AMDGPU_IRQ_STATE_DISABLE);
				if (r)
					DRM_ERROR("error disabling interrupt (%d)\n",
						  r);
			}
		}
	}
	spin_unlock_irqrestore(&adev->irq.lock, irqflags);
}

/**
 * amdgpu_irq_handler - IRQ handler
 *
 * @irq: IRQ number (unused)
 * @arg: pointer to DRM device
 *
 * IRQ handler for amdgpu driver (all ASICs).
 *
 * Returns:
 * result of handling the IRQ, as defined by &irqreturn_t
 */
irqreturn_t amdgpu_irq_handler(void *arg)
{
	struct drm_device *dev = (struct drm_device *) arg;
	struct amdgpu_device *adev = dev->dev_private;
	irqreturn_t ret;

	if (!adev->irq.installed)
		return 0;

	ret = amdgpu_ih_process(adev);
	if (ret == IRQ_HANDLED)
		pm_runtime_mark_last_busy(dev->dev);
	return ret;
}

/**
 * amdgpu_msi_ok - check whether MSI functionality is enabled
 *
 * @adev: amdgpu device pointer (unused)
 *
 * Checks whether MSI functionality has been disabled via module parameter
 * (all ASICs).
 *
 * Returns:
 * *true* if MSIs are allowed to be enabled or *false* otherwise
 */
bool amdgpu_msi_ok(struct amdgpu_device *adev)
{
	if (amdgpu_msi == 1)
		return true;
	else if (amdgpu_msi == 0)
		return false;

	return true;
}

/**
 * amdgpu_irq_init - initialize interrupt handling
 *
 * @adev: amdgpu device pointer
 *
 * Sets up work functions for hotplug and reset interrupts, enables MSI
 * functionality, initializes vblank, hotplug and reset interrupt handling.
 *
 * Returns:
 * 0 on success or error code on failure
 */
int amdgpu_irq_init(struct amdgpu_device *adev)
{
	int r = 0;

	mtx_init(&adev->irq.lock, IPL_TTY);

#ifdef notyet
	/* Enable MSI if not disabled by module parameter */
	adev->irq.msi_enabled = false;

	if (amdgpu_msi_ok(adev)) {
		int ret = pci_enable_msi(adev->pdev);
		if (!ret) {
			adev->irq.msi_enabled = true;
			dev_dbg(adev->dev, "amdgpu: using MSI.\n");
		}
	}
#endif

	if (!amdgpu_device_has_dc_support(adev)) {
		if (!adev->enable_virtual_display)
			/* Disable vblank IRQs aggressively for power-saving */
			/* XXX: can this be enabled for DC? */
			adev->ddev->vblank_disable_immediate = true;

		r = drm_vblank_init(adev->ddev, adev->mode_info.num_crtc);
		if (r)
			return r;

		/* Pre-DCE11 */
		INIT_WORK(&adev->hotplug_work,
				amdgpu_hotplug_work_func);
	}

	INIT_WORK(&adev->reset_work, amdgpu_irq_reset_work_func);

	adev->irq.installed = true;
	r = drm_irq_install(adev->ddev, adev->ddev->pdev->irq);
	if (r) {
		adev->irq.installed = false;
		if (!amdgpu_device_has_dc_support(adev))
			flush_work(&adev->hotplug_work);
		cancel_work_sync(&adev->reset_work);
		return r;
	}
	adev->ddev->max_vblank_count = 0x00ffffff;

	DRM_DEBUG("amdgpu: irq initialized.\n");
	return 0;
}

/**
 * amdgpu_irq_fini - shut down interrupt handling
 *
 * @adev: amdgpu device pointer
 *
 * Tears down work functions for hotplug and reset interrupts, disables MSI
 * functionality, shuts down vblank, hotplug and reset interrupt handling,
 * turns off interrupts from all sources (all ASICs).
 */
void amdgpu_irq_fini(struct amdgpu_device *adev)
{
	unsigned i, j;

	if (adev->irq.installed) {
		drm_irq_uninstall(adev->ddev);
		adev->irq.installed = false;
		if (adev->irq.msi_enabled)
			pci_disable_msi(adev->pdev);
		if (!amdgpu_device_has_dc_support(adev))
			flush_work(&adev->hotplug_work);
		cancel_work_sync(&adev->reset_work);
	}

	for (i = 0; i < AMDGPU_IH_CLIENTID_MAX; ++i) {
		if (!adev->irq.client[i].sources)
			continue;

		for (j = 0; j < AMDGPU_MAX_IRQ_SRC_ID; ++j) {
			struct amdgpu_irq_src *src = adev->irq.client[i].sources[j];

			if (!src)
				continue;

			kfree(src->enabled_types);
			src->enabled_types = NULL;
			if (src->data) {
				kfree(src->data);
				kfree(src);
				adev->irq.client[i].sources[j] = NULL;
			}
		}
		kfree(adev->irq.client[i].sources);
		adev->irq.client[i].sources = NULL;
	}
}

/**
 * amdgpu_irq_add_id - register IRQ source
 *
 * @adev: amdgpu device pointer
 * @client_id: client id
 * @src_id: source id
 * @source: IRQ source pointer
 *
 * Registers IRQ source on a client.
 *
 * Returns:
 * 0 on success or error code otherwise
 */
int amdgpu_irq_add_id(struct amdgpu_device *adev,
		      unsigned client_id, unsigned src_id,
		      struct amdgpu_irq_src *source)
{
	if (client_id >= AMDGPU_IH_CLIENTID_MAX)
		return -EINVAL;

	if (src_id >= AMDGPU_MAX_IRQ_SRC_ID)
		return -EINVAL;

	if (!source->funcs)
		return -EINVAL;

	if (!adev->irq.client[client_id].sources) {
		adev->irq.client[client_id].sources =
			kcalloc(AMDGPU_MAX_IRQ_SRC_ID,
				sizeof(struct amdgpu_irq_src *),
				GFP_KERNEL);
		if (!adev->irq.client[client_id].sources)
			return -ENOMEM;
	}

	if (adev->irq.client[client_id].sources[src_id] != NULL)
		return -EINVAL;

	if (source->num_types && !source->enabled_types) {
		atomic_t *types;

		types = kcalloc(source->num_types, sizeof(atomic_t),
				GFP_KERNEL);
		if (!types)
			return -ENOMEM;

		source->enabled_types = types;
	}

	adev->irq.client[client_id].sources[src_id] = source;
	return 0;
}

/**
 * amdgpu_irq_dispatch - dispatch IRQ to IP blocks
 *
 * @adev: amdgpu device pointer
 * @entry: interrupt vector pointer
 *
 * Dispatches IRQ to IP blocks.
 */
void amdgpu_irq_dispatch(struct amdgpu_device *adev,
			 struct amdgpu_iv_entry *entry)
{
	unsigned client_id = entry->client_id;
	unsigned src_id = entry->src_id;
	struct amdgpu_irq_src *src;
	int r;

	trace_amdgpu_iv(entry);

	if (client_id >= AMDGPU_IH_CLIENTID_MAX) {
		DRM_DEBUG("Invalid client_id in IV: %d\n", client_id);
		return;
	}

	if (src_id >= AMDGPU_MAX_IRQ_SRC_ID) {
		DRM_DEBUG("Invalid src_id in IV: %d\n", src_id);
		return;
	}

	if (adev->irq.virq[src_id]) {
		STUB();
#ifdef notyet
		generic_handle_irq(irq_find_mapping(adev->irq.domain, src_id));
#endif
	} else {
		if (!adev->irq.client[client_id].sources) {
			DRM_DEBUG("Unregistered interrupt client_id: %d src_id: %d\n",
				  client_id, src_id);
			return;
		}

		src = adev->irq.client[client_id].sources[src_id];
		if (!src) {
			DRM_DEBUG("Unhandled interrupt src_id: %d\n", src_id);
			return;
		}

		r = src->funcs->process(adev, src, entry);
		if (r)
			DRM_ERROR("error processing interrupt (%d)\n", r);
	}
}

/**
 * amdgpu_irq_update - update hardware interrupt state
 *
 * @adev: amdgpu device pointer
 * @src: interrupt source pointer
 * @type: type of interrupt
 *
 * Updates interrupt state for the specific source (all ASICs).
 */
int amdgpu_irq_update(struct amdgpu_device *adev,
			     struct amdgpu_irq_src *src, unsigned type)
{
	unsigned long irqflags;
	enum amdgpu_interrupt_state state;
	int r;

	spin_lock_irqsave(&adev->irq.lock, irqflags);

	/* We need to determine after taking the lock, otherwise
	   we might disable just enabled interrupts again */
	if (amdgpu_irq_enabled(adev, src, type))
		state = AMDGPU_IRQ_STATE_ENABLE;
	else
		state = AMDGPU_IRQ_STATE_DISABLE;

	r = src->funcs->set(adev, src, type, state);
	spin_unlock_irqrestore(&adev->irq.lock, irqflags);
	return r;
}

/**
 * amdgpu_irq_gpu_reset_resume_helper - update interrupt states on all sources
 *
 * @adev: amdgpu device pointer
 *
 * Updates state of all types of interrupts on all sources on resume after
 * reset.
 */
void amdgpu_irq_gpu_reset_resume_helper(struct amdgpu_device *adev)
{
	int i, j, k;

	for (i = 0; i < AMDGPU_IH_CLIENTID_MAX; ++i) {
		if (!adev->irq.client[i].sources)
			continue;

		for (j = 0; j < AMDGPU_MAX_IRQ_SRC_ID; ++j) {
			struct amdgpu_irq_src *src = adev->irq.client[i].sources[j];

			if (!src)
				continue;
			for (k = 0; k < src->num_types; k++)
				amdgpu_irq_update(adev, src, k);
		}
	}
}

/**
 * amdgpu_irq_get - enable interrupt
 *
 * @adev: amdgpu device pointer
 * @src: interrupt source pointer
 * @type: type of interrupt
 *
 * Enables specified type of interrupt on the specified source (all ASICs).
 *
 * Returns:
 * 0 on success or error code otherwise
 */
int amdgpu_irq_get(struct amdgpu_device *adev, struct amdgpu_irq_src *src,
		   unsigned type)
{
	if (!adev->ddev->irq_enabled)
		return -ENOENT;

	if (type >= src->num_types)
		return -EINVAL;

	if (!src->enabled_types || !src->funcs->set)
		return -EINVAL;

	if (atomic_inc_return(&src->enabled_types[type]) == 1)
		return amdgpu_irq_update(adev, src, type);

	return 0;
}

/**
 * amdgpu_irq_put - disable interrupt
 *
 * @adev: amdgpu device pointer
 * @src: interrupt source pointer
 * @type: type of interrupt
 *
 * Enables specified type of interrupt on the specified source (all ASICs).
 *
 * Returns:
 * 0 on success or error code otherwise
 */
int amdgpu_irq_put(struct amdgpu_device *adev, struct amdgpu_irq_src *src,
		   unsigned type)
{
	if (!adev->ddev->irq_enabled)
		return -ENOENT;

	if (type >= src->num_types)
		return -EINVAL;

	if (!src->enabled_types || !src->funcs->set)
		return -EINVAL;

	if (atomic_dec_and_test(&src->enabled_types[type]))
		return amdgpu_irq_update(adev, src, type);

	return 0;
}

/**
 * amdgpu_irq_enabled - check whether interrupt is enabled or not
 *
 * @adev: amdgpu device pointer
 * @src: interrupt source pointer
 * @type: type of interrupt
 *
 * Checks whether the given type of interrupt is enabled on the given source.
 *
 * Returns:
 * *true* if interrupt is enabled, *false* if interrupt is disabled or on
 * invalid parameters
 */
bool amdgpu_irq_enabled(struct amdgpu_device *adev, struct amdgpu_irq_src *src,
			unsigned type)
{
	if (!adev->ddev->irq_enabled)
		return false;

	if (type >= src->num_types)
		return false;

	if (!src->enabled_types || !src->funcs->set)
		return false;

	return !!atomic_read(&src->enabled_types[type]);
}

#ifdef __linux__
/* XXX: Generic IRQ handling */
static void amdgpu_irq_mask(struct irq_data *irqd)
{
	/* XXX */
}

static void amdgpu_irq_unmask(struct irq_data *irqd)
{
	/* XXX */
}

/* amdgpu hardware interrupt chip descriptor */
static struct irq_chip amdgpu_irq_chip = {
	.name = "amdgpu-ih",
	.irq_mask = amdgpu_irq_mask,
	.irq_unmask = amdgpu_irq_unmask,
};
#endif

#ifdef __linux__
/**
 * amdgpu_irqdomain_map - create mapping between virtual and hardware IRQ numbers
 *
 * @d: amdgpu IRQ domain pointer (unused)
 * @irq: virtual IRQ number
 * @hwirq: hardware irq number
 *
 * Current implementation assigns simple interrupt handler to the given virtual
 * IRQ.
 *
 * Returns:
 * 0 on success or error code otherwise
 */
static int amdgpu_irqdomain_map(struct irq_domain *d,
				unsigned int irq, irq_hw_number_t hwirq)
{
	if (hwirq >= AMDGPU_MAX_IRQ_SRC_ID)
		return -EPERM;

	irq_set_chip_and_handler(irq,
				 &amdgpu_irq_chip, handle_simple_irq);
	return 0;
}

/* Implementation of methods for amdgpu IRQ domain */
static const struct irq_domain_ops amdgpu_hw_irqdomain_ops = {
	.map = amdgpu_irqdomain_map,
};
#endif

/**
 * amdgpu_irq_add_domain - create a linear IRQ domain
 *
 * @adev: amdgpu device pointer
 *
 * Creates an IRQ domain for GPU interrupt sources
 * that may be driven by another driver (e.g., ACP).
 *
 * Returns:
 * 0 on success or error code otherwise
 */
int amdgpu_irq_add_domain(struct amdgpu_device *adev)
{
	STUB();
	return 0;
#if 0
	adev->irq.domain = irq_domain_add_linear(NULL, AMDGPU_MAX_IRQ_SRC_ID,
						 &amdgpu_hw_irqdomain_ops, adev);
	if (!adev->irq.domain) {
		DRM_ERROR("GPU irq add domain failed\n");
		return -ENODEV;
	}

	return 0;
#endif
}

/**
 * amdgpu_irq_remove_domain - remove the IRQ domain
 *
 * @adev: amdgpu device pointer
 *
 * Removes the IRQ domain for GPU interrupt sources
 * that may be driven by another driver (e.g., ACP).
 */
void amdgpu_irq_remove_domain(struct amdgpu_device *adev)
{
	STUB();
#if 0
	if (adev->irq.domain) {
		irq_domain_remove(adev->irq.domain);
		adev->irq.domain = NULL;
	}
#endif
}

/**
 * amdgpu_irq_create_mapping - create mapping between domain Linux IRQs
 *
 * @adev: amdgpu device pointer
 * @src_id: IH source id
 *
 * Creates mapping between a domain IRQ (GPU IH src id) and a Linux IRQ
 * Use this for components that generate a GPU interrupt, but are driven
 * by a different driver (e.g., ACP).
 *
 * Returns:
 * Linux IRQ
 */
unsigned amdgpu_irq_create_mapping(struct amdgpu_device *adev, unsigned src_id)
{
	STUB();
	return 0;
#if 0
	adev->irq.virq[src_id] = irq_create_mapping(adev->irq.domain, src_id);

	return adev->irq.virq[src_id];
#endif
}
