/*	$OpenBSD: dev_net.c,v 1.7 1999/01/11 05:11:41 millert Exp $ */

/*
 * Copyright (c) 1995 Gordon W. Ross
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 4. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Gordon W. Ross
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This module implements a "raw device" interface suitable for
 * use by the stand-alone I/O library NFS code.  This interface
 * does not support any "block" access, and exists only for the
 * purpose of initializing the network interface, getting boot
 * parameters, and performing the NFS mount.
 *
 * At open time, this does:
 *
 * find interface      - netif_open()
 * RARP for IP address - rarp_getipaddress()
 * RPC/bootparams      - callrpc(d, RPC_BOOTPARAMS, ...)
 * RPC/mountd          - nfs_mount(sock, ip, path)
 *
 * the root file handle from mountd is saved in a global
 * for use by the NFS open code (NFS/lookup).
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/in_systm.h>

#include <machine/prom.h>

#include "stand.h"
#include "libsa.h"
#include "net.h"
#include "netif.h"
#include "config.h"
#include "bootparam.h"

extern int nfs_root_node[];	/* XXX - get from nfs_mount() */

struct in_addr myip, rootip, gateip, mask;
char rootpath[FNAME_SIZE];

int netdev_sock = -1;
static int open_count;

/*
 * Called by devopen after it sets f->f_dev to our devsw entry.
 * This opens the low-level device and sets f->f_devdata.
 */
int
net_open(f, devname)
	struct open_file *f;
	char *devname;		/* Device part of file name (or NULL). */
{
	int error = 0;

	/* On first open, do netif open, mount, etc. */
	if (open_count == 0) {
		/* Find network interface. */
		if ((netdev_sock = netif_open(devname)) < 0)
			return (error=ENXIO);
		if ((error = net_mountroot(f, devname)) != 0)
			return (error);
	}
	open_count++;
	f->f_devdata = nfs_root_node;
	return (error);
}

int
net_close(f)
	struct open_file *f;
{
	/* On last close, do netif close, etc. */
	if (open_count > 0)
		if (--open_count == 0)
			netif_close(netdev_sock);
	f->f_devdata = NULL;
}

int
net_ioctl()
{
	return EIO;
}

int
net_strategy()
{
	return EIO;
}

int
net_mountroot(f, devname)
	struct open_file *f;
	char *devname;		/* Device part of file name (or NULL). */
{
	int error;

#ifdef DEBUG
	printf("net_mountroot: %s\n", devname);
#endif

	/*
	 * Get info for NFS boot: our IP address, our hostname,
	 * server IP address, and our root path on the server.
	 * There are two ways to do this:  The old, Sun way,
	 * and the more modern, BOOTP way. (RFC951, RFC1048)
	 */

#ifdef	SUN_BOOTPARAMS
	/* Get boot info using RARP and Sun bootparams. */

	/* Get our IP address.  (rarp.c) */
	if (rarp_getipaddress(netdev_sock) == -1)
		return (EIO);
	printf("boot: client IP address: %s\n", intoa(myip.s_addr));

	/* Get our hostname, server IP address. */
	if (bp_whoami(netdev_sock))
		return (EIO);
	printf("boot: client name: %s\n", hostname);

	/* Get the root pathname. */
	if (bp_getfile(netdev_sock, "root", &rootip, rootpath))
		return (EIO);

#else

	/* Get boot info using BOOTP way. (RFC951, RFC1048) */
	bootp(netdev_sock);

	printf("Using IP address: %s\n", intoa(myip.s_addr));

	printf("myip: %s (%s)", hostname, intoa(myip));
	if (gateip)
		printf(", gateip: %s", intoa(gateip));
	if (mask)
		printf(", mask: %s", intoa(mask));
	printf("\n");

#endif

	printf("root addr=%s path=%s\n", intoa(rootip.s_addr), rootpath);

	/* Get the NFS file handle (mount). */
	error = nfs_mount(netdev_sock, rootip, rootpath);

	return (error);
}

/*
 * machdep_common_ether: get ethernet address
 */
void
machdep_common_ether(ether)
	u_char *ether;
{
	u_char *ea;

	if (bugargs.cputyp == CPU_147) {
		ea = (u_char *) ETHER_ADDR_147;

		if ((*(int *) ea & 0x2fffff00) == 0x2fffff00)
			panic("ERROR: ethernet address not set!");
		ether[0] = 0x08;
		ether[1] = 0x00;
		ether[2] = 0x3e;
		ether[3] = ea[0];
		ether[4] = ea[1];
		ether[5] = ea[2];
	} else {
		ea = (u_char *) ETHER_ADDR_16X;

		if (ea[0] + ea[1] + ea[2] + ea[3] + ea[4] + ea[5] == 0)
			panic("ERROR: ethernet address not set!");
		ether[0] = ea[0];
		ether[1] = ea[1];
		ether[2] = ea[2];
		ether[3] = ea[3];
		ether[4] = ea[4];
		ether[5] = ea[5];
	}
}
