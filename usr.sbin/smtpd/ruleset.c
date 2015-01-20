/*	$OpenBSD: ruleset.c,v 1.31 2015/01/20 17:37:54 deraadt Exp $ */

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


static int ruleset_check_source(struct table *,
    const struct sockaddr_storage *, int);
static int ruleset_check_mailaddr(struct table *, const struct mailaddr *);

struct rule *
ruleset_match(const struct envelope *evp)
{
	const struct mailaddr		*maddr = &evp->dest;
	const struct sockaddr_storage	*ss = &evp->ss;
	struct rule			*r;
	int				 ret;

	TAILQ_FOREACH(r, env->sc_rules, r_entry) {

		if (r->r_tag[0] != '\0') {
			ret = strcmp(r->r_tag, evp->tag);
			if (ret != 0 && !r->r_nottag)
				continue;
			if (ret == 0 && r->r_nottag)
				continue;
		}

		ret = ruleset_check_source(r->r_sources, ss, evp->flags);
		if (ret == -1) {
			errno = EAGAIN;
			return (NULL);
		}
		if ((ret == 0 && !r->r_notsources) || (ret != 0 && r->r_notsources))
			continue;

		if (r->r_senders) {
			ret = ruleset_check_mailaddr(r->r_senders, &evp->sender);
			if (ret == -1) {
				errno = EAGAIN;
				return (NULL);
			}
			if ((ret == 0 && !r->r_notsenders) || (ret != 0 && r->r_notsenders))
				continue;
		}

		if (r->r_recipients) {
			ret = ruleset_check_mailaddr(r->r_recipients, &evp->dest);
			if (ret == -1) {
				errno = EAGAIN;
				return (NULL);
			}
			if ((ret == 0 && !r->r_notrecipients) || (ret != 0 && r->r_notrecipients))
				continue;
		}

		ret = r->r_destination == NULL ? 1 :
		    table_lookup(r->r_destination, NULL, maddr->domain, K_DOMAIN,
			NULL);
		if (ret == -1) {
			errno = EAGAIN;
			return NULL;
		}
		if ((ret == 0 && !r->r_notdestination) || (ret != 0 && r->r_notdestination))
			continue;

		if (r->r_desttype == DEST_VDOM &&
		    (r->r_action == A_RELAY || r->r_action == A_RELAYVIA)) {
			if (! aliases_virtual_check(r->r_mapping,
				&evp->rcpt)) {
				return NULL;
			}
		}
		goto matched;
	}

	errno = 0;
	log_trace(TRACE_RULES, "no rule matched");
	return (NULL);

matched:
	log_trace(TRACE_RULES, "rule matched: %s", rule_to_text(r));
	return r;
}

static int
ruleset_check_source(struct table *table, const struct sockaddr_storage *ss,
    int evpflags)
{
	const char   *key;

	if (evpflags & (EF_AUTHENTICATED | EF_INTERNAL))
		key = "local";
	else
		key = ss_to_text(ss);
	switch (table_lookup(table, NULL, key, K_NETADDR, NULL)) {
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
ruleset_check_mailaddr(struct table *table, const struct mailaddr *maddr)
{
	const char	*key;

	key = mailaddr_to_text(maddr);
	if (key == NULL)
		return -1;

	switch (table_lookup(table, NULL, key, K_MAILADDR, NULL)) {
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
