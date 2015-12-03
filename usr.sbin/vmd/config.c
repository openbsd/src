/*	$OpenBSD: config.c,v 1.4 2015/12/03 23:32:32 reyk Exp $	*/

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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <util.h>
#include <imsg.h>

#include "proc.h"
#include "vmd.h"

int
config_init(struct vmd *env)
{
	struct privsep	*ps = &env->vmd_ps;
	unsigned int	 what;

	/* Global configuration */
	if (privsep_process == PROC_PARENT) {
		ps->ps_what[PROC_PARENT] = CONFIG_ALL;
		ps->ps_what[PROC_VMM] = CONFIG_VMS;
	}

	/* Other configuration */
	what = ps->ps_what[privsep_process];
	if (what & CONFIG_VMS) {
		if ((env->vmd_vms = calloc(1, sizeof(*env->vmd_vms))) == NULL)
			return (-1);
		TAILQ_INIT(env->vmd_vms);
	}

	return (0);
}

void
config_purge(struct vmd *env, unsigned int reset)
{
	struct privsep		*ps = &env->vmd_ps;
	struct vmd_vm		*vm;
	unsigned int		 what;

	what = ps->ps_what[privsep_process] & reset;
	if (what & CONFIG_VMS && env->vmd_vms != NULL) {
		while ((vm = TAILQ_FIRST(env->vmd_vms)) != NULL)
			vm_remove(vm);
		env->vmd_vmcount = 0;
	}
}

int
config_setreset(struct vmd *env, unsigned int reset)
{
	struct privsep	*ps = &env->vmd_ps;
	unsigned int	 id;

	for (id = 0; id < PROC_MAX; id++) {
		if ((reset & ps->ps_what[id]) == 0 ||
		    id == privsep_process)
			continue;
		proc_compose(ps, id, IMSG_CTL_RESET, &reset, sizeof(reset));
	}

	return (0);
}

int
config_getreset(struct vmd *env, struct imsg *imsg)
{
	unsigned int	 mode;

	IMSG_SIZE_CHECK(imsg, &mode);
	memcpy(&mode, imsg->data, sizeof(mode));

	config_purge(env, mode);

	return (0);
}

int
config_getvm(struct privsep *ps, struct vm_create_params *vcp,
    int kernel_fd, uint32_t peerid)
{
	struct vmd		*env = ps->ps_env;
	struct vmd_vm		*vm;
	unsigned int		 i;
	int			 fd, ttys_fd;

	if (vcp->vcp_ncpus > VMM_MAX_VCPUS_PER_VM) {
		log_debug("invalid number of CPUs");
		return (-1);
	} else if (vcp->vcp_ndisks > VMM_MAX_DISKS_PER_VM) {
		log_debug("invalid number of disks");
		return (-1);
	} else if (vcp->vcp_nnics > VMM_MAX_NICS_PER_VM) {
		log_debug("invalid number of interfaces");
		return (-1);
	}

	if ((vm = calloc(1, sizeof(*vm))) == NULL)
		return (-1);

	memcpy(&vm->vm_params, vcp, sizeof(vm->vm_params));

	for (i = 0; i < vcp->vcp_ndisks; i++)
		vm->vm_disks[i] = -1;
	for (i = 0; i < vcp->vcp_nnics; i++)
		vm->vm_ifs[i] = -1;
	vm->vm_kernel = -1;
	vm->vm_vmid = ++env->vmd_nvm;

	if (vm_getbyvmid(vm->vm_vmid) != NULL)
		fatalx("too many vms");

	TAILQ_INSERT_TAIL(env->vmd_vms, vm, vm_entry);

	if (privsep_process != PROC_PARENT) {
		if (kernel_fd == -1) {
			log_debug("invalid kernel fd");
			goto fail;
		}
		vm->vm_kernel = kernel_fd;
	} else {
		vm->vm_peerid = peerid;

		/* Open kernel for child */
		if ((fd = open(vcp->vcp_kernel, O_RDONLY)) == -1) {
			log_warn("%s: can't open kernel %s", __func__,
			    vcp->vcp_kernel);
			goto fail;
		}

		proc_compose_imsg(ps, PROC_VMM, -1,
		    IMSG_VMDOP_START_VM_REQUEST, vm->vm_vmid, fd,
		    vcp, sizeof(*vcp));

		/* Open disk images for child */
		for (i = 0 ; i < vcp->vcp_ndisks; i++) {
			if ((fd = open(vcp->vcp_disks[i], O_RDWR)) == -1) {
				log_warn("%s: can't open %s", __func__,
				    vcp->vcp_disks[i]);
				goto fail;
			}
			proc_compose_imsg(ps, PROC_VMM, -1,
			    IMSG_VMDOP_START_VM_DISK, vm->vm_vmid, fd,
			    &i, sizeof(i));
		}

		/* Open disk network interfaces */
		for (i = 0 ; i < vcp->vcp_nnics; i++) {
			if ((fd = opentap()) == -1) {
				log_warn("%s: can't open tap", __func__);
				goto fail;
			}
			proc_compose_imsg(ps, PROC_VMM, -1,
			    IMSG_VMDOP_START_VM_IF, vm->vm_vmid, fd,
			    &i, sizeof(i));
		}

		/* Open TTY */
		if (openpty(&fd, &ttys_fd,
		    vm->vm_ttyname, NULL, NULL) == -1) {
			log_warn("%s: can't open tty", __func__);
			goto fail;
		}
		close(ttys_fd);

		proc_compose_imsg(ps, PROC_VMM, -1,
		    IMSG_VMDOP_START_VM_END, vm->vm_vmid, fd,
		    vcp, sizeof(*vcp));
	}

	return (0);

 fail:
	vm_remove(vm);
	return (-1);
}

int
config_getdisk(struct privsep *ps, struct imsg *imsg)
{
	struct vmd_vm	*vm;
	unsigned int	 n;

	if ((vm = vm_getbyvmid(imsg->hdr.peerid)) == NULL)
		return (-1);

	IMSG_SIZE_CHECK(imsg, &n);
	memcpy(&n, imsg->data, sizeof(n));

	if (n >= vm->vm_params.vcp_ndisks ||
	    vm->vm_disks[n] != -1 || imsg->fd == -1) {
		log_debug("invalid disk id");
		return (-1);
	}
	vm->vm_disks[n] = imsg->fd;

	return (0);
}

int
config_getif(struct privsep *ps, struct imsg *imsg)
{
	struct vmd_vm	*vm;
	unsigned int	 n;

	if ((vm = vm_getbyvmid(imsg->hdr.peerid)) == NULL)
		return (-1);

	IMSG_SIZE_CHECK(imsg, &n);
	memcpy(&n, imsg->data, sizeof(n));
	if (n >= vm->vm_params.vcp_nnics ||
	    vm->vm_ifs[n] != -1 || imsg->fd == -1) {
		log_debug("invalid interface id");
		return (-1);
	}
	vm->vm_ifs[n] = imsg->fd;

	return (0);
}
