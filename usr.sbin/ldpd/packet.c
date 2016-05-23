/*	$OpenBSD: packet.c,v 1.51 2016/05/23 16:18:51 renato Exp $ */

/*
 * Copyright (c) 2009 Michele Marchetto <michele@openbsd.org>
 * Copyright (c) 2004, 2005, 2008 Esben Norby <norby@openbsd.org>
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <net/if_dl.h>
#include <fcntl.h>
#include <unistd.h>

#include <errno.h>
#include <event.h>
#include <stdlib.h>
#include <string.h>

#include "ldpd.h"
#include "ldp.h"
#include "log.h"
#include "ldpe.h"

extern struct ldpd_conf        *leconf;
extern struct ldpd_sysdep	sysdep;

struct iface	*disc_find_iface(unsigned int, struct in_addr, int);
ssize_t		 session_get_pdu(struct ibuf_read *, char **);

static int	 msgcnt = 0;

int
gen_ldp_hdr(struct ibuf *buf, u_int16_t size)
{
	struct ldp_hdr	ldp_hdr;

	bzero(&ldp_hdr, sizeof(ldp_hdr));
	ldp_hdr.version = htons(LDP_VERSION);
	/* exclude the 'Version' and 'PDU Length' fields from the total */
	ldp_hdr.length = htons(size - LDP_HDR_DEAD_LEN);
	ldp_hdr.lsr_id = leconf->rtr_id.s_addr;
	ldp_hdr.lspace_id = 0;

	return (ibuf_add(buf, &ldp_hdr, LDP_HDR_SIZE));
}

int
gen_msg_tlv(struct ibuf *buf, u_int32_t type, u_int16_t size)
{
	struct ldp_msg	msg;

	/* We want just the size of the value */
	size -= TLV_HDR_LEN;

	bzero(&msg, sizeof(msg));
	msg.type = htons(type);
	msg.length = htons(size);
	if (type != MSG_TYPE_HELLO)
		msg.msgid = htonl(++msgcnt);

	return (ibuf_add(buf, &msg, sizeof(msg)));
}

/* send packets */
int
send_packet(int fd, struct iface *iface, void *pkt, size_t len,
    struct sockaddr_in *dst)
{
	/* set outgoing interface for multicast traffic */
	if (iface && IN_MULTICAST(ntohl(dst->sin_addr.s_addr)))
		if (if_set_mcast(iface) == -1) {
			log_warn("%s: error setting multicast interface, %s",
			    __func__, iface->name);
			return (-1);
		}

	if (sendto(fd, pkt, len, 0, (struct sockaddr *)dst,
	    sizeof(*dst)) == -1) {
		log_warn("%s: error sending packet to %s", __func__,
		    inet_ntoa(dst->sin_addr));
		return (-1);
	}

	return (0);
}

