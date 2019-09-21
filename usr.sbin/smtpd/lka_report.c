/*	$OpenBSD: lka_report.c,v 1.33 2019/09/21 08:10:44 semarie Exp $	*/

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

#define	PROTOCOL_VERSION	"0.4"

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
	{ "link-greeting" },
	{ "link-identify" },
	{ "link-tls" },
	{ "link-auth" },

	{ "tx-reset" },
	{ "tx-begin" },
	{ "tx-mail" },
	{ "tx-rcpt" },
	{ "tx-envelope" },
	{ "tx-data" },
	{ "tx-commit" },
	{ "tx-rollback" },

	{ "protocol-client" },
	{ "protocol-server" },

	{ "filter-report" },
	{ "filter-response" },

	{ "timeout" },
};

static void
report_smtp_broadcast(uint64_t, const char *, struct timeval *, const char *,
    const char *, ...) __attribute__((__format__ (printf, 5, 6)));

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

	if (strncmp(hook, "smtp-in|", 8) == 0) {
		subsystem = &smtp_in;
		hook += 8;
	}
#if 0
	/* No smtp-out event has been implemented yet */
	else if (strncmp(hook, "smtp-out|", 9) == 0) {
		subsystem = &smtp_out;
		hook += 9;
	}
#endif
	else
		fatalx("Invalid message direction: %s", hook);

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
		fatalx("Unrecognized report name: %s", hook);

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

	else if (strcmp("smtp-out", direction) == 0)
		d = &smtp_out;

	else
		fatalx("unexpected direction: %s", direction);

	tailq = dict_xget(d, event);
	TAILQ_FOREACH(rp, tailq, entries) {
		if (!lka_filter_proc_in_session(reqid, rp->name))
			continue;

		va_start(ap, format);
		if (io_printf(lka_proc_get_io(rp->name),
		    "report|%s|%lld.%06ld|%s|%s|%016"PRIx64"%s",
		    PROTOCOL_VERSION, tv->tv_sec, tv->tv_usec, direction,
		    event, reqid, format[0] != '\n' ? "|" : "") == -1 ||
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

	if (strcmp(ss_to_text(ss_src), "local") == 0) {
		(void)snprintf(src, sizeof src, "unix:%s", SMTPD_SOCKET);
		(void)snprintf(dest, sizeof dest, "unix:%s", SMTPD_SOCKET);
	} else {
		(void)snprintf(src, sizeof src, "%s:%d", ss_to_text(ss_src), src_port);
		(void)snprintf(dest, sizeof dest, "%s:%d", ss_to_text(ss_dest), dest_port);
	}

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
	    "%s|%s|%s|%s\n", rdns, fcrdns_str, src, dest);
}

void
lka_report_smtp_link_disconnect(const char *direction, struct timeval *tv, uint64_t reqid)
{
	report_smtp_broadcast(reqid, direction, tv, "link-disconnect", "\n");
}

void
lka_report_smtp_link_greeting(const char *direction, uint64_t reqid,
    struct timeval *tv, const char *domain)
{
	report_smtp_broadcast(reqid, direction, tv, "link-greeting", "%s\n",
	    domain);
}

void
lka_report_smtp_link_auth(const char *direction, struct timeval *tv, uint64_t reqid,
    const char *username, const char *result)
{
	report_smtp_broadcast(reqid, direction, tv, "link-auth", "%s|%s\n",
	    username, result);
}

void
lka_report_smtp_link_identify(const char *direction, struct timeval *tv,
    uint64_t reqid, const char *method, const char *heloname)
{
	report_smtp_broadcast(reqid, direction, tv, "link-identify", "%s|%s\n",
	    method, heloname);
}

void
lka_report_smtp_link_tls(const char *direction, struct timeval *tv, uint64_t reqid, const char *ciphers)
{
	report_smtp_broadcast(reqid, direction, tv, "link-tls", "%s\n",
	    ciphers);
}

void
lka_report_smtp_tx_reset(const char *direction, struct timeval *tv, uint64_t reqid, uint32_t msgid)
{
	report_smtp_broadcast(reqid, direction, tv, "tx-reset", "%08x\n",
	    msgid);
}

