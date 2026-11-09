/*	$OpenBSD: transport_imsg.c,v 1.1 2026/11/09 12:31:36 hshoexer Exp $	*/

/*
 * Copyright (c) 2026 Hans-Joerg Hoexer <hshoexer@yerbouti.franken.de>
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

/*
 * The imsg transport, used when isakmpd runs as the ikev1 process of
 * iked(8).  The ikev2 process of iked owns the UDP sockets; it forwards
 * every IKEv1 datagram over imsg together with the peer and local
 * address and takes our replies back the same way.
 *
 * Unlike udp.c and virtual.c this is a single-level transport: each
 * instance is its own "virtual" transport and carries the send queues,
 * and there is one instance per peer/local address pair, created when a
 * datagram arrives.  There are no file descriptors; fd_set and fd_isset
 * are stubs so that transport_send_messages() keeps working, and the
 * iked mode event loop calls transport_imsg_flush() to drain the queues.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <imsg.h>
#include <limits.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>

#include "conf.h"
#include "ikedmode.h"
#include "log.h"
#include "message.h"
#include "transport.h"
#include "isakmp.h"
#include "udp.h"
#include "util.h"

#include "iked/types.h"

struct imsg_transport {
	struct transport	 transport;
	struct sockaddr		*src;		/* our address */
	struct sockaddr		*dst;		/* the peer's address */
	int			 natt;		/* UDP encapsulated */
};

static struct transport	*transport_imsg_create(char *);
static void		 transport_imsg_remove(struct transport *);
static int		 transport_imsg_fd_set(struct transport *, fd_set *,
			     int);
static int		 transport_imsg_fd_isset(struct transport *,
			     fd_set *);
static void		 transport_imsg_handle_message(struct transport *);
static int		 transport_imsg_send_message(struct message *,
			     struct transport *);
static void		 transport_imsg_get_dst(struct transport *,
			     struct sockaddr **);
static void		 transport_imsg_get_src(struct transport *,
			     struct sockaddr **);
static char		*transport_imsg_decode_ids(struct transport *);
static struct transport	*transport_imsg_clone(struct transport *,
			     struct sockaddr *);
static struct msg_head	*transport_imsg_get_queue(struct message *);
static struct transport	*transport_imsg_make(struct sockaddr *,
			     struct sockaddr *, int);

static struct transport_vtbl transport_imsg_vtbl = {
	{ 0 }, "imsg",
	transport_imsg_create,
	0,
	transport_imsg_remove,
	transport_imsg_fd_set,
	transport_imsg_fd_isset,
	transport_imsg_handle_message,
	transport_imsg_send_message,
	transport_imsg_get_dst,
	transport_imsg_get_src,
	transport_imsg_decode_ids,
	transport_imsg_clone,
	transport_imsg_get_queue
};

void
transport_imsg_init(void)
{
	transport_method_add(&transport_imsg_vtbl);
}

/*
 * Create a transport for the pair of addresses SRC (ours) and DST (the
 * peer).  Like the clones in udp.c it starts without references; the
 * message or SA using it holds the reference and the transport goes away
 * with the last one.
 */
static struct transport *
transport_imsg_make(struct sockaddr *src, struct sockaddr *dst, int natt)
{
	struct imsg_transport	*t;

	t = calloc(1, sizeof *t);
	if (!t) {
		log_error("transport_imsg_make: calloc (1, %lu) failed",
		    (unsigned long)sizeof *t);
		return 0;
	}
	t->src = malloc(SA_LEN(src));
	t->dst = malloc(SA_LEN(dst));
	if (!t->src || !t->dst) {
		log_error("transport_imsg_make: malloc failed");
		free(t->src);
		free(t->dst);
		free(t);
		return 0;
	}
	memcpy(t->src, src, SA_LEN(src));
	memcpy(t->dst, dst, SA_LEN(dst));
	t->natt = natt;

	t->transport.vtbl = &transport_imsg_vtbl;
	transport_setup(&t->transport, 1);
	t->transport.virtual = &t->transport;
	LIST_INSERT_HEAD(&transport_list, &t->transport, link);

	LOG_DBG((LOG_TRANSPORT, 50, "transport_imsg_make: transport %p %s",
	    t, transport_imsg_decode_ids(&t->transport)));
	return &t->transport;
}

