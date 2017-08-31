/*	$OpenBSD: priv.c,v 1.11 2017/08/31 09:00:46 mlarkin Exp $	*/

/*
 * Copyright (c) 2016 Reyk Floeter <reyk@openbsd.org>
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
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/tree.h>

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <net/if_bridge.h>

#include <arpa/inet.h>

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>

#include "proc.h"
#include "vmd.h"

int	 priv_dispatch_parent(int, struct privsep_proc *, struct imsg *);
void	 priv_run(struct privsep *, struct privsep_proc *, void *);

static struct privsep_proc procs[] = {
	{ "parent",	PROC_PARENT,	priv_dispatch_parent }
};

void
priv(struct privsep *ps, struct privsep_proc *p)
{
	proc_run(ps, p, procs, nitems(procs), priv_run, NULL);
}

void
priv_run(struct privsep *ps, struct privsep_proc *p, void *arg)
{
	struct vmd		*env = ps->ps_env;

	/*
	 * no pledge(2) in the "priv" process:
	 * write ioctls are not permitted by pledge.
	 */

	/* Open our own socket for generic interface ioctls */
	if ((env->vmd_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		fatal("socket");
}

int
priv_dispatch_parent(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	const char		*desct[] = { "tap", "switch", "bridge", NULL };
	struct privsep		*ps = p->p_ps;
	struct vmop_ifreq	 vfr;
	struct vmd		*env = ps->ps_env;
	struct ifreq		 ifr;
	struct ifbreq		 ifbr;
	struct ifgroupreq	 ifgr;
	struct ifaliasreq	 ifra;
	char			 type[IF_NAMESIZE];

	switch (imsg->hdr.type) {
	case IMSG_VMDOP_PRIV_IFDESCR:
	case IMSG_VMDOP_PRIV_IFCREATE:
	case IMSG_VMDOP_PRIV_IFRDOMAIN:
	case IMSG_VMDOP_PRIV_IFADD:
	case IMSG_VMDOP_PRIV_IFUP:
	case IMSG_VMDOP_PRIV_IFDOWN:
	case IMSG_VMDOP_PRIV_IFGROUP:
	case IMSG_VMDOP_PRIV_IFADDR:
		IMSG_SIZE_CHECK(imsg, &vfr);
		memcpy(&vfr, imsg->data, sizeof(vfr));

		/* We should not get malicious requests from the parent */
		if (priv_getiftype(vfr.vfr_name, type, NULL) == -1 ||
		    priv_findname(type, desct) == -1)
			fatalx("%s: rejected priv operation on interface: %s",
			    __func__, vfr.vfr_name);
		break;
	case IMSG_VMDOP_CONFIG:
	case IMSG_CTL_RESET:
		break;
	default:
		return (-1);
	}

	switch (imsg->hdr.type) {
	case IMSG_VMDOP_PRIV_IFDESCR:
		/* Set the interface description */
		strlcpy(ifr.ifr_name, vfr.vfr_name, sizeof(ifr.ifr_name));
		ifr.ifr_data = (caddr_t)vfr.vfr_value;
		if (ioctl(env->vmd_fd, SIOCSIFDESCR, &ifr) < 0)
			log_warn("SIOCSIFDESCR");
		break;
	case IMSG_VMDOP_PRIV_IFCREATE:
		/* Create the bridge if it doesn't exist */
		strlcpy(ifr.ifr_name, vfr.vfr_name, sizeof(ifr.ifr_name));
		if (ioctl(env->vmd_fd, SIOCIFCREATE, &ifr) < 0 &&
		    errno != EEXIST)
			log_warn("SIOCIFCREATE");
		break;
	case IMSG_VMDOP_PRIV_IFRDOMAIN:
		strlcpy(ifr.ifr_name, vfr.vfr_name, sizeof(ifr.ifr_name));
		ifr.ifr_rdomainid = vfr.vfr_id;
		if (ioctl(env->vmd_fd, SIOCSIFRDOMAIN, &ifr) < 0)
			log_warn("SIOCSIFRDOMAIN");
		break;
	case IMSG_VMDOP_PRIV_IFADD:
		if (priv_getiftype(vfr.vfr_value, type, NULL) == -1)
			fatalx("%s: rejected to add interface: %s",
			    __func__, vfr.vfr_value);

		/* Attach the device to the bridge */
		strlcpy(ifbr.ifbr_name, vfr.vfr_name,
		    sizeof(ifbr.ifbr_name));
		strlcpy(ifbr.ifbr_ifsname, vfr.vfr_value,
		    sizeof(ifbr.ifbr_ifsname));
		if (ioctl(env->vmd_fd, SIOCBRDGADD, &ifbr) < 0 &&
		    errno != EEXIST)
			log_warn("SIOCBRDGADD");
		break;
	case IMSG_VMDOP_PRIV_IFUP:
	case IMSG_VMDOP_PRIV_IFDOWN:
		/* Set the interface status */
		strlcpy(ifr.ifr_name, vfr.vfr_name, sizeof(ifr.ifr_name));
		if (ioctl(env->vmd_fd, SIOCGIFFLAGS, &ifr) < 0) {
			log_warn("SIOCGIFFLAGS");
			break;
		}
		if (imsg->hdr.type == IMSG_VMDOP_PRIV_IFUP)
			ifr.ifr_flags |= IFF_UP;
		else
			ifr.ifr_flags &= ~IFF_UP;
		if (ioctl(env->vmd_fd, SIOCSIFFLAGS, &ifr) < 0)
			log_warn("SIOCSIFFLAGS");
		break;
	case IMSG_VMDOP_PRIV_IFGROUP:
		if (priv_validgroup(vfr.vfr_value) == -1)
			fatalx("%s: invalid group name", __func__);

		if (strlcpy(ifgr.ifgr_name, vfr.vfr_name,
		    sizeof(ifgr.ifgr_name)) >= sizeof(ifgr.ifgr_name) ||
		    strlcpy(ifgr.ifgr_group, vfr.vfr_value,
		    sizeof(ifgr.ifgr_group)) >= sizeof(ifgr.ifgr_group))
			fatalx("%s: group name too long", __func__);

		if (ioctl(env->vmd_fd, SIOCAIFGROUP, &ifgr) < 0 &&
		    errno != EEXIST)
			log_warn("SIOCAIFGROUP");
		break;
	case IMSG_VMDOP_PRIV_IFADDR:
		memset(&ifra, 0, sizeof(ifra));

		/* Set the interface address */
		strlcpy(ifra.ifra_name, vfr.vfr_name, sizeof(ifra.ifra_name));

		memcpy(&ifra.ifra_addr, &vfr.vfr_ifra.ifra_addr,
		    sizeof(ifra.ifra_addr));
		ifra.ifra_addr.sa_family = AF_INET;
		ifra.ifra_addr.sa_len = sizeof(struct sockaddr_in);

		memcpy(&ifra.ifra_mask, &vfr.vfr_ifra.ifra_mask,
		    sizeof(ifra.ifra_mask));
		ifra.ifra_mask.sa_family = AF_INET;
		ifra.ifra_mask.sa_len = sizeof(struct sockaddr_in);

		if (ioctl(env->vmd_fd, SIOCAIFADDR, &ifra) < 0)
			log_warn("SIOCAIFADDR");
		break;
	case IMSG_VMDOP_CONFIG:
		config_getconfig(env, imsg);
		break;
	case IMSG_CTL_RESET:
		config_getreset(env, imsg);
		break;
	default:
		return (-1);
	}

	return (0);
}

int
priv_getiftype(char *ifname, char *type, unsigned int *unitptr)
{
	const char	*errstr;
	size_t		 span;
	unsigned int	 unit;

	/* Extract the name part */
	span = strcspn(ifname, "0123456789");
	if (span == 0 || span >= strlen(ifname) || span >= (IF_NAMESIZE - 1))
		return (-1);
	memcpy(type, ifname, span);
	type[span] = 0;

	/* Now parse the unit (we don't strictly validate the format here) */
	unit = strtonum(ifname + span, 0, UINT_MAX, &errstr);
	if (errstr != NULL)
		return (-1);
	if (unitptr != NULL)
		*unitptr = unit;

	return (0);
}

int
priv_findname(const char *name, const char **names)
{
	unsigned int	 i;

	for (i = 0; names[i] != NULL; i++) {
		if (strcmp(name, names[i]) == 0)
			return (0);
	}

	return (-1);
}

int
priv_validgroup(const char *name)
{
	if (strlen(name) >= IF_NAMESIZE)
		return (-1);
	/* Group can not end with a digit */
	if (name[0] && isdigit(name[strlen(name) - 1]))
		return (-1);
	return (0);
}

/*
 * Called from the process peer
 */

int
vm_priv_ifconfig(struct privsep *ps, struct vmd_vm *vm)
{
	struct vmd		*env = ps->ps_env;
	struct vm_create_params	*vcp = &vm->vm_params.vmc_params;
	struct vmd_if		*vif;
	struct vmd_switch	*vsw;
	unsigned int		 i;
	struct vmop_ifreq	 vfr, vfbr;
	struct sockaddr_in	*sin4;

	for (i = 0; i < VMM_MAX_NICS_PER_VM; i++) {
		vif = &vm->vm_ifs[i];

		if (vif->vif_name == NULL)
			break;

		if (strlcpy(vfr.vfr_name, vif->vif_name,
		    sizeof(vfr.vfr_name)) >= sizeof(vfr.vfr_name))
			return (-1);

		/* Use the configured rdomain or get it from the process */
		if (vif->vif_flags & VMIFF_RDOMAIN)
			vfr.vfr_id = vif->vif_rdomain;
		else
			vfr.vfr_id = getrtable();
		if (vfr.vfr_id != 0)
			log_debug("%s: interface %s rdomain %u", __func__,
			    vfr.vfr_name, vfr.vfr_id);

		proc_compose(ps, PROC_PRIV, IMSG_VMDOP_PRIV_IFRDOMAIN,
		    &vfr, sizeof(vfr));

		/* Description can be truncated */
		(void)snprintf(vfr.vfr_value, sizeof(vfr.vfr_value),
		    "vm%u-if%u-%s", vm->vm_vmid, i, vcp->vcp_name);

		log_debug("%s: interface %s description %s", __func__,
		    vfr.vfr_name, vfr.vfr_value);

		proc_compose(ps, PROC_PRIV, IMSG_VMDOP_PRIV_IFDESCR,
		    &vfr, sizeof(vfr));

		/* Add interface to bridge/switch */
		if ((vsw = switch_getbyname(vif->vif_switch)) != NULL) {
			memset(&vfbr, 0, sizeof(vfbr));

			if (strlcpy(vfbr.vfr_name, vsw->sw_ifname,
			    sizeof(vfbr.vfr_name)) >= sizeof(vfbr.vfr_name))
				return (-1);
			if (strlcpy(vfbr.vfr_value, vif->vif_name,
			    sizeof(vfbr.vfr_value)) >= sizeof(vfbr.vfr_value))
				return (-1);
			if (vsw->sw_flags & VMIFF_RDOMAIN)
				vfbr.vfr_id = vsw->sw_rdomain;
			else
				vfbr.vfr_id = getrtable();

			log_debug("%s: interface %s add %s", __func__,
			    vfbr.vfr_name, vfbr.vfr_value);

			proc_compose(ps, PROC_PRIV, IMSG_VMDOP_PRIV_IFCREATE,
			    &vfbr, sizeof(vfbr));
			proc_compose(ps, PROC_PRIV, IMSG_VMDOP_PRIV_IFRDOMAIN,
			    &vfbr, sizeof(vfbr));
			proc_compose(ps, PROC_PRIV, IMSG_VMDOP_PRIV_IFADD,
			    &vfbr, sizeof(vfbr));
		} else if (vif->vif_switch != NULL)
			log_warnx("switch %s not found", vif->vif_switch);

		/* First group is defined per-interface */
		if (vif->vif_group) {
			if (strlcpy(vfr.vfr_value, vif->vif_group,
			    sizeof(vfr.vfr_value)) >= sizeof(vfr.vfr_value))
				return (-1);

			log_debug("%s: interface %s group %s", __func__,
			    vfr.vfr_name, vfr.vfr_value);

			proc_compose(ps, PROC_PRIV, IMSG_VMDOP_PRIV_IFGROUP,
			    &vfr, sizeof(vfr));
		}

		/* The second group is defined per-switch */
		if (vsw != NULL && vsw->sw_group != NULL) {
			if (strlcpy(vfr.vfr_value, vsw->sw_group,
			    sizeof(vfr.vfr_value)) >= sizeof(vfr.vfr_value))
				return (-1);

			log_debug("%s: interface %s group %s switch %s",
			    __func__, vfr.vfr_name, vfr.vfr_value,
			    vsw->sw_name);

			proc_compose(ps, PROC_PRIV, IMSG_VMDOP_PRIV_IFGROUP,
			    &vfr, sizeof(vfr));
		}

		/* Set the new interface status to up or down */
		proc_compose(ps, PROC_PRIV, (vif->vif_flags & VMIFF_UP) ?
		    IMSG_VMDOP_PRIV_IFUP : IMSG_VMDOP_PRIV_IFDOWN,
		    &vfr, sizeof(vfr));

		if (vm->vm_params.vmc_ifflags[i] & VMIFF_LOCAL) {
			sin4 = (struct sockaddr_in *)&vfr.vfr_ifra.ifra_mask;
			sin4->sin_family = AF_INET;
			sin4->sin_len = sizeof(*sin4);
			sin4->sin_addr.s_addr = htonl(0xfffffffe);

			sin4 = (struct sockaddr_in *)&vfr.vfr_ifra.ifra_addr;
			sin4->sin_family = AF_INET;
			sin4->sin_len = sizeof(*sin4);
			if ((sin4->sin_addr.s_addr =
			    vm_priv_addr(&env->vmd_cfg.cfg_localprefix,
			    vm->vm_vmid, i, 0)) == 0)
				return (-1);

			log_debug("%s: interface %s address %s/31",
			    __func__, vfr.vfr_name,
			    inet_ntoa(sin4->sin_addr));

			proc_compose(ps, PROC_PRIV, IMSG_VMDOP_PRIV_IFADDR,
			    &vfr, sizeof(vfr));
		}
	}

	return (0);
}

int
vm_priv_brconfig(struct privsep *ps, struct vmd_switch *vsw)
{
	struct vmd_if		*vif;
	struct vmop_ifreq	 vfr;

	memset(&vfr, 0, sizeof(vfr));

	if (strlcpy(vfr.vfr_name, vsw->sw_ifname,
	    sizeof(vfr.vfr_name)) >= sizeof(vfr.vfr_name))
		return (-1);

	proc_compose(ps, PROC_PRIV, IMSG_VMDOP_PRIV_IFCREATE,
	    &vfr, sizeof(vfr));

	/* Use the configured rdomain or get it from the process */
	if (vsw->sw_flags & VMIFF_RDOMAIN)
		vfr.vfr_id = vsw->sw_rdomain;
	else
		vfr.vfr_id = getrtable();
	if (vfr.vfr_id != 0)
		log_debug("%s: interface %s rdomain %u", __func__,
		    vfr.vfr_name, vfr.vfr_id);

	proc_compose(ps, PROC_PRIV, IMSG_VMDOP_PRIV_IFRDOMAIN,
	    &vfr, sizeof(vfr));

	/* Description can be truncated */
	(void)snprintf(vfr.vfr_value, sizeof(vfr.vfr_value),
	    "switch%u-%s", vsw->sw_id, vsw->sw_name);

	log_debug("%s: interface %s description %s", __func__,
	    vfr.vfr_name, vfr.vfr_value);

	proc_compose(ps, PROC_PRIV, IMSG_VMDOP_PRIV_IFDESCR,
	    &vfr, sizeof(vfr));

	TAILQ_FOREACH(vif, &vsw->sw_ifs, vif_entry) {
		if (strlcpy(vfr.vfr_value, vif->vif_name,
		    sizeof(vfr.vfr_value)) >= sizeof(vfr.vfr_value))
			return (-1);

		log_debug("%s: interface %s add %s", __func__,
		    vfr.vfr_name, vfr.vfr_value);

		proc_compose(ps, PROC_PRIV, IMSG_VMDOP_PRIV_IFADD,
		    &vfr, sizeof(vfr));
	}

	/* Set the new interface status to up or down */
	proc_compose(ps, PROC_PRIV, (vsw->sw_flags & VMIFF_UP) ?
	    IMSG_VMDOP_PRIV_IFUP : IMSG_VMDOP_PRIV_IFDOWN,
	    &vfr, sizeof(vfr));

	vsw->sw_running = 1;
	return (0);
}

uint32_t
vm_priv_addr(struct address *h, uint32_t vmid, int idx, int isvm)
{
	in_addr_t		prefix, mask, addr;

	/*
	 * 1. Set the address prefix and mask, 100.64.0.0/10 by default.
	 */
	if (h->ss.ss_family != AF_INET ||
	    h->prefixlen < 0 || h->prefixlen > 32)
		fatal("local prefix");
	prefix = ss2sin(&h->ss)->sin_addr.s_addr;
	mask = prefixlen2mask(h->prefixlen);

	/* 2. Encode the VM ID as a per-VM subnet range N, 100.64.N.0/24. */
	addr = vmid << 8;

	/*
	 * 3. Assign a /31 subnet M per VM interface, 100.64.N.M/31.
	 * Each subnet contains exactly two IP addresses; skip the
	 * first subnet to avoid a gateway address ending with .0.
	 */
	addr |= (idx + 1) * 2;

	/* 4. Use the first address for the gateway, the second for the VM. */
	if (isvm)
		addr++;

	/* 5. Convert to network byte order and add the prefix. */
	addr = htonl(addr) | prefix;

	/*
	 * Validate the results:
	 * - the address should not exceed the prefix (eg. VM ID to high).
	 * - up to 126 interfaces can be encoded per VM.
	 */
	if (prefix != (addr & mask) || idx >= 0x7f) {
		log_warnx("%s: dhcp address range exceeded,"
		    " vm id %u interface %d", __func__, vmid, idx);
		return (0);
	}

	return (addr);
}
