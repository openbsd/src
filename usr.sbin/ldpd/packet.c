/*	$OpenBSD: packet.c,v 1.4 2010/02/25 17:40:46 claudio Exp $ */

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
#include <netinet/in_systm.h>
#include <netinet/ip.h>
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

int		 ldp_hdr_sanity_check(struct ldp_hdr *, u_int16_t,
		    const struct iface *);
struct iface	*find_iface(struct ldpd_conf *, unsigned int, struct in_addr);
struct iface	*session_find_iface(struct ldpd_conf *, struct in_addr);
ssize_t		 session_get_pdu(struct buf_read *, char **);

static int	 msgcnt = 0;

int
gen_ldp_hdr(struct buf *buf, struct iface *iface, u_int16_t size)
{
	struct ldp_hdr	ldp_hdr;

	bzero(&ldp_hdr, sizeof(ldp_hdr));
	ldp_hdr.version = htons(LDP_VERSION);

	/* We want just the size of the value */
	size -= TLV_HDR_LEN;

	ldp_hdr.length = htons(size);
	ldp_hdr.lsr_id = ldpe_router_id();
	ldp_hdr.lspace_id = iface->lspace_id;

	return (buf_add(buf, &ldp_hdr, LDP_HDR_SIZE));
}

int
gen_msg_tlv(struct buf *buf, u_int32_t type, u_int16_t size)
{
	struct ldp_msg	msg;

	/* We want just the size of the value */
	size -= TLV_HDR_LEN;

	bzero(&msg, sizeof(msg));
	msg.type = htons(type);
	msg.length = htons(size);
	msg.msgid = htonl(++msgcnt);

	return (buf_add(buf, &msg, sizeof(msg)));
}

