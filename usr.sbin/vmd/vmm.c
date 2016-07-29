/*	$OpenBSD: vmm.c,v 1.34 2016/07/29 16:36:51 stefan Exp $	*/

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

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/mman.h>

#include <dev/ic/comreg.h>
#include <dev/ic/i8253reg.h>
#include <dev/isa/isareg.h>
#include <dev/pci/pcireg.h>

#include <machine/param.h>
#include <machine/specialreg.h>
#include <machine/vmmvar.h>

#include <errno.h>
#include <fcntl.h>
#include <imsg.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "vmd.h"
#include "loadfile.h"
#include "pci.h"
#include "virtio.h"
#include "proc.h"

#define MAX_PORTS 65536

/*
 * Emulated 8250 UART
 */
#define COM1_DATA	0x3f8
#define COM1_IER	0x3f9
#define COM1_IIR	0x3fa
#define COM1_LCR	0x3fb
#define COM1_MCR	0x3fc
#define COM1_LSR	0x3fd
#define COM1_MSR	0x3fe
#define COM1_SCR	0x3ff

/*
 * Emulated i8253 PIT (counter)
 */
#define TIMER_BASE	0x40
#define TIMER_CTRL	0x43	/* 8253 Timer #1 */
#define NS_PER_TICK	(1000000000 / TIMER_FREQ)

/* i8253 registers */
struct i8253_counter {
	struct timeval tv;	/* timer start time */
	uint16_t start;		/* starting value */
	uint16_t olatch;	/* output latch */
	uint16_t ilatch;	/* input latch */
	uint8_t last_r;		/* last read byte (MSB/LSB) */
	uint8_t last_w;		/* last written byte (MSB/LSB) */
};

/* ns8250 UART registers */
struct ns8250_regs {
	uint8_t lcr;		/* Line Control Register */
	uint8_t fcr;		/* FIFO Control Register */
	uint8_t iir;		/* Interrupt ID Register */
	uint8_t ier;		/* Interrupt Enable Register */
	uint8_t divlo;		/* Baud rate divisor low byte */
	uint8_t divhi;		/* Baud rate divisor high byte */
	uint8_t msr;		/* Modem Status Register */
	uint8_t lsr;		/* Line Status Register */
	uint8_t mcr;		/* Modem Control Register */
	uint8_t scr;		/* Scratch Register */
	uint8_t data;		/* Unread input data */
};

typedef uint8_t (*io_fn_t)(struct vm_run_params *);

struct i8253_counter i8253_counter[3];
struct ns8250_regs com1_regs;
io_fn_t ioports_map[MAX_PORTS];

void vmm_sighdlr(int, short, void *);
int start_client_vmd(void);
int opentap(void);
int start_vm(struct imsg *, uint32_t *);
int terminate_vm(struct vm_terminate_params *);
int get_info_vm(struct privsep *, struct imsg *, int);
int run_vm(int *, int *, struct vm_create_params *, struct vcpu_init_state *);
void *vcpu_run_loop(void *);
int vcpu_exit(struct vm_run_params *);
int vcpu_reset(uint32_t, uint32_t, struct vcpu_init_state *);
void create_memory_map(struct vm_create_params *);
int alloc_guest_mem(struct vm_create_params *);
int vmm_create_vm(struct vm_create_params *);
void init_emulated_hw(struct vm_create_params *, int *, int *);
void vcpu_exit_inout(struct vm_run_params *);
uint8_t vcpu_exit_pci(struct vm_run_params *);
uint8_t vcpu_exit_i8253(struct vm_run_params *);
uint8_t vcpu_exit_com(struct vm_run_params *);
void vcpu_process_com_data(union vm_exit *);
void vcpu_process_com_lcr(union vm_exit *);
void vcpu_process_com_lsr(union vm_exit *);
void vcpu_process_com_ier(union vm_exit *);
void vcpu_process_com_mcr(union vm_exit *);
void vcpu_process_com_iir(union vm_exit *);
void vcpu_process_com_msr(union vm_exit *);
void vcpu_process_com_scr(union vm_exit *);

int vmm_dispatch_parent(int, struct privsep_proc *, struct imsg *);
void vmm_run(struct privsep *, struct privsep_proc *, void *);

static struct vm_mem_range *find_gpa_range(struct vm_create_params *, paddr_t,
    size_t);

int con_fd;
struct vmd_vm *current_vm;

extern struct vmd *env;

extern char *__progname;

static struct privsep_proc procs[] = {
	{ "parent",	PROC_PARENT,	vmm_dispatch_parent  },
};

/*
 * Represents a standard register set for an OS to be booted
 * as a flat 32 bit address space, before paging is enabled.
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
static const struct vcpu_init_state vcpu_init_flat32 = {
	0x2,					/* RFLAGS */
	0x0,					/* RIP */
	0x0,					/* RSP */
	CR0_CD | CR0_NW | CR0_ET | CR0_PE | CR0_PG, /* CR0 */
	PML4_PAGE,				/* CR3 */
	{ 0x8, 0xFFFFFFFF, 0xC09F, 0x0},	/* CS */
	{ 0x10, 0xFFFFFFFF, 0xC093, 0x0},	/* DS */
	{ 0x10, 0xFFFFFFFF, 0xC093, 0x0},	/* ES */
	{ 0x10, 0xFFFFFFFF, 0xC093, 0x0},	/* FS */
	{ 0x10, 0xFFFFFFFF, 0xC093, 0x0},	/* GS */
	{ 0x10, 0xFFFFFFFF, 0xC093, 0x0},	/* SS */
	{ 0x0, 0xFFFF, 0x0, 0x0},		/* GDTR */
	{ 0x0, 0xFFFF, 0x0, 0x0},		/* IDTR */
	{ 0x0, 0xFFFF, 0x0082, 0x0},		/* LDTR */
	{ 0x0, 0xFFFF, 0x008B, 0x0},		/* TR */
};

