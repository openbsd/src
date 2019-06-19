/*	$OpenBSD: lka_report.c,v 1.17 2019/01/05 09:43:39 gilles Exp $	*/

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

struct reporter_proc {
	TAILQ_ENTRY(reporter_proc)	entries;
	const char		       *name;
};
TAILQ_HEAD(reporters, reporter_proc);

static struct dict	smtp_in;
static struct dict	smtp_out;

static struct smtp_events {
	const char     *event;
} smtp_events[] = {
	{ "link-connect" },
	{ "link-disconnect" },
	{ "link-identify" },
	{ "link-tls" },

	{ "tx-begin" },
	{ "tx-mail" },
	{ "tx-rcpt" },
	{ "tx-envelope" },
	{ "tx-data" },
	{ "tx-commit" },
	{ "tx-rollback" },

	{ "protocol-client" },
	{ "protocol-server" },

	{ "filter-response" },

	{ "timeout" },
};


void
lka_report_init(void)
{
	struct reporters	*tailq;
	size_t			 i;

	dict_init(&smtp_in);
	dict_init(&smtp_out);

	for (i = 0; i < nitems(smtp_events); ++i) {
		tailq = xcalloc(1, sizeof (struct reporters *));
		TAILQ_INIT(tailq);
		dict_xset(&smtp_in, smtp_events[i].event, tailq);

		tailq = xcalloc(1, sizeof (struct reporters *));
		TAILQ_INIT(tailq);
		dict_xset(&smtp_out, smtp_events[i].event, tailq);
	}
}

void
lka_report_register_hook(const char *name, const char *hook)
{
	struct dict	*subsystem;
	struct reporter_proc	*rp;
	struct reporters	*tailq;
	void *iter;
	size_t	i;

	if (strncasecmp(hook, "smtp-in|", 8) == 0) {
		subsystem = &smtp_in;
		hook += 8;
	}
	else if (strncasecmp(hook, "smtp-out|", 9) == 0) {
		subsystem = &smtp_out;
		hook += 9;
	}
	else
		return;

	if (strcmp(hook, "*") == 0) {
		iter = NULL;
		while (dict_iter(subsystem, &iter, NULL, (void **)&tailq)) {
			rp = xcalloc(1, sizeof *rp);
			rp->name = xstrdup(name);
			TAILQ_INSERT_TAIL(tailq, rp, entries);
		}
		return;
	}

	for (i = 0; i < nitems(smtp_events); i++)
		if (strcmp(hook, smtp_events[i].event) == 0)
			break;
	if (i == nitems(smtp_events))
		return;

	tailq = dict_get(subsystem, hook);
	rp = xcalloc(1, sizeof *rp);
	rp->name = xstrdup(name);
	TAILQ_INSERT_TAIL(tailq, rp, entries);
}

static void
report_smtp_broadcast(uint64_t reqid, const char *direction, struct timeval *tv, const char *event,
    const char *format, ...)
{
	va_list		ap;
	struct dict	*d;
	struct reporters	*tailq;
	struct reporter_proc	*rp;

	if (strcmp("smtp-in", direction) == 0)
		d = &smtp_in;

	if (strcmp("smtp-out", direction) == 0)
		d = &smtp_out;

	tailq = dict_xget(d, event);
	TAILQ_FOREACH(rp, tailq, entries) {
		if (!lka_filter_proc_in_session(reqid, rp->name))
			continue;

		va_start(ap, format);
		if (io_printf(lka_proc_get_io(rp->name), "report|%d|%lld.%06ld|%s|%s|",
			PROTOCOL_VERSION, tv->tv_sec, tv->tv_usec, direction, event) == -1 ||
		    io_vprintf(lka_proc_get_io(rp->name), format, ap) == -1)
			fatalx("failed to write to processor");
		va_end(ap);
	}
}

void
lka_report_smtp_link_connect(const char *direction, struct timeval *tv, uint64_t reqid, const char *rdns,
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
	
	report_smtp_broadcast(reqid, direction, tv, "link-connect",
	    "%016"PRIx64"|%s|%s|%s:%d|%s:%d\n",
	    reqid, rdns, fcrdns_str, src, src_port, dest, dest_port);
}

