/*	$OpenBSD: ikev1.c,v 1.10 2011/05/09 11:15:18 reyk Exp $	*/
/*	$vantronix: ikev1.c,v 1.13 2010/05/28 15:34:35 reyk Exp $	*/

/*
 * Copyright (c) 2010 Reyk Floeter <reyk@vantronix.net>
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
 * XXX Either implement IKEv1,
 * XXX or find a way to pass IKEv1 messages to isakmpd,
 * XXX or remove this file and ikev1 from the iked tree.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/uio.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>
#include <err.h>
#include <pwd.h>
#include <event.h>

#include "iked.h"
#include "ikev2.h"

int	 ikev1_dispatch_parent(int, struct privsep_proc *, struct imsg *);
int	 ikev1_dispatch_ikev2(int, struct privsep_proc *, struct imsg *);
int	 ikev1_dispatch_cert(int, struct privsep_proc *, struct imsg *);

void	 ikev1_msg_cb(int, short, void *);
void	 ikev1_recv(struct iked *, struct iked_message *);

static struct privsep_proc procs[] = {
	{ "parent",	PROC_PARENT,	ikev1_dispatch_parent },
	{ "ikev2",	PROC_IKEV2,	ikev1_dispatch_ikev2 },
	{ "certstore",	PROC_CERT,	ikev1_dispatch_cert }
};

pid_t
ikev1(struct privsep *ps, struct privsep_proc *p)
{
	return (proc_run(ps, p, procs, nitems(procs), NULL, NULL));
}

int
ikev1_dispatch_parent(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct iked		*env = p->p_env;

	switch (imsg->hdr.type) {
	case IMSG_CTL_RESET:
		log_debug("%s: config reload", __func__);
		return (0);
	case IMSG_CTL_COUPLE:
	case IMSG_CTL_DECOUPLE:
		return (0);
	case IMSG_CTL_ACTIVE:
	case IMSG_CTL_PASSIVE:
		return (0);
	case IMSG_UDP_SOCKET:
		return (config_getsocket(env, imsg, ikev1_msg_cb));
	case IMSG_COMPILE:
		return (0);
	default:
		break;
	}

	return (-1);
}

int
ikev1_dispatch_ikev2(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	struct iked		*env = p->p_env;
	struct iked_message	 msg;
	u_int8_t		*buf;
	ssize_t			 len;

	switch (imsg->hdr.type) {
	case IMSG_IKE_MESSAGE:
		log_debug("%s: message", __func__);
		IMSG_SIZE_CHECK(imsg, &msg);
		memcpy(&msg, imsg->data, sizeof(msg));

		len = IMSG_DATA_SIZE(imsg) - sizeof(msg);
		buf = (u_int8_t *)imsg->data + sizeof(msg);
		if (len <= 0 || (msg.msg_data = ibuf_new(buf, len)) == NULL) {
			log_debug("%s: short message", __func__);
			return (0);
		}

		log_debug("%s: message length %d", __func__, len);

		ikev1_recv(env, &msg);
		ikev2_msg_cleanup(env, &msg);
		return (0);
	default:
		break;
	}

	return (-1);
}

int
ikev1_dispatch_cert(int fd, struct privsep_proc *p, struct imsg *imsg)
{
	return (-1);
}

void
ikev1_msg_cb(int fd, short event, void *arg)
{
	struct iked_socket	*sock = arg;
	struct iked		*env = sock->sock_env;
	struct iked_message	 msg;
	struct ike_header	 hdr;
	u_int8_t		 buf[IKED_MSGBUF_MAX];
	size_t			 len;
	struct iovec		 iov[2];

	msg.msg_peerlen = sizeof(msg.msg_peer);
	msg.msg_locallen = sizeof(msg.msg_local);

	if ((len = recvfromto(fd, buf, sizeof(buf), 0,
	    (struct sockaddr*)&msg.msg_peer, &msg.msg_peerlen,
	    (struct sockaddr*)&msg.msg_local, &msg.msg_locallen)) < 1)
		return;

	if ((size_t)len <= sizeof(hdr))
		return;
	memcpy(&hdr, buf, sizeof(hdr));

	if ((msg.msg_data = ibuf_new(buf, len)) == NULL)
		return;

	if (hdr.ike_version == IKEV2_VERSION) {
		iov[0].iov_base = &msg;
		iov[0].iov_len = sizeof(msg);
		iov[1].iov_base = buf;
		iov[1].iov_len = len;

		proc_composev_imsg(env, PROC_IKEV2, IMSG_IKE_MESSAGE, -1,
		    iov, 2);
		goto done;
	}

	ikev1_recv(env, &msg);

 done:
	ikev2_msg_cleanup(env, &msg);
}

void
ikev1_recv(struct iked *env, struct iked_message *msg)
{
	struct ike_header	*hdr;

	if (ibuf_size(msg->msg_data) <= sizeof(*hdr)) {
		log_debug("%s: short message", __func__);
		return;
	}

	hdr = (struct ike_header *)ibuf_data(msg->msg_data);

	log_debug("%s: header ispi %s rspi %s"
	    " nextpayload %u version 0x%02x exchange %u flags 0x%02x"
	    " msgid %u length %u", __func__,
	    print_spi(betoh64(hdr->ike_ispi), 8),
	    print_spi(betoh64(hdr->ike_rspi), 8),
	    hdr->ike_nextpayload,
	    hdr->ike_version,
	    hdr->ike_exchange,
	    hdr->ike_flags,
	    betoh32(hdr->ike_msgid),
	    betoh32(hdr->ike_length));

	log_debug("%s: IKEv1 not supported", __func__);
}
