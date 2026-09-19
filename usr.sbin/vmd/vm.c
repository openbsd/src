/*	$OpenBSD: vm.c,v 1.135 2026/09/19 17:21:52 dv Exp $	*/

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
#include <sys/mman.h>
#include <sys/resource.h>

#include <dev/vmm/vmm.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <poll.h>
#include <pthread.h>
#include <pthread_np.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "atomicio.h"
#ifdef __amd64__
#include "acpi.h"
#include "lapic.h"
#endif
#include "pci.h"
#include "virtio.h"
#include "vmd.h"

static int run_vm(struct vmd_vm *, struct vcpu_reg_state *);
static void vm_dispatch_vmm(int, short, void *);
static void vm_sighdlr(int, short, void *);
static void vm_stop_event(int, short, void *);
static void stop_vm(struct vmd_vm *);
static void *event_thread(void *);
#ifdef __amd64__
static void *lapic_timer_thread(void *);
#endif
static void *vcpu_run_loop(void *);
static int vcpu_apply_pending_startup(uint32_t);
static int vcpu_should_stop(void);
static int vmm_create_vm(struct vmd_vm *);
static void pause_vm(struct vmd_vm *);
static void unpause_vm(struct vmd_vm *);
static int start_vm(struct vmd_vm *, int);

int con_fd;
struct vmd_vm *current_vm;

extern struct vmd *env;

pthread_mutex_t threadmutex;
pthread_cond_t threadcond;

pthread_cond_t vcpu_run_cond[VMM_MAX_VCPUS_PER_VM];
pthread_mutex_t vcpu_run_mtx[VMM_MAX_VCPUS_PER_VM];
pthread_cond_t vcpu_unpause_cond[VMM_MAX_VCPUS_PER_VM];
pthread_mutex_t vcpu_unpause_mtx[VMM_MAX_VCPUS_PER_VM];

pthread_mutex_t vm_mtx;
uint8_t vcpu_hlt[VMM_MAX_VCPUS_PER_VM];
uint8_t vcpu_hlt_intr[VMM_MAX_VCPUS_PER_VM];
/* Protected by threadmutex. */
uint8_t vcpu_done[VMM_MAX_VCPUS_PER_VM];
uint8_t vcpu_paused[VMM_MAX_VCPUS_PER_VM];

uint64_t vcpu_wake_gen[VMM_MAX_VCPUS_PER_VM];
uint64_t vcpu_enter_gen[VMM_MAX_VCPUS_PER_VM];

enum vcpu_runstate {
	VCPU_RUNSTATE_RUNNING,
	VCPU_RUNSTATE_WAIT_SIPI,
	VCPU_RUNSTATE_INIT,
	VCPU_RUNSTATE_SIPI
};

static uint8_t vcpu_runstate[VMM_MAX_VCPUS_PER_VM];
static uint8_t vcpu_sipi_vector[VMM_MAX_VCPUS_PER_VM];

struct event ev_sigterm;

#ifdef __amd64__
/* Protected by threadmutex. */
static int lapic_timer_stop;
#endif

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
	struct vmd_vm		 vm;
	size_t			 sz = 0;
	int			 ret = 0;

	/*
	 * The vm process relies on global state. Set the fd for /dev/vmm.
	 */
	env->vmd_vmm_fd = fd_vmm;

	/*
	 * We aren't root, so we can't chroot(2). Use unveil(2) instead.
	 */
	if (unveil(env->vmd_execpath, "x") == -1)
		fatal("unveil %s", env->vmd_execpath);
#ifdef __amd64__
	if (unveil("/etc/firmware/vmm.dsdt", "r") == -1 && errno != ENOENT)
		fatal("unveil /etc/firmware/vmm.dsdt");
	(void)acpi_load_dsdt("/etc/firmware/vmm.dsdt");
