/*	$OpenBSD: netio.c,v 1.3 2006/08/17 06:31:10 miod Exp $	*/
/*	$NetBSD: netio.c,v 1.5 1997/01/30 10:32:56 thorpej Exp $	*/

/*
 * Copyright (c) 1995, 1996 Jason R. Thorpe
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

#include <lib/libsa/stand.h>

#include "samachdep.h"

#include <lib/libsa/net.h>
#include <lib/libsa/netif.h>
#include <lib/libsa/bootparam.h>
#include <lib/libsa/nfs.h>

extern int nfs_root_node[];	/* XXX - get from nfs_mount() */

struct	in_addr myip, rootip, gateip;
n_long	netmask;
char rootpath[FNAME_SIZE];

int netdev_sock = -1;
static int open_count;

int netio_ask = 0;		/* default to bootparam, can override */

static	char input_line[100];

/* Why be any different? */
#define SUN_BOOTPARAMS

int	netclose(struct open_file *);
int	netmountroot(struct open_file *, char *);
int	netopen(struct open_file *, char *);
int	netstrategy(void *, int, daddr_t, size_t, void *, size_t *);

/*
 * Called by devopen after it sets f->f_dev to our devsw entry.
 * This opens the low-level device and sets f->f_devdata.
 */
int
netopen(struct open_file *f, char *devname)
{
	int error = 0;

	/* On first open, do netif open, mount, etc. */
	if (open_count == 0) {
		/* Find network interface. */
		if ((netdev_sock = netif_open(devname)) < 0)
			return (error=ENXIO);
		if ((error = netmountroot(f, devname)) != 0)
			return (error);
	}
	open_count++;
	f->f_devdata = nfs_root_node;
	return (error);
}

int
netclose(struct open_file *f)
{
	/* On last close, do netif close, etc. */
	if (open_count > 0)
		if (--open_count == 0)
			netif_close(netdev_sock);
	f->f_devdata = NULL;
	return (0);
}

int
netstrategy(void *devdata, int func, daddr_t dblk, size_t size, void *v_buf,
    size_t *rsize)
{

	*rsize = size;
	return EIO;
}

int
netmountroot(struct open_file *f, char *devname)
{
	int error;
	struct iodesc *d;

#ifdef DEBUG
	printf("netmountroot: %s\n", devname);
#endif

	if (netio_ask) {
 get_my_ip:
		printf("My IP address? ");
		bzero(input_line, sizeof(input_line));
		gets(input_line);
		if ((myip.s_addr = inet_addr(input_line)) ==
		    htonl(INADDR_NONE)) {
			printf("invalid IP address: %s\n", input_line);
			goto get_my_ip;
		}

 get_my_netmask:
		printf("My netmask? ");
		bzero(input_line, sizeof(input_line));
		gets(input_line);
		if ((netmask = inet_addr(input_line)) ==
		    htonl(INADDR_NONE)) {
			printf("invalid netmask: %s\n", input_line);
			goto get_my_netmask;
		}

 get_my_gateway:
		printf("My gateway? ");
		bzero(input_line, sizeof(input_line));
		gets(input_line);
		if ((gateip.s_addr = inet_addr(input_line)) ==
		    htonl(INADDR_NONE)) {
			printf("invalid IP address: %s\n", input_line);
			goto get_my_gateway;
		}

 get_server_ip:
		printf("Server IP address? ");
		bzero(input_line, sizeof(input_line));
		gets(input_line);
		if ((rootip.s_addr = inet_addr(input_line)) ==
		    htonl(INADDR_NONE)) {
			printf("invalid IP address: %s\n", input_line);
			goto get_server_ip;
		}

 get_server_path:
		printf("Server path? ");
		bzero(rootpath, sizeof(rootpath));
		gets(rootpath);
		if (rootpath[0] == '\0' || rootpath[0] == '\n')
			goto get_server_path;

		if ((d = socktodesc(netdev_sock)) == NULL)
			return (EMFILE);

		d->myip = myip;

		goto do_nfs_mount;
	}

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
		return (errno);

	printf("boot: client IP address: %s\n", inet_ntoa(myip));

	/* Get our hostname, server IP address. */
	if (bp_whoami(netdev_sock))
		return (errno);

	printf("boot: client name: %s\n", hostname);

	/* Get the root pathname. */
	if (bp_getfile(netdev_sock, "root", &rootip, rootpath))
		return (errno);

#else

	/* Get boot info using BOOTP way. (RFC951, RFC1048) */
	bootp(netdev_sock);

	printf("Using IP address: %s\n", inet_ntoa(myip));

	printf("myip: %s (%s)", hostname, inet_ntoa(myip));
	if (gateip)
		printf(", gateip: %s", inet_ntoa(gateip));
	if (mask)
		printf(", mask: %s", intoa(netmask));
	printf("\n");

#endif /* SUN_BOOTPARAMS */

	printf("root addr=%s path=%s\n", inet_ntoa(rootip), rootpath);

 do_nfs_mount:
	/* Get the NFS file handle (mount). */
	error = nfs_mount(netdev_sock, rootip, rootpath);

	return (error);
}
