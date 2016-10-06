/*	$OpenBSD: vmm.c,v 1.49 2016/10/06 20:41:28 reyk Exp $	*/

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

#include <sys/param.h>	/* nitems */
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

io_fn_t ioports_map[MAX_PORTS];

void vmm_sighdlr(int, short, void *);
int start_client_vmd(void);
int opentap(char *);
int start_vm(struct imsg *, uint32_t *);
int terminate_vm(struct vm_terminate_params *);
int get_info_vm(struct privsep *, struct imsg *, int);
int run_vm(int *, int *, struct vm_create_params *, struct vcpu_reg_state *);
void *event_thread(void *);
void *vcpu_run_loop(void *);
int vcpu_exit(struct vm_run_params *);
int vcpu_reset(uint32_t, uint32_t, struct vcpu_reg_state *);
void create_memory_map(struct vm_create_params *);
int alloc_guest_mem(struct vm_create_params *);
int vmm_create_vm(struct vm_create_params *);
void init_emulated_hw(struct vm_create_params *, int *, int *);
void vcpu_exit_inout(struct vm_run_params *);
uint8_t vcpu_exit_pci(struct vm_run_params *);
int vmm_dispatch_parent(int, struct privsep_proc *, struct imsg *);
void vmm_run(struct privsep *, struct privsep_proc *, void *);
int vcpu_pic_intr(uint32_t, uint32_t, uint8_t);

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
static const struct vcpu_reg_state vcpu_init_flat32 = {
	.vrs_gprs[VCPU_REGS_RFLAGS] = 0x2,
	.vrs_gprs[VCPU_REGS_RIP] = 0x0,
	.vrs_gprs[VCPU_REGS_RSP] = 0x0,
	.vrs_crs[VCPU_REGS_CR0] = CR0_CD | CR0_NW | CR0_ET | CR0_PE | CR0_PG,
	.vrs_crs[VCPU_REGS_CR3] = PML4_PAGE,
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
};

void
vmm(struct privsep *ps, struct privsep_proc *p)
{
	proc_run(ps, p, procs, nitems(procs), vmm_run, NULL);
}

void
vmm_run(struct privsep *ps, struct privsep_proc *p, void *arg)
{
	if (config_init(ps->ps_env) == -1)
		fatal("failed to initialize configuration");

	signal_del(&ps->ps_evsigchld);
	signal_set(&ps->ps_evsigchld, SIGCHLD, vmm_sighdlr, ps);
	signal_add(&ps->ps_evsigchld, NULL);

	/*
	 * pledge in the vmm process:
 	 * stdio - for malloc and basic I/O including events.
	 * vmm - for the vmm ioctls and operations.
	 * proc - for forking and maitaining vms.
	 * recvfd - for disks, interfaces and other fds.
	 */
	if (pledge("stdio vmm recvfd proc", NULL) == -1)
		fatal("pledge");

	/* Get and terminate all running VMs */
	get_info_vm(ps, NULL, 1);
}

