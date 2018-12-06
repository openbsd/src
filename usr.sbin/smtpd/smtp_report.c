/*	$OpenBSD: smtp_report.c,v 1.7 2018/12/06 16:05:04 gilles Exp $	*/

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
smtp_report_link_connect(uint64_t qid, const char *rdns, int fcrdns,
    const struct sockaddr_storage *ss_src,
    const struct sockaddr_storage *ss_dest)
{
	m_create(p_lka, IMSG_SMTP_REPORT_LINK_CONNECT, 0, 0, -1);
	m_add_time(p_lka, time(NULL));
	m_add_id(p_lka, qid);
	m_add_string(p_lka, rdns);
	m_add_int(p_lka, fcrdns);
	m_add_sockaddr(p_lka, (const struct sockaddr *)ss_src);
	m_add_sockaddr(p_lka, (const struct sockaddr *)ss_dest);
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
smtp_report_tx_begin(uint64_t qid, uint32_t msgid)
{
	m_create(p_lka, IMSG_SMTP_REPORT_TX_BEGIN, 0, 0, -1);
	m_add_time(p_lka, time(NULL));
	m_add_id(p_lka, qid);
	m_add_u32(p_lka, msgid);
	m_close(p_lka);
}

void
smtp_report_tx_mail(uint64_t qid, uint32_t msgid, const char *address, int ok)
{
	m_create(p_lka, IMSG_SMTP_REPORT_TX_MAIL, 0, 0, -1);
	m_add_time(p_lka, time(NULL));
	m_add_id(p_lka, qid);
	m_add_u32(p_lka, msgid);
	m_add_string(p_lka, address);
	m_add_int(p_lka, ok);
	m_close(p_lka);
}

void
smtp_report_tx_rcpt(uint64_t qid, uint32_t msgid, const char *address, int ok)
{
	m_create(p_lka, IMSG_SMTP_REPORT_TX_RCPT, 0, 0, -1);
	m_add_time(p_lka, time(NULL));
	m_add_id(p_lka, qid);
	m_add_u32(p_lka, msgid);
	m_add_string(p_lka, address);
	m_add_int(p_lka, ok);
	m_close(p_lka);
}

void
smtp_report_tx_envelope(uint64_t qid, uint32_t msgid, uint64_t evpid)
{
	m_create(p_lka, IMSG_SMTP_REPORT_TX_ENVELOPE, 0, 0, -1);
	m_add_time(p_lka, time(NULL));
	m_add_id(p_lka, qid);
	m_add_u32(p_lka, msgid);
	m_add_id(p_lka, evpid);
	m_close(p_lka);
}

void
smtp_report_tx_commit(uint64_t qid, uint32_t msgid, size_t msgsz)
{
	m_create(p_lka, IMSG_SMTP_REPORT_TX_COMMIT, 0, 0, -1);
	m_add_time(p_lka, time(NULL));
	m_add_id(p_lka, qid);
	m_add_u32(p_lka, msgid);
	m_add_size(p_lka, msgsz);
	m_close(p_lka);
}

void
smtp_report_tx_rollback(uint64_t qid, uint32_t msgid)
{
	m_create(p_lka, IMSG_SMTP_REPORT_TX_ROLLBACK, 0, 0, -1);
	m_add_time(p_lka, time(NULL));
	m_add_id(p_lka, qid);
	m_add_u32(p_lka, msgid);
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
