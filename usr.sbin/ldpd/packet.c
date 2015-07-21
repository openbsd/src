/*	$OpenBSD: packet.c,v 1.40 2015/07/21 04:39:28 renato Exp $ */

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

struct iface	*disc_find_iface(unsigned int, struct in_addr);
ssize_t		 session_get_pdu(struct ibuf_read *, char **);

static int	 msgcnt = 0;

int
gen_ldp_hdr(struct ibuf *buf, u_int16_t size)
{
	struct ldp_hdr	ldp_hdr;

	bzero(&ldp_hdr, sizeof(ldp_hdr));
	ldp_hdr.version = htons(LDP_VERSION);

	/* We want just the size of the value */
	size -= TLV_HDR_LEN;

	ldp_hdr.length = htons(size);
	ldp_hdr.lsr_id = ldpe_router_id();
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
			log_warn("send_packet: error setting multicast "
			    "interface, %s", iface->name);
			return (-1);
		}

	if (sendto(fd, pkt, len, 0, (struct sockaddr *)dst,
	    sizeof(*dst)) == -1) {
		log_warn("send_packet: error sending packet to %s",
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
		log_debug("recv_packet: unknown LDP packet type, source %s",
		    inet_ntoa(src.sin_addr));
	}
}

struct iface *
disc_find_iface(unsigned int ifindex, struct in_addr src)
{
	struct iface	*iface;
	struct if_addr	*if_addr;

	LIST_FOREACH(iface, &leconf->iface_list, entry)
		LIST_FOREACH(if_addr, &iface->addr_list, entry)
			switch (iface->type) {
			case IF_TYPE_POINTOPOINT:
				if (ifindex == iface->ifindex &&
				    if_addr->dstbrd.s_addr == src.s_addr)
					return (iface);
				break;
			default:
				if (ifindex == iface->ifindex &&
				    (if_addr->addr.s_addr &
					if_addr->mask.s_addr) ==
				    (src.s_addr & if_addr->mask.s_addr))
					return (iface);
				break;
			}

	return (NULL);
}

struct tcp_conn *
tcp_new(int fd, struct nbr *nbr)
{
	struct tcp_conn *tcp;

	if ((tcp = calloc(1, sizeof(*tcp))) == NULL)
		fatal("tcp_new");
	if ((tcp->rbuf = calloc(1, sizeof(struct ibuf_read))) == NULL)
		fatal("tcp_new");

	if (nbr)
		tcp->nbr = nbr;

	tcp->fd = fd;
	evbuf_init(&tcp->wbuf, tcp->fd, session_write, tcp);
	event_set(&tcp->rev, tcp->fd, EV_READ | EV_PERSIST, session_read, tcp);
	event_add(&tcp->rev, NULL);

	return (tcp);
}

void
tcp_close(struct tcp_conn *tcp)
{
	evbuf_clear(&tcp->wbuf);
	event_del(&tcp->rev);
	close(tcp->fd);
	free(tcp->rbuf);
	free(tcp);
}

void
session_accept(int fd, short event, void *bula)
{
	struct sockaddr_in	 src;
	int			 newfd;
	socklen_t		 len = sizeof(src);
	struct nbr_params	*nbrp;
	int			 opt;

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
			log_debug("sess_recv_packet: accept error: %s",
			    strerror(errno));
		return;
	}

	nbrp = nbr_params_find(src.sin_addr);
	if (nbrp && nbrp->auth.method == AUTH_MD5SIG) {
		if (sysdep.no_pfkey || sysdep.no_md5sig) {
			log_warnx("md5sig configured but not available");
			close(newfd);
			return;
		}

		len = sizeof(opt);
		if (getsockopt(newfd, IPPROTO_TCP, TCP_MD5SIG,
		    &opt, &len) == -1)
			fatal("getsockopt TCP_MD5SIG");
		if (!opt) {	/* non-md5'd connection! */
			log_warnx(
			    "connection attempt without md5 signature");
			close(newfd);
			return;
		}
	}

	tcp_new(newfd, NULL);
}

