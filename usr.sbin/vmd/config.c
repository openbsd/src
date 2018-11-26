/*	$OpenBSD: config.c,v 1.57 2018/11/26 05:44:46 ori Exp $	*/

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
#include <sys/stat.h>
#include <sys/socket.h>

#include <net/if.h>

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <util.h>
#include <errno.h>
#include <imsg.h>

#include "proc.h"
#include "vmd.h"

/* Supported bridge types */
const char *vmd_descsw[] = { "switch", "bridge", NULL };

static int	 config_init_localprefix(struct vmd_config *);

static int
config_init_localprefix(struct vmd_config *cfg)
{
	struct sockaddr_in6	*sin6;

	if (host(VMD_DHCP_PREFIX, &cfg->cfg_localprefix) == -1)
		return (-1);

	/* IPv6 is disabled by default */
	cfg->cfg_flags &= ~VMD_CFG_INET6;

	/* Generate random IPv6 prefix only once */
	if (cfg->cfg_flags & VMD_CFG_AUTOINET6)
		return (0);
	if (host(VMD_ULA_PREFIX, &cfg->cfg_localprefix6) == -1)
		return (-1);
	/* Randomize the 56 bits "Global ID" and "Subnet ID" */
	sin6 = ss2sin6(&cfg->cfg_localprefix6.ss);
	arc4random_buf(&sin6->sin6_addr.s6_addr[1], 7);
	cfg->cfg_flags |= VMD_CFG_AUTOINET6;

	return (0);
}

int
config_init(struct vmd *env)
{
	struct privsep		*ps = &env->vmd_ps;
	unsigned int		 what;

	/* Global configuration */
	ps->ps_what[PROC_PARENT] = CONFIG_ALL;
	ps->ps_what[PROC_VMM] = CONFIG_VMS;

	/* Local prefix */
	if (config_init_localprefix(&env->vmd_cfg) == -1)
		return (-1);

	/* Other configuration */
	what = ps->ps_what[privsep_process];
	if (what & CONFIG_VMS) {
		if ((env->vmd_vms = calloc(1, sizeof(*env->vmd_vms))) == NULL)
			return (-1);
		if ((env->vmd_known = calloc(1, sizeof(*env->vmd_known))) == NULL)
			return (-1);
		TAILQ_INIT(env->vmd_vms);
		TAILQ_INIT(env->vmd_known);
	}
	if (what & CONFIG_SWITCHES) {
		if ((env->vmd_switches = calloc(1,
		    sizeof(*env->vmd_switches))) == NULL)
			return (-1);
		TAILQ_INIT(env->vmd_switches);
	}
	if (what & CONFIG_USERS) {
		if ((env->vmd_users = calloc(1,
		    sizeof(*env->vmd_users))) == NULL)
			return (-1);
		TAILQ_INIT(env->vmd_users);
	}

	return (0);
}

void
config_purge(struct vmd *env, unsigned int reset)
{
	struct privsep		*ps = &env->vmd_ps;
	struct name2id		*n2i;
	struct vmd_vm		*vm;
	struct vmd_switch	*vsw;
	unsigned int		 what;

	DPRINTF("%s: %s purging vms and switches",
	    __func__, ps->ps_title[privsep_process]);

	/* Reset global configuration (prefix was verified before) */
	config_init_localprefix(&env->vmd_cfg);

	/* Reset other configuration */
	what = ps->ps_what[privsep_process] & reset;
	if (what & CONFIG_VMS && env->vmd_vms != NULL) {
		while ((vm = TAILQ_FIRST(env->vmd_vms)) != NULL) {
			vm_remove(vm, __func__);
		}
		while ((n2i = TAILQ_FIRST(env->vmd_known)) != NULL) {
			TAILQ_REMOVE(env->vmd_known, n2i, entry);
			free(n2i);
		}
		env->vmd_nvm = 0;
	}
	if (what & CONFIG_SWITCHES && env->vmd_switches != NULL) {
		while ((vsw = TAILQ_FIRST(env->vmd_switches)) != NULL)
			switch_remove(vsw);
		env->vmd_nswitches = 0;
	}
}

