/* $OpenBSD: udp.c,v 1.71 2004/05/23 18:17:56 hshoexer Exp $	 */
/* $EOM: udp.c,v 1.57 2001/01/26 10:09:57 niklas Exp $	 */

/*
 * Copyright (c) 1998, 1999, 2001 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 2000 Angelos D. Keromytis.  All rights reserved.
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
 * This code was written under funding by Ericsson Radio Systems.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#ifndef linux
#include <sys/sockio.h>
#endif
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sysdep.h"

#include "conf.h"
#include "if.h"
#include "isakmp.h"
#include "log.h"
#include "message.h"
#include "monitor.h"
#include "sysdep.h"
#include "transport.h"
#include "udp.h"
#include "util.h"

#define UDP_SIZE 65536

/* If a system doesn't have SO_REUSEPORT, SO_REUSEADDR will have to do. */
#ifndef SO_REUSEPORT
#define SO_REUSEPORT SO_REUSEADDR
#endif

struct udp_transport {
	struct transport transport;
	struct sockaddr *src, *dst;
	int             s;
	LIST_ENTRY(udp_transport) link;
};

static struct transport *udp_clone(struct udp_transport *, struct sockaddr *);
static struct transport *udp_create(char *);
static void     udp_reinit(void);
static void     udp_remove(struct transport *);
static void     udp_report(struct transport *);
static int      udp_fd_set(struct transport *, fd_set *, int);
static int      udp_fd_isset(struct transport *, fd_set *);
static void     udp_handle_message(struct transport *);
static struct transport *udp_make(struct sockaddr *);
static int      udp_send_message(struct message *);
static void     udp_get_dst(struct transport *, struct sockaddr **);
static void     udp_get_src(struct transport *, struct sockaddr **);
static char    *udp_decode_ids(struct transport *);
#if 0
static in_port_t udp_decode_port(char *);
#endif

static struct transport_vtbl udp_transport_vtbl = {
	{0}, "udp",
	udp_create,
	udp_reinit,
	udp_remove,
	udp_report,
	udp_fd_set,
	udp_fd_isset,
	udp_handle_message,
	udp_send_message,
	udp_get_dst,
	udp_get_src,
	udp_decode_ids
};

/* A list of UDP transports we listen for messages on.  */
static
LIST_HEAD(udp_listen_list, udp_transport) udp_listen_list;

static struct transport *default_transport, *default_transport6;
char		*udp_default_port = 0;
char		*udp_bind_port = 0;
int		bind_family = 0;

/* Find an UDP transport listening on ADDR:PORT.  */
static struct udp_transport *
udp_listen_lookup(struct sockaddr *addr)
{
	struct udp_transport *u;

	for (u = LIST_FIRST(&udp_listen_list); u; u = LIST_NEXT(u, link))
		if (sysdep_sa_len(u->src) == sysdep_sa_len(addr) &&
		    memcmp(u->src, addr, sysdep_sa_len(addr)) == 0)
			return u;
	return 0;
}

/* Create a UDP transport structure bound to LADDR just for listening.  */
static struct transport *
udp_make(struct sockaddr *laddr)
{
	struct udp_transport *t = 0;
	int		s, on, wildcardaddress = 0;

	t = calloc(1, sizeof *t);
	if (!t) {
		log_print("udp_make: malloc (%lu) failed",
		    (unsigned long)sizeof *t);
		return 0;
	}
	s = socket(laddr->sa_family, SOCK_DGRAM, IPPROTO_UDP);
	if (s == -1) {
		log_error("udp_make: socket (%d, %d, %d)", laddr->sa_family,
		    SOCK_DGRAM, IPPROTO_UDP);
		goto err;
	}
	/* Make sure we don't get our traffic encrypted.  */
	if (sysdep_cleartext(s, laddr->sa_family) == -1)
		goto err;

	/* Wildcard address ?  */
	switch (laddr->sa_family) {
	case AF_INET:
		if (((struct sockaddr_in *)laddr)->sin_addr.s_addr ==
		    INADDR_ANY)
			wildcardaddress = 1;
		break;
	case AF_INET6:
		if (IN6_IS_ADDR_UNSPECIFIED(&((struct sockaddr_in6 *)laddr)->sin6_addr))
			wildcardaddress = 1;
		break;
	}

	/*
	 * In order to have several bound specific address-port combinations
	 * with the same port SO_REUSEADDR is needed.  If this is a wildcard
	 * socket and we are not listening there, but only sending from it
	 * make sure it is entirely reuseable with SO_REUSEPORT.
	 */
	on = 1;
	if (setsockopt(s, SOL_SOCKET,
	    wildcardaddress ? SO_REUSEPORT : SO_REUSEADDR,
	    (void *)&on, sizeof on) == -1) {
		log_error("udp_make: setsockopt (%d, %d, %d, %p, %lu)", s,
		    SOL_SOCKET, wildcardaddress ? SO_REUSEPORT : SO_REUSEADDR,
		    &on, (unsigned long)sizeof on);
		goto err;
	}
	t->transport.vtbl = &udp_transport_vtbl;
	t->src = laddr;
	if (monitor_bind(s, t->src, sysdep_sa_len(t->src))) {
		char	*tstr;

		if (sockaddr2text(t->src, &tstr, 0))
			log_error("udp_make: bind (%d, %p, %lu)", s, &t->src,
			    (unsigned long)sizeof t->src);
		else {
			log_error("udp_make: bind (%d, %s, %lu)", s, tstr,
			    (unsigned long)sizeof t->src);
			free(tstr);
		}
		goto err;
	}
	t->s = s;
	transport_add(&t->transport);
	transport_reference(&t->transport);
	t->transport.flags |= TRANSPORT_LISTEN;
	return &t->transport;

err:
	if (s >= 0)
		close(s);
	if (t) {
		/* Already closed.  */
		t->s = -1;
		udp_remove(&t->transport);
	}
	return 0;
}

