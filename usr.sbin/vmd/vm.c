/*	$OpenBSD: vm.c,v 1.43 2018/12/10 21:30:33 claudio Exp $	*/

/*
 * Copyright (c) 2015 Mike Larkin <mlarkin@openbsd.org>
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
#include <sys/queue.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/mman.h>

#include <dev/ic/i8253reg.h>
#include <dev/isa/isareg.h>
#include <dev/pci/pcireg.h>

#include <machine/param.h>
#include <machine/psl.h>
#include <machine/pte.h>
#include <machine/specialreg.h>
#include <machine/vmmvar.h>

#include <net/if.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "vmd.h"
#include "vmm.h"
#include "loadfile.h"
#include "pci.h"
#include "virtio.h"
#include "proc.h"
#include "i8253.h"
#include "i8259.h"
#include "ns8250.h"
#include "mc146818.h"
#include "fw_cfg.h"
#include "atomicio.h"

io_fn_t ioports_map[MAX_PORTS];

int run_vm(int, int[][VM_MAX_BASE_PER_DISK], int *,
    struct vmop_create_params *, struct vcpu_reg_state *);
void vm_dispatch_vmm(int, short, void *);
void *event_thread(void *);
void *vcpu_run_loop(void *);
int vcpu_exit(struct vm_run_params *);
int vcpu_reset(uint32_t, uint32_t, struct vcpu_reg_state *);
void create_memory_map(struct vm_create_params *);
int alloc_guest_mem(struct vm_create_params *);
int vmm_create_vm(struct vm_create_params *);
void init_emulated_hw(struct vmop_create_params *, int,
    int[][VM_MAX_BASE_PER_DISK], int *);
void restore_emulated_hw(struct vm_create_params *, int, int *,
    int[][VM_MAX_BASE_PER_DISK],int);
void vcpu_exit_inout(struct vm_run_params *);
uint8_t vcpu_exit_pci(struct vm_run_params *);
int vcpu_pic_intr(uint32_t, uint32_t, uint8_t);
int loadfile_bios(FILE *, struct vcpu_reg_state *);
int send_vm(int, struct vm_create_params *);
int dump_send_header(int);
int dump_vmr(int , struct vm_mem_range *);
int dump_mem(int, struct vm_create_params *);
void restore_vmr(int, struct vm_mem_range *);
void restore_mem(int, struct vm_create_params *);
void pause_vm(struct vm_create_params *);
void unpause_vm(struct vm_create_params *);

static struct vm_mem_range *find_gpa_range(struct vm_create_params *, paddr_t,
    size_t);

int con_fd;
struct vmd_vm *current_vm;

extern struct vmd *env;

extern char *__progname;

pthread_mutex_t threadmutex;
pthread_cond_t threadcond;

pthread_cond_t vcpu_run_cond[VMM_MAX_VCPUS_PER_VM];
pthread_mutex_t vcpu_run_mtx[VMM_MAX_VCPUS_PER_VM];
uint8_t vcpu_hlt[VMM_MAX_VCPUS_PER_VM];
uint8_t vcpu_done[VMM_MAX_VCPUS_PER_VM];

/*
 * Represents a standard register set for an OS to be booted
 * as a flat 64 bit address space.
 *
 * NOT set here are:
 *  RIP
 *  RSP
 *  GDTR BASE
 *
 * Specific bootloaders should clone this structure and override
 * those fields as needed.
 *
 * Note - CR3 and various bits in CR0 may be overridden by vmm(4) based on
 *        features of the CPU in use.
 */
static const struct vcpu_reg_state vcpu_init_flat64 = {
#ifdef __i386__
	.vrs_gprs[VCPU_REGS_EFLAGS] = 0x2,
	.vrs_gprs[VCPU_REGS_EIP] = 0x0,
	.vrs_gprs[VCPU_REGS_ESP] = 0x0,
#else
	.vrs_gprs[VCPU_REGS_RFLAGS] = 0x2,
	.vrs_gprs[VCPU_REGS_RIP] = 0x0,
	.vrs_gprs[VCPU_REGS_RSP] = 0x0,
#endif
	.vrs_crs[VCPU_REGS_CR0] = CR0_CD | CR0_NW | CR0_ET | CR0_PE | CR0_PG,
	.vrs_crs[VCPU_REGS_CR3] = PML4_PAGE,
	.vrs_crs[VCPU_REGS_CR4] = CR4_PAE | CR4_PSE,
	.vrs_crs[VCPU_REGS_PDPTE0] = 0ULL,
	.vrs_crs[VCPU_REGS_PDPTE1] = 0ULL,
	.vrs_crs[VCPU_REGS_PDPTE2] = 0ULL,
	.vrs_crs[VCPU_REGS_PDPTE3] = 0ULL,
	.vrs_sregs[VCPU_REGS_CS] = { 0x8, 0xFFFFFFFF, 0xC09F, 0x0},
	.vrs_sregs[VCPU_REGS_DS] = { 0x10, 0xFFFFFFFF, 0xC093, 0x0},
	.vrs_sregs[VCPU_REGS_ES] = { 0x10, 0xFFFFFFFF, 0xC093, 0x0},
	.vrs_sregs[VCPU_REGS_FS] = { 0x10, 0xFFFFFFFF, 0xC093, 0x0},
	.vrs_sregs[VCPU_REGS_GS] = { 0x10, 0xFFFFFFFF, 0xC093, 0x0},
	.vrs_sregs[VCPU_REGS_SS] = { 0x10, 0xFFFFFFFF, 0xC093, 0x0},
	.vrs_gdtr = { 0x0, 0xFFFF, 0x0, 0x0},
	.vrs_idtr = { 0x0, 0xFFFF, 0x0, 0x0},
	.vrs_sregs[VCPU_REGS_LDTR] = { 0x0, 0xFFFF, 0x0082, 0x0},
	.vrs_sregs[VCPU_REGS_TR] = { 0x0, 0xFFFF, 0x008B, 0x0},
	.vrs_msrs[VCPU_REGS_EFER] = EFER_LME | EFER_LMA,
#ifndef __i386__
	.vrs_msrs[VCPU_REGS_STAR] = 0ULL,
	.vrs_msrs[VCPU_REGS_LSTAR] = 0ULL,
	.vrs_msrs[VCPU_REGS_CSTAR] = 0ULL,
	.vrs_msrs[VCPU_REGS_SFMASK] = 0ULL,
	.vrs_msrs[VCPU_REGS_KGSBASE] = 0ULL,
	.vrs_msrs[VCPU_REGS_MISC_ENABLE] = 0ULL,
	.vrs_crs[VCPU_REGS_XCR0] = XCR0_X87
#endif
};

/*
 * Represents a standard register set for an BIOS to be booted
 * as a flat 16 bit address space.
 */
static const struct vcpu_reg_state vcpu_init_flat16 = {
#ifdef __i386__
	.vrs_gprs[VCPU_REGS_EFLAGS] = 0x2,
	.vrs_gprs[VCPU_REGS_EIP] = 0xFFF0,
	.vrs_gprs[VCPU_REGS_ESP] = 0x0,
#else
	.vrs_gprs[VCPU_REGS_RFLAGS] = 0x2,
	.vrs_gprs[VCPU_REGS_RIP] = 0xFFF0,
	.vrs_gprs[VCPU_REGS_RSP] = 0x0,
#endif
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
#ifndef __i386__
	.vrs_msrs[VCPU_REGS_STAR] = 0ULL,
	.vrs_msrs[VCPU_REGS_LSTAR] = 0ULL,
	.vrs_msrs[VCPU_REGS_CSTAR] = 0ULL,
	.vrs_msrs[VCPU_REGS_SFMASK] = 0ULL,
	.vrs_msrs[VCPU_REGS_KGSBASE] = 0ULL,
	.vrs_crs[VCPU_REGS_XCR0] = XCR0_X87
#endif
};

/*
 * loadfile_bios
 *
 * Alternatively to loadfile_elf, this function loads a non-ELF BIOS image
 * directly into memory.
 *
 * Parameters:
 *  fp: file of a kernel file to load
 *  (out) vrs: register state to set on init for this kernel
 *
 * Return values:
 *  0 if successful
 *  various error codes returned from read(2) or loadelf functions
 */
