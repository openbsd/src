/*	$OpenBSD: vmd.c,v 1.81 2018/03/14 07:29:34 mlarkin Exp $	*/

/*
 * Copyright (c) 2015 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/queue.h>
#include <sys/wait.h>
#include <sys/cdefs.h>
#include <sys/stat.h>
#include <sys/tty.h>
#include <sys/ttycom.h>
#include <sys/ioctl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <pwd.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>

#include <machine/specialreg.h>
#include <machine/vmmvar.h>

#include "proc.h"
#include "atomicio.h"
#include "vmd.h"

__dead void usage(void);

int	 main(int, char **);
int	 vmd_configure(void);
void	 vmd_sighdlr(int sig, short event, void *arg);
void	 vmd_shutdown(void);
int	 vmd_control_run(void);
int	 vmd_dispatch_control(int, struct privsep_proc *, struct imsg *);
int	 vmd_dispatch_vmm(int, struct privsep_proc *, struct imsg *);
int	 check_vmh(struct vm_dump_header *);

struct vmd	*env;

static struct privsep_proc procs[] = {
	/* Keep "priv" on top as procs[0] */
	{ "priv",	PROC_PRIV,	NULL, priv },
	{ "control",	PROC_CONTROL,	vmd_dispatch_control, control },
	{ "vmm",	PROC_VMM,	vmd_dispatch_vmm, vmm, vmm_shutdown },
};

/* For the privileged process */
static struct privsep_proc *proc_priv = &procs[0];
static struct passwd proc_privpw;