/* Discovery functions */
void
disc_recv_packet(int fd, short event, void *bula)
{
	union {
		struct cmsghdr hdr;
		char	buf[CMSG_SPACE(sizeof(struct sockaddr_dl))];
	} cmsgbuf;
	struct sockaddr_in	 src;
	struct msghdr		 msg;
	struct iovec		 iov;
	struct ldp_hdr		 ldp_hdr;
	struct ldp_msg		 ldp_msg;
	struct iface		*iface = NULL;
	char			*buf;
	struct cmsghdr		*cmsg;
	ssize_t			 r;
	u_int16_t		 len;
	unsigned int		 ifindex = 0;

	if (event != EV_READ)
		return;

	/* setup buffer */
	bzero(&msg, sizeof(msg));
	iov.iov_base = buf = pkt_ptr;
	iov.iov_len = IBUF_READ_SIZE;
	msg.msg_name = &src;
	msg.msg_namelen = sizeof(src);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = &cmsgbuf.buf;
	msg.msg_controllen = sizeof(cmsgbuf.buf);

	if ((r = recvmsg(fd, &msg, 0)) == -1) {
		if (errno != EAGAIN && errno != EINTR)
			log_debug("%s: read error: %s", __func__,
			    strerror(errno));
		return;
	}

	multicast = (msg.msg_flags & MSG_MCAST) ? 1 : 0;
	if (bad_ip_addr(src.sin_addr)) {
		log_debug("%s: invalid source address: %s", __func__,
		    inet_ntoa(src.sin_addr));
		return;
	}

	for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level == IPPROTO_IP &&
		    cmsg->cmsg_type == IP_RECVIF) {
			ifindex = ((struct sockaddr_dl *)
			    CMSG_DATA(cmsg))->sdl_index;
			break;
		}
	}

	len = (u_int16_t)r;

	/* find a matching interface */
	if ((fd == leconf->ldp_discovery_socket) &&
	    (iface = disc_find_iface(ifindex, src.sin_addr)) == NULL) {
		log_debug("disc_recv_packet: cannot find a matching subnet "
		    "on interface index %d for %s", ifindex,
		    inet_ntoa(src.sin_addr));
		return;
	}

	/* LDP header sanity checks */
	if (len < LDP_HDR_SIZE || len > LDP_MAX_LEN) {
		log_debug("disc_recv_packet: bad packet size");
		return;
	}
	bcopy(buf, &ldp_hdr, sizeof(ldp_hdr));

	if (ntohs(ldp_hdr.version) != LDP_VERSION) {
		log_debug("dsc_recv_packet: invalid LDP version %d",
		    ldp_hdr.version);
		return;
	}

	if (ntohs(ldp_hdr.length) >
	    len - sizeof(ldp_hdr.version) - sizeof(ldp_hdr.length)) {
		log_debug("disc_recv_packet: invalid LDP packet length %u",
		    ntohs(ldp_hdr.length));
		return;
	}

	if (len < LDP_HDR_SIZE + LDP_MSG_LEN) {
		log_debug("disc_recv_packet: invalid LDP packet length %d",
		    ntohs(ldp_hdr.length));
		return;
	}

	bcopy(buf + LDP_HDR_SIZE, &ldp_msg, sizeof(ldp_msg));

	/* switch LDP packet type */
	switch (ntohs(ldp_msg.type)) {
	case MSG_TYPE_HELLO:
		recv_hello(iface, src.sin_addr, buf, len);
		break;
	default:
		log_debug("%s: unknown LDP packet type, source %s", __func__,
		    inet_ntoa(src.sin_addr));
	}
}

struct iface *
disc_find_iface(unsigned int ifindex, struct in_addr src,
    int multicast)
{
	struct iface	*iface;
	struct if_addr	*if_addr;

	iface = if_lookup(leconf, ifindex);
	if (iface == NULL)
		return (NULL);

	if (!multicast)
		return (iface);

	LIST_FOREACH(if_addr, &iface->addr_list, entry) {
		switch (iface->type) {
		case IF_TYPE_POINTOPOINT:
			if (ifindex == iface->ifindex &&
			    if_addr->dstbrd.s_addr == src.s_addr)
				return (iface);
			break;
		default:
			if (ifindex == iface->ifindex &&
			    (if_addr->addr.s_addr & if_addr->mask.s_addr) ==
			    (src.s_addr & if_addr->mask.s_addr))
				return (iface);
			break;
		}
	}

	return (NULL);
}

void
session_accept(int fd, short event, void *bula)
{
	struct sockaddr_in	 src;
	socklen_t		 len = sizeof(src);
	int			 newfd;
	struct nbr		*nbr;
	struct pending_conn	*pconn;

	if (!(event & EV_READ))
		return;

	newfd = accept4(fd, (struct sockaddr *)&src, &len,
	    SOCK_NONBLOCK | SOCK_CLOEXEC);
	if (newfd == -1) {
		/*
		 * Pause accept if we are out of file descriptors, or
		 * libevent will haunt us here too.
		 */
		if (errno == ENFILE || errno == EMFILE) {
			accept_pause();
		} else if (errno != EWOULDBLOCK && errno != EINTR &&
		    errno != ECONNABORTED)
			log_debug("%s: accept error: %s", __func__,
			    strerror(errno));
		return;
	}

	/*
	 * Since we don't support label spaces, we can identify this neighbor
	 * just by its source address. This way we don't need to wait for its
	 * Initialization message to know who we are talking to.
	 */
	nbr = nbr_find_addr(src.sin_addr);
	if (nbr == NULL) {
		/*
		 * According to RFC 5036, we would need to send a No Hello
		 * Error Notification message and close this TCP connection
		 * right now. But doing so would trigger the backoff exponential
		 * timer in the remote peer, which would considerably slow down
		 * the session establishment process. The trick here is to wait
		 * five seconds before sending the Notification Message. There's
		 * a good chance that the remote peer will send us a Hello
		 * message within this interval, so it's worth waiting before
		 * taking a more drastic measure.
		 */
		pconn = pending_conn_find(src.sin_addr);
		if (pconn)
			close(newfd);
		else
			pending_conn_new(newfd, src.sin_addr);
		return;
	}
	/* protection against buggy implementations */
	if (nbr_session_active_role(nbr)) {
		close(newfd);
		return;
	}
	if (nbr->state != NBR_STA_PRESENT) {
		log_debug("%s: lsr-id %s: rejecting additional transport "
		    "connection", __func__, inet_ntoa(nbr->id));
		close(newfd);
		return;
	}

	session_accept_nbr(nbr, newfd);
}

