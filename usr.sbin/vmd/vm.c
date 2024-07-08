/*	$OpenBSD: vm.c,v 1.102 2024/07/08 17:33:45 dv Exp $	*/

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

#include <sys/param.h>	/* PAGE_SIZE, MAXCOMLEN */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/resource.h>

#include <dev/ic/i8253reg.h>
#include <dev/isa/isareg.h>
#include <dev/pci/pcireg.h>

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
#include <pthread_np.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "atomicio.h"
#include "fw_cfg.h"
#include "i8253.h"
#include "i8259.h"
#include "loadfile.h"
#include "mc146818.h"
#include "mmio.h"
#include "ns8250.h"
#include "pci.h"
#include "virtio.h"
#include "vmd.h"
#include "vmm.h"

#define MB(x)	(x * 1024UL * 1024UL)
#define GB(x)	(x * 1024UL * 1024UL * 1024UL)

#define MMIO_NOTYET 0

io_fn_t ioports_map[MAX_PORTS];

static int run_vm(struct vmop_create_params *, struct vcpu_reg_state *);
void vm_dispatch_vmm(int, short, void *);
void *event_thread(void *);
void *vcpu_run_loop(void *);
int vcpu_exit(struct vm_run_params *);
int vcpu_reset(uint32_t, uint32_t, struct vcpu_reg_state *);
void create_memory_map(struct vm_create_params *);
static int vmm_create_vm(struct vmd_vm *);
int alloc_guest_mem(struct vmd_vm *);
void init_emulated_hw(struct vmop_create_params *, int,
    int[][VM_MAX_BASE_PER_DISK], int *);
void restore_emulated_hw(struct vm_create_params *, int, int *,
    int[][VM_MAX_BASE_PER_DISK],int);
void vcpu_exit_inout(struct vm_run_params *);
int vcpu_exit_eptviolation(struct vm_run_params *);
uint8_t vcpu_exit_pci(struct vm_run_params *);
int vcpu_pic_intr(uint32_t, uint32_t, uint8_t);
int loadfile_bios(gzFile, off_t, struct vcpu_reg_state *);
static int send_vm(int, struct vmd_vm *);
int dump_send_header(int);
static int dump_vmr(int , struct vm_mem_range *);
static int dump_mem(int, struct vmd_vm *);
void restore_vmr(int, struct vm_mem_range *);
void restore_mem(int, struct vm_create_params *);
int restore_vm_params(int, struct vm_create_params *);
static void pause_vm(struct vmd_vm *);
static void unpause_vm(struct vmd_vm *);

int translate_gva(struct vm_exit*, uint64_t, uint64_t *, int);

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
pthread_barrier_t vm_pause_barrier;
pthread_cond_t vcpu_unpause_cond[VMM_MAX_VCPUS_PER_VM];
pthread_mutex_t vcpu_unpause_mtx[VMM_MAX_VCPUS_PER_VM];

pthread_mutex_t vm_mtx;
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
	.vrs_gprs[VCPU_REGS_RFLAGS] = 0x2,
	.vrs_gprs[VCPU_REGS_RIP] = 0x0,
	.vrs_gprs[VCPU_REGS_RSP] = 0x0,
	.vrs_crs[VCPU_REGS_CR0] = CR0_ET | CR0_PE | CR0_PG,
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
	.vrs_msrs[VCPU_REGS_MISC_ENABLE] = 0ULL,
	.vrs_crs[VCPU_REGS_XCR0] = XFEATURE_X87
};

/*
 * Represents a standard register set for an BIOS to be booted
 * as a flat 16 bit address space.
 */
static const struct vcpu_reg_state vcpu_init_flat16 = {
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
	.vrs_crs[VCPU_REGS_XCR0] = XFEATURE_X87
};

/*
 * vm_main
 *
 * Primary entrypoint for launching a vm. Does not return.
 *
 * fd: file descriptor for communicating with vmm process.
 * fd_vmm: file descriptor for communicating with vmm(4) device
 */
void
vm_main(int fd, int fd_vmm)
{
	struct vm_create_params	*vcp = NULL;
	struct vmd_vm		 vm;
	size_t			 sz = 0;
	int			 ret = 0;

	/*
	 * The vm process relies on global state. Set the fd for /dev/vmm.
	 */
	env->vmd_fd = fd_vmm;

	/*
	 * We aren't root, so we can't chroot(2). Use unveil(2) instead.
	 */
	if (unveil(env->argv0, "x") == -1)
		fatal("unveil %s", env->argv0);
	if (unveil(NULL, NULL) == -1)
		fatal("unveil lock");

	/*
	 * pledge in the vm processes:
	 * stdio - for malloc and basic I/O including events.
	 * vmm - for the vmm ioctls and operations.
	 * proc exec - fork/exec for launching devices.
	 * recvfd - for vm send/recv and sending fd to devices.
	 */
	if (pledge("stdio vmm proc exec recvfd", NULL) == -1)
		fatal("pledge");

	/* Receive our vm configuration. */
	memset(&vm, 0, sizeof(vm));
	sz = atomicio(read, fd, &vm, sizeof(vm));
	if (sz != sizeof(vm)) {
		log_warnx("failed to receive start message");
		_exit(EIO);
	}

	/* Update process with the vm name. */
	vcp = &vm.vm_params.vmc_params;
	setproctitle("%s", vcp->vcp_name);
	log_procinit("vm/%s", vcp->vcp_name);

	/* Receive the local prefix settings. */
	sz = atomicio(read, fd, &env->vmd_cfg.cfg_localprefix,
	    sizeof(env->vmd_cfg.cfg_localprefix));
	if (sz != sizeof(env->vmd_cfg.cfg_localprefix)) {
		log_warnx("failed to receive local prefix");
		_exit(EIO);
	}

	/*
	 * We need, at minimum, a vm_kernel fd to boot a vm. This is either a
	 * kernel or a BIOS image.
	 */
	if (!(vm.vm_state & VM_STATE_RECEIVED)) {
		if (vm.vm_kernel == -1) {
			log_warnx("%s: failed to receive boot fd",
			    vcp->vcp_name);
			_exit(EINVAL);
		}
	}

	ret = start_vm(&vm, fd);
	_exit(ret);
}

/*
 * loadfile_bios
 *
 * Alternatively to loadfile_elf, this function loads a non-ELF BIOS image
 * directly into memory.
 *
 * Parameters:
 *  fp: file of a kernel file to load
 *  size: uncompressed size of the image
 *  (out) vrs: register state to set on init for this kernel
 *
 * Return values:
 *  0 if successful
 *  various error codes returned from read(2) or loadelf functions
 */
