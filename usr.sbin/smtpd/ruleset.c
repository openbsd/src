/*	$OpenBSD: ruleset.c,v 1.42 2018/12/28 11:40:29 eric Exp $ */

/*
 * Copyright (c) 2009 Gilles Chehade <gilles@poolp.org>
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
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "smtpd.h"
#include "log.h"

#define MATCH_RESULT(r, neg) ((r) == -1 ? -1 : ((neg) < 0 ? !(r) : (r)))

static int
ruleset_match_tag(struct rule *r, const struct envelope *evp)
{
	int		ret;
	struct table	*table;
	enum table_service service = K_STRING;

	if (!r->flag_tag)
		return 1;

	if (r->flag_tag_regex)
		service = K_REGEX;

	table = table_find(env, r->table_tag);
	ret = table_match(table, service, evp->tag);

	return MATCH_RESULT(ret, r->flag_tag);
}

static int
ruleset_match_from(struct rule *r, const struct envelope *evp)
{
	int		ret;
	const char	*key;
	struct table	*table;
	enum table_service service = K_NETADDR;

	if (!r->flag_from)
		return 1;

	if (r->flag_from_socket) {
		/* XXX - socket needs to be distinguished from "local" */
		return -1;
	}

	if (evp->flags & EF_INTERNAL)
		key = "local";
	else if (r->flag_from_rdns) {
		if (strcmp(evp->hostname, "<unknown>") == 0)
			return 0;
		key = evp->hostname;
	}
	else
		key = ss_to_text(&evp->ss);

	if (r->flag_from_regex)
		service = K_REGEX;

	table = table_find(env, r->table_from);
	ret = table_match(table, service, key);

	return MATCH_RESULT(ret, r->flag_from);
}

static int
ruleset_match_to(struct rule *r, const struct envelope *evp)
{
	int		ret;
	struct table	*table;
	enum table_service service = K_DOMAIN;

	if (!r->flag_for)
		return 1;

	if (r->flag_for_regex)
		service = K_REGEX;

	table = table_find(env, r->table_for);
	ret = table_match(table, service, evp->dest.domain);

	return MATCH_RESULT(ret, r->flag_for);
}

static int
ruleset_match_smtp_helo(struct rule *r, const struct envelope *evp)
{
	int		ret;
	struct table	*table;
	enum table_service service = K_DOMAIN;

	if (!r->flag_smtp_helo)
		return 1;

	if (r->flag_smtp_helo_regex)
		service = K_REGEX;

	table = table_find(env, r->table_smtp_helo);
	ret = table_match(table, service, evp->helo);

	return MATCH_RESULT(ret, r->flag_smtp_helo);
}

static int
ruleset_match_smtp_starttls(struct rule *r, const struct envelope *evp)
{
	if (!r->flag_smtp_starttls)
		return 1;

	/* XXX - not until TLS flag is added to envelope */
	return -1;
}

static int
ruleset_match_smtp_auth(struct rule *r, const struct envelope *evp)
{
	int	ret;

	if (!r->flag_smtp_auth)
		return 1;

	if (!(evp->flags & EF_AUTHENTICATED))
		ret = 0;
	else if (r->table_smtp_auth) {
		/* XXX - not until smtp_session->username is added to envelope */
		/*
		 * table = table_find(m->from_table);
		 * key = evp->username;
		 * return table_match(table, K_CREDENTIALS, key);
		 */
		return -1;

	}
	else
		ret = 1;

	return MATCH_RESULT(ret, r->flag_smtp_auth);
}

static int
ruleset_match_smtp_mail_from(struct rule *r, const struct envelope *evp)
{
	int		ret;
	const char	*key;
	struct table	*table;
	enum table_service service = K_MAILADDR;

	if (!r->flag_smtp_mail_from)
		return 1;

	if (r->flag_smtp_mail_from_regex)
		service = K_REGEX;

	if ((key = mailaddr_to_text(&evp->sender)) == NULL)
		return -1;

	table = table_find(env, r->table_smtp_mail_from);
	ret = table_match(table, service, key);

	return MATCH_RESULT(ret, r->flag_smtp_mail_from);
}

static int
ruleset_match_smtp_rcpt_to(struct rule *r, const struct envelope *evp)
{
	int		ret;
	const char	*key;
	struct table	*table;
	enum table_service service = K_MAILADDR;

	if (!r->flag_smtp_rcpt_to)
		return 1;

	if (r->flag_smtp_rcpt_to_regex)
		service = K_REGEX;

	if ((key = mailaddr_to_text(&evp->dest)) == NULL)
		return -1;

	table = table_find(env, r->table_smtp_rcpt_to);
	ret = table_match(table, service, key);

	return MATCH_RESULT(ret, r->flag_smtp_rcpt_to);
}

struct rule *
ruleset_match(const struct envelope *evp)
{
	struct rule	*r;
	int		i = 0;

#define	MATCH_EVAL(x)				\
	switch ((x)) {				\
	case -1:	goto tempfail;		\
	case 0:		continue;		\
	default:	break;			\
	}
	TAILQ_FOREACH(r, env->sc_rules, r_entry) {
		++i;
		MATCH_EVAL(ruleset_match_tag(r, evp));
		MATCH_EVAL(ruleset_match_from(r, evp));
		MATCH_EVAL(ruleset_match_to(r, evp));
		MATCH_EVAL(ruleset_match_smtp_helo(r, evp));
		MATCH_EVAL(ruleset_match_smtp_auth(r, evp));
		MATCH_EVAL(ruleset_match_smtp_starttls(r, evp));
		MATCH_EVAL(ruleset_match_smtp_mail_from(r, evp));
		MATCH_EVAL(ruleset_match_smtp_rcpt_to(r, evp));
		goto matched;
	}
#undef	MATCH_EVAL

	errno = 0;
	log_trace(TRACE_RULES, "no rule matched");
	return (NULL);

tempfail:
	errno = EAGAIN;
	log_trace(TRACE_RULES, "temporary failure in processing of a rule");
	return (NULL);

matched:
	log_trace(TRACE_RULES, "rule #%d matched: %s", i, rule_to_text(r));
	return r;
}