pid_t
vmm(struct privsep *ps, struct privsep_proc *p)
{
	return (proc_run(ps, p, procs, nitems(procs), vmm_run, NULL));
}

void
vmm_run(struct privsep *ps, struct privsep_proc *p, void *arg)
{
	if (config_init(ps->ps_env) == -1)
		fatal("failed to initialize configuration");

	signal_del(&ps->ps_evsigchld);
	signal_set(&ps->ps_evsigchld, SIGCHLD, vmm_sighdlr, ps);
	signal_add(&ps->ps_evsigchld, NULL);

#if 0
	/*
	 * pledge in the vmm process:
 	 * stdio - for malloc and basic I/O including events.
	 * vmm - for the vmm ioctls and operations.
	 * proc - for forking and maitaining vms.
	 * recvfd - for disks, interfaces and other fds.
	 */
	/* XXX'ed pledge to hide it from grep as long as it's disabled */
	if (XXX("stdio vmm recvfd proc", NULL) == -1)
		fatal("pledge");
#endif

	/* Get and terminate all running VMs */
	get_info_vm(ps, NULL, 1);
}

int
vmm_dispatch_parent(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct privsep		*ps = p->p_ps;
	int			 res = 0, cmd = 0;
	struct vm_create_params	 vcp;
	struct vm_terminate_params vtp;
	struct vmop_result	 vmr;
	uint32_t		 id = 0;
	struct vmd_vm		*vm;

	switch (imsg->hdr.type) {
	case IMSG_VMDOP_START_VM_REQUEST:
		IMSG_SIZE_CHECK(imsg, &vcp);
		memcpy(&vcp, imsg->data, sizeof(vcp));
		res = config_getvm(ps, &vcp, imsg->fd, imsg->hdr.peerid);
		if (res == -1) {
			res = errno;
			cmd = IMSG_VMDOP_START_VM_RESPONSE;
		}
		break;
	case IMSG_VMDOP_START_VM_DISK:
		res = config_getdisk(ps, imsg);
		if (res == -1) {
			res = errno;
			cmd = IMSG_VMDOP_START_VM_RESPONSE;
		}
		break;
	case IMSG_VMDOP_START_VM_IF:
		res = config_getif(ps, imsg);
		if (res == -1) {
			res = errno;
			cmd = IMSG_VMDOP_START_VM_RESPONSE;
		}
		break;
	case IMSG_VMDOP_START_VM_END:
		res = start_vm(imsg, &id);
		cmd = IMSG_VMDOP_START_VM_RESPONSE;
		break;
	case IMSG_VMDOP_TERMINATE_VM_REQUEST:
		IMSG_SIZE_CHECK(imsg, &vtp);
		memcpy(&vtp, imsg->data, sizeof(vtp));
		id = vtp.vtp_vm_id;
		res = terminate_vm(&vtp);
		cmd = IMSG_VMDOP_TERMINATE_VM_RESPONSE;
		if (res == 0) {
			/* Remove local reference */
			vm = vm_getbyid(id);
			vm_remove(vm);
		}
		break;
	case IMSG_VMDOP_GET_INFO_VM_REQUEST:
		res = get_info_vm(ps, imsg, 0);
		cmd = IMSG_VMDOP_GET_INFO_VM_END_DATA;
		break;
	case IMSG_CTL_RESET:
		config_getreset(env, imsg);
		break;
	default:
		return (-1);
	}

	switch (cmd) {
	case 0:
		break;
	case IMSG_VMDOP_START_VM_RESPONSE:
		if (res != 0) {
			vm = vm_getbyvmid(imsg->hdr.peerid);
			vm_remove(vm);
		}
	case IMSG_VMDOP_TERMINATE_VM_RESPONSE:
		memset(&vmr, 0, sizeof(vmr));
		vmr.vmr_result = res;
		vmr.vmr_id = id;
		if (proc_compose_imsg(ps, PROC_PARENT, -1, cmd,
		    imsg->hdr.peerid, -1, &vmr, sizeof(vmr)) == -1)
			return (-1);
		break;
	default:
		if (proc_compose_imsg(ps, PROC_PARENT, -1, cmd,
		    imsg->hdr.peerid, -1, &res, sizeof(res)) == -1)
			return (-1);
		break;
	}

	return (0);
}