int
loadfile_bios(gzFile fp, off_t size, struct vcpu_reg_state *vrs)
{
	off_t	 off;

	/* Set up a "flat 16 bit" register state for BIOS */
	memcpy(vrs, &vcpu_init_flat16, sizeof(*vrs));

	/* Seek to the beginning of the BIOS image */
	if (gzseek(fp, 0, SEEK_SET) == -1)
		return (-1);

	/* The BIOS image must end at 1MB */
	if ((off = MB(1) - size) < 0)
		return (-1);

	/* Read BIOS image into memory */
	if (mread(fp, off, size) != (size_t)size) {
		errno = EIO;
		return (-1);
	}

	if (gzseek(fp, 0, SEEK_SET) == -1)
		return (-1);

	/* Read a second BIOS copy into memory ending at 4GB */
	off = GB(4) - size;
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
 * 3. drops additional privileges by calling pledge(2)
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
	int			 nicfds[VM_MAX_NICS_PER_VM];
	int			 ret;
	gzFile			 fp;
	size_t			 i;
	struct vm_rwregs_params  vrp;
	struct stat		 sb;

	/*
	 * We first try to initialize and allocate memory before bothering
	 * vmm(4) with a request to create a new vm.
	 */
	if (!(vm->vm_state & VM_STATE_RECEIVED))
		create_memory_map(vcp);

	ret = alloc_guest_mem(vm);
	if (ret) {
		struct rlimit lim;
		char buf[FMT_SCALED_STRSIZE];
		if (ret == ENOMEM && getrlimit(RLIMIT_DATA, &lim) == 0) {
			if (fmt_scaled(lim.rlim_cur, buf) == 0)
				fatalx("could not allocate guest memory (data "
				    "limit is %s)", buf);
		}
		errno = ret;
		log_warn("could not allocate guest memory");
		return (ret);
	}

	/* We've allocated guest memory, so now create the vm in vmm(4). */
	ret = vmm_create_vm(vm);
	if (ret) {
		/* Let the vmm process know we failed by sending a 0 vm id. */
		vcp->vcp_id = 0;
		atomicio(vwrite, fd, &vcp->vcp_id, sizeof(vcp->vcp_id));
		return (ret);
	}

	/*
	 * Some of vmd currently relies on global state (current_vm, con_fd).
	 */
	current_vm = vm;
	con_fd = vm->vm_tty;
	if (fcntl(con_fd, F_SETFL, O_NONBLOCK) == -1) {
		log_warn("failed to set nonblocking mode on console");
		return (1);
	}

	/*
	 * We now let the vmm process know we were successful by sending it our
	 * vmm(4) assigned vm id.
	 */
	if (atomicio(vwrite, fd, &vcp->vcp_id, sizeof(vcp->vcp_id)) !=
	    sizeof(vcp->vcp_id)) {
		log_warn("failed to send created vm id to vmm process");
		return (1);
	}

	/* Prepare either our boot image or receive an existing vm to launch. */
	if (vm->vm_state & VM_STATE_RECEIVED) {
		ret = atomicio(read, vm->vm_receive_fd, &vrp, sizeof(vrp));
		if (ret != sizeof(vrp))
			fatal("received incomplete vrp - exiting");
		vrs = vrp.vrwp_regs;
	} else {
		/*
		 * Set up default "flat 64 bit" register state - RIP,
		 * RSP, and GDT info will be set in bootloader
		 */
		memcpy(&vrs, &vcpu_init_flat64, sizeof(vrs));

		/* Find and open kernel image */
		if ((fp = gzdopen(vm->vm_kernel, "r")) == NULL)
			fatalx("failed to open kernel - exiting");

		/* Load kernel image */
		ret = loadfile_elf(fp, vm, &vrs, vmc->vmc_bootdevice);

		/*
		 * Try BIOS as a fallback (only if it was provided as an image
		 * with vm->vm_kernel and the file is not compressed)
		 */
		if (ret && errno == ENOEXEC && vm->vm_kernel != -1 &&
		    gzdirect(fp) && (ret = fstat(vm->vm_kernel, &sb)) == 0)
			ret = loadfile_bios(fp, sb.st_size, &vrs);

		if (ret)
			fatal("failed to load kernel or BIOS - exiting");

		gzclose(fp);
	}

	if (vm->vm_kernel != -1)
		close_fd(vm->vm_kernel);

	/* Initialize our mutexes. */
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
	ret = pthread_mutex_init(&vm_mtx, NULL);
	if (ret) {
		log_warn("%s: could not initialize vm state mutex",
		    __func__);
		return (ret);
	}

	/* Lock thread mutex now. It's unlocked when waiting on threadcond. */
	mutex_lock(&threadmutex);

	/*
	 * Finalize our communication socket with the vmm process. From here
	 * onwards, communication with the vmm process is event-based.
	 */
	event_init();
	if (vmm_pipe(vm, fd, vm_dispatch_vmm) == -1)
		fatal("setup vm pipe");

	/*
	 * Initialize or restore our emulated hardware.
	 */
	for (i = 0; i < VMM_MAX_NICS_PER_VM; i++)
		nicfds[i] = vm->vm_ifs[i].vif_fd;

	if (vm->vm_state & VM_STATE_RECEIVED) {
		restore_mem(vm->vm_receive_fd, vcp);
		restore_emulated_hw(vcp, vm->vm_receive_fd, nicfds,
		    vm->vm_disks, vm->vm_cdrom);
		if (restore_vm_params(vm->vm_receive_fd, vcp))
			fatal("restore vm params failed");
		unpause_vm(vm);
	} else
		init_emulated_hw(vmc, vm->vm_cdrom, vm->vm_disks, nicfds);

	/* Drop privleges further before starting the vcpu run loop(s). */
	if (pledge("stdio vmm recvfd", NULL) == -1)
		fatal("pledge");

	/*
	 * Execute the vcpu run loop(s) for this VM.
	 */
	ret = run_vm(&vm->vm_params, &vrs);

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
	struct vmop_addr_result	 var;
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
			virtio_broadcast_imsg(vm, IMSG_CTL_VERBOSE, &verbose,
			    sizeof(verbose));
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
			pause_vm(vm);
			imsg_compose_event(&vm->vm_iev,
			    IMSG_VMDOP_PAUSE_VM_RESPONSE,
			    imsg.hdr.peerid, imsg.hdr.pid, -1, &vmr,
			    sizeof(vmr));
			break;
		case IMSG_VMDOP_UNPAUSE_VM:
			vmr.vmr_result = 0;
			vmr.vmr_id = vm->vm_vmid;
			unpause_vm(vm);
			imsg_compose_event(&vm->vm_iev,
			    IMSG_VMDOP_UNPAUSE_VM_RESPONSE,
			    imsg.hdr.peerid, imsg.hdr.pid, -1, &vmr,
			    sizeof(vmr));
			break;
		case IMSG_VMDOP_SEND_VM_REQUEST:
			vmr.vmr_id = vm->vm_vmid;
			vmr.vmr_result = send_vm(imsg_get_fd(&imsg), vm);
			imsg_compose_event(&vm->vm_iev,
			    IMSG_VMDOP_SEND_VM_RESPONSE,
			    imsg.hdr.peerid, imsg.hdr.pid, -1, &vmr,
			    sizeof(vmr));
			if (!vmr.vmr_result) {
				imsg_flush(&current_vm->vm_iev.ibuf);
				_exit(0);
			}
			break;
		case IMSG_VMDOP_PRIV_GET_ADDR_RESPONSE:
			IMSG_SIZE_CHECK(&imsg, &var);
			memcpy(&var, imsg.data, sizeof(var));

			log_debug("%s: received tap addr %s for nic %d",
			    vm->vm_params.vmc_params.vcp_name,
			    ether_ntoa((void *)var.var_addr), var.var_nic_idx);

			vionet_set_hostmac(vm, var.var_nic_idx, var.var_addr);
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
 * vm_shutdown
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
send_vm(int fd, struct vmd_vm *vm)
{
	struct vm_rwregs_params	   vrp;
	struct vm_rwvmparams_params vpp;
	struct vmop_create_params *vmc;
	struct vm_terminate_params vtp;
	unsigned int		   flags = 0;
	unsigned int		   i;
	int			   ret = 0;
	size_t			   sz;

	if (dump_send_header(fd)) {
		log_warnx("%s: failed to send vm dump header", __func__);
		goto err;
	}

	pause_vm(vm);

	vmc = calloc(1, sizeof(struct vmop_create_params));
	if (vmc == NULL) {
		log_warn("%s: calloc error getting vmc", __func__);
		ret = -1;
		goto err;
	}

	flags |= VMOP_CREATE_MEMORY;
	memcpy(&vmc->vmc_params, &current_vm->vm_params, sizeof(struct
	    vmop_create_params));
	vmc->vmc_flags = flags;
	vrp.vrwp_vm_id = vm->vm_params.vmc_params.vcp_id;
	vrp.vrwp_mask = VM_RWREGS_ALL;
	vpp.vpp_mask = VM_RWVMPARAMS_ALL;
	vpp.vpp_vm_id = vm->vm_params.vmc_params.vcp_id;

	sz = atomicio(vwrite, fd, vmc, sizeof(struct vmop_create_params));
	if (sz != sizeof(struct vmop_create_params)) {
		ret = -1;
		goto err;
	}

	for (i = 0; i < vm->vm_params.vmc_params.vcp_ncpus; i++) {
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

	/* Dump memory before devices to aid in restoration. */
	if ((ret = dump_mem(fd, vm)))
		goto err;
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

	for (i = 0; i < vm->vm_params.vmc_params.vcp_ncpus; i++) {
		vpp.vpp_vcpu_id = i;
		if ((ret = ioctl(env->vmd_fd, VMM_IOC_READVMPARAMS, &vpp))) {
			log_warn("%s: readvmparams failed", __func__);
			goto err;
		}

		sz = atomicio(vwrite, fd, &vpp,
		    sizeof(struct vm_rwvmparams_params));
		if (sz != sizeof(struct vm_rwvmparams_params)) {
			log_warn("%s: dumping vm params failed", __func__);
			ret = -1;
			goto err;
		}
	}

	vtp.vtp_vm_id = vm->vm_params.vmc_params.vcp_id;
	if (ioctl(env->vmd_fd, VMM_IOC_TERM, &vtp) == -1) {
		log_warnx("%s: term IOC error: %d, %d", __func__,
		    errno, ENOENT);
	}
err:
	close(fd);
	if (ret)
		unpause_vm(vm);
	return ret;
}

int
dump_send_header(int fd) {
	struct vm_dump_header	   vmh;
	int			   i;

	memcpy(&vmh.vmh_signature, VM_DUMP_SIGNATURE,
	    sizeof(vmh.vmh_signature));

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
dump_mem(int fd, struct vmd_vm *vm)
{
	unsigned int	i;
	int		ret;
	struct		vm_mem_range *vmr;

	for (i = 0; i < vm->vm_params.vmc_params.vcp_nmemranges; i++) {
		vmr = &vm->vm_params.vmc_params.vcp_memranges[i];
		ret = dump_vmr(fd, vmr);
		if (ret)
			return ret;
	}
	return (0);
}

int
restore_vm_params(int fd, struct vm_create_params *vcp) {
	unsigned int			i;
	struct vm_rwvmparams_params    vpp;

	for (i = 0; i < vcp->vcp_ncpus; i++) {
		if (atomicio(read, fd, &vpp, sizeof(vpp)) != sizeof(vpp)) {
			log_warn("%s: error restoring vm params", __func__);
			return (-1);
		}
		vpp.vpp_vm_id = vcp->vcp_id;
		vpp.vpp_vcpu_id = i;
		if (ioctl(env->vmd_fd, VMM_IOC_WRITEVMPARAMS, &vpp) < 0) {
			log_debug("%s: writing vm params failed", __func__);
			return (-1);
		}
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

static void
pause_vm(struct vmd_vm *vm)
{
	unsigned int n;
	int ret;

	mutex_lock(&vm_mtx);
	if (vm->vm_state & VM_STATE_PAUSED) {
		mutex_unlock(&vm_mtx);
		return;
	}
	current_vm->vm_state |= VM_STATE_PAUSED;
	mutex_unlock(&vm_mtx);

	ret = pthread_barrier_init(&vm_pause_barrier, NULL,
	    vm->vm_params.vmc_params.vcp_ncpus + 1);
	if (ret) {
		log_warnx("%s: cannot initialize pause barrier (%d)",
		    __progname, ret);
		return;
	}

	for (n = 0; n < vm->vm_params.vmc_params.vcp_ncpus; n++) {
		ret = pthread_cond_broadcast(&vcpu_run_cond[n]);
		if (ret) {
			log_warnx("%s: can't broadcast vcpu run cond (%d)",
			    __func__, (int)ret);
			return;
		}
	}
	ret = pthread_barrier_wait(&vm_pause_barrier);
	if (ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD) {
		log_warnx("%s: could not wait on pause barrier (%d)",
		    __func__, (int)ret);
		return;
	}

	ret = pthread_barrier_destroy(&vm_pause_barrier);
	if (ret) {
		log_warnx("%s: could not destroy pause barrier (%d)",
		    __progname, ret);
		return;
	}

	i8253_stop();
	mc146818_stop();
	ns8250_stop();
	virtio_stop(vm);
}

static void
unpause_vm(struct vmd_vm *vm)
{
	unsigned int n;
	int ret;

	mutex_lock(&vm_mtx);
	if (!(vm->vm_state & VM_STATE_PAUSED)) {
		mutex_unlock(&vm_mtx);
		return;
	}
	current_vm->vm_state &= ~VM_STATE_PAUSED;
	mutex_unlock(&vm_mtx);

	for (n = 0; n < vm->vm_params.vmc_params.vcp_ncpus; n++) {
		ret = pthread_cond_broadcast(&vcpu_unpause_cond[n]);
		if (ret) {
			log_warnx("%s: can't broadcast vcpu unpause cond (%d)",
			    __func__, (int)ret);
			return;
		}
	}

	i8253_start();
	mc146818_start();
	ns8250_start();
	virtio_start(vm);
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

	if (ioctl(env->vmd_fd, VMM_IOC_RESETCPU, &vrp) == -1)
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
	size_t len, mem_bytes;
	size_t above_1m = 0, above_4g = 0;

	mem_bytes = vcp->vcp_memranges[0].vmr_size;
	vcp->vcp_nmemranges = 0;
	if (mem_bytes == 0 || mem_bytes > VMM_MAX_VM_MEM_SIZE)
		return;

	/* First memory region: 0 - LOWMEM_KB (DOS low mem) */
	len = LOWMEM_KB * 1024;
	vcp->vcp_memranges[0].vmr_gpa = 0x0;
	vcp->vcp_memranges[0].vmr_size = len;
	vcp->vcp_memranges[0].vmr_type = VM_MEM_RAM;
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
	len = MB(1) - (LOWMEM_KB * 1024);
	vcp->vcp_memranges[1].vmr_gpa = LOWMEM_KB * 1024;
	vcp->vcp_memranges[1].vmr_size = len;
	vcp->vcp_memranges[1].vmr_type = VM_MEM_RESERVED;
	mem_bytes -= len;

	/* If we have less than 2MB remaining, still create a 2nd BIOS area. */
	if (mem_bytes <= MB(2)) {
		vcp->vcp_memranges[2].vmr_gpa = VMM_PCI_MMIO_BAR_END;
		vcp->vcp_memranges[2].vmr_size = MB(2);
		vcp->vcp_memranges[2].vmr_type = VM_MEM_RESERVED;
		vcp->vcp_nmemranges = 3;
		return;
	}

	/*
	 * Calculate the how to split any remaining memory across the 4GB
	 * boundary while making sure we do not place physical memory into
	 * MMIO ranges.
	 */
	if (mem_bytes > VMM_PCI_MMIO_BAR_BASE - MB(1)) {
		above_1m = VMM_PCI_MMIO_BAR_BASE - MB(1);
		above_4g = mem_bytes - above_1m;
	} else {
		above_1m = mem_bytes;
		above_4g = 0;
	}

	/* Third memory region: area above 1MB to MMIO region */
	vcp->vcp_memranges[2].vmr_gpa = MB(1);
	vcp->vcp_memranges[2].vmr_size = above_1m;
	vcp->vcp_memranges[2].vmr_type = VM_MEM_RAM;

	/* Fourth region: PCI MMIO range */
	vcp->vcp_memranges[3].vmr_gpa = VMM_PCI_MMIO_BAR_BASE;
	vcp->vcp_memranges[3].vmr_size = VMM_PCI_MMIO_BAR_END -
	    VMM_PCI_MMIO_BAR_BASE + 1;
	vcp->vcp_memranges[3].vmr_type = VM_MEM_MMIO;

	/* Fifth region: 2nd copy of BIOS above MMIO ending at 4GB */
	vcp->vcp_memranges[4].vmr_gpa = VMM_PCI_MMIO_BAR_END + 1;
	vcp->vcp_memranges[4].vmr_size = MB(2);
	vcp->vcp_memranges[4].vmr_type = VM_MEM_RESERVED;

	/* Sixth region: any remainder above 4GB */
	if (above_4g > 0) {
		vcp->vcp_memranges[5].vmr_gpa = GB(4);
		vcp->vcp_memranges[5].vmr_size = above_4g;
		vcp->vcp_memranges[5].vmr_type = VM_MEM_RAM;
		vcp->vcp_nmemranges = 6;
	} else
		vcp->vcp_nmemranges = 5;
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
alloc_guest_mem(struct vmd_vm *vm)
{
	void *p;
	int ret = 0;
	size_t i, j;
	struct vm_create_params *vcp = &vm->vm_params.vmc_params;
	struct vm_mem_range *vmr;

	for (i = 0; i < vcp->vcp_nmemranges; i++) {
		vmr = &vcp->vcp_memranges[i];

		/*
		 * We only need R/W as userland. vmm(4) will use R/W/X in its
		 * mapping.
		 *
		 * We must use MAP_SHARED so emulated devices will be able
		 * to generate shared mappings.
		 */
		p = mmap(NULL, vmr->vmr_size, PROT_READ | PROT_WRITE,
		    MAP_ANON | MAP_CONCEAL | MAP_SHARED, -1, 0);
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

	return (ret);
}

/*
 * vmm_create_vm
 *
 * Requests vmm(4) to create a new VM using the supplied creation
 * parameters. This operation results in the creation of the in-kernel
 * structures for the VM, but does not start the VM's vcpu(s).
 *
 * Parameters:
 *  vm: pointer to the vm object
 *
 * Return values:
 *  0: success
 *  !0 : ioctl to vmm(4) failed
 */
static int
vmm_create_vm(struct vmd_vm *vm)
{
	struct vm_create_params *vcp = &vm->vm_params.vmc_params;

	/* Sanity check arguments */
	if (vcp->vcp_ncpus > VMM_MAX_VCPUS_PER_VM)
		return (EINVAL);

	if (vcp->vcp_nmemranges == 0 ||
	    vcp->vcp_nmemranges > VMM_MAX_MEM_RANGES)
		return (EINVAL);

	if (vm->vm_params.vmc_ndisks > VM_MAX_DISKS_PER_VM)
		return (EINVAL);

	if (vm->vm_params.vmc_nnics > VM_MAX_NICS_PER_VM)
		return (EINVAL);

	if (ioctl(env->vmd_fd, VMM_IOC_CREATE, vcp) == -1)
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
	size_t i;
	uint64_t memlo, memhi;

	/* Calculate memory size for NVRAM registers */
	memlo = memhi = 0;
	for (i = 0; i < vcp->vcp_nmemranges; i++) {
		if (vcp->vcp_memranges[i].vmr_gpa == MB(1) &&
		    vcp->vcp_memranges[i].vmr_size > (15 * MB(1)))
			memlo = vcp->vcp_memranges[i].vmr_size - (15 * MB(1));
		else if (vcp->vcp_memranges[i].vmr_gpa == GB(4))
			memhi = vcp->vcp_memranges[i].vmr_size;
	}

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

	/* Initialize PCI */
	for (i = VM_PCI_IO_BAR_BASE; i <= VM_PCI_IO_BAR_END; i++)
		ioports_map[i] = vcpu_exit_pci;

	ioports_map[PCI_MODE1_ADDRESS_REG] = vcpu_exit_pci;
	ioports_map[PCI_MODE1_DATA_REG] = vcpu_exit_pci;
	ioports_map[PCI_MODE1_DATA_REG + 1] = vcpu_exit_pci;
	ioports_map[PCI_MODE1_DATA_REG + 2] = vcpu_exit_pci;
	ioports_map[PCI_MODE1_DATA_REG + 3] = vcpu_exit_pci;
	pci_init();

	/* Initialize virtio devices */
	virtio_init(current_vm, child_cdrom, child_disks, child_taps);

	/*
	 * Init QEMU fw_cfg interface. Must be done last for pci hardware
	 * detection.
	 */
	fw_cfg_init(vmc);
	ioports_map[FW_CFG_IO_SELECT] = vcpu_exit_fw_cfg;
	ioports_map[FW_CFG_IO_DATA] = vcpu_exit_fw_cfg;
	ioports_map[FW_CFG_IO_DMA_ADDR_HIGH] = vcpu_exit_fw_cfg_dma;
	ioports_map[FW_CFG_IO_DMA_ADDR_LOW] = vcpu_exit_fw_cfg_dma;
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
	for (i = VM_PCI_IO_BAR_BASE; i <= VM_PCI_IO_BAR_END; i++)
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
static int
run_vm(struct vmop_create_params *vmc, struct vcpu_reg_state *vrs)
{
	struct vm_create_params *vcp = &vmc->vmc_params;
	struct vm_rwregs_params vregsp;
	uint8_t evdone = 0;
	size_t i;
	int ret;
	pthread_t *tid, evtid;
	char tname[MAXCOMLEN + 1];
	struct vm_run_params **vrp;
	void *exit_status;

	if (vcp == NULL)
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

	log_debug("%s: starting %zu vcpu thread(s) for vm %s", __func__,
	    vcp->vcp_ncpus, vcp->vcp_name);

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
		if (current_vm->vm_state & VM_STATE_RECEIVED) {
			vregsp.vrwp_vm_id = vcp->vcp_id;
			vregsp.vrwp_vcpu_id = i;
			vregsp.vrwp_regs = *vrs;
			vregsp.vrwp_mask = VM_RWREGS_ALL;
			if ((ret = ioctl(env->vmd_fd, VMM_IOC_WRITEREGS,
			    &vregsp)) == -1) {
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

		ret = pthread_cond_init(&vcpu_unpause_cond[i], NULL);
		if (ret) {
			log_warnx("%s: cannot initialize unpause var (%d)",
			    __progname, ret);
			return (ret);
		}

		ret = pthread_mutex_init(&vcpu_unpause_mtx[i], NULL);
		if (ret) {
			log_warnx("%s: cannot initialize unpause mtx (%d)",
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

		snprintf(tname, sizeof(tname), "vcpu-%zu", i);
		pthread_set_name_np(tid[i], tname);
	}

	log_debug("%s: waiting on events for VM %s", __func__, vcp->vcp_name);
	ret = pthread_create(&evtid, NULL, event_thread, &evdone);
	if (ret) {
		errno = ret;
		log_warn("%s: could not create event thread", __func__);
		return (ret);
	}
	pthread_set_name_np(evtid, "event");

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
		mutex_lock(&vm_mtx);
		for (i = 0; i < vcp->vcp_ncpus; i++) {
			if (vcpu_done[i] == 0)
				continue;

			if (pthread_join(tid[i], &exit_status)) {
				log_warn("%s: failed to join thread %zd - "
				    "exiting", __progname, i);
				mutex_unlock(&vm_mtx);
				return (EIO);
			}

			ret = (intptr_t)exit_status;
		}
		mutex_unlock(&vm_mtx);

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
		mutex_lock(&vm_mtx);
		for (i = 0; i < vcp->vcp_ncpus; i++) {
			if (vcpu_done[i] == 0)
				break;
		}
		mutex_unlock(&vm_mtx);
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

	*donep = 1;

	mutex_lock(&threadmutex);
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
	uint32_t n = vrp->vrp_vcpu_id;
	int paused = 0, halted = 0;

	for (;;) {
		ret = pthread_mutex_lock(&vcpu_run_mtx[n]);

		if (ret) {
			log_warnx("%s: can't lock vcpu run mtx (%d)",
			    __func__, (int)ret);
			return ((void *)ret);
		}

		mutex_lock(&vm_mtx);
		paused = (current_vm->vm_state & VM_STATE_PAUSED) != 0;
		halted = vcpu_hlt[n];
		mutex_unlock(&vm_mtx);

		/* If we are halted and need to pause, pause */
		if (halted && paused) {
			ret = pthread_barrier_wait(&vm_pause_barrier);
			if (ret != 0 && ret != PTHREAD_BARRIER_SERIAL_THREAD) {
				log_warnx("%s: could not wait on pause barrier (%d)",
				    __func__, (int)ret);
				return ((void *)ret);
			}

			ret = pthread_mutex_lock(&vcpu_unpause_mtx[n]);
			if (ret) {
				log_warnx("%s: can't lock vcpu unpause mtx (%d)",
				    __func__, (int)ret);
				return ((void *)ret);
			}

			/* i8259 may be firing as we pause, release run mtx. */
			mutex_unlock(&vcpu_run_mtx[n]);
			ret = pthread_cond_wait(&vcpu_unpause_cond[n],
			    &vcpu_unpause_mtx[n]);
			if (ret) {
				log_warnx(
				    "%s: can't wait on unpause cond (%d)",
				    __func__, (int)ret);
				break;
			}
			mutex_lock(&vcpu_run_mtx[n]);

			ret = pthread_mutex_unlock(&vcpu_unpause_mtx[n]);
			if (ret) {
				log_warnx("%s: can't unlock unpause mtx (%d)",
				    __func__, (int)ret);
				break;
			}
		}

		/* If we are halted and not paused, wait */
		if (halted) {
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

		ret = pthread_mutex_unlock(&vcpu_run_mtx[n]);

		if (ret) {
			log_warnx("%s: can't unlock mutex on cond (%d)",
			    __func__, (int)ret);
			break;
		}

		if (vrp->vrp_irqready && i8259_is_pending()) {
			vrp->vrp_inject.vie_vector = i8259_ack();
			vrp->vrp_inject.vie_type = VCPU_INJECT_INTR;
		} else
			vrp->vrp_inject.vie_type = VCPU_INJECT_NONE;

		/* Still more interrupts pending? */
		vrp->vrp_intr_pending = i8259_is_pending();

		if (ioctl(env->vmd_fd, VMM_IOC_RUN, vrp) == -1) {
			/* If run ioctl failed, exit */
			ret = errno;
			log_warn("%s: vm %d / vcpu %d run ioctl failed",
			    __func__, current_vm->vm_vmid, n);
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

	mutex_lock(&vm_mtx);
	vcpu_done[n] = 1;
	mutex_unlock(&vm_mtx);

	mutex_lock(&threadmutex);
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

	if (ioctl(env->vmd_fd, VMM_IOC_INTR, &vip) == -1)
		return (errno);

	return (0);
}

/*
 * vcpu_exit_pci
 *
 * Handle all I/O to the emulated PCI subsystem.
 *
 * Parameters:
 *  vrp: vcpu run parameters containing guest state for this exit
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
	case VM_PCI_IO_BAR_BASE ... VM_PCI_IO_BAR_END:
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

	if (vei->vei.vei_rep || vei->vei.vei_string) {
#ifdef MMIO_DEBUG
		log_info("%s: %s%s%s %d-byte, enc=%d, data=0x%08x, port=0x%04x",
		    __func__,
		    vei->vei.vei_rep == 0 ? "" : "REP ",
		    vei->vei.vei_dir == VEI_DIR_IN ? "IN" : "OUT",
		    vei->vei.vei_string == 0 ? "" : "S",
		    vei->vei.vei_size, vei->vei.vei_encoding,
		    vei->vei.vei_data, vei->vei.vei_port);
		log_info("%s: ECX = 0x%llx, RDX = 0x%llx, RSI = 0x%llx",
		    __func__,
		    vei->vrs.vrs_gprs[VCPU_REGS_RCX],
		    vei->vrs.vrs_gprs[VCPU_REGS_RDX],
		    vei->vrs.vrs_gprs[VCPU_REGS_RSI]);
#endif /* MMIO_DEBUG */
		fatalx("%s: can't emulate REP prefixed IN(S)/OUT(S)",
		    __func__);
	}

	if (ioports_map[vei->vei.vei_port] != NULL)
		intr = ioports_map[vei->vei.vei_port](vrp);
	else if (vei->vei.vei_dir == VEI_DIR_IN)
		set_return_data(vei, 0xFFFFFFFF);

	vei->vrs.vrs_gprs[VCPU_REGS_RIP] += vei->vei.vei_insn_len;

	if (intr != 0xFF)
		vcpu_assert_pic_irq(vrp->vrp_vm_id, vrp->vrp_vcpu_id, intr);
}

/*
 * vcpu_exit_eptviolation
 *
 * handle an EPT Violation
 *
 * Parameters:
 *  vrp: vcpu run parameters containing guest state for this exit
 *
 * Return values:
 *  0: no action required
 *  EFAULT: a protection fault occured, kill the vm.
 */
int
vcpu_exit_eptviolation(struct vm_run_params *vrp)
{
	struct vm_exit *ve = vrp->vrp_exit;
	int ret = 0;
#if MMIO_NOTYET
	struct x86_insn insn;
	uint64_t va, pa;
	size_t len = 15;		/* Max instruction length in x86. */
#endif /* MMIO_NOTYET */
	switch (ve->vee.vee_fault_type) {
	case VEE_FAULT_HANDLED:
		break;

#if MMIO_NOTYET
	case VEE_FAULT_MMIO_ASSIST:
		/* Intel VMX might give us the length of the instruction. */
		if (ve->vee.vee_insn_info & VEE_LEN_VALID)
			len = ve->vee.vee_insn_len;

		if (len > 15)
			fatalx("%s: invalid instruction length %lu", __func__,
			    len);

		/* If we weren't given instruction bytes, we need to fetch. */
		if (!(ve->vee.vee_insn_info & VEE_BYTES_VALID)) {
			memset(ve->vee.vee_insn_bytes, 0,
			    sizeof(ve->vee.vee_insn_bytes));
			va = ve->vrs.vrs_gprs[VCPU_REGS_RIP];

			/* XXX Only support instructions that fit on 1 page. */
			if ((va & PAGE_MASK) + len > PAGE_SIZE) {
				log_warnx("%s: instruction might cross page "
				    "boundary", __func__);
				ret = EINVAL;
				break;
			}

			ret = translate_gva(ve, va, &pa, PROT_EXEC);
			if (ret != 0) {
				log_warnx("%s: failed gva translation",
				    __func__);
				break;
			}

			ret = read_mem(pa, ve->vee.vee_insn_bytes, len);
			if (ret != 0) {
				log_warnx("%s: failed to fetch instruction "
				    "bytes from 0x%llx", __func__, pa);
				break;
			}
		}

		ret = insn_decode(ve, &insn);
		if (ret == 0)
			ret = insn_emulate(ve, &insn);
		break;
#endif /* MMIO_NOTYET */

	case VEE_FAULT_PROTECT:
		log_debug("%s: EPT Violation: rip=0x%llx", __progname,
		    ve->vrs.vrs_gprs[VCPU_REGS_RIP]);
		ret = EFAULT;
		break;

	default:
		fatalx("%s: invalid fault_type %d", __progname,
		    ve->vee.vee_fault_type);
		/* UNREACHED */
	}

	return (ret);
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
	case SVM_VMEXIT_NPF:
	case VMX_EXIT_EPT_VIOLATION:
		ret = vcpu_exit_eptviolation(vrp);
		if (ret)
			return (ret);
		break;
	case VMX_EXIT_IO:
	case SVM_VMEXIT_IOIO:
		vcpu_exit_inout(vrp);
		break;
	case VMX_EXIT_HLT:
	case SVM_VMEXIT_HLT:
		mutex_lock(&vm_mtx);
		vcpu_hlt[vrp->vrp_vcpu_id] = 1;
		mutex_unlock(&vm_mtx);
		break;
	case VMX_EXIT_TRIPLE_FAULT:
	case SVM_VMEXIT_SHUTDOWN:
		/* reset VM */
		return (EAGAIN);
	default:
		log_debug("%s: unknown exit reason 0x%x",
		    __progname, vrp->vrp_exit_reason);
	}

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
		if (gpa < vmr->vmr_gpa + vmr->vmr_size)
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

/*
 * hvaddr_mem
 *
 * Translate a guest physical address to a host virtual address, checking the
 * provided memory range length to confirm it's contiguous within the same
 * guest memory range (vm_mem_range).
 *
 * Parameters:
 *  gpa: guest physical address to translate
 *  len: number of bytes in the intended range
 *
 * Return values:
 *  void* to host virtual memory on success
 *  NULL on error, setting errno to:
 *    EFAULT: gpa falls outside guest memory ranges
 *    EINVAL: requested len extends beyond memory range
 */
void *
hvaddr_mem(paddr_t gpa, size_t len)
{
	struct vm_mem_range *vmr;
	size_t off;

	vmr = find_gpa_range(&current_vm->vm_params.vmc_params, gpa, len);
	if (vmr == NULL) {
		log_warnx("%s: failed - invalid gpa: 0x%lx\n", __func__, gpa);
		errno = EFAULT;
		return (NULL);
	}

	off = gpa - vmr->vmr_gpa;
	if (len > (vmr->vmr_size - off)) {
		log_warnx("%s: failed - invalid memory range: gpa=0x%lx, "
		    "len=%zu", __func__, gpa, len);
		errno = EINVAL;
		return (NULL);
	}

	return ((char *)vmr->vmr_va + off);
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

		mutex_lock(&vm_mtx);
		vcpu_hlt[vcpu_id] = 0;
		mutex_unlock(&vm_mtx);

		mutex_lock(&vcpu_run_mtx[vcpu_id]);
		ret = pthread_cond_signal(&vcpu_run_cond[vcpu_id]);
		if (ret)
			fatalx("%s: can't signal (%d)", __func__, ret);
		mutex_unlock(&vcpu_run_mtx[vcpu_id]);
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

/*
 * translate_gva
 *
 * Translates a guest virtual address to a guest physical address by walking
 * the currently active page table (if needed).
 *
 * XXX ensure translate_gva updates the A bit in the PTE
 * XXX ensure translate_gva respects segment base and limits in i386 mode
 * XXX ensure translate_gva respects segment wraparound in i8086 mode
 * XXX ensure translate_gva updates the A bit in the segment selector
 * XXX ensure translate_gva respects CR4.LMSLE if available
 *
 * Parameters:
 *  exit: The VCPU this translation should be performed for (guest MMU settings
 *   are gathered from this VCPU)
 *  va: virtual address to translate
 *  pa: pointer to paddr_t variable that will receive the translated physical
 *   address. 'pa' is unchanged on error.
 *  mode: one of PROT_READ, PROT_WRITE, PROT_EXEC indicating the mode in which
 *   the address should be translated
 *
 * Return values:
 *  0: the address was successfully translated - 'pa' contains the physical
 *     address currently mapped by 'va'.
 *  EFAULT: the PTE for 'VA' is unmapped. A #PF will be injected in this case
 *     and %cr2 set in the vcpu structure.
 *  EINVAL: an error occurred reading paging table structures
 */
int
translate_gva(struct vm_exit* exit, uint64_t va, uint64_t* pa, int mode)
{
	int level, shift, pdidx;
	uint64_t pte, pt_paddr, pte_paddr, mask, low_mask, high_mask;
	uint64_t shift_width, pte_size;
	struct vcpu_reg_state *vrs;

	vrs = &exit->vrs;

	if (!pa)
		return (EINVAL);

	if (!(vrs->vrs_crs[VCPU_REGS_CR0] & CR0_PG)) {
		log_debug("%s: unpaged, va=pa=0x%llx", __func__, va);
		*pa = va;
		return (0);
	}

	pt_paddr = vrs->vrs_crs[VCPU_REGS_CR3];

	log_debug("%s: guest %%cr0=0x%llx, %%cr3=0x%llx", __func__,
	    vrs->vrs_crs[VCPU_REGS_CR0], vrs->vrs_crs[VCPU_REGS_CR3]);

	if (vrs->vrs_crs[VCPU_REGS_CR0] & CR0_PE) {
		if (vrs->vrs_crs[VCPU_REGS_CR4] & CR4_PAE) {
			pte_size = sizeof(uint64_t);
			shift_width = 9;

			if (vrs->vrs_msrs[VCPU_REGS_EFER] & EFER_LMA) {
				/* 4 level paging */
				level = 4;
				mask = L4_MASK;
				shift = L4_SHIFT;
			} else {
				/* 32 bit with PAE paging */
				level = 3;
				mask = L3_MASK;
				shift = L3_SHIFT;
			}
		} else {
			/* 32 bit paging */
			level = 2;
			shift_width = 10;
			mask = 0xFFC00000;
			shift = 22;
			pte_size = sizeof(uint32_t);
		}
	} else
		return (EINVAL);

	/* XXX: Check for R bit in segment selector and set A bit */

	for (;level > 0; level--) {
		pdidx = (va & mask) >> shift;
		pte_paddr = (pt_paddr) + (pdidx * pte_size);

		log_debug("%s: read pte level %d @ GPA 0x%llx", __func__,
		    level, pte_paddr);
		if (read_mem(pte_paddr, &pte, pte_size)) {
			log_warn("%s: failed to read pte", __func__);
			return (EFAULT);
		}

		log_debug("%s: PTE @ 0x%llx = 0x%llx", __func__, pte_paddr,
		    pte);

		/* XXX: Set CR2  */
		if (!(pte & PG_V))
			return (EFAULT);

		/* XXX: Check for SMAP */
		if ((mode == PROT_WRITE) && !(pte & PG_RW))
			return (EPERM);

		if ((exit->cpl > 0) && !(pte & PG_u))
			return (EPERM);

		pte = pte | PG_U;
		if (mode == PROT_WRITE)
			pte = pte | PG_M;
		if (write_mem(pte_paddr, &pte, pte_size)) {
			log_warn("%s: failed to write back flags to pte",
			    __func__);
			return (EIO);
		}

		/* XXX: EINVAL if in 32bit and PG_PS is 1 but CR4.PSE is 0 */
		if (pte & PG_PS)
			break;

		if (level > 1) {
			pt_paddr = pte & PG_FRAME;
			shift -= shift_width;
			mask = mask >> shift_width;
		}
	}

	low_mask = (1 << shift) - 1;
	high_mask = (((uint64_t)1ULL << ((pte_size * 8) - 1)) - 1) ^ low_mask;
	*pa = (pte & high_mask) | (va & low_mask);

	log_debug("%s: final GPA for GVA 0x%llx = 0x%llx\n", __func__, va, *pa);

	return (0);
}

void
vm_pipe_init(struct vm_dev_pipe *p, void (*cb)(int, short, void *))
{
	vm_pipe_init2(p, cb, NULL);
}

/*
 * vm_pipe_init2
 *
 * Initialize a vm_dev_pipe, setting up its file descriptors and its
 * event structure with the given callback and argument.
 *
 * Parameters:
 *  p: pointer to vm_dev_pipe struct to initizlize
 *  cb: callback to use for READ events on the read end of the pipe
 *  arg: pointer to pass to the callback on event trigger
 */
void
vm_pipe_init2(struct vm_dev_pipe *p, void (*cb)(int, short, void *), void *arg)
{
	int ret;
	int fds[2];

	memset(p, 0, sizeof(struct vm_dev_pipe));

	ret = pipe2(fds, O_CLOEXEC);
	if (ret)
		fatal("failed to create vm_dev_pipe pipe");

	p->read = fds[0];
	p->write = fds[1];

	event_set(&p->read_ev, p->read, EV_READ | EV_PERSIST, cb, arg);
}

/*
 * vm_pipe_send
 *
 * Send a message to an emulated device vie the provided vm_dev_pipe. This
 * relies on the fact sizeof(msg) < PIPE_BUF to ensure atomic writes.
 *
 * Parameters:
 *  p: pointer to initialized vm_dev_pipe
 *  msg: message to send in the channel
 */
void
vm_pipe_send(struct vm_dev_pipe *p, enum pipe_msg_type msg)
{
	size_t n;
	n = write(p->write, &msg, sizeof(msg));
	if (n != sizeof(msg))
		fatal("failed to write to device pipe");
}

/*
 * vm_pipe_recv
 *
 * Receive a message for an emulated device via the provided vm_dev_pipe.
 * Returns the message value, otherwise will exit on failure. This relies on
 * the fact sizeof(enum pipe_msg_type) < PIPE_BUF for atomic reads.
 *
 * Parameters:
 *  p: pointer to initialized vm_dev_pipe
 *
 * Return values:
 *  a value of enum pipe_msg_type or fatal exit on read(2) error
 */
enum pipe_msg_type
vm_pipe_recv(struct vm_dev_pipe *p)
{
	size_t n;
	enum pipe_msg_type msg;
	n = read(p->read, &msg, sizeof(msg));
	if (n != sizeof(msg))
		fatal("failed to read from device pipe");

	return msg;
}

/*
 * Re-map the guest address space using vmm(4)'s VMM_IOC_SHARE
 *
 * Returns 0 on success, non-zero in event of failure.
 */
int
remap_guest_mem(struct vmd_vm *vm, int vmm_fd)
{
	struct vm_create_params	*vcp;
	struct vm_mem_range	*vmr;
	struct vm_sharemem_params vsp;
	size_t			 i, j;
	void			*p = NULL;
	int			 ret;

	if (vm == NULL)
		return (1);

	vcp = &vm->vm_params.vmc_params;

	/*
	 * Initialize our VM shared memory request using our original
	 * creation parameters. We'll overwrite the va's after mmap(2).
	 */
	memset(&vsp, 0, sizeof(vsp));
	vsp.vsp_nmemranges = vcp->vcp_nmemranges;
	vsp.vsp_vm_id = vcp->vcp_id;
	memcpy(&vsp.vsp_memranges, &vcp->vcp_memranges,
	    sizeof(vsp.vsp_memranges));

	/*
	 * Use mmap(2) to identify virtual address space for our mappings.
	 */
	for (i = 0; i < VMM_MAX_MEM_RANGES; i++) {
		if (i < vsp.vsp_nmemranges) {
			vmr = &vsp.vsp_memranges[i];

			/* Ignore any MMIO ranges. */
			if (vmr->vmr_type == VM_MEM_MMIO) {
				vmr->vmr_va = 0;
				vcp->vcp_memranges[i].vmr_va = 0;
				continue;
			}

			/* Make initial mappings for the memrange. */
			p = mmap(NULL, vmr->vmr_size, PROT_READ, MAP_ANON, -1,
			    0);
			if (p == MAP_FAILED) {
				ret = errno;
				log_warn("%s: mmap", __func__);
				for (j = 0; j < i; j++) {
					vmr = &vcp->vcp_memranges[j];
					munmap((void *)vmr->vmr_va,
					    vmr->vmr_size);
				}
				return (ret);
			}
			vmr->vmr_va = (vaddr_t)p;
			vcp->vcp_memranges[i].vmr_va = vmr->vmr_va;
		}
	}

	/*
	 * munmap(2) now that we have va's and ranges that don't overlap. vmm
	 * will use the va's and sizes to recreate the mappings for us.
	 */
	for (i = 0; i < vsp.vsp_nmemranges; i++) {
		vmr = &vsp.vsp_memranges[i];
		if (vmr->vmr_type == VM_MEM_MMIO)
			continue;
		if (munmap((void*)vmr->vmr_va, vmr->vmr_size) == -1)
			fatal("%s: munmap", __func__);
	}

	/*
	 * Ask vmm to enter the shared mappings for us. They'll point
	 * to the same host physical memory, but will have a randomized
	 * virtual address for the calling process.
	 */
	if (ioctl(vmm_fd, VMM_IOC_SHAREMEM, &vsp) == -1)
		return (errno);

	return (0);
}