int
config_setconfig(struct vmd *env)
{
	struct privsep	*ps = &env->vmd_ps;
	unsigned int	 id;

	DPRINTF("%s: setting config", __func__);

	for (id = 0; id < PROC_MAX; id++) {
		if (id == privsep_process)
			continue;
		proc_compose(ps, id, IMSG_VMDOP_CONFIG, &env->vmd_cfg,
		    sizeof(env->vmd_cfg));
	}

	return (0);
}

int
config_getconfig(struct vmd *env, struct imsg *imsg)
{
	struct privsep	*ps = &env->vmd_ps;

	log_debug("%s: %s retrieving config",
	    __func__, ps->ps_title[privsep_process]);

	IMSG_SIZE_CHECK(imsg, &env->vmd_cfg);
	memcpy(&env->vmd_cfg, imsg->data, sizeof(env->vmd_cfg));

	return (0);
}

int
config_setreset(struct vmd *env, unsigned int reset)
{
	struct privsep	*ps = &env->vmd_ps;
	unsigned int	 id;

	DPRINTF("%s: resetting state", __func__);

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

	log_debug("%s: %s resetting state",
	    __func__, env->vmd_ps.ps_title[privsep_process]);

	config_purge(env, mode);

	return (0);
}

int
config_setvm(struct privsep *ps, struct vmd_vm *vm, uint32_t peerid, uid_t uid)
{
	int diskfds[VMM_MAX_DISKS_PER_VM][VM_MAX_BASE_PER_DISK];
	struct vmd_if		*vif;
	struct vmop_create_params *vmc = &vm->vm_params;
	struct vm_create_params	*vcp = &vmc->vmc_params;
	unsigned int		 i, j;
	int			 fd = -1, vmboot = 0;
	int			 kernfd = -1;
	int			*tapfds = NULL;
	int			 cdromfd = -1;
	int			 saved_errno = 0;
	int			 n = 0, aflags, oflags;
	char			 ifname[IF_NAMESIZE], *s;
	char			 path[PATH_MAX];
	char			 base[PATH_MAX];
	unsigned int		 unit;
	struct timeval		 tv, rate, since_last;

	errno = 0;

	if (vm->vm_running) {
		log_warnx("%s: vm is already running", __func__);
		errno = EALREADY;
		return (-1);
	}

	/* increase the user reference counter and check user limits */
	if (vm->vm_user != NULL && user_get(vm->vm_user->usr_id.uid) != NULL) {
		user_inc(vcp, vm->vm_user, 1);
		if (user_checklimit(vm->vm_user, vcp) == -1) {
			errno = EPERM;
			goto fail;
		}
	}

	/*
	 * Rate-limit the VM so that it cannot restart in a loop:
	 * if the VM restarts after less than VM_START_RATE_SEC seconds,
	 * we increment the limit counter.  After VM_START_RATE_LIMIT
	 * of suchs fast reboots the VM is stopped.
	 */
	getmonotime(&tv);
	if (vm->vm_start_tv.tv_sec) {
		timersub(&tv, &vm->vm_start_tv, &since_last);

		rate.tv_sec = VM_START_RATE_SEC;
		rate.tv_usec = 0;
		if (timercmp(&since_last, &rate, <))
			vm->vm_start_limit++;
		else {
			/* Reset counter */
			vm->vm_start_limit = 0;
		}

		log_debug("%s: vm %u restarted after %lld.%ld seconds,"
		    " limit %d/%d", __func__, vcp->vcp_id, since_last.tv_sec,
		    since_last.tv_usec, vm->vm_start_limit,
		    VM_START_RATE_LIMIT);

		if (vm->vm_start_limit >= VM_START_RATE_LIMIT) {
			log_warnx("%s: vm %u restarted too quickly",
			    __func__, vcp->vcp_id);
			errno = EPERM;
			goto fail;
		}
	}
	vm->vm_start_tv = tv;

	for (i = 0; i < VMM_MAX_DISKS_PER_VM; i++)
		for (j = 0; j < VM_MAX_BASE_PER_DISK; j++)
			diskfds[i][j] = -1;

	tapfds = reallocarray(NULL, vcp->vcp_nnics, sizeof(*tapfds));
	if (tapfds == NULL) {
		log_warn("%s: can't allocate tap fds", __func__);
		goto fail;
	}
	for (i = 0; i < vcp->vcp_nnics; i++)
		tapfds[i] = -1;

	vm->vm_peerid = peerid;
	vm->vm_uid = uid;

	if (!vm->vm_received) {
		if (strlen(vcp->vcp_kernel)) {
			/*
			 * Boot kernel from disk image if path matches the
			 * root disk.
			 */
			if (vcp->vcp_ndisks &&
			    strcmp(vcp->vcp_kernel, vcp->vcp_disks[0]) == 0)
				vmboot = 1;
			/* Open external kernel for child */
			else if ((kernfd =
			    open(vcp->vcp_kernel, O_RDONLY)) == -1) {
				log_warn("%s: can't open kernel or BIOS "
				    "boot image %s", __func__, vcp->vcp_kernel);
				goto fail;
			}
		}

		/*
		 * Try to open the default BIOS image if no kernel/BIOS has been
		 * specified.  The BIOS is an external firmware file that is
		 * typically distributed separately due to an incompatible
		 * license.
		 */
		if (kernfd == -1 && !vmboot &&
		    (kernfd = open(VM_DEFAULT_BIOS, O_RDONLY)) == -1) {
			log_warn("can't open %s", VM_DEFAULT_BIOS);
			errno = VMD_BIOS_MISSING;
			goto fail;
		}

		if (!vmboot && vm_checkaccess(kernfd,
		    vmc->vmc_checkaccess & VMOP_CREATE_KERNEL,
		    uid, R_OK) == -1) {
			log_warnx("vm \"%s\" no read access to kernel %s",
			    vcp->vcp_name, vcp->vcp_kernel);
			errno = EPERM;
			goto fail;
		}
	}

	/* Open CDROM image for child */
	if (strlen(vcp->vcp_cdrom)) {
		/* Stat cdrom to ensure it is a regular file */
		if ((cdromfd =
		    open(vcp->vcp_cdrom, O_RDONLY)) == -1) {
			log_warn("can't open cdrom %s", vcp->vcp_cdrom);
			errno = VMD_CDROM_MISSING;
			goto fail;
		}

		if (vm_checkaccess(cdromfd,
		    vmc->vmc_checkaccess & VMOP_CREATE_CDROM,
		    uid, R_OK) == -1) {
			log_warnx("vm \"%s\" no read access to cdrom %s",
			    vcp->vcp_name, vcp->vcp_cdrom);
			errno = EPERM;
			goto fail;
		}
	}

	/* Open disk images for child */
	for (i = 0 ; i < vcp->vcp_ndisks; i++) {
		if (strlcpy(path, vcp->vcp_disks[i], sizeof(path))
		   >= sizeof(path))
			log_warnx("disk path %s too long", vcp->vcp_disks[i]);
		memset(vmc->vmc_diskbases, 0, sizeof(vmc->vmc_diskbases));
		oflags = O_RDWR|O_EXLOCK|O_NONBLOCK;
		aflags = R_OK|W_OK;
		for (j = 0; j < VM_MAX_BASE_PER_DISK; j++) {
			/* Stat disk[i] to ensure it is a regular file */
			if ((diskfds[i][j] = open(path, oflags)) == -1) {
				log_warn("can't open disk %s",
				    vcp->vcp_disks[i]);
				errno = VMD_DISK_MISSING;
				goto fail;
			}

			if (vm_checkaccess(diskfds[i][j],
			    vmc->vmc_checkaccess & VMOP_CREATE_DISK,
			    uid, aflags) == -1) {
				log_warnx("vm \"%s\" unable to access "
				    "disk %s", vcp->vcp_name, path);
				errno = EPERM;
				goto fail;
			}

			/*
			 * Clear the write and exclusive flags for base images.
			 * All writes should go to the top image, allowing them
			 * to be shared.
			 */
			oflags = O_RDONLY|O_NONBLOCK;
			aflags = R_OK;
			n = virtio_get_base(diskfds[i][j], base, sizeof(base),
			    vmc->vmc_disktypes[i], path);
			if (n == 0)
				break;
			if (n == -1) {
				log_warnx("vm \"%s\" unable to read "
				    "base %s for disk %s", vcp->vcp_name,
				    base, vcp->vcp_disks[i]);
				goto fail;
			}
			(void)strlcpy(path, base, sizeof(path));
		}
	}

	/* Open network interfaces */
	for (i = 0 ; i < vcp->vcp_nnics; i++) {
		vif = &vm->vm_ifs[i];

		/* Check if the user has requested a specific tap(4) */
		s = vmc->vmc_ifnames[i];
		if (*s != '\0' && strcmp("tap", s) != 0) {
			if (priv_getiftype(s, ifname, &unit) == -1 ||
			    strcmp(ifname, "tap") != 0) {
				log_warnx("%s: invalid tap name %s",
				    __func__, s);
				errno = EINVAL;
				goto fail;
			}
		} else
			s = NULL;

		/*
		 * Either open the requested tap(4) device or get
		 * the next available one.
		 */
		if (s != NULL) {
			snprintf(path, PATH_MAX, "/dev/%s", s);
			tapfds[i] = open(path, O_RDWR | O_NONBLOCK);
		} else {
			tapfds[i] = opentap(ifname);
			s = ifname;
		}
		if (tapfds[i] == -1) {
			log_warn("%s: can't open tap %s", __func__, s);
			goto fail;
		}
		if ((vif->vif_name = strdup(s)) == NULL) {
			log_warn("%s: can't save tap %s", __func__, s);
			goto fail;
		}

		/* Check if the the interface is attached to a switch */
		s = vmc->vmc_ifswitch[i];
		if (*s != '\0') {
			if ((vif->vif_switch = strdup(s)) == NULL) {
				log_warn("%s: can't save switch %s",
				    __func__, s);
				goto fail;
			}
		}

		/* Check if the the interface is assigned to a group */
		s = vmc->vmc_ifgroup[i];
		if (*s != '\0') {
			if ((vif->vif_group = strdup(s)) == NULL) {
				log_warn("%s: can't save group %s",
				    __func__, s);
				goto fail;
			}
		}

		/* non-default rdomain (requires VMIFF_RDOMAIN below) */
		vif->vif_rdomain = vmc->vmc_ifrdomain[i];

		/* Set the interface status */
		vif->vif_flags =
		    vmc->vmc_ifflags[i] & (VMIFF_UP|VMIFF_OPTMASK);
	}

	/* Open TTY */
	if (vm->vm_ttyname == NULL) {
		if (vm_opentty(vm) == -1) {
			log_warn("%s: can't open tty %s", __func__,
			    vm->vm_ttyname == NULL ? "" : vm->vm_ttyname);
			goto fail;
		}
	}
	if ((fd = dup(vm->vm_tty)) == -1) {
		log_warn("%s: can't re-open tty %s", __func__, vm->vm_ttyname);
		goto fail;
	}

	/* Send VM information */
	if (vm->vm_received)
		proc_compose_imsg(ps, PROC_VMM, -1,
		    IMSG_VMDOP_RECEIVE_VM_REQUEST, vm->vm_vmid, fd,  vmc,
		    sizeof(struct vmop_create_params));
	else
		proc_compose_imsg(ps, PROC_VMM, -1,
		    IMSG_VMDOP_START_VM_REQUEST, vm->vm_vmid, kernfd,
		    vmc, sizeof(*vmc));

	if (strlen(vcp->vcp_cdrom))
		proc_compose_imsg(ps, PROC_VMM, -1,
		    IMSG_VMDOP_START_VM_CDROM, vm->vm_vmid, cdromfd,
		    NULL, 0);

	for (i = 0; i < vcp->vcp_ndisks; i++) {
		for (j = 0; j < VM_MAX_BASE_PER_DISK; j++) {
			if (diskfds[i][j] == -1)
				break;
			proc_compose_imsg(ps, PROC_VMM, -1,
			    IMSG_VMDOP_START_VM_DISK, vm->vm_vmid,
			    diskfds[i][j], &i, sizeof(i));
		}
	}
	for (i = 0; i < vcp->vcp_nnics; i++) {
		proc_compose_imsg(ps, PROC_VMM, -1,
		    IMSG_VMDOP_START_VM_IF, vm->vm_vmid, tapfds[i],
		    &i, sizeof(i));
	}

	if (!vm->vm_received)
		proc_compose_imsg(ps, PROC_VMM, -1,
		    IMSG_VMDOP_START_VM_END, vm->vm_vmid, fd,  NULL, 0);

	free(tapfds);

	vm->vm_running = 1;
	return (0);

 fail:
	saved_errno = errno;
	log_warnx("failed to start vm %s", vcp->vcp_name);

	if (kernfd != -1)
		close(kernfd);
	if (cdromfd != -1)
		close(cdromfd);
	for (i = 0; i < vcp->vcp_ndisks; i++)
		for (j = 0; j < VM_MAX_BASE_PER_DISK; j++)
			if (diskfds[i][j] != -1)
				close(diskfds[i][j]);
	if (tapfds != NULL) {
		for (i = 0; i < vcp->vcp_nnics; i++)
			close(tapfds[i]);
		free(tapfds);
	}

	if (vm->vm_from_config) {
		vm_stop(vm, 0, __func__);
	} else {
		vm_remove(vm, __func__);
	}
	errno = saved_errno;
	if (errno == 0)
		errno = EINVAL;
	return (-1);
}