void
vmm_sighdlr(int sig, short event, void *arg)
{
	struct privsep *ps = arg;
	int status;
	uint32_t vmid;
	pid_t pid;
	struct vmop_result vmr;
	struct vmd_vm *vm;
	struct vm_terminate_params vtp;

	switch (sig) {
	case SIGCHLD:
		do {
			pid = waitpid(-1, &status, WNOHANG);
			if (pid <= 0)
				continue;

			if (WIFEXITED(status) || WIFSIGNALED(status)) {
				vm = vm_getbypid(pid);
				if (vm == NULL) {
					/*
					 * If the VM is gone already, it
					 * got terminated via a
					 * IMSG_VMDOP_TERMINATE_VM_REQUEST.
					 */
					continue;
				}

				vmid = vm->vm_params.vcp_id;
				vtp.vtp_vm_id = vmid;
				if (terminate_vm(&vtp) == 0) {
					memset(&vmr, 0, sizeof(vmr));
					vmr.vmr_result = 0;
					vmr.vmr_id = vmid;
					vm_remove(vm);
					if (proc_compose_imsg(ps, PROC_PARENT,
					    -1, IMSG_VMDOP_TERMINATE_VM_EVENT,
					    0, -1, &vmr, sizeof(vmr)) == -1)
						log_warnx("could not signal "
						    "termination of VM %u to "
						    "parent", vmid);
				} else
					log_warnx("could not terminate VM %u",
					    vmid);
			} else
				fatalx("unexpected cause of SIGCHLD");
		} while (pid > 0 || (pid == -1 && errno == EINTR));
		break;
	default:
		fatalx("unexpected signal");
	}
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
 *  vis: the register state to initialize 
 *
 * Return values:
 *  0: success
 *  !0 : ioctl to vmm(4) failed (eg, ENOENT if the supplied VM ID is not
 *      valid)
 */
int
vcpu_reset(uint32_t vmid, uint32_t vcpu_id, struct vcpu_init_state *vis)
{
	struct vm_resetcpu_params vrp;

	memset(&vrp, 0, sizeof(vrp));
	vrp.vrp_vm_id = vmid;
	vrp.vrp_vcpu_id = vcpu_id;
	memcpy(&vrp.vrp_init_state, vis, sizeof(struct vcpu_init_state));

	if (ioctl(env->vmd_fd, VMM_IOC_RESETCPU, &vrp) < 0)
		return (errno);

	return (0);
}

/*
 * terminate_vm
 *
 * Requests vmm(4) to terminate the VM whose ID is provided in the
 * supplied vm_terminate_params structure (vtp->vtp_vm_id)
 *
 * Parameters
 *  vtp: vm_create_params struct containing the ID of the VM to terminate
 *
 * Return values:
 *  0: success
 *  !0 : ioctl to vmm(4) failed (eg, ENOENT if the supplied VM is not
 *      valid)
 */
int
terminate_vm(struct vm_terminate_params *vtp)
{
	if (ioctl(env->vmd_fd, VMM_IOC_TERM, vtp) < 0)
		return (errno);

	return (0);
}

/*
 * opentap
 *
 * Opens the next available tap device, up to MAX_TAP.
 *
 * Returns a file descriptor to the tap node opened, or -1 if no tap
 * devices were available.
 */
int
opentap(void)
{
	int i, fd;
	char path[PATH_MAX];

	for (i = 0; i < MAX_TAP; i++) {
		snprintf(path, PATH_MAX, "/dev/tap%d", i);
		fd = open(path, O_RDWR | O_NONBLOCK);
		if (fd != -1)
			return (fd);
	}

	return (-1);
}

/*
 * start_vm
 *
 * Starts a new VM with the creation parameters supplied (in the incoming
 * imsg->data field). This function performs a basic sanity check on the
 * incoming parameters and then performs the following steps to complete
 * the creation of the VM:
 *
 * 1. opens the VM disk image files specified in the VM creation parameters
 * 2. opens the specified VM kernel
 * 3. creates a VM console tty pair using openpty
 * 4. forks, passing the file descriptors opened in steps 1-3 to the child
 *     vmd responsible for dropping privilege and running the VM's VCPU
 *     loops.
 *
 * Parameters:
 *  imsg: The incoming imsg body whose 'data' field is a vm_create_params
 *      struct containing the VM creation parameters.
 *  id: Returns the VM id as reported by the kernel.
 *
 * Return values:
 *  0: success
 *  !0 : failure - typically an errno indicating the source of the failure
 */
int
start_vm(struct imsg *imsg, uint32_t *id)
{
	struct vm_create_params	*vcp;
	struct vmd_vm		*vm;
	size_t			 i;
	int			 ret = EINVAL;
	int			 fds[2];
	struct vcpu_init_state vis;

	if ((vm = vm_getbyvmid(imsg->hdr.peerid)) == NULL) {
		log_warn("%s: can't find vm", __func__);
		ret = ENOENT;
		goto err;
	}
	vcp = &vm->vm_params;

	if ((vm->vm_tty = imsg->fd) == -1) {
		log_warn("%s: can't get tty", __func__);
		goto err;
	}

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, fds) == -1)
		fatal("socketpair");

	/* Start child vmd for this VM (fork, chroot, drop privs) */
	ret = start_client_vmd();

	/* Start child failed? - cleanup and leave */
	if (ret == -1) {
		log_warn("%s: start child failed", __func__);
		ret = EIO;
		goto err;
	}

	if (ret > 0) {
		/* Parent */
		vm->vm_pid = ret;

		for (i = 0 ; i < vcp->vcp_ndisks; i++) {
			close(vm->vm_disks[i]);
			vm->vm_disks[i] = -1;
		}

		for (i = 0 ; i < vcp->vcp_nnics; i++) {
			close(vm->vm_ifs[i]);
			vm->vm_ifs[i] = -1;
		}

		close(vm->vm_kernel);
		vm->vm_kernel = -1;

		close(vm->vm_tty);
		vm->vm_tty = -1;

		/* read back the kernel-generated vm id from the child */
		close(fds[1]);
		if (read(fds[0], &vcp->vcp_id, sizeof(vcp->vcp_id)) !=
		    sizeof(vcp->vcp_id))
			fatal("read vcp id");
		close(fds[0]);

		if (vcp->vcp_id == 0)
			goto err;

		*id = vcp->vcp_id;

		return (0);
	} else {
		/* Child */
		setproctitle(vcp->vcp_name);
		log_procinit(vcp->vcp_name);

		create_memory_map(vcp);
		ret = alloc_guest_mem(vcp);
		if (ret) {
			errno = ret;
			fatal("could not allocate guest memory - exiting");
		}

		ret = vmm_create_vm(vcp);
		current_vm = vm;

		/* send back the kernel-generated vm id (0 on error) */
		close(fds[0]);
		if (write(fds[1], &vcp->vcp_id, sizeof(vcp->vcp_id)) !=
		    sizeof(vcp->vcp_id))
			fatal("write vcp id");
		close(fds[1]);

		if (ret) {
			errno = ret;
			fatal("create vmm ioctl failed - exiting");
		}

#if 0
		/*
		 * pledge in the vm processes:
	 	 * stdio - for malloc and basic I/O including events.
		 * vmm - for the vmm ioctls and operations.
		 */
		if (XXX("stdio vmm", NULL) == -1)
			fatal("pledge");
#endif

		/*
		 * Set up default "flat 32 bit" register state - RIP,
		 * RSP, and GDT info will be set in bootloader
	 	 */
		memcpy(&vis, &vcpu_init_flat32, sizeof(struct vcpu_init_state));

		/* Load kernel image */
		ret = loadelf_main(vm->vm_kernel, vcp, &vis);
		if (ret) {
			errno = ret;
			fatal("failed to load kernel - exiting");
		}

		close(vm->vm_kernel);

		con_fd = vm->vm_tty;
		if (fcntl(con_fd, F_SETFL, O_NONBLOCK) == -1)
			fatal("failed to set nonblocking mode on console");

		/* Execute the vcpu run loop(s) for this VM */
		ret = run_vm(vm->vm_disks, vm->vm_ifs, vcp, &vis);

		_exit(ret != 0);
	}

	return (0);

 err:
	vm_remove(vm);

	return (ret);
}

