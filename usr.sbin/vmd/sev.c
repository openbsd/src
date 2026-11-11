/*	$OpenBSD: sev.c,v 1.11 2026/11/11 17:32:23 hshoexer Exp $	*/

/*
 * Copyright (c) 2023-2025 Hans-Joerg Hoexer <hshoexer@genua.de>
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

#include <sys/types.h>
#include <sys/param.h>	/* roundup */

#include <crypto/xform.h>
#include <dev/ic/pspvar.h>

#include <stdlib.h>
#include <string.h>

#include "vmd.h"

extern struct vmd_vm	*current_vm;

/*
 * Prepare guest to use SEV.
 *
 * This asks the PSP to create a new crypto context including a
 * memory encryption key and assign a handle to the context.
 *
 * When the PSP driver psp(4) attaches, it initializes the platform.
 * If this fails for whatever reason we can not run a guest using SEV.
 */
int
sev_init(struct vmd_vm *vm)
{
	struct vmop_create_params *vmc = &vm->vm_params;
	uint32_t		 handle;
	uint16_t		 pstate;
	uint8_t			 gstate;

	if (!vmc->vmc_sev)
		return (0);

	if (psp_get_pstate(&pstate, NULL, NULL, NULL, NULL)) {
		log_warnx("%s: failed to get platform state", __func__);
		return (-1);
	}
	if (pstate == PSP_PSTATE_UNINIT) {
		log_warnx("%s: platform uninitialized", __func__);
		return (-1);
	}

	if (psp_launch_start(&handle, vmc->vmc_seves) < 0) {
		log_warnx("%s: launch failed", __func__);
		return (-1);
	}
	vm->vm_sev_handle = handle;

	if (psp_get_gstate(vm->vm_sev_handle, NULL, NULL, &gstate)) {
		log_warnx("%s: failed to get guest state", __func__);
		return (-1);
	}
	if (gstate != PSP_GSTATE_LUPDATE) {
		log_warnx("%s: invalid guest state: 0x%hx", __func__, gstate);
		return (-1);
	}

	return (0);
}

/*
 * Record memory segments to be encrypted for SEV.
 */
int
sev_register_encryption(paddr_t gpa, size_t size)
{
	struct vmop_create_params *vmc;
	void			*hva;
	int			 i;

	vmc = &current_vm->vm_params;

	if (!vmc->vmc_sev)
		return (0);

	if (size == 0)
		return (0);

	if (size > SSIZE_MAX) {
		log_warnx("%s: invalid segment gpa 0x%lx size 0x%zx", __func__,
		    gpa, size);
		return (-1);
	}

	/* Adjust address and size to be aligned to AES_XTS_BLOCKSIZE. */
	size += (gpa & (AES_XTS_BLOCKSIZE - 1));
	gpa &= ~(AES_XTS_BLOCKSIZE - 1);
	size = roundup(size, AES_XTS_BLOCKSIZE);

	/* The PSP takes one host VA range, so stay within one memrange. */
	hva = hvaddr_mem(gpa, size);
	if (hva == NULL)
		return (-1);

	if (current_vm->vm_sev_nmemsegments ==
	    nitems(current_vm->vm_sev_memsegments)) {
		log_warnx("%s: failed - out of SEV memory segments", __func__);
		return (-1);
	}
	i = current_vm->vm_sev_nmemsegments++;

	current_vm->vm_sev_memsegments[i].vmr_va = (vaddr_t)hva;
	current_vm->vm_sev_memsegments[i].vmr_size = size;
	current_vm->vm_sev_memsegments[i].vmr_gpa = gpa;

	log_debug("%s: i %d gpa 0x%lx size 0x%lx vmr_va 0x%lx vmr_gpa 0x%lx "
	    "vmr_size 0x%lx", __func__, i, gpa, size,
	    current_vm->vm_sev_memsegments[i].vmr_va,
	    current_vm->vm_sev_memsegments[i].vmr_gpa,
	    current_vm->vm_sev_memsegments[i].vmr_size);

	return (0);
}

static int
memseg_cmp(const void *a, const void *b)
{
	const struct vm_mem_range	*vmra = a;
	const struct vm_mem_range	*vmrb = b;

	return vmra->vmr_gpa < vmrb->vmr_gpa ? -1 :
	    vmra->vmr_gpa > vmrb->vmr_gpa;
}

/*
 * Encrypt and measure previously recorded memory segments.
 *
 * This encrypts the memory initially used by the guest.  This
 * includes the kernel or BIOS image, initial stack, boot arguments
 * and page tables.
 *
 * We also ask the PSP to provide a measurement.  However, right
 * now we can not really verify it.
 */