int
vmm_dispatch_parent(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct privsep		*ps = p->p_ps;
	int			 res = 0, cmd = 0;
	struct vmop_create_params vmc;
	struct vm_terminate_params vtp;
	struct vmop_result	 vmr;
	uint32_t		 id = 0;
	struct vmd_vm		*vm;

	switch (imsg->hdr.type) {
	case IMSG_VMDOP_START_VM_REQUEST:
		IMSG_SIZE_CHECK(imsg, &vmc);
		memcpy(&vmc, imsg->data, sizeof(vmc));
		res = config_getvm(ps, &vmc, imsg->fd, imsg->hdr.peerid);
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
 * vmm_shutdown
 * 
 * Terminate VMs on shutdown to avoid "zombie VM" processes.
 */
void
vmm_shutdown(void)
{
	struct vm_terminate_params vtp;
	struct vmd_vm *vm, *vm_next;

	TAILQ_FOREACH_SAFE(vm, env->vmd_vms, vm_entry, vm_next) {
		vtp.vtp_vm_id = vm->vm_params.vcp_id;

		/* XXX suspend or request graceful shutdown */
		terminate_vm(&vtp);
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
 * Parameters
 *  ifname: an optional buffer of at least IF_NAMESIZE bytes.
 *
 * Returns a file descriptor to the tap node opened, or -1 if no tap
 * devices were available.
 */
int
opentap(char *ifname)
{
	int i, fd;
	char path[PATH_MAX];

	strlcpy(ifname, "tap", IF_NAMESIZE);
	for (i = 0; i < MAX_TAP; i++) {
		snprintf(path, PATH_MAX, "/dev/tap%d", i);
		fd = open(path, O_RDWR | O_NONBLOCK);
		if (fd != -1) {
			if (ifname != NULL)
				snprintf(ifname, IF_NAMESIZE, "tap%d", i);
			return (fd);
		}
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
	int			 fds[2], nicfds[VMM_MAX_NICS_PER_VM];
	struct vcpu_reg_state vrs;

	if ((vm = vm_getbyvmid(imsg->hdr.peerid)) == NULL) {
		log_warnx("%s: can't find vm", __func__);
		ret = ENOENT;
		goto err;
	}
	vcp = &vm->vm_params;

	if ((vm->vm_tty = imsg->fd) == -1) {
		log_warnx("%s: can't get tty", __func__);
		goto err;
	}

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, fds) == -1)
		fatal("socketpair");

	/* Start child vmd for this VM (fork, chroot, drop privs) */
	ret = start_client_vmd();

	/* Start child failed? - cleanup and leave */
	if (ret == -1) {
		log_warnx("%s: start child failed", __func__);
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
			close(vm->vm_ifs[i].vif_fd);
			vm->vm_ifs[i].vif_fd = -1;
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
		setproctitle("%s", vcp->vcp_name);
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

		/*
		 * pledge in the vm processes:
	 	 * stdio - for malloc and basic I/O including events.
		 * vmm - for the vmm ioctls and operations.
		 */
		if (pledge("stdio vmm", NULL) == -1)
			fatal("pledge");

		/*
		 * Set up default "flat 32 bit" register state - RIP,
		 * RSP, and GDT info will be set in bootloader
	 	 */
		memcpy(&vrs, &vcpu_init_flat32, sizeof(struct vcpu_reg_state));

		/* Load kernel image */
		ret = loadelf_main(vm->vm_kernel, vcp, &vrs);
		if (ret) {
			errno = ret;
			fatal("failed to load kernel - exiting");
		}

		close(vm->vm_kernel);

		con_fd = vm->vm_tty;
		if (fcntl(con_fd, F_SETFL, O_NONBLOCK) == -1)
			fatal("failed to set nonblocking mode on console");

		for (i = 0; i < VMM_MAX_NICS_PER_VM; i++)
			nicfds[i] = vm->vm_ifs[i].vif_fd;

		/* Execute the vcpu run loop(s) for this VM */
		ret = run_vm(vm->vm_disks, nicfds, vcp, &vrs);

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
			log_debug("%s: terminated VM %s (id %d)", __func__,
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
	
	/* Init i8253 PIT */
	i8253_init(vcp->vcp_id);
	ioports_map[TIMER_CTRL] = vcpu_exit_i8253;
	ioports_map[TIMER_BASE + TIMER_CNTR0] = vcpu_exit_i8253;
	ioports_map[TIMER_BASE + TIMER_CNTR1] = vcpu_exit_i8253;
	ioports_map[TIMER_BASE + TIMER_CNTR2] = vcpu_exit_i8253;

	/* Init mc146818 RTC */
	mc146818_init(vcp->vcp_id);
	ioports_map[IO_RTC] = vcpu_exit_mc146818;
	ioports_map[IO_RTC + 1] = vcpu_exit_mc146818;

	/* Init master and slave PICs */
	i8259_init();
	ioports_map[IO_ICU1] = vcpu_exit_i8259;
	ioports_map[IO_ICU1 + 1] = vcpu_exit_i8259;
	ioports_map[IO_ICU2] = vcpu_exit_i8259;
	ioports_map[IO_ICU2 + 1] = vcpu_exit_i8259;

	/* Init ns8250 UART */
	ns8250_init(con_fd, vcp->vcp_id);
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
 *  vrs: VCPU register state to initialize
 *
 * Return values:
 *  0: the VM exited normally
 *  !0 : the VM exited abnormally or failed to start
 */
int
run_vm(int *child_disks, int *child_taps, struct vm_create_params *vcp,
    struct vcpu_reg_state *vrs)
{
	uint8_t evdone = 0;
	size_t i;
	int ret;
	pthread_t *tid, evtid;
	struct vm_run_params **vrp;
	void *exit_status;

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

	event_init();

	tid = calloc(vcp->vcp_ncpus, sizeof(pthread_t));
	vrp = calloc(vcp->vcp_ncpus, sizeof(struct vm_run_params *));
	if (tid == NULL || vrp == NULL) {
		log_warn("%s: memory allocation error - exiting.",
		    __progname);
		return (ENOMEM);
	}

	log_debug("%s: initializing hardware for vm %s", __func__,
	    vcp->vcp_name);

	init_emulated_hw(vcp, child_disks, child_taps);

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

		if (vcpu_reset(vcp->vcp_id, i, vrs)) {
			log_warnx("%s: cannot reset VCPU %zu - exiting.",
			    __progname, i);
			return (EIO);
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

			if (exit_status != NULL) {
				log_warnx("%s: vm %d vcpu run thread %zd "
				    "exited abnormally", __progname,
				    vcp->vcp_id, i);
				return (EIO);
			}
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

		/* Did all VCPU threads exit successfully? => return 0 */
		for (i = 0; i < vcp->vcp_ncpus; i++) {
			if (vcpu_done[i] == 0)
				break;
		}
		if (i == vcp->vcp_ncpus)
			return (0);

		/* Some more threads to wait for, start over */

	}

	return (0);
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

		/* If we are halted, wait */
		if (vcpu_hlt[n]) {
			ret = pthread_cond_wait(&vcpu_run_cond[n],
			    &vcpu_run_mtx[n]);

			if (ret) {
				log_warnx("%s: can't wait on cond (%d)",
				    __func__, (int)ret);
				(void)pthread_mutex_unlock(&vcpu_run_mtx[n]);
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
			irq = i8259_ack();
			vrp->vrp_irq = irq;
		} else
			vrp->vrp_irq = 0xFFFF;

		/* Still more pending? */
		if (i8259_is_pending()) {
			/* XXX can probably avoid ioctls here by providing intr in vrp */
			if (vcpu_pic_intr(vrp->vrp_vm_id, vrp->vrp_vcpu_id, 1)) {
				fatal("can't set INTR");
			}
		} else {
			if (vcpu_pic_intr(vrp->vrp_vm_id, vrp->vrp_vcpu_id, 0)) {
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
			if (vcpu_exit(vrp)) {
				ret = EIO;
				break;
			}
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
 * i8253 PIT, the com1 ns8250 UART, and the MC146818 RTC/NVRAM device.
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
	else if (vei->vei.vei_dir == VEI_DIR_IN)
			vei->vei.vei_data = 0xFFFFFFFF;
	
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
	case VMX_EXIT_IO:
		vcpu_exit_inout(vrp);
		break;
	case VMX_EXIT_HLT:
		ret = pthread_mutex_lock(&vcpu_run_mtx[vrp->vrp_vcpu_id]);
		if (ret) {
			log_warnx("%s: can't lock vcpu mutex (%d)",
			    __func__, ret);
			return (1);
		}
		vcpu_hlt[vrp->vrp_vcpu_id] = 1;
		ret = pthread_mutex_unlock(&vcpu_run_mtx[vrp->vrp_vcpu_id]);
		if (ret) {
			log_warnx("%s: can't unlock vcpu mutex (%d)",
			    __func__, ret);
			return (1);
		}
		break;
	case VMX_EXIT_INT_WINDOW:
		break;
	case VMX_EXIT_TRIPLE_FAULT:
		log_warnx("%s: triple fault", __progname);
		return (1);
	default:
		log_debug("%s: unknown exit reason %d",
		    __progname, vrp->vrp_exit_reason);
	}

	/* XXX this may not be irq 9 all the time */
	if (vionet_process_rx())
		vcpu_assert_pic_irq(vrp->vrp_vm_id, vrp->vrp_vcpu_id, 9);

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
 * Copies data from 'buf' into the guest VM's memory at paddr 'dst'.
 *
 * Parameters:
 *  dst: the destination paddr_t in the guest VM
 *  buf: data to copy
 *  len: number of bytes to copy
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

	vmr = find_gpa_range(&current_vm->vm_params, src, len);
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