int
vmd_dispatch_control(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct privsep			*ps = p->p_ps;
	int				 res = 0, ret = 0, cmd = 0, verbose;
	unsigned int			 v = 0;
	struct vmop_create_params	 vmc;
	struct vmop_id			 vid;
	struct vm_terminate_params	 vtp;
	struct vmop_result		 vmr;
	struct vm_dump_header		 vmh;
	struct vmd_vm			*vm = NULL;
	char				*str = NULL;
	uint32_t			 id = 0;

	switch (imsg->hdr.type) {
	case IMSG_VMDOP_START_VM_REQUEST:
		IMSG_SIZE_CHECK(imsg, &vmc);
		memcpy(&vmc, imsg->data, sizeof(vmc));
		ret = vm_register(ps, &vmc, &vm, 0, vmc.vmc_uid);
		if (vmc.vmc_flags == 0) {
			/* start an existing VM with pre-configured options */
			if (!(ret == -1 && errno == EALREADY &&
			    vm->vm_running == 0)) {
				res = errno;
				cmd = IMSG_VMDOP_START_VM_RESPONSE;
			}
		} else if (ret != 0) {
			res = errno;
			cmd = IMSG_VMDOP_START_VM_RESPONSE;
		}
		if (res == 0 &&
		    config_setvm(ps, vm, imsg->hdr.peerid, vm->vm_params.vmc_uid) == -1) {
			res = errno;
			cmd = IMSG_VMDOP_START_VM_RESPONSE;
		}
		break;
	case IMSG_VMDOP_TERMINATE_VM_REQUEST:
		IMSG_SIZE_CHECK(imsg, &vid);
		memcpy(&vid, imsg->data, sizeof(vid));
		if ((id = vid.vid_id) == 0) {
			/* Lookup vm (id) by name */
			if ((vm = vm_getbyname(vid.vid_name)) == NULL) {
				res = ENOENT;
				cmd = IMSG_VMDOP_TERMINATE_VM_RESPONSE;
				break;
			} else if (vm->vm_shutdown) {
				res = EALREADY;
				cmd = IMSG_VMDOP_TERMINATE_VM_RESPONSE;
				break;
			} else if (vm->vm_running == 0) {
				res = EINVAL;
				cmd = IMSG_VMDOP_TERMINATE_VM_RESPONSE;
				break;
			}
			id = vm->vm_vmid;
		} else if ((vm = vm_getbyvmid(id)) == NULL) {
			res = ENOENT;
			cmd = IMSG_VMDOP_TERMINATE_VM_RESPONSE;
			break;
		}
		if (vm_checkperm(vm, vid.vid_uid) != 0) {
			res = EPERM;
			cmd = IMSG_VMDOP_TERMINATE_VM_RESPONSE;
			break;
		}
		memset(&vtp, 0, sizeof(vtp));
		vtp.vtp_vm_id = id;
		if (proc_compose_imsg(ps, PROC_VMM, -1, imsg->hdr.type,
		    imsg->hdr.peerid, -1, &vtp, sizeof(vtp)) == -1)
			return (-1);
		break;
	case IMSG_VMDOP_GET_INFO_VM_REQUEST:
		proc_forward_imsg(ps, imsg, PROC_VMM, -1);
		break;
	case IMSG_VMDOP_LOAD:
		IMSG_SIZE_CHECK(imsg, str); /* at least one byte for path */
		str = get_string((uint8_t *)imsg->data,
		    IMSG_DATA_SIZE(imsg));
	case IMSG_VMDOP_RELOAD:
		if (vmd_reload(0, str) == -1)
			cmd = IMSG_CTL_FAIL;
		else
			cmd = IMSG_CTL_OK;
		free(str);
		break;
	case IMSG_CTL_RESET:
		IMSG_SIZE_CHECK(imsg, &v);
		memcpy(&v, imsg->data, sizeof(v));
		if (vmd_reload(v, NULL) == -1)
			cmd = IMSG_CTL_FAIL;
		else
			cmd = IMSG_CTL_OK;
		break;
	case IMSG_CTL_VERBOSE:
		IMSG_SIZE_CHECK(imsg, &verbose);
		memcpy(&verbose, imsg->data, sizeof(verbose));
		log_setverbose(verbose);

		proc_forward_imsg(ps, imsg, PROC_VMM, -1);
		proc_forward_imsg(ps, imsg, PROC_PRIV, -1);
		cmd = IMSG_CTL_OK;
		break;
	case IMSG_VMDOP_PAUSE_VM:
	case IMSG_VMDOP_UNPAUSE_VM:
		IMSG_SIZE_CHECK(imsg, &vid);
		memcpy(&vid, imsg->data, sizeof(vid));
		if (vid.vid_id == 0) {
			if ((vm = vm_getbyname(vid.vid_name)) == NULL) {
				res = ENOENT;
				cmd = IMSG_VMDOP_PAUSE_VM_RESPONSE;
				break;
			} else {
				vid.vid_id = vm->vm_vmid;
			}
		} else if (vm_getbyid(vid.vid_id) == NULL) {
			res = ENOENT;
			cmd = IMSG_VMDOP_PAUSE_VM_RESPONSE;
			break;
		}
		proc_compose_imsg(ps, PROC_VMM, -1, imsg->hdr.type,
		    imsg->hdr.peerid, -1, &vid, sizeof(vid));
		break;
	case IMSG_VMDOP_SEND_VM_REQUEST:
		IMSG_SIZE_CHECK(imsg, &vid);
		memcpy(&vid, imsg->data, sizeof(vid));
		id = vid.vid_id;
		if (vid.vid_id == 0) {
			if ((vm = vm_getbyname(vid.vid_name)) == NULL) {
				res = ENOENT;
				cmd = IMSG_VMDOP_SEND_VM_RESPONSE;
				close(imsg->fd);
				break;
			} else {
				vid.vid_id = vm->vm_vmid;
			}
		} else if ((vm = vm_getbyvmid(vid.vid_id)) == NULL) {
			res = ENOENT;
			cmd = IMSG_VMDOP_SEND_VM_RESPONSE;
			close(imsg->fd);
			break;
		} else {
		}
		vmr.vmr_id = vid.vid_id;
		log_debug("%s: sending fd to vmm", __func__);
		proc_compose_imsg(ps, PROC_VMM, -1, imsg->hdr.type,
		    imsg->hdr.peerid, imsg->fd, &vid, sizeof(vid));
		break;
	case IMSG_VMDOP_RECEIVE_VM_REQUEST:
		IMSG_SIZE_CHECK(imsg, &vid);
		memcpy(&vid, imsg->data, sizeof(vid));
		if (imsg->fd == -1) {
			log_warnx("%s: invalid fd", __func__);
			return (-1);
		}
		if (atomicio(read, imsg->fd, &vmh, sizeof(vmh)) !=
		    sizeof(vmh)) {
			log_warnx("%s: error reading vmh from recevied vm",
			    __func__);
			res = EIO;
			close(imsg->fd);
			cmd = IMSG_VMDOP_START_VM_RESPONSE;
			break;
		}
		if(check_vmh(&vmh)) {
			res = ENOENT;
			close(imsg->fd);
			cmd = IMSG_VMDOP_START_VM_RESPONSE;
			break;
		}
		if (atomicio(read, imsg->fd, &vmc, sizeof(vmc)) !=
		    sizeof(vmc)) {
			log_warnx("%s: error reading vmc from recevied vm",
			    __func__);
			res = EIO;
			close(imsg->fd);
			cmd = IMSG_VMDOP_START_VM_RESPONSE;
			break;
		}
		strlcpy(vmc.vmc_params.vcp_name, vid.vid_name,
		    sizeof(vmc.vmc_params.vcp_name));
		vmc.vmc_params.vcp_id = 0;

		ret = vm_register(ps, &vmc, &vm, 0, vmc.vmc_uid);
		if (ret != 0) {
			res = errno;
			cmd = IMSG_VMDOP_START_VM_RESPONSE;
			close(imsg->fd);
		} else {
			vm->vm_received = 1;
			config_setvm(ps, vm, imsg->hdr.peerid, vmc.vmc_uid);
			log_debug("%s: sending fd to vmm", __func__);
			proc_compose_imsg(ps, PROC_VMM, -1,
			    IMSG_VMDOP_RECEIVE_VM_END, vm->vm_vmid, imsg->fd,
			    NULL, 0);
		}
		break;
	default:
		return (-1);
	}

	switch (cmd) {
	case 0:
		break;
	case IMSG_VMDOP_START_VM_RESPONSE:
	case IMSG_VMDOP_TERMINATE_VM_RESPONSE:
		memset(&vmr, 0, sizeof(vmr));
		vmr.vmr_result = res;
		vmr.vmr_id = id;
		if (proc_compose_imsg(ps, PROC_CONTROL, -1, cmd,
		    imsg->hdr.peerid, -1, &vmr, sizeof(vmr)) == -1)
			return (-1);
		break;
	default:
		if (proc_compose_imsg(ps, PROC_CONTROL, -1, cmd,
		    imsg->hdr.peerid, -1, &res, sizeof(res)) == -1)
			return (-1);
		break;
	}

	return (0);
}