/* Clone a listen transport U, record a destination RADDR for outbound use.  */
static struct transport *
udp_clone(struct udp_transport *u, struct sockaddr *raddr)
{
	struct udp_transport *u2;
	struct transport *t;

	t = malloc(sizeof *u);
	if (!t) {
		log_error("udp_clone: malloc (%lu) failed",
		    (unsigned long)sizeof *u);
		return 0;
	}
	u2 = (struct udp_transport *)t;

	memcpy(u2, u, sizeof *u);

	u2->src = malloc(sysdep_sa_len(u->src));
	if (!u2->src) {
		log_error("udp_clone: malloc (%d) failed",
		    sysdep_sa_len(u->src));
		free(t);
		return 0;
	}
	memcpy(u2->src, u->src, sysdep_sa_len(u->src));

	u2->dst = malloc(sysdep_sa_len(raddr));
	if (!u2->dst) {
		log_error("udp_clone: malloc (%d) failed",
		    sysdep_sa_len(raddr));
		free(u2->src);
		free(t);
		return 0;
	}
	memcpy(u2->dst, raddr, sysdep_sa_len(raddr));

	t->flags &= ~TRANSPORT_LISTEN;
	transport_add(t);
	return t;
}

/*
 * Initialize an object of the UDP transport class.  Fill in the local
 * IP address and port information and create a server socket bound to
 * that specific port.  Add the polymorphic transport structure to the
 * system-wide pools of known ISAKMP transports.
 */
static struct transport *
udp_bind(const struct sockaddr *addr)
{
	struct sockaddr *src = malloc(sysdep_sa_len((struct sockaddr *)addr));

	if (!src)
		return 0;

	memcpy(src, addr, sysdep_sa_len((struct sockaddr *)addr));
	return udp_make(src);
}

/*
 * When looking at a specific network interface address, if it's an INET one,
 * create an UDP server socket bound to it.
 * Return 0 if successful, -1 otherwise.
 */