void
session_accept_nbr(struct nbr *nbr, int fd)
{
	struct nbr_params	*nbrp;
	int			 opt;
	socklen_t		 len;

	nbrp = nbr_params_find(leconf, nbr->id);
	if (nbrp && nbrp->auth.method == AUTH_MD5SIG) {
		if (sysdep.no_pfkey || sysdep.no_md5sig) {
			log_warnx("md5sig configured but not available");
			close(fd);
			return;
		}

		len = sizeof(opt);
		if (getsockopt(fd, IPPROTO_TCP, TCP_MD5SIG, &opt, &len) == -1)
			fatal("getsockopt TCP_MD5SIG");
		if (!opt) {	/* non-md5'd connection! */
			log_warnx("connection attempt without md5 signature");
			close(fd);
			return;
		}
	}

	nbr->tcp = tcp_new(fd, nbr);
	nbr_fsm(nbr, NBR_EVT_MATCH_ADJ);
}

void
session_read(int fd, short event, void *arg)
{
	struct nbr	*nbr = arg;
	struct tcp_conn	*tcp = nbr->tcp;
	struct ldp_hdr	*ldp_hdr;
	struct ldp_msg	*ldp_msg;
	char		*buf, *pdu;
	ssize_t		 n, len;
	int		 msg_size;
	u_int16_t	 pdu_len;

	if (event != EV_READ)
		return;

	if ((n = read(fd, tcp->rbuf->buf + tcp->rbuf->wpos,
	    sizeof(tcp->rbuf->buf) - tcp->rbuf->wpos)) == -1) {
		if (errno != EINTR && errno != EAGAIN) {
			log_warn("%s: read error", __func__);
			nbr_fsm(nbr, NBR_EVT_CLOSE_SESSION);
			return;
		}
		/* retry read */
		return;
	}
	if (n == 0) {
		/* connection closed */
		log_debug("%s: connection closed by remote end", __func__);
		nbr_fsm(nbr, NBR_EVT_CLOSE_SESSION);
		return;
	}
	tcp->rbuf->wpos += n;

	while ((len = session_get_pdu(tcp->rbuf, &buf)) > 0) {
		pdu = buf;
		ldp_hdr = (struct ldp_hdr *)pdu;
		if (ntohs(ldp_hdr->version) != LDP_VERSION) {
			session_shutdown(nbr, S_BAD_PROTO_VER, 0, 0);
			free(buf);
			return;
		}

		pdu_len = ntohs(ldp_hdr->length);
		/*
	 	 * RFC 5036 - Section 3.5.3:
		 * "Prior to completion of the negotiation, the maximum
		 * allowable length is 4096 bytes".
		 */
		if (nbr->state == NBR_STA_OPER)
			max_pdu_len = nbr->max_pdu_len;
		else
			max_pdu_len = LDP_MAX_LEN;
		if (pdu_len < (LDP_HDR_PDU_LEN + LDP_MSG_SIZE) ||
		    pdu_len > max_pdu_len) {
			session_shutdown(nbr, S_BAD_PDU_LEN, 0, 0);
			free(buf);
			return;
		}
		pdu_len -= LDP_HDR_PDU_LEN;

		if (ldp_hdr->lsr_id != nbr->id.s_addr ||
		    ldp_hdr->lspace_id != 0) {
			session_shutdown(nbr, S_BAD_LDP_ID, 0, 0);
			free(buf);
			return;
		}

		pdu += LDP_HDR_SIZE;
		len -= LDP_HDR_SIZE;

		if (nbr->state == NBR_STA_OPER)
			nbr_fsm(nbr, NBR_EVT_PDU_RCVD);

		while (len >= LDP_MSG_LEN) {
			u_int16_t type;

			ldp_msg = (struct ldp_msg *)pdu;
			type = ntohs(ldp_msg->type);

			pdu_len = ntohs(ldp_msg->length) + TLV_HDR_LEN;
			if (pdu_len > len ||
			    pdu_len < LDP_MSG_LEN - TLV_HDR_LEN) {
				session_shutdown(nbr, S_BAD_TLV_LEN,
				    ldp_msg->msgid, ldp_msg->type);
				free(buf);
				return;
			}

			/* check for error conditions earlier */
			switch (type) {
			case MSG_TYPE_INIT:
				if ((nbr->state != NBR_STA_INITIAL) &&
				    (nbr->state != NBR_STA_OPENSENT)) {
					session_shutdown(nbr, S_SHUTDOWN,
					    ldp_msg->msgid, ldp_msg->type);
					free(buf);
					return;
				}
				break;
			case MSG_TYPE_KEEPALIVE:
				if ((nbr->state == NBR_STA_INITIAL) ||
				    (nbr->state == NBR_STA_OPENSENT)) {
					session_shutdown(nbr, S_SHUTDOWN,
					    ldp_msg->msgid, ldp_msg->type);
					free(buf);
					return;
				}
				break;
			case MSG_TYPE_ADDR:
			case MSG_TYPE_ADDRWITHDRAW:
			case MSG_TYPE_LABELMAPPING:
			case MSG_TYPE_LABELREQUEST:
			case MSG_TYPE_LABELWITHDRAW:
			case MSG_TYPE_LABELRELEASE:
			case MSG_TYPE_LABELABORTREQ:
				if (nbr->state != NBR_STA_OPER) {
					session_shutdown(nbr, S_SHUTDOWN,
					    ldp_msg->msgid, ldp_msg->type);
					free(buf);
					return;
				}
				break;
			default:
				break;
			}

			/* switch LDP packet type */
			switch (type) {
			case MSG_TYPE_NOTIFICATION:
				ret = recv_notification(nbr, pdu, msg_size);
				break;
			case MSG_TYPE_INIT:
				ret = recv_init(nbr, pdu, msg_size);
				break;
			case MSG_TYPE_KEEPALIVE:
				ret = recv_keepalive(nbr, pdu, msg_size);
				break;
			case MSG_TYPE_ADDR:
			case MSG_TYPE_ADDRWITHDRAW:
				ret = recv_address(nbr, pdu, msg_size);
				break;
			case MSG_TYPE_LABELMAPPING:
			case MSG_TYPE_LABELREQUEST:
			case MSG_TYPE_LABELWITHDRAW:
			case MSG_TYPE_LABELRELEASE:
			case MSG_TYPE_LABELABORTREQ:
				ret = recv_labelmessage(nbr, pdu, msg_size,
				    type);
				break;
			default:
				log_debug("%s: unknown LDP packet from nbr %s",
				    __func__, inet_ntoa(nbr->id));
				if (!(ntohs(ldp_msg->type) & UNKNOWN_FLAG)) {
					session_shutdown(nbr, S_UNKNOWN_MSG,
					    ldp_msg->msgid, ldp_msg->type);
					free(buf);
					return;
				}
				/* unknown flag is set, ignore the message */
				ret = 0;
				break;
			}

			if (ret == -1) {
				/* parser failed, giving up */
				free(buf);
				return;
			}

			/* Analyse the next message */
			pdu += msg_size;
			len -= msg_size;
		}
		free(buf);
		if (len != 0) {
			session_shutdown(nbr, S_BAD_PDU_LEN, 0, 0);
			return;
		}
	}
}

