/*	$OpenBSD: lka_filter.c,v 1.1 2018/11/03 13:42:24 gilles Exp $	*/

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

static void	filter_proceed(uint64_t, enum filter_phase, const char *);
static void	filter_rewrite(uint64_t, enum filter_phase, const char *);
static void	filter_reject(uint64_t, enum filter_phase, const char *);
static void	filter_disconnect(uint64_t, enum filter_phase, const char *);

static int	filter_exec_notimpl(uint64_t, struct filter_rule *, const char *);
static int	filter_exec_connected(uint64_t, struct filter_rule *, const char *);
static int	filter_exec_helo(uint64_t, struct filter_rule *, const char *);
static int	filter_exec_mail_from(uint64_t, struct filter_rule *, const char *);
static int	filter_exec_rcpt_to(uint64_t, struct filter_rule *, const char *);


static struct filter_exec {
	enum filter_phase	phase;
	const char	       *phase_name;
	int		       (*func)(uint64_t, struct filter_rule *, const char *);
} filter_execs[] = {
	{ FILTER_AUTH,     	"auth",		filter_exec_notimpl },
	{ FILTER_CONNECTED,	"connected",	filter_exec_connected },
	{ FILTER_DATA,    	"data",		filter_exec_notimpl },
	{ FILTER_EHLO,		"ehlo",		filter_exec_helo },
	{ FILTER_HELO,		"helo",		filter_exec_helo },
	{ FILTER_STARTTLS,     	"starttls",	filter_exec_notimpl },
	{ FILTER_MAIL_FROM,    	"mail-from",	filter_exec_mail_from },
	{ FILTER_NOOP,    	"noop",		filter_exec_notimpl },
	{ FILTER_QUIT,    	"quit",		filter_exec_notimpl },
	{ FILTER_RCPT_TO,    	"rcpt-to",	filter_exec_rcpt_to },
	{ FILTER_RSET,    	"rset",		filter_exec_notimpl },
};

void
lka_filter(uint64_t reqid, enum filter_phase phase, const char *param)
{
	struct filter_rule	*rule;
	uint8_t			i;

	for (i = 0; i < nitems(filter_execs); ++i)
		if (phase == filter_execs[i].phase)
			break;
	if (i == nitems(filter_execs))
		goto proceed;

	TAILQ_FOREACH(rule, &env->sc_filter_rules[phase], entry) {
		if (! filter_execs[i].func(reqid, rule, param)) {
			if (rule->rewrite)
				filter_rewrite(reqid, phase, rule->rewrite);
			else if (rule->disconnect)
				filter_disconnect(reqid, phase, rule->disconnect);
			else
				filter_reject(reqid, phase, rule->reject);
			return;
		}
	}

proceed:
	filter_proceed(reqid, phase, param);
}

static void
filter_proceed(uint64_t reqid, enum filter_phase phase, const char *param)
{
	m_create(p_pony, IMSG_SMTP_FILTER, 0, 0, -1);
	m_add_id(p_pony, reqid);
	m_add_int(p_pony, phase);
	m_add_int(p_pony, FILTER_PROCEED);
	m_add_string(p_pony, param);
	m_close(p_pony);
}

static void
filter_rewrite(uint64_t reqid, enum filter_phase phase, const char *param)
{
	m_create(p_pony, IMSG_SMTP_FILTER, 0, 0, -1);
	m_add_id(p_pony, reqid);
	m_add_int(p_pony, phase);
	m_add_int(p_pony, FILTER_REWRITE);
	m_add_string(p_pony, param);
	m_close(p_pony);
}

static void
filter_reject(uint64_t reqid, enum filter_phase phase, const char *message)
{
	m_create(p_pony, IMSG_SMTP_FILTER, 0, 0, -1);
	m_add_id(p_pony, reqid);
	m_add_int(p_pony, phase);
	m_add_int(p_pony, FILTER_REJECT);
	m_add_string(p_pony, message);
	m_close(p_pony);
}

static void
filter_disconnect(uint64_t reqid, enum filter_phase phase, const char *message)
{
	m_create(p_pony, IMSG_SMTP_FILTER, 0, 0, -1);
	m_add_id(p_pony, reqid);
	m_add_int(p_pony, phase);
	m_add_int(p_pony, FILTER_DISCONNECT);
	m_add_string(p_pony, message);
	m_close(p_pony);
}


/* below is code for builtin filters */

static int
filter_check_table(struct filter_rule *rule, enum table_service kind, const char *key)
{
	int	ret = 0;

	if (rule->table) {
		if (table_lookup(rule->table, NULL, key, kind, NULL) > 0)
			ret = 1;
		ret = rule->not_table < 0 ? !ret : ret;
	}
	return ret;
}

static int
filter_check_regex(struct filter_rule *rule, const char *key)
{
	int	ret = 0;

	if (rule->regex) {
		if (table_lookup(rule->regex, NULL, key, K_REGEX, NULL) > 0)
			ret = 1;
		ret = rule->not_regex < 0 ? !ret : ret;
	}
	return ret;
}

static int
filter_exec_notimpl(uint64_t reqid, struct filter_rule *rule, const char *param)
{
	return 1;
}

static int
filter_exec_connected(uint64_t reqid, struct filter_rule *rule, const char *param)
{
	if (filter_check_table(rule, K_NETADDR, param) ||
	    filter_check_regex(rule, param))
		return 0;
	return 1;
}

static int
filter_exec_helo(uint64_t reqid, struct filter_rule *rule, const char *param)
{
	if (filter_check_table(rule, K_DOMAIN, param) ||
	    filter_check_regex(rule, param))
		return 0;
	return 1;
}

static int
filter_exec_mail_from(uint64_t reqid, struct filter_rule *rule, const char *param)
{
	char	buffer[SMTPD_MAXMAILADDRSIZE];

	(void)strlcpy(buffer, param+1, sizeof(buffer));
	buffer[strcspn(buffer, ">")] = '\0';
	param = buffer;

	if (filter_check_table(rule, K_MAILADDR, param) ||
	    filter_check_regex(rule, param))
		return 0;
	return 1;
}

static int
filter_exec_rcpt_to(uint64_t reqid, struct filter_rule *rule, const char *param)
{
	char	buffer[SMTPD_MAXMAILADDRSIZE];

	(void)strlcpy(buffer, param+1, sizeof(buffer));
	buffer[strcspn(buffer, ">")] = '\0';
	param = buffer;

	if (filter_check_table(rule, K_MAILADDR, param) ||
	    filter_check_regex(rule, param))
		return 0;
	return 1;
}
