/*	$OpenBSD: vmm.c,v 1.91 2018/12/04 08:15:09 claudio Exp $	*/

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

void vmm_sighdlr(int, short, void *);
int vmm_start_vm(struct imsg *, uint32_t *, pid_t *);
int vmm_dispatch_parent(int, struct privsep_proc *, struct imsg *);
void vmm_run(struct privsep *, struct privsep_proc *, void *);
void vmm_dispatch_vm(int, short, void *);
int terminate_vm(struct vm_terminate_params *);
int get_info_vm(struct privsep *, struct imsg *, int);
int opentap(char *);

extern struct vmd *env;

static struct privsep_proc procs[] = {
	{ "parent",	PROC_PARENT,	vmm_dispatch_parent  },
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
	 * send - for sending send/recv fds to vm proc.
	 * recvfd - for disks, interfaces and other fds.
	 */
	if (pledge("stdio vmm sendfd recvfd proc", NULL) == -1)
		fatal("pledge");

	/* Get and terminate all running VMs */
	get_info_vm(ps, NULL, 1);
}

int
vmm_dispatch_parent(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct privsep		*ps = p->p_ps;
	int			 res = 0, cmd = 0, verbose, ret;
	struct vmd_vm		*vm = NULL;
	struct vm_terminate_params vtp;
	struct vmop_id		 vid;
	struct vmop_result	 vmr;
	struct vmop_create_params vmc;
	uint32_t		 id = 0, peerid = imsg->hdr.peerid;
	pid_t			 pid = 0;
	unsigned int		 mode, flags;

	switch (imsg->hdr.type) {
	case IMSG_VMDOP_START_VM_REQUEST:
		res = config_getvm(ps, imsg);
		if (res == -1) {
			res = errno;
			cmd = IMSG_VMDOP_START_VM_RESPONSE;
		}
		break;
	case IMSG_VMDOP_START_VM_CDROM:
		res = config_getcdrom(ps, imsg);
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
		res = vmm_start_vm(imsg, &id, &pid);
		/* Check if the ID can be mapped correctly */
		if ((id = vm_id2vmid(id, NULL)) == 0)
			res = ENOENT;
		cmd = IMSG_VMDOP_START_VM_RESPONSE;
		break;
	case IMSG_VMDOP_WAIT_VM_REQUEST:
		IMSG_SIZE_CHECK(imsg, &vid);
		memcpy(&vid, imsg->data, sizeof(vid));
		id = vid.vid_id;

		DPRINTF("%s: recv'ed WAIT_VM for %d", __func__, id);

		cmd = IMSG_VMDOP_TERMINATE_VM_RESPONSE;
		if (id == 0) {
			res = ENOENT;
		} else if ((vm = vm_getbyvmid(id)) != NULL) {
			if (vm->vm_peerid != (uint32_t)-1) {
				peerid = vm->vm_peerid;
				res = EINTR;
			} else
				cmd = 0;
			vm->vm_peerid = imsg->hdr.peerid;
		} else {
			/* vm doesn't exist, cannot stop vm */
			log_debug("%s: cannot stop vm that is not running",
			    __func__);
			res = VMD_VM_STOP_INVALID;
		}
		break;
	case IMSG_VMDOP_TERMINATE_VM_REQUEST:
		IMSG_SIZE_CHECK(imsg, &vid);
		memcpy(&vid, imsg->data, sizeof(vid));
		id = vid.vid_id;
		flags = vid.vid_flags;

		DPRINTF("%s: recv'ed TERMINATE_VM for %d", __func__, id);

		cmd = IMSG_VMDOP_TERMINATE_VM_RESPONSE;

		if (id == 0) {
			res = ENOENT;
		} else if ((vm = vm_getbyvmid(id)) != NULL) {
			if (flags & VMOP_FORCE) {
				vtp.vtp_vm_id = vm_vmid2id(vm->vm_vmid, vm);
				vm->vm_shutdown = 1;
				(void)terminate_vm(&vtp);
				res = 0;
			} else if (vm->vm_shutdown == 0) {
				log_debug("%s: sending shutdown request"
				    " to vm %d", __func__, id);

				/*
				 * Request reboot but mark the VM as shutting
				 * down. This way we can terminate the VM after
				 * the triple fault instead of reboot and 
				 * avoid being stuck in the ACPI-less powerdown
				 * ("press any key to reboot") of the VM.
				 */
				vm->vm_shutdown = 1;
				if (imsg_compose_event(&vm->vm_iev,
				    IMSG_VMDOP_VM_REBOOT,
				    0, 0, -1, NULL, 0) == -1)
					res = errno;
				else
					res = 0;
			} else {
				/*
				 * VM is currently being shutdown.
				 * Check to see if the VM process is still
				 * active.  If not, return VMD_VM_STOP_INVALID.
				 */
				if (vm_vmid2id(vm->vm_vmid, vm) == 0) {
					log_debug("%s: no vm running anymore",
					    __func__);
					res = VMD_VM_STOP_INVALID;
				}
			}
			if ((flags & VMOP_WAIT) &&
			    res == 0 && vm->vm_shutdown == 1) {
				if (vm->vm_peerid != (uint32_t)-1) {
					peerid = vm->vm_peerid;
					res = EINTR;
				} else
					cmd = 0;
				vm->vm_peerid = imsg->hdr.peerid;
			}
		} else {
			/* vm doesn't exist, cannot stop vm */
			log_debug("%s: cannot stop vm that is not running",
			    __func__);
			res = VMD_VM_STOP_INVALID;
		}
		break;
	case IMSG_VMDOP_GET_INFO_VM_REQUEST:
		res = get_info_vm(ps, imsg, 0);
		cmd = IMSG_VMDOP_GET_INFO_VM_END_DATA;
		break;
	case IMSG_VMDOP_CONFIG:
		config_getconfig(env, imsg);
		break;
	case IMSG_CTL_RESET:
		IMSG_SIZE_CHECK(imsg, &mode);
		memcpy(&mode, imsg->data, sizeof(mode));

		if (mode & CONFIG_VMS) {
			/* Terminate and remove all VMs */
			vmm_shutdown();
			mode &= ~CONFIG_VMS;
		}

		config_getreset(env, imsg);
		break;
	case IMSG_CTL_VERBOSE:
		IMSG_SIZE_CHECK(imsg, &verbose);
		memcpy(&verbose, imsg->data, sizeof(verbose));
		log_setverbose(verbose);

		/* Forward message to each VM process */
		TAILQ_FOREACH(vm, env->vmd_vms, vm_entry) {
			imsg_compose_event(&vm->vm_iev,
			    imsg->hdr.type, imsg->hdr.peerid, imsg->hdr.pid,
			    -1, &verbose, sizeof(verbose));
		}
		break;
	case IMSG_VMDOP_PAUSE_VM:
		IMSG_SIZE_CHECK(imsg, &vid);
		memcpy(&vid, imsg->data, sizeof(vid));
		id = vid.vid_id;
		vm = vm_getbyvmid(id);
		if ((vm = vm_getbyvmid(id)) == NULL) {
			res = ENOENT;
			cmd = IMSG_VMDOP_PAUSE_VM_RESPONSE;
			break;
		}
		imsg_compose_event(&vm->vm_iev,
		    imsg->hdr.type, imsg->hdr.peerid, imsg->hdr.pid,
		    imsg->fd, &vid, sizeof(vid));
		break;
	case IMSG_VMDOP_UNPAUSE_VM:
		IMSG_SIZE_CHECK(imsg, &vid);
		memcpy(&vid, imsg->data, sizeof(vid));
		id = vid.vid_id;
		if ((vm = vm_getbyvmid(id)) == NULL) {
			res = ENOENT;
			cmd = IMSG_VMDOP_UNPAUSE_VM_RESPONSE;
			break;
		}
		imsg_compose_event(&vm->vm_iev,
		    imsg->hdr.type, imsg->hdr.peerid, imsg->hdr.pid,
		    imsg->fd, &vid, sizeof(vid));
		break;
	case IMSG_VMDOP_SEND_VM_REQUEST:
		IMSG_SIZE_CHECK(imsg, &vid);
		memcpy(&vid, imsg->data, sizeof(vid));
		id = vid.vid_id;
		if ((vm = vm_getbyvmid(id)) == NULL) {
			res = ENOENT;
			close(imsg->fd);
			cmd = IMSG_VMDOP_START_VM_RESPONSE;
			break;
		}
		imsg_compose_event(&vm->vm_iev,
		    imsg->hdr.type, imsg->hdr.peerid, imsg->hdr.pid,
		    imsg->fd, &vid, sizeof(vid));
		break;
	case IMSG_VMDOP_RECEIVE_VM_REQUEST:
		IMSG_SIZE_CHECK(imsg, &vmc);
		memcpy(&vmc, imsg->data, sizeof(vmc));
		ret = vm_register(ps, &vmc, &vm,
		    imsg->hdr.peerid, vmc.vmc_owner.uid);
		vm->vm_tty = imsg->fd;
		vm->vm_received = 1;
		break;
	case IMSG_VMDOP_RECEIVE_VM_END:
		if ((vm = vm_getbyvmid(imsg->hdr.peerid)) == NULL) {
			res = ENOENT;
			close(imsg->fd);
			cmd = IMSG_VMDOP_START_VM_RESPONSE;
			break;
		}
		vm->vm_receive_fd = imsg->fd;
		res = vmm_start_vm(imsg, &id, &pid);
		/* Check if the ID can be mapped correctly */
		if ((id = vm_id2vmid(id, NULL)) == 0)
			res = ENOENT;
		cmd = IMSG_VMDOP_START_VM_RESPONSE;
		break;
	default:
		return (-1);
	}

	switch (cmd) {
	case 0:
		break;
	case IMSG_VMDOP_START_VM_RESPONSE:
		if (res != 0) {
			/* Remove local reference if it exists */
			if ((vm = vm_getbyvmid(imsg->hdr.peerid)) != NULL) {
				log_debug("%s: removing vm, START_VM_RESPONSE",
				    __func__);
				vm_remove(vm, __func__);
			}
		}
		if (id == 0)
			id = imsg->hdr.peerid;
	case IMSG_VMDOP_PAUSE_VM_RESPONSE:
	case IMSG_VMDOP_UNPAUSE_VM_RESPONSE:
	case IMSG_VMDOP_TERMINATE_VM_RESPONSE:
		memset(&vmr, 0, sizeof(vmr));
		vmr.vmr_result = res;
		vmr.vmr_id = id;
		vmr.vmr_pid = pid;
		if (proc_compose_imsg(ps, PROC_PARENT, -1, cmd,
		    peerid, -1, &vmr, sizeof(vmr)) == -1)
			return (-1);
		break;
	default:
		if (proc_compose_imsg(ps, PROC_PARENT, -1, cmd,
		    peerid, -1, &res, sizeof(res)) == -1)
			return (-1);
		break;
	}

	return (0);
}