void
session_read(int fd, short event, void *arg)
{
	struct tcp_conn	*tcp = arg;
	struct nbr	*nbr = tcp->nbr;
	struct ldp_hdr	*ldp_hdr;
	struct ldp_msg	*ldp_msg;
	char		*buf, *pdu;
	ssize_t		 n, len;
	int		 msg_size;
	u_int16_t	 pdu_len;

	if (event != EV_READ) {
		log_debug("session_read: spurious event");
		return;
	}

	if ((n = read(fd, tcp->rbuf->buf + tcp->rbuf->wpos,
	    sizeof(tcp->rbuf->buf) - tcp->rbuf->wpos)) == -1) {
		if (errno != EINTR && errno != EAGAIN) {
			log_warn("session_read: read error");
			if (nbr)
				nbr_fsm(nbr, NBR_EVT_CLOSE_SESSION);
			else
				tcp_close(tcp);
			return;
		}
		/* retry read */
		return;
	}
	if (n == 0) {
		/* connection closed */
		log_debug("session_read: connection closed by remote end");
		if (nbr)
			nbr_fsm(nbr, NBR_EVT_CLOSE_SESSION);
		else
			tcp_close(tcp);
		return;
	}
	tcp->rbuf->wpos += n;

	while ((len = session_get_pdu(tcp->rbuf, &buf)) > 0) {
		pdu = buf;
		ldp_hdr = (struct ldp_hdr *)pdu;
		if (ntohs(ldp_hdr->version) != LDP_VERSION) {
			if (nbr)
				session_shutdown(nbr, S_BAD_PROTO_VER, 0, 0);
			else {
				send_notification(S_BAD_PROTO_VER, tcp, 0, 0);
				msgbuf_write(&tcp->wbuf.wbuf);
				tcp_close(tcp);
			}
			free(buf);
			return;
		}

		pdu_len = ntohs(ldp_hdr->length);
		if (pdu_len < (LDP_HDR_PDU_LEN + LDP_MSG_LEN) ||
		    pdu_len > LDP_MAX_LEN) {
			if (nbr)
				session_shutdown(nbr, S_BAD_PDU_LEN, 0, 0);
			else {
				send_notification(S_BAD_PDU_LEN, tcp, 0, 0);
				msgbuf_write(&tcp->wbuf.wbuf);
				tcp_close(tcp);
			}
			free(buf);
			return;
		}

		if (nbr) {
			if (ldp_hdr->lsr_id != nbr->id.s_addr ||
			    ldp_hdr->lspace_id != 0) {
				session_shutdown(nbr, S_BAD_LDP_ID, 0, 0);
				free(buf);
				return;
			}
		} else {
			nbr = nbr_find_ldpid(ldp_hdr->lsr_id);
			if (!nbr) {
				send_notification(S_NO_HELLO, tcp, 0, 0);
				msgbuf_write(&tcp->wbuf.wbuf);
				tcp_close(tcp);
				free(buf);
				return;
			}
			/* handle duplicate SYNs */
			if (nbr->tcp) {
				tcp_close(tcp);
				free(buf);
				return;
			}

			nbr->tcp = tcp;
			tcp->nbr = nbr;
			nbr_fsm(nbr, NBR_EVT_MATCH_ADJ);
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
			case MSG_TYPE_NOTIFICATION:
			case MSG_TYPE_ADDR:
			case MSG_TYPE_ADDRWITHDRAW:
			case MSG_TYPE_LABELMAPPING:
			case MSG_TYPE_LABELREQUEST:
			case MSG_TYPE_LABELWITHDRAW:
			case MSG_TYPE_LABELRELEASE:
			case MSG_TYPE_LABELABORTREQ:
			default:
				if (nbr->state != NBR_STA_OPER) {
					session_shutdown(nbr, S_SHUTDOWN,
					    ldp_msg->msgid, ldp_msg->type);
					free(buf);
					return;
				}
				break;
			}

			/* switch LDP packet type */
			switch (type) {
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
			case MSG_TYPE_ADDRWITHDRAW:
				msg_size = recv_address(nbr, pdu, pdu_len);
				break;
			case MSG_TYPE_LABELMAPPING:
			case MSG_TYPE_LABELREQUEST:
			case MSG_TYPE_LABELWITHDRAW:
			case MSG_TYPE_LABELRELEASE:
			case MSG_TYPE_LABELABORTREQ:
				msg_size = recv_labelmessage(nbr, pdu,
				    pdu_len, type);
				break;
			default:
				log_debug("session_read: unknown LDP packet "
				    "from nbr %s", inet_ntoa(nbr->id));
				if (!(ntohs(ldp_msg->type) & UNKNOWN_FLAG)) {
					session_shutdown(nbr, S_UNKNOWN_MSG,
					    ldp_msg->msgid, ldp_msg->type);
					free(buf);
					return;
				}
				/* unknown flag is set, ignore the message */
				msg_size = ntohs(ldp_msg->length);
				break;
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
	struct tcp_conn *tcp = arg;
	struct nbr *nbr = tcp->nbr;

	if (event & EV_WRITE) {
		if (msgbuf_write(&tcp->wbuf.wbuf) <= 0 && errno != EAGAIN) {
			if (nbr)
				nbr_fsm(nbr, NBR_EVT_CLOSE_SESSION);
		}
	} else
		log_debug("session_write: spurious event");

	evbuf_event_add(&tcp->wbuf);
}

void
session_shutdown(struct nbr *nbr, u_int32_t status, u_int32_t msgid,
    u_int32_t type)
{
	log_debug("session_shutdown: nbr ID %s", inet_ntoa(nbr->id));

	send_notification_nbr(nbr, status, msgid, type);

	/* try to flush write buffer, if it fails tough shit */
	msgbuf_write(&nbr->tcp->wbuf.wbuf);

	nbr_fsm(nbr, NBR_EVT_CLOSE_SESSION);
}

void
session_close(struct nbr *nbr)
{
	log_debug("session_close: closing session with nbr ID %s",
	    inet_ntoa(nbr->id));

	tcp_close(nbr->tcp);
	nbr->tcp = NULL;

	nbr_stop_ktimer(nbr);
	nbr_stop_ktimeout(nbr);

	accept_unpause();
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