/*
 * get_info_vm
 *
 * Returns a list of VMs known to vmm(4).
 *
 * Parameters:
 *  ps: the privsep context.
 *  imsg: the received imsg including the peer id.
 *  terminate: terminate the listed vm.
 *
 * Return values:
 *  0: success
 *  !0 : failure (eg, ENOMEM, EIO or another error code from vmm(4) ioctl)
 */
int
get_info_vm(struct privsep *ps, struct imsg *imsg, int terminate)
{
	int ret;
	size_t ct, i;
	struct vm_info_params vip;
	struct vm_info_result *info;
	struct vm_terminate_params vtp;
	struct vmop_info_result vir;

	/*
	 * We issue the VMM_IOC_INFO ioctl twice, once with an input
	 * buffer size of 0, which results in vmm(4) returning the
	 * number of bytes required back to us in vip.vip_size,
	 * and then we call it again after malloc'ing the required
	 * number of bytes.
	 *
	 * It is possible that we could fail a second time (eg, if
	 * another VM was created in the instant between the two
	 * ioctls, but in that case the caller can just try again
	 * as vmm(4) will return a zero-sized list in that case.
	 */
	vip.vip_size = 0;
	info = NULL;
	ret = 0;
	memset(&vir, 0, sizeof(vir));

	/* First ioctl to see how many bytes needed (vip.vip_size) */
	if (ioctl(env->vmd_fd, VMM_IOC_INFO, &vip) < 0)
		return (errno);

	if (vip.vip_info_ct != 0)
		return (EIO);

	info = malloc(vip.vip_size);
	if (info == NULL)
		return (ENOMEM);

	/* Second ioctl to get the actual list */
	vip.vip_info = info;
	if (ioctl(env->vmd_fd, VMM_IOC_INFO, &vip) < 0) {
		ret = errno;
		free(info);
		return (ret);
	}

	/* Return info */
	ct = vip.vip_size / sizeof(struct vm_info_result);
	for (i = 0; i < ct; i++) {
		if (terminate) {
			vtp.vtp_vm_id = info[i].vir_id;
			if ((ret = terminate_vm(&vtp)) != 0)
				return (ret);
			log_debug("%s: terminated id %d",
			    info[i].vir_name, info[i].vir_id);
			continue;
		}
		memcpy(&vir.vir_info, &info[i], sizeof(vir.vir_info));
		if (proc_compose_imsg(ps, PROC_PARENT, -1,
		    IMSG_VMDOP_GET_INFO_VM_DATA, imsg->hdr.peerid, -1,
		    &vir, sizeof(vir)) == -1)
			return (EIO);
	}
	free(info);
	return (0);
}


/*
 * start_client_vmd
 *
 * forks a copy of the parent vmd, chroots to VMD_USER's home, drops
 * privileges (changes to user VMD_USER), and returns.
 * Should the fork operation succeed, but later chroot/privsep
 * fail, the child exits.
 *
 * Return values (returns to both child and parent on success):
 *  -1 : failure
 *  0: return to child vmd returns 0
 *  !0 : return to parent vmd returns the child's pid
 */
int
start_client_vmd(void)
{
	int child_pid;

	child_pid = fork();
	if (child_pid < 0)
		return (-1);

	if (!child_pid) {
		/* child, already running without privileges */
		return (0);
	}

	/* Parent */
	return (child_pid);
}