int
loadfile_bios(FILE *fp, struct vcpu_reg_state *vrs)
{
	off_t	 size, off;

	/* Set up a "flat 16 bit" register state for BIOS */
	memcpy(vrs, &vcpu_init_flat16, sizeof(*vrs));

	/* Get the size of the BIOS image and seek to the beginning */
	if (fseeko(fp, 0, SEEK_END) == -1 || (size = ftello(fp)) == -1 ||
	    fseeko(fp, 0, SEEK_SET) == -1)
		return (-1);

	/* The BIOS image must end at 1M */
	if ((off = 1048576 - size) < 0)
		return (-1);

	/* Read BIOS image into memory */
	if (mread(fp, off, size) != (size_t)size) {
		errno = EIO;
		return (-1);
	}

	log_debug("%s: loaded BIOS image", __func__);

	return (0);
}

/*
 * start_vm
 *
 * After forking a new VM process, starts the new VM with the creation
 * parameters supplied (in the incoming vm->vm_params field). This
 * function performs a basic sanity check on the incoming parameters
 * and then performs the following steps to complete the creation of the VM:
 *
 * 1. validates and create the new VM
 * 2. opens the imsg control channel to the parent and drops more privilege
 * 3. drops additional privleges by calling pledge(2)
 * 4. loads the kernel from the disk image or file descriptor
 * 5. runs the VM's VCPU loops.
 *
 * Parameters:
 *  vm: The VM data structure that is including the VM create parameters.
 *  fd: The imsg socket that is connected to the parent process.
 *
 * Return values:
 *  0: success
 *  !0 : failure - typically an errno indicating the source of the failure
 */
int
start_vm(struct vmd_vm *vm, int fd)
{
	struct vmop_create_params *vmc = &vm->vm_params;
	struct vm_create_params	*vcp = &vmc->vmc_params;
	struct vcpu_reg_state	 vrs;
	int			 nicfds[VMM_MAX_NICS_PER_VM];
	int			 ret;
	FILE			*fp;
	struct vmboot_params	 vmboot;
	size_t			 i;
	struct vm_rwregs_params  vrp;

	/* Child */
	setproctitle("%s", vcp->vcp_name);
	log_procinit(vcp->vcp_name);

	if (!vm->vm_received)
		create_memory_map(vcp);

	ret = alloc_guest_mem(vcp);

	if (ret) {
		errno = ret;
		fatal("could not allocate guest memory - exiting");
	}

	ret = vmm_create_vm(vcp);
	current_vm = vm;

	/* send back the kernel-generated vm id (0 on error) */
	if (write(fd, &vcp->vcp_id, sizeof(vcp->vcp_id)) !=
	    sizeof(vcp->vcp_id))
		fatal("write vcp id");

	if (ret) {
		errno = ret;
		fatal("create vmm ioctl failed - exiting");
	}

	/*
	 * pledge in the vm processes:
	 * stdio - for malloc and basic I/O including events.
	 * recvfd - for send/recv.
	 * vmm - for the vmm ioctls and operations.
	 */
	if (pledge("stdio vmm recvfd", NULL) == -1)
		fatal("pledge");

	if (vm->vm_received) {
		ret = read(vm->vm_receive_fd, &vrp, sizeof(vrp));
		if (ret != sizeof(vrp)) {
			fatal("received incomplete vrp - exiting");
		}
		vrs = vrp.vrwp_regs;
	} else {
		/*
		 * Set up default "flat 64 bit" register state - RIP,
		 * RSP, and GDT info will be set in bootloader
		 */
		memcpy(&vrs, &vcpu_init_flat64, sizeof(vrs));

		/* Find and open kernel image */
		if ((fp = vmboot_open(vm->vm_kernel,
		    vm->vm_disks[0], vmc->vmc_diskbases[0],
		    vmc->vmc_disktypes[0], &vmboot)) == NULL)
			fatalx("failed to open kernel - exiting");

		/* Load kernel image */
		ret = loadfile_elf(fp, vcp, &vrs,
		    vmboot.vbp_bootdev, vmboot.vbp_howto, vmc->vmc_bootdevice);

		/*
		 * Try BIOS as a fallback (only if it was provided as an image
		 * with vm->vm_kernel and not loaded from the disk)
		 */
		if (ret && errno == ENOEXEC && vm->vm_kernel != -1)
			ret = loadfile_bios(fp, &vrs);

		if (ret)
			fatal("failed to load kernel or BIOS - exiting");

		vmboot_close(fp, &vmboot);
	}

	if (vm->vm_kernel != -1)
		close(vm->vm_kernel);

	con_fd = vm->vm_tty;
	if (fcntl(con_fd, F_SETFL, O_NONBLOCK) == -1)
		fatal("failed to set nonblocking mode on console");

	for (i = 0; i < VMM_MAX_NICS_PER_VM; i++)
		nicfds[i] = vm->vm_ifs[i].vif_fd;

	event_init();

	if (vm->vm_received) {
		restore_emulated_hw(vcp, vm->vm_receive_fd, nicfds,
		    vm->vm_disks, vm->vm_cdrom);
		mc146818_start();
		restore_mem(vm->vm_receive_fd, vcp);
	}

	if (vmm_pipe(vm, fd, vm_dispatch_vmm) == -1)
		fatal("setup vm pipe");

	/* Execute the vcpu run loop(s) for this VM */
	ret = run_vm(vm->vm_cdrom, vm->vm_disks, nicfds, &vm->vm_params, &vrs);

	/* Ensure that any in-flight data is written back */
	virtio_shutdown(vm);

	return (ret);
}

/*
 * vm_dispatch_vmm
 *
 * imsg callback for messages that are received from the vmm parent process.
 */
void
vm_dispatch_vmm(int fd, short event, void *arg)
{
	struct vmd_vm		*vm = arg;
	struct vmop_result	 vmr;
	struct imsgev		*iev = &vm->vm_iev;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct imsg		 imsg;
	ssize_t			 n;
	int			 verbose;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("%s: imsg_read", __func__);
		if (n == 0)
			_exit(0);
	}

	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("%s: msgbuf_write fd %d", __func__, ibuf->fd);
		if (n == 0)
			_exit(0);
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get", __func__);
		if (n == 0)
			break;

#if DEBUG > 1
		log_debug("%s: got imsg %d from %s",
		    __func__, imsg.hdr.type,
		    vm->vm_params.vmc_params.vcp_name);
#endif

		switch (imsg.hdr.type) {
		case IMSG_CTL_VERBOSE:
			IMSG_SIZE_CHECK(&imsg, &verbose);
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_setverbose(verbose);
			break;
		case IMSG_VMDOP_VM_SHUTDOWN:
			if (vmmci_ctl(VMMCI_SHUTDOWN) == -1)
				_exit(0);
			break;
		case IMSG_VMDOP_VM_REBOOT:
			if (vmmci_ctl(VMMCI_REBOOT) == -1)
				_exit(0);
			break;
		case IMSG_VMDOP_PAUSE_VM:
			vmr.vmr_result = 0;
			vmr.vmr_id = vm->vm_vmid;
			pause_vm(&vm->vm_params.vmc_params);
			imsg_compose_event(&vm->vm_iev,
			    IMSG_VMDOP_PAUSE_VM_RESPONSE,
			    imsg.hdr.peerid, imsg.hdr.pid, -1, &vmr,
			    sizeof(vmr));
			break;
		case IMSG_VMDOP_UNPAUSE_VM:
			vmr.vmr_result = 0;
			vmr.vmr_id = vm->vm_vmid;
			unpause_vm(&vm->vm_params.vmc_params);
			imsg_compose_event(&vm->vm_iev,
			    IMSG_VMDOP_UNPAUSE_VM_RESPONSE,
			    imsg.hdr.peerid, imsg.hdr.pid, -1, &vmr,
			    sizeof(vmr));
			break;
		case IMSG_VMDOP_SEND_VM_REQUEST:
			vmr.vmr_id = vm->vm_vmid;
			vmr.vmr_result = send_vm(imsg.fd,
			    &vm->vm_params.vmc_params);
			imsg_compose_event(&vm->vm_iev,
			    IMSG_VMDOP_SEND_VM_RESPONSE,
			    imsg.hdr.peerid, imsg.hdr.pid, -1, &vmr,
			    sizeof(vmr));
			break;
		default:
			fatalx("%s: got invalid imsg %d from %s",
			    __func__, imsg.hdr.type,
			    vm->vm_params.vmc_params.vcp_name);
		}
		imsg_free(&imsg);
	}
	imsg_event_add(iev);
}

