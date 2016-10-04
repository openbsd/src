/*	$OpenBSD: priv.c,v 1.1 2016/10/04 17:17:30 reyk Exp $	*/

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

#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include "proc.h"
#include "vmd.h"

int	 priv_dispatch_parent(int, struct privsep_proc *, struct imsg *);
void	 priv_run(struct privsep *, struct privsep_proc *, void *);

int	 priv_getiftype(char *, char *, unsigned int *);
int	 priv_findname(const char *, const char **);

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
	char			 type[IF_NAMESIZE];
	unsigned int		 unit;

	switch (imsg->hdr.type) {
	case IMSG_VMDOP_PRIV_IFDESCR:
		IMSG_SIZE_CHECK(imsg, &vfr);
		memcpy(&vfr, imsg->data, sizeof(vfr));

		/* We should not get malicious requests from the parent */
		if (priv_getiftype(vfr.vfr_name, type, &unit) == -1 ||
		    priv_findname(type, desct) == -1)
			fatalx("%s: rejected priv operation on interface: %s",
			    __func__, vfr.vfr_name);

		strlcpy(ifr.ifr_name, vfr.vfr_name, sizeof(ifr.ifr_name));
		ifr.ifr_data = (caddr_t)vfr.vfr_value;
		if (ioctl(env->vmd_fd, SIOCSIFDESCR, &ifr) < 0)
			log_warn("SIOCSIFDESCR");
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

/*
 * Called from the process peer
 */

int
vm_priv_ifconfig(struct privsep *ps, struct vmd_vm *vm)
{
	struct vm_create_params	*vcp = &vm->vm_params;
	unsigned int		 i;
	struct vmop_ifreq	 vfr;

	for (i = 0; i < VMM_MAX_NICS_PER_VM; i++) {
		if (vm->vm_ifnames[i] == NULL)
			break;

		if (strlcpy(vfr.vfr_name, vm->vm_ifnames[i],
		    sizeof(vfr.vfr_name)) >= sizeof(vfr.vfr_name))
			return (-1);

		/* Description can be truncated */
		(void)snprintf(vfr.vfr_value, sizeof(vfr.vfr_value),
		    "vm%u-if%u-%s", vm->vm_vmid, i, vcp->vcp_name);

		proc_compose(ps, PROC_PRIV, IMSG_VMDOP_PRIV_IFDESCR,
		    &vfr, sizeof(vfr));

		/* XXX Add interface to bridge/switch */
	}

	return (0);
}