/*
 * create_memory_map
 *
 * Sets up the guest physical memory ranges that the VM can access.
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
init_emulated_hw(struct vm_create_params *vcp, int *child_disks,
    int *child_taps)
{
	int i;

	/* Reset the IO port map */
	memset(&ioports_map, 0, sizeof(io_fn_t) * MAX_PORTS);
	
	/* Init the i8253 PIT's 3 counters */
	memset(&i8253_counter, 0, sizeof(struct i8253_counter) * 3);
	gettimeofday(&i8253_counter[0].tv, NULL);
	gettimeofday(&i8253_counter[1].tv, NULL);
	gettimeofday(&i8253_counter[2].tv, NULL);
	i8253_counter[0].start = TIMER_DIV(100);
	i8253_counter[1].start = TIMER_DIV(100);
	i8253_counter[2].start = TIMER_DIV(100);
	ioports_map[TIMER_CTRL] = vcpu_exit_i8253;
	ioports_map[TIMER_BASE + TIMER_CNTR0] = vcpu_exit_i8253;
	ioports_map[TIMER_BASE + TIMER_CNTR1] = vcpu_exit_i8253;
	ioports_map[TIMER_BASE + TIMER_CNTR2] = vcpu_exit_i8253;

	/* Init ns8250 UART */
	memset(&com1_regs, 0, sizeof(struct ns8250_regs));
	for (i = COM1_DATA; i <= COM1_SCR; i++)
		ioports_map[i] = vcpu_exit_com;

	/* Initialize PCI */
	for (i = VMM_PCI_IO_BAR_BASE; i <= VMM_PCI_IO_BAR_END; i++)
		ioports_map[i] = vcpu_exit_pci;
	
	ioports_map[PCI_MODE1_ADDRESS_REG] = vcpu_exit_pci;
	ioports_map[PCI_MODE1_DATA_REG] = vcpu_exit_pci;
	pci_init();

	/* Initialize virtio devices */
	virtio_init(vcp, child_disks, child_taps);
}

/*
 * run_vm
 *
 * Runs the VM whose creation parameters are specified in vcp
 *
 * Parameters:
 *  child_disks: previously-opened child VM disk file file descriptors
 *  child_taps: previously-opened child tap file descriptors
 *  vcp: vm_create_params struct containing the VM's desired creation
 *      configuration
 *  vis: VCPU register state to initialize
 *
 * Return values:
 *  0: the VM exited normally
 *  !0 : the VM exited abnormally or failed to start
 */
int
run_vm(int *child_disks, int *child_taps, struct vm_create_params *vcp,
    struct vcpu_init_state *vis)
{
	size_t i;
	int ret;
	pthread_t *tid;
	void *exit_status;
	struct vm_run_params **vrp;

	if (vcp == NULL)
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

	ret = 0;

	tid = calloc(vcp->vcp_ncpus, sizeof(pthread_t));
	vrp = calloc(vcp->vcp_ncpus, sizeof(struct vm_run_params *));
	if (tid == NULL || vrp == NULL) {
		log_warn("%s: memory allocation error - exiting.",
		    __progname);
		return (ENOMEM);
	}

	init_emulated_hw(vcp, child_disks, child_taps);

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
			/* caller will exit, so skip free'ing */
			return (ENOMEM);
		}
		vrp[i]->vrp_exit = malloc(sizeof(union vm_exit));
		if (vrp[i]->vrp_exit == NULL) {
			log_warn("%s: memory allocation error - "
			    "exiting.", __progname);
			/* caller will exit, so skip free'ing */
			return (ENOMEM);
		}
		vrp[i]->vrp_vm_id = vcp->vcp_id;
		vrp[i]->vrp_vcpu_id = i;

		if (vcpu_reset(vcp->vcp_id, i, vis)) {
			log_warn("%s: cannot reset VCPU %zu - exiting.",
			    __progname, i);
			return (EIO);
		}

		/* Start each VCPU run thread at vcpu_run_loop */
		ret = pthread_create(&tid[i], NULL, vcpu_run_loop, vrp[i]);
		if (ret) {
			/* caller will _exit after this return */
			return (ret);
		}
	}

	/* Wait for all the threads to exit */
	for (i = 0; i < vcp->vcp_ncpus; i++) {
		if (pthread_join(tid[i], &exit_status)) {
			log_warn("%s: failed to join thread %zd - "
			    "exiting", __progname, i);
			return (EIO);
		}

		if (exit_status != NULL) {
			log_warnx("%s: vm %d vcpu run thread %zd exited "
			    "abnormally", __progname, vcp->vcp_id, i);
			ret = EIO;
		}
	}

	return (ret);
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
	intptr_t ret;

	vrp->vrp_continue = 0;
	vrp->vrp_injint = -1;

	for (;;) {
		if (ioctl(env->vmd_fd, VMM_IOC_RUN, vrp) < 0) {
			/* If run ioctl failed, exit */
			ret = errno;
			return ((void *)ret);
		}

		/* If the VM is terminating, exit normally */
		if (vrp->vrp_exit_reason == VM_EXIT_TERMINATED)
			return (NULL);

		if (vrp->vrp_exit_reason != VM_EXIT_NONE) {
			/*
			 * vmm(4) needs help handling an exit, handle in
			 * vcpu_exit.
			 */
			if (vcpu_exit(vrp))
				return ((void *)EIO);
		}
	}

	return (NULL);
}

/*
 * vcpu_exit_i8253
 *
 * Handles emulated i8253 PIT access (in/out instruction to PIT ports).
 * We don't emulate all the modes of the i8253, just the basic squarewave
 * clock.
 *
 * Parameters:
 *  vrp: vm run parameters containing exit information for the I/O
 *      instruction being performed
 *
 * Return value:
 *  Interrupt to inject to the guest VM, or 0xFF if no interrupt should
 *      be injected.
 */
