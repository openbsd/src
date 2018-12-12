/*	$OpenBSD: lka_report.c,v 1.14 2018/12/12 21:27:49 gilles Exp $	*/

/*
 * Copyright (c) 2018 Gilles Chehade <gilles@poolp.org>
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
#include <sys/tree.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "smtpd.h"
#include "log.h"

#define	PROTOCOL_VERSION	1

static void
report_smtp_broadcast(const char *direction, time_t tm, const char *format, ...)
{
	va_list		ap;
	void		*hdl = NULL;
	const char	*reporter;
	struct dict	*d;

	if (strcmp("smtp-in", direction) == 0)
		d = env->sc_smtp_reporters_dict;
	if (strcmp("smtp-out", direction) == 0)
		d = env->sc_mta_reporters_dict;

	va_start(ap, format);
	while (dict_iter(d, &hdl, &reporter, NULL)) {
		if (io_printf(lka_proc_get_io(reporter), "report|%d|%zd|%s|",
			PROTOCOL_VERSION, tm, direction) == -1 ||
		    io_vprintf(lka_proc_get_io(reporter), format, ap) == -1)
			fatalx("failed to write to processor");
	}
	va_end(ap);
}

void
lka_report_smtp_link_connect(const char *direction, time_t tm, uint64_t reqid, const char *rdns,
    int fcrdns,
    const struct sockaddr_storage *ss_src,
    const struct sockaddr_storage *ss_dest)
{
	char	src[NI_MAXHOST + 5];
	char	dest[NI_MAXHOST + 5];
	uint16_t	src_port = 0;
	uint16_t	dest_port = 0;
	const char     *fcrdns_str;

	if (ss_src->ss_family == AF_INET)
		src_port = ntohs(((const struct sockaddr_in *)ss_src)->sin_port);
	else if (ss_src->ss_family == AF_INET6)
		src_port = ntohs(((const struct sockaddr_in6 *)ss_src)->sin6_port);

	if (ss_dest->ss_family == AF_INET)
		dest_port = ntohs(((const struct sockaddr_in *)ss_dest)->sin_port);
	else if (ss_dest->ss_family == AF_INET6)
		dest_port = ntohs(((const struct sockaddr_in6 *)ss_dest)->sin6_port);

	(void)strlcpy(src, ss_to_text(ss_src), sizeof src);
	(void)strlcpy(dest, ss_to_text(ss_dest), sizeof dest);

	switch (fcrdns) {
	case 1:
		fcrdns_str = "pass";
		break;
	case 0:
		fcrdns_str = "fail";
		break;
	default:
		fcrdns_str = "error";
		break;
	}
	
	report_smtp_broadcast(direction, tm,
	    "link-connect|%016"PRIx64"|%s|%s|%s:%d|%s:%d\n",
	    reqid, rdns, fcrdns_str, src, src_port, dest, dest_port);
}

void
lka_report_smtp_link_disconnect(const char *direction, time_t tm, uint64_t reqid)
{
	report_smtp_broadcast(direction, tm,
	    "link-disconnect|%016"PRIx64"\n", reqid);
}

void
lka_report_smtp_link_identify(const char *direction, time_t tm, uint64_t reqid, const char *heloname)
{
	report_smtp_broadcast(direction, tm,
	    "link-identify|%016"PRIx64"|%s\n", reqid, heloname);
}

void
lka_report_smtp_link_tls(const char *direction, time_t tm, uint64_t reqid, const char *ciphers)
{
	report_smtp_broadcast(direction, tm,
	    "link-tls|%016"PRIx64"|%s\n", reqid, ciphers);
}

void
lka_report_smtp_tx_begin(const char *direction, time_t tm, uint64_t reqid, uint32_t msgid)
{
	report_smtp_broadcast(direction, tm,
	    "tx-begin|%016"PRIx64"|%08x\n", reqid, msgid);
}

void
lka_report_smtp_tx_mail(const char *direction, time_t tm, uint64_t reqid, uint32_t msgid, const char *address, int ok)
{
	const char *result;

	switch (ok) {
	case 1:
		result = "ok";
		break;
	case 0:
		result = "permfail";
		break;
	default:
		result = "tempfail";
		break;
	}
	report_smtp_broadcast(direction, tm,
	    "tx-mail|%016"PRIx64"|%08x|%s|%s\n", reqid, msgid, address, result);
}

void
lka_report_smtp_tx_rcpt(const char *direction, time_t tm, uint64_t reqid, uint32_t msgid, const char *address, int ok)
{
	const char *result;

	switch (ok) {
	case 1:
		result = "ok";
		break;
	case 0:
		result = "permfail";
		break;
	default:
		result = "tempfail";
		break;
	}
	report_smtp_broadcast(direction, tm,
	    "tx-rcpt|%016"PRIx64"|%08x|%s|%s\n", reqid, msgid, address, result);
}

void
lka_report_smtp_tx_envelope(const char *direction, time_t tm, uint64_t reqid, uint32_t msgid, uint64_t evpid)
{
	report_smtp_broadcast(direction, tm,
	    "tx-envelope|%016"PRIx64"|%08x|%016"PRIx64"\n",
	    reqid, msgid, evpid);
}

void
lka_report_smtp_tx_data(const char *direction, time_t tm, uint64_t reqid, uint32_t msgid, int ok)
{
	const char *result;

	switch (ok) {
	case 1:
		result = "ok";
		break;
	case 0:
		result = "permfail";
		break;
	default:
		result = "tempfail";
		break;
	}
	report_smtp_broadcast(direction, tm,
	    "tx-data|%016"PRIx64"|%08x|%s\n", reqid, msgid, result);
}

void
lka_report_smtp_tx_commit(const char *direction, time_t tm, uint64_t reqid, uint32_t msgid, size_t msgsz)
{
	report_smtp_broadcast(direction, tm,
	    "tx-commit|%016"PRIx64"|%08x|%zd\n",
	    reqid, msgid, msgsz);
}

void
lka_report_smtp_tx_rollback(const char *direction, time_t tm, uint64_t reqid, uint32_t msgid)
{
	report_smtp_broadcast(direction, tm,
	    "tx-rollback|%016"PRIx64"|%08x\n",
	    reqid, msgid);
}

void
lka_report_smtp_protocol_client(const char *direction, time_t tm, uint64_t reqid, const char *command)
{
	report_smtp_broadcast(direction, tm,
	    "protocol-client|%016"PRIx64"|%s\n",
	    reqid, command);
}

void
lka_report_smtp_protocol_server(const char *direction, time_t tm, uint64_t reqid, const char *response)
{
	report_smtp_broadcast(direction, tm,
	    "protocol-server|%016"PRIx64"|%s\n",
	    reqid, response);
}

void
lka_report_smtp_filter_response(const char *direction, time_t tm, uint64_t reqid,
    int phase, int response, const char *param)
{
	const char *phase_name;
	const char *response_name;

	switch (phase) {
	case FILTER_CONNECTED:
		phase_name = "connected";
		break;
	case FILTER_HELO:
		phase_name = "helo";
		break;
	case FILTER_EHLO:
		phase_name = "ehlo";
		break;
	case FILTER_STARTTLS:
		phase_name = "tls";
		break;
	case FILTER_AUTH:
		phase_name = "auth";
		break;
	case FILTER_MAIL_FROM:
		phase_name = "mail-from";
		break;
	case FILTER_RCPT_TO:
		phase_name = "rcpt-to";
		break;
	case FILTER_DATA:
		phase_name = "data";
		break;
	case FILTER_DATA_LINE:
		phase_name = "data-line";
		break;
	case FILTER_RSET:
		phase_name = "rset";
		break;
	case FILTER_QUIT:
		phase_name = "quit";
		break;
	case FILTER_NOOP:
		phase_name = "noop";
		break;
	case FILTER_HELP:
		phase_name = "help";
		break;
	case FILTER_WIZ:
		phase_name = "wiz";
		break;
	case FILTER_COMMIT:
		phase_name = "commit";
		break;
	default:
		phase_name = "";
	}

	switch (response) {
	case FILTER_PROCEED:
		response_name = "proceed";
		break;
	case FILTER_REWRITE:
		response_name = "rewrite";
		break;
	case FILTER_REJECT:
		response_name = "reject";
		break;
	case FILTER_DISCONNECT:
		response_name = "disconnect";
		break;
	default:
		response_name = "";
	}

	report_smtp_broadcast(direction, tm,
	    "filter-response|%016"PRIx64"|%s|%s|%s\n",
	    reqid, phase_name, response_name, param ? param : "");
}