/*
 * A datagram from the ikev2 process: DG describes the addresses, BUF the
 * raw ISAKMP message of LEN bytes.
 */
void
transport_imsg_recv(struct ikev1_datagram *dg, u_int8_t *buf, size_t len)
{
	struct transport	*t;
	struct message		*msg;

	if (dg->dg_peerlen > sizeof dg->dg_peer ||
	    dg->dg_locallen > sizeof dg->dg_local ||
	    dg->dg_peerlen != SA_LEN((struct sockaddr *)&dg->dg_peer) ||
	    dg->dg_locallen != SA_LEN((struct sockaddr *)&dg->dg_local) ||
	    dg->dg_peer.ss_family != dg->dg_local.ss_family ||
	    (dg->dg_peer.ss_family != AF_INET &&
	    dg->dg_peer.ss_family != AF_INET6)) {
		log_print("transport_imsg_recv: invalid addresses, "
		    "dropping datagram");
		return;
	}

	t = transport_imsg_make((struct sockaddr *)&dg->dg_local,
	    (struct sockaddr *)&dg->dg_peer, dg->dg_natt);
	if (!t)
		return;

	msg = message_alloc(t, buf, len);
	if (!msg) {
		log_error("transport_imsg_recv: failed to allocate message "
		    "structure, dropping datagram received on transport %p",
		    t);
		t->vtbl->remove(t);
		return;
	}
	message_recv(msg);
}

/*
 * Hand all queued messages to the ikev2 process.  transport_send_messages()
 * sends one message per transport and call, as the main loop of isakmpd
 * does one per select(2) round.
 */
void
transport_imsg_flush(void)
{
	struct transport	*t;
	int			 pending;

	do {
		transport_send_messages(0);
		pending = 0;
		for (t = LIST_FIRST(&transport_list); t;
		    t = LIST_NEXT(t, link))
			if (TAILQ_FIRST(&t->virtual->sendq) ||
			    TAILQ_FIRST(&t->virtual->prio_sendq))
				pending = 1;
	} while (pending);
}

/*
 * Create a transport to the peer described by the configuration section
 * NAME, for the initiator role: "Address" and "Port" are the peer's,
 * "Local-address" is ours and is mandatory, there is no interface list to
 * pick it from.  Our port is the one the ikev2 process listens on.
 */
static struct transport *
transport_imsg_create(char *name)
{
	struct transport	*t = 0;
	struct sockaddr		*src = 0, *dst = 0;
	char			*addr_str, *port_str, *local_str, *lport_str;

	addr_str = conf_get_str(name, "Address");
	if (!addr_str) {
		log_print("transport_imsg_create: no address configured for "
		    "\"%s\"", name);
		return 0;
	}
	port_str = conf_get_str(name, "Port");
	if (!port_str)
		port_str = udp_default_port ? udp_default_port :
		    UDP_DEFAULT_PORT_STR;
	if (text2sockaddr(addr_str, port_str, &dst, 0, 0)) {
		log_print("transport_imsg_create: address \"%s\" not "
		    "understood", addr_str);
		return 0;
	}

	local_str = conf_get_str(name, "Local-address");
	if (!local_str) {
		log_print("transport_imsg_create: no local address configured "
		    "for \"%s\"", name);
		goto done;
	}
	lport_str = udp_default_port ? udp_default_port :
	    UDP_DEFAULT_PORT_STR;
	if (text2sockaddr(local_str, lport_str, &src, 0, 0)) {
		log_print("transport_imsg_create: local address \"%s\" not "
		    "understood", local_str);
		goto done;
	}
	if (src->sa_family != dst->sa_family) {
		log_print("transport_imsg_create: address family mismatch "
		    "for \"%s\"", name);
		goto done;
	}

	t = transport_imsg_make(src, dst, 0);
done:
	free(src);
	free(dst);
	return t;
}