/*
 * vm_ctl
 *
 * Tell the vmm parent process to shutdown or reboot the VM and exit.
 */
__dead void
vm_shutdown(unsigned int cmd)
{
	switch (cmd) {
	case VMMCI_NONE:
	case VMMCI_SHUTDOWN:
		(void)imsg_compose_event(&current_vm->vm_iev,
		    IMSG_VMDOP_VM_SHUTDOWN, 0, 0, -1, NULL, 0);
		break;
	case VMMCI_REBOOT:
		(void)imsg_compose_event(&current_vm->vm_iev,
		    IMSG_VMDOP_VM_REBOOT, 0, 0, -1, NULL, 0);
		break;
	default:
		fatalx("invalid vm ctl command: %d", cmd);
	}
	imsg_flush(&current_vm->vm_iev.ibuf);

	_exit(0);
}

int
send_vm(int fd, struct vm_create_params *vcp)
{
	struct vm_rwregs_params	   vrp;
	struct vmop_create_params *vmc;
	struct vm_terminate_params vtp;
	unsigned int		   flags = 0;
	unsigned int		   i;
	int			   ret = 0;
	size_t			   sz;

	if (dump_send_header(fd)) {
		log_info("%s: failed to send vm dump header", __func__);
		goto err;
	}

	pause_vm(vcp);

	vmc = calloc(1, sizeof(struct vmop_create_params));
	if (vmc == NULL) {
		log_warn("%s: calloc error geting vmc", __func__);
		ret = -1;
		goto err;
	}

	flags |= VMOP_CREATE_MEMORY;
	memcpy(&vmc->vmc_params, &current_vm->vm_params, sizeof(struct
	    vmop_create_params));
	vmc->vmc_flags = flags;
	vrp.vrwp_vm_id = vcp->vcp_id;
	vrp.vrwp_mask = VM_RWREGS_ALL;

	sz = atomicio(vwrite, fd, vmc,sizeof(struct vmop_create_params));
	if (sz != sizeof(struct vmop_create_params)) {
		ret = -1;
		goto err;
	}

	for (i = 0; i < vcp->vcp_ncpus; i++) {
		vrp.vrwp_vcpu_id = i;
		if ((ret = ioctl(env->vmd_fd, VMM_IOC_READREGS, &vrp))) {
			log_warn("%s: readregs failed", __func__);
			goto err;
		}

		sz = atomicio(vwrite, fd, &vrp,
		    sizeof(struct vm_rwregs_params));
		if (sz != sizeof(struct vm_rwregs_params)) {
			log_warn("%s: dumping registers failed", __func__);
			ret = -1;
			goto err;
		}
	}

	if ((ret = i8253_dump(fd)))
		goto err;
	if ((ret = i8259_dump(fd)))
		goto err;
	if ((ret = ns8250_dump(fd)))
		goto err;
	if ((ret = mc146818_dump(fd)))
		goto err;
	if ((ret = fw_cfg_dump(fd)))
		goto err;
	if ((ret = pci_dump(fd)))
		goto err;
	if ((ret = virtio_dump(fd)))
		goto err;
	if ((ret = dump_mem(fd, vcp)))
		goto err;

	vtp.vtp_vm_id = vcp->vcp_id;
	if (ioctl(env->vmd_fd, VMM_IOC_TERM, &vtp) < 0) {
		log_warnx("%s: term IOC error: %d, %d", __func__,
		    errno, ENOENT);
	}
err:
	close(fd);
	if (ret)
		unpause_vm(vcp);
	return ret;
}

int
dump_send_header(int fd) {
	struct vm_dump_header	   vmh;
	int			   i;

	vmh.vmh_cpuids[0].code = 0x00;
	vmh.vmh_cpuids[0].leaf = 0x00;

	vmh.vmh_cpuids[1].code = 0x01;
	vmh.vmh_cpuids[1].leaf = 0x00;

	vmh.vmh_cpuids[2].code = 0x07;
	vmh.vmh_cpuids[2].leaf = 0x00;

	vmh.vmh_cpuids[3].code = 0x0d;
	vmh.vmh_cpuids[3].leaf = 0x00;

	vmh.vmh_cpuids[4].code = 0x80000001;
	vmh.vmh_cpuids[4].leaf = 0x00;

	vmh.vmh_version = VM_DUMP_VERSION;

	for (i=0; i < VM_DUMP_HEADER_CPUID_COUNT; i++) {
		CPUID_LEAF(vmh.vmh_cpuids[i].code,
		    vmh.vmh_cpuids[i].leaf,
		    vmh.vmh_cpuids[i].a,
		    vmh.vmh_cpuids[i].b,
		    vmh.vmh_cpuids[i].c,
		    vmh.vmh_cpuids[i].d);
	}

	if (atomicio(vwrite, fd, &vmh, sizeof(vmh)) != sizeof(vmh))
		return (-1);

	return (0);
}

int
dump_mem(int fd, struct vm_create_params *vcp)
{
	unsigned int	i;
	int		ret;
	struct		vm_mem_range *vmr;

	for (i = 0; i < vcp->vcp_nmemranges; i++) {
		vmr = &vcp->vcp_memranges[i];
		ret = dump_vmr(fd, vmr);
		if (ret)
			return ret;
	}
	return (0);
}

void
restore_mem(int fd, struct vm_create_params *vcp)
{
	unsigned int	     i;
	struct vm_mem_range *vmr;

	for (i = 0; i < vcp->vcp_nmemranges; i++) {
		vmr = &vcp->vcp_memranges[i];
		restore_vmr(fd, vmr);
	}
}

int
dump_vmr(int fd, struct vm_mem_range *vmr)
{
	size_t	rem = vmr->vmr_size, read=0;
	char	buf[PAGE_SIZE];

	while (rem > 0) {
		if (read_mem(vmr->vmr_gpa + read, buf, PAGE_SIZE)) {
			log_warn("failed to read vmr");
			return (-1);
		}
		if (atomicio(vwrite, fd, buf, sizeof(buf)) != sizeof(buf)) {
			log_warn("failed to dump vmr");
			return (-1);
		}
		rem = rem - PAGE_SIZE;
		read = read + PAGE_SIZE;
	}
	return (0);
}

void
restore_vmr(int fd, struct vm_mem_range *vmr)
{
	size_t	rem = vmr->vmr_size, wrote=0;
	char	buf[PAGE_SIZE];

	while (rem > 0) {
		if (atomicio(read, fd, buf, sizeof(buf)) != sizeof(buf))
			fatal("failed to restore vmr");
		if (write_mem(vmr->vmr_gpa + wrote, buf, PAGE_SIZE))
			fatal("failed to write vmr");
		rem = rem - PAGE_SIZE;
		wrote = wrote + PAGE_SIZE;
	}
}

void
pause_vm(struct vm_create_params *vcp)
{
	if (current_vm->vm_paused)
		return;

	current_vm->vm_paused = 1;

	/* XXX: vcpu_run_loop is running in another thread and we have to wait
	 * for the vm to exit before returning */
	sleep(1);

	i8253_stop();
	mc146818_stop();
}

void
unpause_vm(struct vm_create_params *vcp)
{
	unsigned int n;
	if (!current_vm->vm_paused)
		return;

	current_vm->vm_paused = 0;

	i8253_start();
	mc146818_start();
	for (n = 0; n <= vcp->vcp_ncpus; n++)
		pthread_cond_broadcast(&vcpu_run_cond[n]);
}