uint8_t
vcpu_exit_i8253(struct vm_run_params *vrp)
{
	uint32_t out_data;
	uint8_t sel, rw, data;
	uint64_t ns, ticks;
	struct timeval now, delta;
	union vm_exit *vei = vrp->vrp_exit;

	if (vei->vei.vei_port == TIMER_CTRL) {
		if (vei->vei.vei_dir == 0) { /* OUT instruction */
			out_data = vei->vei.vei_data;
			sel = out_data &
			    (TIMER_SEL0 | TIMER_SEL1 | TIMER_SEL2);
			sel = sel >> 6;
			if (sel > 2) {
				log_warnx("%s: i8253 PIT: invalid "
				    "timer selected (%d)",
				    __progname, sel);
				goto ret;
			}

			rw = vei->vei.vei_data & (TIMER_LATCH | TIMER_16BIT);

			if ((rw & TIMER_16BIT) == TIMER_LSB ||
			    (rw & TIMER_16BIT) == TIMER_MSB) {
				log_warnx("%s: i8253 PIT: invalid timer mode "
				    "0x%x selected", __func__,
				    (rw & TIMER_16BIT));
			}

			/*
			 * Since we don't truly emulate each tick of the PIT
			 * clock, when the guest asks for the timer to be
			 * latched, simulate what the counter would have been
			 * had we performed full emulation. We do this by
			 * calculating when the counter was reset vs how much
			 * time has elapsed, then bias by the counter tick
			 * rate.
			 */
			if (rw == TIMER_LATCH) {
				gettimeofday(&now, NULL);
				delta.tv_sec = now.tv_sec -
				    i8253_counter[sel].tv.tv_sec;
				delta.tv_usec = now.tv_usec -
				    i8253_counter[sel].tv.tv_usec;
				if (delta.tv_usec < 0) {
					delta.tv_sec--;
					delta.tv_usec += 1000000;
				}
				if (delta.tv_usec > 1000000) {
					delta.tv_sec++;
					delta.tv_usec -= 1000000;
				}
				ns = delta.tv_usec * 1000 +
				    delta.tv_sec * 1000000000;
				ticks = ns / NS_PER_TICK;
				i8253_counter[sel].olatch =
				    i8253_counter[sel].start -
				    ticks % i8253_counter[sel].start;
				goto ret;
			}

			goto ret;
		} else {
			log_warnx("%s: i8253 PIT: read from control "
			    "port unsupported - returning 0", __progname);
			vei->vei.vei_data = 0;
		}
	} else {
		sel = vei->vei.vei_port - (TIMER_CNTR0 + TIMER_BASE);
		if (vei->vei.vei_dir == 0) { /* OUT instruction */
			if (i8253_counter[sel].last_w == 0) {
				out_data = vei->vei.vei_data;
				i8253_counter[sel].ilatch |= (out_data << 8);
				i8253_counter[sel].last_w = 1;
			} else {
				out_data = vei->vei.vei_data;
				i8253_counter[sel].ilatch |= out_data;
				i8253_counter[sel].start =
				    i8253_counter[sel].ilatch;
				i8253_counter[sel].last_w = 0;
			}
		} else {
			if (i8253_counter[sel].last_r == 0) {
				data = i8253_counter[sel].olatch >> 8;
				vei->vei.vei_data = data;
				i8253_counter[sel].last_w = 1;
			} else {
				data = i8253_counter[sel].olatch & 0xFF;
				vei->vei.vei_data = data;
				i8253_counter[sel].last_w = 0;
			}
		}
	}

ret:
	/* XXX don't yet support interrupts generated from the 8253 */
	return (0xFF);
}

/*
 * vcpu_process_com_data
 *
 * Emulate in/out instructions to the com1 (ns8250) UART data register
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_data(union vm_exit *vei)
{
	/*
	 * vei_dir == 0 : out instruction
	 *
	 * The guest wrote to the data register. Since we are emulating a
	 * no-fifo chip, write the character immediately to the pty and
	 * assert TXRDY in IIR (if the guest has requested TXRDY interrupt
	 * reporting)
	 */
	if (vei->vei.vei_dir == 0) {
		write(con_fd, &vei->vei.vei_data, 1);
		if (com1_regs.ier & 0x2) {
			/* Set TXRDY */
			com1_regs.iir |= IIR_TXRDY;
			/* Set "interrupt pending" (IIR low bit cleared) */
			com1_regs.iir &= ~0x1;
		}
	} else {
		/*
		 * vei_dir == 1 : in instruction
		 *
		 * The guest read from the data register. Check to see if
		 * there is data available (RXRDY) and if so, consume the
		 * input data and return to the guest. Also clear the
		 * interrupt info register regardless.
		 */
		if (com1_regs.lsr & LSR_RXRDY) {
			vei->vei.vei_data = com1_regs.data;
			com1_regs.data = 0x0;
			com1_regs.lsr &= ~LSR_RXRDY;
		} else {
			/* XXX should this be com1_regs.data or 0xff? */
			vei->vei.vei_data = com1_regs.data;
			log_warnx("guest reading com1 when not ready");
		}

		/* Reading the data register always clears RXRDY from IIR */
		com1_regs.iir &= ~IIR_RXRDY;

		/*
		 * Clear "interrupt pending" by setting IIR low bit to 1
		 * if no interrupt are pending
		 */
		if (com1_regs.iir == 0x0)
			com1_regs.iir = 0x1;
	}
}

/*
 * vcpu_process_com_lcr
 *
 * Emulate in/out instructions to the com1 (ns8250) UART line control register
 *
 * Paramters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_lcr(union vm_exit *vei)
{
	/*
	 * vei_dir == 0 : out instruction
	 *
	 * Write content to line control register
	 */
	if (vei->vei.vei_dir == 0) {
		com1_regs.lcr = (uint8_t)vei->vei.vei_data;
	} else {
		/*
		 * vei_dir == 1 : in instruction
		 *
		 * Read line control register
		 */
		vei->vei.vei_data = com1_regs.lcr;
	}
}