int
sev_encrypt_memory(struct vmd_vm *vm)
{
	struct vmop_create_params *vmc = &vm->vm_params;
	struct vm_mem_range	*vmr, *nvmr;
	paddr_t			 end;
	size_t			 i;

	if (!vmc->vmc_sev)
		return (0);

	/* Sort segments low to high. */
	qsort(vm->vm_sev_memsegments, vm->vm_sev_nmemsegments,
	    sizeof(vm->vm_sev_memsegments[0]), memseg_cmp);

	/* Merge overlapping segments; these always share one memrange. */
	i = 0;
	while (i + 1 < vm->vm_sev_nmemsegments) {
		vmr = &vm->vm_sev_memsegments[i];
		nvmr = vmr + 1;

		log_debug("%s: %zu gpa [0x%lx 0x%lx] ngpa [0x%lx 0x%lx]",
		    __func__, i,
		    vmr->vmr_gpa, vmr->vmr_gpa + vmr->vmr_size,
		    nvmr->vmr_gpa, nvmr->vmr_gpa + nvmr->vmr_size);

		if (vmr->vmr_gpa + vmr->vmr_size <= nvmr->vmr_gpa) {
			i++;
			continue;
		}

		end = MAX(vmr->vmr_gpa + vmr->vmr_size,
		    nvmr->vmr_gpa + nvmr->vmr_size);
		vmr->vmr_size = end - vmr->vmr_gpa;

		memmove(nvmr, nvmr + 1,
		    (vm->vm_sev_nmemsegments - i - 2) * sizeof(*nvmr));
		vm->vm_sev_nmemsegments--;
	}

	for (i = 0; i < vm->vm_sev_nmemsegments; i++) {
		vmr = &vm->vm_sev_memsegments[i];

		/* tell PSP to encrypt this range */
		if (psp_launch_update(vm->vm_sev_handle, vmr->vmr_va,
		    vmr->vmr_size)) {
			log_warnx("%s: failed to launch update page "
			    "%zu:0x%lx", __func__, i, vmr->vmr_va);
			return (-1);
		}

		log_debug("%s: encrypted %zu:0x%lx size 0x%lx", __func__, i,
		    vmr->vmr_va, vmr->vmr_size);
	}

	return (0);
}


/*
 * Activate a guest's SEV crypto state.
 */
int
sev_activate(struct vmd_vm *vm, int vcpu_id)
{
	struct vmop_create_params *vmc = &vm->vm_params;
	uint8_t			 gstate;

	if (!vmc->vmc_sev)
		return (0);

	if (psp_df_flush() ||
	    psp_activate(vm->vm_sev_handle, vm->vm_sev_asid[vcpu_id])) {
		log_warnx("%s: failed to activate guest: 0x%x:0x%x", __func__,
		    vm->vm_sev_handle, vm->vm_sev_asid[vcpu_id]);
		return (-1);
	}

	if (psp_get_gstate(vm->vm_sev_handle, NULL, NULL, &gstate)) {
		log_warnx("%s: failed to get guest state", __func__);
		return (-1);
	}
	if (gstate != PSP_GSTATE_LUPDATE) {
		log_warnx("%s: invalid guest state: 0x%hx", __func__, gstate);
		return (-1);
	}

	return (0);
}


int
sev_encrypt_state(struct vmd_vm *vm, int vcpu_id)
{
	struct vmop_create_params *vmc = &vm->vm_params;

	if (!vmc->vmc_seves)
		return (0);

	if (psp_encrypt_state(vm->vm_sev_handle, vm->vm_sev_asid[vcpu_id],
	    vm->vm_fd, vcpu_id)) {
		log_warnx("%s: failed to encrypt state: 0x%x 0x%x 0x%0x 0x%0x",
		    __func__, vm->vm_sev_handle, vm->vm_sev_asid[vcpu_id],
		    vm->vm_vmid, vcpu_id);
		return (-1);
	}

	return (0);
}

int
sev_launch_finalize(struct vmd_vm *vm)
{
	struct vmop_create_params *vmc = &vm->vm_params;
	uint8_t		gstate;

	if (!vmc->vmc_sev)
		return (0);

	if (psp_launch_measure(vm->vm_sev_handle)) {
		log_warnx("%s: failed to launch measure", __func__);
		return (-1);
	}
	if (psp_launch_finish(vm->vm_sev_handle)) {
		log_warnx("%s: failed to launch finish", __func__);
		return (-1);
	}

	if (psp_get_gstate(vm->vm_sev_handle, NULL, NULL, &gstate)) {
		log_warnx("%s: failed to get guest state", __func__);
		return (-1);
	}
	if (gstate != PSP_GSTATE_RUNNING) {
		log_warnx("%s: invalid guest state: 0x%hx", __func__, gstate);
		return (-1);
	}

	return (0);
}

/*
 * Deactivate and decommission a guest's SEV crypto state.
 */
int
sev_shutdown(struct vmd_vm *vm)
{
	struct vmop_create_params *vmc = &vm->vm_params;

	if (!vmc->vmc_sev)
		return (0);

	if (psp_guest_shutdown(vm->vm_sev_handle)) {
		log_warnx("failed to deactivate guest");
		return (-1);
	}
	vm->vm_sev_handle = 0;

	return (0);
}
