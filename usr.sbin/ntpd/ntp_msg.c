/*	$OpenBSD: ntp_msg.c,v 1.14 2004/12/14 06:27:13 dtucker Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2004 Alexander Guy <alexander.guy@andern.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "ntpd.h"
#include "ntp.h"

int
ntp_getmsg(char *p, ssize_t len, struct ntp_msg *msg)
{
	if (len != NTP_MSGSIZE_NOAUTH && len != NTP_MSGSIZE) {
		log_warnx("malformed packet received");
		return (-1);
	}

#define	copyin(f,p)	memcpy(&(f), (p), sizeof(f)); (p) += sizeof(f)

	copyin(msg->status, p);
	copyin(msg->stratum, p);
	copyin(msg->ppoll, p);
	copyin(msg->precision, p);
	copyin(msg->rootdelay.int_parts, p);
	copyin(msg->rootdelay.fractions, p);
	copyin(msg->dispersion.int_parts, p);
	copyin(msg->dispersion.fractions, p);
	copyin(msg->refid, p);
	copyin(msg->reftime.int_partl, p);
	copyin(msg->reftime.fractionl, p);
	copyin(msg->orgtime.int_partl, p);
	copyin(msg->orgtime.fractionl, p);
	copyin(msg->rectime.int_partl, p);
	copyin(msg->rectime.fractionl, p);
	copyin(msg->xmttime.int_partl, p);
	copyin(msg->xmttime.fractionl, p);

	return (0);
}

int
ntp_sendmsg(int fd, struct sockaddr *sa, struct ntp_msg *msg, ssize_t len,
    int auth)
{
	char		 buf[NTP_MSGSIZE];
	char		*p = buf;
	socklen_t	sa_len;

#define	copyout(p,f)	memcpy((p), &(f), sizeof(f)); p += sizeof(f)

	copyout(p, msg->status);
	copyout(p, msg->stratum);
	copyout(p, msg->ppoll);
	copyout(p, msg->precision);
	copyout(p, msg->rootdelay.int_parts);
	copyout(p, msg->rootdelay.fractions);
	copyout(p, msg->dispersion.int_parts);
	copyout(p, msg->dispersion.fractions);
	copyout(p, msg->refid);
	copyout(p, msg->reftime.int_partl);
	copyout(p, msg->reftime.fractionl);
	copyout(p, msg->orgtime.int_partl);
	copyout(p, msg->orgtime.fractionl);
	copyout(p, msg->rectime.int_partl);
	copyout(p, msg->rectime.fractionl);
	copyout(p, msg->xmttime.int_partl);
	copyout(p, msg->xmttime.fractionl);

	if (sa != NULL)
		sa_len = SA_LEN(sa);
	else
		sa_len = 0;

	if (sendto(fd, &buf, len, 0, sa, sa_len) != len) {
		if (errno == ENOBUFS || errno == EHOSTUNREACH ||
		    errno == ENETDOWN || errno == EHOSTDOWN) {
			/* logging is futile */
			return (-1);
		}
		log_warn("sendto");
		return (-1);
	}

	return (0);
}
