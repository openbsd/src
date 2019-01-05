/*	$OpenBSD: dispatch.c,v 1.158 2019/01/05 21:40:44 krw Exp $	*/

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
#include <net/if_media.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <arpa/inet.h>

#include <errno.h>
#include <ifaddrs.h>
#include <imsg.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dhcp.h"
#include "dhcpd.h"
#include "log.h"
#include "privsep.h"


void bpffd_handler(struct interface_info *);
void dhcp_packet_dispatch(struct interface_info *, struct sockaddr_in *,
    struct ether_addr *);
void flush_unpriv_ibuf(void);
void sendhup(void);

/*
 * Loop waiting for packets, timeouts or routing messages.
 */
void
dispatch(struct interface_info *ifi, int routefd)
{
	struct pollfd		 fds[3];
	void			(*func)(struct interface_info *);
	time_t			 cur_time, howlong;
	int			 nfds, to_msec;

	while (quit == 0 || quit == SIGHUP) {
		if (quit == SIGHUP) {
			/* Ignore any future packets, messages or timeouts. */
			if (ifi->bpffd != -1) {
				close(ifi->bpffd);
				ifi->bpffd = -1;
			}
			if (routefd != -1) {
				close(routefd);
				routefd = -1;
			}
			if (ifi->timeout_func != NULL)
				cancel_timeout(ifi);
			sendhup();
			to_msec = 100;
		} else if (ifi->timeout_func != NULL) {
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
		fds[0].fd = ifi->bpffd;
		fds[1].fd = routefd;
		fds[2].fd = unpriv_ibuf->fd;
		fds[0].events = fds[1].events = fds[2].events = POLLIN;

		if (unpriv_ibuf->w.queued)
			fds[2].events |= POLLOUT;

		nfds = poll(fds, 3, to_msec);
		if (nfds == -1) {
			if (errno == EINTR)
				continue;
			log_warn("%s: poll(bpffd, routefd, unpriv_ibuf)",
			    log_procname);
			quit = INTERNALSIG;
			continue;
		}

		if ((fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
			log_debug("%s: bpffd: ERR|HUP|NVAL", log_procname);
			quit = INTERNALSIG;
			continue;
		}
		if ((fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
			log_debug("%s: routefd: ERR|HUP|NVAL", log_procname);
			quit = INTERNALSIG;
			continue;
		}
		if ((fds[2].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
			log_debug("%s: unpriv_ibuf: ERR|HUP|NVAL", log_procname);
			quit = INTERNALSIG;
			continue;
		}

		if (nfds == 0)
			continue;

		if ((fds[0].revents & POLLIN) != 0)
			bpffd_handler(ifi);
		if ((fds[1].revents & POLLIN) != 0)
			routefd_handler(ifi, routefd);
		if ((fds[2].revents & POLLOUT) != 0)
			flush_unpriv_ibuf();
		if ((fds[2].revents & POLLIN) != 0)
			quit = INTERNALSIG;
	}

	if (quit != INTERNALSIG && quit != SIGHUP)
		fatalx("%s", strsignal(quit));
}

void
bpffd_handler(struct interface_info *ifi)
{
	struct sockaddr_in	 from;
	struct ether_addr	 hfrom;
	unsigned char		*next, *lim;
	ssize_t			 n;

	n = read(ifi->bpffd, ifi->rbuf, ifi->rbuf_max);
	if (n == -1) {
		log_warn("%s: read(bpffd)", log_procname);
		ifi->errors++;
		if (ifi->errors > 20)
			fatalx("too many read(bpffd) failures");
		return;
	}
	ifi->errors = 0;

	lim = ifi->rbuf + n;
	for (next = ifi->rbuf; quit == 0 && n > 0; next += n) {
		n = receive_packet(next, lim, &from, &hfrom, &ifi->recv_packet);
		if (n > 0)
			dhcp_packet_dispatch(ifi, &from, &hfrom);
	}
}

void
dhcp_packet_dispatch(struct interface_info *ifi, struct sockaddr_in *from,
    struct ether_addr *hfrom)
{
	struct in_addr		 ifrom;
	struct dhcp_packet	*packet = &ifi->recv_packet;
	struct reject_elem	*ap;
	struct option_data	*options;
	char			*src;
	int			 i, rslt;

	ifrom.s_addr = from->sin_addr.s_addr;

	if (packet->hlen != ETHER_ADDR_LEN) {
		log_debug("%s: discarding packet with hlen == %u", log_procname,
		    packet->hlen);
		return;
	} else if (memcmp(&ifi->hw_address, packet->chaddr,
	    sizeof(ifi->hw_address))) {
		log_debug("%s: discarding packet with chaddr == %s",
		    log_procname,
		    ether_ntoa((struct ether_addr *)packet->chaddr));
		return;
	}

	if (ifi->xid != packet->xid) {
		log_debug("%s: discarding packet with XID != %u (%u)",
		    log_procname, ifi->xid, packet->xid);
		return;
	}

	TAILQ_FOREACH(ap, &config->reject_list, next)
	    if (ifrom.s_addr == ap->addr.s_addr) {
		    log_debug("%s: discarding packet from address on reject "
			"list (%s)", log_procname, inet_ntoa(ifrom));
		    return;
	    }

	options = unpack_options(&ifi->recv_packet);

	/*
	 * RFC 6842 says if the server sends a client identifier
	 * that doesn't match then the packet must be dropped.
	 */
	i = DHO_DHCP_CLIENT_IDENTIFIER;
	if ((options[i].len != 0) &&
	    ((options[i].len != config->send_options[i].len) ||
	    memcmp(options[i].data, config->send_options[i].data,
	    options[i].len) != 0)) {
		log_debug("%s: discarding packet with client-identifier %s'",
		    log_procname, pretty_print_option(i, &options[i], 0));
		return;
	}

	rslt = asprintf(&src, "%s (%s)", inet_ntoa(ifrom), ether_ntoa(hfrom));
	if (rslt == -1)
		fatal("src");

	i = DHO_DHCP_MESSAGE_TYPE;
	if (options[i].data != NULL) {
		/* Always try a DHCP packet, even if a bad option was seen. */
		switch (options[i].data[0]) {
		case DHCPOFFER:
			dhcpoffer(ifi, options, src);
			break;
		case DHCPNAK:
			dhcpnak(ifi, src);
			break;
		case DHCPACK:
			dhcpack(ifi, options, src);
			break;
		default:
			log_debug("%s: discarding DHCP packet of unknown type "
			    "(%d)", log_procname, options[i].data[0]);
			break;
		}
	} else if (packet->op == BOOTREPLY) {
		bootreply(ifi, options, src);
	} else {
		log_debug("%s: discarding packet which is neither DHCP nor "
		    "BOOTP", log_procname);
	}

	free(src);
}

/*
 * flush_unpriv_ibuf stuffs queued messages into the imsg socket.
 */
void
flush_unpriv_ibuf(void)
{
	while (unpriv_ibuf->w.queued) {
		if (msgbuf_write(&unpriv_ibuf->w) <= 0) {
			if (errno == EAGAIN)
				break;
			if (quit == 0)
				quit = INTERNALSIG;
			if (errno != EPIPE && errno != 0)
				log_warn("%s: msgbuf_write(unpriv_ibuf)",
				    log_procname);
			break;
		}
	}
}

void
set_timeout(struct interface_info *ifi, time_t secs,
    void (*where)(struct interface_info *))
{
	time(&ifi->timeout);
	if (secs > 0)
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
		log_warn("%s: imsg_compose(IMSG_HUP)", log_procname);
}