/*
 * vcpu_reset
 *
 * Requests vmm(4) to reset the VCPUs in the indicated VM to
 * the register state provided
 *
 * Parameters
 *  vmid: VM ID to reset
 *  vcpu_id: VCPU ID to reset
 *  vrs: the register state to initialize
 *
 * Return values:
 *  0: success
 *  !0 : ioctl to vmm(4) failed (eg, ENOENT if the supplied VM ID is not
 *      valid)
 */
int
vcpu_reset(uint32_t vmid, uint32_t vcpu_id, struct vcpu_reg_state *vrs)
{
	struct vm_resetcpu_params vrp;

	memset(&vrp, 0, sizeof(vrp));
	vrp.vrp_vm_id = vmid;
	vrp.vrp_vcpu_id = vcpu_id;
	memcpy(&vrp.vrp_init_state, vrs, sizeof(struct vcpu_reg_state));

	log_debug("%s: resetting vcpu %d for vm %d", __func__, vcpu_id, vmid);

	if (ioctl(env->vmd_fd, VMM_IOC_RESETCPU, &vrp) < 0)
		return (errno);

	return (0);
}

/*
 * create_memory_map
 *
 * Sets up the guest physical memory ranges that the VM can access.
 *
 * Parameters:
 *  vcp: VM create parameters describing the VM whose memory map
 *       is being created
 *
 * Return values:
 *  nothing
 */
void
create_memory_map(struct vm_create_params *vcp)
{
	size_t len, mem_bytes, mem_mb;

	mem_mb = vcp->vcp_memranges[0].vmr_size;
	vcp->vcp_nmemranges = 0;
	if (mem_mb < 1 || mem_mb > VMM_MAX_VM_MEM_SIZE)
		return;

	mem_bytes = mem_mb * 1024 * 1024;

	/* First memory region: 0 - LOWMEM_KB (DOS low mem) */
	len = LOWMEM_KB * 1024;
	vcp->vcp_memranges[0].vmr_gpa = 0x0;
	vcp->vcp_memranges[0].vmr_size = len;
	mem_bytes -= len;

	/*
	 * Second memory region: LOWMEM_KB - 1MB.
	 *
	 * N.B. - Normally ROMs or parts of video RAM are mapped here.
	 * We have to add this region, because some systems
	 * unconditionally write to 0xb8000 (VGA RAM), and
	 * we need to make sure that vmm(4) permits accesses
	 * to it. So allocate guest memory for it.
	 */
	len = 0x100000 - LOWMEM_KB * 1024;
	vcp->vcp_memranges[1].vmr_gpa = LOWMEM_KB * 1024;
	vcp->vcp_memranges[1].vmr_size = len;
	mem_bytes -= len;

	/* Make sure that we do not place physical memory into MMIO ranges. */
	if (mem_bytes > VMM_PCI_MMIO_BAR_BASE - 0x100000)
		len = VMM_PCI_MMIO_BAR_BASE - 0x100000;
	else
		len = mem_bytes;

	/* Third memory region: 1MB - (1MB + len) */
	vcp->vcp_memranges[2].vmr_gpa = 0x100000;
	vcp->vcp_memranges[2].vmr_size = len;
	mem_bytes -= len;

	if (mem_bytes > 0) {
		/* Fourth memory region for the remaining memory (if any) */
		vcp->vcp_memranges[3].vmr_gpa = VMM_PCI_MMIO_BAR_END + 1;
		vcp->vcp_memranges[3].vmr_size = mem_bytes;
		vcp->vcp_nmemranges = 4;
	} else
		vcp->vcp_nmemranges = 3;
}

/*
 * alloc_guest_mem
 *
 * Allocates memory for the guest.
 * Instead of doing a single allocation with one mmap(), we allocate memory
 * separately for every range for the following reasons:
 * - ASLR for the individual ranges
 * - to reduce memory consumption in the UVM subsystem: if vmm(4) had to
 *   map the single mmap'd userspace memory to the individual guest physical
 *   memory ranges, the underlying amap of the single mmap'd range would have
 *   to allocate per-page reference counters. The reason is that the
 *   individual guest physical ranges would reference the single mmap'd region
 *   only partially. However, if every guest physical range has its own
 *   corresponding mmap'd userspace allocation, there are no partial
 *   references: every guest physical range fully references an mmap'd
 *   range => no per-page reference counters have to be allocated.
 *
 * Return values:
 *  0: success
 *  !0: failure - errno indicating the source of the failure
 */
int
alloc_guest_mem(struct vm_create_params *vcp)
{
	void *p;
	int ret;
	size_t i, j;
	struct vm_mem_range *vmr;

	for (i = 0; i < vcp->vcp_nmemranges; i++) {
		vmr = &vcp->vcp_memranges[i];
		p = mmap(NULL, vmr->vmr_size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANON, -1, 0);
		if (p == MAP_FAILED) {
			ret = errno;
			for (j = 0; j < i; j++) {
				vmr = &vcp->vcp_memranges[j];
				munmap((void *)vmr->vmr_va, vmr->vmr_size);
			}

			return (ret);
		}

		vmr->vmr_va = (vaddr_t)p;
	}

	return (0);
}

/*
 * vmm_create_vm
 *
 * Requests vmm(4) to create a new VM using the supplied creation
 * parameters. This operation results in the creation of the in-kernel
 * structures for the VM, but does not start the VM's vcpu(s).
 *
 * Parameters:
 *  vcp: vm_create_params struct containing the VM's desired creation
 *      configuration
 *
 * Return values:
 *  0: success
 *  !0 : ioctl to vmm(4) failed
 */
int
vmm_create_vm(struct vm_create_params *vcp)
{
	/* Sanity check arguments */
	if (vcp->vcp_ncpus > VMM_MAX_VCPUS_PER_VM)
		return (EINVAL);

	if (vcp->vcp_nmemranges == 0 ||
	    vcp->vcp_nmemranges > VMM_MAX_MEM_RANGES)
		return (EINVAL);

	if (vcp->vcp_ndisks > VMM_MAX_DISKS_PER_VM)
		return (EINVAL);

	if (vcp->vcp_nnics > VMM_MAX_NICS_PER_VM)
		return (EINVAL);

	if (ioctl(env->vmd_fd, VMM_IOC_CREATE, vcp) < 0)
		return (errno);

	return (0);
}

/*
 * init_emulated_hw
 *
 * Initializes the userspace hardware emulation
 */