int
config_getvm(struct privsep *ps, struct imsg *imsg)
{
	struct vmop_create_params	 vmc;
	struct vmd_vm			*vm;

	IMSG_SIZE_CHECK(imsg, &vmc);
	memcpy(&vmc, imsg->data, sizeof(vmc));

	errno = 0;
	if (vm_register(ps, &vmc, &vm, imsg->hdr.peerid, 0) == -1)
		goto fail;

	/* If the fd is -1, the kernel will be searched on the disk */
	vm->vm_kernel = imsg->fd;
	vm->vm_running = 1;
	vm->vm_peerid = (uint32_t)-1;

	return (0);

 fail:
	if (imsg->fd != -1) {
		close(imsg->fd);
		imsg->fd = -1;
	}

	vm_remove(vm, __func__);
	if (errno == 0)
		errno = EINVAL;

	return (-1);
}

int
config_getdisk(struct privsep *ps, struct imsg *imsg)
{
	struct vmd_vm	*vm;
	unsigned int	 n, idx;

	errno = 0;
	if ((vm = vm_getbyvmid(imsg->hdr.peerid)) == NULL) {
		errno = ENOENT;
		return (-1);
	}

	IMSG_SIZE_CHECK(imsg, &n);
	memcpy(&n, imsg->data, sizeof(n));

	if (n >= vm->vm_params.vmc_params.vcp_ndisks || imsg->fd == -1) {
		log_warnx("invalid disk id");
		errno = EINVAL;
		return (-1);
	}
	idx = vm->vm_params.vmc_diskbases[n]++;
	if (idx >= VM_MAX_BASE_PER_DISK) {
		log_warnx("too many bases for disk");
		errno = EINVAL;
		return (-1);
	}
	vm->vm_disks[n][idx] = imsg->fd;
	return (0);
}