int
vmd_dispatch_vmm(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct vmop_result	 vmr;
	struct privsep		*ps = p->p_ps;
	int			 res = 0;
	struct vmd_vm		*vm;
	struct vm_create_params	*vcp;
	struct vmop_info_result	 vir;

	switch (imsg->hdr.type) {
	case IMSG_VMDOP_PAUSE_VM_RESPONSE:
		IMSG_SIZE_CHECK(imsg, &vmr);
		memcpy(&vmr, imsg->data, sizeof(vmr));
		if ((vm = vm_getbyvmid(vmr.vmr_id)) == NULL)
			break;
		proc_compose_imsg(ps, PROC_CONTROL, -1,
		    imsg->hdr.type, imsg->hdr.peerid, -1,
		    imsg->data, sizeof(imsg->data));
		log_info("%s: paused vm %d successfully",
		    vm->vm_params.vmc_params.vcp_name,
		    vm->vm_vmid);
		break;
	case IMSG_VMDOP_UNPAUSE_VM_RESPONSE:
		IMSG_SIZE_CHECK(imsg, &vmr);
		memcpy(&vmr, imsg->data, sizeof(vmr));
		if ((vm = vm_getbyvmid(vmr.vmr_id)) == NULL)
			break;
		proc_compose_imsg(ps, PROC_CONTROL, -1,
		    imsg->hdr.type, imsg->hdr.peerid, -1,
		    imsg->data, sizeof(imsg->data));
		log_info("%s: unpaused vm %d successfully.",
		    vm->vm_params.vmc_params.vcp_name,
		    vm->vm_vmid);
		break;
	case IMSG_VMDOP_START_VM_RESPONSE:
		IMSG_SIZE_CHECK(imsg, &vmr);
		memcpy(&vmr, imsg->data, sizeof(vmr));
		if ((vm = vm_getbyvmid(imsg->hdr.peerid)) == NULL)
			break;
		vm->vm_pid = vmr.vmr_pid;
		vcp = &vm->vm_params.vmc_params;
		vcp->vcp_id = vmr.vmr_id;

		/*
		 * If the peerid is not -1, forward the response back to the
		 * the control socket.  If it is -1, the request originated
		 * from the parent, not the control socket.
		 */
		if (vm->vm_peerid != (uint32_t)-1) {
			(void)strlcpy(vmr.vmr_ttyname, vm->vm_ttyname,
			    sizeof(vmr.vmr_ttyname));
			if (proc_compose_imsg(ps, PROC_CONTROL, -1,
			    imsg->hdr.type, vm->vm_peerid, -1,
			    &vmr, sizeof(vmr)) == -1) {
				errno = vmr.vmr_result;
				log_warn("%s: failed to foward vm result",
				    vcp->vcp_name);
				vm_remove(vm);
				return (-1);
			}
		}

		if (vmr.vmr_result) {
			errno = vmr.vmr_result;
			log_warn("%s: failed to start vm", vcp->vcp_name);
			vm_remove(vm);
			break;
		}

		/* Now configure all the interfaces */
		if (vm_priv_ifconfig(ps, vm) == -1) {
			log_warn("%s: failed to configure vm", vcp->vcp_name);
			vm_remove(vm);
			break;
		}

		log_info("%s: started vm %d successfully, tty %s",
		    vcp->vcp_name, vm->vm_vmid, vm->vm_ttyname);
		break;
	case IMSG_VMDOP_TERMINATE_VM_RESPONSE:
		IMSG_SIZE_CHECK(imsg, &vmr);
		memcpy(&vmr, imsg->data, sizeof(vmr));
		log_debug("%s: forwarding TERMINATE VM for vm id %d",
		    __func__, vmr.vmr_id);
		proc_forward_imsg(ps, imsg, PROC_CONTROL, -1);
		if ((vm = vm_getbyvmid(vmr.vmr_id)) == NULL)
			break;
		if (vmr.vmr_result == 0) {
			/* Mark VM as shutting down */
			vm->vm_shutdown = 1;
		}
		break;
	case IMSG_VMDOP_SEND_VM_RESPONSE:
		IMSG_SIZE_CHECK(imsg, &vmr);
		memcpy(&vmr, imsg->data, sizeof(vmr));
		if ((vm = vm_getbyvmid(vmr.vmr_id)) == NULL)
			break;
		if (!vmr.vmr_result)
			log_info("%s: sent vm %d successfully.",
			    vm->vm_params.vmc_params.vcp_name,
			    vm->vm_vmid);
	case IMSG_VMDOP_TERMINATE_VM_EVENT:
		IMSG_SIZE_CHECK(imsg, &vmr);
		memcpy(&vmr, imsg->data, sizeof(vmr));
		log_debug("%s: handling TERMINATE_EVENT for vm id %d ret %d",
		    __func__, vmr.vmr_id, vmr.vmr_result);
		if ((vm = vm_getbyvmid(vmr.vmr_id)) == NULL) {
			log_debug("%s: vm %d is no longer available",
			    __func__, vmr.vmr_id);
			break;
		}
		if (vmr.vmr_result != EAGAIN) {
			if (vm->vm_from_config) {
				log_debug("%s: about to stop vm id %d",
				    __func__, vm->vm_vmid);
				vm_stop(vm, 0);
			} else {
				log_debug("%s: about to remove vm %d",
				    __func__, vm->vm_vmid);
				vm_remove(vm);
			}
		} else {
			/* Stop VM instance but keep the tty open */
			log_debug("%s: about to stop vm id %d with tty open",
			    __func__, vm->vm_vmid);
			vm_stop(vm, 1);
			config_setvm(ps, vm, (uint32_t)-1, vm->vm_uid);
		}
		break;
	case IMSG_VMDOP_GET_INFO_VM_DATA:
		IMSG_SIZE_CHECK(imsg, &vir);
		memcpy(&vir, imsg->data, sizeof(vir));
		if ((vm = vm_getbyvmid(vir.vir_info.vir_id)) != NULL) {
			memset(vir.vir_ttyname, 0, sizeof(vir.vir_ttyname));
			if (vm->vm_ttyname != NULL)
				strlcpy(vir.vir_ttyname, vm->vm_ttyname,
				    sizeof(vir.vir_ttyname));
			if (vm->vm_shutdown) {
				/* XXX there might be a nicer way */
				(void)strlcat(vir.vir_info.vir_name,
				    " - stopping",
				    sizeof(vir.vir_info.vir_name));
			}
			/* get the user id who started the vm */
			vir.vir_uid = vm->vm_uid;
			vir.vir_gid = vm->vm_params.vmc_gid;
		}
		if (proc_compose_imsg(ps, PROC_CONTROL, -1, imsg->hdr.type,
		    imsg->hdr.peerid, -1, &vir, sizeof(vir)) == -1) {
			log_debug("%s: GET_INFO_VM failed for vm %d, removing",
			    __func__, vm->vm_vmid);
			vm_remove(vm);
			return (-1);
		}
		break;
	case IMSG_VMDOP_GET_INFO_VM_END_DATA:
		/*
		 * PROC_VMM has responded with the *running* VMs, now we
		 * append the others. These use the special value 0 for their
		 * kernel id to indicate that they are not running.
		 */
		TAILQ_FOREACH(vm, env->vmd_vms, vm_entry) {
			if (!vm->vm_running) {
				memset(&vir, 0, sizeof(vir));
				vir.vir_info.vir_id = vm->vm_vmid;
				strlcpy(vir.vir_info.vir_name,
				    vm->vm_params.vmc_params.vcp_name,
				    VMM_MAX_NAME_LEN);
				vir.vir_info.vir_memory_size =
				    vm->vm_params.vmc_params.vcp_memranges[0].vmr_size;
				vir.vir_info.vir_ncpus =
				    vm->vm_params.vmc_params.vcp_ncpus;
				/* get the configured user id for this vm */
				vir.vir_uid = vm->vm_params.vmc_uid;
				vir.vir_gid = vm->vm_params.vmc_gid;
				if (proc_compose_imsg(ps, PROC_CONTROL, -1,
				    IMSG_VMDOP_GET_INFO_VM_DATA,
				    imsg->hdr.peerid, -1, &vir,
				    sizeof(vir)) == -1) {
					log_debug("%s: GET_INFO_VM_END failed",
					    __func__);
					vm_remove(vm);
					return (-1);
				}
			}
		}
		IMSG_SIZE_CHECK(imsg, &res);
		proc_forward_imsg(ps, imsg, PROC_CONTROL, -1);
		break;
	default:
		return (-1);
	}

	return (0);
}