void
init_emulated_hw(struct vmop_create_params *vmc, int child_cdrom,
    int child_disks[][VM_MAX_BASE_PER_DISK], int *child_taps)
{
	struct vm_create_params *vcp = &vmc->vmc_params;
	int i;
	uint64_t memlo, memhi;

	/* Calculate memory size for NVRAM registers */
	memlo = memhi = 0;
	if (vcp->vcp_nmemranges > 2)
		memlo = vcp->vcp_memranges[2].vmr_size - 15 * 0x100000;

	if (vcp->vcp_nmemranges > 3)
		memhi = vcp->vcp_memranges[3].vmr_size;

	/* Reset the IO port map */
	memset(&ioports_map, 0, sizeof(io_fn_t) * MAX_PORTS);

	/* Init i8253 PIT */
	i8253_init(vcp->vcp_id);
	ioports_map[TIMER_CTRL] = vcpu_exit_i8253;
	ioports_map[TIMER_BASE + TIMER_CNTR0] = vcpu_exit_i8253;
	ioports_map[TIMER_BASE + TIMER_CNTR1] = vcpu_exit_i8253;
	ioports_map[TIMER_BASE + TIMER_CNTR2] = vcpu_exit_i8253;
	ioports_map[PCKBC_AUX] = vcpu_exit_i8253_misc;

	/* Init mc146818 RTC */
	mc146818_init(vcp->vcp_id, memlo, memhi);
	ioports_map[IO_RTC] = vcpu_exit_mc146818;
	ioports_map[IO_RTC + 1] = vcpu_exit_mc146818;

	/* Init master and slave PICs */
	i8259_init();
	ioports_map[IO_ICU1] = vcpu_exit_i8259;
	ioports_map[IO_ICU1 + 1] = vcpu_exit_i8259;
	ioports_map[IO_ICU2] = vcpu_exit_i8259;
	ioports_map[IO_ICU2 + 1] = vcpu_exit_i8259;
	ioports_map[ELCR0] = vcpu_exit_elcr;
	ioports_map[ELCR1] = vcpu_exit_elcr;

	/* Init ns8250 UART */
	ns8250_init(con_fd, vcp->vcp_id);
	for (i = COM1_DATA; i <= COM1_SCR; i++)
		ioports_map[i] = vcpu_exit_com;

	/* Init QEMU fw_cfg interface */
	fw_cfg_init(vmc);
	ioports_map[FW_CFG_IO_SELECT] = vcpu_exit_fw_cfg;
	ioports_map[FW_CFG_IO_DATA] = vcpu_exit_fw_cfg;
	ioports_map[FW_CFG_IO_DMA_ADDR_HIGH] = vcpu_exit_fw_cfg_dma;
	ioports_map[FW_CFG_IO_DMA_ADDR_LOW] = vcpu_exit_fw_cfg_dma;

	/* Initialize PCI */
	for (i = VMM_PCI_IO_BAR_BASE; i <= VMM_PCI_IO_BAR_END; i++)
		ioports_map[i] = vcpu_exit_pci;

	ioports_map[PCI_MODE1_ADDRESS_REG] = vcpu_exit_pci;
	ioports_map[PCI_MODE1_DATA_REG] = vcpu_exit_pci;
	ioports_map[PCI_MODE1_DATA_REG + 1] = vcpu_exit_pci;
	ioports_map[PCI_MODE1_DATA_REG + 2] = vcpu_exit_pci;
	ioports_map[PCI_MODE1_DATA_REG + 3] = vcpu_exit_pci;
	pci_init();

	/* Initialize virtio devices */
	virtio_init(current_vm, child_cdrom, child_disks, child_taps);
}
/*
 * restore_emulated_hw
 *
 * Restores the userspace hardware emulation from fd
 */
void
restore_emulated_hw(struct vm_create_params *vcp, int fd,
    int *child_taps, int child_disks[][VM_MAX_BASE_PER_DISK], int child_cdrom)
{
	/* struct vm_create_params *vcp = &vmc->vmc_params; */
	int i;
	memset(&ioports_map, 0, sizeof(io_fn_t) * MAX_PORTS);

	/* Init i8253 PIT */
	i8253_restore(fd, vcp->vcp_id);
	ioports_map[TIMER_CTRL] = vcpu_exit_i8253;
	ioports_map[TIMER_BASE + TIMER_CNTR0] = vcpu_exit_i8253;
	ioports_map[TIMER_BASE + TIMER_CNTR1] = vcpu_exit_i8253;
	ioports_map[TIMER_BASE + TIMER_CNTR2] = vcpu_exit_i8253;

	/* Init master and slave PICs */
	i8259_restore(fd);
	ioports_map[IO_ICU1] = vcpu_exit_i8259;
	ioports_map[IO_ICU1 + 1] = vcpu_exit_i8259;
	ioports_map[IO_ICU2] = vcpu_exit_i8259;
	ioports_map[IO_ICU2 + 1] = vcpu_exit_i8259;

	/* Init ns8250 UART */
	ns8250_restore(fd, con_fd, vcp->vcp_id);
	for (i = COM1_DATA; i <= COM1_SCR; i++)
		ioports_map[i] = vcpu_exit_com;

	/* Init mc146818 RTC */
	mc146818_restore(fd, vcp->vcp_id);
	ioports_map[IO_RTC] = vcpu_exit_mc146818;
	ioports_map[IO_RTC + 1] = vcpu_exit_mc146818;

	/* Init QEMU fw_cfg interface */
	fw_cfg_restore(fd);
	ioports_map[FW_CFG_IO_SELECT] = vcpu_exit_fw_cfg;
	ioports_map[FW_CFG_IO_DATA] = vcpu_exit_fw_cfg;
	ioports_map[FW_CFG_IO_DMA_ADDR_HIGH] = vcpu_exit_fw_cfg_dma;
	ioports_map[FW_CFG_IO_DMA_ADDR_LOW] = vcpu_exit_fw_cfg_dma;

	/* Initialize PCI */
	for (i = VMM_PCI_IO_BAR_BASE; i <= VMM_PCI_IO_BAR_END; i++)
		ioports_map[i] = vcpu_exit_pci;

	ioports_map[PCI_MODE1_ADDRESS_REG] = vcpu_exit_pci;
	ioports_map[PCI_MODE1_DATA_REG] = vcpu_exit_pci;
	ioports_map[PCI_MODE1_DATA_REG + 1] = vcpu_exit_pci;
	ioports_map[PCI_MODE1_DATA_REG + 2] = vcpu_exit_pci;
	ioports_map[PCI_MODE1_DATA_REG + 3] = vcpu_exit_pci;
	pci_restore(fd);
	virtio_restore(fd, current_vm, child_cdrom, child_disks, child_taps);
}

/*
 * run_vm
 *
 * Runs the VM whose creation parameters are specified in vcp
 *
 * Parameters:
 *  child_cdrom: previously-opened child ISO disk file descriptor
 *  child_disks: previously-opened child VM disk file file descriptors
 *  child_taps: previously-opened child tap file descriptors
 *  vmc: vmop_create_params struct containing the VM's desired creation
 *      configuration
 *  vrs: VCPU register state to initialize
 *
 * Return values:
 *  0: the VM exited normally
 *  !0 : the VM exited abnormally or failed to start
 */
int
run_vm(int child_cdrom, int child_disks[][VM_MAX_BASE_PER_DISK],
    int *child_taps, struct vmop_create_params *vmc,
    struct vcpu_reg_state *vrs)
{
	struct vm_create_params *vcp = &vmc->vmc_params;
	struct vm_rwregs_params vregsp;
	uint8_t evdone = 0;
	size_t i;
	int ret;
	pthread_t *tid, evtid;
	struct vm_run_params **vrp;
	void *exit_status;

	if (vcp == NULL)
		return (EINVAL);

	if (child_cdrom == -1 && strlen(vcp->vcp_cdrom))
		return (EINVAL);

	if (child_disks == NULL && vcp->vcp_ndisks != 0)
		return (EINVAL);

	if (child_taps == NULL && vcp->vcp_nnics != 0)
		return (EINVAL);

	if (vcp->vcp_ncpus > VMM_MAX_VCPUS_PER_VM)
		return (EINVAL);

	if (vcp->vcp_ndisks > VMM_MAX_DISKS_PER_VM)
		return (EINVAL);

	if (vcp->vcp_nnics > VMM_MAX_NICS_PER_VM)
		return (EINVAL);

	if (vcp->vcp_nmemranges == 0 ||
	    vcp->vcp_nmemranges > VMM_MAX_MEM_RANGES)
		return (EINVAL);

	tid = calloc(vcp->vcp_ncpus, sizeof(pthread_t));
	vrp = calloc(vcp->vcp_ncpus, sizeof(struct vm_run_params *));
	if (tid == NULL || vrp == NULL) {
		log_warn("%s: memory allocation error - exiting.",
		    __progname);
		return (ENOMEM);
	}

	log_debug("%s: initializing hardware for vm %s", __func__,
	    vcp->vcp_name);

	if (!current_vm->vm_received)
		init_emulated_hw(vmc, child_cdrom, child_disks, child_taps);

	ret = pthread_mutex_init(&threadmutex, NULL);
	if (ret) {
		log_warn("%s: could not initialize thread state mutex",
		    __func__);
		return (ret);
	}
	ret = pthread_cond_init(&threadcond, NULL);
	if (ret) {
		log_warn("%s: could not initialize thread state "
		    "condition variable", __func__);
		return (ret);
	}

	mutex_lock(&threadmutex);

	log_debug("%s: starting vcpu threads for vm %s", __func__,
	    vcp->vcp_name);

