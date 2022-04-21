/*	$OpenBSD: vcpu.c,v 1.2 2022/04/21 19:21:05 bluhm Exp $	*/

/*
 * Copyright (c) 2022 Dave Voutila <dv@openbsd.org>
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
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <machine/specialreg.h>
#include <machine/vmmvar.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define KIB		1024
#define MIB		(1 << 20)
#define VMM_NODE	"/dev/vmm"

const char 		*VM_NAME = "regress";

/* Originally from vmd(8)'s vm.c */
const struct vcpu_reg_state vcpu_init_flat16 = {
	.vrs_gprs[VCPU_REGS_RFLAGS] = 0x2,
	.vrs_gprs[VCPU_REGS_RIP] = 0xFFF0,
	.vrs_gprs[VCPU_REGS_RSP] = 0x0,
	.vrs_crs[VCPU_REGS_CR0] = 0x60000010,
	.vrs_crs[VCPU_REGS_CR3] = 0,
	.vrs_sregs[VCPU_REGS_CS] = { 0xF000, 0xFFFF, 0x809F, 0xF0000},
	.vrs_sregs[VCPU_REGS_DS] = { 0x0, 0xFFFF, 0x8093, 0x0},
	.vrs_sregs[VCPU_REGS_ES] = { 0x0, 0xFFFF, 0x8093, 0x0},
	.vrs_sregs[VCPU_REGS_FS] = { 0x0, 0xFFFF, 0x8093, 0x0},
	.vrs_sregs[VCPU_REGS_GS] = { 0x0, 0xFFFF, 0x8093, 0x0},
	.vrs_sregs[VCPU_REGS_SS] = { 0x0, 0xFFFF, 0x8093, 0x0},
	.vrs_gdtr = { 0x0, 0xFFFF, 0x0, 0x0},
	.vrs_idtr = { 0x0, 0xFFFF, 0x0, 0x0},
	.vrs_sregs[VCPU_REGS_LDTR] = { 0x0, 0xFFFF, 0x0082, 0x0},
	.vrs_sregs[VCPU_REGS_TR] = { 0x0, 0xFFFF, 0x008B, 0x0},
	.vrs_msrs[VCPU_REGS_EFER] = 0ULL,
	.vrs_drs[VCPU_REGS_DR0] = 0x0,
	.vrs_drs[VCPU_REGS_DR1] = 0x0,
	.vrs_drs[VCPU_REGS_DR2] = 0x0,
	.vrs_drs[VCPU_REGS_DR3] = 0x0,
	.vrs_drs[VCPU_REGS_DR6] = 0xFFFF0FF0,
	.vrs_drs[VCPU_REGS_DR7] = 0x400,
	.vrs_msrs[VCPU_REGS_STAR] = 0ULL,
	.vrs_msrs[VCPU_REGS_LSTAR] = 0ULL,
	.vrs_msrs[VCPU_REGS_CSTAR] = 0ULL,
	.vrs_msrs[VCPU_REGS_SFMASK] = 0ULL,
	.vrs_msrs[VCPU_REGS_KGSBASE] = 0ULL,
	.vrs_crs[VCPU_REGS_XCR0] = XCR0_X87
};