int
check_vmh(struct vm_dump_header *vmh) {
	int i;
	unsigned int code, leaf;
	unsigned int a, b, c, d;


	if (vmh->vmh_version != VM_DUMP_VERSION) {
		log_warnx("%s: incompatible dump version", __func__);
		return (-1);
	}

	for (i = 0; i < VM_DUMP_HEADER_CPUID_COUNT; i++) {
		code = vmh->vmh_cpuids[i].code;
		leaf = vmh->vmh_cpuids[i].leaf;
		if (leaf != 0x00) {
			log_debug("%s: invalid leaf 0x%x for code 0x%x",
			    __func__, leaf, code);
			return (-1);
		}

		switch(code) {
		case 0x00:
		CPUID_LEAF(code, leaf, a, b, c, d);
		if (vmh->vmh_cpuids[i].a > a) {
			log_debug("%s: incompatible cpuid level", __func__);
			return (-1);
		}
		if (!(vmh->vmh_cpuids[i].b == b &&
		    vmh->vmh_cpuids[i].c == c &&
		    vmh->vmh_cpuids[i].d == d)) {
			log_debug("%s: incompatible cpu brand", __func__);
			return (-1);
		}
		break;

		case 0x01:
		CPUID_LEAF(code, leaf, a, b, c, d);
		if ((vmh->vmh_cpuids[i].c & c & VMM_CPUIDECX_MASK) !=
		    (vmh->vmh_cpuids[i].c & VMM_CPUIDECX_MASK)) {
			log_debug("%s: incompatible cpu features "
			    "code: 0x%x leaf: 0x%x  reg: c", __func__,
			    code, leaf);
			return (-1);
		}
		if ((vmh->vmh_cpuids[i].d & d & VMM_CPUIDEDX_MASK) !=
		    (vmh->vmh_cpuids[i].d & VMM_CPUIDEDX_MASK)) {
			log_debug("%s: incompatible cpu features "
			    "code: 0x%x leaf: 0x%x  reg: d", __func__,
			    code, leaf);
			return (-1);
		}
		break;

		case 0x07:
		CPUID_LEAF(code, leaf, a, b, c, d);
		if ((vmh->vmh_cpuids[i].b & b & VMM_SEFF0EBX_MASK) !=
		    (vmh->vmh_cpuids[i].b & VMM_SEFF0EBX_MASK)) {
			log_debug("%s: incompatible cpu features "
			    "code: 0x%x leaf: 0x%x  reg: c", __func__,
			    code, leaf);
			return (-1);
		}
		if ((vmh->vmh_cpuids[i].c & c & VMM_SEFF0ECX_MASK) !=
		    (vmh->vmh_cpuids[i].c & VMM_SEFF0ECX_MASK)) {
			log_debug("%s: incompatible cpu features "
			    "code: 0x%x leaf: 0x%x  reg: d", __func__,
			    code, leaf);
			return (-1);
		}
		break;

		case 0x0d:
		CPUID_LEAF(code, leaf, a, b, c, d);
		if (vmh->vmh_cpuids[i].b > b) {
			log_debug("%s: incompatible cpu: insufficient "
			    "max save area for enabled XCR0 features",
			    __func__);
			return (-1);
		}
		if (vmh->vmh_cpuids[i].c > c) {
			log_debug("%s: incompatible cpu: insufficient "
			    "max save area for supported XCR0 features",
			    __func__);
			return (-1);
		}
		break;

		case 0x80000001:
		CPUID_LEAF(code, leaf, a, b, c, d);
		if ((vmh->vmh_cpuids[i].a & a) != vmh->vmh_cpuids[i].a) {
			log_debug("%s: incompatible cpu features "
			    "code: 0x%x leaf: 0x%x  reg: a", __func__,
			    code, leaf);
			return (-1);
		}
		if ((vmh->vmh_cpuids[i].c & c) != vmh->vmh_cpuids[i].c) {
			log_debug("%s: incompatible cpu features "
			    "code: 0x%x leaf: 0x%x  reg: c", __func__,
			    code, leaf);
			return (-1);
		}
		if ((vmh->vmh_cpuids[i].d & d) != vmh->vmh_cpuids[i].d) {
			log_debug("%s: incompatible cpu features "
			    "code: 0x%x leaf: 0x%x  reg: d", __func__,
			    code, leaf);
			return (-1);
		}
		break;

		default:
		log_debug("%s: unknown code 0x%x", __func__, code);
		return (-1);
		}
	}

	return (0);
}

void
vmd_sighdlr(int sig, short event, void *arg)
{
	if (privsep_process != PROC_PARENT)
		return;
	log_debug("%s: handling signal", __func__);

	switch (sig) {
	case SIGHUP:
		log_info("%s: reload requested with SIGHUP", __func__);

		/*
		 * This is safe because libevent uses async signal handlers
		 * that run in the event loop and not in signal context.
		 */
		(void)vmd_reload(0, NULL);
		break;
	case SIGPIPE:
		log_info("%s: ignoring SIGPIPE", __func__);
		break;
	case SIGUSR1:
		log_info("%s: ignoring SIGUSR1", __func__);
		break;
	case SIGTERM:
	case SIGINT:
		vmd_shutdown();
		break;
	default:
		fatalx("unexpected signal");
	}
}