/*
 * vcpu_process_com_iir
 *
 * Emulate in/out instructions to the com1 (ns8250) UART interrupt information
 * register. Note that writes to this register actually are to a different
 * register, the FCR (FIFO control register) that we don't emulate but still
 * consume the data provided.
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_iir(union vm_exit *vei)
{
	/*
	 * vei_dir == 0 : out instruction
	 *
	 * Write to FCR
	 */
	if (vei->vei.vei_dir == 0) {
		com1_regs.fcr = vei->vei.vei_data;
	} else {
		/*
		 * vei_dir == 1 : in instruction
		 *
		 * Read IIR. Reading the IIR resets the TXRDY bit in the IIR
		 * after the data is read.
		 */
		vei->vei.vei_data = com1_regs.iir;
		com1_regs.iir &= ~IIR_TXRDY;

		/*
		 * Clear "interrupt pending" by setting IIR low bit to 1
		 * if no interrupts are pending
		 */
		if (com1_regs.iir == 0x0)
			com1_regs.iir = 0x1;
	}
}

/*
 * vcpu_process_com_mcr
 *
 * Emulate in/out instructions to the com1 (ns8250) UART modem control
 * register.
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_mcr(union vm_exit *vei)
{
	/*
	 * vei_dir == 0 : out instruction
	 *
	 * Write to MCR
	 */
	if (vei->vei.vei_dir == 0) {
		com1_regs.mcr = vei->vei.vei_data;
	} else {
		/*
		 * vei_dir == 1 : in instruction
		 *
		 * Read from MCR
		 */
		vei->vei.vei_data = com1_regs.mcr;
	}
}

/*
 * vcpu_process_com_lsr
 *
 * Emulate in/out instructions to the com1 (ns8250) UART line status register.
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_lsr(union vm_exit *vei)
{
	/*
	 * vei_dir == 0 : out instruction
	 *
	 * Write to LSR. This is an illegal operation, so we just log it and
	 * continue.
	 */
	if (vei->vei.vei_dir == 0) {
		log_warnx("%s: LSR UART write 0x%x unsupported",
		    __progname, vei->vei.vei_data);
	} else {
		/*
		 * vei_dir == 1 : in instruction
		 *
		 * Read from LSR. We always report TXRDY and TSRE since we
		 * can process output characters immediately (at any time).
		 */
		vei->vei.vei_data = com1_regs.lsr | LSR_TSRE | LSR_TXRDY;
	}
}

/*
 * vcpu_process_com_msr
 *
 * Emulate in/out instructions to the com1 (ns8250) UART modem status register.
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_msr(union vm_exit *vei)
{
	/*
	 * vei_dir == 0 : out instruction
	 *
	 * Write to MSR. This is an illegal operation, so we just log it and
	 * continue.
	 */
	if (vei->vei.vei_dir == 0) {
		log_warnx("%s: MSR UART write 0x%x unsupported",
		    __progname, vei->vei.vei_data);
	} else {
		/*
		 * vei_dir == 1 : in instruction
		 *
		 * Read from MSR. We always report DCD, DSR, and CTS.
		 */
		vei->vei.vei_data = com1_regs.lsr | MSR_DCD | MSR_DSR | MSR_CTS;
	}
}

/*
 * vcpu_process_com_scr
 *
 * Emulate in/out instructions to the com1 (ns8250) UART scratch register. The
 * scratch register is sometimes used to distinguish an 8250 from a 16450,
 * and/or used to distinguish submodels of the 8250 (eg 8250A, 8250B). We
 * simulate an "original" 8250 by forcing the scratch register to return data
 * on read that is different from what was written.
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_scr(union vm_exit *vei)
{
	/*
	 * vei_dir == 0 : out instruction
	 *
	 * Write to SCR
	 */
	if (vei->vei.vei_dir == 0) {
		com1_regs.scr = vei->vei.vei_data;
	} else {
		/*
		 * vei_dir == 1 : in instruction
		 *
		 * Read from SCR. To make sure we don't accidentally simulate
		 * a real scratch register, we negate what was written on
		 * subsequent readback.
		 */
		vei->vei.vei_data = ~com1_regs.scr;
	}
}

/*
 * vcpu_process_com_ier
 *
 * Emulate in/out instructions to the com1 (ns8250) UART interrupt enable
 * register.
 *
 * Parameters:
 *  vei: vm exit information from vmm(4) containing information on the in/out
 *      instruction being performed
 */
void
vcpu_process_com_ier(union vm_exit *vei)
{
	/*
	 * vei_dir == 0 : out instruction
	 *
	 * Write to IER
	 */
	if (vei->vei.vei_dir == 0) {
		com1_regs.ier = vei->vei.vei_data;
	} else {
		/*
		 * vei_dir == 1 : in instruction
		 *
		 * Read from IER
		 */
		vei->vei.vei_data = com1_regs.ier;
	}
}

/*
 * vcpu_exit_com
 *
 * Process com1 (ns8250) UART exits. vmd handles most basic 8250
 * features with the exception of the divisor latch (eg, no baud
 * rate support)
 *
 * Parameters:
 *  vrp: vcpu run parameters containing guest state for this exit
 *
 * Return value:
 *  Interrupt to inject to the guest VM, or 0xFF if no interrupt should
 *      be injected.
 */