void
session_write(int fd, short event, void *arg)
{
	struct tcp_conn *tcp = arg;
	struct nbr	*nbr = tcp->nbr;

	if (!(event & EV_WRITE))
		return;

	if (msgbuf_write(&tcp->wbuf.wbuf) <= 0)
		if (errno != EAGAIN && nbr)
			nbr_fsm(nbr, NBR_EVT_CLOSE_SESSION);

	if (nbr == NULL && !tcp->wbuf.wbuf.queued) {
		/*
		 * We are done sending the notification message, now we can
		 * close the socket.
		 */
		tcp_close(tcp);
		return;
	}

	evbuf_event_add(&tcp->wbuf);
}

void
session_shutdown(struct nbr *nbr, u_int32_t status, u_int32_t msgid,
    u_int32_t type)
{
	if (nbr->tcp == NULL)
		return;

	log_debug("%s: nbr ID %s", __func__, inet_ntoa(nbr->id));

	send_notification_nbr(nbr, status, msgid, type);

	/* try to flush write buffer, if it fails tough shit */
	msgbuf_write(&nbr->tcp->wbuf.wbuf);

	nbr_fsm(nbr, NBR_EVT_CLOSE_SESSION);
}

void
session_close(struct nbr *nbr)
{
	log_debug("%s: closing session with nbr ID %s", __func__,
	    inet_ntoa(nbr->id));

	tcp_close(nbr->tcp);
	nbr_stop_ktimer(nbr);
	nbr_stop_ktimeout(nbr);
}