static int
udp_bind_if(char *ifname, struct sockaddr *if_addr, void *arg)
{
	char	*port = (char *) arg, *addr_str, *ep;
	struct sockaddr_storage saddr_st;
	struct sockaddr *saddr = (struct sockaddr *) & saddr_st;
	struct conf_list *listen_on;
	struct udp_transport *u;
	struct conf_list_node *address;
	struct sockaddr *addr;
	struct transport *t;
	struct ifreq    flags_ifr;
	int		s, error;
	long		lport;

	/*
	 * Well, UDP is an internet protocol after all so drop other ifreqs.
	*/
	if ((if_addr->sa_family != AF_INET ||
	    sysdep_sa_len(if_addr) != sizeof(struct sockaddr_in)) &&
	    (if_addr->sa_family != AF_INET6 ||
	    sysdep_sa_len(if_addr) != sizeof(struct sockaddr_in6)))
		return 0;

	/*
	 * Only create sockets for families we should listen to.
	 */
	if (bind_family) {
		switch (if_addr->sa_family) {
		case AF_INET:
			if ((bind_family & BIND_FAMILY_INET4) == 0)
				return 0;
			break;
		case AF_INET6:
			if ((bind_family & BIND_FAMILY_INET6) == 0)
				return 0;
			break;
		default:
			return 0;
		}
	}

	/*
	 * These special addresses are not useable as they have special meaning
	 * in the IP stack.
	 */
	if (if_addr->sa_family == AF_INET &&
	    (((struct sockaddr_in *)if_addr)->sin_addr.s_addr == INADDR_ANY ||
	    (((struct sockaddr_in *)if_addr)->sin_addr.s_addr == INADDR_NONE)))
		return 0;

	/*
	 * Go through the list of transports and see if we already have this
	 * address bound. If so, unmark the transport and skip it; this allows
	 * us to call this function when we suspect a new address has appeared.
	 */
	if (sysdep_sa_len(if_addr) > sizeof saddr_st)
		return 0;
	memcpy(saddr, if_addr, sysdep_sa_len(if_addr));
	switch (saddr->sa_family) { /* Add the port number to the sockaddr. */
	case AF_INET:
		((struct sockaddr_in *)saddr)->sin_port =
		    htons(strtol(port, &ep, 10));
		break;
	case AF_INET6:
		((struct sockaddr_in6 *)saddr)->sin6_port =
		    htons(strtol(port, &ep, 10));
		break;
	}

	if ((u = udp_listen_lookup(saddr)) != 0) {
		u->transport.flags &= ~TRANSPORT_MARK;
		return 0;
	}
	/* Don't bother with interfaces that are down.  */
	s = socket(if_addr->sa_family, SOCK_DGRAM, 0);
	if (s == -1) {
		log_error("udp_bind_if: socket (%d, SOCK_DGRAM, 0) failed",
		    if_addr->sa_family);
		return -1;
	}
	strlcpy(flags_ifr.ifr_name, ifname, sizeof flags_ifr.ifr_name);
	if (ioctl(s, SIOCGIFFLAGS, (caddr_t) & flags_ifr) == -1) {
		log_error("udp_bind_if: ioctl (%d, SIOCGIFFLAGS, ...) failed",
		    s);
		return -1;
	}
	close(s);
	if (!(flags_ifr.ifr_flags & IFF_UP))
		return 0;

	/*
	 * Set port.
	 * XXX Use getservbyname too.
	 */
	lport = strtol(port, &ep, 10);
	if (*ep != '\0' || lport < (long) 0 || lport > (long) USHRT_MAX) {
		log_print("udp_bind_if: "
		    "port string \"%s\" not convertible to in_port_t", port);
		return -1;
	}
	switch (if_addr->sa_family) {
	case AF_INET:
		((struct sockaddr_in *)if_addr)->sin_port = htons(lport);
		break;
	case AF_INET6:
		((struct sockaddr_in6 *)if_addr)->sin6_port = htons(lport);
		break;
	default:
		log_print("udp_bind_if: unsupported protocol family %d",
		    if_addr->sa_family);
		break;
	}

	/*
	 * If we are explicit about what addresses we can listen to, be sure
	 * to respect that option.
	 * This is quite wasteful redoing the list-run for every interface,
	 * but who cares?  This is not an operation that needs to be fast.
	 */
	listen_on = conf_get_list("General", "Listen-on");
	if (listen_on) {
		for (address = TAILQ_FIRST(&listen_on->fields); address;
		    address = TAILQ_NEXT(address, link)) {
			if (text2sockaddr(address->field, port, &addr)) {
				log_print("udp_bind_if: invalid address %s "
				    "in \"Listen-on\"", address->field);
				continue;
			}
			/* If found, take the easy way out. */
			if (memcmp(addr, if_addr, sysdep_sa_len(addr)) == 0) {
				free(addr);
				break;
			}
			free(addr);
		}
		conf_free_list(listen_on);

		/*
		 * If address is zero then we did not find the address among
		 * the ones we should listen to.
		 * XXX We do not discover if we do not find our listen
		 * addresses...  Maybe this should be the other way round.
		 */
		if (!address)
			return 0;
	}
	t = udp_bind(if_addr);
	if (!t) {
		error = sockaddr2text(if_addr, &addr_str, 0);
		log_print("udp_bind_if: failed to create a socket on %s:%s",
		    error ? "unknown" : addr_str, port);
		if (!error)
			free(addr_str);
		return -1;
	}
	LIST_INSERT_HEAD(&udp_listen_list, (struct udp_transport *)t, link);
	return 0;
}