int
main(int argc, char **argv)
{
	struct vm_create_params		 vcp;
	struct vm_exit			*exit = NULL;
	struct vm_info_params		 vip;
	struct vm_info_result		*info = NULL, *ours = NULL;
	struct vm_resetcpu_params	 vresetp;
	struct vm_run_params		 vrunp;
	struct vm_terminate_params	 vtp;

	struct vm_mem_range		*vmr;
	int				 fd, ret = 1;
	size_t				 i, j;
	void				*p;

	fd = open(VMM_NODE, O_RDWR);
	if (fd == -1)
		err(1, "open %s", VMM_NODE);

	/*
	 * 1. Create our VM with 1 vcpu and 2 MiB of memory.
	 */
	memset(&vcp, 0, sizeof(vcp));
	strlcpy(vcp.vcp_name, VM_NAME, sizeof(vcp.vcp_name));
	vcp.vcp_ncpus = 1;

	/* Split into two ranges, similar to how vmd(8) might do it. */
	vcp.vcp_nmemranges = 2;
	vcp.vcp_memranges[0].vmr_gpa = 0x0;
	vcp.vcp_memranges[0].vmr_size = 640 * KIB;
	vcp.vcp_memranges[1].vmr_gpa = 640 * KIB;
	vcp.vcp_memranges[1].vmr_size = (2 * MIB) - (640 * KIB);

	/* Allocate memory. */
	for (i = 0; i < vcp.vcp_nmemranges; i++) {
		vmr = &vcp.vcp_memranges[i];
		p = mmap(NULL, vmr->vmr_size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANON, -1, 0);
		if (p == MAP_FAILED)
			err(1, "mmap");

		/*
		 * Fill with 2-byte IN instructions that read from what would
		 * be an ancient XT PC Keyboard status port. These reads will
		 * trigger vm exits.
		 */
		if (vmr->vmr_size % 2 != 0)
			errx(1, "memory ranges must be multiple of 2");
		for (j = 0; j < vmr->vmr_size; j += 2) {
			((uint8_t*)p)[j + 0] = 0xE4;
			((uint8_t*)p)[j + 1] = PCKBC_AUX;
		}
		vmr->vmr_va = (vaddr_t)p;
		printf("mapped region %zu: { gpa: 0x%08lx, size: %lu }\n",
		    i, vmr->vmr_gpa, vmr->vmr_size);
	}

	if (ioctl(fd, VMM_IOC_CREATE, &vcp) == -1)
		err(1, "VMM_IOC_CREATE");
	printf("created vm %d named \"%s\"\n", vcp.vcp_id, vcp.vcp_name);

	/*
	 * 2. Check that our VM exists.
	 */
	memset(&vip, 0, sizeof(vip));
	vip.vip_size = 0;
	info = NULL;

	if (ioctl(fd, VMM_IOC_INFO, &vip) == -1) {
		warn("VMM_IOC_INFO(1)");
		goto out;
	}

	if (vip.vip_size == 0) {
		warn("no vms found");
		goto out;
	}

	info = malloc(vip.vip_size);
	if (info == NULL) {
		warn("malloc");
		goto out;
	}

	/* Second request that retrieves the VMs. */
	vip.vip_info = info;
	if (ioctl(fd, VMM_IOC_INFO, &vip) == -1) {
		warn("VMM_IOC_INFO(2)");
		goto out;
	}

	for (i = 0; i * sizeof(*info) < vip.vip_size; i++) {
		if (info[i].vir_id == vcp.vcp_id) {
			ours = &info[i];
			break;
		}
	}
	if (ours == NULL) {
		warn("failed to find vm %uz\n", vcp.vcp_id);
		goto out;
	}

	if (ours->vir_id != vcp.vcp_id) {
		warnx("expected vm id %uz, got %uz", vcp.vcp_id, ours->vir_id);
		goto out;
	}
	if (strncmp(ours->vir_name, VM_NAME, strlen(VM_NAME)) != 0) {
		warnx("expected vm name \"%s\", got \"%s\"", VM_NAME,
		    ours->vir_name);
		goto out;
	}
	printf("found vm %d named \"%s\"\n", vcp.vcp_id, ours->vir_name);
	ours = NULL;

	/*
	 * 3. Reset our VCPU and initialize register state.
	 */
	memset(&vresetp, 0, sizeof(vresetp));
	vresetp.vrp_vm_id = vcp.vcp_id;
	vresetp.vrp_vcpu_id = 0;	/* XXX SP */
	memcpy(&vresetp.vrp_init_state, &vcpu_init_flat16,
	    sizeof(vcpu_init_flat16));

	if (ioctl(fd, VMM_IOC_RESETCPU, &vresetp) == -1) {
		warn("VMM_IOC_RESETCPU");
		goto out;
	}
	printf("reset vcpu %d for vm %d\n", vresetp.vrp_vcpu_id,
	    vresetp.vrp_vm_id);

	/*
	 * 4. Run the vcpu, expecting an immediate exit for IO assist.
	 */
	exit = malloc(sizeof(*exit));
	if (exit == NULL) {
		warn("failed to allocate memory for vm_exit");
		goto out;
	}

	memset(&vrunp, 0, sizeof(vrunp));
	vrunp.vrp_exit = exit;
	vrunp.vrp_vcpu_id = 0;		/* XXX SP */
	vrunp.vrp_vm_id = vcp.vcp_id;
	vrunp.vrp_irq = 0x0;
	vrunp.vrp_irqready = 1;

	if (ioctl(fd, VMM_IOC_RUN, &vrunp) == -1) {
		warn("VMM_IOC_RUN");
		goto out;
	}

	if (vrunp.vrp_vm_id != vcp.vcp_id) {
		warnx("expected vm id %uz, got %uz\n", vcp.vcp_id,
		    vrunp.vrp_vm_id);
		goto out;
	}

	switch (vrunp.vrp_exit_reason) {
	case SVM_VMEXIT_IOIO:
	case VMX_EXIT_IO:
		printf("vcpu %d on vm %d exited for io assist\n",
		    vrunp.vrp_vcpu_id, vrunp.vrp_vm_id);
		break;
	default:
		warnx("unexpected vm exit reason: 0%04x",
		    vrunp.vrp_exit_reason);
		goto out;
	}

	exit = vrunp.vrp_exit;
	if (exit->vei.vei_port != PCKBC_AUX) {
		warnx("expected io port to be PCKBC_AUX, got 0x%02x",
		    exit->vei.vei_port);
		goto out;
	}

	/*
	 * If we made it here, we're close to passing. Any failures during
	 * cleanup will reset ret back to non-zero.
	 */
	ret = 0;

out:
	/*
	 * 5. Terminate our VM and clean up.
	 */
	memset(&vtp, 0, sizeof(vtp));
	vtp.vtp_vm_id = vcp.vcp_id;
	if (ioctl(fd, VMM_IOC_TERM, &vtp) == -1) {
		warn("VMM_IOC_TERM");
		ret = 1;
	} else
		printf("terminated vm %d\n", vtp.vtp_vm_id);

	close(fd);
	free(info);
	free(exit);

	/* Unmap memory. */
	for (i = 0; i < vcp.vcp_nmemranges; i++) {
		vmr = &vcp.vcp_memranges[i];
		if (vmr->vmr_va) {
			if (munmap((void *)vmr->vmr_va, vmr->vmr_size)) {
				warn("failed to unmap region %zu at 0x%08lx",
				    i, vmr->vmr_va);
				ret = 1;
			} else
				printf("unmapped region %zu @ gpa 0x%08lx\n",
				    i, vmr->vmr_gpa);
		}
	}

	return (ret);
}