void
vmm_sighdlr(int sig, short event, void *arg)
{
	struct privsep *ps = arg;
	int status, ret = 0;
	uint32_t vmid;
	pid_t pid;
	struct vmop_result vmr;
	struct vmd_vm *vm;
	struct vm_terminate_params vtp;

	log_debug("%s: handling signal %d", __func__, sig);
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

				if (WIFEXITED(status))
					ret = WEXITSTATUS(status);

				/* don't reboot on pending shutdown */
				if (ret == EAGAIN && vm->vm_shutdown)
					ret = 0;

				vmid = vm->vm_params.vmc_params.vcp_id;
				vtp.vtp_vm_id = vmid;

				if (terminate_vm(&vtp) == 0)
					log_debug("%s: terminated vm %s"
					    " (id %d)", __func__,
					    vm->vm_params.vmc_params.vcp_name,
					    vm->vm_vmid);

				memset(&vmr, 0, sizeof(vmr));
				vmr.vmr_result = ret;
				vmr.vmr_id = vm_id2vmid(vmid, vm);
				if (proc_compose_imsg(ps, PROC_PARENT,
				    -1, IMSG_VMDOP_TERMINATE_VM_EVENT,
				    vm->vm_peerid, -1,
				    &vmr, sizeof(vmr)) == -1)
					log_warnx("could not signal "
					    "termination of VM %u to "
					    "parent", vm->vm_vmid);

				vm_remove(vm, __func__);
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
		vtp.vtp_vm_id = vm_vmid2id(vm->vm_vmid, vm);

		/* XXX suspend or request graceful shutdown */
		(void)terminate_vm(&vtp);
		vm_remove(vm, __func__);
	}
}