void
lka_report_smtp_tx_begin(const char *direction, struct timeval *tv, uint64_t reqid, uint32_t msgid)
{
	report_smtp_broadcast(reqid, direction, tv, "tx-begin", "%08x\n",
	    msgid);
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
	report_smtp_broadcast(reqid, direction, tv, "tx-mail", "%08x|%s|%s\n",
	    msgid, address, result);
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
	report_smtp_broadcast(reqid, direction, tv, "tx-rcpt", "%08x|%s|%s\n",
	    msgid, address, result);
}

void
lka_report_smtp_tx_envelope(const char *direction, struct timeval *tv, uint64_t reqid, uint32_t msgid, uint64_t evpid)
{
	report_smtp_broadcast(reqid, direction, tv, "tx-envelope",
	    "%08x|%016"PRIx64"\n", msgid, evpid);
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
	report_smtp_broadcast(reqid, direction, tv, "tx-data", "%08x|%s\n",
	    msgid, result);
}

void
lka_report_smtp_tx_commit(const char *direction, struct timeval *tv, uint64_t reqid, uint32_t msgid, size_t msgsz)
{
	report_smtp_broadcast(reqid, direction, tv, "tx-commit", "%08x|%zd\n",
	    msgid, msgsz);
}

void
lka_report_smtp_tx_rollback(const char *direction, struct timeval *tv, uint64_t reqid, uint32_t msgid)
{
	report_smtp_broadcast(reqid, direction, tv, "tx-rollback", "%08x\n",
	    msgid);
}

void
lka_report_smtp_protocol_client(const char *direction, struct timeval *tv, uint64_t reqid, const char *command)
{
	report_smtp_broadcast(reqid, direction, tv, "protocol-client", "%s\n",
	    command);
}

void
lka_report_smtp_protocol_server(const char *direction, struct timeval *tv, uint64_t reqid, const char *response)
{
	report_smtp_broadcast(reqid, direction, tv, "protocol-server", "%s\n",
	    response);
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
	case FILTER_JUNK:
		response_name = "junk";
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
	    "%s|%s%s%s\n", phase_name, response_name, param ? "|" : "",
	    param ? param : "");
}

void
lka_report_smtp_timeout(const char *direction, struct timeval *tv, uint64_t reqid)
{
	report_smtp_broadcast(reqid, direction, tv, "timeout", "\n");
}

void
lka_report_filter_report(uint64_t reqid, const char *name, int builtin,
    const char *direction, struct timeval *tv, const char *message)
{
	report_smtp_broadcast(reqid, direction, tv, "filter-report",
	    "%s|%s|%s\n", builtin ? "builtin" : "proc",
	    name, message);
}

void
lka_report_proc(const char *name, const char *line)
{
	char buffer[LINE_MAX];
	struct timeval tv;
	char *ep, *sp, *direction;
	uint64_t reqid;

	if (strlcpy(buffer, line + 7, sizeof(buffer)) >= sizeof(buffer))
		fatalx("Invalid report: line too long: %s", line);

	errno = 0;
	tv.tv_sec = strtoll(buffer, &ep, 10);
	if (ep[0] != '.' || errno != 0)
		fatalx("Invalid report: invalid time: %s", line);
	sp = ep + 1;
	tv.tv_usec = strtol(sp, &ep, 10);
	if (ep[0] != '|' || errno != 0)
		fatalx("Invalid report: invalid time: %s", line);
	if (ep - sp != 6)
		fatalx("Invalid report: invalid time: %s", line);

	direction = ep + 1;
	if (strncmp(direction, "smtp-in|", 8) == 0) {
		direction[7] = '\0';
		direction += 7;
#if 0
	} else if (strncmp(direction, "smtp-out|", 9) == 0) {
		direction[8] = '\0';
		direction += 8;
#endif
	} else
		fatalx("Invalid report: invalid direction: %s", line);

	reqid = strtoull(sp, &ep, 16);
	if (ep[0] != '|' || errno != 0)
		fatalx("Invalid report: invalid reqid: %s", line);
	sp = ep + 1;

	lka_report_filter_report(reqid, name, 0, direction, &tv, sp);
}