ssize_t
session_get_pdu(struct ibuf_read *r, char **b)
{
	struct ldp_hdr	l;
	size_t		av, dlen, left;

	av = r->wpos;
	if (av < sizeof(l))
		return (0);

	memcpy(&l, r->buf, sizeof(l));
	dlen = ntohs(l.length) + LDP_HDR_DEAD_LEN;
	if (dlen > av)
		return (0);

	if ((*b = malloc(dlen)) == NULL)
		return (-1);

	memcpy(*b, r->buf, dlen);
	if (dlen < av) {
		left = av - dlen;
		memmove(r->buf, r->buf + dlen, left);
		r->wpos = left;
	} else
		r->wpos = 0;

	return (dlen);
}

struct tcp_conn *
tcp_new(int fd, struct nbr *nbr)
{
	struct tcp_conn *tcp;

	if ((tcp = calloc(1, sizeof(*tcp))) == NULL)
		fatal(__func__);

	tcp->fd = fd;
	evbuf_init(&tcp->wbuf, tcp->fd, session_write, tcp);

	if (nbr) {
		if ((tcp->rbuf = calloc(1, sizeof(struct ibuf_read))) == NULL)
			fatal(__func__);

		event_set(&tcp->rev, tcp->fd, EV_READ | EV_PERSIST,
		    session_read, nbr);
		event_add(&tcp->rev, NULL);
		tcp->nbr = nbr;
	}

	return (tcp);
}

void
tcp_close(struct tcp_conn *tcp)
{
	evbuf_clear(&tcp->wbuf);

	if (tcp->nbr) {
		event_del(&tcp->rev);
		free(tcp->rbuf);
		tcp->nbr->tcp = NULL;
	}

	close(tcp->fd);
	accept_unpause();
	free(tcp);
}

struct pending_conn *
pending_conn_new(int fd, struct in_addr addr)
{
	struct pending_conn	*pconn;
	struct timeval		 tv;

	if ((pconn = calloc(1, sizeof(*pconn))) == NULL)
		fatal(__func__);

	pconn->fd = fd;
	pconn->addr = addr;
	evtimer_set(&pconn->ev_timeout, pending_conn_timeout, pconn);
	TAILQ_INSERT_TAIL(&global.pending_conns, pconn, entry);

	timerclear(&tv);
	tv.tv_sec = PENDING_CONN_TIMEOUT;
	if (evtimer_add(&pconn->ev_timeout, &tv) == -1)
		fatal(__func__);

	return (pconn);
}

void
pending_conn_del(struct pending_conn *pconn)
{
	if (evtimer_pending(&pconn->ev_timeout, NULL) &&
	    evtimer_del(&pconn->ev_timeout) == -1)
		fatal(__func__);

	TAILQ_REMOVE(&global.pending_conns, pconn, entry);
	free(pconn);
}

struct pending_conn *
pending_conn_find(struct in_addr addr)
{
	struct pending_conn	*pconn;

	TAILQ_FOREACH(pconn, &global.pending_conns, entry)
		if (addr.s_addr == pconn->addr.s_addr)
			return (pconn);

	return (NULL);
}

void
pending_conn_timeout(int fd, short event, void *arg)
{
	struct pending_conn	*pconn = arg;
	struct tcp_conn		*tcp;

	log_debug("%s: no adjacency with remote end: %s", __func__,
	    inet_ntoa(pconn->addr));

	/*
	 * Create a write buffer detached from any neighbor to send a
	 * notification message reliably.
	 */
	tcp = tcp_new(pconn->fd, NULL);
	send_notification(S_NO_HELLO, tcp, 0, 0);
	msgbuf_write(&tcp->wbuf.wbuf);

	pending_conn_del(pconn);
}
