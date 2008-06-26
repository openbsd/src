/*	$OpenBSD: netio.c,v 1.5 2008/06/26 05:42:13 ray Exp $	*/
/*	$NetBSD: netio.c,v 1.6 2000/05/26 20:16:46 ragge Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
 *	This product includes software developed by Gordon W. Ross
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
 * use by the stand-alone I/O library NFS code.	 This interface
 * does not support any "block" access, and exists only for the
 * purpose of initializing the network interface, getting boot
 * parameters, and performing the NFS mount.
 *
 * At open time, this does:
 *
 * find interface      - netif_open()
 * RARP for IP address - rarp_getipaddress()
 * RPC/bootparams      - callrpc(d, RPC_BOOTPARAMS, ...)
 * RPC/mountd	       - nfs_mount(sock, ip, path)
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

#include "lib/libsa/stand.h"
#include "lib/libsa/net.h"
#include "lib/libsa/netif.h"
#include "lib/libsa/bootparam.h"
#include "lib/libsa/nfs.h"
#include "lib/libsa/bootp.h"

#include "vaxstand.h"

static struct iodesc desc;
static int inited = 0;

struct iodesc *
socktodesc(sock)
{
	return &desc;
}

int
net_devinit(struct open_file *f, struct netif_driver *drv, u_char *eaddr) {
	static struct netif best_if;
	struct iodesc *s;
	int r;

	if (inited)
		return 0;
	/* find a free socket */
	s = &desc;

	bzero(s, sizeof(*s));
	best_if.nif_driver = drv;
	s->io_netif = &best_if;
	bcopy(eaddr, s->myea, 6);

	/*
	 * Get info for NFS boot: our IP address, our hostname,
	 * server IP address, and our root path on the server.
	 * There are two ways to do this:  The old, Sun way,
	 * and the more modern, BOOTP way. (RFC951, RFC1048)
	 */

#ifdef SUPPORT_BOOTP

	/* Get boot info using BOOTP way. (RFC951, RFC1048) */
	printf("Trying BOOTP\n");
	bootp(0);

	if (myip.s_addr) {
		printf("Using IP address: %s\n", inet_ntoa(myip));

		printf("myip: %s (%s)\n", hostname, inet_ntoa(myip));
	} else

#endif /* SUPPORT_BOOTP */
	{
#ifdef	SUPPORT_BOOTPARAMS
		/* Get boot info using RARP and Sun bootparams. */

		printf("Trying BOOTPARAMS\n");
		/* Get our IP address.	(rarp.c) */
		if (rarp_getipaddress(0) == -1)
			return (errno);

		printf("boot: client IP address: %s\n", inet_ntoa(myip));

		/* Get our hostname, server IP address. */
		if (bp_whoami(0))
			return (errno);

		printf("boot: client name: %s\n", hostname);

		/* Get the root pathname. */
		if (bp_getfile(0, "root", &rootip, rootpath))
			return (errno);
#endif
	}
	printf("root addr=%s path=%s\n", inet_ntoa(rootip), rootpath);
	f->f_devdata = s;

	/* Get the NFS file handle (mount). */
	r = nfs_mount(0, rootip, rootpath);
	if (r)
		return r;

	inited = 1;
	return 0;
}

ssize_t
netif_put(struct iodesc *desc, void *pkt, size_t len)
{
	return (*desc->io_netif->nif_driver->netif_put)(desc, pkt, len);
}

ssize_t
netif_get(struct iodesc *desc, void *pkt, size_t len, time_t timo)
{
	return (*desc->io_netif->nif_driver->netif_get)(desc, pkt, len, timo);
}