/* send and receive packets */
int
send_packet(struct iface *iface, void *pkt, size_t len, struct sockaddr_in *dst)
{
	/* set outgoing interface for multicast traffic */
	if (IN_MULTICAST(ntohl(dst->sin_addr.s_addr)))
		if (if_set_mcast(iface) == -1) {
			log_warn("send_packet: error setting multicast "
			    "interface, %s", iface->name);
			return (-1);
		}

	if (sendto(iface->discovery_fd, pkt, len, 0,
	    (struct sockaddr *)dst, sizeof(*dst)) == -1) {
		log_warn("send_packet: error sending packet on interface %s",
		    iface->name);
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
	struct ldpd_conf	*xconf = bula;
	struct ldp_hdr		*ldp_hdr;
	struct ldp_msg		*ldp_msg;
	struct iface		*iface;
	char			*buf;
	struct cmsghdr		*cmsg;
	ssize_t			 r;
	u_int16_t		 len;
	int			 l;
	unsigned int		 ifindex = 0;

	if (event != EV_READ)
		return;

	/* setup buffer */
	bzero(&msg, sizeof(msg));
	iov.iov_base = buf = pkt_ptr;
	iov.iov_len = READ_BUF_SIZE;
	msg.msg_name = &src;
	msg.msg_namelen = sizeof(src);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_control = &cmsgbuf.buf;
	msg.msg_controllen = sizeof(cmsgbuf.buf);

	if ((r = recvmsg(fd, &msg, 0)) == -1) {
		if (errno != EAGAIN && errno != EINTR)
			log_debug("disc_recv_packet: read error: %s",
			    strerror(errno));
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
	if ((iface = find_iface(xconf, ifindex, src.sin_addr)) == NULL) {
		log_debug("disc_recv_packet: cannot find a matching interface");
		return;
	}

	/* LDP header sanity checks */
	if (len < LDP_HDR_SIZE || len > LDP_MAX_LEN) {
		log_debug("disc_recv_packet: bad packet size");
		return;
	}
	ldp_hdr = (struct ldp_hdr *)buf;

	if (ntohs(ldp_hdr->version) != LDP_VERSION) {
		log_debug("dsc_recv_packet: invalid LDP version %d",
		    ldp_hdr->version);
		return;
	}

	if ((l = ldp_hdr_sanity_check(ldp_hdr, len, iface)) == -1)
		return;

	if (l > len) {
		log_debug("disc_recv_packet: invalid LDP packet length %d",
		    ntohs(ldp_hdr->length));
		return;
	}

	ldp_msg = (struct ldp_msg *)(buf + LDP_HDR_SIZE);

	if (len < LDP_HDR_SIZE + LDP_MSG_LEN) {
		log_debug("disc_recv_packet: invalid LDP packet length %d",
		    ntohs(ldp_hdr->length));
		return;
	}

	/* switch LDP packet type */
	switch (ntohs(ldp_msg->type)) {
	case MSG_TYPE_HELLO:
		recv_hello(iface, src.sin_addr, buf, len);
		break;
	default:
		log_debug("recv_packet: unknown LDP packet type, interface %s",
		    iface->name);
	}
}

int
ldp_hdr_sanity_check(struct ldp_hdr *ldp_hdr, u_int16_t len,
    const struct iface *iface)
{
	struct in_addr		 addr;

	if (iface->type != IF_TYPE_VIRTUALLINK) {
		if (ldp_hdr->lspace_id != iface->lspace_id) {
			addr.s_addr = ldp_hdr->lspace_id;
			log_debug("ldp_hdr_sanity_check: invalid label space "
			    "ID %s, interface %s", inet_ntoa(addr),
			    iface->name);
			return (-1);
		}
	} else {
		if (ldp_hdr->lspace_id != 0) {
			addr.s_addr = ldp_hdr->lspace_id;
			log_debug("ldp_hdr_sanity_check: invalid label space "
			    "ID %s, interface %s", inet_ntoa(addr),
			    iface->name);
			return (-1);
		}
	}

	return (ntohs(ldp_hdr->length));
}

struct iface *
find_iface(struct ldpd_conf *xconf, unsigned int ifindex, struct in_addr src)
{
	struct iface	*iface = NULL;

	/* returned interface needs to be active */
	LIST_FOREACH(iface, &xconf->iface_list, entry) {
		switch (iface->type) {
		case IF_TYPE_VIRTUALLINK:
			if ((src.s_addr == iface->dst.s_addr) &&
			    !iface->passive)
				return (iface);
			break;
		case IF_TYPE_POINTOPOINT:
			if (ifindex == iface->ifindex &&
			    iface->dst.s_addr == src.s_addr &&
			    !iface->passive)
				return (iface);
			break;
		default:
			if (ifindex == iface->ifindex &&
			    (iface->addr.s_addr & iface->mask.s_addr) ==
			    (src.s_addr & iface->mask.s_addr) &&
			    !iface->passive)
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
	struct ldpd_conf	*xconf = bula;
	struct iface		*iface;
	struct nbr		*nbr = NULL;
	int			 newfd;
	socklen_t		 len = sizeof(src);

	if (event != EV_READ)
		return;

	newfd = accept(fd, (struct sockaddr *)&src, &len);
	if (newfd == -1) {
		log_debug("sess_recv_packet: accept error: %s",
		    strerror(errno));
		return;
	}

	if (fcntl(newfd, F_SETFL, O_NONBLOCK) == -1) {
		log_debug("sess_recv_packet: unable to set non blocking flag");
		return;
	}

	if ((iface = session_find_iface(xconf, src.sin_addr)) == NULL) {
		log_debug("sess_recv_packet: cannot find a matching interface");
		return;
	}

	/* XXX */
	nbr = nbr_find_ip(iface, src.sin_addr.s_addr);
	if (nbr == NULL) {
		/* If there is no neighbor matching there is no
		   Hello adjacency: send notification */
		send_notification(S_NO_HELLO, iface, newfd, 0, 0);
		close(newfd);
		return;
	}

	nbr->fd = newfd;
	nbr_fsm(nbr, NBR_EVT_SESSION_UP);
}

void
session_read(int fd, short event, void *arg)
{
	struct nbr	*nbr = arg;
	struct iface	*iface = nbr->iface;
	struct ldp_hdr	*ldp_hdr;
	struct ldp_msg	*ldp_msg;
	char		*buf, *pdu;
	ssize_t		 n, len;
	int		 l, msg_size;
	u_int16_t	 pdu_len;

	if (event != EV_READ) {
		log_debug("session_read: spurious event");
		return;
	}

	if ((n = read(fd, nbr->rbuf->buf + nbr->rbuf->wpos,
	    sizeof(nbr->rbuf->buf) - nbr->rbuf->wpos)) == -1) {
		if (errno != EINTR && errno != EAGAIN) {
			/* XXX find better error */
			session_shutdown(nbr, S_SHUTDOWN, 0, 0);
			return;
		}
		/* retry read */
		return;
	}
	if (n == 0) {
		/* connection closed */
		event_del(&nbr->rev);
		session_shutdown(nbr, S_SHUTDOWN, 0, 0);
		return;
	}
	nbr->rbuf->wpos += n;

	while ((len = session_get_pdu(nbr->rbuf, &buf)) > 0) {
		ldp_hdr = (struct ldp_hdr *)pdu = buf;
		if (ntohs(ldp_hdr->version) != LDP_VERSION) {
			session_shutdown(nbr, S_BAD_PROTO_VER, 0, 0);
			free(buf);
			return;
		}

		pdu_len = ntohs(ldp_hdr->length);
		if (pdu_len < LDP_HDR_SIZE || pdu_len > LDP_MAX_LEN) {
			session_shutdown(nbr, S_BAD_MSG_LEN, 0, 0);
			free(buf);
			return;
		}

		if ((l = ldp_hdr_sanity_check(ldp_hdr, len, iface)) == -1) {
			session_shutdown(nbr, S_BAD_LDP_ID, 0, 0);
			free(buf);
			return;
		}

		pdu += LDP_HDR_SIZE;
		len -= LDP_HDR_SIZE;

		while (len >= LDP_MSG_LEN) {
			ldp_msg = (struct ldp_msg *)pdu;

			pdu_len = ntohs(ldp_msg->length) + TLV_HDR_LEN;
			if (pdu_len > len ||
			    pdu_len < LDP_MSG_LEN - TLV_HDR_LEN) {
				session_shutdown(nbr, S_BAD_TLV_LEN, 0, 0);
				free(buf);
				return;
			}

			/* switch LDP packet type */
			switch (ntohs(ldp_msg->type)) {
			case MSG_TYPE_NOTIFICATION:
				msg_size = recv_notification(nbr, pdu, pdu_len);
				break;
			case MSG_TYPE_INIT:
				msg_size = recv_init(nbr, pdu, pdu_len);
				break;
			case MSG_TYPE_KEEPALIVE:
				msg_size = recv_keepalive(nbr, pdu, pdu_len);
				break;
			case MSG_TYPE_ADDR:
				msg_size = recv_address(nbr, pdu, pdu_len);
				break;
			case MSG_TYPE_ADDRWITHDRAW:
				msg_size = recv_address_withdraw(nbr, pdu, pdu_len);
				break;
			case MSG_TYPE_LABELMAPPING:
				msg_size = recv_labelmapping(nbr, pdu, pdu_len);
				break;
			case MSG_TYPE_LABELREQUEST:
				msg_size = recv_labelrequest(nbr, pdu, pdu_len);
				break;
			case MSG_TYPE_LABELWITHDRAW:
				msg_size = recv_labelwithdraw(nbr, pdu, pdu_len);
				break;
			case MSG_TYPE_LABELRELEASE:
				msg_size = recv_labelrelease(nbr, pdu, pdu_len);
				break;
			case MSG_TYPE_LABELABORTREQ:
			case MSG_TYPE_HELLO:
			default:
				log_debug("session_read: unknown LDP packet type "
				    "interface %s", iface->name);
				free(buf);
				return;
			}

			if (msg_size == -1) {
				/* parser failed, giving up */
				free(buf);
				return;
			}

			/* Analyse the next message */
			pdu += msg_size + TLV_HDR_LEN;
			len -= msg_size + TLV_HDR_LEN;
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
	struct nbr *nbr = arg;

	if (event & EV_WRITE) {
		if (msgbuf_write(&nbr->wbuf.wbuf) == -1)
			nbr_fsm(nbr, NBR_EVT_CLOSE_SESSION);
	} else
		log_debug("session_write: spurious event");

	evbuf_event_add(&nbr->wbuf);
}

void
session_shutdown(struct nbr *nbr, u_int32_t status, u_int32_t msgid,
    u_int32_t type)
{
	send_notification_nbr(nbr, status, msgid, type);
	send_notification_nbr(nbr, S_SHUTDOWN, msgid, type);

	nbr_fsm(nbr, NBR_EVT_CLOSE_SESSION);
}

void
session_close(struct nbr *nbr)
{
	log_debug("session_close: closing session with nbr ID %s",
	    inet_ntoa(nbr->id));

	evbuf_clear(&nbr->wbuf);

	if (evtimer_pending(&nbr->keepalive_timer, NULL))
		evtimer_del(&nbr->keepalive_timer);
	if (evtimer_pending(&nbr->keepalive_timeout, NULL))
		evtimer_del(&nbr->keepalive_timeout);

	close(nbr->fd);
}

struct iface *
session_find_iface(struct ldpd_conf *xconf, struct in_addr src)
{
	struct iface	*iface = NULL;

	/* returned interface needs to be active */
	LIST_FOREACH(iface, &xconf->iface_list, entry) {
		switch (iface->type) {
		case IF_TYPE_VIRTUALLINK:
			if ((src.s_addr == iface->dst.s_addr) &&
			    !iface->passive)
				return (iface);
			break;
		case IF_TYPE_POINTOPOINT:
			if (iface->dst.s_addr == src.s_addr &&
			    !iface->passive)
				return (iface);
			break;
		default:
			if ((iface->addr.s_addr & iface->mask.s_addr) ==
			    (src.s_addr & iface->mask.s_addr) &&
			    !iface->passive)
				return (iface);
			break;
		}
	}

	return (NULL);
}

ssize_t
session_get_pdu(struct buf_read *r, char **b)
{
	struct ldp_hdr	l;
	size_t		av, dlen, left;

	av = r->wpos;
	if (av < sizeof(l))
		return (0);

	memcpy(&l, r->buf, sizeof(l));
	dlen = ntohs(l.length) + TLV_HDR_LEN;
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
