/*	$OpenBSD: ntp_msg.c,v 1.11 2004/10/22 21:24:20 henning Exp $ */

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
	memcpy(&msg->rootdelay.int_part, p, sizeof(msg->rootdelay.int_part));
	p += sizeof(msg->rootdelay.int_part);
	memcpy(&msg->rootdelay.fraction, p, sizeof(msg->rootdelay.fraction));
	p += sizeof(msg->rootdelay.fraction);
	memcpy(&msg->dispersion.int_part, p, sizeof(msg->dispersion.int_part));
	p += sizeof(msg->dispersion.int_part);
	memcpy(&msg->dispersion.fraction, p, sizeof(msg->dispersion.fraction));
	p += sizeof(msg->dispersion.fraction);
	memcpy(&msg->refid, p, sizeof(msg->refid));
	p += sizeof(msg->refid);
	memcpy(&msg->reftime.int_part, p, sizeof(msg->reftime.int_part));
	p += sizeof(msg->reftime.int_part);
	memcpy(&msg->reftime.fraction, p, sizeof(msg->reftime.fraction));
	p += sizeof(msg->reftime.fraction);
	memcpy(&msg->orgtime.int_part, p, sizeof(msg->orgtime.int_part));
	p += sizeof(msg->orgtime.int_part);
	memcpy(&msg->orgtime.fraction, p, sizeof(msg->orgtime.fraction));
	p += sizeof(msg->orgtime.fraction);
	memcpy(&msg->rectime.int_part, p, sizeof(msg->rectime.int_part));
	p += sizeof(msg->rectime.int_part);
	memcpy(&msg->rectime.fraction, p, sizeof(msg->rectime.fraction));
	p += sizeof(msg->rectime.fraction);
	memcpy(&msg->xmttime.int_part, p, sizeof(msg->xmttime.int_part));
	p += sizeof(msg->xmttime.int_part);
	memcpy(&msg->xmttime.fraction, p, sizeof(msg->xmttime.fraction));
	p += sizeof(msg->xmttime.fraction);

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
	memcpy(p, &msg->rootdelay.int_part, sizeof(msg->rootdelay.int_part));
	p += sizeof(msg->rootdelay.int_part);
	memcpy(p, &msg->rootdelay.fraction, sizeof(msg->rootdelay.fraction));
	p += sizeof(msg->rootdelay.fraction);
	memcpy(p, &msg->dispersion.int_part, sizeof(msg->dispersion.int_part));
	p += sizeof(msg->dispersion.int_part);
	memcpy(p, &msg->dispersion.fraction, sizeof(msg->dispersion.fraction));
	p += sizeof(msg->dispersion.fraction);
	memcpy(p, &msg->refid, sizeof(msg->refid));
	p += sizeof(msg->refid);
	memcpy(p, &msg->reftime.int_part, sizeof(msg->reftime.int_part));
	p += sizeof(msg->reftime.int_part);
	memcpy(p, &msg->reftime.fraction, sizeof(msg->reftime.fraction));
	p += sizeof(msg->reftime.fraction);
	memcpy(p, &msg->orgtime.int_part, sizeof(msg->orgtime.int_part));
	p += sizeof(msg->orgtime.int_part);
	memcpy(p, &msg->orgtime.fraction, sizeof(msg->orgtime.fraction));
	p += sizeof(msg->orgtime.fraction);
	memcpy(p, &msg->rectime.int_part, sizeof(msg->rectime.int_part));
	p += sizeof(msg->rectime.int_part);
	memcpy(p, &msg->rectime.fraction, sizeof(msg->rectime.fraction));
	p += sizeof(msg->rectime.fraction);
	memcpy(p, &msg->xmttime.int_part, sizeof(msg->xmttime.int_part));
	p += sizeof(msg->xmttime.int_part);
	memcpy(p, &msg->xmttime.fraction, sizeof(msg->xmttime.fraction));
	p += sizeof(msg->xmttime.fraction);

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