void
lka_report_smtp_link_disconnect(const char *direction, struct timeval *tv, uint64_t reqid)
{
	report_smtp_broadcast(reqid, direction, tv, "link-disconnect",
	    "%016"PRIx64"\n", reqid);
}

void
lka_report_smtp_link_identify(const char *direction, struct timeval *tv, uint64_t reqid, const char *heloname)
{
	report_smtp_broadcast(reqid, direction, tv, "link-identify",
	    "%016"PRIx64"|%s\n", reqid, heloname);
}

void
lka_report_smtp_link_tls(const char *direction, struct timeval *tv, uint64_t reqid, const char *ciphers)
{
	report_smtp_broadcast(reqid, direction, tv, "link-tls",
	    "%016"PRIx64"|%s\n", reqid, ciphers);
}

void
lka_report_smtp_tx_begin(const char *direction, struct timeval *tv, uint64_t reqid, uint32_t msgid)
{
	report_smtp_broadcast(reqid, direction, tv, "tx-begin",
	    "%016"PRIx64"|%08x\n", reqid, msgid);
}

void
lka_report_smtp_tx_mail(const char *direction, struct timeval *tv, uint64_t reqid, uint32_t msgid, const char *address, int ok)
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
	report_smtp_broadcast(reqid, direction, tv, "tx-mail",
	    "%016"PRIx64"|%08x|%s|%s\n", reqid, msgid, address, result);
}

void
lka_report_smtp_tx_rcpt(const char *direction, struct timeval *tv, uint64_t reqid, uint32_t msgid, const char *address, int ok)
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
	report_smtp_broadcast(reqid, direction, tv, "tx-rcpt",
	    "%016"PRIx64"|%08x|%s|%s\n", reqid, msgid, address, result);
}

void
lka_report_smtp_tx_envelope(const char *direction, struct timeval *tv, uint64_t reqid, uint32_t msgid, uint64_t evpid)
{
	report_smtp_broadcast(reqid, direction, tv, "tx-envelope",
	    "%016"PRIx64"|%08x|%016"PRIx64"\n",
	    reqid, msgid, evpid);
}

void
lka_report_smtp_tx_data(const char *direction, struct timeval *tv, uint64_t reqid, uint32_t msgid, int ok)
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
	report_smtp_broadcast(reqid, direction, tv, "tx-data",
	    "%016"PRIx64"|%08x|%s\n", reqid, msgid, result);
}

void
lka_report_smtp_tx_commit(const char *direction, struct timeval *tv, uint64_t reqid, uint32_t msgid, size_t msgsz)
{
	report_smtp_broadcast(reqid, direction, tv, "tx-commit",
	    "%016"PRIx64"|%08x|%zd\n",
	    reqid, msgid, msgsz);
}

void
lka_report_smtp_tx_rollback(const char *direction, struct timeval *tv, uint64_t reqid, uint32_t msgid)
{
	report_smtp_broadcast(reqid, direction, tv, "tx-rollback",
	    "%016"PRIx64"|%08x\n",
	    reqid, msgid);
}

void
lka_report_smtp_protocol_client(const char *direction, struct timeval *tv, uint64_t reqid, const char *command)
{
	report_smtp_broadcast(reqid, direction, tv, "protocol-client",
	    "%016"PRIx64"|%s\n",
	    reqid, command);
}

void
lka_report_smtp_protocol_server(const char *direction, struct timeval *tv, uint64_t reqid, const char *response)
{
	report_smtp_broadcast(reqid, direction, tv, "protocol-server",
	    "%016"PRIx64"|%s\n",
	    reqid, response);
}

void
lka_report_smtp_filter_response(const char *direction, struct timeval *tv, uint64_t reqid,
    int phase, int response, const char *param)
{
	const char *phase_name;
	const char *response_name;

	switch (phase) {
	case FILTER_CONNECT:
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

	report_smtp_broadcast(reqid, direction, tv, "filter-response",
	    "%016"PRIx64"|%s|%s%s%s\n",
	    reqid, phase_name, response_name, param ? "|" : "", param ? param : "");
}

void
lka_report_smtp_timeout(const char *direction, struct timeval *tv, uint64_t reqid)
{
	report_smtp_broadcast(reqid, direction, tv, "timeout",
	    "%016"PRIx64"\n",
	    reqid);
}
