/*	$OpenBSD: netif_sun.c,v 1.5 2014/11/19 19:41:25 miod Exp $	*/
/*	$NetBSD: netif_sun.c,v 1.2 1995/09/18 21:31:48 pk Exp $	*/

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
 * The Sun PROM has a fairly general set of network drivers,
 * so it is easiest to just replace the netif module with
 * this adaptation to the PROM network interface.
 */

#include <sys/param.h>
#include <sys/socket.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <lib/libsa/stand.h>
#include <lib/libsa/net.h>
#include <lib/libsa/netif.h>

#include <sparc/stand/common/promdev.h>

static struct netif netif_prom;

#ifdef NETIF_DEBUG
int netif_debug;
#endif

struct iodesc sockets[SOPEN_MAX];

struct iodesc *
socktodesc(sock)
	int sock;
{
	if (sock != 0) {
		return(NULL);
	}
	return (sockets);
}

int
netif_open(machdep_hint)
	void *machdep_hint;
{
	struct promdata *pd = machdep_hint;
	struct iodesc *io;

	/* find a free socket */
	io = sockets;
	if (io->io_netif) {
#ifdef	DEBUG
		printf("netif_open: device busy\n");
#endif
		errno = ENFILE;
		return (-1);
	}
	bzero(io, sizeof(*io));

	netif_prom.nif_devdata = pd;
	io->io_netif = &netif_prom;

	/* Put our ethernet address in io->myea */
	prom_getether(pd->fd, io->myea);

	return(0);
}

int
netif_close(fd)
	int fd;
{
	struct iodesc *io;
	struct netif *ni;

	if (fd != 0) {
		errno = EBADF;
		return(-1);
	}

	io = &sockets[fd];
	ni = io->io_netif;
	if (ni != NULL) {
		ni->nif_devdata = NULL;
		io->io_netif = NULL;
	}
	return(0);
}

/*
 * Send a packet.  The ether header is already there.
 * Return the length sent (or -1 on error).
 */
ssize_t
netif_put(desc, pkt, len)
	struct iodesc *desc;
	void *pkt;
	size_t len;
{
	struct promdata *pd;
	ssize_t rv;
	size_t sendlen;

	pd = (struct promdata *)desc->io_netif->nif_devdata;

#ifdef NETIF_DEBUG
	if (netif_debug) {
		struct ether_header *eh;

		printf("netif_put: desc=0x%x pkt=0x%x len=%d\n",
			   desc, pkt, len);
		eh = pkt;
		printf("dst: %s ", ether_sprintf(eh->ether_dhost));
		printf("src: %s ", ether_sprintf(eh->ether_shost));
		printf("type: 0x%x\n", eh->ether_type & 0xFFFF);
	}
#endif

	sendlen = len;
	if (sendlen < 60) {
		sendlen = 60;
#ifdef NETIF_DEBUG
		printf("netif_put: length padded to %d\n", sendlen);
#endif
	}

	rv = (*pd->xmit)(pd, pkt, sendlen);

#ifdef NETIF_DEBUG
	if (netif_debug)
		printf("netif_put: xmit returned %d\n", rv);
#endif

	return rv;
}

/*
 * Receive a packet, including the ether header.
 * Return the total length received (or -1 on error).
 */
ssize_t
netif_get(desc, pkt, maxlen, timo)
	struct iodesc *desc;
	void *pkt;
	size_t maxlen;
	time_t timo;
{
	struct promdata *pd;
	int tick0, tmo_ticks;
	ssize_t len;

	pd = (struct promdata *)desc->io_netif->nif_devdata;

#ifdef NETIF_DEBUG
	if (netif_debug)
		printf("netif_get: pkt=0x%x, maxlen=%d, tmo=%d\n",
			   pkt, maxlen, timo);
#endif

	tmo_ticks = timo * hz;
	tick0 = getticks();

	do {
		len = (*pd->recv)(pd, pkt, maxlen);
	} while ((len == 0) && ((getticks() - tick0) < tmo_ticks));

#ifdef NETIF_DEBUG
	if (netif_debug)
		printf("netif_get: received len=%d\n", len);
#endif

	if (len < 12)
		return -1;

#ifdef NETIF_DEBUG
	if (netif_debug) {
		struct ether_header *eh = pkt;

		printf("dst: %s ", ether_sprintf(eh->ether_dhost));
		printf("src: %s ", ether_sprintf(eh->ether_shost));
		printf("type: 0x%x\n", eh->ether_type & 0xFFFF);
	}
#endif

	return len;
}