/*
 * NAME is a section name found in the config database.  Setup and return
 * a transport useable to talk to the peer specified by that name.
 */
static struct transport *
udp_create(char *name)
{
	struct udp_transport *u;
	struct transport *rv;
	struct sockaddr *dst, *addr;
	char	*addr_str, *port_str;

	port_str = conf_get_str(name, "Port");
	if (!port_str)
		port_str = udp_default_port;
	if (!port_str)
		port_str = "500";

	addr_str = conf_get_str(name, "Address");
	if (!addr_str) {
		log_print("udp_create: no address configured for \"%s\"", name);
		return 0;
	}
	if (text2sockaddr(addr_str, port_str, &dst)) {
		log_print("udp_create: address \"%s\" not understood",
		    addr_str);
		return 0;
	}
	addr_str = conf_get_str(name, "Local-address");
	if (!addr_str)
		addr_str = conf_get_str("General", "Listen-on");
	if (!addr_str) {
		if ((dst->sa_family == AF_INET && !default_transport) ||
		    (dst->sa_family == AF_INET6 && !default_transport6)) {
			log_print("udp_create: no default transport");
			rv = 0;
			goto ret;
		} else {
			/* XXX Ugly! */
			rv = udp_clone((struct udp_transport *)
			    (dst->sa_family == AF_INET ? default_transport :
			    default_transport6), dst);
			goto ret;
		}
	}
	if (text2sockaddr(addr_str, port_str, &addr)) {
		log_print("udp_create: address \"%s\" not understood",
		    addr_str);
		rv = 0;
		goto ret;
	}
	u = udp_listen_lookup(addr);
	free(addr);
	if (!u) {
		log_print("udp_create: %s:%s must exist as a listener too",
		    addr_str, port_str);
		rv = 0;
		goto ret;
	}
	rv = udp_clone(u, dst);

ret:
	free(dst);
	return rv;
}

void
udp_remove(struct transport *t)
{
	struct udp_transport *u = (struct udp_transport *)t;

	if (u->src)
		free(u->src);
	if (u->dst)
		free(u->dst);
	if (t->flags & TRANSPORT_LISTEN) {
		if (u->s >= 0)
			close(u->s);
		if (t == default_transport)
			default_transport = 0;
		else if (t == default_transport6)
			default_transport6 = 0;
		if (u->link.le_prev)
			LIST_REMOVE(u, link);
	}
	free(t);
}

/* Report transport-method specifics of the T transport. */
void
udp_report(struct transport *t)
{
	struct udp_transport *u = (struct udp_transport *)t;
	char	*src, *dst;

	if (sockaddr2text(u->src, &src, 0))
		goto ret;

	if (!u->dst || sockaddr2text(u->dst, &dst, 0))
		dst = 0;

	LOG_DBG((LOG_REPORT, 0, "udp_report: fd %d src %s dst %s", u->s, src,
	    dst ? dst : "<none>"));

ret:
	if (dst)
		free(dst);
	if (src)
		free(src);
}

/*
 * Probe the interface list and determine what new interfaces have
 * appeared.
 *
 * At the same time, we try to determine whether existing interfaces have
 * been rendered invalid; we do this by marking all UDP transports before
 * we call udp_bind_if () through if_map (), and then releasing those
 * transports that have not been unmarked.
 */
static void
udp_reinit(void)
{
	struct udp_transport *u, *u2;
	char		*port;

	/* Initialize the protocol and port numbers. */
	port = udp_default_port ? udp_default_port : "500";

	/* Mark all UDP transports, except the default ones. */
	for (u = LIST_FIRST(&udp_listen_list); u; u = LIST_NEXT(u, link))
		if (&u->transport != default_transport &&
		    &u->transport != default_transport6)
			u->transport.flags |= TRANSPORT_MARK;

	/* Re-probe interface list. */
	if (if_map(udp_bind_if, port) == -1)
		log_print("udp_init: Could not bind the ISAKMP UDP port %s "
		    "on all interfaces", port);

	/*
	 * Release listening transports for local addresses that no
	 * longer exist. udp_bind_if () will have left those still marked.
         */
	u = LIST_FIRST(&udp_listen_list);
	while (u) {
		u2 = LIST_NEXT(u, link);

		if (u->transport.flags & TRANSPORT_MARK) {
			LIST_REMOVE(u, link);
			transport_release(&u->transport);
		}
		u = u2;
	}
}