/*
 * vmm_pipe
 *
 * Create a new imsg control channel between vmm parent and a VM
 * (can be called on both sides).
 */
int
vmm_pipe(struct vmd_vm *vm, int fd, void (*cb)(int, short, void *))
{
	struct imsgev	*iev = &vm->vm_iev;

	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
		log_warn("failed to set nonblocking mode on vm pipe");
		return (-1);
	}

	imsg_init(&iev->ibuf, fd);
	iev->handler = cb;
	iev->data = vm;
	imsg_event_add(iev);

	return (0);
}

/*
 * vmm_dispatch_vm
 *
 * imsg callback for messages that are received from a VM child process.
 */
void
vmm_dispatch_vm(int fd, short event, void *arg)
{
	struct vmd_vm		*vm = arg;
	struct vmop_result	 vmr;
	struct imsgev		*iev = &vm->vm_iev;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct imsg		 imsg;
	ssize_t			 n;
	unsigned int		 i;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("%s: imsg_read", __func__);
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			return;
		}
	}

	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("%s: msgbuf_write fd %d", __func__, ibuf->fd);
		if (n == 0) {
			/* this pipe is dead, so remove the event handler */
			event_del(&iev->ev);
			return;
		}
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get", __func__);
		if (n == 0)
			break;

		DPRINTF("%s: got imsg %d from %s",
		    __func__, imsg.hdr.type,
		    vm->vm_params.vmc_params.vcp_name);

		switch (imsg.hdr.type) {
		case IMSG_VMDOP_VM_SHUTDOWN:
			vm->vm_shutdown = 1;
			break;
		case IMSG_VMDOP_VM_REBOOT:
			vm->vm_shutdown = 0;
			break;
		case IMSG_VMDOP_SEND_VM_RESPONSE:
			IMSG_SIZE_CHECK(&imsg, &vmr);
			memcpy(&vmr, imsg.data, sizeof(vmr));
			if (!vmr.vmr_result) {
				vm_remove(vm, __func__);
			}
		case IMSG_VMDOP_PAUSE_VM_RESPONSE:
		case IMSG_VMDOP_UNPAUSE_VM_RESPONSE:
			for (i = 0; i < sizeof(procs); i++) {
				if (procs[i].p_id == PROC_PARENT) {
					proc_forward_imsg(procs[i].p_ps,
					    &imsg, PROC_PARENT, -1);
					break;
				}
			}
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
 * terminate_vm
 *
 * Requests vmm(4) to terminate the VM whose ID is provided in the
 * supplied vm_terminate_params structure (vtp->vtp_vm_id)
 *
 * Parameters
 *  vtp: vm_terminate_params struct containing the ID of the VM to terminate
 *
 * Return values:
 *  0: success
 *  !0 : ioctl to vmm(4) failed (eg, ENOENT if the supplied VM is not
 *      valid)
 */
int
terminate_vm(struct vm_terminate_params *vtp)
{
	if (ioctl(env->vmd_fd, VMM_IOC_TERM, vtp) == -1)
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
 * vmm_start_vm
 *
 * Prepares and forks a new VM process.
 *
 * Parameters:
 *  imsg: The VM data structure that is including the VM create parameters.
 *  id: Returns the VM id as reported by the kernel and obtained from the VM.
 *  pid: Returns the VM pid to the parent.
 *
 * Return values:
 *  0: success
 *  !0 : failure - typically an errno indicating the source of the failure
 */
int
vmm_start_vm(struct imsg *imsg, uint32_t *id, pid_t *pid)
{
	struct vm_create_params	*vcp;
	struct vmd_vm		*vm;
	int			 ret = EINVAL;
	int			 fds[2];
	size_t			 i, j;

	if ((vm = vm_getbyvmid(imsg->hdr.peerid)) == NULL) {
		log_warnx("%s: can't find vm", __func__);
		ret = ENOENT;
		goto err;
	}
	vcp = &vm->vm_params.vmc_params;

	if (!vm->vm_received) {
		if ((vm->vm_tty = imsg->fd) == -1) {
			log_warnx("%s: can't get tty", __func__);
			goto err;
		}
	}

	if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, fds) == -1)
		fatal("socketpair");

	/* Start child vmd for this VM (fork, chroot, drop privs) */
	ret = fork();

	/* Start child failed? - cleanup and leave */
	if (ret == -1) {
		log_warnx("%s: start child failed", __func__);
		ret = EIO;
		goto err;
	}

	if (ret > 0) {
		/* Parent */
		vm->vm_pid = ret;
		close(fds[1]);

		for (i = 0 ; i < vcp->vcp_ndisks; i++) {
			for (j = 0; j < VM_MAX_BASE_PER_DISK; j++) {
				if (vm->vm_disks[i][j] != -1)
					close(vm->vm_disks[i][j]);
				vm->vm_disks[i][j] = -1;
			}
		}
		for (i = 0 ; i < vcp->vcp_nnics; i++) {
			close(vm->vm_ifs[i].vif_fd);
			vm->vm_ifs[i].vif_fd = -1;
		}
		if (vm->vm_kernel != -1) {
			close(vm->vm_kernel);
			vm->vm_kernel = -1;
		}
		if (vm->vm_cdrom != -1) {
			close(vm->vm_cdrom);
			vm->vm_cdrom = -1;
		}
		if (vm->vm_tty != -1) {
			close(vm->vm_tty);
			vm->vm_tty = -1;
		}

		/* read back the kernel-generated vm id from the child */
		if (read(fds[0], &vcp->vcp_id, sizeof(vcp->vcp_id)) !=
		    sizeof(vcp->vcp_id))
			fatal("read vcp id");

		if (vcp->vcp_id == 0)
			goto err;

		*id = vcp->vcp_id;
		*pid = vm->vm_pid;

		if (vmm_pipe(vm, fds[0], vmm_dispatch_vm) == -1)
			fatal("setup vm pipe");

		return (0);
	} else {
		/* Child */
		close(fds[0]);
		close(PROC_PARENT_SOCK_FILENO);

		ret = start_vm(vm, fds[1]);

		_exit(ret);
	}

	return (0);

 err:
	vm_remove(vm, __func__);

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
			log_debug("%s: terminated vm %s (id %d)", __func__,
			    info[i].vir_name, info[i].vir_id);
			continue;
		}
		memcpy(&vir.vir_info, &info[i], sizeof(vir.vir_info));
		vir.vir_info.vir_id = vm_id2vmid(info[i].vir_id, NULL);
		if (proc_compose_imsg(ps, PROC_PARENT, -1,
		    IMSG_VMDOP_GET_INFO_VM_DATA, imsg->hdr.peerid, -1,
		    &vir, sizeof(vir)) == -1)
			return (EIO);
	}
	free(info);
	return (0);
}
