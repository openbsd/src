/*	$OpenBSD: ntp_msg.c,v 1.12 2004/12/08 15:47:38 mickey Exp $ */

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

	memcpy(&msg->status, p, sizeof(msg->status));
	p += sizeof(msg->status);
	memcpy(&msg->stratum, p, sizeof(msg->stratum));
	p += sizeof(msg->stratum);
	memcpy(&msg->ppoll, p, sizeof(msg->ppoll));
	p += sizeof(msg->ppoll);
	memcpy(&msg->precision, p, sizeof(msg->precision));
	p += sizeof(msg->precision);
	memcpy(&msg->rootdelay.int_parts, p, sizeof(msg->rootdelay.int_parts));
	p += sizeof(msg->rootdelay.int_parts);
	memcpy(&msg->rootdelay.fractions, p, sizeof(msg->rootdelay.fractions));
	p += sizeof(msg->rootdelay.fractions);
	memcpy(&msg->dispersion.int_parts, p, sizeof(msg->dispersion.int_parts));
	p += sizeof(msg->dispersion.int_parts);
	memcpy(&msg->dispersion.fractions, p, sizeof(msg->dispersion.fractions));
	p += sizeof(msg->dispersion.fractions);
	memcpy(&msg->refid, p, sizeof(msg->refid));
	p += sizeof(msg->refid);
	memcpy(&msg->reftime.int_partl, p, sizeof(msg->reftime.int_partl));
	p += sizeof(msg->reftime.int_partl);
	memcpy(&msg->reftime.fractionl, p, sizeof(msg->reftime.fractionl));
	p += sizeof(msg->reftime.fractionl);
	memcpy(&msg->orgtime.int_partl, p, sizeof(msg->orgtime.int_partl));
	p += sizeof(msg->orgtime.int_partl);
	memcpy(&msg->orgtime.fractionl, p, sizeof(msg->orgtime.fractionl));
	p += sizeof(msg->orgtime.fractionl);
	memcpy(&msg->rectime.int_partl, p, sizeof(msg->rectime.int_partl));
	p += sizeof(msg->rectime.int_partl);
	memcpy(&msg->rectime.fractionl, p, sizeof(msg->rectime.fractionl));
	p += sizeof(msg->rectime.fractionl);
	memcpy(&msg->xmttime.int_partl, p, sizeof(msg->xmttime.int_partl));
	p += sizeof(msg->xmttime.int_partl);
	memcpy(&msg->xmttime.fractionl, p, sizeof(msg->xmttime.fractionl));
	p += sizeof(msg->xmttime.fractionl);

	return (0);
}

int
ntp_sendmsg(int fd, struct sockaddr *sa, struct ntp_msg *msg, ssize_t len,
    int auth)
{
	char		 buf[NTP_MSGSIZE];
	char		*p;
	u_int8_t	sa_len;

	p = buf;
	memcpy(p, &msg->status, sizeof(msg->status));
	p += sizeof(msg->status);
	memcpy(p, &msg->stratum, sizeof(msg->stratum));
	p += sizeof(msg->stratum);
	memcpy(p, &msg->ppoll, sizeof(msg->ppoll));
	p += sizeof(msg->ppoll);
	memcpy(p, &msg->precision, sizeof(msg->precision));
	p += sizeof(msg->precision);
	memcpy(p, &msg->rootdelay.int_parts, sizeof(msg->rootdelay.int_parts));
	p += sizeof(msg->rootdelay.int_parts);
	memcpy(p, &msg->rootdelay.fractions, sizeof(msg->rootdelay.fractions));
	p += sizeof(msg->rootdelay.fractions);
	memcpy(p, &msg->dispersion.int_parts, sizeof(msg->dispersion.int_parts));
	p += sizeof(msg->dispersion.int_parts);
	memcpy(p, &msg->dispersion.fractions, sizeof(msg->dispersion.fractions));
	p += sizeof(msg->dispersion.fractions);
	memcpy(p, &msg->refid, sizeof(msg->refid));
	p += sizeof(msg->refid);
	memcpy(p, &msg->reftime.int_partl, sizeof(msg->reftime.int_partl));
	p += sizeof(msg->reftime.int_partl);
	memcpy(p, &msg->reftime.fractionl, sizeof(msg->reftime.fractionl));
	p += sizeof(msg->reftime.fractionl);
	memcpy(p, &msg->orgtime.int_partl, sizeof(msg->orgtime.int_partl));
	p += sizeof(msg->orgtime.int_partl);
	memcpy(p, &msg->orgtime.fractionl, sizeof(msg->orgtime.fractionl));
	p += sizeof(msg->orgtime.fractionl);
	memcpy(p, &msg->rectime.int_partl, sizeof(msg->rectime.int_partl));
	p += sizeof(msg->rectime.int_partl);
	memcpy(p, &msg->rectime.fractionl, sizeof(msg->rectime.fractionl));
	p += sizeof(msg->rectime.fractionl);
	memcpy(p, &msg->xmttime.int_partl, sizeof(msg->xmttime.int_partl));
	p += sizeof(msg->xmttime.int_partl);
	memcpy(p, &msg->xmttime.fractionl, sizeof(msg->xmttime.fractionl));
	p += sizeof(msg->xmttime.fractionl);

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