/*
 * Find out the magic numbers for the UDP protocol as well as the UDP port
 * to use.  Setup an UDP server for each address of this machine, and one
 * for the generic case when we are the initiator.
 */
void
udp_init(void)
{
	struct sockaddr_storage dflt_stor;
	struct sockaddr_in *dflt = (struct sockaddr_in *) & dflt_stor;
	struct conf_list *listen_on;
	char           *port;
	long            lport;
	char           *ep;

	/* Initialize the protocol and port numbers.  */
	port = udp_default_port ? udp_default_port : "500";

	LIST_INIT(&udp_listen_list);

	transport_method_add(&udp_transport_vtbl);

	/* Bind the ISAKMP UDP port on all network interfaces we have.  */
	if (if_map(udp_bind_if, port) == -1)
		log_fatal("udp_init: Could not bind the ISAKMP UDP port %s "
		    "on all interfaces", port);

	/* Only listen to the specified address if Listen-on is configured */
	listen_on = conf_get_list("General", "Listen-on");
	if (listen_on) {
		LOG_DBG((LOG_TRANSPORT, 50,
		    "udp_init: not binding ISAKMP UDP port to INADDR_ANY"));
		conf_free_list(listen_on);
		return;
	}
	/*
	 * Get port.
	 * XXX Use getservbyname too.
         */
	lport = strtol(port, &ep, 10);
	if (*ep != '\0' || lport < (long) 0 || lport > (long) USHRT_MAX) {
		log_print("udp_init: port string \"%s\" not convertible to "
		    "in_port_t", port);
		return;
	}
	/*
	 * Bind to INADDR_ANY in case of new addresses popping up.  Packet
	 * reception on this transport is taken as a hint to reprobe the
	 * interface list.
         */
	if (!bind_family || (bind_family & BIND_FAMILY_INET4)) {
		memset(&dflt_stor, 0, sizeof dflt_stor);
		dflt->sin_family = AF_INET;
#if !defined (LINUX_IPSEC)
		((struct sockaddr_in *)dflt)->sin_len =
		    sizeof(struct sockaddr_in);
#endif
		((struct sockaddr_in *)dflt)->sin_port = htons(lport);

		default_transport = udp_bind((struct sockaddr *)&dflt_stor);
		if (!default_transport) {
			log_error("udp_init: could not allocate default "
			    "IPv4 ISAKMP UDP port");
			return;
		}
		LIST_INSERT_HEAD(&udp_listen_list,
		    (struct udp_transport *)default_transport, link);
	}
	if (!bind_family || (bind_family & BIND_FAMILY_INET6)) {
		memset(&dflt_stor, 0, sizeof dflt_stor);
		dflt->sin_family = AF_INET6;
#if !defined (LINUX_IPSEC)
		((struct sockaddr_in6 *)dflt)->sin6_len =
		    sizeof(struct sockaddr_in6);
#endif
		((struct sockaddr_in6 *)dflt)->sin6_port = htons(lport);

		default_transport6 = udp_bind((struct sockaddr *)&dflt_stor);
		if (!default_transport6) {
			log_error("udp_init: could not allocate default "
			    "IPv6 ISAKMP UDP port");
			return;
		}
		LIST_INSERT_HEAD(&udp_listen_list,
		    (struct udp_transport *)default_transport6, link);
	}
}

/*
 * Set transport T's socket in FDS, return a value useable by select(2)
 * as the number of file descriptors to check.
 */
static int
udp_fd_set(struct transport *t, fd_set *fds, int bit)
{
	struct udp_transport *u = (struct udp_transport *)t;

	if (bit)
		FD_SET(u->s, fds);
	else
		FD_CLR(u->s, fds);

	return u->s + 1;
}

/* Check if transport T's socket is set in FDS.  */
static int
udp_fd_isset(struct transport *t, fd_set *fds)
{
	struct udp_transport *u = (struct udp_transport *)t;

	return FD_ISSET(u->s, fds);
}

/*
 * A message has arrived on transport T's socket.  If T is single-ended,
 * clone it into a double-ended transport which we will use from now on.
 * Package the message as we want it and continue processing in the message
 * module.
 */
