/*	$OpenBSD: lka_report.c,v 1.1 2018/11/01 14:48:49 gilles Exp $	*/

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

static void
report_smtp_broadcast(const char *format, ...)
{
	va_list		ap;
	void		*hdl = NULL;
	const char	*reporter;

	va_start(ap, format);
	while (dict_iter(env->sc_smtp_reporters_dict, &hdl, &reporter, NULL)) {
		if (io_vprintf(lka_proc_get_io(reporter), format, ap) == -1)
			fatalx("failed to write to processor");
	}
	va_end(ap);
}

void
lka_report_smtp_link_connect(time_t tm, uint64_t reqid, const char *src_addr, const char *dest_addr)
{
	report_smtp_broadcast("report|in-smtp-link-connect|"
	    "%zd|%016"PRIx64"|%s|%s\n",
	    tm, reqid, src_addr, dest_addr);
}

void
lka_report_smtp_link_disconnect(time_t tm, uint64_t reqid)
{
	report_smtp_broadcast("report|in-smtp-link-disconnect|"
	    "%zd|%016"PRIx64"\n",
	    tm, reqid);
}

void
lka_report_smtp_link_tls(time_t tm, uint64_t reqid, const char *ciphers)
{
	report_smtp_broadcast("report|in-smtp-link-tls|"
	    "%zd|%016"PRIx64"|%s\n",
	    tm, reqid, ciphers);
}

void
lka_report_smtp_tx_begin(time_t tm, uint64_t reqid)
{
	report_smtp_broadcast("report|in-smtp-tx-begin|"
	    "%zd|%016"PRIx64"\n",
	    tm, reqid);
}

void
lka_report_smtp_tx_commit(time_t tm, uint64_t reqid)
{
	report_smtp_broadcast("report|in-smtp-tx-commit|"
	    "%zd|%016"PRIx64"\n",
	    tm, reqid);
}

void
lka_report_smtp_tx_rollback(time_t tm, uint64_t reqid)
{
	report_smtp_broadcast("report|in-smtp-tx-rollback|"
	    "%zd|%016"PRIx64"\n",
	    tm, reqid);
}

void
lka_report_smtp_protocol_client(time_t tm, uint64_t reqid, const char *command)
{
	report_smtp_broadcast("report|in-smtp-protocol-client|"
	    "%zd|%016"PRIx64"|%s\n",
	    tm, reqid, command);
}

void
lka_report_smtp_protocol_server(time_t tm, uint64_t reqid, const char *response)
{
	report_smtp_broadcast("report|in-smtp-protocol-server|"
	    "%zd|%016"PRIx64"|%s\n",
	    tm, reqid, response);
}