	/*
	 * Create and launch one thread for each VCPU. These threads may
	 * migrate between PCPUs over time; the need to reload CPU state
	 * in such situations is detected and performed by vmm(4) in the
	 * kernel.
	 */
	for (i = 0 ; i < vcp->vcp_ncpus; i++) {
		vrp[i] = malloc(sizeof(struct vm_run_params));
		if (vrp[i] == NULL) {
			log_warn("%s: memory allocation error - "
			    "exiting.", __progname);
			/* caller will exit, so skip freeing */
			return (ENOMEM);
		}
		vrp[i]->vrp_exit = malloc(sizeof(struct vm_exit));
		if (vrp[i]->vrp_exit == NULL) {
			log_warn("%s: memory allocation error - "
			    "exiting.", __progname);
			/* caller will exit, so skip freeing */
			return (ENOMEM);
		}
		vrp[i]->vrp_vm_id = vcp->vcp_id;
		vrp[i]->vrp_vcpu_id = i;

		if (vcpu_reset(vcp->vcp_id, i, vrs)) {
			log_warnx("%s: cannot reset VCPU %zu - exiting.",
			    __progname, i);
			return (EIO);
		}

		/* once more because reset_cpu changes regs */
		if (current_vm->vm_received) {
			vregsp.vrwp_vm_id = vcp->vcp_id;
			vregsp.vrwp_vcpu_id = i;
			vregsp.vrwp_regs = *vrs;
			vregsp.vrwp_mask = VM_RWREGS_ALL;
			if ((ret = ioctl(env->vmd_fd, VMM_IOC_WRITEREGS,
			    &vregsp)) < 0) {
				log_warn("%s: writeregs failed", __func__);
				return (ret);
			}
		}

		ret = pthread_cond_init(&vcpu_run_cond[i], NULL);
		if (ret) {
			log_warnx("%s: cannot initialize cond var (%d)",
			    __progname, ret);
			return (ret);
		}

		ret = pthread_mutex_init(&vcpu_run_mtx[i], NULL);
		if (ret) {
			log_warnx("%s: cannot initialize mtx (%d)",
			    __progname, ret);
			return (ret);
		}

		vcpu_hlt[i] = 0;

		/* Start each VCPU run thread at vcpu_run_loop */
		ret = pthread_create(&tid[i], NULL, vcpu_run_loop, vrp[i]);
		if (ret) {
			/* caller will _exit after this return */
			ret = errno;
			log_warn("%s: could not create vcpu thread %zu",
			    __func__, i);
			return (ret);
		}
	}

	log_debug("%s: waiting on events for VM %s", __func__, vcp->vcp_name);
	ret = pthread_create(&evtid, NULL, event_thread, &evdone);
	if (ret) {
		errno = ret;
		log_warn("%s: could not create event thread", __func__);
		return (ret);
	}

	for (;;) {
		ret = pthread_cond_wait(&threadcond, &threadmutex);
		if (ret) {
			log_warn("%s: waiting on thread state condition "
			    "variable failed", __func__);
			return (ret);
		}

		/*
		 * Did a VCPU thread exit with an error? => return the first one
		 */
		for (i = 0; i < vcp->vcp_ncpus; i++) {
			if (vcpu_done[i] == 0)
				continue;

			if (pthread_join(tid[i], &exit_status)) {
				log_warn("%s: failed to join thread %zd - "
				    "exiting", __progname, i);
				return (EIO);
			}

			ret = (intptr_t)exit_status;
		}

		/* Did the event thread exit? => return with an error */
		if (evdone) {
			if (pthread_join(evtid, &exit_status)) {
				log_warn("%s: failed to join event thread - "
				    "exiting", __progname);
				return (EIO);
			}

			log_warnx("%s: vm %d event thread exited "
			    "unexpectedly", __progname, vcp->vcp_id);
			return (EIO);
		}

		/* Did all VCPU threads exit successfully? => return */
		for (i = 0; i < vcp->vcp_ncpus; i++) {
			if (vcpu_done[i] == 0)
				break;
		}
		if (i == vcp->vcp_ncpus)
			return (ret);

		/* Some more threads to wait for, start over */
	}

	return (ret);
}

void *
event_thread(void *arg)
{
	uint8_t *donep = arg;
	intptr_t ret;

	ret = event_dispatch();

	mutex_lock(&threadmutex);
	*donep = 1;
	pthread_cond_signal(&threadcond);
	mutex_unlock(&threadmutex);

	return (void *)ret;
 }

/*
 * vcpu_run_loop
 *
 * Runs a single VCPU until vmm(4) requires help handling an exit,
 * or the VM terminates.
 *
 * Parameters:
 *  arg: vcpu_run_params for the VCPU being run by this thread
 *
 * Return values:
 *  NULL: the VCPU shutdown properly
 *  !NULL: error processing VCPU run, or the VCPU shutdown abnormally
 */
void *
vcpu_run_loop(void *arg)
{
	struct vm_run_params *vrp = (struct vm_run_params *)arg;
	intptr_t ret = 0;
	int irq;
	uint32_t n;

	vrp->vrp_continue = 0;
	n = vrp->vrp_vcpu_id;

	for (;;) {
		ret = pthread_mutex_lock(&vcpu_run_mtx[n]);

		if (ret) {
			log_warnx("%s: can't lock vcpu run mtx (%d)",
			    __func__, (int)ret);
			return ((void *)ret);
		}

		/* If we are halted or paused, wait */
		if (vcpu_hlt[n]) {
			while (current_vm->vm_paused == 1) {
				ret = pthread_cond_wait(&vcpu_run_cond[n],
				    &vcpu_run_mtx[n]);
				if (ret) {
					log_warnx(
					    "%s: can't wait on cond (%d)",
					    __func__, (int)ret);
					(void)pthread_mutex_unlock(
					    &vcpu_run_mtx[n]);
					break;
				}
			}
			if (vcpu_hlt[n]) {
				ret = pthread_cond_wait(&vcpu_run_cond[n],
				    &vcpu_run_mtx[n]);

				if (ret) {
					log_warnx(
					    "%s: can't wait on cond (%d)",
					    __func__, (int)ret);
					(void)pthread_mutex_unlock(
					    &vcpu_run_mtx[n]);
					break;
				}
			}
		}

		ret = pthread_mutex_unlock(&vcpu_run_mtx[n]);

		if (ret) {
			log_warnx("%s: can't unlock mutex on cond (%d)",
			    __func__, (int)ret);
			break;
		}

		if (vrp->vrp_irqready && i8259_is_pending()) {
			irq = i8259_ack();
			vrp->vrp_irq = irq;
		} else
			vrp->vrp_irq = 0xFFFF;

		/* Still more pending? */
		if (i8259_is_pending()) {
			/* XXX can probably avoid ioctls here by providing intr in vrp */
			if (vcpu_pic_intr(vrp->vrp_vm_id,
			    vrp->vrp_vcpu_id, 1)) {
				fatal("can't set INTR");
			}
		} else {
			if (vcpu_pic_intr(vrp->vrp_vm_id,
			    vrp->vrp_vcpu_id, 0)) {
				fatal("can't clear INTR");
			}
		}

		if (ioctl(env->vmd_fd, VMM_IOC_RUN, vrp) < 0) {
			/* If run ioctl failed, exit */
			ret = errno;
			log_warn("%s: vm %d / vcpu %d run ioctl failed",
			    __func__, vrp->vrp_vm_id, n);
			break;
		}

		/* If the VM is terminating, exit normally */
		if (vrp->vrp_exit_reason == VM_EXIT_TERMINATED) {
			ret = (intptr_t)NULL;
			break;
		}

		if (vrp->vrp_exit_reason != VM_EXIT_NONE) {
			/*
			 * vmm(4) needs help handling an exit, handle in
			 * vcpu_exit.
			 */
			ret = vcpu_exit(vrp);
			if (ret)
				break;
		}
	}

	mutex_lock(&threadmutex);
	vcpu_done[n] = 1;
	pthread_cond_signal(&threadcond);
	mutex_unlock(&threadmutex);

	return ((void *)ret);
}