static void
transport_imsg_remove(struct transport *t)
{
	struct imsg_transport	*it = (struct imsg_transport *)t;
	struct transport	*p;

	for (p = LIST_FIRST(&transport_list); p && p != t;
	    p = LIST_NEXT(p, link))
		;
	if (p == t)
		LIST_REMOVE(t, link);

	LOG_DBG((LOG_TRANSPORT, 90, "transport_imsg_remove: removed "
	    "transport %p", t));
	free(it->src);
	free(it->dst);
	free(t);
}

/* No descriptor: nothing to set, always ready to send.  */
static int
transport_imsg_fd_set(struct transport *t, fd_set *fds, int bit)
{
	return 0;
}

static int
transport_imsg_fd_isset(struct transport *t, fd_set *fds)
{
	return 1;
}

/* Datagrams arrive through transport_imsg_recv(), never here.  */
static void
transport_imsg_handle_message(struct transport *t)
{
}

static int
transport_imsg_send_message(struct message *msg, struct transport *t)
{
	struct imsg_transport	*it = (struct imsg_transport *)msg->transport;
	struct ikev1_datagram	 dg;
	struct iovec		 iov[IOV_MAX];
	size_t			 len = sizeof dg;
	u_int32_t		 i;

	if (msg->iovlen + 1 > IOV_MAX) {
		log_print("transport_imsg_send_message: message %p has too "
		    "many segments", msg);
		return -1;
	}

	bzero(&dg, sizeof dg);
	memcpy(&dg.dg_peer, it->dst, SA_LEN(it->dst));
	dg.dg_peerlen = SA_LEN(it->dst);
	memcpy(&dg.dg_local, it->src, SA_LEN(it->src));
	dg.dg_locallen = SA_LEN(it->src);
	dg.dg_natt = it->natt;

	iov[0].iov_base = &dg;
	iov[0].iov_len = sizeof dg;
	for (i = 0; i < msg->iovlen; i++) {
		iov[i + 1] = msg->iov[i];
		len += msg->iov[i].iov_len;
	}

	if (len > MAX_IMSGSIZE - IMSG_HEADER_SIZE) {
		log_print("transport_imsg_send_message: message %p too "
		    "large (%lu bytes)", msg, (unsigned long)len);
		return -1;
	}

	if (ikedmode_composev(PROC_IKEV2, IMSG_IKE_MESSAGE, iov,
	    msg->iovlen + 1) == -1) {
		log_print("transport_imsg_send_message: cannot send "
		    "message %p to ikev2", msg);
		return -1;
	}
	return 0;
}

static void
transport_imsg_get_dst(struct transport *t, struct sockaddr **dst)
{
	*dst = ((struct imsg_transport *)t)->dst;
}

static void
transport_imsg_get_src(struct transport *t, struct sockaddr **src)
{
	*src = ((struct imsg_transport *)t)->src;
}

static char *
transport_imsg_decode_ids(struct transport *t)
{
	struct imsg_transport	*it = (struct imsg_transport *)t;
	static char		 result[1024];
	char			 idsrc[256], iddst[256];

	if (getnameinfo(it->src, SA_LEN(it->src), idsrc, sizeof idsrc,
	    NULL, 0, NI_NUMERICHOST) != 0)
		strlcpy(idsrc, "<error>", sizeof idsrc);
	if (getnameinfo(it->dst, SA_LEN(it->dst), iddst, sizeof iddst,
	    NULL, 0, NI_NUMERICHOST) != 0)
		strlcpy(iddst, "<error>", sizeof iddst);

	snprintf(result, sizeof result, "src: %s dst: %s", idsrc, iddst);
	return result;
}

/* Clone T with a new peer address RADDR.  */
static struct transport *
transport_imsg_clone(struct transport *t, struct sockaddr *raddr)
{
	struct imsg_transport	*it = (struct imsg_transport *)t;

	return transport_imsg_make(it->src, raddr, it->natt);
}

static struct msg_head *
transport_imsg_get_queue(struct message *msg)
{
	if (msg->flags & MSG_PRIORITIZED)
		return &msg->transport->prio_sendq;
	else
		return &msg->transport->sendq;
}