#endif
	if (unveil(NULL, NULL) == -1)
		fatal("unveil lock");

	/*
	 * pledge in the vm processes:
	 * stdio - for malloc and basic I/O including events.
	 * vmm - for the vmm ioctls and operations.
	 * proc exec - fork/exec for launching devices.
	 */
	if (pledge("stdio vmm proc exec", NULL) == -1)
		fatal("pledge");

	/* Receive our vm configuration. */
	memset(&vm, 0, sizeof(vm));
	sz = atomicio(read, fd, &vm, sizeof(vm));
	if (sz != sizeof(vm)) {
		log_warnx("failed to receive start message");
		_exit(EIO);
	}

	/* Update process with the vm name. */
	setproctitle("%s", vm.vm_params.vmc_name);
	log_procinit("vm/%s", vm.vm_params.vmc_name);

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
	if (vm.vm_kernel == -1) {
		log_warnx("failed to receive boot fd");
		_exit(EINVAL);
	}

	if (vm.vm_params.vmc_sev && env->vmd_psp_fd < 0) {
		log_warnx("%s not available", PSP_NODE);
		_exit(EINVAL);
	}

	ret = start_vm(&vm, fd);
	_exit(ret);
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
	struct vcpu_reg_state	 vrs;
	int			 ret, nicfds[VM_MAX_NICS_PER_VM];
	size_t			 i;

	/*
	 * We first try to initialize and allocate memory before bothering
	 * vmm(4) with a request to create a new vm.
	 */
	create_memory_map(vm);

	/* Create the vm in vmm(4). */
	ret = vmm_create_vm(vm);
	if (ret) {
		struct rlimit lim;
		char buf[FMT_SCALED_STRSIZE];
		if (ret == ENOMEM && getrlimit(RLIMIT_DATA, &lim) == 0) {
			if (fmt_scaled(lim.rlim_cur, buf) == 0)
				fatalx("could not allocate guest memory (data "
				    "limit is %s)", buf);
		} else {
			errno = ret;
			log_warn("could not create vm");
		}
		/* Let the vmm process know we failed by sending the error code. */
		atomicio(vwrite, fd, &ret, sizeof(ret));
		return (ret);
	}

	/* Setup SEV. */
	ret = sev_init(vm);
	if (ret) {
		log_warnx("could not initialize SEV");
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
	/* We now let the vmm process know we were successful. */
	ret = 0;
	if (atomicio(vwrite, fd, &ret, sizeof(ret)) != sizeof(ret)) {
		log_warn("failed to send vm start status to vmm process");
		return (1);
	}

	/* Prepare our boot image. */
	if (load_firmware(vm, &vrs))
		fatalx("failed to load kernel or firmware image");

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

	/*
	 * Finalize our communication socket with the vmm process. From here
	 * onwards, communication with the vmm process is event-based.
	 */
	event_init();
	if (vmm_pipe(vm, fd, vm_dispatch_vmm) == -1)
		fatal("setup vm pipe");

	/*
	 * Initialize our emulated hardware.
	 */
	for (i = 0; i < VMM_MAX_NICS_PER_VM; i++)
		nicfds[i] = vm->vm_ifs[i].vif_fd;
	ret = init_emulated_hw(vm, vm->vm_cdrom, vm->vm_disks, nicfds);
	if (ret) {
		virtio_shutdown(vm);
		return (ret);
	}

	/* Drop privileges further before starting the vcpu run loop(s). */
	if (pledge("stdio vmm", NULL) == -1)
		fatal("pledge");

	/*
	 * Execute the vcpu run loop(s) for this VM.
	 */
	ret = run_vm(vm, &vrs);

	/* Shutdown SEV. */
	if (sev_shutdown(vm))
		log_warnx("%s: could not shutdown SEV", __func__);

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
	uint32_t		 id, type;
	pid_t			 pid;
	int			 n, verbose;

	if (event & EV_READ) {
		if ((n = imsgbuf_read(ibuf)) == -1)
			fatal("%s: imsgbuf_read", __func__);
		if (n == 0)
			_exit(0);
	}

	if (event & EV_WRITE) {
		if (imsgbuf_write(ibuf) == -1) {
			if (errno == EPIPE)
				_exit(0);
			fatal("%s: imsgbuf_write fd %d", __func__, ibuf->fd);
		}
	}

	for (;;) {
		if ((n = imsgbuf_get(ibuf, &imsg)) == -1)
			fatal("%s: imsgbuf_get", __func__);
		if (n == 0)
			break;

		type = imsg_get_type(&imsg);
		id = imsg_get_id(&imsg);
		pid = imsg_get_pid(&imsg);
#if DEBUG > 1
		log_debug("%s: got imsg %d from %s", __func__, type,
		    vm->vm_params.vmc_params.vcp_name);
#endif

		switch (type) {
		case IMSG_CTL_VERBOSE:
			verbose = imsg_int_read(&imsg);
			log_setverbose(verbose);
			virtio_broadcast_imsg(vm, IMSG_CTL_VERBOSE, &verbose,
			    sizeof(verbose));
			break;
		case IMSG_VMDOP_VM_SHUTDOWN:
			if (vmmci_ctl(&vmmci, VMMCI_SHUTDOWN) == -1)
				_exit(0);
			break;
		case IMSG_VMDOP_VM_REBOOT:
			if (vmmci_ctl(&vmmci, VMMCI_REBOOT) == -1)
				_exit(0);
			break;
		case IMSG_VMDOP_PAUSE_VM:
			vmr.vmr_result = 0;
			vmr.vmr_id = vm->vm_vmid;
			pause_vm(vm);
			imsg_compose_event(&vm->vm_iev,
			    IMSG_VMDOP_PAUSE_VM_RESPONSE, id, pid, -1, &vmr,
			    sizeof(vmr));
			break;
		case IMSG_VMDOP_UNPAUSE_VM:
			vmr.vmr_result = 0;
			vmr.vmr_id = vm->vm_vmid;
			unpause_vm(vm);
			imsg_compose_event(&vm->vm_iev,
			    IMSG_VMDOP_UNPAUSE_VM_RESPONSE, id, pid, -1, &vmr,
			    sizeof(vmr));
			break;
		case IMSG_VMDOP_PRIV_GET_ADDR_RESPONSE:
			vmop_addr_result_read(&imsg, &var);
			log_debug("%s: received tap addr %s for nic %d",
			    vm->vm_params.vmc_name,
			    ether_ntoa((void *)var.var_addr), var.var_nic_idx);

			vionet_set_hostmac(vm, var.var_nic_idx, var.var_addr);
			break;
		default:
			fatalx("%s: got invalid imsg %d from %s", __func__,
			    type, vm->vm_params.vmc_name);
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
	imsgbuf_flush(&current_vm->vm_iev.ibuf);

	if (sev_shutdown(current_vm))
		log_warnx("%s: could not shutdown SEV", __func__);

	_exit(0);
}

static void
pause_vm(struct vmd_vm *vm)
{
	unsigned int n;
	int ret;

	mutex_lock(&vm_mtx);
	if (vm->vm_state & (VM_STATE_PAUSED | VM_STATE_SHUTDOWN)) {
		mutex_unlock(&vm_mtx);
		return;
	}
	current_vm->vm_state |= VM_STATE_PAUSED;
	mutex_unlock(&vm_mtx);

	for (n = 0; n < vm->vm_params.vmc_ncpus; n++) {
		mutex_lock(&vcpu_run_mtx[n]);
		ret = pthread_cond_broadcast(&vcpu_run_cond[n]);
		mutex_unlock(&vcpu_run_mtx[n]);
		if (ret) {
			log_warnx("%s: can't broadcast vcpu run cond (%d)",
			    __func__, (int)ret);
			return;
		}
	}
	mutex_lock(&threadmutex);
	for (;;) {
		for (n = 0; n < vm->vm_params.vmc_ncpus; n++) {
			if (!vcpu_paused[n] && !vcpu_done[n])
				break;
		}
		if (n == vm->vm_params.vmc_ncpus)
			break;
		ret = pthread_cond_wait(&threadcond, &threadmutex);
		if (ret)
			fatalx("cannot wait for vcpus to pause (%d)", ret);
	}
	mutex_unlock(&threadmutex);

	mutex_lock(&vm_mtx);
	ret = (vm->vm_state & VM_STATE_SHUTDOWN) != 0;
	mutex_unlock(&vm_mtx);
	if (ret)
		return;

	pause_vm_md(vm);
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

	for (n = 0; n < vm->vm_params.vmc_ncpus; n++) {
		mutex_lock(&vcpu_unpause_mtx[n]);
		ret = pthread_cond_broadcast(&vcpu_unpause_cond[n]);
		mutex_unlock(&vcpu_unpause_mtx[n]);
		if (ret) {
			log_warnx("%s: can't broadcast vcpu unpause cond (%d)",
			    __func__, (int)ret);
			return;
		}
	}

	unpause_vm_md(vm);
}

/*
 * vcpu_reset
 *
 * Requests vmm(4) to reset the VCPUs in the indicated VM to
 * the register state provided
 *
 * Parameters
 *  fd: vm file descriptor to reset
 *  vcpu_id: VCPU ID to reset
 *  vrs: the register state to initialize
 *
 * Return values:
 *  0: success
 *  !0 : ioctl to vmm(4) failed (eg, ENOENT if the supplied VCPU ID is not
 *      valid)
 */
int
vcpu_reset(int fd, uint32_t vcpu_id, struct vcpu_reg_state *vrs)
{
	struct vm_resetcpu_params vrp;

	memset(&vrp, 0, sizeof(vrp));
	vrp.vrp_vcpu_id = vcpu_id;
	memcpy(&vrp.vrp_init_state, vrs, sizeof(struct vcpu_reg_state));

	log_debug("%s: resetting vcpu %d", __func__, vcpu_id);

	if (ioctl(fd, VMM_IOC_RESETCPU, &vrp) == -1)
		return (errno);

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
 *  vm: pointer to the vm object
 *
 * Return values:
 *  0: success
 *  !0 : ioctl to vmm(4) failed
 */
static int
vmm_create_vm(struct vmd_vm *vm)
{
	struct vm_create_params		 vcp;
	struct vmop_create_params	*vmc = &vm->vm_params;
	size_t				 i;
	int				 error;

	/* Sanity check arguments */
	if (vmc->vmc_ncpus == 0 ||
	    vmc->vmc_ncpus > VMM_MAX_VCPUS_PER_VM)
		return (EINVAL);

	if (vmc->vmc_nmemranges == 0 ||
	    vmc->vmc_nmemranges > VMM_MAX_MEM_RANGES)
		return (EINVAL);

	if (vmc->vmc_ndisks > VM_MAX_DISKS_PER_VM)
		return (EINVAL);

	if (vmc->vmc_nnics > VM_MAX_NICS_PER_VM)
		return (EINVAL);

	memset(&vcp, 0, sizeof(vcp));
	vcp.vcp_nmemranges = vmc->vmc_nmemranges;
	vcp.vcp_ncpus = vmc->vmc_ncpus;
	memcpy(vcp.vcp_memranges, vmc->vmc_memranges,
	    sizeof(vcp.vcp_memranges));
	memcpy(vcp.vcp_name, vmc->vmc_name, sizeof(vcp.vcp_name));
	vcp.vcp_sev = vmc->vmc_sev;
	vcp.vcp_seves = vmc->vmc_seves;

	if (ioctl(env->vmd_vmm_fd, VMM_IOC_CREATE, &vcp) == -1) {
		error = errno;
		close_fd(env->vmd_vmm_fd);
		env->vmd_vmm_fd = -1;
		return (error);
	}
	close_fd(env->vmd_vmm_fd);
	env->vmd_vmm_fd = -1;

	vm->vm_fd = vcp.vcp_fd;
	for (i = 0; i < vcp.vcp_ncpus; i++)
		vm->vm_sev_asid[i] = vcp.vcp_asid[i];
	for (i = 0; i < vmc->vmc_nmemranges; i++)
		vmc->vmc_memranges[i].vmr_va = vcp.vcp_memranges[i].vmr_va;
	vm->vm_poscbit = vcp.vcp_poscbit;

	return (0);
}


/*
 * run_vm
 *
 * Runs the VM whose creation parameters are specified in vcp
 *
 * Parameters:
 *  vm:  vm to begin emulating
 *  vrs: VCPU register state to initialize
 *
 * Return values:
 *  0: the VM exited normally
 *  !0 : the VM exited abnormally or failed to start
 */
static int
run_vm(struct vmd_vm *vm, struct vcpu_reg_state *vrs)
{
	struct vmop_create_params *vmc;
	struct event ev_stop;
	uint8_t evdone = 0, evstop = 0;
	size_t i, nstarted = 0;
	int ret, error = 0, stopping, event_loop_started = 0;
	int event_pipe[2];
	pthread_t *tid, evtid;
#ifdef __amd64__
	pthread_t laptid;
	int lapic_timer_started = 0;
#endif
	char tname[MAXCOMLEN + 1];
	struct vm_run_params **vrp;
	void *exit_status;

	vmc = &vm->vm_params;

	if (vmc->vmc_nmemranges == 0 ||
	    vmc->vmc_nmemranges > VMM_MAX_MEM_RANGES)
		return (EINVAL);

	tid = calloc(vmc->vmc_ncpus, sizeof(pthread_t));
	if (tid == NULL) {
		log_warn("failed to allocate pthread structures");
		return (ENOMEM);
	}
	vrp = calloc(vmc->vmc_ncpus, sizeof(struct vm_run_params *));
	if (vrp == NULL) {
		log_warn("failed to allocate vm run params array");
		return (ENOMEM);
	}

	log_debug("%s: starting %zu vcpu thread(s) for vm %s", __func__,
	    vmc->vmc_ncpus, vmc->vmc_name);

	/*
	 * Prepare each VCPU before launching any worker. These threads may
	 * migrate between PCPUs over time; the need to reload CPU state
	 * in such situations is detected and performed by vmm(4) in the
	 * kernel.
	 */
	for (i = 0 ; i < vmc->vmc_ncpus; i++) {
		vrp[i] = calloc(1, sizeof(struct vm_run_params));
		if (vrp[i] == NULL) {
			log_warn("failed to allocate vm run parameters");
			/* caller will exit, so skip freeing */
			return (ENOMEM);
		}
		vrp[i]->vrp_exit = calloc(1, sizeof(struct vm_exit));
		if (vrp[i]->vrp_exit == NULL) {
			log_warn("failed to allocate vm exit area");
			/* caller will exit, so skip freeing */
			return (ENOMEM);
		}
		vrp[i]->vrp_vcpu_id = i;

#ifdef __amd64__
		if (i == 0) {
			ret = vcpu_reset(vm->vm_fd, i, vrs);
			vcpu_runstate[i] = VCPU_RUNSTATE_RUNNING;
		} else {
			struct vcpu_reg_state ap_vrs;

			vcpu_init_ap(&ap_vrs);
			ret = vcpu_reset(vm->vm_fd, i, &ap_vrs);
			vcpu_runstate[i] = VCPU_RUNSTATE_WAIT_SIPI;
		}
#else
		ret = vcpu_reset(vm->vm_fd, i, vrs);
		vcpu_runstate[i] = VCPU_RUNSTATE_RUNNING;
#endif
		if (ret) {
			log_warnx("cannot reset vcpu %zu", i);
			return (EIO);
		}

		if (sev_activate(vm, i)) {
			log_warnx("SEV activation failed for vcpu %zu", i);
			return (EIO);
		}

		if (sev_encrypt_memory(vm)) {
			log_warnx("memory encryption failed for vcpu %zu", i);
			return (EIO);
		}

		if (sev_encrypt_state(vm, i)) {
			log_warnx("state encryption failed for vcpu %zu", i);
			return (EIO);
		}

		if (sev_launch_finalize(vm)) {
			log_warnx("encryption failed for vcpu %zu", i);
			return (EIO);
		}

		ret = pthread_cond_init(&vcpu_run_cond[i], NULL);
		if (ret) {
			log_warnx("cannot initialize cond var (%d)", ret);
			return (ret);
		}

		ret = pthread_mutex_init(&vcpu_run_mtx[i], NULL);
		if (ret) {
			log_warnx("cannot initialize mtx (%d)", ret);
			return (ret);
		}

		ret = pthread_cond_init(&vcpu_unpause_cond[i], NULL);
		if (ret) {
			log_warnx("cannot initialize unpause var (%d)", ret);
			return (ret);
		}

		ret = pthread_mutex_init(&vcpu_unpause_mtx[i], NULL);
		if (ret) {
			log_warnx("cannot initialize unpause mtx (%d)", ret);
			return (ret);
		}

		vcpu_hlt[i] = 0;
		vcpu_hlt_intr[i] = 0;
		vcpu_done[i] = 0;
		vcpu_paused[i] = 0;
		vcpu_wake_gen[i] = 0;
		vcpu_enter_gen[i] = 0;
	}

	/* Only the event thread may stop its event loop. */
	if (pipe2(event_pipe, O_CLOEXEC) == -1)
		fatal("event stop pipe");
	event_set(&ev_stop, event_pipe[0], EV_READ | EV_PERSIST,
	    vm_stop_event, &evstop);
	signal_set(&ev_sigterm, SIGTERM, vm_sighdlr, NULL);
	event_add(&ev_stop, NULL);
	signal_add(&ev_sigterm, NULL);

	/*
	 * Do not launch the BSP until every AP's mutex, condition variable and
	 * reset state are ready. Firmware can send INIT/SIPI immediately.
	 */
	for (i = 0; i < vmc->vmc_ncpus; i++) {
		ret = pthread_create(&tid[i], NULL, vcpu_run_loop, vrp[i]);
		if (ret) {
			log_warnx("%s: could not create vcpu thread %zu (%d)",
			    __func__, i, ret);
			error = ret;
			goto stop;
		}
		nstarted++;

		snprintf(tname, sizeof(tname), "vcpu-%zu", i);
		pthread_set_name_np(tid[i], tname);
	}

	log_debug("%s: waiting on events for VM %s", __func__, vmc->vmc_name);
#ifdef __amd64__
	lapic_timer_stop = 0;
	ret = pthread_create(&laptid, NULL, lapic_timer_thread,
	    (void *)(intptr_t)vmc->vmc_ncpus);
	if (ret) {
		errno = ret;
		log_warn("%s: could not create LAPIC timer thread", __func__);
		error = ret;
		goto stop;
	}
	lapic_timer_started = 1;
	pthread_set_name_np(laptid, "lapictmr");
#endif
	ret = pthread_create(&evtid, NULL, event_thread, &evdone);
	if (ret) {
		errno = ret;
		log_warn("%s: could not create event thread", __func__);
		error = ret;
		goto stop;
	}
	event_loop_started = 1;
	pthread_set_name_np(evtid, "event");

	/* Completion flags and their notification share one mutex. */
	mutex_lock(&threadmutex);
	for (;;) {
		mutex_lock(&vm_mtx);
		stopping = (vm->vm_state & VM_STATE_SHUTDOWN) != 0;
		mutex_unlock(&vm_mtx);

		for (i = 0; i < nstarted; i++) {
			if (vcpu_done[i])
				break;
		}
		if (stopping || evdone || i < nstarted)
			break;

		ret = pthread_cond_wait(&threadcond, &threadmutex);
		if (ret)
			fatalx("cannot wait for vm threads (%d)", ret);
	}
	if (evdone) {
		log_warnx("event thread exited unexpectedly");
		error = EIO;
	}
	mutex_unlock(&threadmutex);

	/* Keep processing device events until every VCPU has finished. */
stop:
	stop_vm(vm);
	for (i = 0; i < nstarted; i++) {
		ret = pthread_join(tid[i], &exit_status);
		if (ret)
			fatalx("cannot join vcpu %zu (%d)", i, ret);
		/* A guest reset takes precedence over sibling exit statuses. */
		if ((intptr_t)exit_status == EAGAIN || error == 0)
			error = (intptr_t)exit_status;
	}
#ifdef __amd64__
	if (lapic_timer_started) {
		/* VCPUs no longer need timer interrupts to make progress. */
		mutex_lock(&threadmutex);
		lapic_timer_stop = 1;
		mutex_unlock(&threadmutex);
		ret = pthread_join(laptid, &exit_status);
		if (ret)
			fatalx("cannot join LAPIC timer thread (%d)", ret);
	}
#endif
	if (event_loop_started) {
		do {
			ret = write(event_pipe[1], "", 1);
		} while (ret == -1 && errno == EINTR);
		if (ret != 1)
			fatal("cannot stop event thread");
		ret = pthread_join(evtid, &exit_status);
		if (ret)
			fatalx("cannot join event thread (%d)", ret);
		if ((!evstop || (intptr_t)exit_status != 0) && error == 0)
			error = EIO;
	}
	event_del(&ev_stop);
	/* Keep catching shutdown signals until device cleanup finishes. */
	close(event_pipe[0]);
	close(event_pipe[1]);
	return (error);
}

#ifdef __amd64__
static void *
lapic_timer_thread(void *arg)
{
	size_t ncpus = (size_t)(intptr_t)arg;
	size_t i;
	int done, error, stopping;

	for (;;) {
		mutex_lock(&threadmutex);
		stopping = lapic_timer_stop;
		mutex_unlock(&threadmutex);
		if (stopping)
			break;

		usleep(200);

		for (i = 0; i < ncpus; i++) {
			mutex_lock(&threadmutex);
			done = vcpu_done[i];
			mutex_unlock(&threadmutex);
			if (done)
				continue;
			if (!lapic_timer_check(i))
				continue;

			error = vcpu_intr(current_vm->vm_fd, i, 1);
			if (error != 0) {
				log_debug("%s: could not interrupt vcpu %zu: %s",
				    __func__, i, strerror(error));
				continue;
			}
			vcpu_unhalt(i);
			vcpu_signal_run(i);
		}
	}

	return (NULL);
}
#endif

static void *
event_thread(void *arg)
{
	uint8_t *donep = arg;
	intptr_t ret;

	ret = event_dispatch();

	mutex_lock(&threadmutex);
	*donep = 1;
	pthread_cond_broadcast(&threadcond);
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
static void *
vcpu_run_loop(void *arg)
{
	struct vm_run_params *vrp = (struct vm_run_params *)arg;
	intptr_t ret = 0;
	uint32_t n = vrp->vrp_vcpu_id;
	int paused, stopping, vector;

	for (;;) {
		if (vcpu_should_stop()) {
			ret = 0;
			break;
		}

		ret = vcpu_apply_pending_startup(n);
		if (ret != 0)
			break;

		ret = pthread_mutex_lock(&vcpu_run_mtx[n]);
		if (ret) {
			log_warnx("%s: can't lock vcpu run mtx (%d)",
			    __func__, (int)ret);
			goto out;
		}

	check_state:
		mutex_lock(&vm_mtx);
		paused = (current_vm->vm_state & VM_STATE_PAUSED) != 0;
		stopping = (current_vm->vm_state & VM_STATE_SHUTDOWN) != 0;
		mutex_unlock(&vm_mtx);

		if (stopping) {
			log_debug("%s: vcpu %u stopping", __func__, n);
			mutex_unlock(&vcpu_run_mtx[n]);
			ret = 0;
			goto out;
		}

		/* Acknowledge the pause without holding the run mutex. */
		if (paused) {
			mutex_unlock(&vcpu_run_mtx[n]);
			mutex_lock(&threadmutex);
			vcpu_paused[n] = 1;
			pthread_cond_broadcast(&threadcond);
			mutex_unlock(&threadmutex);

			mutex_lock(&vcpu_unpause_mtx[n]);
			for (;;) {
				mutex_lock(&vm_mtx);
				paused = (current_vm->vm_state &
				    VM_STATE_PAUSED) != 0;
				stopping = (current_vm->vm_state &
				    VM_STATE_SHUTDOWN) != 0;
				mutex_unlock(&vm_mtx);
				if (!paused || stopping)
					break;
				ret = pthread_cond_wait(&vcpu_unpause_cond[n],
				    &vcpu_unpause_mtx[n]);
				if (ret)
					break;
			}
			mutex_unlock(&vcpu_unpause_mtx[n]);
			mutex_lock(&threadmutex);
			vcpu_paused[n] = 0;
			mutex_unlock(&threadmutex);

			if (ret) {
				log_warnx("%s: can't wait on unpause cond (%d)",
				    __func__, (int)ret);
				goto out;
			}
			mutex_lock(&vcpu_run_mtx[n]);
			goto check_state;
		}

		/* Apply a transition queued after the check at the top of the loop. */
		if (vcpu_runstate[n] == VCPU_RUNSTATE_INIT ||
		    vcpu_runstate[n] == VCPU_RUNSTATE_SIPI) {
			(void)pthread_mutex_unlock(&vcpu_run_mtx[n]);
			continue;
		}

		/* APs wait here until a SIPI transition makes them runnable. */
		if (vcpu_runstate[n] != VCPU_RUNSTATE_RUNNING || vcpu_hlt[n]) {
			ret = pthread_cond_wait(&vcpu_run_cond[n],
			    &vcpu_run_mtx[n]);
			if (ret) {
				log_warnx(
				    "%s: can't wait on cond (%d)",
				    __func__, (int)ret);
				mutex_unlock(&vcpu_run_mtx[n]);
				goto out;
			}
			goto check_state;
		}

		ret = pthread_mutex_unlock(&vcpu_run_mtx[n]);
		if (ret) {
			log_warnx("%s: can't unlock mutex on cond (%d)",
			    __func__, (int)ret);
			break;
		}

		if (vrp->vrp_irqready && intr_pending(n)) {
			vector = intr_ack(n);
			if (vector == 0xffff) {
				/* Interrupt state changed between pending and ack. */
				vrp->vrp_inject.vie_type = VCPU_INJECT_NONE;
			} else {
				vrp->vrp_inject.vie_vector = vector;
				vrp->vrp_inject.vie_type = VCPU_INJECT_INTR;
			}
		} else
			vrp->vrp_inject.vie_type = VCPU_INJECT_NONE;

		/* Still more interrupts pending? */
		vrp->vrp_intr_pending = intr_pending(n);

		/* Pair a later HLT exit with wakeups racing this guest entry. */
		mutex_lock(&vcpu_run_mtx[n]);
		vcpu_enter_gen[n] = vcpu_wake_gen[n];
		mutex_unlock(&vcpu_run_mtx[n]);

		if (ioctl(current_vm->vm_fd, VMM_IOC_RUN, vrp) == -1) {
			/* If run ioctl failed, exit */
			ret = errno;
			log_debug("%s: vm %d / vcpu %d run ioctl failed",
			    __func__, current_vm->vm_vmid, n);
			break;
		}

		/* INIT supersedes any ordinary exit which raced with its kick. */
		mutex_lock(&vcpu_run_mtx[n]);
		paused = vcpu_runstate[n] != VCPU_RUNSTATE_RUNNING;
		mutex_unlock(&vcpu_run_mtx[n]);
		if (paused && !vcpu_should_stop())
			continue;

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

		/* A sibling requested teardown; do not re-enter the guest. */
		if (vcpu_should_stop()) {
			ret = 0;
			break;
		}
	}

out:
	if (ret != 0)
		stop_vm(current_vm);

	mutex_lock(&threadmutex);
	vcpu_done[n] = 1;
	pthread_cond_broadcast(&threadcond);
	mutex_unlock(&threadmutex);

	return ((void *)ret);
}

/* Return whether shutdown has been requested by any VM thread. */
static int
vcpu_should_stop(void)
{
	int stopping;

	mutex_lock(&vm_mtx);
	stopping = (current_vm->vm_state & VM_STATE_SHUTDOWN) != 0;
	mutex_unlock(&vm_mtx);

	return (stopping);
}

static int
vcpu_apply_pending_startup(uint32_t vcpu_id)
{
#ifdef __amd64__
	struct vcpu_reg_state vrs;
	uint8_t state, vector;
	int ret;

	for (;;) {
		mutex_lock(&vcpu_run_mtx[vcpu_id]);
		state = vcpu_runstate[vcpu_id];
		vector = vcpu_sipi_vector[vcpu_id];
		mutex_unlock(&vcpu_run_mtx[vcpu_id]);

		if (state != VCPU_RUNSTATE_INIT && state != VCPU_RUNSTATE_SIPI)
			return (0);

		if (state == VCPU_RUNSTATE_INIT)
			vcpu_init_ap(&vrs);
		else
			vcpu_init_sipi(&vrs, vector);

		ret = vcpu_reset(current_vm->vm_fd, vcpu_id, &vrs);
		if (ret != 0) {
			log_warnx("%s: cannot reset vcpu %u: %s", __func__,
			    vcpu_id, strerror(ret));
			return (ret);
		}
		mutex_lock(&vcpu_run_mtx[vcpu_id]);
		if (vcpu_runstate[vcpu_id] == state &&
		    (state != VCPU_RUNSTATE_SIPI ||
		    vcpu_sipi_vector[vcpu_id] == vector)) {
			vcpu_runstate[vcpu_id] = state == VCPU_RUNSTATE_INIT ?
			    VCPU_RUNSTATE_WAIT_SIPI : VCPU_RUNSTATE_RUNNING;
		}
		mutex_unlock(&vcpu_run_mtx[vcpu_id]);
	}
#else
	return (0);
#endif
}

int
vcpu_intr(int fd, uint32_t vcpu_id, uint8_t intr)
{
	struct vm_intr_params vip;

	memset(&vip, 0, sizeof(vip));

	vip.vip_vcpu_id = vcpu_id; /* XXX always 0? */
	vip.vip_intr = intr;

	if (ioctl(fd, VMM_IOC_INTR, &vip) == -1)
		return (errno);

	return (0);
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
 *  p: pointer to vm_dev_pipe struct to initialize
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
 * Send a message to an emulated device via the provided vm_dev_pipe. This
 * relies on the fact sizeof(msg) < PIPE_BUF to ensure atomic writes.
 *
 * Parameters:
 *  p: pointer to initialized vm_dev_pipe
 *  msg: message to send in the channel
 */
void
vm_pipe_send(struct vm_dev_pipe *p, enum pipe_msg_type msg)
{
	ssize_t n;
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
 * Re-map the guest address space using vmm(4)'s VMM_IOC_SHAREMEM
 *
 * Returns 0 on success or an errno in event of failure.
 */
int
remap_guest_mem(struct vmd_vm *vm, int vm_fd)
{
	size_t i;
	struct vm_sharemem_params vsp;

	if (vm == NULL)
		return (EINVAL);

	memset(&vsp, 0, sizeof(vsp));

	/* Ask vmm(4) to enter a shared mapping to guest memory. */
	if (ioctl(vm_fd, VMM_IOC_SHAREMEM, &vsp) == -1)
		return (errno);

	/* Update with the location of the new mappings. */
	for (i = 0; i < vm->vm_params.vmc_nmemranges; i++)
		vm->vm_params.vmc_memranges[i].vmr_va = vsp.vsp_va[i];

	return (0);
}

void
vcpu_halt(uint32_t vcpu_id, int interruptible)
{
	mutex_lock(&vcpu_run_mtx[vcpu_id]);
	vcpu_hlt_intr[vcpu_id] = interruptible != 0;
	/*
	 * An interrupt can race the HLT exit after it has signalled this
	 * condition variable but before the vCPU thread records the halt. Keep
	 * an interruptible vCPU runnable when that wakeup raced this guest entry
	 * or an interrupt is still pending. A non-interruptible AP halt instead
	 * remains parked until INIT/SIPI changes its run state.
	 */
	if (!interruptible ||
	    (vcpu_wake_gen[vcpu_id] == vcpu_enter_gen[vcpu_id] &&
	    !intr_pending(vcpu_id)))
		vcpu_hlt[vcpu_id] = 1;
	mutex_unlock(&vcpu_run_mtx[vcpu_id]);
}

void
vcpu_unhalt(uint32_t vcpu_id)
{
	mutex_lock(&vcpu_run_mtx[vcpu_id]);
	vcpu_wake_gen[vcpu_id]++;
	if (!vcpu_hlt[vcpu_id] || vcpu_hlt_intr[vcpu_id])
		vcpu_hlt[vcpu_id] = 0;
	mutex_unlock(&vcpu_run_mtx[vcpu_id]);
}

void
vcpu_signal_run(uint32_t vcpu_id)
{
	int ret;

	mutex_lock(&vcpu_run_mtx[vcpu_id]);
	ret = pthread_cond_signal(&vcpu_run_cond[vcpu_id]);
	if (ret)
		fatalx("%s: can't signal (%d)", __func__, ret);
	mutex_unlock(&vcpu_run_mtx[vcpu_id]);
}

/* Queue the architectural INIT transition and force a running AP to exit. */
void
vcpu_assert_init(uint32_t vcpu_id)
{
	int ret;

	if (vcpu_id >= current_vm->vm_params.vmc_ncpus)
		return;
	mutex_lock(&vcpu_run_mtx[vcpu_id]);
#ifdef __amd64__
	/* Serialize the LAPIC reset against a closely following SIPI. */
	lapic_reset(vcpu_id);
#endif /* __amd64__ */
	vcpu_runstate[vcpu_id] = VCPU_RUNSTATE_INIT;
	vcpu_hlt[vcpu_id] = 0;
	ret = pthread_cond_signal(&vcpu_run_cond[vcpu_id]);
	mutex_unlock(&vcpu_run_mtx[vcpu_id]);
	if (ret != 0)
		fatalx("%s: can't signal vcpu %u (%d)", __func__, vcpu_id,
		    ret);

	ret = vcpu_intr(current_vm->vm_fd, vcpu_id, 1);
	if (ret != 0)
		log_warnx("%s: cannot kick vcpu %u: %s", __func__, vcpu_id,
		    strerror(ret));
}

/* Queue the first SIPI received by an AP in WAIT_SIPI or pending INIT. */
void
vcpu_start_sipi(uint32_t vcpu_id, uint8_t vector)
{
	int ret = 0;

	if (vcpu_id >= current_vm->vm_params.vmc_ncpus)
		return;

	mutex_lock(&vcpu_run_mtx[vcpu_id]);
	if (vcpu_runstate[vcpu_id] == VCPU_RUNSTATE_WAIT_SIPI ||
	    vcpu_runstate[vcpu_id] == VCPU_RUNSTATE_INIT) {
		vcpu_sipi_vector[vcpu_id] = vector;
		vcpu_runstate[vcpu_id] = VCPU_RUNSTATE_SIPI;
		vcpu_hlt[vcpu_id] = 0;
		ret = pthread_cond_signal(&vcpu_run_cond[vcpu_id]);
	}
	mutex_unlock(&vcpu_run_mtx[vcpu_id]);

	if (ret != 0)
		fatalx("%s: can't signal vcpu %u (%d)", __func__, vcpu_id,
		    ret);
}

void
vm_sighdlr(int sig, short event, void *arg)
{
	switch (sig) {
	case SIGTERM:
		stop_vm(current_vm);
		break;
	default:
		fatalx("unexpected signal: %d", sig);
	}
}

static void
stop_vm(struct vmd_vm *vm)
{
	size_t n;
	int ret;

	mutex_lock(&vm_mtx);
	vm->vm_state |= VM_STATE_SHUTDOWN;
	mutex_unlock(&vm_mtx);

	for (n = 0; n < vm->vm_params.vmc_ncpus; n++) {
		mutex_lock(&vcpu_run_mtx[n]);
		ret = pthread_cond_broadcast(&vcpu_run_cond[n]);
		mutex_unlock(&vcpu_run_mtx[n]);
		if (ret)
			fatalx("cannot wake halted vcpu %zu (%d)", n, ret);
		mutex_lock(&vcpu_unpause_mtx[n]);
		ret = pthread_cond_broadcast(&vcpu_unpause_cond[n]);
		mutex_unlock(&vcpu_unpause_mtx[n]);
		if (ret)
			fatalx("cannot wake paused vcpu %zu (%d)", n, ret);

		/* Running VCPUs must also observe the shutdown request. */
		ret = vcpu_intr(vm->vm_fd, n, 1);
		if (ret != 0)
			log_debug("%s: cannot kick vcpu %zu: %s", __func__, n,
			    strerror(ret));
	}

	mutex_lock(&threadmutex);
	pthread_cond_broadcast(&threadcond);
	mutex_unlock(&threadmutex);
}

static void
vm_stop_event(int fd, short event, void *arg)
{
	uint8_t *stopped = arg;

	/* All VCPUs have been joined; device events are no longer needed. */
	*stopped = 1;
	event_loopbreak();
}
