/*	$OpenBSD: smtp_report.c,v 1.1 2018/11/01 14:48:49 gilles Exp $	*/

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
#include <sys/uio.h>

#include <netinet/in.h>

#include <ctype.h>
#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <limits.h>
#include <inttypes.h>
#include <openssl/ssl.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>

#include "smtpd.h"
#include "log.h"
#include "ssl.h"
#include "rfc5322.h"

void
smtp_report_link_connect(uint64_t qid, const char *src, const char *dest)
{
	m_create(p_lka, IMSG_SMTP_REPORT_LINK_CONNECT, 0, 0, -1);
	m_add_time(p_lka, time(NULL));
	m_add_id(p_lka, qid);
	m_add_string(p_lka, src);
	m_add_string(p_lka, dest);
	m_close(p_lka);
}

void
smtp_report_link_tls(uint64_t qid, const char *ssl)
{
	m_create(p_lka, IMSG_SMTP_REPORT_LINK_TLS, 0, 0, -1);
	m_add_time(p_lka, time(NULL));
	m_add_id(p_lka, qid);
	m_add_string(p_lka, ssl);
	m_close(p_lka);
}

void
smtp_report_link_disconnect(uint64_t qid)
{
	m_create(p_lka, IMSG_SMTP_REPORT_LINK_DISCONNECT, 0, 0, -1);
	m_add_time(p_lka, time(NULL));
	m_add_id(p_lka, qid);
	m_close(p_lka);
}

void
smtp_report_tx_begin(uint64_t qid)
{
	m_create(p_lka, IMSG_SMTP_REPORT_TX_BEGIN, 0, 0, -1);
	m_add_time(p_lka, time(NULL));
	m_add_id(p_lka, qid);
	m_close(p_lka);
}

void
smtp_report_tx_commit(uint64_t qid)
{
	m_create(p_lka, IMSG_SMTP_REPORT_TX_COMMIT, 0, 0, -1);
	m_add_time(p_lka, time(NULL));
	m_add_id(p_lka, qid);
	m_close(p_lka);
}

void
smtp_report_tx_rollback(uint64_t qid)
{
	m_create(p_lka, IMSG_SMTP_REPORT_TX_ROLLBACK, 0, 0, -1);
	m_add_time(p_lka, time(NULL));
	m_add_id(p_lka, qid);
	m_close(p_lka);
}

void
smtp_report_protocol_client(uint64_t qid, const char *command)
{
	m_create(p_lka, IMSG_SMTP_REPORT_PROTOCOL_CLIENT, 0, 0, -1);
	m_add_time(p_lka, time(NULL));
	m_add_id(p_lka, qid);
	m_add_string(p_lka, command);
	m_close(p_lka);
}

void
smtp_report_protocol_server(uint64_t qid, const char *response)
{
	m_create(p_lka, IMSG_SMTP_REPORT_PROTOCOL_SERVER, 0, 0, -1);
	m_add_time(p_lka, time(NULL));
	m_add_id(p_lka, qid);
	m_add_string(p_lka, response);
	m_close(p_lka);
}