int
vcpu_pic_intr(uint32_t vm_id, uint32_t vcpu_id, uint8_t intr)
{
	struct vm_intr_params vip;

	memset(&vip, 0, sizeof(vip));

	vip.vip_vm_id = vm_id;
	vip.vip_vcpu_id = vcpu_id; /* XXX always 0? */
	vip.vip_intr = intr;

	if (ioctl(env->vmd_fd, VMM_IOC_INTR, &vip) < 0)
		return (errno);

	return (0);
}

/*
 * vcpu_exit_pci
 *
 * Handle all I/O to the emulated PCI subsystem.
 *
 * Parameters:
 *  vrp: vcpu run paramters containing guest state for this exit
 *
 * Return value:
 *  Interrupt to inject to the guest VM, or 0xFF if no interrupt should
 *      be injected.
 */
uint8_t
vcpu_exit_pci(struct vm_run_params *vrp)
{
	struct vm_exit *vei = vrp->vrp_exit;
	uint8_t intr;

	intr = 0xFF;

	switch (vei->vei.vei_port) {
	case PCI_MODE1_ADDRESS_REG:
		pci_handle_address_reg(vrp);
		break;
	case PCI_MODE1_DATA_REG:
	case PCI_MODE1_DATA_REG + 1:
	case PCI_MODE1_DATA_REG + 2:
	case PCI_MODE1_DATA_REG + 3:
		pci_handle_data_reg(vrp);
		break;
	case VMM_PCI_IO_BAR_BASE ... VMM_PCI_IO_BAR_END:
		intr = pci_handle_io(vrp);
		break;
	default:
		log_warnx("%s: unknown PCI register 0x%llx",
		    __progname, (uint64_t)vei->vei.vei_port);
		break;
	}

	return (intr);
}

/*
 * vcpu_exit_inout
 *
 * Handle all I/O exits that need to be emulated in vmd. This includes the
 * i8253 PIT, the com1 ns8250 UART, and the MC146818 RTC/NVRAM device.
 *
 * Parameters:
 *  vrp: vcpu run parameters containing guest state for this exit
 */
void
vcpu_exit_inout(struct vm_run_params *vrp)
{
	struct vm_exit *vei = vrp->vrp_exit;
	uint8_t intr = 0xFF;

	if (ioports_map[vei->vei.vei_port] != NULL)
		intr = ioports_map[vei->vei.vei_port](vrp);
	else if (vei->vei.vei_dir == VEI_DIR_IN)
			set_return_data(vei, 0xFFFFFFFF);

	if (intr != 0xFF)
		vcpu_assert_pic_irq(vrp->vrp_vm_id, vrp->vrp_vcpu_id, intr);
}

/*
 * vcpu_exit
 *
 * Handle a vcpu exit. This function is called when it is determined that
 * vmm(4) requires the assistance of vmd to support a particular guest
 * exit type (eg, accessing an I/O port or device). Guest state is contained
 * in 'vrp', and will be resent to vmm(4) on exit completion.
 *
 * Upon conclusion of handling the exit, the function determines if any
 * interrupts should be injected into the guest, and asserts the proper
 * IRQ line whose interrupt should be vectored.
 *
 * Parameters:
 *  vrp: vcpu run parameters containing guest state for this exit
 *
 * Return values:
 *  0: the exit was handled successfully
 *  1: an error occurred (eg, unknown exit reason passed in 'vrp')
 */
int
vcpu_exit(struct vm_run_params *vrp)
{
	int ret;

	switch (vrp->vrp_exit_reason) {
	case VMX_EXIT_INT_WINDOW:
	case SVM_VMEXIT_VINTR:
	case VMX_EXIT_CPUID:
	case VMX_EXIT_EXTINT:
	case SVM_VMEXIT_INTR:
	case VMX_EXIT_EPT_VIOLATION:
	case SVM_VMEXIT_NPF:
	case SVM_VMEXIT_MSR:
	case SVM_VMEXIT_CPUID:
		/*
		 * We may be exiting to vmd to handle a pending interrupt but
		 * at the same time the last exit type may have been one of
		 * these. In this case, there's nothing extra to be done
		 * here (and falling through to the default case below results
		 * in more vmd log spam).
		 */
		break;
	case VMX_EXIT_IO:
	case SVM_VMEXIT_IOIO:
		vcpu_exit_inout(vrp);
		break;
	case VMX_EXIT_HLT:
	case SVM_VMEXIT_HLT:
		ret = pthread_mutex_lock(&vcpu_run_mtx[vrp->vrp_vcpu_id]);
		if (ret) {
			log_warnx("%s: can't lock vcpu mutex (%d)",
			    __func__, ret);
			return (ret);
		}
		vcpu_hlt[vrp->vrp_vcpu_id] = 1;
		ret = pthread_mutex_unlock(&vcpu_run_mtx[vrp->vrp_vcpu_id]);
		if (ret) {
			log_warnx("%s: can't unlock vcpu mutex (%d)",
			    __func__, ret);
			return (ret);
		}
		break;
	case VMX_EXIT_TRIPLE_FAULT:
	case SVM_VMEXIT_SHUTDOWN:
		/* reset VM */
		return (EAGAIN);
	default:
		log_debug("%s: unknown exit reason 0x%x",
		    __progname, vrp->vrp_exit_reason);
	}

	/* Process any pending traffic */
	vionet_process_rx(vrp->vrp_vm_id);

	vrp->vrp_continue = 1;

	return (0);
}

/*
 * find_gpa_range
 *
 * Search for a contiguous guest physical mem range.
 *
 * Parameters:
 *  vcp: VM create parameters that contain the memory map to search in
 *  gpa: the starting guest physical address
 *  len: the length of the memory range
 *
 * Return values:
 *  NULL: on failure if there is no memory range as described by the parameters
 *  Pointer to vm_mem_range that contains the start of the range otherwise.
 */
static struct vm_mem_range *
find_gpa_range(struct vm_create_params *vcp, paddr_t gpa, size_t len)
{
	size_t i, n;
	struct vm_mem_range *vmr;

	/* Find the first vm_mem_range that contains gpa */
	for (i = 0; i < vcp->vcp_nmemranges; i++) {
		vmr = &vcp->vcp_memranges[i];
		if (vmr->vmr_gpa + vmr->vmr_size >= gpa)
			break;
	}

	/* No range found. */
	if (i == vcp->vcp_nmemranges)
		return (NULL);

	/*
	 * vmr may cover the range [gpa, gpa + len) only partly. Make
	 * sure that the following vm_mem_ranges are contiguous and
	 * cover the rest.
	 */
	n = vmr->vmr_size - (gpa - vmr->vmr_gpa);
	if (len < n)
		len = 0;
	else
		len -= n;
	gpa = vmr->vmr_gpa + vmr->vmr_size;
	for (i = i + 1; len != 0 && i < vcp->vcp_nmemranges; i++) {
		vmr = &vcp->vcp_memranges[i];
		if (gpa != vmr->vmr_gpa)
			return (NULL);
		if (len <= vmr->vmr_size)
			len = 0;
		else
			len -= vmr->vmr_size;

		gpa = vmr->vmr_gpa + vmr->vmr_size;
	}

	if (len != 0)
		return (NULL);

	return (vmr);
}

void *
vaddr_mem(paddr_t gpa, size_t len)
{
	struct vm_create_params *vcp = &current_vm->vm_params.vmc_params;
	size_t i;
	struct vm_mem_range *vmr;
	paddr_t gpend = gpa + len;

	/* Find the first vm_mem_range that contains gpa */
	for (i = 0; i < vcp->vcp_nmemranges; i++) {
		vmr = &vcp->vcp_memranges[i];
		if (gpa < vmr->vmr_gpa)
			continue;

		if (gpend >= vmr->vmr_gpa + vmr->vmr_size)
			continue;

		return ((char *)vmr->vmr_va + (gpa - vmr->vmr_gpa));
	}

	return (NULL);
}