__dead void
usage(void)
{
	extern char *__progname;
	fprintf(stderr, "usage: %s [-dnv] [-D macro=value] [-f file]\n",
	    __progname);
	exit(1);
}

int
main(int argc, char **argv)
{
	struct privsep		*ps;
	int			 ch;
	const char		*conffile = VMD_CONF;
	enum privsep_procid	 proc_id = PROC_PARENT;
	int			 proc_instance = 0;
	const char		*errp, *title = NULL;
	int			 argc0 = argc;

	/* log to stderr until daemonized */
	log_init(1, LOG_DAEMON);

	if ((env = calloc(1, sizeof(*env))) == NULL)
		fatal("calloc: env");

	while ((ch = getopt(argc, argv, "D:P:I:df:vn")) != -1) {
		switch (ch) {
		case 'D':
			if (cmdline_symset(optarg) < 0)
				log_warnx("could not parse macro definition %s",
				    optarg);
			break;
		case 'd':
			env->vmd_debug = 2;
			break;
		case 'f':
			conffile = optarg;
			break;
		case 'v':
			env->vmd_verbose++;
			break;
		case 'n':
			env->vmd_noaction = 1;
			break;
		case 'P':
			title = optarg;
			proc_id = proc_getid(procs, nitems(procs), title);
			if (proc_id == PROC_MAX)
				fatalx("invalid process name");
			break;
		case 'I':
			proc_instance = strtonum(optarg, 0,
			    PROC_MAX_INSTANCES, &errp);
			if (errp)
				fatalx("invalid process instance");
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	if (argc > 0)
		usage();

	if (env->vmd_noaction && !env->vmd_debug)
		env->vmd_debug = 1;

	/* check for root privileges */
	if (env->vmd_noaction == 0) {
		if (geteuid())
			fatalx("need root privileges");
	}

	ps = &env->vmd_ps;
	ps->ps_env = env;
	env->vmd_fd = -1;

	if (config_init(env) == -1)
		fatal("failed to initialize configuration");

	if ((ps->ps_pw = getpwnam(VMD_USER)) == NULL)
		fatal("unknown user %s", VMD_USER);

	/* First proc runs as root without pledge but in default chroot */
	proc_priv->p_pw = &proc_privpw; /* initialized to all 0 */
	proc_priv->p_chroot = ps->ps_pw->pw_dir; /* from VMD_USER */

	/* Open /dev/vmm */
	if (env->vmd_noaction == 0) {
		env->vmd_fd = open(VMM_NODE, O_RDWR);
		if (env->vmd_fd == -1)
			fatal("%s", VMM_NODE);
	}

	/* Configure the control socket */
	ps->ps_csock.cs_name = SOCKET_NAME;
	TAILQ_INIT(&ps->ps_rcsocks);

	/* Configuration will be parsed after forking the children */
	env->vmd_conffile = conffile;

	log_init(env->vmd_debug, LOG_DAEMON);
	log_setverbose(env->vmd_verbose);

	if (env->vmd_noaction)
		ps->ps_noaction = 1;
	ps->ps_instance = proc_instance;
	if (title != NULL)
		ps->ps_title[proc_id] = title;

	/* only the parent returns */
	proc_init(ps, procs, nitems(procs), argc0, argv, proc_id);

	log_procinit("parent");
	if (!env->vmd_debug && daemon(0, 0) == -1)
		fatal("can't daemonize");

	if (ps->ps_noaction == 0)
		log_info("startup");

	event_init();

	signal_set(&ps->ps_evsigint, SIGINT, vmd_sighdlr, ps);
	signal_set(&ps->ps_evsigterm, SIGTERM, vmd_sighdlr, ps);
	signal_set(&ps->ps_evsighup, SIGHUP, vmd_sighdlr, ps);
	signal_set(&ps->ps_evsigpipe, SIGPIPE, vmd_sighdlr, ps);
	signal_set(&ps->ps_evsigusr1, SIGUSR1, vmd_sighdlr, ps);

	signal_add(&ps->ps_evsigint, NULL);
	signal_add(&ps->ps_evsigterm, NULL);
	signal_add(&ps->ps_evsighup, NULL);
	signal_add(&ps->ps_evsigpipe, NULL);
	signal_add(&ps->ps_evsigusr1, NULL);

	if (!env->vmd_noaction)
		proc_connect(ps);

	if (vmd_configure() == -1)
		fatalx("configuration failed");

	event_dispatch();

	log_debug("parent exiting");

	return (0);
}

int
vmd_configure(void)
{
	struct vmd_vm		*vm;
	struct vmd_switch	*vsw;

	if ((env->vmd_ptmfd = open(PATH_PTMDEV, O_RDWR|O_CLOEXEC)) == -1)
		fatal("open %s", PATH_PTMDEV);

	/*
	 * pledge in the parent process:
	 * stdio - for malloc and basic I/O including events.
	 * rpath - for reload to open and read the configuration files.
	 * wpath - for opening disk images and tap devices.
	 * tty - for openpty and TIOCUCNTL.
	 * proc - run kill to terminate its children safely.
	 * sendfd - for disks, interfaces and other fds.
	 * recvfd - for send and receive.
	 * getpw - lookup user or group id by name.
	 * chown, fattr - change tty ownership
	 * flock - locking disk files
	 */
	if (pledge("stdio rpath wpath proc tty recvfd sendfd getpw"
	    " chown fattr flock", NULL) == -1)
		fatal("pledge");

	if (parse_config(env->vmd_conffile) == -1) {
		proc_kill(&env->vmd_ps);
		exit(1);
	}

	if (env->vmd_noaction) {
		fprintf(stderr, "configuration OK\n");
		proc_kill(&env->vmd_ps);
		exit(0);
	}

	TAILQ_FOREACH(vsw, env->vmd_switches, sw_entry) {
		if (vsw->sw_running)
			continue;
		if (vm_priv_brconfig(&env->vmd_ps, vsw) == -1) {
			log_warn("%s: failed to create switch %s",
			    __func__, vsw->sw_name);
			switch_remove(vsw);
			return (-1);
		}
	}

	TAILQ_FOREACH(vm, env->vmd_vms, vm_entry) {
		if (vm->vm_disabled) {
			log_debug("%s: not creating vm %s (disabled)",
			    __func__,
			    vm->vm_params.vmc_params.vcp_name);
			continue;
		}
		if (config_setvm(&env->vmd_ps, vm, -1, vm->vm_params.vmc_uid) == -1)
			return (-1);
	}

	/* Send shared global configuration to all children */
	if (config_setconfig(env) == -1)
		return (-1);

	return (0);
}

int
vmd_reload(unsigned int reset, const char *filename)
{
	struct vmd_vm		*vm, *next_vm;
	struct vmd_switch	*vsw;
	int			 reload = 0;

	/* Switch back to the default config file */
	if (filename == NULL || *filename == '\0') {
		filename = env->vmd_conffile;
		reload = 1;
	}

	log_debug("%s: level %d config file %s", __func__, reset, filename);

	if (reset) {
		/* Purge the configuration */
		config_purge(env, reset);
		config_setreset(env, reset);
	} else {
		/*
		 * Load or reload the configuration.
		 *
		 * Reloading removes all non-running VMs before processing the
		 * config file, whereas loading only adds to the existing list
		 * of VMs.
		 */

		if (reload) {
			TAILQ_FOREACH_SAFE(vm, env->vmd_vms, vm_entry, next_vm) {
				if (vm->vm_running == 0) {
					log_debug("%s: calling vm_remove",
					    __func__);
					vm_remove(vm);
				}
			}

			/* Update shared global configuration in all children */
			if (config_setconfig(env) == -1)
				return (-1);
		}

		if (parse_config(filename) == -1) {
			log_debug("%s: failed to load config file %s",
			    __func__, filename);
			return (-1);
		}

		TAILQ_FOREACH(vsw, env->vmd_switches, sw_entry) {
			if (vsw->sw_running)
				continue;
			if (vm_priv_brconfig(&env->vmd_ps, vsw) == -1) {
				log_warn("%s: failed to create switch %s",
				    __func__, vsw->sw_name);
				switch_remove(vsw);
				return (-1);
			}
		}

		TAILQ_FOREACH(vm, env->vmd_vms, vm_entry) {
			if (vm->vm_running == 0) {
				if (vm->vm_disabled) {
					log_debug("%s: not creating vm %s"
					    " (disabled)", __func__,
					    vm->vm_params.vmc_params.vcp_name);
					continue;
				}
				if (config_setvm(&env->vmd_ps, vm, -1, vm->vm_params.vmc_uid) == -1)
					return (-1);
			} else {
				log_debug("%s: not creating vm \"%s\": "
				    "(running)", __func__,
				    vm->vm_params.vmc_params.vcp_name);
			}
		}
	}

	return (0);
}

void
vmd_shutdown(void)
{
	struct vmd_vm *vm, *vm_next;

	log_debug("%s: performing shutdown", __func__);
	TAILQ_FOREACH_SAFE(vm, env->vmd_vms, vm_entry, vm_next) {
		vm_remove(vm);
	}

	proc_kill(&env->vmd_ps);
	free(env);

	log_warnx("parent terminating");
	exit(0);
}

struct vmd_vm *
vm_getbyvmid(uint32_t vmid)
{
	struct vmd_vm	*vm;

	if (vmid == 0)
		return (NULL);
	TAILQ_FOREACH(vm, env->vmd_vms, vm_entry) {
		if (vm->vm_vmid == vmid)
			return (vm);
	}

	return (NULL);
}

struct vmd_vm *
vm_getbyid(uint32_t id)
{
	struct vmd_vm	*vm;

	if (id == 0)
		return (NULL);
	TAILQ_FOREACH(vm, env->vmd_vms, vm_entry) {
		if (vm->vm_params.vmc_params.vcp_id == id)
			return (vm);
	}

	return (NULL);
}

uint32_t
vm_id2vmid(uint32_t id, struct vmd_vm *vm)
{
	if (vm == NULL && (vm = vm_getbyid(id)) == NULL)
		return (0);
	dprintf("%s: vmm id %u is vmid %u", __func__,
	    id, vm->vm_vmid);
	return (vm->vm_vmid);
}

uint32_t
vm_vmid2id(uint32_t vmid, struct vmd_vm *vm)
{
	if (vm == NULL && (vm = vm_getbyvmid(vmid)) == NULL)
		return (0);
	dprintf("%s: vmid %u is vmm id %u", __func__,
	    vmid, vm->vm_params.vmc_params.vcp_id);
	return (vm->vm_params.vmc_params.vcp_id);
}

struct vmd_vm *
vm_getbyname(const char *name)
{
	struct vmd_vm	*vm;

	if (name == NULL)
		return (NULL);
	TAILQ_FOREACH(vm, env->vmd_vms, vm_entry) {
		if (strcmp(vm->vm_params.vmc_params.vcp_name, name) == 0)
			return (vm);
	}

	return (NULL);
}

struct vmd_vm *
vm_getbypid(pid_t pid)
{
	struct vmd_vm	*vm;

	TAILQ_FOREACH(vm, env->vmd_vms, vm_entry) {
		if (vm->vm_pid == pid)
			return (vm);
	}

	return (NULL);
}

void
vm_stop(struct vmd_vm *vm, int keeptty)
{
	unsigned int	 i;

	if (vm == NULL)
		return;

	log_debug("%s: stopping vm %d", __func__, vm->vm_vmid);
	vm->vm_running = 0;
	vm->vm_shutdown = 0;

	if (vm->vm_iev.ibuf.fd != -1) {
		event_del(&vm->vm_iev.ev);
		close(vm->vm_iev.ibuf.fd);
	}
	for (i = 0; i < VMM_MAX_DISKS_PER_VM; i++) {
		if (vm->vm_disks[i] != -1) {
			close(vm->vm_disks[i]);
			vm->vm_disks[i] = -1;
		}
	}
	for (i = 0; i < VMM_MAX_NICS_PER_VM; i++) {
		if (vm->vm_ifs[i].vif_fd != -1) {
			close(vm->vm_ifs[i].vif_fd);
			vm->vm_ifs[i].vif_fd = -1;
		}
		free(vm->vm_ifs[i].vif_name);
		free(vm->vm_ifs[i].vif_switch);
		free(vm->vm_ifs[i].vif_group);
		vm->vm_ifs[i].vif_name = NULL;
		vm->vm_ifs[i].vif_switch = NULL;
		vm->vm_ifs[i].vif_group = NULL;
	}
	if (vm->vm_kernel != -1) {
		close(vm->vm_kernel);
		vm->vm_kernel = -1;
	}
	if (vm->vm_cdrom != -1) {
		close(vm->vm_cdrom);
		vm->vm_cdrom = -1;
	}
	if (!keeptty) {
		vm_closetty(vm);
		vm->vm_uid = 0;
	}
}

void
vm_remove(struct vmd_vm *vm)
{
	if (vm == NULL)
		return;

	log_debug("%s: removing vm id %d from running config",
	    __func__, vm->vm_vmid);
	TAILQ_REMOVE(env->vmd_vms, vm, vm_entry);
	log_debug("%s: calling vm_stop", __func__);
	vm_stop(vm, 0);
	free(vm);
}

int
vm_register(struct privsep *ps, struct vmop_create_params *vmc,
    struct vmd_vm **ret_vm, uint32_t id, uid_t uid)
{
	struct vmd_vm		*vm = NULL;
	struct vm_create_params	*vcp = &vmc->vmc_params;
	static const uint8_t	 zero_mac[ETHER_ADDR_LEN];
	uint32_t		 rng;
	unsigned int		 i;
	struct vmd_switch	*sw;
	char			*s;

	errno = 0;
	*ret_vm = NULL;

	if ((vm = vm_getbyname(vcp->vcp_name)) != NULL ||
	    (vm = vm_getbyvmid(vcp->vcp_id)) != NULL) {
		if (vm_checkperm(vm, uid) != 0) {
			errno = EPERM;
			goto fail;
		}
		*ret_vm = vm;
		errno = EALREADY;
		goto fail;
	}

	/*
	 * non-root users can only start existing VMs
	 * XXX there could be a mechanism to allow overriding some options
	 */
	if (vm_checkperm(NULL, uid) != 0) {
		errno = EPERM;
		goto fail;
	}
	if (vmc->vmc_flags == 0) {
		errno = ENOENT;
		goto fail;
	}
	if (vcp->vcp_ncpus == 0)
		vcp->vcp_ncpus = 1;
	if (vcp->vcp_memranges[0].vmr_size == 0)
		vcp->vcp_memranges[0].vmr_size = VM_DEFAULT_MEMORY;
	if (vcp->vcp_ncpus > VMM_MAX_VCPUS_PER_VM) {
		log_warnx("invalid number of CPUs");
		goto fail;
	} else if (vcp->vcp_ndisks > VMM_MAX_DISKS_PER_VM) {
		log_warnx("invalid number of disks");
		goto fail;
	} else if (vcp->vcp_nnics > VMM_MAX_NICS_PER_VM) {
		log_warnx("invalid number of interfaces");
		goto fail;
	} else if (strlen(vcp->vcp_kernel) == 0 &&
	    vcp->vcp_ndisks == 0 && strlen(vcp->vcp_cdrom) == 0) {
		log_warnx("no kernel or disk/cdrom specified");
		goto fail;
	} else if (strlen(vcp->vcp_name) == 0) {
		log_warnx("invalid VM name");
		goto fail;
	} else if (*vcp->vcp_name == '-' || *vcp->vcp_name == '.' ||
		   *vcp->vcp_name == '_') {
		log_warnx("Invalid VM name");
		goto fail;
	} else {
		for (s = vcp->vcp_name; *s != '\0'; ++s) {
			if (!(isalnum(*s) || *s == '.' || *s == '-' ||
			      *s == '_')) {
				log_warnx("Invalid VM name");
				goto fail;
			}
		}
	}

	if ((vm = calloc(1, sizeof(*vm))) == NULL)
		goto fail;

	memcpy(&vm->vm_params, vmc, sizeof(vm->vm_params));
	vmc = &vm->vm_params;
	vcp = &vmc->vmc_params;
	vm->vm_pid = -1;
	vm->vm_tty = -1;
	vm->vm_receive_fd = -1;
	vm->vm_paused = 0;

	for (i = 0; i < vcp->vcp_ndisks; i++)
		vm->vm_disks[i] = -1;
	for (i = 0; i < vcp->vcp_nnics; i++) {
		vm->vm_ifs[i].vif_fd = -1;

		if ((sw = switch_getbyname(vmc->vmc_ifswitch[i])) != NULL) {
			/* inherit per-interface flags from the switch */
			vmc->vmc_ifflags[i] |= (sw->sw_flags & VMIFF_OPTMASK);
		}

		/*
		 * If the MAC address is zero, always randomize it in vmd(8)
		 * because we cannot rely on the guest OS to do the right
		 * thing like OpenBSD does.  Based on ether_fakeaddr()
		 * from the kernel, incremented by one to differentiate
		 * the source.
		 */
		if (memcmp(zero_mac, &vcp->vcp_macs[i], ETHER_ADDR_LEN) == 0) {
			rng = arc4random();
			vcp->vcp_macs[i][0] = 0xfe;
			vcp->vcp_macs[i][1] = 0xe1;
			vcp->vcp_macs[i][2] = 0xba + 1;
			vcp->vcp_macs[i][3] = 0xd0 | ((i + 1) & 0xf);
			vcp->vcp_macs[i][4] = rng;
			vcp->vcp_macs[i][5] = rng >> 8;
		}
	}
	vm->vm_kernel = -1;
	vm->vm_cdrom = -1;
	vm->vm_iev.ibuf.fd = -1;

	if (++env->vmd_nvm == 0)
		fatalx("too many vms");

	/* Assign a new internal Id if not specified */
	vm->vm_vmid = id == 0 ? env->vmd_nvm : id;

	log_debug("%s: registering vm %d", __func__, vm->vm_vmid);
	TAILQ_INSERT_TAIL(env->vmd_vms, vm, vm_entry);

	*ret_vm = vm;
	return (0);
 fail:
	if (errno == 0)
		errno = EINVAL;
	return (-1);
}

/*
 * vm_checkperm
 *
 * Checks if the user represented by the 'uid' parameter is allowed to
 * manipulate the VM described by the 'vm' parameter (or connect to said VM's
 * console.)
 *
 * Parameters:
 *  vm: the VM whose permission is to be checked
 *  uid: the user ID of the user making the request
 *
 * Return values:
 *   0: the permission should be granted
 *  -1: the permission check failed (also returned if vm == null)
 */
int
vm_checkperm(struct vmd_vm *vm, uid_t uid)
{
	struct group	*gr;
	struct passwd	*pw;
	char		**grmem;

	/* root has no restrictions */
	if (uid == 0)
		return (0);

	if (vm == NULL)
		return (-1);

	/* check supplementary groups */
	if (vm->vm_params.vmc_gid != -1 &&
	    (pw = getpwuid(uid)) != NULL &&
	    (gr = getgrgid(vm->vm_params.vmc_gid)) != NULL) {
		for (grmem = gr->gr_mem; *grmem; grmem++)
			if (strcmp(*grmem, pw->pw_name) == 0)
				return (0);
	}

	/* check user */
	if ((vm->vm_running && vm->vm_uid == uid) ||
	    (!vm->vm_running && vm->vm_params.vmc_uid == uid))
		return (0);

	return (-1);
}

int
vm_opentty(struct vmd_vm *vm)
{
	struct ptmget		 ptm;
	struct stat		 st;
	struct group		*gr;
	uid_t			 uid;
	gid_t			 gid;
	mode_t			 mode;
	int			 on;

	/*
	 * Open tty with pre-opened PTM fd
	 */
	if ((ioctl(env->vmd_ptmfd, PTMGET, &ptm) == -1))
		return (-1);

	/*
	 * We use user ioctl(2) mode to pass break commands.
	 */
	on = 1;
	if (ioctl(ptm.cfd, TIOCUCNTL, &on))
		fatal("could not enable user ioctl mode");

	vm->vm_tty = ptm.cfd;
	close(ptm.sfd);
	if ((vm->vm_ttyname = strdup(ptm.sn)) == NULL)
		goto fail;

	uid = vm->vm_uid;
	gid = vm->vm_params.vmc_gid;

	if (vm->vm_params.vmc_gid != -1) {
		mode = 0660;
	} else if ((gr = getgrnam("tty")) != NULL) {
		gid = gr->gr_gid;
		mode = 0620;
	} else {
		mode = 0600;
		gid = 0;
	}

	log_debug("%s: vm %s tty %s uid %d gid %d mode %o",
	    __func__, vm->vm_params.vmc_params.vcp_name,
	    vm->vm_ttyname, uid, gid, mode);

	/*
	 * Change ownership and mode of the tty as required.
	 * Loosely based on the implementation of sshpty.c
	 */
	if (stat(vm->vm_ttyname, &st) == -1)
		goto fail;

	if (st.st_uid != uid || st.st_gid != gid) {
		if (chown(vm->vm_ttyname, uid, gid) == -1) {
			log_warn("chown %s %d %d failed, uid %d",
			    vm->vm_ttyname, uid, gid, getuid());

			/* Ignore failure on read-only filesystems */
			if (!((errno == EROFS) &&
			    (st.st_uid == uid || st.st_uid == 0)))
				goto fail;
		}
	}

	if ((st.st_mode & (S_IRWXU|S_IRWXG|S_IRWXO)) != mode) {
		if (chmod(vm->vm_ttyname, mode) == -1) {
			log_warn("chmod %s %o failed, uid %d",
			    vm->vm_ttyname, mode, getuid());

			/* Ignore failure on read-only filesystems */
			if (!((errno == EROFS) &&
			    (st.st_uid == uid || st.st_uid == 0)))
				goto fail;
		}
	}

	return (0);
 fail:
	vm_closetty(vm);
	return (-1);
}

void
vm_closetty(struct vmd_vm *vm)
{
	if (vm->vm_tty != -1) {
		/* Release and close the tty */
		if (fchown(vm->vm_tty, 0, 0) == -1)
			log_warn("chown %s 0 0 failed", vm->vm_ttyname);
		if (fchmod(vm->vm_tty, 0666) == -1)
			log_warn("chmod %s 0666 failed", vm->vm_ttyname);
		close(vm->vm_tty);
		vm->vm_tty = -1;
	}
	free(vm->vm_ttyname);
	vm->vm_ttyname = NULL;
}

void
switch_remove(struct vmd_switch *vsw)
{
	if (vsw == NULL)
		return;

	TAILQ_REMOVE(env->vmd_switches, vsw, sw_entry);

	free(vsw->sw_group);
	free(vsw->sw_name);
	free(vsw);
}

struct vmd_switch *
switch_getbyname(const char *name)
{
	struct vmd_switch	*vsw;

	if (name == NULL)
		return (NULL);
	TAILQ_FOREACH(vsw, env->vmd_switches, sw_entry) {
		if (strcmp(vsw->sw_name, name) == 0)
			return (vsw);
	}

	return (NULL);
}

char *
get_string(uint8_t *ptr, size_t len)
{
	size_t	 i;

	for (i = 0; i < len; i++)
		if (!isprint(ptr[i]))
			break;

	return strndup(ptr, i);
}

uint32_t
prefixlen2mask(uint8_t prefixlen)
{
	if (prefixlen == 0)
		return (0);

	if (prefixlen > 32)
		prefixlen = 32;

	return (htonl(0xffffffff << (32 - prefixlen)));
}
