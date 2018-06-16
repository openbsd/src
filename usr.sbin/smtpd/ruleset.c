/*	$OpenBSD: ruleset.c,v 1.36 2018/06/16 19:41:26 gilles Exp $ */

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


static int
ruleset_match_table_lookup(struct table *table, const char *key, enum table_service service)
{
	switch (table_lookup(table, NULL, key, service, NULL)) {
	case 1:
		return 1;
	case -1:
		log_warnx("warn: failure to perform a table lookup on table %s",
		    table->t_name);
		return -1;
	default:
		break;
	}
	return 0;
}

static int
ruleset_match_tag(struct rule *r, const struct envelope *evp)
{
	int		ret;
	struct table	*table;

	if (!r->flag_tag)
		return 1;

	table = table_find(env, r->table_tag, NULL);
	if ((ret = ruleset_match_table_lookup(table, evp->tag, K_STRING)) < 0)
		return ret;

	return r->flag_tag < 0 ? !ret : ret;
}

static int
ruleset_match_from(struct rule *r, const struct envelope *evp)
{
	int		ret;
	const char	*key;
	struct table	*table;

	if (!r->flag_from)
		return 1;

	if (r->flag_from_socket) {
		/* XXX - socket needs to be distinguished from "local" */
		return -1;
	}

	/* XXX - socket should also be considered local */
	if (evp->flags & EF_INTERNAL)
		key = "local";
	else
		key = ss_to_text(&evp->ss);

	table = table_find(env, r->table_from, NULL);
	if ((ret = ruleset_match_table_lookup(table, key, K_NETADDR)) < 0)
		return -1;

	return r->flag_from < 0 ? !ret : ret;
}

static int
ruleset_match_to(struct rule *r, const struct envelope *evp)
{
	int		ret;
	struct table	*table;

	if (!r->flag_for)
		return 1;

	table = table_find(env, r->table_for, NULL);
	if ((ret = ruleset_match_table_lookup(table, evp->dest.domain,
		    K_DOMAIN)) < 0)
		return -1;

	return r->flag_for < 0 ? !ret : ret;
}

static int
ruleset_match_smtp_helo(struct rule *r, const struct envelope *evp)
{
	int		ret;
	struct table	*table;

	if (!r->flag_smtp_helo)
		return 1;

	table = table_find(env, r->table_smtp_helo, NULL);
	if ((ret = ruleset_match_table_lookup(table, evp->helo, K_DOMAIN)) < 0)
		return -1;

	return r->flag_smtp_helo < 0 ? !ret : ret;
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
		 * table = table_find(m->from_table, NULL);
		 * key = evp->username;
		 * return ruleset_match_table_lookup(table, key, K_CREDENTIALS);
		 */
		return -1;

	}
	else
		ret = 1;

	return r->flag_smtp_auth < 0 ? !ret : ret;
}

static int
ruleset_match_smtp_mail_from(struct rule *r, const struct envelope *evp)
{
	int		ret;
	const char	*key;
	struct table	*table;

	if (!r->flag_smtp_mail_from)
		return 1;

	if ((key = mailaddr_to_text(&evp->sender)) == NULL)
		return -1;

	table = table_find(env, r->table_smtp_mail_from, NULL);
	if ((ret = ruleset_match_table_lookup(table, key, K_MAILADDR)) < 0)
		return -1;

	return r->flag_smtp_mail_from < 0 ? !ret : ret;
}

static int
ruleset_match_smtp_rcpt_to(struct rule *r, const struct envelope *evp)
{
	int		ret;
	const char	*key;
	struct table	*table;

	if (!r->flag_smtp_rcpt_to)
		return 1;

	if ((key = mailaddr_to_text(&evp->dest)) == NULL)
		return -1;

	table = table_find(env, r->table_smtp_rcpt_to, NULL);
	if ((ret = ruleset_match_table_lookup(table, key, K_MAILADDR)) < 0)
		return -1;

	return r->flag_smtp_rcpt_to < 0 ? !ret : ret;
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