/*
 * write_mem
 *
 * Copies data from 'buf' into the guest VM's memory at paddr 'dst'.
 *
 * Parameters:
 *  dst: the destination paddr_t in the guest VM
 *  buf: data to copy (or NULL to zero the data)
 *  len: number of bytes to copy
 *
 * Return values:
 *  0: success
 *  EINVAL: if the guest physical memory range [dst, dst + len) does not
 *      exist in the guest.
 */
int
write_mem(paddr_t dst, const void *buf, size_t len)
{
	const char *from = buf;
	char *to;
	size_t n, off;
	struct vm_mem_range *vmr;

	vmr = find_gpa_range(&current_vm->vm_params.vmc_params, dst, len);
	if (vmr == NULL) {
		errno = EINVAL;
		log_warn("%s: failed - invalid memory range dst = 0x%lx, "
		    "len = 0x%zx", __func__, dst, len);
		return (EINVAL);
	}

	off = dst - vmr->vmr_gpa;
	while (len != 0) {
		n = vmr->vmr_size - off;
		if (len < n)
			n = len;

		to = (char *)vmr->vmr_va + off;
		if (buf == NULL)
			memset(to, 0, n);
		else {
			memcpy(to, from, n);
			from += n;
		}
		len -= n;
		off = 0;
		vmr++;
	}

	return (0);
}

/*
 * read_mem
 *
 * Reads memory at guest paddr 'src' into 'buf'.
 *
 * Parameters:
 *  src: the source paddr_t in the guest VM to read from.
 *  buf: destination (local) buffer
 *  len: number of bytes to read
 *
 * Return values:
 *  0: success
 *  EINVAL: if the guest physical memory range [dst, dst + len) does not
 *      exist in the guest.
 */
int
read_mem(paddr_t src, void *buf, size_t len)
{
	char *from, *to = buf;
	size_t n, off;
	struct vm_mem_range *vmr;

	vmr = find_gpa_range(&current_vm->vm_params.vmc_params, src, len);
	if (vmr == NULL) {
		errno = EINVAL;
		log_warn("%s: failed - invalid memory range src = 0x%lx, "
		    "len = 0x%zx", __func__, src, len);
		return (EINVAL);
	}

	off = src - vmr->vmr_gpa;
	while (len != 0) {
		n = vmr->vmr_size - off;
		if (len < n)
			n = len;

		from = (char *)vmr->vmr_va + off;
		memcpy(to, from, n);

		to += n;
		len -= n;
		off = 0;
		vmr++;
	}

	return (0);
}

int
iovec_mem(paddr_t src, size_t len, struct iovec *iov, int iovcnt)
{
	size_t n, off;
	struct vm_mem_range *vmr;
	int niov = 0;

	vmr = find_gpa_range(&current_vm->vm_params.vmc_params, src, len);
	if (vmr == NULL) {
		errno = EINVAL;
		return (-1);
	}

	off = src - vmr->vmr_gpa;
	while (len > 0) {
		if (niov == iovcnt) {
			errno = ENOMEM;
			return (-1);
		}

		n = vmr->vmr_size - off;
		if (len < n)
			n = len;

		iov[niov].iov_base = (char *)vmr->vmr_va + off;
		iov[niov].iov_len = n;

		niov++;

		len -= n;
		off = 0;
		vmr++;
	}

	return (niov);
}

/*
 * vcpu_assert_pic_irq
 *
 * Injects the specified IRQ on the supplied vcpu/vm
 *
 * Parameters:
 *  vm_id: VM ID to inject to
 *  vcpu_id: VCPU ID to inject to
 *  irq: IRQ to inject
 */
void
vcpu_assert_pic_irq(uint32_t vm_id, uint32_t vcpu_id, int irq)
{
	int ret;

	i8259_assert_irq(irq);

	if (i8259_is_pending()) {
		if (vcpu_pic_intr(vm_id, vcpu_id, 1))
			fatalx("%s: can't assert INTR", __func__);

		ret = pthread_mutex_lock(&vcpu_run_mtx[vcpu_id]);
		if (ret)
			fatalx("%s: can't lock vcpu mtx (%d)", __func__, ret);

		vcpu_hlt[vcpu_id] = 0;
		ret = pthread_cond_signal(&vcpu_run_cond[vcpu_id]);
		if (ret)
			fatalx("%s: can't signal (%d)", __func__, ret);
		ret = pthread_mutex_unlock(&vcpu_run_mtx[vcpu_id]);
		if (ret)
			fatalx("%s: can't unlock vcpu mtx (%d)", __func__, ret);
	}
}

/*
 * vcpu_deassert_pic_irq
 *
 * Clears the specified IRQ on the supplied vcpu/vm
 *
 * Parameters:
 *  vm_id: VM ID to clear in
 *  vcpu_id: VCPU ID to clear in
 *  irq: IRQ to clear
 */
void
vcpu_deassert_pic_irq(uint32_t vm_id, uint32_t vcpu_id, int irq)
{
	i8259_deassert_irq(irq);

	if (!i8259_is_pending()) {
		if (vcpu_pic_intr(vm_id, vcpu_id, 0))
			fatalx("%s: can't deassert INTR for vm_id %d, "
			    "vcpu_id %d", __func__, vm_id, vcpu_id);
	}
}

/*
 * fd_hasdata
 *
 * Determines if data can be read from a file descriptor.
 *
 * Parameters:
 *  fd: the fd to check
 *
 * Return values:
 *  1 if data can be read from an fd, or 0 otherwise.
 */
int
fd_hasdata(int fd)
{
	struct pollfd pfd[1];
	int nready, hasdata = 0;

	pfd[0].fd = fd;
	pfd[0].events = POLLIN;
	nready = poll(pfd, 1, 0);
	if (nready == -1)
		log_warn("checking file descriptor for data failed");
	else if (nready == 1 && pfd[0].revents & POLLIN)
		hasdata = 1;
	return (hasdata);
}

/*
 * mutex_lock
 *
 * Wrapper function for pthread_mutex_lock that does error checking and that
 * exits on failure
 */
void
mutex_lock(pthread_mutex_t *m)
{
	int ret;

	ret = pthread_mutex_lock(m);
	if (ret) {
		errno = ret;
		fatal("could not acquire mutex");
	}
}

/*
 * mutex_unlock
 *
 * Wrapper function for pthread_mutex_unlock that does error checking and that
 * exits on failure
 */
void
mutex_unlock(pthread_mutex_t *m)
{
	int ret;

	ret = pthread_mutex_unlock(m);
	if (ret) {
		errno = ret;
		fatal("could not release mutex");
	}
}

/*
 * set_return_data
 *
 * Utility function for manipulating register data in vm exit info structs. This
 * function ensures that the data is copied to the vei->vei.vei_data field with
 * the proper size for the operation being performed.
 *
 * Parameters:
 *  vei: exit information
 *  data: return data
 */
void
set_return_data(struct vm_exit *vei, uint32_t data)
{
	switch (vei->vei.vei_size) {
	case 1:
		vei->vei.vei_data &= ~0xFF;
		vei->vei.vei_data |= (uint8_t)data;
		break;
	case 2:
		vei->vei.vei_data &= ~0xFFFF;
		vei->vei.vei_data |= (uint16_t)data;
		break;
	case 4:
		vei->vei.vei_data = data;
		break;
	}
}

/*
 * get_input_data
 *
 * Utility function for manipulating register data in vm exit info
 * structs. This function ensures that the data is copied from the
 * vei->vei.vei_data field with the proper size for the operation being
 * performed.
 *
 * Parameters:
 *  vei: exit information
 *  data: location to store the result
 */
void
get_input_data(struct vm_exit *vei, uint32_t *data)
{
	switch (vei->vei.vei_size) {
	case 1:
		*data &= 0xFFFFFF00;
		*data |= (uint8_t)vei->vei.vei_data;
		break;
	case 2:
		*data &= 0xFFFF0000;
		*data |= (uint16_t)vei->vei.vei_data;
		break;
	case 4:
		*data = vei->vei.vei_data;
		break;
	default:
		log_warnx("%s: invalid i/o size %d", __func__,
		    vei->vei.vei_size);
	}

}