int
config_getif(struct privsep *ps, struct imsg *imsg)
{
	struct vmd_vm	*vm;
	unsigned int	 n;

	errno = 0;
	if ((vm = vm_getbyvmid(imsg->hdr.peerid)) == NULL) {
		errno = ENOENT;
		return (-1);
	}

	IMSG_SIZE_CHECK(imsg, &n);
	memcpy(&n, imsg->data, sizeof(n));
	if (n >= vm->vm_params.vmc_params.vcp_nnics ||
	    vm->vm_ifs[n].vif_fd != -1 || imsg->fd == -1) {
		log_warnx("invalid interface id");
		goto fail;
	}
	vm->vm_ifs[n].vif_fd = imsg->fd;
	return (0);
 fail:
	if (imsg->fd != -1)
		close(imsg->fd);
	errno = EINVAL;
	return (-1);
}

int
config_getcdrom(struct privsep *ps, struct imsg *imsg)
{
	struct vmd_vm	*vm;

	errno = 0;
	if ((vm = vm_getbyvmid(imsg->hdr.peerid)) == NULL) {
		errno = ENOENT;
		return (-1);
	}

	if (imsg->fd == -1) {
		log_warnx("invalid cdrom id");
		goto fail;
	}

	vm->vm_cdrom = imsg->fd;
	return (0);
 fail:
	if (imsg->fd != -1)
		close(imsg->fd);
	errno = EINVAL;
	return (-1);
}