uint8_t
vcpu_exit_com(struct vm_run_params *vrp)
{
	union vm_exit *vei = vrp->vrp_exit;

	switch (vei->vei.vei_port) {
	case COM1_LCR:
		vcpu_process_com_lcr(vei);
		break;
	case COM1_IER:
		vcpu_process_com_ier(vei);
		break;
	case COM1_IIR:
		vcpu_process_com_iir(vei);
		break;
	case COM1_MCR:
		vcpu_process_com_mcr(vei);
		break;
	case COM1_LSR:
		vcpu_process_com_lsr(vei);
		break;
	case COM1_MSR:
		vcpu_process_com_msr(vei);
		break;
	case COM1_SCR:
		vcpu_process_com_scr(vei);
		break;
	case COM1_DATA:
		vcpu_process_com_data(vei);
		break;
	}

	return (0xFF);
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
	union vm_exit *vei = vrp->vrp_exit;
	uint8_t intr;

	intr = 0xFF;

	switch (vei->vei.vei_port) {
	case PCI_MODE1_ADDRESS_REG:
		pci_handle_address_reg(vrp);
		break;
	case PCI_MODE1_DATA_REG:
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
 * i8253 PIT and the com1 ns8250 UART.
 *
 * Parameters:
 *  vrp: vcpu run parameters containing guest state for this exit
 */
void
vcpu_exit_inout(struct vm_run_params *vrp)
{
	union vm_exit *vei = vrp->vrp_exit;
	uint8_t intr = 0xFF;

	if (ioports_map[vei->vei.vei_port] != NULL)
		intr = ioports_map[vei->vei.vei_port](vrp);
	else if (vei->vei.vei_dir == 1)
			vei->vei.vei_data = 0xFFFFFFFF;
	
	if (intr != 0xFF)
		vrp->vrp_injint = intr;
	else
		vrp->vrp_injint = -1;
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
 * interrupts should be injected into the guest, and sets vrp->vrp_injint
 * to the IRQ line whose interrupt should be vectored (or -1 if no interrupt
 * is to be injected).
 *
 * Parameters:
 *  vrp: vcpu run parameters containing guest state for this exit
 *
 * Return values:
 *  0: the exit was handled successfully
 *  1: an error occurred (exit not handled)
 */
int
vcpu_exit(struct vm_run_params *vrp)
{
	ssize_t sz;
	char ch;

	switch (vrp->vrp_exit_reason) {
	case VMX_EXIT_IO:
		vcpu_exit_inout(vrp);
		break;
	case VMX_EXIT_HLT:
		/*
		 * XXX handle halted state, no reason to run this vcpu again
		 * until a vm interrupt is to be injected
		 */
		break;
	default:
		log_warnx("%s: unknown exit reason %d",
		    __progname, vrp->vrp_exit_reason);
		return (1);
	}

	/* XXX interrupt priority */
	if (vionet_process_rx())
		vrp->vrp_injint = 9;

	/*
	 * Is there a new character available on com1?
	 * If so, consume the character, buffer it into the com1 data register
	 * assert IRQ4, and set the line status register RXRDY bit.
	 *
	 * XXX - move all this com intr checking to another function
	 */
	sz = read(con_fd, &ch, sizeof(char));
	if (sz == 1) {
		com1_regs.lsr |= LSR_RXRDY;
		com1_regs.data = ch;
		/* XXX these ier and iir bits should be IER_x and IIR_x */
		if (com1_regs.ier & 0x1) {
			com1_regs.iir |= (2 << 1);
			com1_regs.iir &= ~0x1;
		}
	}

	/*
	 * Clear "interrupt pending" by setting IIR low bit to 1 if no
	 * interrupts are pending
	 */
	/* XXX these iir magic numbers should be IIR_x */
	if ((com1_regs.iir & ~0x1) == 0x0)
		com1_regs.iir = 0x1;

	/* If pending interrupt and nothing waiting to be injected, inject */
	if ((com1_regs.iir & 0x1) == 0)
		if (vrp->vrp_injint == -1)
			vrp->vrp_injint = 0x4;
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

/*
 * write_mem
 *
 * Pushes data from 'buf' into the guest VM's memory at paddr 'dst'.
 *
 * Parameters:
 *  dst: the destination paddr_t in the guest VM to push into.
 *  buf: data to push
 *  len: size of 'buf'
 *
 * Return values:
 *  0: success
 *  EINVAL: if the guest physical memory range [dst, dst + len) does not
 *      exist in the guest.
 */
int
write_mem(paddr_t dst, void *buf, size_t len)
{
	char *from = buf, *to;
	size_t n, off;
	struct vm_mem_range *vmr;

	vmr = find_gpa_range(&current_vm->vm_params, dst, len);
	if (vmr == NULL) {
		errno = EINVAL;
		log_warn("writepage ioctl failed: "
		    "invalid memory range dst = 0x%lx, len = 0x%zx", dst, len);
		return (EINVAL);
	}

	off = dst - vmr->vmr_gpa;
	while (len != 0) {
		n = vmr->vmr_size - off;
		if (len < n)
			n = len;

		to = (char *)vmr->vmr_va + off;
		memcpy(to, from, n);

		from += n;
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
 *  len: size of 'buf'
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

	vmr = find_gpa_range(&current_vm->vm_params, src, len);
	if (vmr == NULL) {
		errno = EINVAL;
		log_warn("readpage ioctl failed: "
		    "invalid memory range src = 0x%lx, len = 0x%zx", src, len);
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
 * fd_hasdata
 *
 * Returns 1 if data can be read from an fd, or 0 otherwise.
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