static void
udp_handle_message(struct transport * t)
{
	struct udp_transport *u = (struct udp_transport *) t;
	u_int8_t        buf[UDP_SIZE];
	struct sockaddr_storage from;
	u_int32_t       len = sizeof from;
	ssize_t         n;
	struct message *msg;

	n = recvfrom(u->s, buf, UDP_SIZE, 0, (struct sockaddr *) & from, &len);
	if (n == -1) {
		log_error("recvfrom (%d, %p, %d, %d, %p, %p)", u->s, buf,
		    UDP_SIZE, 0, &from, &len);
		return;
	}
	/*
	 * If we received the packet over the default transports, reprobe the
	 * interfaces.
         */
	if (t == default_transport || t == default_transport6) {
		udp_reinit();

		/*
		 * As we don't know the actual destination address of the
		 * packet, we can't really deal with it. So, just ignore it
		 * and hope we catch the retransmission.
	         */
		return;
	}
	/*
	 * Make a specialized UDP transport structure out of the incoming
	 * transport and the address information we got from recvfrom(2).
         */
	t = udp_clone(u, (struct sockaddr *)&from);
	if (!t)
		return;

	msg = message_alloc(t, buf, n);
	if (!msg) {
		log_error("failed to allocate message structure, dropping "
		    "packet received on transport %p", u);
		return;
	}
	message_recv(msg);
}

/* Physically send the message MSG over its associated transport.  */
static int
udp_send_message(struct message *msg)
{
	struct udp_transport *u = (struct udp_transport *)msg->transport;
	ssize_t         n;
	struct msghdr   m;

	/*
	 * Sending on connected sockets requires that no destination address is
	 * given, or else EISCONN will occur.
         */
	m.msg_name = (caddr_t) u->dst;
	m.msg_namelen = sysdep_sa_len(u->dst);
	m.msg_iov = msg->iov;
	m.msg_iovlen = msg->iovlen;
	m.msg_control = 0;
	m.msg_controllen = 0;
	m.msg_flags = 0;
	n = sendmsg(u->s, &m, 0);
	if (n == -1) {
		/* XXX We should check whether the address has gone away */
		log_error("sendmsg (%d, %p, %d)", u->s, &m, 0);
		return -1;
	}
	return 0;
}

/*
 * Get transport T's peer address and stuff it into the sockaddr pointed
 * to by DST.
 */
static void
udp_get_dst(struct transport *t, struct sockaddr **dst)
{
	*dst = ((struct udp_transport *)t)->dst;
}

/*
 * Get transport T's local address and stuff it into the sockaddr pointed
 * to by SRC.  Put its length into SRC_LEN.
 */
static void
udp_get_src(struct transport *t, struct sockaddr **src)
{
	*src = ((struct udp_transport *)t)->src;
}

static char *
udp_decode_ids(struct transport *t)
{
	static char     result[1024];
	char            idsrc[256], iddst[256];

#ifdef HAVE_GETNAMEINFO
	if (getnameinfo(((struct udp_transport *)t)->src,
	    sysdep_sa_len(((struct udp_transport *)t)->src),
	    idsrc, sizeof idsrc, NULL, 0, NI_NUMERICHOST) != 0) {
		log_print("udp_decode_ids: getnameinfo () failed for 'src'");
		strlcpy(idsrc, "<error>", 256);
	}
	if (getnameinfo(((struct udp_transport *)t)->dst,
	    sysdep_sa_len(((struct udp_transport *)t)->dst),
	    iddst, sizeof iddst, NULL, 0, NI_NUMERICHOST) != 0) {
		log_print("udp_decode_ids: getnameinfo () failed for 'dst'");
		strlcpy(iddst, "<error>", 256);
	}
#else
	strlcpy(idsrc, inet_ntoa(((struct udp_transport *)t)->src.sin_addr),
	    256);
	strlcpy(iddst, inet_ntoa(((struct udp_transport *)t)->dst.sin_addr),
	    256);
#endif				/* HAVE_GETNAMEINFO */

	snprintf(result, sizeof result, "src: %s dst: %s", idsrc, iddst);
	return result;
}

#if 0
/*
 * Take a string containing an ext representation of port and return a
 * binary port number in host byte order.  Return zero if anything goes wrong.
 * XXX Currently unused.
 */
static in_port_t
udp_decode_port(char *port_str)
{
	char           *port_str_end;
	long            port_long;
	struct servent *service;

	port_long = ntohl(strtol(port_str, &port_str_end, 0));
	if (port_str == port_str_end) {
		service = getservbyname(port_str, "udp");
		if (!service) {
			log_print("udp_decode_port: service \"%s\" unknown",
			    port_str);
			return 0;
		}
		return ntohs(service->s_port);
	} else if (port_long < 1 || port_long > 65535) {
		log_print("udp_decode_port: port %ld out of range", port_long);
		return 0;
	}
	return port_long;
}
#endif
