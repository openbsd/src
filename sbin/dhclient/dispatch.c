/*	$OpenBSD: dispatch.c,v 1.130 2017/07/01 23:27:56 krw Exp $	*/

/*
 * Copyright 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 1995, 1996, 1997, 1998, 1999
 * The Internet Software Consortium.   All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of The Internet Software Consortium nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE INTERNET SOFTWARE CONSORTIUM OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This software has been written for the Internet Software Consortium
 * by Ted Lemon <mellon@fugue.com> in cooperation with Vixie
 * Enterprises.  To learn more about the Internet Software Consortium,
 * see ``http://www.vix.com/isc''.  To learn more about Vixie
 * Enterprises, see ``http://www.vix.com''.
 */

#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <errno.h>
#include <ifaddrs.h>
#include <imsg.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dhcp.h"
#include "dhcpd.h"
#include "log.h"
#include "privsep.h"


void packethandler(struct interface_info *ifi);

void
get_hw_address(struct interface_info *ifi)
{
	struct ifaddrs *ifap, *ifa;
	struct sockaddr_dl *sdl;
	struct if_data *ifdata;
	int found;

	if (getifaddrs(&ifap) != 0)
		fatalx("getifaddrs failed");

	found = 0;
	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if ((ifa->ifa_flags & IFF_LOOPBACK) ||
		    (ifa->ifa_flags & IFF_POINTOPOINT))
			continue;

		if (strcmp(ifi->name, ifa->ifa_name) != 0)
			continue;
		found = 1;

		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;

		sdl = (struct sockaddr_dl *)ifa->ifa_addr;
		if (sdl->sdl_type != IFT_ETHER ||
		    sdl->sdl_alen != ETHER_ADDR_LEN)
			continue;

		ifdata = ifa->ifa_data;
		ifi->rdomain = ifdata->ifi_rdomain;

		memcpy(ifi->hw_address.ether_addr_octet, LLADDR(sdl),
		    ETHER_ADDR_LEN);
		ifi->flags |= IFI_VALID_LLADDR;
	}

	if (!found)
		fatalx("%s: no such interface", ifi->name);

	freeifaddrs(ifap);
}

/*
 * Loop waiting for packets, timeouts or routing messages.
 */
void
dispatch(struct interface_info *ifi, int routefd)
{
	struct pollfd		 fds[3];
	void			(*func)(struct interface_info *);
	time_t			 cur_time, howlong;
	int			 count, to_msec;

	while (quit == 0 || quit == SIGHUP) {
		if (quit == SIGHUP) {
			log_warnx("%s; restarting", strsignal(quit));
			sendhup();
		}

		if (ifi->timeout_func) {
			time(&cur_time);
			if (ifi->timeout <= cur_time) {
				func = ifi->timeout_func;
				cancel_timeout(ifi);
				(*(func))(ifi);
				continue;
			}
			/*
			 * Figure timeout in milliseconds, and check for
			 * potential overflow, so we can cram into an
			 * int for poll, while not polling with a
			 * negative timeout and blocking indefinitely.
			 */
			howlong = ifi->timeout - cur_time;
			if (howlong > INT_MAX / 1000)
				howlong = INT_MAX / 1000;
			to_msec = howlong * 1000;
		} else
			to_msec = -1;

		/*
		 * Set up the descriptors to be polled.
		 *
		 *  fds[0] == bpf socket for incoming packets
		 *  fds[1] == routing socket for incoming RTM messages
		 *  fds[2] == imsg socket to privileged process
		 */
		fds[0].fd = ifi->bfdesc;
		fds[1].fd = routefd;
		fds[2].fd = unpriv_ibuf->fd;
		fds[0].events = fds[1].events = fds[2].events = POLLIN;

		if (unpriv_ibuf->w.queued)
			fds[2].events |= POLLOUT;

		count = poll(fds, 3, to_msec);
		if (count == -1) {
			if (errno == EAGAIN || errno == EINTR) {
				continue;
			} else {
				log_warn("poll");
				quit = INTERNALSIG;
				continue;
			}
		}

		if ((fds[0].revents & (POLLIN | POLLHUP)))
			packethandler(ifi);
		if ((fds[1].revents & (POLLIN | POLLHUP)))
			routehandler(ifi, routefd);
		if (fds[2].revents & POLLOUT)
			flush_unpriv_ibuf("dispatch");
		if ((fds[2].revents & (POLLIN | POLLHUP))) {
			/* Pipe to [priv] closed. Assume it emitted error. */
			quit = INTERNALSIG;
		}
	}

	if (quit != INTERNALSIG)
		fatalx("%s", strsignal(quit));
}

void
packethandler(struct interface_info *ifi)
{
	struct sockaddr_in from;
	struct ether_addr hfrom;
	struct in_addr ifrom;
	ssize_t result;

	if ((result = receive_packet(ifi, &from, &hfrom)) == -1) {
		ifi->errors++;
		if (ifi->errors > 20)
			fatalx("%s too many receive_packet failures",
			    ifi->name);
		else
			log_warn("%s receive_packet failed", ifi->name);
		return;
	}
	ifi->errors = 0;

	if (result == 0)
		return;

	ifrom.s_addr = from.sin_addr.s_addr;

	do_packet(ifi, from.sin_port, ifrom, &hfrom);
}

void
interface_link_forceup(char *name, int ioctlfd)
{
	struct ifreq ifr;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(ioctlfd, SIOCGIFFLAGS, (caddr_t)&ifr) == -1) {
		log_warn("SIOCGIFFLAGS");
		return;
	}

	/* Force it up if it isn't already. */
	if ((ifr.ifr_flags & IFF_UP) == 0) {
		ifr.ifr_flags |= IFF_UP;
		if (ioctl(ioctlfd, SIOCSIFFLAGS, (caddr_t)&ifr) == -1) {
			log_warn("SIOCSIFFLAGS");
			return;
		}
	}
}

int
interface_status(char *name)
{
	struct ifaddrs *ifap, *ifa;
	struct if_data *ifdata;

	if (getifaddrs(&ifap) != 0)
		fatalx("getifaddrs failed");

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if ((ifa->ifa_flags & IFF_LOOPBACK) ||
		    (ifa->ifa_flags & IFF_POINTOPOINT))
			continue;

		if (strcmp(name, ifa->ifa_name) != 0)
			continue;

		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;

		if ((ifa->ifa_flags & (IFF_UP|IFF_RUNNING)) !=
		    (IFF_UP|IFF_RUNNING))
			return 0;

		ifdata = ifa->ifa_data;

		return LINK_STATE_IS_UP(ifdata->ifi_link_state);
	}

	return 0;
}

void
set_timeout(struct interface_info *ifi, time_t secs,
    void (*where)(struct interface_info *))
{
	time(&ifi->timeout);
	ifi->timeout += secs;
	ifi->timeout_func = where;
}

void
cancel_timeout(struct interface_info *ifi)
{
	ifi->timeout = 0;
	ifi->timeout_func = NULL;
}

/*
 * Inform the [priv] process a HUP was received.
 */
void
sendhup(void)
{
	int rslt;

	rslt = imsg_compose(unpriv_ibuf, IMSG_HUP, 0, 0, -1, NULL, 0);
	if (rslt == -1)
		log_warn("sendhup: imsg_compose");

	flush_unpriv_ibuf("sendhup");
}
