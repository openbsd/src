/*	$OpenBSD: log.c,v 1.3 2011/01/04 10:36:31 claudio Exp $ */

/*
 * Copyright (c) 2009 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <scsi/iscsi.h>

#include <errno.h>
#include <event.h>
#include <netdb.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "iscsid.h"
#include "log.h"

int	debug;

void	 logit(int, const char *, ...);

void
log_init(int n_debug)
{
	extern char	*__progname;

	debug = n_debug;

	if (!debug)
		openlog(__progname, LOG_PID | LOG_NDELAY, LOG_DAEMON);

	tzset();
}

void
logit(int pri, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	vlog(pri, fmt, ap);
	va_end(ap);
}

void
vlog(int pri, const char *fmt, va_list ap)
{
	char	*nfmt;

	if (debug) {
		/* best effort in out of mem situations */
		if (asprintf(&nfmt, "%s\n", fmt) == -1) {
			vfprintf(stderr, fmt, ap);
			fprintf(stderr, "\n");
		} else {
			vfprintf(stderr, nfmt, ap);
			free(nfmt);
		}
		fflush(stderr);
	} else
		vsyslog(pri, fmt, ap);
}

void
log_warn(const char *emsg, ...)
{
	char	*nfmt;
	va_list	 ap;

	/* best effort to even work in out of memory situations */
	if (emsg == NULL)
		logit(LOG_CRIT, "%s", strerror(errno));
	else {
		va_start(ap, emsg);

		if (asprintf(&nfmt, "%s: %s", emsg, strerror(errno)) == -1) {
			/* we tried it... */
			vlog(LOG_CRIT, emsg, ap);
			logit(LOG_CRIT, "%s", strerror(errno));
		} else {
			vlog(LOG_CRIT, nfmt, ap);
			free(nfmt);
		}
		va_end(ap);
	}
}

void
log_warnx(const char *emsg, ...)
{
	va_list	 ap;

	va_start(ap, emsg);
	vlog(LOG_CRIT, emsg, ap);
	va_end(ap);
}

void
log_info(const char *emsg, ...)
{
	va_list	 ap;

	va_start(ap, emsg);
	vlog(LOG_INFO, emsg, ap);
	va_end(ap);
}

void
log_debug(const char *emsg, ...)
{
	va_list	 ap;

	if (debug) {
		va_start(ap, emsg);
		vlog(LOG_DEBUG, emsg, ap);
		va_end(ap);
	}
}

void
fatal(const char *emsg)
{
	if (emsg == NULL)
		logit(LOG_CRIT, "fatal: %s", strerror(errno));
	else
		if (errno)
			logit(LOG_CRIT, "fatal: %s: %s", emsg, strerror(errno));
		else
			logit(LOG_CRIT, "fatal: %s", emsg);

	exit(1);
}

void
fatalx(const char *emsg)
{
	errno = 0;
	fatal(emsg);
}

void
log_hexdump(void *buf, size_t len)
{
	u_char b[16];
	size_t i, j, l;

	if (!debug)
		return;

	for (i = 0; i < len; i += l) {
		fprintf(stderr, "%4zi:", i);
		l = sizeof(b) < len - i ? sizeof(b) : len - i;
		bcopy((char *)buf + i, b, l);

		for (j = 0; j < sizeof(b); j++) {
			if (j % 2 == 0)
				fprintf(stderr, " ");
			if (j % 8 == 0)
				fprintf(stderr, " ");
			if (j < l)
				fprintf(stderr, "%02x", (int)b[j]);
			else
				fprintf(stderr, "  ");
		}
		fprintf(stderr, "  |");
		for (j = 0; j < l; j++) {
			if (b[j] >= 0x20 && b[j] <= 0x7e)
				fprintf(stderr, "%c", b[j]);
			else
				fprintf(stderr, ".");
		}
		fprintf(stderr, "|\n");
	}
}

void
log_pdu(struct pdu *p, int all)
{
	struct iscsi_pdu *pdu;
	void *b;
	size_t s;

	if (!debug)
		return;

	if (!(pdu = pdu_getbuf(p, NULL, PDU_HEADER))) {
		log_debug("empty pdu");
		return;
	}

	fprintf(stderr, "PDU: op %x%s flags %02x%02x%02x ahs %d len %d\n",
		ISCSI_PDU_OPCODE(pdu->opcode), ISCSI_PDU_I(pdu) ? " I" : "",
		pdu->flags, pdu->_reserved1[0], pdu->_reserved1[1],
		pdu->ahslen, pdu->datalen[0] << 16 | pdu->datalen[1] << 8 |
		pdu->datalen[2]);
	fprintf(stderr, "     lun %02x%02x%02x%02x%02x%02x%02x%02x itt %u "
	    "cmdsn %u expstatsn %u\n", pdu->lun[0], pdu->lun[1], pdu->lun[2],
	    pdu->lun[3], pdu->lun[4], pdu->lun[5], pdu->lun[6], pdu->lun[7],
	    ntohl(pdu->itt), ntohl(pdu->cmdsn), ntohl(pdu->expstatsn));
	log_hexdump(pdu, sizeof(*pdu));

	if (all && (b = pdu_getbuf(p, &s, PDU_DATA)))
		log_hexdump(b, s);
}
